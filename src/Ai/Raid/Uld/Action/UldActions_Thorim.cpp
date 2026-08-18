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
#include "UldBossHelper.h"
#include "UldEncounter_Thorim.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "RtiValue.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Unit.h"
#include "Vehicle.h"
#include <RtiTargetValue.h>
#include <TankAssistStrategy.h>

const Position ULDUAR_THORIM_JUMP_START_POINT = Position(2137.137f, -291.19025f, 438.24753f, 1.7059844f);

bool ThorimUnbalancingStrikeAction::isUseful()
{
    ThorimUnbalancingStrikeTrigger thorimUnbalancingStrikeTrigger(botAI);
    if (!thorimUnbalancingStrikeTrigger.IsActive())
        return false;

    return botAI->HasCheat(BotCheatMask::raid);
}

bool ThorimUnbalancingStrikeAction::Execute(Event /*event*/)
{
    bot->RemoveAura(SPELL_UNBALANCING_STRIKE);
    return true;
}

bool ThorimMarkDpsTargetAction::isUseful()
{
    ThorimMarkDpsTargetTrigger thorimMarkDpsTargetTrigger(botAI);
    return thorimMarkDpsTargetTrigger.IsActive();
}

bool ThorimMarkDpsTargetAction::Execute(Event /*event*/)
{
    Unit* targetToMark = nullptr;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    ObjectGuid currentMoonTarget = group->GetTargetIcon(RtiTargetValue::moonIndex);
    Unit* currentMoonUnit = botAI->GetUnit(currentMoonTarget);
    Unit* boss = AI_VALUE2(Unit*, "find target", "thorim");
    if (!currentMoonUnit && boss && boss->IsAlive() && boss->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
    {
        group->SetTargetIcon(RtiTargetValue::moonIndex, bot->GetGUID(), boss->GetGUID());
    }

    if (currentMoonUnit && boss && currentMoonUnit->GetEntry() == boss->GetEntry() &&
        boss->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
    {
        group->SetTargetIcon(RtiTargetValue::skullIndex, bot->GetGUID(), boss->GetGUID());
        return true;
    }

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
            targetToMark = acolyte;
        else if (evoker && evoker->IsAlive() && bot->GetDistance(evoker) < 50.0f &&
                 (!currentSkullUnit || currentSkullUnit->GetEntry() != evoker->GetEntry()))
            targetToMark = evoker;
        else
            return false;
    }
    else if (botAI->IsAssistTankOfIndex(bot, 0))
    {
        ObjectGuid currentCrossTarget = group->GetTargetIcon(RtiTargetValue::crossIndex);
        Unit* currentCrossUnit = botAI->GetUnit(currentCrossTarget);
        if (currentCrossUnit && !currentCrossUnit->IsAlive())
        {
            currentCrossUnit = nullptr;
        }

        Unit* acolyte = AI_VALUE2(Unit*, "find target", "dark rune acolyte");
        Unit* runicColossus = AI_VALUE2(Unit*, "find target", "runic colossus");
        Unit* ancientRuneGiant = AI_VALUE2(Unit*, "find target", "ancient rune giant");
        Unit* ironHonorGuard = AI_VALUE2(Unit*, "find target", "iron honor guard");
        Unit* ironRingGuard = AI_VALUE2(Unit*, "find target", "iron ring guard");

        if (acolyte && acolyte->IsAlive() && (!currentCrossUnit || currentCrossUnit->GetEntry() != acolyte->GetEntry()))
            targetToMark = acolyte;
        else if (runicColossus && runicColossus->IsAlive() &&
                 (!currentCrossUnit || currentCrossUnit->GetEntry() != runicColossus->GetEntry()))
            targetToMark = runicColossus;
        else if (ancientRuneGiant && ancientRuneGiant->IsAlive() &&
                 (!currentCrossUnit || currentCrossUnit->GetEntry() != ancientRuneGiant->GetEntry()))
            targetToMark = ancientRuneGiant;
        else if (ironHonorGuard && ironHonorGuard->IsAlive() &&
                 (!currentCrossUnit || currentCrossUnit->GetEntry() != ironHonorGuard->GetEntry()))
            targetToMark = ironHonorGuard;
        else if (ironRingGuard && ironRingGuard->IsAlive() &&
                 (!currentCrossUnit || currentCrossUnit->GetEntry() != ironRingGuard->GetEntry()))
            targetToMark = ironRingGuard;
        else
            return false;
    }

    if (!targetToMark)
        return false;  // No target to mark

    if (botAI->IsMainTank(bot))
    {
        group->SetTargetIcon(RtiTargetValue::skullIndex, bot->GetGUID(), targetToMark->GetGUID());
        return true;
    }

    if (botAI->IsAssistTankOfIndex(bot, 0))
    {
        group->SetTargetIcon(RtiTargetValue::crossIndex, bot->GetGUID(), targetToMark->GetGUID());
        return true;
    }

    return false;
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

    // Reach then hold. A tight deadband has the bot sliding on its spot forever, and a moving bot
    // casts nothing.
    if (!ThorimArenaAnchorNeedsMove(botAI, bot, anchor))
        return false;

    return MoveTo(bot->GetMapId(), anchor.GetPositionX(), anchor.GetPositionY(), anchor.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_COMBAT, true);
}

bool ThorimArenaLeashAction::isUseful()
{
    ThorimArenaLeashTrigger thorimArenaLeashTrigger(botAI);
    return thorimArenaLeashTrigger.IsActive();
}

bool ThorimArenaLeashAction::Execute(Event /*event*/)
{
    // Straying is the symptom; the master walking off down the corridor is usually the cause, so take
    // "follow master" here as well rather than waiting for the arena node to catch a quiet moment.
    FollowMasterStrategy followMasterStrategy(botAI);
    if (botAI->HasStrategy(followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT))
    {
        botAI->ChangeStrategy(REMOVE_STRATEGY_CHAR + followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT);
        ThorimNoteFollowMasterStripped(bot);
    }

    // Back to this bot's own spot, not to the middle: dragging the whole squad onto one point every
    // time the fence trips would wreck the formation the fence exists to protect. The middle is the
    // fallback, and never the nearest box edge - the edge is where a rounding error turns into a wipe.
    Position anchor = ULDUAR_THORIM_NEAR_ARENA_CENTER;
    GetThorimArenaAnchor(botAI, bot, anchor);

    return MoveTo(bot->GetMapId(), anchor.GetPositionX(), anchor.GetPositionY(), anchor.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_COMBAT, true);
}

bool ThorimLaneMovementAction::MoveToGauntletWaypoint(bool leftLane, uint8 index, bool forceCombatPriority)
{
    Position const& waypoint = GetThorimGauntletWaypoint(leftLane, index);

    // The second waypoint has always moved at combat priority and without the shortened delay, in
    // both lanes: it is the one where the squad rounds into the Colossus's line of sight.
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

    Unit* master = botAI->GetMaster();
    if (!master)
        return false;

    std::string const rti = AI_VALUE(std::string, "rti");
    if (rti != "cross")
    {
        botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("cross");
    }

    if (master->GetDistance(ULDUAR_THORIM_NEAR_ENTRANCE_POSITION) < 10.0f && (bot->GetDistance2d(master) > 5.0f))
    {
        if (MoveTo(bot->GetMapId(), master->GetPositionX(), master->GetPositionY(), master->GetPositionZ(), false,
                   false, false, true, MovementPriority::MOVEMENT_NORMAL, true))
        {
            if (!botAI->HasStrategy(followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT))
            {
                botAI->ChangeStrategy(ADD_STRATEGY_CHAR + followMasterStrategy.getName(),
                                      BotState::BOT_STATE_NON_COMBAT);
            }

            return true;
        }
    }

    uint8 index = 0;
    bool leftLane = false;
    if (ThorimGauntletLaneIndex(master, index, leftLane))
    {
        // Same progress down the corridor as the master, but in whichever lane the last Runic Smash
        // telegraph left safe - otherwise the formation walks everyone back into the blast.
        bool preferredLane = false;
        if (ThorimPreferredGauntletLane(botAI, preferredLane))
            leftLane = preferredLane;

        return MoveToGauntletWaypoint(leftLane, index, false);
    }

    Unit* boss = AI_VALUE2(Unit*, "find target", "thorim");
    if (boss && boss->IsAlive() && bot->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD &&
        boss->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
    {
        MoveTo(bot->GetMapId(), ULDUAR_THORIM_JUMP_START_POINT.GetPositionX(),
               ULDUAR_THORIM_JUMP_START_POINT.GetPositionY(), ULDUAR_THORIM_JUMP_START_POINT.GetPositionZ(), false,
               false, false, true, MovementPriority::MOVEMENT_NORMAL, true);

        if (bot->GetDistance(ULDUAR_THORIM_JUMP_START_POINT) > 0.5f)
            return false;

        JumpTo(bot->GetMapId(), ULDUAR_THORIM_JUMP_END_POINT.GetPositionX(),
               ULDUAR_THORIM_JUMP_END_POINT.GetPositionY(), ULDUAR_THORIM_JUMP_END_POINT.GetPositionZ(),
               MovementPriority::MOVEMENT_COMBAT);
    }

    return false;
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

    // No AttackStop here on purpose: the target stays, so everything that is not a melee swing keeps
    // landing from out here. FleePosition is no good either - it clamps travel to
    // AiPlayerbot.FleeDistance and would leave the bot inside the shield's reach.
    return MoveAway(colossus, ULDUAR_THORIM_BARRIER_BAIL_DISTANCE - currentDistance);
}

bool ThorimLightningChargeAction::isUseful()
{
    ThorimLightningChargeTrigger thorimLightningChargeTrigger(botAI);
    return thorimLightningChargeTrigger.IsActive();
}

bool ThorimLightningChargeAction::Execute(Event /*event*/)
{
    Position spot;
    if (!TryGetThorimPhase2Spot(botAI, bot, ThorimPhase2Role::MeleeRing, spot))
        return false;

    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false, false,
                  true, MovementPriority::MOVEMENT_COMBAT, true);
}

bool ThorimResetEncounterStateAction::Execute(Event /*event*/)
{
    // Hand "follow master" back before the record of having taken it goes with the rest of the state,
    // or an arena squad bot never follows anyone again after the kill.
    if (ThorimFollowMasterStripped(bot))
    {
        FollowMasterStrategy followMasterStrategy(botAI);
        if (!botAI->HasStrategy(followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT))
        {
            botAI->ChangeStrategy(ADD_STRATEGY_CHAR + followMasterStrategy.getName(),
                                  BotState::BOT_STATE_NON_COMBAT);
        }
    }

    ResetThorimEncounterState(bot, false);

    // Never claims the tick: clearing state is bookkeeping, and the pull still needs every node below
    // this one to run on the same tick.
    return false;
}

bool ThorimFallFromFloorAction::Execute(Event /*event*/)
{
    Player* master = botAI->GetMaster();

    if (!master)
        return false;

    return bot->TeleportTo(bot->GetMapId(), master->GetPositionX(), master->GetPositionY(), master->GetPositionZ(),
                           master->GetOrientation());
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

    bool const ringSlot = role == ThorimPhase2Role::OffTank || role == ThorimPhase2Role::MeleeRing;

    // Reach then hold. A tight deadband against a ring recomputed from a moving boss has the bot
    // sliding in place forever, and a moving bot casts nothing.
    if (ringSlot && !ThorimRingNeedsMove(botAI, bot, targetPosition))
        return false;

    // The main tank backs into his spot so he keeps facing the boss he is dragging south.
    bool const backward = role == ThorimPhase2Role::MainTank;

    MoveTo(bot->GetMapId(), targetPosition.GetPositionX(), targetPosition.GetPositionY(), targetPosition.GetPositionZ(),
           false, false, false, true, MovementPriority::MOVEMENT_COMBAT, true, backward);

    if (ringSlot)
        return true;

    return bot->GetDistance(targetPosition) <= 1.0f;
}

bool ThorimPhase2PositioningAction::isUseful()
{
    ThorimPhase2PositioningTrigger thorimPhase2PositioningTrigger(botAI);
    return thorimPhase2PositioningTrigger.IsActive();
}

bool ThorimUnbalancingStrikeSwapAction::isUseful()
{
    ThorimUnbalancingStrikeSwapTrigger thorimUnbalancingStrikeSwapTrigger(botAI);
    return thorimUnbalancingStrikeSwapTrigger.IsActive();
}

bool ThorimUnbalancingStrikeSwapAction::Execute(Event event)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "thorim");
    if (!boss || !boss->IsAlive())
        return false;

    if (AI_VALUE(Unit*, "current target") != boss)
        return Attack(boss);

    if (boss->GetVictim() != bot)
        return botAI->DoSpecificAction("taunt spell", event, true);

    return false;
}
