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
#include "Strategy.h"
#include "UldActions_Freya.h"
#include "UldEncounterGate.h"
#include "UldEncounter_Freya.h"
#include "UldMultipliers_Freya.h"
#include "UldTriggers_Freya.h"

#include <cmath>

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

// True when the walk to the current target ends in a low lasher's blast. Reach melee stops on the bot's side, set
// behind at whichever flank point 3/5 pi off the target's facing is nearer, so both flanks count.
bool FreyaMeleeApproachEndsInBlast(PlayerbotAI* botAI, bool behind)
{
    Player* bot = botAI->GetBot();
    if (PlayerbotAI::IsTank(bot))
        return false;

    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    if (!target)
        return false;

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);
    if (state.detonatingLashers.empty())
        return false;

    float const reach = sPlayerbotAIConfig.MeleeDistance + bot->GetCombatReach() + target->GetCombatReach();
    auto const endsInBlast = [&state, target, reach](float angle)
    {
        float const x = target->GetPositionX() + reach * std::cos(angle);
        float const y = target->GetPositionY() + reach * std::sin(angle);
        for (Unit* lasher : state.detonatingLashers)
            if (lasher->IsAlive() && lasher->GetHealthPct() < ULDUAR_FREYA_LASHER_BAIL_PCT &&
                lasher->GetExactDist2d(x, y) < ULDUAR_FREYA_DETONATE_RADIUS + 1.0f)
                return true;

        return false;
    };

    if (!behind)
        return endsInBlast(target->GetAngle(bot));

    float const flank = float(M_PI) * 3.0f / 5.0f;
    return endsInBlast(target->GetOrientation() + flank) || endsInBlast(target->GetOrientation() - flank);
}

bool FreyaMeleeReachIntoBlast(PlayerbotAI* botAI) { return FreyaMeleeApproachEndsInBlast(botAI, false); }

bool FreyaMeleeBehindIntoBlast(PlayerbotAI* botAI) { return FreyaMeleeApproachEndsInBlast(botAI, true); }

void DefineFreya(EncounterBuilder& e)
{
    // Survival first, then the pacify counter, then targeting. A bot that is dead, blown up or
    // silenced contributes nothing to the trio wave it is being steered at.
    e.Node<FreyaNearNatureBombTrigger, FreyaMoveAwayNatureBombAction>(ACTION_RAID + 4);

    // The main tank answers a bomb by moving Freya, not itself. Bombs land at players' feet and the
    // melee stack is on the boss, so a volley buries her melee ring and the rest of the melee lose it.
    e.Node<FreyaTankNatureBombTrigger, FreyaTankNatureBombAction>(ACTION_RAID + 4);

    // The two 8 yd circles. Both sit with the other escapes, above the spore node, which would otherwise
    // walk the carrier straight back into the ball.
    e.Node<FreyaNaturesFuryBailTrigger, FreyaNaturesFuryBailAction>(ACTION_RAID + 4);
    e.Node<FreyaStepOutOfSunbeamTrigger, FreyaStepOutOfSunbeamAction>(ACTION_RAID + 4);

    // Detonating Lasher wave, which outruns the raid and wipes its threat every 10s. Leave a blast first (a dead
    // bot does no CC), then root, snare, ghouls, and gather last, only when nothing urgent is asking.
    e.Node<FreyaLasherAboutToBlowTrigger, FreyaLasherAboutToBlowAction>(ACTION_RAID + 3);
    e.Node<FreyaFrostNovaLashersTrigger, FreyaFrostNovaLashersAction>(ACTION_RAID + 2);
    e.Node<FreyaTrapLashersTrigger, FreyaTrapLashersAction>(ACTION_RAID + 2);
    e.Node<FreyaSummonArmyTrigger, FreyaSummonArmyAction>(ACTION_RAID + 2);
    e.Node<FreyaRangedCampTrigger, FreyaRangedCampAction>(ACTION_RAID);

    // Over the priority node, which would otherwise keep a bot it left on Freya shooting her.
    e.Node<FreyaReachHeldPackTrigger, FreyaReachHeldPackAction>(ACTION_RAID + 1);

    // Conservator's Grip is raid-wide and cannot be outranged, so a spore outranks attacking: a
    // pacified bot cannot swing at anything anyway.
    e.Node<FreyaMoveToHealingSporeTrigger, FreyaMoveToHealingSporeAction>(ACTION_RAID + 2);

    e.Node<FreyaTankAddsTrigger, FreyaTankAddsAction>(ACTION_RAID + 1);

    // Freya walks after whoever holds her, so every tank escape moves her. Under the bomb escape, which
    // has to be able to leave the leash.
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

    // Hard mode (config-gated): break out of Iron Roots and dodge the Unstable Sun Beam. Breaking the
    // root outranks the dodge: a rooted bot can't move, so it has to free itself before it can step out.
    // Hand of Freedom and Blink first, since either frees the bot at once.
    e.Node<FreyaFreeRootedAllyTrigger, FreyaFreeRootedAllyAction>(ACTION_RAID + 5);
    e.Node<FreyaBreakIronRootsTrigger, FreyaBreakIronRootsAction>(ACTION_RAID + 5);
    e.Node<FreyaDodgeUnstableSunBeamTrigger, FreyaDodgeUnstableSunBeamAction>(ACTION_RAID + 4);
    e.Node<FreyaGroundTremorHoldCastTrigger, FreyaGroundTremorHoldCastAction>(ACTION_EMERGENCY + 2);

    // The generic picker would reclaim the trio split on alternating ticks. Tank assist is off for every
    // tank all fight: gating it on the ladder lets an off-tank drag a lasher wave into the raid stack.
    e.OwnTargeting("freya disable automatic targeting", Role::Dps, FreyaOwnsTargets, Family::DpsAssist);
    e.OwnTargeting("freya disable automatic targeting", Role::Tank, FreyaOwnsTargets, Family::TankAssist);

    e.Multiplier<FreyaTrioSyncMultiplier>(Family::Melee | Family::Spell);
    e.Multiplier<FreyaSnaplasherHoldMultiplier>(Family::AnyAction);

    // Whether an action is AoE is its threat type, which any action can override.
    e.Multiplier<FreyaLasherFinishAoeMultiplier>(Family::AnyAction);

    e.Multiplier<FreyaLasherTrapReserveMultiplier>(Family::Spell);
    e.Multiplier<FreyaArmyRuneReserveMultiplier>(Family::Spell);

    // Avoid AoE sits at ACTION_EMERGENCY above every Freya escape, and swaps their all-hazard answer for
    // a flat FleeDistance hop that clears none of them.
    e.Block("freya avoid aoe hold", Role::Any, FreyaAlive, Family::AvoidAoe);

    // Ghouls hold lashers better than any tank, and the class chain taunts one off a ghoul, or Righteous Defenses
    // the ghoul itself, on every cooldown. The add tank's pickup casts its own, only for lashers on players.
    e.Block("freya lasher taunt reserve", Role::Tank, FreyaTankOnLasher, 0,
            {"hand of reckoning", "righteous defense", "taunt", "growl", "dark command", "death grip", "mocking blow"});

    // Trio and Conservator waves stand next to Freya, so a generic area taunt pulls her off the main tank.
    // The lasher pickup casts its own shouts, only with Freya out of reach.
    e.Block("freya area taunt hold", Role::Tank, FreyaAlive, 0, {"challenging shout", "challenging roar"});

    // Righteous Defense taunts everything hitting its target, so aimed at the other tank it pulls the Snaplasher too.
    e.Block("freya righteous defense hold", Role::Tank, FreyaTargetOnOtherTank, 0, {"righteous defense"});

    // Freya faces her tank, away from the bombs, so "behind" walks a melee bot from a clear spot in her reach toward
    // them. Reach melee stays open: a walk into a blast re-fires the escape, which aims back into reach.
    e.Block("freya nature bomb melee guard", Role::Melee, FreyaMeleeNearBomb, 0, {"set behind"});

    // The lasher bail stops at the blast's edge, so reach melee would walk the bot right back to a target still in it.
    // Named rather than Family::Reach, which also covers a battle res walking to its corpse.
    e.Block("freya lasher blast reach guard", Role::Melee, FreyaMeleeReachIntoBlast, Family::Charge, {"reach melee"});
    e.Block("freya lasher blast behind guard", Role::Melee, FreyaMeleeBehindIntoBlast, Family::SetBehind);

    e.Multiplier<FreyaGroundTremorCastGateMultiplier>(Family::Spell);
    e.Multiplier<BossNatureAspectHoldMultiplier>(Family::Spell, "freya");

    e.Tick(TickFreya);
}
}  // namespace

EncounterDefinition const& UldFreyaDefinition()
{
    static EncounterDefinition const definition(ULD_BOSS_FREYA, BossStateGate, "freya", &DefineFreya);
    return definition;
}
