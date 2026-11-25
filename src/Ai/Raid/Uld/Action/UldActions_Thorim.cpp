#include "UldActions_Thorim.h"
#include "UldActions_Shared.h"

#include <CombatStrategy.h>
#include <FollowMasterStrategy.h>

#include <cmath>

#include "AiObjectContext.h"
#include "DBCEnums.h"
#include "GameObject.h"
#include "Group.h"
#include "LastMovementValue.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Position.h"
#include "RaidCombatHelpers.h"
#include "UldEncounter_Thorim.h"
#include "UldScripts.h"
#include "EncounterHelpers.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Unit.h"
#include "Vehicle.h"
#include <TankAssistStrategy.h>

bool ThorimDpsPriorityAction::isUseful()
{
    ThorimDpsPriorityTrigger thorimDpsPriorityTrigger(botAI);
    return thorimDpsPriorityTrigger.IsActive();
}

bool ThorimDpsPriorityAction::Execute(Event /*event*/)
{
    ThorimClearStaleMarks(botAI, bot);

    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    if (currentTarget && !ThorimDpsTargetAllowed(botAI, currentTarget))
    {
        bot->AttackStop();
        bot->InterruptNonMeleeSpells(true);
        bot->SetTarget(ObjectGuid::Empty);
        bot->SetSelection(ObjectGuid());
        currentTarget = nullptr;
        context->GetValue<Unit*>("current target")->Set(nullptr);
    }

    Unit* target = GetThorimDpsTarget(botAI, bot, currentTarget);
    if (!target || !ThorimDpsTargetAllowed(botAI, target))
        return false;

    if (target == currentTarget && (!PlayerbotAI::IsMelee(bot) || bot->HasUnitState(UNIT_STATE_MELEE_ATTACKING)))
        return false;

    return Attack(target);
}

bool ThorimArenaPositioningAction::isUseful()
{
    ThorimArenaPositioningTrigger thorimArenaPositioningTrigger(botAI);
    return thorimArenaPositioningTrigger.IsActive();
}

bool ThorimArenaPositioningAction::Execute(Event /*event*/)
{
    Position anchor;
    if (!GetThorimArenaAnchor(botAI, bot, anchor))
        return false;

    FollowMasterStrategy followMasterStrategy(botAI);
    if (botAI->HasStrategy(followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT))
    {
        botAI->ChangeStrategy(REMOVE_STRATEGY_CHAR + followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT);
        ThorimNoteFollowMasterStripped(bot);
    }

    if (!ThorimArenaAnchorNeedsMove(botAI, bot, anchor))
        return false;

    return MoveTo(bot->GetMapId(), anchor.GetPositionX(), anchor.GetPositionY(), anchor.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_COMBAT, true);
}

bool ThorimCastPersonalDefensiveAction::isUseful()
{
    ThorimPersonalDefensiveTrigger thorimPersonalDefensiveTrigger(botAI);
    return thorimPersonalDefensiveTrigger.IsActive();
}

bool ThorimCastPersonalDefensiveAction::Execute(Event /*event*/)
{
    RaidPredictedHit hit;
    RaidPersonalPick const pick = GetRaidPersonalDefensive(
        botAI, ULDUAR_THORIM_DEFENSIVE_SCOPE, GetThorimLightningChargeHit(botAI, hit) ? &hit : nullptr);
    if (!pick)
        return false;

    if (bot->IsNonMeleeSpellCast(false))
        bot->InterruptNonMeleeSpells(false);

    return pick.consumable ? botAI->DoSpecificAction(pick.consumable, Event(), true)
                           : botAI->CastSpell(pick.spellId, bot);
}

bool ThorimFrostAuraAction::isUseful()
{
    ThorimFrostAuraTrigger thorimFrostAuraTrigger(botAI);
    return thorimFrostAuraTrigger.IsActive();
}

bool ThorimFrostAuraAction::Execute(Event /*event*/)
{
    return botAI->DoSpecificAction("frost resistance aura", Event(), true);
}

bool ThorimPetLeashAction::isUseful()
{
    ThorimPetLeashTrigger thorimPetLeashTrigger(botAI);
    return thorimPetLeashTrigger.IsActive();
}

bool ThorimPetLeashAction::Execute(Event /*event*/)
{
    std::vector<Unit*> pets;

    if (ThorimStrayPets(botAI, bot, pets))
        for (Unit* pet : pets)
            ThorimRecallPet(bot, pet);

    if (ThorimPetsOffRingSlot(botAI, bot, pets))
        for (Unit* pet : pets)
            ThorimSendPetToRingSlot(botAI, bot, pet);

    return false;
}

bool ThorimChargedOrbAction::isUseful()
{
    ThorimChargedOrbTrigger thorimChargedOrbTrigger(botAI);
    return thorimChargedOrbTrigger.IsActive();
}

bool ThorimChargedOrbAction::Execute(Event /*event*/)
{
    Position escape;
    if (!ThorimChargedOrbEscape(botAI, bot, escape))
        return false;

    ThorimNoteOrbEscape(bot, escape);

    return MoveTo(bot->GetMapId(), escape.GetPositionX(), escape.GetPositionY(), escape.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_COMBAT, true);
}

bool ThorimArenaLeashAction::isUseful()
{
    ThorimArenaLeashTrigger thorimArenaLeashTrigger(botAI);
    return thorimArenaLeashTrigger.IsActive();
}

bool ThorimArenaLeashAction::Execute(Event /*event*/)
{
    FollowMasterStrategy followMasterStrategy(botAI);
    if (botAI->HasStrategy(followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT))
    {
        botAI->ChangeStrategy(REMOVE_STRATEGY_CHAR + followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT);
        ThorimNoteFollowMasterStripped(bot);
    }

    Position anchor = ULDUAR_THORIM_NEAR_ARENA_CENTER;
    GetThorimArenaAnchor(botAI, bot, anchor);

    return MoveTo(bot->GetMapId(), anchor.GetPositionX(), anchor.GetPositionY(), anchor.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_COMBAT, true);
}

bool ThorimLaneMovementAction::MoveToGauntletWaypoint(bool leftLane, uint8 index, bool forceCombatPriority)
{
    Position const& waypoint = GetThorimGauntletWaypoint(leftLane, index);

    bool const combat = forceCombatPriority || index == 1;

    return MoveTo(bot->GetMapId(), waypoint.GetPositionX(), waypoint.GetPositionY(), waypoint.GetPositionZ(), false,
                  false, false, true,
                  combat ? MovementPriority::MOVEMENT_COMBAT : MovementPriority::MOVEMENT_NORMAL, !combat);
}

bool ThorimGauntletPositioningAction::isUseful()
{
    ThorimGauntletPositioningTrigger thorimGauntletPositioningTrigger(botAI);
    return thorimGauntletPositioningTrigger.IsActive();
}

bool ThorimGauntletPositioningAction::Execute(Event /*event*/)
{
    FollowMasterStrategy followMasterStrategy(botAI);

    Player* leader = ThorimGauntletLeader(botAI);
    if (!leader || leader == bot)
        return false;

    if (leader->GetDistance(ULDUAR_THORIM_NEAR_ENTRANCE_POSITION) < ULDUAR_THORIM_GATE_RADIUS)
    {
        if (bot->GetDistance2d(leader) <= ULDUAR_THORIM_GATE_FOLLOW_DISTANCE || ThorimFightingStartPack(botAI, bot))
            return false;

        if (!MoveTo(bot->GetMapId(), leader->GetPositionX(), leader->GetPositionY(), leader->GetPositionZ(), false,
                    false, false, true, MovementPriority::MOVEMENT_NORMAL, true))
            return false;

        if (leader == botAI->GetMaster() &&
            !botAI->HasStrategy(followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT))
            botAI->ChangeStrategy(ADD_STRATEGY_CHAR + followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT);

        return true;
    }

    uint8 index = 0;
    bool leftLane = false;
    if (ThorimGauntletLaneIndex(leader, index, leftLane))
    {
        bool preferredLane = false;
        if (ThorimPreferredGauntletLane(botAI, preferredLane))
            leftLane = preferredLane;

        return MoveToGauntletWaypoint(leftLane, index, false);
    }

    if (ThorimTrailsGauntletLeader(botAI, bot, leader))
    {
        Position const spot = ThorimGauntletTrailSpot(bot, leader);
        return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                      false, true, MovementPriority::MOVEMENT_NORMAL, true);
    }

    return false;
}

bool ThorimGauntletLeverAction::isUseful()
{
    ThorimGauntletLeverTrigger thorimGauntletLeverTrigger(botAI);
    return thorimGauntletLeverTrigger.IsActive();
}

bool ThorimGauntletLeverAction::Execute(Event /*event*/)
{
    ObjectGuid leverGuid;
    bool const usable = ThorimLeverUsable(bot, leverGuid);
    if (!usable && !ThorimLeverWaiting(botAI, bot, leverGuid))
        return false;

    GameObject* lever = botAI->GetGameObject(leverGuid);
    if (!lever)
        return false;

    if (!lever->IsAtInteractDistance(bot))
        return MoveTo(lever, 0.0f);

    if (!usable)
        return false;

    WorldPacket data(CMSG_GAMEOBJ_USE);
    data << lever->GetGUID();
    bot->GetSession()->HandleGameObjectUseOpcode(data);
    ThorimNoteLeverPulled(bot);
    return true;
}

bool ThorimGauntletLeadAction::isUseful()
{
    ThorimGauntletLeadTrigger thorimGauntletLeadTrigger(botAI);
    return thorimGauntletLeadTrigger.IsActive();
}

bool ThorimGauntletLeadAction::Execute(Event /*event*/)
{
    Position next;
    if (ThorimGauntletLeadStep(botAI, bot, next))
        return MoveTo(bot->GetMapId(), next.GetPositionX(), next.GetPositionY(), next.GetPositionZ(), false, false,
                      false, true, MovementPriority::MOVEMENT_COMBAT, true);

    if (Unit* colossus = ThorimGauntletLeadColossus(botAI, bot))
    {
        if (bot->GetDistance(colossus) > ULDUAR_THORIM_LEAD_COLOSSUS_DISTANCE)
            return MoveTo(colossus, ULDUAR_THORIM_LEAD_COLOSSUS_DISTANCE - ULDUAR_THORIM_LEAD_COLOSSUS_STOP_MARGIN,
                          MovementPriority::MOVEMENT_COMBAT);

        return MoveTo(colossus->GetMapId(), colossus->GetPositionX(), colossus->GetPositionY(),
                      colossus->GetPositionZ(), false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }

    if (Unit* giant = ThorimGauntletLeadRuneGiant(botAI, bot))
        return MoveTo(giant->GetMapId(), giant->GetPositionX(), giant->GetPositionY(), giant->GetPositionZ(), false,
                      false, false, false, MovementPriority::MOVEMENT_COMBAT);

    if (ThorimGauntletGiantHold(botAI, bot, next))
    {
        LastMovement& booking = AI_VALUE(LastMovement&, "last movement");
        if (ThorimGiantWalkBooked(booking))
            booking.priority = MovementPriority::MOVEMENT_NORMAL;

        return MoveTo(bot->GetMapId(), next.GetPositionX(), next.GetPositionY(), next.GetPositionZ(), false, false,
                      false, true, MovementPriority::MOVEMENT_COMBAT, true);
    }

    return false;
}

bool ThorimBalconyAdvanceAction::isUseful()
{
    ThorimBalconyAdvanceTrigger thorimBalconyAdvanceTrigger(botAI);
    return thorimBalconyAdvanceTrigger.IsActive();
}

bool ThorimBalconyAdvanceAction::Execute(Event /*event*/)
{
    Unit* boss = GetThorim(botAI);
    if (!boss || !boss->IsAlive())
        return false;

    bool const bossDown = boss->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD;

    uint8 const step = ThorimAdvanceBalconyStep(bot);

    // Has to stay ahead of the jump test: hold 2 sits inside the edge tolerance.
    Position hold;
    if (bossDown && step >= ULDUAR_THORIM_BALCONY_WAYPOINTS - 1 && ThorimBalconyHoldSpot(botAI, bot, hold))
    {
        if (bot->GetExactDist2d(&hold) <= ULDUAR_THORIM_BALCONY_HOLD_TOLERANCE)
            return false;

        return MoveTo(bot->GetMapId(), hold.GetPositionX(), hold.GetPositionY(), hold.GetPositionZ(), false, false,
                      false, true, MovementPriority::MOVEMENT_COMBAT, true);
    }

    bool const atEdge =
        bot->GetExactDist2d(&ULDUAR_THORIM_JUMP_START_POINT) <= ULDUAR_THORIM_BALCONY_ARRIVE_TOLERANCE;

    if (bossDown && atEdge)
        return JumpTo(bot->GetMapId(), ULDUAR_THORIM_JUMP_END_POINT.GetPositionX(),
                      ULDUAR_THORIM_JUMP_END_POINT.GetPositionY(), ULDUAR_THORIM_JUMP_END_POINT.GetPositionZ(),
                      MovementPriority::MOVEMENT_COMBAT);

    if (step >= ULDUAR_THORIM_BALCONY_WAYPOINTS && atEdge)
        return false;

    Position const& waypoint = GetThorimBalconyWaypoint(step);

    return MoveTo(bot->GetMapId(), waypoint.GetPositionX(), waypoint.GetPositionY(), waypoint.GetPositionZ(), false,
                  false, false, true, MovementPriority::MOVEMENT_COMBAT, true);
}

bool ThorimRunicSmashAction::isUseful()
{
    ThorimRunicSmashTrigger thorimRunicSmashTrigger(botAI);
    return thorimRunicSmashTrigger.IsActive();
}

bool ThorimRunicSmashAction::Execute(Event /*event*/)
{
    bool safeLane = false;
    if (!ThorimPreferredGauntletLane(botAI, safeLane))
        return false;

    uint8 index = 0;
    if (!ThorimResolveGauntletIndex(botAI, bot, index))
        return false;

    return MoveToGauntletWaypoint(safeLane, index, true);
}

bool ThorimRunicBarrierBailAction::isUseful()
{
    ThorimRunicBarrierBailTrigger thorimRunicBarrierBailTrigger(botAI);
    return thorimRunicBarrierBailTrigger.IsActive();
}

bool ThorimRunicBarrierBailAction::Execute(Event /*event*/)
{
    Unit* colossus = GetThorimRunicColossus(botAI);
    if (!colossus)
        return false;

    float const currentDistance = bot->GetDistance(colossus);
    if (currentDistance >= ULDUAR_THORIM_BARRIER_BAIL_DISTANCE)
        return false;

    return MoveAway(colossus, ULDUAR_THORIM_BARRIER_BAIL_DISTANCE - currentDistance);
}

bool ThorimResetEncounterStateAction::Execute(Event /*event*/)
{
    if (ThorimFollowMasterStripped(bot))
    {
        FollowMasterStrategy followMasterStrategy(botAI);
        if (!botAI->HasStrategy(followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT))
            botAI->ChangeStrategy(ADD_STRATEGY_CHAR + followMasterStrategy.getName(),
                                  BotState::BOT_STATE_NON_COMBAT);
    }

    ResetThorimEncounterState(bot, false);

    return false;
}

bool ThorimFallFromFloorAction::Execute(Event /*event*/)
{
    Player* leader = GetRaidLeaderOnInstance(botAI);
    if (!leader || leader == bot)
        return false;

    bot->NearTeleportTo(leader->GetPositionX(), leader->GetPositionY(), leader->GetPositionZ(),
                        leader->GetOrientation());
    return true;
}

bool ThorimFallFromFloorAction::isUseful()
{
    ThorimFallFromFloorTrigger thorimFallFromFloorTrigger(botAI);
    return thorimFallFromFloorTrigger.IsActive();
}

bool ThorimPhase2PositioningAction::Execute(Event /*event*/)
{
    ThorimPhase2Role const role = GetThorimPhase2Role(botAI, bot);

    Position targetPosition;
    if (!TryGetThorimPhase2Spot(botAI, bot, role, targetPosition))
        return false;

    bool const ringSlot = role == ThorimPhase2Role::MeleeRing;

    if (ringSlot)
    {
        if (ThorimRingBlizzardCovered(bot, targetPosition))
            return false;

        if (!ThorimRingWantsMove(botAI, bot, targetPosition))
        {
            ThorimRingMarkArrived(bot);
            return false;
        }

        if (ThorimRingChainWait(botAI, bot, targetPosition, false))
            return false;

        ThorimRingClearArrived(bot);
    }

    if (role == ThorimPhase2Role::Ranged && bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL) &&
        ThorimCampSpotInLitCone(botAI, *bot))
        bot->InterruptSpell(CURRENT_CHANNELED_SPELL);

    Unit* const boss = GetThorim(botAI);
    bool const tank = role == ThorimPhase2Role::MainTank || role == ThorimPhase2Role::OffTank;
    bool const backward = tank && boss && boss->GetVictim() == bot &&
                          std::cos(bot->GetAbsoluteAngle(&targetPosition) - bot->GetAbsoluteAngle(boss)) < 0.0f;

    MoveTo(bot->GetMapId(), targetPosition.GetPositionX(), targetPosition.GetPositionY(), targetPosition.GetPositionZ(),
           false, false, false, true, MovementPriority::MOVEMENT_COMBAT, true, backward);

    if (ringSlot)
        return true;

    if (tank)
        return !ThorimTankSpotNeedsMove(botAI, bot, targetPosition);

    return bot->GetDistance(targetPosition) <= ULDUAR_THORIM_PHASE2_SPOT_TOLERANCE;
}

bool ThorimPhase2PositioningAction::isUseful()
{
    ThorimPhase2PositioningTrigger thorimPhase2PositioningTrigger(botAI);
    return thorimPhase2PositioningTrigger.IsActive();
}

bool ThorimUnreachableStepAction::Execute(Event /*event*/)
{
    Unit* boss = ThorimUnreachableBy(botAI, bot);
    Position step;
    if (!boss || !ThorimUnreachableStep(bot, boss, step))
        return false;

    bool const backward = std::cos(bot->GetAbsoluteAngle(&step) - bot->GetAbsoluteAngle(boss)) < 0.0f;
    return MoveTo(bot->GetMapId(), step.GetPositionX(), step.GetPositionY(), step.GetPositionZ(), false, false, false,
                  true, MovementPriority::MOVEMENT_FORCED, true, backward);
}

bool ThorimUnreachableStepAction::isUseful()
{
    ThorimUnreachableTrigger thorimUnreachableTrigger(botAI);
    return thorimUnreachableTrigger.IsActive();
}

bool ThorimSifBlizzardAction::Execute(Event event)
{
    ThorimPhase2Role const role = ThorimPhase2Active(botAI) ? GetThorimPhase2Role(botAI, bot) : ThorimPhase2Role::None;

    Position escape;
    if (role == ThorimPhase2Role::MeleeRing)
    {
        if (!ThorimRingBlizzardStep(botAI, bot, escape) || ThorimRingChainWait(botAI, bot, escape, true))
            return false;
    }
    else if (role != ThorimPhase2Role::Ranged)
        return MoveAwayFromCreatureAction::Execute(event);
    else if (!ThorimCampBlizzardEscape(botAI, bot, escape))
        return false;

    return MoveTo(bot->GetMapId(), escape.GetPositionX(), escape.GetPositionY(), escape.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_FORCED, true);
}

bool ThorimUnbalancingStrikeSwapAction::isUseful()
{
    ThorimUnbalancingStrikeSwapTrigger thorimUnbalancingStrikeSwapTrigger(botAI);
    return thorimUnbalancingStrikeSwapTrigger.IsActive();
}

bool ThorimTakeBossAction::Execute(Event event)
{
    Unit* boss = GetThorim(botAI);
    if (!boss || !boss->IsAlive())
        return false;

    if (AI_VALUE(Unit*, "current target") != boss)
        return Attack(boss);

    if (boss->GetVictim() != bot)
        return botAI->DoSpecificAction("taunt spell", event, true);

    return false;
}

bool ThorimTankPickupAction::isUseful()
{
    ThorimTankPickupTrigger thorimTankPickupTrigger(botAI);
    return thorimTankPickupTrigger.IsActive();
}
