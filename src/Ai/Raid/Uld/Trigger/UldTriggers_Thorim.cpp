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

bool ThorimDpsPriorityTrigger::IsActive()
{
    // Cheap gate first. This node runs for every bot in the instance on every tick, and everything
    // below it walks the nearby-unit list. The wing rather than a flat 110 yd: the upper hallway is
    // 105-176 yd out, and this is the only node that takes a forbidden target back off a bot, so a
    // radius that stops short of the platform leaves the corridor squad stuck on the boss up there.
    if (!NearThorimEncounter(bot))
        return false;

    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    // A healer's target drives its wand and its offensive dispels rather than a rotation, so steering
    // it every tick would fight the healing. Pull it off something it must not be holding, no more.
    if (botAI->IsHeal(bot))
        return currentTarget && !ThorimDpsTargetAllowed(botAI, currentTarget);

    // Same for a tank in phase 1: "tank assist" keeps it on whatever is swinging at the raid,
    // ThorimDisableAutomaticTargetingMultiplier leaves that action alone there, and two live pickers
    // on one bot is the oscillation this node exists to end. Phase 2 mutes tank assist instead, so
    // the tank steers off this like everybody else - without it an off-tank waiting to swap has no
    // picker at all and sits on whatever add it held when the boss dropped.
    if (botAI->IsTank(bot) && !ThorimPhase2Active(botAI))
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

    // Same stand-down the arena node carries. Without it the anchor walks a bot straight back into
    // the hazard it just dodged: the four ranged who took the most Blizzard damage in the hard mode
    // trace were also the four who ran the furthest, in and out of it.
    ThorimSifBlizzardTrigger blizzard(botAI);
    if (blizzard.IsActive())
        return false;

    ThorimSifFrostNovaTrigger frostNova(botAI);
    if (frostNova.IsActive())
        return false;

    ThorimPhase2Role const role = GetThorimPhase2Role(botAI, bot);
    if (role == ThorimPhase2Role::None)
        return false;

    Position spot;
    if (!TryGetThorimPhase2Spot(botAI, bot, role, spot))
        return false;

    Unit* boss = GetThorim(botAI);
    bool const holdsBoss = boss && boss->GetVictim() == bot;

    // Whoever has him walks him to the anchor, and only that one; a second tank would drag him back.
    // The off-tank counts here because his own ring point is measured off the boss, so towing him from
    // that has the spot walking away as fast as he chases it and the pair drift across the room.
    if (role == ThorimPhase2Role::MainTank || (role == ThorimPhase2Role::OffTank && holdsBoss))
        return holdsBoss && bot->GetDistance(spot) > 1.0f;

    if (role == ThorimPhase2Role::Ranged)
    {
        if (bot->GetDistance(spot) <= 1.0f)
            return false;

        // Wait out a zone sitting on the spot rather than walk back into it and get flung off again.
        // Not while under the cone though: that is 10-36k against a 4-5k tick.
        return !ThorimSpotUnderBlizzard(bot, spot) || ThorimCampSpotInLitCone(botAI, *bot);
    }

    // Arrival is written here, where the "stay" answer shows up: the action only runs when there is a
    // move to make, so it never gets that answer. Fine in a trigger since it only widens the tolerance,
    // so a second ask this tick still says stay.
    if (!ThorimRingWantsMove(botAI, bot, spot))
    {
        ThorimRingMarkArrived(bot);
        return false;
    }

    return true;
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
    // The only tank node that reads the boss without an aura on the bot to prove it is at him, so it
    // needs the wing test of its own now that the handle answers from anywhere on the map.
    if (!NearThorimEncounter(bot))
        return false;

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

bool ThorimTankPickupTrigger::IsActive()
{
    if (!ThorimPhase2Active(botAI))
        return false;

    Unit* boss = GetThorim(botAI);
    if (!boss || !boss->IsInWorld() || boss->IsDuringRemoveFromWorld())
        return false;

    if (!boss->IsAlive() || !boss->IsHostileTo(bot))
        return false;

    // Nothing to reach from up there, and the walk back is the whole corridor. The balcony node
    // drops the bot first, same as the phase 2 spot does.
    if (bot->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
        return false;

    Unit* victim = boss->GetVictim();
    if (victim == bot)
        return false;

    // A tank already has him, so this is the swap node's call from here. Answering anyway is the
    // second taunter that drags him back and forth.
    if (Player* holder = victim ? victim->ToPlayer() : nullptr)
        if (PlayerbotAI::IsTank(holder))
            return false;

    ThorimPhase2Role const role = GetThorimPhase2Role(botAI, bot);
    if (role == ThorimPhase2Role::MainTank)
        return true;

    if (role != ThorimPhase2Role::OffTank)
        return false;

    // Off-tank only covers when there is no main tank left to do it - he is the corridor tank and
    // still running back at the phase change, and two of them answering is the ping-pong again.
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    ObjectGuid const mainTank = PlayerbotAI::GetMainTankGuid(group);
    if (mainTank.IsEmpty())
        return true;

    Player* tank = ObjectAccessor::FindPlayer(mainTank);
    return !tank || !tank->IsAlive() || !NearThorimEncounter(tank);
}

bool ThorimSifBlizzardTrigger::IsActive()
{
    if (!IsThorimHardModeActive(botAI))
        return false;

    // No tank dodges this in phase 2. It used to be "whoever holds him", and a human tank's taunt
    // flipped that off for the half second it took the dodge to fire - which does not step out of the
    // blizzard, it takes the farthest of eight compass rays out to 30 yd. A tank standing on the
    // anchor went 30 yd into the middle, taunted the boss back from there, and towed him into the
    // ranged camp. Blizzard costs a tank ~100k a pull; one swing of the boss on a clothie is 11-38k.
    Unit* boss = GetThorim(botAI);
    if (boss && boss->GetVictim() == bot)
        return false;

    if (ThorimPhase2Active(botAI))
    {
        ThorimPhase2Role const role = GetThorimPhase2Role(botAI, bot);
        if (role == ThorimPhase2Role::MainTank || role == ThorimPhase2Role::OffTank)
            return false;

        // The ring answers this itself now, by sliding along its own radius to a clear bearing. This
        // node takes the furthest of eight rays out to 30 yd, which is how every accepted melee flee
        // ended up asking for the full 30 and landing a median 35 yd off the boss - out of melee, and
        // back in a Blizzard within 6s a quarter to half the time anyway.
        if (role == ThorimPhase2Role::MeleeRing)
            return false;

        // The camp tests the zones themselves at their real reach, not the bunny at 15: every home spot
        // clears the zone path by 12.9 yd, so a bot at home never has to move. A bot under the cone
        // leaves the Blizzard to its shelter walk. The test is flat, hence the floor check: a bot still
        // on the balcony would otherwise dodge zones on the floor below it.
        if (role == ThorimPhase2Role::Ranged)
            return bot->GetPositionZ() <= ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD && ThorimSpotUnderBlizzard(bot, *bot) &&
                   !ThorimCampSpotInLitCone(botAI, *bot);
    }

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
