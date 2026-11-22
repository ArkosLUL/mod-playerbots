#ifndef PLAYERBOTS_RAID_TOCACTIONS_ICEHOWL_H
#define PLAYERBOTS_RAID_TOCACTIONS_ICEHOWL_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"
#include "RaidObs.h"
#include "ToCActions_NorthrendBeasts.h"

class IcehowlTankHoldBossAction : public AttackAction
{
public:
    static constexpr char const* Name = "icehowl tank hold boss";

    IcehowlTankHoldBossAction(PlayerbotAI* botAI) : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IcehowlClearChargePathAction : public MovementAction
{
public:
    static constexpr char const* Name = "icehowl clear charge path";

    IcehowlClearChargePathAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;

private:
    MoveOutcome IssueDodge(Position const& spot, bool replacing);

    Position _dodgeSpot;
    uint32 _dodgeSpotMs = 0;
    bool _hasDodgeSpot = false;
    bool _dodgeParked = false;
};

class IcehowlTankDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "icehowl tank defensive";

    IcehowlTankDefensiveAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IcehowlMoveToBreathStandAction : public NorthrendBeastsWalkAction
{
public:
    static constexpr char const* Name = "icehowl move to breath stand";

    IcehowlMoveToBreathStandAction(PlayerbotAI* botAI) : NorthrendBeastsWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IcehowlDazeNitroAction : public Action
{
public:
    static constexpr char const* Name = "icehowl daze nitro";

    IcehowlDazeNitroAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool isUseful() override;
    bool Execute(Event event) override;

private:
    uint32 _triedMs = 0;
    bool _tried = false;
};

#endif
