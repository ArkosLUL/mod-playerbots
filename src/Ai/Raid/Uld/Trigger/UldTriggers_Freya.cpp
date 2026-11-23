#include "UldTriggers_Freya.h"

#include "GameObject.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "RaidTankDefensive.h"
#include "UldEncounter_Freya.h"
#include "UldScripts.h"
#include "EncounterHelpers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "Vehicle.h"
#include <MovementActions.h>
#include <FollowMasterStrategy.h>

using namespace EncounterHelpers;

bool FreyaNearNatureBombTrigger::IsActive()
{
    Unit* boss = GetFreyaScan(botAI).Boss();
    if (!boss || !boss->IsAlive())
        return false;

    if (PlayerbotAI::IsTank(bot))
        return false;

    return bot->FindNearestGameObject(GOBJECT_NATURE_BOMB, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS) != nullptr;
}

bool FreyaTankNatureBombTrigger::IsActive()
{
    Unit* boss = GetFreyaScan(botAI).Boss();
    if (!boss || !boss->IsAlive())
        return false;

    if (!PlayerbotAI::IsMainTank(bot))
        return false;

    if (bot->FindNearestGameObject(GOBJECT_NATURE_BOMB, ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS))
        return true;

    return boss->GetVictim() == bot &&
           boss->FindNearestGameObject(GOBJECT_NATURE_BOMB, ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS) != nullptr;
}

bool FreyaSetDpsPriorityTrigger::IsActive()
{
    Unit* boss = GetFreyaScan(botAI).Boss();
    if (!boss || !boss->IsAlive())
        return false;

    return PlayerbotAI::IsDps(bot);
}

bool FreyaTankAddsTrigger::IsActive()
{
    Unit* boss = GetFreyaScan(botAI).Boss();
    if (!boss || !boss->IsAlive())
        return false;

    return PlayerbotAI::IsMainTank(bot) || PlayerbotAI::IsAssistTankOfIndex(bot, 0, true);
}

bool FreyaKiteSnaplasherTrigger::IsActive()
{
    Unit* boss = GetFreyaScan(botAI).Boss();
    if (!boss || !boss->IsAlive())
        return false;

    Unit* snaplasher = GetFreyaSnaplasherToKite(botAI);
    if (!snaplasher)
        return false;

    return bot->GetExactDist2d(snaplasher) <
           GetFreyaSnaplasherKiteReach(snaplasher, bot, ULDUAR_FREYA_SNAPLASHER_KITE_MARGIN);
}

bool FreyaAddTankDefensiveTrigger::IsActive()
{
    if (!PlayerbotAI::IsTank(bot))
        return false;

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);
    if (GetFreyaStackedSnaplasherTank(state) != bot)
        return false;

    if (HasFreyaTankExternal(bot))
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "freya.adddef", "covered");

        return false;
    }

    return NextTankDefensive(botAI, bot, "freya.adddef", true) != 0;
}

bool FreyaAddTankExternalTrigger::IsActive()
{
    if (!PlayerbotAI::IsHeal(bot))
        return false;

    Player* tank = nullptr;
    char const* external = GetFreyaTankExternal(botAI, tank);
    if (!external && tank && RaidObs::Active())
        RaidObs::NoteDerived(bot, "freya.addext", IsFreyaAddTankCovered(tank) ? "covered" : "none");

    return external != nullptr;
}

bool FreyaRedirectThreatTrigger::IsActive()
{
    if (bot->getClass() != CLASS_HUNTER && bot->getClass() != CLASS_ROGUE)
        return false;

    Unit* boss = GetFreyaScan(botAI).Boss();

    return boss && boss->IsAlive();
}

bool FreyaMoveToHealingSporeTrigger::IsActive()
{
    Unit* boss = GetFreyaScan(botAI).Boss();
    if (!boss || !boss->IsAlive())
        return false;

    if (PlayerbotAI::IsTank(bot))
        return false;

    if (bot->HasAura(SPELL_POTENT_PHEROMONES))
        return false;

    if (!GetFreyaScanUnitByEntry(botAI, NPC_ANCIENT_CONSERVATOR))
        return false;

    Unit* spore = GetFreyaTargetSpore(botAI);
    if (!spore)
        return false;

    return bot->GetExactDist2d(spore) > ULDUAR_FREYA_SPORE_RADIUS - 1.0f;
}

bool FreyaBreakIronRootsTrigger::IsActive()
{
    return IsFreyaRooted(bot);
}

bool FreyaFreeRootedAllyTrigger::IsActive()
{
    Unit* boss = GetFreyaScan(botAI).Boss();
    if (!boss || !boss->IsAlive())
        return false;

    switch (bot->getClass())
    {
        case CLASS_MAGE:
            return IsFreyaRooted(bot) && GetFreyaHazardsUnder(botAI, bot) > 0;
        case CLASS_PALADIN:
            return GetFreyaRootedAllyToFree(botAI) != nullptr;
        default:
            return false;
    }
}

bool FreyaDodgeUnstableSunBeamTrigger::IsActive()
{
    for (ObjectGuid const& guid : GetFreyaScan(botAI).Stalkers())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() != NPC_FREYA_SUN_BEAM && unit->GetEntry() != NPC_FREYA_UNSTABLE_SUN_BEAM)
            continue;

        if (bot->GetExactDist2d(unit) < ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS)
            return true;
    }

    return false;
}

bool FreyaLasherAboutToBlowTrigger::IsActive()
{
    Unit* boss = GetFreyaScan(botAI).Boss();
    if (!boss || !boss->IsAlive())
        return false;

    if (PlayerbotAI::IsTank(bot) || !PlayerbotAI::IsMelee(bot))
        return false;

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    return !GetFreyaLowLasherPositions(botAI, state, ULDUAR_FREYA_LASHER_BAIL_PCT, ULDUAR_FREYA_DETONATE_RADIUS)
                .empty();
}

bool FreyaRangedCampTrigger::IsActive()
{
    Unit* boss = GetFreyaScan(botAI).Boss();
    if (!boss || !boss->IsAlive())
        return false;

    if (PlayerbotAI::IsTank(bot) || !(PlayerbotAI::IsRangedDps(bot) || PlayerbotAI::IsHeal(bot)))
        return false;

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);
    if (state.detonatingLashers.empty())
        return false;

    std::vector<Position> const blasts =
        GetFreyaLowLasherPositions(botAI, state, ULDUAR_FREYA_LASHER_BAIL_PCT, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS);
    for (Position const& blast : blasts)
        if (bot->GetExactDist2d(blast.GetPositionX(), blast.GetPositionY()) <= ULDUAR_FREYA_DETONATE_RADIUS)
            return true;

    bool submerged = false;
    for (Unit* lasher : state.detonatingLashers)
    {
        if (!IsFreyaLasherSubmerged(lasher))
            continue;

        if (bot->GetExactDist2d(lasher) < ULDUAR_FREYA_LASHER_CAMP_STANDOFF)
            return true;

        submerged = true;
    }

    if (!blasts.empty() || submerged)
        return false;

    if (PlayerbotAI::IsRangedDps(bot) && IsFreyaLasherHeldByGhoul(botAI, GetFreyaRangedLasherAim(state)))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (PlayerbotAI::IsRangedDps(bot) && target && target->IsAlive() && target->GetEntry() == NPC_EONARS_GIFT)
        return false;

    Position const camp = GetFreyaLasherCampSpot(botAI);
    if (camp == Position())
        return false;

    float const tolerance =
        PlayerbotAI::IsHeal(bot) ? ULDUAR_FREYA_HEALER_CAMP_TOLERANCE : ULDUAR_FREYA_RANGED_CAMP_TOLERANCE;

    return bot->GetExactDist2d(camp.GetPositionX(), camp.GetPositionY()) > tolerance;
}

bool FreyaReachHeldPackTrigger::IsActive()
{
    if (PlayerbotAI::IsTank(bot) || !PlayerbotAI::IsRangedDps(bot))
        return false;

    Unit* boss = GetFreyaScan(botAI).Boss();
    if (!boss || !boss->IsAlive())
        return false;

    if (AI_VALUE(Unit*, "current target") != boss)
        return false;

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);
    if (state.detonatingLashers.empty() ||
        CountFreyaLashersNear(bot->GetPosition(), state, sPlayerbotAIConfig.SpellDistance))
        return false;

    return IsFreyaLasherHeldByGhoul(botAI, GetFreyaRangedLasherAim(state));
}

bool FreyaReachGiftTrigger::IsActive()
{
    if (PlayerbotAI::IsTank(bot) || !PlayerbotAI::IsRangedDps(bot))
        return false;

    Unit* boss = GetFreyaScan(botAI).Boss();
    if (!boss || !boss->IsAlive())
        return false;

    Unit* gift = AI_VALUE(Unit*, "current target");
    if (!gift || !gift->IsAlive() || gift->GetEntry() != NPC_EONARS_GIFT ||
        bot->IsWithinCombatRange(gift, sPlayerbotAIConfig.SpellDistance))
        return false;

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    float const corridor = bot->GetExactDist2d(gift) + ULDUAR_FREYA_LASHER_CAMP_STANDOFF;
    std::vector<Position> const blasts =
        GetFreyaLowLasherPositions(botAI, state, ULDUAR_FREYA_LASHER_BAIL_PCT, corridor);
    for (Position const& blast : blasts)
        if (bot->GetExactDist2d(blast.GetPositionX(), blast.GetPositionY()) <= ULDUAR_FREYA_DETONATE_RADIUS)
            return false;

    return !blasts.empty() || !GetFreyaSubmergedLasherPositions(botAI, state, corridor).empty();
}

bool FreyaFrostNovaLashersTrigger::IsActive()
{
    if (bot->getClass() != CLASS_MAGE)
        return false;

    Unit* boss = GetFreyaScan(botAI).Boss();
    if (!boss || !boss->IsAlive())
        return false;

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    if (CountFreyaLashersNear(bot->GetPosition(), state, ULDUAR_FREYA_FROST_NOVA_RADIUS) <
        ULDUAR_FREYA_FROST_NOVA_MIN_LASHERS)
        return false;

    return botAI->CanCastSpell("frost nova", bot);
}

bool FreyaTrapLashersTrigger::IsActive()
{
    if (bot->getClass() != CLASS_HUNTER)
        return false;

    Unit* boss = GetFreyaScan(botAI).Boss();
    if (!boss || !boss->IsAlive())
        return false;

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    if (!CountFreyaLashersNear(bot->GetPosition(), state, ULDUAR_FREYA_FROST_TRAP_ARM_RANGE))
        return false;

    return botAI->CanCastSpell("frost trap", bot);
}

bool FreyaSummonArmyTrigger::IsActive()
{
    if (bot->getClass() != CLASS_DEATH_KNIGHT)
        return false;

    Unit* boss = GetFreyaScan(botAI).Boss();
    if (!boss || !boss->IsAlive())
        return false;

    FreyaWaveState state;
    GatherFreyaWaveState(botAI, state);

    if (state.detonatingLashers.empty())
        return false;

    // Keep the order: IsFreyaArmyTurn writes the ghoul mark HasUncoveredFreyaLasher reads.
    if (!IsFreyaArmyTurn(botAI) || !HasUncoveredFreyaLasher(botAI, state))
        return false;

    if (IsFreyaGroundTremorCasting(GetFreyaBossByEntry(botAI)))
        return false;

    return botAI->CanCastSpell("army of the dead", bot);
}

bool FreyaGroundTremorHoldCastTrigger::IsActive()
{
    if (!IsFreyaGroundTremorCasting(GetFreyaBossByEntry(botAI)))
        return false;

    return bot->HasUnitState(UNIT_STATE_CASTING) && !IsFreyaArmyChanneling(bot);
}

bool FreyaNaturesFuryBailTrigger::IsActive()
{
    Unit* boss = GetFreyaScan(botAI).Boss();
    if (!boss || !boss->IsAlive())
        return false;

    if (PlayerbotAI::IsTank(bot))
        return false;

    if (!bot->HasAura(SPELL_NATURES_FURY_10) && !bot->HasAura(SPELL_NATURES_FURY_25))
        return false;

    return CountFreyaRaidNear(botAI, bot->GetPosition(), ULDUAR_FREYA_NATURES_FURY_RADIUS, bot) > 0;
}

bool FreyaStepOutOfSunbeamTrigger::IsActive()
{
    Unit* boss = GetFreyaScan(botAI).Boss();
    if (!boss || !boss->IsAlive())
        return false;

    if (PlayerbotAI::IsTank(bot) || !(PlayerbotAI::IsRangedDps(bot) || PlayerbotAI::IsHeal(bot)))
        return false;

    Unit* target = GetFreyaSunbeamTarget(boss);
    if (!target || target == bot)
        return false;

    return bot->GetExactDist2d(target) <= ULDUAR_FREYA_SUNBEAM_AVOID_RADIUS;
}

bool FreyaTankHoldFreyaTrigger::IsActive()
{
    Unit* boss = GetFreyaScan(botAI).Boss();
    if (!boss || !boss->IsAlive())
        return false;

    if (!PlayerbotAI::IsMainTank(bot) || boss->GetVictim() != bot)
        return false;

    if (boss->GetExactDist2d(&ULDUAR_FREYA_TANK_ANCHOR) <= ULDUAR_FREYA_TANK_LEASH)
        return false;

    std::vector<Position> const bombs = GetFreyaNatureBombPositions(
        bot, bot->GetExactDist2d(&ULDUAR_FREYA_TANK_ANCHOR) + ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS);
    for (Position const& bomb : bombs)
        if (bomb.GetExactDist2d(&ULDUAR_FREYA_TANK_ANCHOR) < ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS)
            return false;

    return !FreyaWalkCrossesNatureBomb(bot->GetPosition(), ULDUAR_FREYA_TANK_ANCHOR, bombs,
                                       ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS);
}
