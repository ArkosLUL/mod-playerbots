/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_BLOODPRINCES_H
#define PLAYERBOTS_ICCACTIONS_BLOODPRINCES_H

#include <string>

#include "Action.h"
#include "ICCHelpers_BloodPrinces.h"
#include "MovementActions.h"
#include "Position.h"
#include "RaidDirectTargeting.h"

class IccBpcWalkAction : public MovementAction
{
public:
    IccBpcWalkAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

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

class IccBpcDodgeAction : public IccBpcWalkAction
{
public:
    static constexpr char const* Name = "icc bpc dodge";

    IccBpcDodgeAction(PlayerbotAI* botAI) : IccBpcWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccBpcCastTankDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "icc bpc cast tank defensive";

    IccBpcCastTankDefensiveAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccBpcBallAction : public IccBpcWalkAction
{
public:
    static constexpr char const* Name = "icc bpc ball";

    IccBpcBallAction(PlayerbotAI* botAI) : IccBpcWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccBpcPrinceTauntAction : public RaidDirectAttackAction
{
public:
    static constexpr char const* Name = "icc bpc prince taunt";

    IccBpcPrinceTauntAction(PlayerbotAI* botAI) : RaidDirectAttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccBpcNucleusAction : public IccBpcWalkAction
{
public:
    static constexpr char const* Name = "icc bpc nucleus";

    IccBpcNucleusAction(PlayerbotAI* botAI) : IccBpcWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccBpcSoakAction : public IccBpcWalkAction
{
public:
    static constexpr char const* Name = "icc bpc soak";

    IccBpcSoakAction(PlayerbotAI* botAI) : IccBpcWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccBpcCastLanceDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "icc bpc cast lance defensive";

    IccBpcCastLanceDefensiveAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccBpcCastFlameDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "icc bpc cast flame defensive";

    IccBpcCastFlameDefensiveAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccBpcDispelAction : public Action
{
public:
    static constexpr char const* Name = "icc bpc dispel";

    IccBpcDispelAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccBpcBrakeStopAction : public Action
{
public:
    static constexpr char const* Name = "icc bpc brake stop";

    IccBpcBrakeStopAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccBpcTargetAction : public RaidDirectAttackAction
{
public:
    static constexpr char const* Name = "icc bpc target";

    IccBpcTargetAction(PlayerbotAI* botAI) : RaidDirectAttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccBpcStandAction : public IccBpcWalkAction
{
public:
    static constexpr char const* Name = "icc bpc stand";

    IccBpcStandAction(PlayerbotAI* botAI) : IccBpcWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

#endif
