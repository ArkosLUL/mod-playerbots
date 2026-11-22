#ifndef PLAYERBOTS_RAID_TOCACTIONS_NORTHRENDBEASTS_H
#define PLAYERBOTS_RAID_TOCACTIONS_NORTHRENDBEASTS_H

#include <cmath>

#include "Action.h"
#include "LastMovementValue.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "Position.h"
#include "RaidDirectTargeting.h"
#include "Timer.h"

class NorthrendBeastsDpsTargetAction : public RaidDirectAttackAction
{
public:
    NorthrendBeastsDpsTargetAction(PlayerbotAI* botAI, std::string const name) : RaidDirectAttackAction(botAI, name) {}
    bool Execute(Event event) override;
};

class NorthrendBeastsWalkAction : public MovementAction
{
public:
    NorthrendBeastsWalkAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

protected:
    Position const* WalkInFlight()
    {
        if (!_walking || !bot->isMoving() || !BookedOnWalkSpot())
            return nullptr;

        return &_walkSpot;
    }

    bool WalkTo(Position const& spot, MovementPriority priority = MovementPriority::MOVEMENT_COMBAT)
    {
        if (bot->IsMovementPreventedByCasting())
            return false;

        uint32 const now = getMSTime();
        if (_walking && !bot->isMoving() && getMSTimeDiff(_walkIssuedMs, now) > WALK_STALL_MS && BookedOnWalkSpot())
            AI_VALUE(LastMovement&, "last movement").clear();

        _walkSpot = spot;
        _walking = true;
        _walkIssuedMs = now;
        return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                      false, false, priority);
    }

    void DropWalk() { _walking = false; }

private:
    static constexpr float WALK_SPOT_TOLERANCE = 0.5f;
    static constexpr uint32 WALK_STALL_MS = 500;

    bool BookedOnWalkSpot()
    {
        LastMovement& last = AI_VALUE(LastMovement&, "last movement");
        return std::fabs(last.lastMoveToX - _walkSpot.GetPositionX()) <= WALK_SPOT_TOLERANCE &&
               std::fabs(last.lastMoveToY - _walkSpot.GetPositionY()) <= WALK_SPOT_TOLERANCE;
    }

    Position _walkSpot;
    uint32 _walkIssuedMs = 0;
    bool _walking = false;
};

#endif
