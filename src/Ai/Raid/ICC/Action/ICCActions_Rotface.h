/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_ROTFACE_H
#define PLAYERBOTS_ICCACTIONS_ROTFACE_H

#include <string>

#include "Action.h"
#include "ICCActions_RaidWide.h"
#include "ICCHelpers_Rotface.h"
#include "MovementActions.h"
#include "Position.h"
#include "RaidDirectTargeting.h"

// Holds its spot while the walk there is in flight. Every MoveTo clears the MotionMaster, so a spot
// re-issued each tick to a bot still walking never lands.
class IccRotfaceWalkAction : public MovementAction
{
public:
    IccRotfaceWalkAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

protected:
    // The latched spot while the bot is still walking to it, else nullptr
    Position const* WalkInFlight();
    // False without booking anything while a cast pins the bot's feet
    bool WalkTo(Position const& spot, MovementPriority priority);
    void DropWalk() { walking = false; }

private:
    bool BookedOnWalkSpot();

    Position walkSpot;
    uint32 walkIssuedMs = 0;
    bool walking = false;
};

class IccRotfaceDodgeAction : public IccRotfaceWalkAction
{
public:
    static constexpr char const* Name = "icc rotface dodge";

    IccRotfaceDodgeAction(PlayerbotAI* botAI) : IccRotfaceWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccRotfaceCastTankDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "icc rotface cast tank defensive";

    IccRotfaceCastTankDefensiveAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccRotfaceDispelAction : public Action
{
public:
    static constexpr char const* Name = "icc rotface dispel";

    IccRotfaceDispelAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccRotfaceOozeTauntAction : public RaidDirectAttackAction
{
public:
    static constexpr char const* Name = "icc rotface ooze taunt";

    IccRotfaceOozeTauntAction(PlayerbotAI* botAI) : RaidDirectAttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccRotfaceKiteAction : public IccRotfaceWalkAction
{
public:
    static constexpr char const* Name = "icc rotface kite";

    IccRotfaceKiteAction(PlayerbotAI* botAI) : IccRotfaceWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccRotfaceCarrierAction : public IccRotfaceWalkAction
{
public:
    static constexpr char const* Name = "icc rotface carrier";

    IccRotfaceCarrierAction(PlayerbotAI* botAI) : IccRotfaceWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccRotfaceTargetAction : public IccDirectAttackAction
{
public:
    static constexpr char const* Name = "icc rotface target";

    IccRotfaceTargetAction(PlayerbotAI* botAI) : IccDirectAttackAction(botAI, Name, &GetRfTarget) {}
};

class IccRotfaceStandAction : public IccRotfaceWalkAction
{
public:
    static constexpr char const* Name = "icc rotface stand";

    IccRotfaceStandAction(PlayerbotAI* botAI) : IccRotfaceWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

#endif
