/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_VALITHRIA_H
#define PLAYERBOTS_ICCACTIONS_VALITHRIA_H

#include <string>

#include "Action.h"
#include "ICCActions_RaidWide.h"
#include "ICCHelpers_Valithria.h"
#include "MovementActions.h"
#include "Position.h"
#include "RaidDirectTargeting.h"

class Creature;

// Holds its spot while the walk there is in flight. Every MoveTo clears the MotionMaster, so a spot
// re-issued each tick to a bot still walking never lands.
class IccVdWalkAction : public MovementAction
{
public:
    IccVdWalkAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

protected:
    // The latched spot while the bot is still walking to it, else nullptr
    Position const* WalkInFlight();
    // False without booking anything while a cast pins the bot's feet, unless `interrupt` drops the cast
    bool WalkTo(Position const& spot, MovementPriority priority, bool interrupt = false);
    void DropWalk() { walking = false; }

private:
    bool BookedOnWalkSpot();

    Position walkSpot;
    uint32 walkIssuedMs = 0;
    bool walking = false;
};

class IccValithriaDodgeAction : public IccVdWalkAction
{
public:
    static constexpr char const* Name = "icc valithria dodge";

    IccValithriaDodgeAction(PlayerbotAI* botAI) : IccVdWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccValithriaKiteAction : public IccVdWalkAction
{
public:
    static constexpr char const* Name = "icc valithria kite";

    IccValithriaKiteAction(PlayerbotAI* botAI) : IccVdWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

// Flies in Dream State on straight splines: the navmesh would pull an airborne point down to the floor.
class IccValithriaDreamAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc valithria dream";

    IccValithriaDreamAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;

private:
    bool FlyTo(Position const& dest);
    bool Land();

    Position flightGoal;
    bool flying = false;
};

class IccValithriaPortalAction : public IccVdWalkAction
{
public:
    static constexpr char const* Name = "icc valithria portal";

    IccValithriaPortalAction(PlayerbotAI* botAI) : IccVdWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccValithriaTauntAction : public RaidDirectAttackAction
{
public:
    static constexpr char const* Name = "icc valithria taunt";

    IccValithriaTauntAction(PlayerbotAI* botAI) : RaidDirectAttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccValithriaSnareAction : public Action
{
public:
    static constexpr char const* Name = "icc valithria snare";

    IccValithriaSnareAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccValithriaVoidAction : public IccVdWalkAction
{
public:
    static constexpr char const* Name = "icc valithria void";

    IccValithriaVoidAction(PlayerbotAI* botAI) : IccVdWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccValithriaTargetAction : public IccDirectAttackAction
{
public:
    static constexpr char const* Name = "icc valithria target";

    IccValithriaTargetAction(PlayerbotAI* botAI) : IccDirectAttackAction(botAI, Name, &GetVdTarget) {}
};

class IccValithriaInterruptAction : public Action
{
public:
    static constexpr char const* Name = "icc valithria interrupt";

    IccValithriaInterruptAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccValithriaFreedomAction : public Action
{
public:
    static constexpr char const* Name = "icc valithria freedom";

    IccValithriaFreedomAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccValithriaCastTankDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "icc valithria cast tank defensive";

    IccValithriaCastTankDefensiveAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

// Cast here rather than through the generic node: the burst gate holds lust off every target in this fight
class IccValithriaLustAction : public Action
{
public:
    static constexpr char const* Name = "icc valithria lust";

    IccValithriaLustAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccValithriaGuardianSpiritAction : public Action
{
public:
    static constexpr char const* Name = "icc valithria guardian spirit";

    IccValithriaGuardianSpiritAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccValithriaBeaconAction : public Action
{
public:
    static constexpr char const* Name = "icc valithria beacon";

    IccValithriaBeaconAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

// Heals her by name, the bot's best rank: she is no party member, so no generic heal reaches her
class IccValithriaHealAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc valithria heal";

    IccValithriaHealAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;

private:
    bool CastHeal(Creature* valithria);
};

#endif
