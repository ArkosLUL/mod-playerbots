#include "UldTriggers_Thorim.h"

#include "GameObject.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldEncounter_Thorim.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "EncounterHelpers.h"
#include "RangeTriggers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "Vehicle.h"
#include <MovementActions.h>
#include <FollowMasterStrategy.h>

using namespace EncounterHelpers;

bool ThorimUnbalancingStrikeTrigger::IsActive()
{
    Unit* boss = GetThorim(botAI);
    if (!boss || !boss->IsInWorld() || boss->IsDuringRemoveFromWorld())
        return false;

    if (!boss->IsAlive())
        return false;

    if (!boss->IsHostileTo(bot))
        return false;

    return bot->HasAura(SPELL_UNBALANCING_STRIKE);
}

bool ThorimDpsPriorityTrigger::IsActive()
{
    // Cheap gate first. This node runs for every bot in the instance on every tick, and everything
    // below it walks the nearby-unit list.
    if (bot->GetDistance(ULDUAR_THORIM_NEAR_ARENA_CENTER) > 110.0f)
        return false;

    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    // A healer's target drives its wand and its offensive dispels rather than a rotation, so steering
    // it every tick would fight the healing. Pull it off something it must not be holding, no more.
    if (botAI->IsHeal(bot))
        return currentTarget && !ThorimDpsTargetAllowed(botAI, currentTarget);

    // Same for a tank, and for the same reason: "tank assist" keeps it on whatever is swinging at the
    // raid, ThorimDisableAutomaticTargetingMultiplier deliberately leaves that action alone, and two
    // live pickers on one bot is the oscillation this node exists to end.
    if (botAI->IsTank(bot))
        return currentTarget && !ThorimDpsTargetAllowed(botAI, currentTarget);

    if (currentTarget && !ThorimDpsTargetAllowed(botAI, currentTarget))
        return true;

    Unit* target = GetThorimDpsTarget(botAI, bot, currentTarget);
    return target && target != currentTarget;
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

    return false;
}

bool ThorimBalconyAdvanceTrigger::IsActive()
{
    if (GetThorimSquad(botAI, bot) != ThorimSquad::Gauntlet)
        return false;

    // Deliberately no master check. The corridor leg follows whoever is leading it, but the hallway is
    // the last thing between the squad and phase 2, and it has to get walked whether or not there is a
    // human up here to walk behind.
    return ThorimBalconyOpen(botAI, bot);
}

bool ThorimArenaPositioningTrigger::IsActive()
{
    Position anchor;
    if (!GetThorimArenaAnchor(botAI, bot, anchor))
        return false;

    // Surviving beats standing on a spot, and letting the anchor fight a dodge is what has a bot step
    // out of a hazard and get walked straight back into it.
    ThorimChargedOrbTrigger chargedOrb(botAI);
    if (chargedOrb.IsActive())
        return false;

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

    // Only the corridor squad has a lane to be in. Without this an arena bot answers the telegraph
    // too: ThorimResolveGauntletIndex falls back to the master's waypoint, and the master is down the
    // corridor, so the dodge walks it 110 yd out of the arena and it never comes back.
    if (GetThorimSquad(botAI, bot) != ThorimSquad::Gauntlet)
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

bool ThorimPetLeashTrigger::IsActive()
{
    std::vector<Unit*> stray;
    return ThorimStrayPets(botAI, bot, stray);
}

bool ThorimChargedOrbTrigger::IsActive()
{
    Position spot;
    return ThorimChargedOrbEscape(botAI, bot, spot);
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
    Unit* boss = GetThorim(botAI);
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
