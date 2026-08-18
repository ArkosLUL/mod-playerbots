#include "UldTriggers_Thorim.h"

#include "GameObject.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldBossHelper.h"
#include "UldEncounter_Thorim.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "RangeTriggers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "Vehicle.h"
#include <MovementActions.h>
#include <FollowMasterStrategy.h>
#include <RtiTargetValue.h>

bool ThorimUnbalancingStrikeTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "thorim");
    if (!boss || !boss->IsInWorld() || boss->IsDuringRemoveFromWorld())
        return false;

    if (!boss->IsAlive())
        return false;

    if (!boss->IsHostileTo(bot))
        return false;

    return bot->HasAura(SPELL_UNBALANCING_STRIKE);
}

bool ThorimMarkDpsTargetTrigger::IsActive()
{
    if (bot->GetDistance(ULDUAR_THORIM_NEAR_ARENA_CENTER) > 110.0f)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    if (botAI->IsMainTank(bot))
    {
        ObjectGuid currentSkullTarget = group->GetTargetIcon(RtiTargetValue::skullIndex);
        Unit* currentSkullUnit = botAI->GetUnit(currentSkullTarget);
        if (currentSkullUnit && !currentSkullUnit->IsAlive())
        {
            currentSkullUnit = nullptr;
        }

        Unit* acolyte = AI_VALUE2(Unit*, "find target", "dark rune acolyte");
        Unit* evoker = AI_VALUE2(Unit*, "find target", "dark rune evoker");

        if (acolyte && acolyte->IsAlive() && bot->GetDistance(acolyte) < 50.0f &&
            (!currentSkullUnit || currentSkullUnit->GetEntry() != acolyte->GetEntry()))
            return true;

        if (evoker && evoker->IsAlive() && bot->GetDistance(evoker) < 50.0f &&
            (!currentSkullUnit || currentSkullUnit->GetEntry() != evoker->GetEntry()))
            return true;

        Unit* boss = AI_VALUE2(Unit*, "find target", "thorim");

        if (!boss || !boss->IsInWorld() || boss->IsDuringRemoveFromWorld())
            return false;

        if (!boss->IsAlive())
            return false;

        if (!boss->IsHostileTo(bot))
            return false;

        if (boss->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD && (!currentSkullUnit || !currentSkullUnit->IsAlive()))
        {
            group->SetTargetIcon(RtiTargetValue::skullIndex, bot->GetGUID(), boss->GetGUID());
            return true;
        }

        return false;
    }
    else if (botAI->IsAssistTankOfIndex(bot, 0))
    {
        Player* mainTank = nullptr;
        for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
        {
            Player* member = gref->GetSource();
            if (member && botAI->IsMainTank(member))
            {
                mainTank = member;
                break;
            }
        }

        if (mainTank && bot->GetDistance(mainTank) < 30.0f)
            return false;

        ObjectGuid currentCrossTarget = group->GetTargetIcon(RtiTargetValue::crossIndex);
        Unit* currentCrossUnit = botAI->GetUnit(currentCrossTarget);
        if (currentCrossUnit && !currentCrossUnit->IsAlive())
        {
            currentCrossUnit = nullptr;
        }

        Unit* acolyte = AI_VALUE2(Unit*, "find target", "dark rune acolyte");
        if (currentCrossUnit && currentCrossUnit->GetEntry() == NPC_DARK_RUNE_ACOLYTE_I)
            return false;

        Unit* runicColossus = AI_VALUE2(Unit*, "find target", "runic colossus");
        Unit* ancientRuneGiant = AI_VALUE2(Unit*, "find target", "ancient rune giant");

        if (acolyte && acolyte->IsAlive() && (!currentCrossUnit || currentCrossUnit->GetEntry() != acolyte->GetEntry()))
            return true;

        if (currentCrossUnit && currentCrossUnit->GetEntry() == NPC_RUNIC_COLOSSUS)
            return false;
        if (runicColossus && runicColossus->IsAlive() &&
            (!currentCrossUnit || currentCrossUnit->GetEntry() != runicColossus->GetEntry()))
            return true;

        if (currentCrossUnit && currentCrossUnit->GetEntry() == NPC_ANCIENT_RUNE_GIANT)
            return false;
        if (ancientRuneGiant && ancientRuneGiant->IsAlive() &&
            (!currentCrossUnit || currentCrossUnit->GetEntry() != ancientRuneGiant->GetEntry()))
            return true;

        return false;
    }

    return false;
}

bool ThorimGauntletPositioningTrigger::IsActive()
{
    if (bot->GetDistance(ULDUAR_THORIM_NEAR_ARENA_CENTER) > 110.0f)
        return false;

    if (GetThorimSquad(botAI, bot) != ThorimSquad::Gauntlet)
        return false;

    Unit* master = botAI->GetMaster();
    if (!master)
        return false;

    if (master->GetDistance(ULDUAR_THORIM_NEAR_ENTRANCE_POSITION) < 10.0f && (bot->GetDistance2d(master) > 5.0f))
    {
        return true;
    }

    uint8 masterIndex = 0;
    bool leftLane = false;
    if (ThorimGauntletLaneIndex(master, masterIndex, leftLane))
    {
        bool preferredLane = false;
        if (ThorimPreferredGauntletLane(botAI, preferredLane))
            leftLane = preferredLane;

        uint8 botIndex = 0;
        if (!ThorimGauntletLaneIndexInLane(bot, leftLane, botIndex))
        {
            if (bot->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
                return false;

            return true;
        }
    }

    Unit* boss = AI_VALUE2(Unit*, "find target", "thorim");
    if (boss && boss->IsAlive() && bot->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD &&
        boss->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
    {
        return true;
    }

    return false;
}

bool ThorimArenaPositioningTrigger::IsActive()
{
    Position anchor;
    if (!GetThorimArenaAnchor(botAI, bot, anchor))
        return false;

    // Surviving beats standing on a spot, and letting the anchor fight a dodge is what has a bot step
    // out of a hazard and get walked straight back into it.
    ThorimSifBlizzardTrigger blizzard(botAI);
    if (blizzard.IsActive())
        return false;

    ThorimSifFrostNovaTrigger frostNova(botAI);
    if (frostNova.IsActive())
        return false;

    return ThorimArenaAnchorNeedsMove(botAI, bot, anchor);
}

bool ThorimFallFromFloorTrigger::IsActive()
{
    if (bot->GetDistance(ULDUAR_THORIM_NEAR_ARENA_CENTER) > 110.0f)
        return false;

    // Check if bot is on the floor
    return bot->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_PATHING_ISSUE_DETECT;
}

bool ThorimPhase2PositioningTrigger::IsActive()
{
    if (!ThorimPhase2Active(botAI))
        return false;

    ThorimPhase2Role const role = GetThorimPhase2Role(botAI, bot);
    if (role == ThorimPhase2Role::None)
        return false;

    Position spot;
    if (!TryGetThorimPhase2Spot(botAI, bot, role, spot))
        return false;

    if (role == ThorimPhase2Role::MainTank)
    {
        // Only the tank actually holding the boss walks him south; a second one would drag him back.
        Unit* boss = GetThorim(botAI);
        return boss && boss->GetVictim() == bot && bot->GetDistance(spot) > 1.0f;
    }

    if (role == ThorimPhase2Role::Ranged)
        return bot->GetDistance(spot) > 1.0f;

    return ThorimRingNeedsMove(botAI, bot, spot);
}

bool ThorimRunicSmashTrigger::IsActive()
{
    bool safeLane = false;
    if (!ThorimRunicSmashImminent(botAI) || !ThorimPreferredGauntletLane(botAI, safeLane))
        return false;

    // A bot still up on the balcony has its own node to get down and nothing to dodge up there.
    if (bot->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
        return false;

    // The centre line is inside the blast too, so this asks for the safe lane rather than "not the
    // hot one".
    uint8 index = 0;
    if (ThorimGauntletLaneIndexInLane(bot, safeLane, index))
        return false;

    return ThorimResolveGauntletIndex(botAI, bot, index);
}

bool ThorimRunicBarrierBailTrigger::IsActive()
{
    if (!ThorimBarrierBailLatched(botAI, bot))
        return false;

    Unit* colossus = GetThorimRunicColossus(botAI);
    return colossus && bot->GetDistance(colossus) < ULDUAR_THORIM_BARRIER_BAIL_DISTANCE;
}

bool ThorimLightningChargeTrigger::IsActive()
{
    // Both tanks, ranged and healers hold and eat it: moving a tank drags the boss and re-anchors the
    // whole ring, and the ranged spots are already outside anything the rotation would buy them.
    if (GetThorimPhase2Role(botAI, bot) != ThorimPhase2Role::MeleeRing)
        return false;

    if (!ThorimLightningChargeActive(botAI))
        return false;

    Position spot;
    if (!TryGetThorimPhase2Spot(botAI, bot, ThorimPhase2Role::MeleeRing, spot))
        return false;

    // Raw distance, not the arrival latch: the dodge must not be gated by a bot that was settled on
    // the slot the ring has just rotated away from.
    return bot->GetDistance(spot) > ULDUAR_THORIM_RING_ARRIVE_TOLERANCE;
}

bool ThorimArenaLeashTrigger::IsActive() { return ThorimArenaLeashBreached(botAI, bot); }

bool ThorimResetEncounterStateTrigger::IsActive()
{
    return ThorimBotHasEncounterState(bot) && ThorimEncounterStateIsStale(botAI);
}

//
// Thorim
//
bool ThorimUnbalancingStrikeSwapTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "thorim");
    if (!boss || !boss->IsInWorld() || boss->IsDuringRemoveFromWorld())
        return false;

    if (!boss->IsAlive() || !boss->IsHostileTo(bot))
        return false;

    bool const isMainTank = botAI->IsMainTank(bot);
    bool const isFirstAssistTank = botAI->IsAssistTankOfIndex(bot, 0);
    if (!isMainTank && !isFirstAssistTank)
        return false;

    // This bot must be the off-tank, not the one currently holding the boss
    Unit* activeTank = boss->GetVictim();
    if (!activeTank || activeTank == bot)
        return false;

    Player* activeTankPlayer = activeTank->ToPlayer();
    if (!activeTankPlayer)
        return false;

    // The active tank must be this bot's swap partner so the taunt is symmetric both ways
    bool const partnerIsSwapTank = isMainTank ? PlayerbotAI::IsAssistTankOfIndex(activeTankPlayer, 0)
                                              : PlayerbotAI::IsMainTank(activeTankPlayer);
    if (!partnerIsSwapTank)
        return false;

    // Don't taunt while this bot still carries Unbalancing Strike (let it fall off first)
    if (bot->HasAura(SPELL_UNBALANCING_STRIKE))
        return false;

    // Swap in once the active tank is suffering Unbalancing Strike
    return activeTank->HasAura(SPELL_UNBALANCING_STRIKE);
}

bool ThorimSifBlizzardTrigger::IsActive()
{
    if (!IsThorimHardModeActive(botAI))
        return false;

    TooCloseToCreatureTrigger tooCloseToBlizzard(botAI);
    return tooCloseToBlizzard.TooCloseToCreature(NPC_SIF_BLIZZARD, ULDUAR_THORIM_SIF_BLIZZARD_RADIUS);
}

bool ThorimSifFrostNovaTrigger::IsActive()
{
    if (!IsThorimHardModeActive(botAI))
        return false;

    // Melee stay on Sif; only ranged/healers keep clear of her point-blank Frost Nova.
    if (!PlayerbotAI::IsRanged(bot))
        return false;

    // Sif is spawned at Thorim's throne in both modes and only drops onto the arena floor once she
    // interrupts her channel to join the fight. While she is still up there she casts nothing, so
    // backing away from her would only stall gauntlet DPS during the timed race.
    Unit* sif = GetFirstAliveUnitByEntry(botAI, NPC_SIF);
    if (!sif || sif->GetPositionZ() >= ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
        return false;

    TooCloseToCreatureTrigger tooCloseToSif(botAI);
    return tooCloseToSif.TooCloseToCreature(NPC_SIF, ULDUAR_THORIM_SIF_FROST_NOVA_RADIUS);
}
