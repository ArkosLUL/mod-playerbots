/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */


#include "OSActions.h"
#include "OSTriggers.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "Random.h"
#include "Timer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

using namespace OsHelpers;

// The one destination gate every positioning action in this strategy goes through.

bool OsPositioningAction::MoveToClamped(float x, float y, float tolerance, MovementPriority priority)
{
    ClampDestination(x, y);

    if (bot->GetExactDist2d(x, y) <= tolerance)
        return false;

    return IssueMove(x, y, priority, priority == MovementPriority::MOVEMENT_FORCED);
}

bool OsPositioningAction::IssueMove(float x, float y, MovementPriority priority, bool rejectOnCollision)
{
    float z = 0.0f;
    if (!ResolveMoveDestination(bot, x, y, z, rejectOnCollision))
        return false;

    // lessDelay shortens the lock this move stamps by one react delay, so the next tick is not spent
    // waiting on a lock that was already paid for.
    return MoveTo(OS_MAP_ID, x, y, z, false, false, false, false, priority, true);
}

bool OsHoldAction::IssueMove(float x, float y, MovementPriority priority, bool rejectOnCollision)
{
    float z = 0.0f;
    if (!ResolveMoveDestination(bot, x, y, z, rejectOnCollision))
        return false;

    return MoveTo(OS_MAP_ID, x, y, z, false, false, false, false, priority, true);
}
