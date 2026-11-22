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

class IccVdWalkAction : public MovementAction
{
public:
    IccVdWalkAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

protected:
    Position const* WalkInFlight();
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
