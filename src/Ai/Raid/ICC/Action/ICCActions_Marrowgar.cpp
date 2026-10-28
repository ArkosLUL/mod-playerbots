/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCActions_Marrowgar.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "EncounterHelpers.h"
#include "LastMovementValue.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "Timer.h"

using namespace EncounterHelpers;

namespace
{
constexpr float WALK_SPOT_TOLERANCE = 0.5f;
// A spline issued last tick may not show in isMoving yet
constexpr uint32 WALK_STALL_MS = 500;
constexpr float DODGE_SWEEP_RADIUS = 16.0f;

void NoteDodge(Player* bot, std::string const& branch)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "lm.dodge", branch);
}

bool ClearOf(Position const& spot, std::vector<Position> const& circles, float radius)
{
    return std::none_of(circles.begin(), circles.end(),
                        [&spot, radius](Position const& circle) { return circle.GetExactDist2d(spot) < radius; });
}
}  // namespace

Position const* IccLmWalkAction::WalkInFlight()
{
    // Moving on some other node's walk doesn't count
    if (!walking || !bot->isMoving() || !BookedOnWalkSpot())
        return nullptr;

    return &walkSpot;
}

bool IccLmWalkAction::WalkTo(Position const& spot, MovementPriority priority)
{
    // A walk issued mid-cast never starts but still books its travel time, and the same spot is refused
    // until that runs out
    if (bot->IsMovementPreventedByCasting())
        return false;

    // A stun or a channel stopped the last walk short, and its booking would refuse this one
    uint32 const now = getMSTime();
    if (walking && !bot->isMoving() && getMSTimeDiff(walkIssuedMs, now) > WALK_STALL_MS && BookedOnWalkSpot())
        AI_VALUE(LastMovement&, "last movement").clear();

    walkSpot = spot;
    walking = true;
    walkIssuedMs = now;
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false, false,
                  false, priority);
}

bool IccLmWalkAction::BookedOnWalkSpot()
{
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    return std::fabs(last.lastMoveToX - walkSpot.GetPositionX()) <= WALK_SPOT_TOLERANCE &&
           std::fabs(last.lastMoveToY - walkSpot.GetPositionY()) <= WALK_SPOT_TOLERANCE;
}

bool IccLmMoveToStandAction::Execute(Event /*event*/)
{
    Position spot;
    if (!GetLmStandWalk(botAI, spot))
    {
        DropWalk();
        return false;
    }

    if (Position const* inFlight = WalkInFlight())
        if (inFlight->GetExactDist2d(spot) <= WALK_SPOT_TOLERANCE)
            return true;

    return WalkTo(spot, MovementPriority::MOVEMENT_COMBAT);
}

bool IccLmDodgeColdflameAction::Execute(Event /*event*/)
{
    if (!IsLmInColdflame(botAI))
    {
        DropWalk();
        return false;
    }

    if (!botAI->CanMove())
    {
        NoteDodge(bot, "stunned");
        return false;
    }

    std::vector<Position> circles;
    GetLmColdflame(botAI, circles);

    Position const* inFlight = WalkInFlight();
    if (inFlight && ClearOf(*inFlight, circles, LM_DODGE_TRIGGER))
    {
        NoteDodge(bot, "hold");
        return true;
    }

    // A patch ticks every second for 6-11k: the cast is worth less than the step out
    if (bot->IsMovementPreventedByCasting())
        bot->InterruptNonMeleeSpells(true);

    Position stand;
    Position const* preferNear = GetLmStandSpot(botAI, stand) ? &stand : nullptr;
    auto const insideGate = [](float x, float /*y*/) { return x < LM_GATE_X_LIMIT; };

    HazardSweepCache sweep;
    auto const findSpot = [&](float clearance)
    {
        std::vector<HazardCircle> hazards;
        hazards.reserve(circles.size());
        for (Position const& circle : circles)
            hazards.emplace_back(circle, clearance);
        return FindNearestPositionClearOfHazards(bot, hazards, DODGE_SWEEP_RADIUS, 2.0f,
                                                 static_cast<float>(M_PI) / 8.0f, preferNear, insideGate, &sweep);
    };

    bool tight = false;
    Position spot = findSpot(LM_DODGE_CLEARANCE);
    if (spot == Position())
    {
        tight = true;
        spot = findSpot(LM_DODGE_TIGHT_CLEARANCE);
    }

    if (spot == Position())
    {
        NoteDodge(bot, "none");
        return false;
    }

    // Our own walk still holds the FORCED booking, and a booking only yields to a higher priority
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    bool const lowered = inFlight && last.priority == MovementPriority::MOVEMENT_FORCED;
    if (lowered)
        last.priority = MovementPriority::MOVEMENT_COMBAT;

    float const travel = bot->GetExactDist2d(spot);
    if (!WalkTo(spot, MovementPriority::MOVEMENT_FORCED))
    {
        if (lowered)
            last.priority = MovementPriority::MOVEMENT_FORCED;

        NoteDodge(bot, "locked");
        return false;
    }

    NoteDodge(bot, tight ? "tight" : "move " + std::to_string(static_cast<int32>(std::lround(travel))));
    return true;
}
