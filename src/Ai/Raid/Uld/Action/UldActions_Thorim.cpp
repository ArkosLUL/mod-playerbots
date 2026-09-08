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
#include "UldEncounter_Thorim.h"
#include "UldScripts.h"
#include "EncounterHelpers.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Unit.h"
#include "Vehicle.h"
#include <TankAssistStrategy.h>

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

bool ThorimDpsPriorityAction::isUseful()
{
    ThorimDpsPriorityTrigger thorimDpsPriorityTrigger(botAI);
    return thorimDpsPriorityTrigger.IsActive();
}

bool ThorimDpsPriorityAction::Execute(Event /*event*/)
{
    // Before anything is read: a pinned target outranks every pick made below, so clearing has to
    // happen first or this action spends the pull losing to a mark it already replaced.
    ThorimClearStaleMarks(botAI, bot);

    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    // Dropping the forbidden target is worth doing even when nothing better is in reach: holding it is
    // what the arena target guard shuts the bot down for, and an empty target lets the generic picker
    // have another go next tick.
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

    // Reach then hold. A tight deadband has the bot sliding on its spot forever, and a moving bot
    // casts nothing.
    if (!ThorimArenaAnchorNeedsMove(botAI, bot, anchor))
        return false;

    return MoveTo(bot->GetMapId(), anchor.GetPositionX(), anchor.GetPositionY(), anchor.GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_COMBAT, true);
}

bool ThorimPetLeashAction::isUseful()
{
    ThorimPetLeashTrigger thorimPetLeashTrigger(botAI);
    return thorimPetLeashTrigger.IsActive();
}

bool ThorimPetLeashAction::Execute(Event /*event*/)
{
    std::vector<Unit*> stray;
    if (!ThorimStrayPets(botAI, bot, stray))
        return false;

    // All of them in one pass. A bot can have a pet and a guardian up at once, and leaving the second
    // one out there for another tick is a pack pulled for no reason.
    for (Unit* pet : stray)
        ThorimRecallPet(bot, pet);

    // False on purpose, same as the Razorscale and Mimiron pet nodes: this walked a pet, not the bot,
    // so the bot still needs whatever node was going to act this tick.
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

    // No arrival latch here, unlike the anchor: the escape point stops being offered the moment the
    // bot is out of the field, so there is nothing for a deadband to damp.
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

    // He has dropped for phase 2 and the bot has not, so the job up here is now the edge. Still walked
    // as the chain though: the edge is on the centre line and so are both Paralytic Field bunnies, so
    // a straight line to it from anywhere south of the second doors goes over one of them. Whole squad
    // ate that once - ten bots, 5 to 12 yd off a bunny, stunned for up to 15s. The jump point is the
    // last waypoint in the chain anyway, so the chain already ends where the jump starts.
    bool const bossDown = boss->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD;

    // Before the jump test, or the latch never reaches the end and a bot on the edge never jumps.
    uint8 const step = ThorimAdvanceBalconyStep(bot);

    // Distance, never the latch alone. A latch that says "past the last waypoint" used to be enough
    // to fire the jump, and a step carried in from an earlier pull then fired it from the top of the
    // ramp: one bot flew a 180 yd arc across the whole hallway. Arrive tolerance rather than the jump
    // one, so a bot parked in the 3 to 6 yd gap between the two still goes.
    bool const atEdge =
        bot->GetExactDist2d(&ULDUAR_THORIM_JUMP_START_POINT) <= ULDUAR_THORIM_BALCONY_ARRIVE_TOLERANCE;

    if (bossDown && atEdge)
    {
        return JumpTo(bot->GetMapId(), ULDUAR_THORIM_JUMP_END_POINT.GetPositionX(),
                      ULDUAR_THORIM_JUMP_END_POINT.GetPositionY(), ULDUAR_THORIM_JUMP_END_POINT.GetPositionZ(),
                      MovementPriority::MOVEMENT_COMBAT);
    }

    // Standing on the edge with him still up: done walking, and the tick belongs to whatever shoots him.
    if (step >= ULDUAR_THORIM_BALCONY_WAYPOINTS && atEdge)
        return false;

    // Clamps to the last waypoint, which is the edge - so a bot knocked past the end walks back to it
    // instead of stalling. Nothing to cross from up there, both bunnies are well south.
    Position const& waypoint = GetThorimBalconyWaypoint(step);

    // Combat priority, because the hallway is walked in combat with a boss nothing up here can reach.
    // At normal priority every step lost to "reach melee", which holds the mover for up to 5s and
    // aims straight up the middle - which is where the two Paralytic Field bunnies sit. This route
    // goes up the east wall and clears both by 16 yd, but only if it is the one issuing the walk.
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
