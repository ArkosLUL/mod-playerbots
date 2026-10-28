/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_FESTERGUT_H
#define PLAYERBOTS_ICCACTIONS_FESTERGUT_H

#include <string>

#include "Action.h"
#include "ICCActions_RaidWide.h"
#include "ICCHelpers_Festergut.h"
#include "MovementActions.h"
#include "Position.h"
#include "RaidDirectTargeting.h"

// Holds its spot while the walk there is in flight: every MoveTo clears the MotionMaster, so a spot
// re-issued each tick to a bot still walking never lands.
class IccFestergutWalkAction : public MovementAction
{
public:
    IccFestergutWalkAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

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

class IccFestergutDodgeGooAction : public IccFestergutWalkAction
{
public:
    static constexpr char const* Name = "icc festergut dodge goo";

    IccFestergutDodgeGooAction(PlayerbotAI* botAI) : IccFestergutWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccFestergutCastPungentDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "icc festergut cast pungent defensive";

    IccFestergutCastPungentDefensiveAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccFestergutCastPungentCoverAction : public Action
{
public:
    static constexpr char const* Name = "icc festergut cast pungent cover";

    IccFestergutCastPungentCoverAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccFestergutCastTankDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "icc festergut cast tank defensive";

    IccFestergutCastTankDefensiveAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccFestergutBloatSwapAction : public RaidDirectAttackAction
{
public:
    static constexpr char const* Name = "icc festergut bloat swap";

    IccFestergutBloatSwapAction(PlayerbotAI* botAI) : RaidDirectAttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

// Drops the attack on him while the threat brake holds this tank
class IccFestergutBrakeStopAction : public Action
{
public:
    static constexpr char const* Name = "icc festergut brake stop";

    IccFestergutBrakeStopAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccFestergutTargetAction : public IccDirectAttackAction
{
public:
    static constexpr char const* Name = "icc festergut target";

    IccFestergutTargetAction(PlayerbotAI* botAI) : IccDirectAttackAction(botAI, Name, &GetFgTarget) {}
};

class IccFestergutStandAction : public IccFestergutWalkAction
{
public:
    static constexpr char const* Name = "icc festergut stand";

    IccFestergutStandAction(PlayerbotAI* botAI) : IccFestergutWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

#endif
