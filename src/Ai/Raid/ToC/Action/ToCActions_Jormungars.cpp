#include "ToCActions_Jormungars.h"

#include <cmath>
#include <string>
#include <vector>

#include "EncounterHelpers.h"
#include "LastMovementValue.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "ToCData.h"
#include "ToCHelpers_Icehowl.h"
#include "ToCHelpers_Jormungars.h"
#include "ToCHelpers_NorthrendBeasts.h"
#include "Unit.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

namespace
{
constexpr float APPROACH_ARRIVE = 1.0f;
// The worm walks to its new spot 2.5 s into the submerge. Re-aiming on every step of that walk would
// churn the MotionMaster.
constexpr float APPROACH_REAIM_DISTANCE = 3.0f;
constexpr float SWEEP_RADIUS = 30.0f;
constexpr float SWEEP_DISTANCE_STEP = 2.0f;
constexpr float SWEEP_ANGLE_STEP = static_cast<float>(M_PI) / 8.0f;
constexpr float WALK_ARRIVE = 1.0f;
constexpr float WALK_SPOT_TOLERANCE = 0.5f;
// A spline issued last tick may not show in isMoving yet
constexpr uint32 WALK_STALL_MS = 500;
// The partner keeps moving, so a straight cure walk stops short of the full reach
constexpr float CURE_DIRECT_INSET = 1.0f;

char const* const CLASS_TAUNTS[] = {"taunt", "hand of reckoning", "dark command", "growl"};

// CastSpell faces the target before the cast can fail, so a taunt on cooldown or out of range would
// turn the bot every tick
bool TryClassTaunt(PlayerbotAI* botAI, Player* bot, Unit* worm)
{
    for (char const* taunt : CLASS_TAUNTS)
    {
        if (!botAI->CanCastSpell(taunt, worm))
            continue;

        uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", taunt)->Get();
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
        if (info && bot->IsWithinCombatRange(worm, bot->GetSpellMaxRangeForTarget(worm, info)) &&
            bot->IsWithinLOSInMap(worm))
            return CastClassTaunt(botAI, worm);
    }

    return false;
}

void NoteWormMove(Player* bot, std::string const& branch)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "nb.wormmove", branch);
}

std::string Yards(float distance) { return std::to_string(static_cast<int32>(std::lround(distance))); }

bool BookedOn(LastMovement const& last, Position const& spot)
{
    return std::fabs(last.lastMoveToX - spot.GetPositionX()) <= WALK_SPOT_TOLERANCE &&
           std::fabs(last.lastMoveToY - spot.GetPositionY()) <= WALK_SPOT_TOLERANCE;
}

bool ClearOfCircles(std::vector<HazardCircle> const& circles, Position const& spot)
{
    for (HazardCircle const& circle : circles)
        if (circle.first.GetExactDist2d(spot) < circle.second)
            return false;

    return true;
}

bool OnWormFloor(float x, float y) { return ARENA_CENTER.GetExactDist2d(x, y) <= WORM_FLOOR_RADIUS; }

// The sweep's rays fan 22.5 degrees apart, so past ~30 yd they can straddle the partner's reach and
// find nothing. Walks straight at the partner instead, if that spot is safe.
Position DirectCureSpot(Player* bot, WormMovePlan const& plan)
{
    Position const& partner = plan.preferNear;
    float const angle = partner.GetAngle(bot);
    float const reach = WORM_CURE_REACH - CURE_DIRECT_INSET;
    Position const spot(partner.GetPositionX() + reach * std::cos(angle),
                        partner.GetPositionY() + reach * std::sin(angle), partner.GetPositionZ());
    return WormSpotStillSafe(plan, spot) ? spot : Position();
}
}  // namespace

bool NorthrendWormsTankHoldAction::Execute(Event /*event*/)
{
    Unit* worm = GetBeastOfDuty(botAI, duty);
    if (!worm)
    {
        walking = WalkKind::None;
        return false;
    }

    bool const mobile = duty == BeastsTankDuty::WormMobile;
    if (mobile)
    {
        // In a heroic overlap Gormok's holder marks him, and two holders trading the skull every tick
        // drag the DPS back and forth
        if (!GetEngagedBeast(botAI, NorthrendBeast::Gormok))
            MarkTargetWithSkull(bot, worm);

        SetRtiTarget(botAI, "skull", worm);
    }

    if (IsWormSubmerged(worm))
        return ApproachSubmerged(worm);

    if (walking == WalkKind::Approach)
        walking = WalkKind::None;

    // The submerge wipes its threat list, so it comes up on whoever threatens it first
    if (worm->GetVictim() != bot && TryClassTaunt(botAI, bot, worm))
        return true;

    if (AI_VALUE(Unit*, "current target") != worm)
        return Attack(worm);

    return mobile && DragOffHazards(worm);
}

// The mobile holder only needs taunt range when it comes up. The stationary worm is rooted, so its
// holder waits in melee of where it surfaces.
bool NorthrendWormsTankHoldAction::ApproachSubmerged(Unit* worm)
{
    float const stand = duty == BeastsTankDuty::WormMobile ? WORM_TAUNT_STAND : WORM_MELEE_RANGE - 2.0f;

    Position chargeStart;
    Position chargeEnd;
    if (bot->GetExactDist2d(worm) <= stand + APPROACH_ARRIVE || IcehowlChargeLatched(botAI, chargeStart, chargeEnd))
    {
        walking = WalkKind::None;
        return false;
    }

    if (WalkInFlight(WalkKind::Approach) && worm->GetExactDist2d(approachFrom) < APPROACH_REAIM_DISTANCE)
        return true;

    float const angle = worm->GetAngle(bot);
    Position const spot(worm->GetPositionX() + stand * std::cos(angle),
                        worm->GetPositionY() + stand * std::sin(angle), worm->GetPositionZ());
    float const travel = bot->GetExactDist2d(spot);
    if (!WalkTo(WalkKind::Approach, spot))
        return false;

    approachFrom = worm->GetPosition();
    NoteWormMove(bot, "approach " + Yards(travel));
    return true;
}

bool NorthrendWormsTankHoldAction::DragOffHazards(Unit* worm)
{
    Position chargeStart;
    Position chargeEnd;
    WormDragPlan plan;
    if (IcehowlChargeLatched(botAI, chargeStart, chargeEnd) || !GetMobileWormDrag(botAI, worm, plan))
    {
        walking = WalkKind::None;
        return false;
    }

    if (Position const* spot = WalkInFlight(WalkKind::Drag))
        if (ClearOfCircles(plan.poolsOnly, *spot))
            return true;

    HazardSweepCache sweep;
    Position spot = FindNearestPositionClearOfHazards(bot, plan.hazards, SWEEP_RADIUS, SWEEP_DISTANCE_STEP,
                                                      SWEEP_ANGLE_STEP, &plan.preferNear, OnWormFloor, &sweep);
    if (spot == Position())
        spot = FindNearestPositionClearOfHazards(bot, plan.poolsOnly, SWEEP_RADIUS, SWEEP_DISTANCE_STEP,
                                                 SWEEP_ANGLE_STEP, &plan.preferNear, OnWormFloor, &sweep);

    if (spot == Position())
        return false;

    float const travel = bot->GetExactDist2d(spot);
    if (!WalkTo(WalkKind::Drag, spot))
        return false;

    NoteWormMove(bot, "drag " + Yards(travel));
    return true;
}

bool NorthrendWormsTankHoldAction::BookedOnWalkSpot()
{
    return BookedOn(AI_VALUE(LastMovement&, "last movement"), walkSpot);
}

Position const* NorthrendWormsTankHoldAction::WalkInFlight(WalkKind kind)
{
    // Moving on some other node's walk doesn't count
    if (walking != kind || !BookedOnWalkSpot())
        return nullptr;

    if (!bot->isMoving() && getMSTimeDiff(walkIssuedMs, getMSTime()) >= WALK_STALL_MS)
        return nullptr;

    return &walkSpot;
}

bool NorthrendWormsTankHoldAction::WalkTo(WalkKind kind, Position const& spot)
{
    // A walk issued mid cast never starts but still books its travel time, and the same spot is
    // refused until that runs out. Wait for the cast to end instead.
    if (bot->IsMovementPreventedByCasting())
        return false;

    // Our own earlier walk holds the booking at this priority and would refuse its replacement
    if (BookedOnWalkSpot())
        AI_VALUE(LastMovement&, "last movement").clear();

    walkSpot = spot;
    walking = kind;
    walkIssuedMs = getMSTime();
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_COMBAT);
}

Player* NorthrendWormsRedirectThreatAction::GetRedirectTank()
{
    Player* holder = nullptr;
    return GetWormRedirectTarget(botAI, holder) ? holder : nullptr;
}

// Misdirection and Tricks go out under ground, but the shots wait for the worm to come up
Unit* NorthrendWormsRedirectThreatAction::GetThreatDumpTarget()
{
    Player* holder = nullptr;
    Unit* worm = GetWormRedirectTarget(botAI, holder);
    return worm && !IsWormSubmerged(worm) ? worm : nullptr;
}

bool NorthrendWormsRepositionAction::Execute(Event /*event*/)
{
    WormMovePlan plan;
    if (!GetWormMovePlan(botAI, plan) || plan.reason == WormMoveReason::None)
    {
        ClearWormWalk(bot);
        return false;
    }

    if (!botAI->CanMove())
    {
        NoteWormMove(bot, "stunned");
        return false;
    }

    std::string const reason = WormMoveReasonName(plan.reason);
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");

    // Every MoveTo clears the MotionMaster, so a walk still carrying the bot to a safe spot is left alone
    bool ownBooking = false;
    Position latched;
    uint32 latchedMs = 0;
    if (GetWormWalk(bot, latched, latchedMs))
    {
        uint32 const age = getMSTimeDiff(latchedMs, getMSTime());
        ownBooking = BookedOn(last, latched);
        if (age < WORM_WALK_LATCH_MS && WormSpotStillSafe(plan, latched))
        {
            if (bot->GetExactDist2d(latched) <= WALK_ARRIVE)
            {
                NoteWormMove(bot, "clear");
                ClearWormWalk(bot);
                return false;
            }

            if (ownBooking && (bot->isMoving() || age < WALK_STALL_MS))
            {
                NoteWormMove(bot, "hold " + reason);
                return true;
            }
        }

        ClearWormWalk(bot);
    }

    // PointMovementGenerator never starts a spline under a cast that pins the feet, and only damage
    // landing on the spot is worth losing the cast for
    bool const pinned = bot->IsMovementPreventedByCasting();
    if (pinned && !plan.urgent)
    {
        NoteWormMove(bot, "locked");
        return false;
    }

    auto const roleFits = [&plan](float x, float y)
    { return (!plan.accept || plan.accept(x, y)) && (!plan.roleAccept || plan.roleAccept(x, y)); };

    HazardSweepCache sweep;
    Position spot = FindNearestPositionClearOfHazards(bot, plan.circles, SWEEP_RADIUS, SWEEP_DISTANCE_STEP,
                                                      SWEEP_ANGLE_STEP, &plan.preferNear, roleFits, &sweep);
    if (spot == Position())
        spot = FindNearestPositionClearOfHazards(bot, plan.circles, SWEEP_RADIUS, SWEEP_DISTANCE_STEP,
                                                 SWEEP_ANGLE_STEP, &plan.preferNear, plan.accept, &sweep);

    if (spot == Position() && plan.urgent)
    {
        // Urgency from a Spew cone alone leaves no urgent circle, and the sweep finds nothing without one
        std::vector<HazardCircle> const ownFeet = { { bot->GetPosition(), 0.0f } };
        spot = FindNearestPositionClearOfHazards(bot, plan.escapeCircles.empty() ? ownFeet : plan.escapeCircles,
                                                 SWEEP_RADIUS, SWEEP_DISTANCE_STEP, SWEEP_ANGLE_STEP,
                                                 &plan.preferNear, plan.escapeAccept, &sweep);
    }

    bool const cureWalk = plan.reason == WormMoveReason::Cure || plan.reason == WormMoveReason::Run;
    if (spot == Position() && cureWalk)
        spot = DirectCureSpot(bot, plan);

    if (spot == Position())
    {
        NoteWormMove(bot, "none " + reason);
        return plan.urgent;
    }

    if (pinned)
        bot->InterruptNonMeleeSpells(true);

    // Our own stale walk holds the booking and would refuse its replacement
    if (ownBooking)
        last.clear();

    MovementPriority const priority =
        plan.urgent || cureWalk ? MovementPriority::MOVEMENT_FORCED : MovementPriority::MOVEMENT_COMBAT;
    float const travel = bot->GetExactDist2d(spot);
    switch (TryMoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                      false, true, priority, true))
    {
        case RaidObs::MoveOutcome::Issued:
            break;
        case RaidObs::MoveOutcome::AlreadyThere:
            NoteWormMove(bot, "clear");
            return false;
        default:
            NoteWormMove(bot, "locked");
            return plan.urgent;
    }

    SetWormWalk(bot, spot);
    NoteWormMove(bot, "move " + Yards(travel) + " " + reason);
    return true;
}
