/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_BLOODQUEEN_H
#define PLAYERBOTS_ICCACTIONS_BLOODQUEEN_H

#include <string>

#include "Action.h"
#include "ICCActions_RaidWide.h"
#include "ICCHelpers_BloodQueen.h"
#include "MovementActions.h"
#include "Position.h"
#include "RaidAntiFear.h"
#include "RaidDirectTargeting.h"

// Holds its spot while the walk there is in flight. Every MoveTo clears the MotionMaster, so a spot
// re-issued each tick to a bot still walking never lands.
class IccBqlWalkAction : public MovementAction
{
public:
    IccBqlWalkAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

protected:
    // The latched spot while the bot is still walking to it, else nullptr
    Position const* WalkInFlight();
    // False without booking anything while a cast pins the bot's feet, unless `interrupt` drops the cast.
    // A walk of its own still in flight at MOVEMENT_FORCED gives way to the new one.
    bool WalkTo(Position const& spot, MovementPriority priority, bool interrupt = false);
    void DropWalk() { walking = false; }

private:
    bool BookedOnWalkSpot();

    Position walkSpot;
    uint32 walkIssuedMs = 0;
    bool walking = false;
};

class IccBqlDodgeAction : public IccBqlWalkAction
{
public:
    static constexpr char const* Name = "icc bql dodge";

    IccBqlDodgeAction(PlayerbotAI* botAI) : IccBqlWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccBqlBiteAction : public IccBqlWalkAction
{
public:
    static constexpr char const* Name = "icc bql vampiric bite";

    IccBqlBiteAction(PlayerbotAI* botAI) : IccBqlWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccBqlShadowsAction : public IccBqlWalkAction
{
public:
    static constexpr char const* Name = "icc bql swarming shadows";

    IccBqlShadowsAction(PlayerbotAI* botAI) : IccBqlWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccBqlPactAction : public IccBqlWalkAction
{
public:
    static constexpr char const* Name = "icc bql pact of darkfallen";

    IccBqlPactAction(PlayerbotAI* botAI) : IccBqlWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccBqlCastWhirlDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "icc bql cast whirl defensive";

    IccBqlCastWhirlDefensiveAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccBqlCastWhirlCoverAction : public Action
{
public:
    static constexpr char const* Name = "icc bql cast whirl cover";

    IccBqlCastWhirlCoverAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccBqlCastTankDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "icc bql cast tank defensive";

    IccBqlCastTankDefensiveAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccBqlAntiFearAction : public RaidAntiFearAction
{
public:
    static constexpr char const* Name = "icc bql anti fear";

    IccBqlAntiFearAction(PlayerbotAI* botAI) : RaidAntiFearAction(botAI, Name) {}

protected:
    bool FearWindowActive() override;
};

class IccBqlFrenzyCcAction : public Action
{
public:
    static constexpr char const* Name = "icc bql frenzy cc";

    IccBqlFrenzyCcAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccBqlTauntAction : public RaidDirectAttackAction
{
public:
    static constexpr char const* Name = "icc bql taunt";

    IccBqlTauntAction(PlayerbotAI* botAI) : RaidDirectAttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccBqlBrakeStopAction : public Action
{
public:
    static constexpr char const* Name = "icc bql brake stop";

    IccBqlBrakeStopAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccBqlTargetAction : public IccDirectAttackAction
{
public:
    static constexpr char const* Name = "icc bql target";

    IccBqlTargetAction(PlayerbotAI* botAI) : IccDirectAttackAction(botAI, Name, &GetBqlTarget) {}
};

class IccBqlStandAction : public IccBqlWalkAction
{
public:
    static constexpr char const* Name = "icc bql stand";

    IccBqlStandAction(PlayerbotAI* botAI) : IccBqlWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

#endif
