/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_PUTRICIDE_H
#define PLAYERBOTS_ICCACTIONS_PUTRICIDE_H

#include <string>

#include "Action.h"
#include "ICCActions_RaidWide.h"
#include "ICCHelpers_Putricide.h"
#include "MovementActions.h"
#include "Position.h"
#include "RaidDirectTargeting.h"

// Holds its spot while the walk there is in flight. Every MoveTo clears the MotionMaster, so a spot
// re-issued each tick to a bot still walking never lands.
class IccPutricideWalkAction : public MovementAction
{
public:
    IccPutricideWalkAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

protected:
    // The latched spot while the bot is still walking to it, else nullptr
    Position const* WalkInFlight();
    // False without booking anything while a cast pins the bot's feet. A walk of our own still in flight at
    // the same priority would refuse the new one, so its booking is lowered first.
    bool WalkTo(Position const& spot, MovementPriority priority);
    void DropWalk() { walking = false; }

private:
    bool BookedOnWalkSpot();

    Position walkSpot;
    uint32 walkIssuedMs = 0;
    bool walking = false;
};

class IccPutricideDodgeAction : public IccPutricideWalkAction
{
public:
    static constexpr char const* Name = "icc putricide dodge";

    IccPutricideDodgeAction(PlayerbotAI* botAI) : IccPutricideWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccPutricideSwapAction : public RaidDirectAttackAction
{
public:
    static constexpr char const* Name = "icc putricide swap";

    IccPutricideSwapAction(PlayerbotAI* botAI) : RaidDirectAttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccPutricideCastTankDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "icc putricide cast tank defensive";

    IccPutricideCastTankDefensiveAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccPutricideKiteAction : public IccPutricideWalkAction
{
public:
    static constexpr char const* Name = "icc putricide kite";

    IccPutricideKiteAction(PlayerbotAI* botAI) : IccPutricideWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccPutricidePlagueAction : public IccPutricideWalkAction
{
public:
    static constexpr char const* Name = "icc putricide plague";

    IccPutricidePlagueAction(PlayerbotAI* botAI) : IccPutricideWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccPutricideStackAction : public IccPutricideWalkAction
{
public:
    static constexpr char const* Name = "icc putricide stack";

    IccPutricideStackAction(PlayerbotAI* botAI) : IccPutricideWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccPutricideAbominationAction : public IccPutricideWalkAction
{
public:
    static constexpr char const* Name = "icc putricide abomination";

    IccPutricideAbominationAction(PlayerbotAI* botAI) : IccPutricideWalkAction(botAI, Name) {}
    bool Execute(Event event) override;

private:
    uint32 lastDrinkMs = 0;
    // The abomination's own walk: the bot rides it, so the bot itself never shows as moving
    Position driveSpot;
    bool driving = false;
};

class IccPutricideBrakeStopAction : public Action
{
public:
    static constexpr char const* Name = "icc putricide brake stop";

    IccPutricideBrakeStopAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccPutricideTargetAction : public IccDirectAttackAction
{
public:
    static constexpr char const* Name = "icc putricide target";

    IccPutricideTargetAction(PlayerbotAI* botAI) : IccDirectAttackAction(botAI, Name, &GetPpTarget) {}
};

class IccPutricideStandAction : public IccPutricideWalkAction
{
public:
    static constexpr char const* Name = "icc putricide stand";

    IccPutricideStandAction(PlayerbotAI* botAI) : IccPutricideWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

#endif
