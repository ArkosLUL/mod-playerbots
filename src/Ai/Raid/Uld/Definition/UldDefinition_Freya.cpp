/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldDefinitions.h"

#include "AiObjectContext.h"
#include "BossAuraActions.h"
#include "BossAuraTriggers.h"
#include "BossResistanceMultipliers.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "RaidTankDefensive.h"
#include "Strategy.h"
#include "UldActions_Freya.h"
#include "UldEncounterGate.h"
#include "UldEncounter_Freya.h"
#include "UldMultipliers_Freya.h"
#include "UldTriggers_Freya.h"

#include <cmath>
#include <vector>

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
bool FreyaAlive(PlayerbotAI* botAI)
{
    Unit* freya = GetFreyaScan(botAI).Boss();
    return freya && freya->IsAlive();
}

bool FreyaOwnsTargets(PlayerbotAI* botAI) { return botAI->GetState() == BOT_STATE_COMBAT && FreyaAlive(botAI); }

bool FreyaTankOnLasher(PlayerbotAI* botAI)
{
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    return target && target->GetEntry() == NPC_DETONATING_LASHER;
}

bool FreyaTargetIsGift(PlayerbotAI* botAI)
{
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    return target && target->IsAlive() && target->GetEntry() == NPC_EONARS_GIFT;
}

bool FreyaTargetOnOtherTank(PlayerbotAI* botAI)
{
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    Unit* victim = target ? target->GetVictim() : nullptr;
    Player* holder = victim ? victim->ToPlayer() : nullptr;
    return holder && holder != botAI->GetBot() && PlayerbotAI::IsTank(holder);
}

bool FreyaMeleeNearBomb(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return !PlayerbotAI::IsTank(bot) &&
           bot->FindNearestGameObject(GOBJECT_NATURE_BOMB, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS) != nullptr;
}

bool FreyaMeleeApproachEndsIn(PlayerbotAI* botAI, bool behind,
                              std::vector<EncounterHelpers::HazardCircle> const& keepOut)
{
    if (keepOut.empty())
        return false;

    Player* bot = botAI->GetBot();
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    if (!target)
        return false;

    float const reach = sPlayerbotAIConfig.MeleeDistance + bot->GetCombatReach() + target->GetCombatReach();
    auto const endsIn = [&keepOut, target, reach](float angle)
    {
        float const x = target->GetPositionX() + reach * std::cos(angle);
        float const y = target->GetPositionY() + reach * std::sin(angle);
        for (auto const& [centre, radius] : keepOut)
            if (centre.GetExactDist2d(x, y) < radius)
                return true;

        return false;
    };

    if (!behind)
        return endsIn(target->GetAngle(bot));

    float const flank = float(M_PI) * 3.0f / 5.0f;
    return endsIn(target->GetOrientation() + flank) || endsIn(target->GetOrientation() - flank);
}

std::vector<EncounterHelpers::HazardCircle> FreyaLowLasherBlasts(PlayerbotAI* botAI)
{
    if (PlayerbotAI::IsTank(botAI->GetBot()))
        return {};

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    std::vector<EncounterHelpers::HazardCircle> blasts;
    for (Unit* lasher : state.detonatingLashers)
        if (lasher->IsAlive() && lasher->GetHealthPct() < ULDUAR_FREYA_LASHER_BAIL_PCT &&
            !IsFreyaLowLasherChasing(botAI->GetBot(), state, lasher))
            blasts.emplace_back(lasher->GetPosition(), ULDUAR_FREYA_DETONATE_RADIUS + 1.0f);

    return blasts;
}

std::vector<EncounterHelpers::HazardCircle> FreyaKitedSnaplasherReach(PlayerbotAI* botAI)
{
    Unit* snaplasher = GetFreyaSnaplasherToKite(botAI);
    if (!snaplasher)
        return {};

    float const reach = GetFreyaSnaplasherKiteReach(snaplasher, botAI->GetBot(), ULDUAR_FREYA_SNAPLASHER_KITE_MARGIN);
    return {{snaplasher->GetPosition(), reach}};
}

bool FreyaMeleeReachIntoBlast(PlayerbotAI* botAI)
{
    return FreyaMeleeApproachEndsIn(botAI, false, FreyaLowLasherBlasts(botAI));
}

bool FreyaMeleeBehindIntoBlast(PlayerbotAI* botAI)
{
    return FreyaMeleeApproachEndsIn(botAI, true, FreyaLowLasherBlasts(botAI));
}

bool FreyaMeleeReachIntoKite(PlayerbotAI* botAI)
{
    return FreyaMeleeApproachEndsIn(botAI, false, FreyaKitedSnaplasherReach(botAI));
}

bool FreyaMeleeBehindIntoKite(PlayerbotAI* botAI)
{
    return FreyaMeleeApproachEndsIn(botAI, true, FreyaKitedSnaplasherReach(botAI));
}

bool FreyaMeleeReachIntoBomb(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (PlayerbotAI::IsTank(bot))
        return false;

    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    if (!target)
        return false;

    float const reach = sPlayerbotAIConfig.MeleeDistance + bot->GetCombatReach() + target->GetCombatReach();
    float const angle = target->GetAngle(bot);
    Position const end(target->GetPositionX() + reach * std::cos(angle),
                       target->GetPositionY() + reach * std::sin(angle), target->GetPositionZ(), 0.0f);

    std::vector<Position> const bombs =
        GetFreyaNatureBombPositions(bot, bot->GetExactDist2d(&end) + ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS);
    for (Position const& bomb : bombs)
        if (bomb.GetExactDist2d(&end) < ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS)
            return true;

    return FreyaWalkCrossesNatureBomb(bot->GetPosition(), end, bombs, ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS);
}

void DefineFreya(EncounterBuilder& e)
{
    e.Node<FreyaNearNatureBombTrigger, FreyaMoveAwayNatureBombAction>(ACTION_RAID + 4);

    e.Node<FreyaTankNatureBombTrigger, FreyaTankNatureBombAction>(ACTION_RAID + 4);

    e.Node<FreyaNaturesFuryBailTrigger, FreyaNaturesFuryBailAction>(ACTION_RAID + 4);
    e.Node<FreyaStepOutOfSunbeamTrigger, FreyaStepOutOfSunbeamAction>(ACTION_RAID + 4);

    e.Node<FreyaAddTankDefensiveTrigger, FreyaAddTankDefensiveAction>(ACTION_RAID + 4);
    e.Node<FreyaAddTankExternalTrigger, FreyaAddTankExternalAction>(ACTION_RAID + 4);

    e.Node<FreyaLasherAboutToBlowTrigger, FreyaLasherAboutToBlowAction>(ACTION_RAID + 3);
    e.Node<FreyaKiteSnaplasherTrigger, FreyaKiteSnaplasherAction>(ACTION_RAID + 3);
    e.Node<FreyaFrostNovaLashersTrigger, FreyaFrostNovaLashersAction>(ACTION_RAID + 2);
    e.Node<FreyaTrapLashersTrigger, FreyaTrapLashersAction>(ACTION_RAID + 2);
    e.Node<FreyaSummonArmyTrigger, FreyaSummonArmyAction>(ACTION_RAID + 2);
    e.Node<FreyaRangedCampTrigger, FreyaRangedCampAction>(ACTION_RAID);

    e.Node<FreyaReachHeldPackTrigger, FreyaReachHeldPackAction>(ACTION_RAID + 1);
    e.Node<FreyaReachGiftTrigger, FreyaReachGiftAction>(ACTION_RAID + 1);

    e.Node<FreyaMoveToHealingSporeTrigger, FreyaMoveToHealingSporeAction>(ACTION_RAID + 2);

    e.Node<FreyaTankAddsTrigger, FreyaTankAddsAction>(ACTION_RAID + 1);

    e.Node<FreyaTankHoldFreyaTrigger, FreyaTankHoldFreyaAction>(ACTION_RAID + 1);

    e.Node<FreyaRedirectThreatTrigger, FreyaRedirectThreatAction>(ACTION_RAID + 1);
    e.Node<FreyaSetDpsPriorityTrigger, FreyaSetDpsPriorityAction>(ACTION_RAID);
    e.Node(
        "freya nature resistance trigger",
        [](PlayerbotAI* botAI) -> Trigger* { return new BossNatureResistanceTrigger(botAI, "freya"); },
        "freya nature resistance action",
        [](PlayerbotAI* botAI) -> Action* { return new BossNatureResistanceAction(botAI, "freya"); }, ACTION_RAID);
    e.Node(
        "freya fire resistance trigger",
        [](PlayerbotAI* botAI) -> Trigger* { return new BossFireResistanceTrigger(botAI, "freya"); },
        "freya fire resistance action",
        [](PlayerbotAI* botAI) -> Action* { return new BossFireResistanceAction(botAI, "freya"); }, ACTION_RAID);

    e.Node<FreyaFreeRootedAllyTrigger, FreyaFreeRootedAllyAction>(ACTION_RAID + 5);
    e.Node<FreyaBreakIronRootsTrigger, FreyaBreakIronRootsAction>(ACTION_RAID + 5);
    e.Node<FreyaDodgeUnstableSunBeamTrigger, FreyaDodgeUnstableSunBeamAction>(ACTION_RAID + 4);
    e.Node<FreyaGroundTremorHoldCastTrigger, FreyaGroundTremorHoldCastAction>(ACTION_EMERGENCY + 2);

    e.OwnTargeting("freya disable automatic targeting", Role::Dps, FreyaOwnsTargets, Family::DpsAssist);
    e.OwnTargeting("freya disable automatic targeting", Role::Tank, FreyaOwnsTargets, Family::TankAssist);

    e.Multiplier<FreyaTrioSyncMultiplier>(Family::Melee | Family::Spell);

    e.Multiplier<FreyaLasherFinishAoeMultiplier>(Family::AnyAction);

    e.Multiplier<FreyaLasherTrapReserveMultiplier>(Family::Spell);
    e.Multiplier<FreyaArmyRuneReserveMultiplier>(Family::Spell);

    e.Block("freya avoid aoe hold", Role::Any, FreyaAlive, Family::AvoidAoe);

    e.Block("freya gift dot hold", Role::Dps, FreyaTargetIsGift, Family::DebuffOnAttacker);

    e.Block("freya lasher taunt reserve", Role::Tank, FreyaTankOnLasher, 0,
            {"hand of reckoning", "righteous defense", "taunt", "growl", "dark command", "death grip", "mocking blow"});

    e.Block("freya area taunt hold", Role::Tank, FreyaAlive, 0, {"challenging shout", "challenging roar"});

    e.Block("freya righteous defense hold", Role::Tank, FreyaTargetOnOtherTank, 0, {"righteous defense"});

    e.Block("freya add tank defensive hold", Role::Tank, IsFreyaAddTankDefensiveHeld, 0, HeldTankDefensiveNames());

    e.Multiplier<FreyaAddTankExternalHoldMultiplier>(Family::Spell);

    e.Block("freya nature bomb melee guard", Role::Melee, FreyaMeleeNearBomb, 0, {"set behind"});
    e.Block("freya nature bomb reach guard", Role::Melee, FreyaMeleeReachIntoBomb, Family::Charge, {"reach melee"});

    e.Block("freya lasher blast reach guard", Role::Melee, FreyaMeleeReachIntoBlast, Family::Charge, {"reach melee"});
    e.Block("freya lasher blast behind guard", Role::Melee, FreyaMeleeBehindIntoBlast, Family::SetBehind);

    e.Block("freya snaplasher kite reach guard", Role::Melee, FreyaMeleeReachIntoKite, Family::Charge, {"reach melee"});
    e.Block("freya snaplasher kite behind guard", Role::Melee, FreyaMeleeBehindIntoKite, Family::SetBehind);

    e.Multiplier<FreyaGroundTremorCastGateMultiplier>(Family::Spell);
    e.Multiplier<BossNatureAspectHoldMultiplier>(Family::Spell, "freya");
}
}

EncounterDefinition const& UldFreyaDefinition()
{
    static EncounterDefinition const definition(ULD_BOSS_FREYA, BossStateGate, "freya", &DefineFreya);
    return definition;
}
