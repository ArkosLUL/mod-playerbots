#include "UldTriggers_Thorim.h"

#include "GameObject.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"
#include "UldEncounter_Thorim.h"
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
    if (!NearThorimEncounter(bot))
        return false;

    Unit* boss = GetThorim(botAI);
    if (!boss || !boss->IsInCombat())
        return false;

    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    if (PlayerbotAI::IsHeal(bot))
        return currentTarget && !ThorimDpsTargetAllowed(botAI, currentTarget);

    if (PlayerbotAI::IsTank(bot) && !ThorimPhase2Active(botAI))
    {
        if (Unit* colossus = ThorimGauntletColossusPull(botAI, bot))
            return colossus != currentTarget;

        if (Unit* giant = ThorimGauntletGiantHeld(botAI, bot))
            return giant != currentTarget;

        return currentTarget && !ThorimDpsTargetAllowed(botAI, currentTarget);
    }

    if (currentTarget && !ThorimDpsTargetAllowed(botAI, currentTarget))
        return true;

    Unit* target = GetThorimDpsTarget(botAI, bot, currentTarget);
    return target && target != currentTarget;
}

bool ThorimGauntletPositioningTrigger::IsActive()
{
    if (bot->GetDistance(ULDUAR_THORIM_NEAR_ARENA_CENTER) > ULDUAR_THORIM_ENCOUNTER_PROXIMITY)
        return false;

    if (GetThorimSquad(botAI, bot) != ThorimSquad::Gauntlet)
        return false;

    Player* leader = ThorimGauntletLeader(botAI);
    if (!leader || leader == bot)
        return false;

    if (leader->GetDistance(ULDUAR_THORIM_NEAR_ENTRANCE_POSITION) < ULDUAR_THORIM_GATE_RADIUS)
        return bot->GetDistance2d(leader) > ULDUAR_THORIM_GATE_FOLLOW_DISTANCE &&
               !ThorimFightingStartPack(botAI, bot);

    uint8 leaderIndex = 0;
    bool leftLane = false;
    if (ThorimGauntletLaneIndex(leader, leaderIndex, leftLane))
    {
        bool preferredLane = false;
        if (ThorimPreferredGauntletLane(botAI, preferredLane))
            leftLane = preferredLane;

        uint8 botIndex = 0;
        if (!ThorimGauntletLaneIndexInLane(bot, leftLane, botIndex))
            return bot->GetPositionZ() <= ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD;

        return botIndex < leaderIndex && !ThorimGauntletFighting(botAI, bot);
    }

    return ThorimTrailsGauntletLeader(botAI, bot, leader);
}

bool ThorimGauntletLeverTrigger::IsActive()
{
    if (!NearThorimEncounter(bot) || GetThorimSquad(botAI, bot) != ThorimSquad::Gauntlet)
        return false;

    if (!ThorimIsBotGauntletLeader(botAI, bot))
        return false;

    ObjectGuid lever;
    if (ThorimLeverUsable(bot, lever))
        return true;

    if (!ThorimLeverWaiting(botAI, bot, lever))
        return false;

    GameObject* go = botAI->GetGameObject(lever);
    return go && !go->IsAtInteractDistance(bot);
}

bool ThorimGauntletLeadTrigger::IsActive()
{
    if (!NearThorimEncounter(bot) || GetThorimSquad(botAI, bot) != ThorimSquad::Gauntlet)
        return false;

    if (!ThorimIsBotGauntletLeader(botAI, bot))
        return false;

    Position next;
    return ThorimGauntletLeadStep(botAI, bot, next) || ThorimGauntletLeadColossus(botAI, bot) ||
           ThorimGauntletLeadRuneGiant(botAI, bot) || ThorimGauntletGiantHold(botAI, bot, next);
}

bool ThorimBalconyAdvanceTrigger::IsActive()
{
    if (GetThorimSquad(botAI, bot) != ThorimSquad::Gauntlet)
        return false;

    return ThorimBalconyOpen(botAI, bot);
}

bool ThorimArenaPositioningTrigger::IsActive()
{
    Position anchor;
    if (!GetThorimArenaAnchor(botAI, bot, anchor))
        return false;

    ThorimChargedOrbTrigger chargedOrb(botAI);
    if (chargedOrb.IsActive())
        return false;

    ThorimSifBlizzardTrigger blizzard(botAI);
    if (blizzard.IsActive())
        return false;

    return ThorimArenaAnchorNeedsMove(botAI, bot, anchor);
}

bool ThorimFallFromFloorTrigger::IsActive()
{
    if (bot->GetDistance(ULDUAR_THORIM_NEAR_ARENA_CENTER) > ULDUAR_THORIM_FLOOR_RANGE)
        return false;

    return bot->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_PATHING_ISSUE_DETECT;
}

bool ThorimPhase2PositioningTrigger::IsActive()
{
    if (!ThorimPhase2Active(botAI))
        return false;

    ThorimPhase2Role const role = GetThorimPhase2Role(botAI, bot);
    if (role == ThorimPhase2Role::None)
        return false;

    if (role != ThorimPhase2Role::MeleeRing)
    {
        ThorimSifBlizzardTrigger blizzard(botAI);
        if (blizzard.IsActive())
            return false;
    }

    Position spot;
    if (!TryGetThorimPhase2Spot(botAI, bot, role, spot))
        return false;

    if (role == ThorimPhase2Role::MainTank || role == ThorimPhase2Role::OffTank)
        return ThorimTankSpotNeedsMove(botAI, bot, spot);

    if (role == ThorimPhase2Role::Ranged)
    {
        if (bot->GetDistance(spot) <= ULDUAR_THORIM_PHASE2_SPOT_TOLERANCE)
            return false;

        return !ThorimWalkUnderBlizzard(bot, *bot, spot) || ThorimCampSpotInLitCone(botAI, *bot);
    }

    if (ThorimRingBlizzardCovered(bot, spot))
        return false;

    if (!ThorimRingWantsMove(botAI, bot, spot))
    {
        ThorimRingMarkArrived(bot);
        return false;
    }

    return true;
}

bool ThorimUnreachableTrigger::IsActive()
{
    Unit* boss = ThorimUnreachableBy(botAI, bot);
    Position step;
    return boss && ThorimUnreachableStep(bot, boss, step);
}

bool ThorimRunicSmashTrigger::IsActive()
{
    bool safeLane = false;
    if (!ThorimRunicSmashImminent(botAI) || !ThorimPreferredGauntletLane(botAI, safeLane))
        return false;

    if (bot->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
        return false;

    if (GetThorimSquad(botAI, bot) != ThorimSquad::Gauntlet)
        return false;

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

bool ThorimFrostAuraTrigger::IsActive()
{
    return IsThorimFrostAuraPaladin(bot) && !botAI->HasAura("frost resistance aura", bot);
}

bool ThorimPetLeashTrigger::IsActive()
{
    std::vector<Unit*> pets;
    return ThorimStrayPets(botAI, bot, pets) || ThorimPetsOffRingSlot(botAI, bot, pets);
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

bool ThorimUnbalancingStrikeSwapTrigger::IsActive()
{
    if (!NearThorimEncounter(bot))
        return false;

    Unit* boss = GetThorim(botAI);
    if (!boss || !boss->IsInWorld() || boss->IsDuringRemoveFromWorld())
        return false;

    if (!boss->IsAlive() || !boss->IsHostileTo(bot))
        return false;

    bool const isMainTank = PlayerbotAI::IsMainTank(bot);
    bool const isFirstAssistTank = PlayerbotAI::IsAssistTankOfIndex(bot, 0);
    if (!isMainTank && !isFirstAssistTank)
        return false;

    Unit* activeTank = boss->GetVictim();
    if (!activeTank || activeTank == bot)
        return false;

    Player* activeTankPlayer = activeTank->ToPlayer();
    if (!activeTankPlayer)
        return false;

    bool const partnerIsSwapTank = isMainTank ? PlayerbotAI::IsAssistTankOfIndex(activeTankPlayer, 0)
                                              : PlayerbotAI::IsMainTank(activeTankPlayer);
    if (!partnerIsSwapTank)
        return false;

    if (!ThorimAtTankSpots(bot))
        return false;

    if (bot->HasAura(SPELL_UNBALANCING_STRIKE))
        return false;

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

    if (bot->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
        return false;

    Unit* victim = boss->GetVictim();
    if (victim == bot)
        return false;

    if (Player* holder = victim ? victim->ToPlayer() : nullptr)
        if (PlayerbotAI::IsTank(holder))
            return false;

    ThorimPhase2Role const role = GetThorimPhase2Role(botAI, bot);
    if (role == ThorimPhase2Role::MainTank)
        return true;

    if (role != ThorimPhase2Role::OffTank)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    ObjectGuid const mainTank = PlayerbotAI::GetMainTankGuid(group);
    if (mainTank.IsEmpty())
        return true;

    Player* tank = ObjectAccessor::FindPlayer(mainTank);
    return !tank || !tank->IsAlive() || !NearThorimEncounter(tank);
}

bool ThorimPersonalDefensiveTrigger::IsActive()
{
    RaidPredictedHit hit;
    return static_cast<bool>(GetRaidPersonalDefensive(
        botAI, ULDUAR_THORIM_DEFENSIVE_SCOPE, GetThorimLightningChargeHit(botAI, hit) ? &hit : nullptr));
}

bool ThorimSifBlizzardTrigger::IsActive()
{
    Unit* boss = GetThorim(botAI);
    if (boss && boss->GetVictim() == bot)
        return false;

    if (ThorimPhase2Active(botAI))
    {
        ThorimPhase2Role const role = GetThorimPhase2Role(botAI, bot);
        if (role == ThorimPhase2Role::MainTank || role == ThorimPhase2Role::OffTank)
            return false;

        if (role == ThorimPhase2Role::MeleeRing)
        {
            Position step;
            return ThorimRingBlizzardStep(botAI, bot, step);
        }

        if (role == ThorimPhase2Role::Ranged)
            return bot->GetPositionZ() <= ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD && ThorimSpotUnderBlizzard(bot, *bot) &&
                   !ThorimCampSpotInLitCone(botAI, *bot);
    }

    TooCloseToCreatureTrigger tooCloseToBlizzard(botAI);
    return tooCloseToBlizzard.TooCloseToCreature(NPC_SIF_BLIZZARD, ULDUAR_THORIM_SIF_BLIZZARD_RADIUS);
}
