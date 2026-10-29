/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_BLOODPRINCES_H
#define PLAYERBOTS_ICCACTIONS_BLOODPRINCES_H

#include <string>

#include "Action.h"
#include "ICCActions_RaidWide.h"
#include "ICCHelpers_BloodPrinces.h"
#include "MovementActions.h"
#include "Position.h"
#include "RaidDirectTargeting.h"

// Holds its spot while the walk there is in flight. Every MoveTo clears the MotionMaster, so a spot
// re-issued each tick to a bot still walking never lands.
class IccBpcWalkAction : public MovementAction
{
public:
    IccBpcWalkAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

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

class IccBpcTargetAction : public IccDirectAttackAction
{
public:
    static constexpr char const* Name = "icc bpc target";

    IccBpcTargetAction(PlayerbotAI* botAI) : IccDirectAttackAction(botAI, Name, &GetBpcTarget) {}
};

class IccBpcStandAction : public IccBpcWalkAction
{
public:
    static constexpr char const* Name = "icc bpc stand";

    IccBpcStandAction(PlayerbotAI* botAI) : IccBpcWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

#endif
