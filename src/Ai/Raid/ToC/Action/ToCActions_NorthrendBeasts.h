#ifndef PLAYERBOTS_RAID_TOCACTIONS_NORTHRENDBEASTS_H
#define PLAYERBOTS_RAID_TOCACTIONS_NORTHRENDBEASTS_H

#include <cmath>

#include "Action.h"
#include "LastMovementValue.h"
#include "MovementActions.h"
#include "NamedObjectContext.h"
#include "Playerbots.h"
#include "Position.h"
#include "Timer.h"

// Holds its spot while the walk there is in flight. Every MoveTo clears the MotionMaster, so a spot
// re-derived each tick from a bot that's still walking never lands.
class NorthrendBeastsWalkAction : public MovementAction
{
public:
    NorthrendBeastsWalkAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {};

protected:
    // The latched spot while the bot is still walking to it, else nullptr
    Position const* WalkInFlight()
    {
        // Moving on some other node's walk doesn't count
        if (!walking || !bot->isMoving() || !BookedOnWalkSpot())
            return nullptr;

        return &walkSpot;
    }

    // False without booking anything while a cast pins the bot's feet
    bool WalkTo(Position const& spot, MovementPriority priority = MovementPriority::MOVEMENT_COMBAT)
    {
        // A walk issued mid cast never starts but still books its travel time, and the same spot is
        // refused until that runs out. Wait for the cast to end instead.
        if (bot->IsMovementPreventedByCasting())
            return false;

        // A stun, a freeze or a channel stopped our last walk short, and its booking would refuse this one
        uint32 const now = getMSTime();
        if (walking && !bot->isMoving() && getMSTimeDiff(walkIssuedMs, now) > WALK_STALL_MS && BookedOnWalkSpot())
            AI_VALUE(LastMovement&, "last movement").clear();

        walkSpot = spot;
        walking = true;
        walkIssuedMs = now;
        return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                      false, false, priority);
    }

    void DropWalk() { walking = false; }

private:
    static constexpr float WALK_SPOT_TOLERANCE = 0.5f;
    // A spline issued last tick may not show in isMoving yet
    static constexpr uint32 WALK_STALL_MS = 500;

    bool BookedOnWalkSpot()
    {
        LastMovement& last = AI_VALUE(LastMovement&, "last movement");
        return std::fabs(last.lastMoveToX - walkSpot.GetPositionX()) <= WALK_SPOT_TOLERANCE &&
               std::fabs(last.lastMoveToY - walkSpot.GetPositionY()) <= WALK_SPOT_TOLERANCE;
    }

    Position walkSpot;
    uint32 walkIssuedMs = 0;
    bool walking = false;
};

// Actions shared by Gormok, the Jormungars and Icehowl.
class ToCNorthrendBeastsActionContext : public NamedObjectContext<Action>
{
};

#endif
