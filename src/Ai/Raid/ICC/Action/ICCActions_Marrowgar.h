/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_MARROWGAR_H
#define PLAYERBOTS_ICCACTIONS_MARROWGAR_H

#include <string>

#include "ICCActions_RaidWide.h"
#include "ICCHelpers_Marrowgar.h"
#include "MovementActions.h"
#include "Position.h"

class IccLmAttackTargetAction : public IccDirectAttackAction
{
public:
    static constexpr char const* Name = "icc lm attack target";

    IccLmAttackTargetAction(PlayerbotAI* botAI) : IccDirectAttackAction(botAI, Name, &GetLmTarget) {}
};

// Holds its spot while the walk there is in flight. Every MoveTo clears the MotionMaster, so a spot
// re-issued each tick to a bot still walking never lands.
class IccLmWalkAction : public MovementAction
{
public:
    IccLmWalkAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

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

class IccLmMoveToStandAction : public IccLmWalkAction
{
public:
    static constexpr char const* Name = "icc lm move to stand";

    IccLmMoveToStandAction(PlayerbotAI* botAI) : IccLmWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccLmDodgeColdflameAction : public IccLmWalkAction
{
public:
    static constexpr char const* Name = "icc lm dodge coldflame";

    IccLmDodgeColdflameAction(PlayerbotAI* botAI) : IccLmWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

#endif
