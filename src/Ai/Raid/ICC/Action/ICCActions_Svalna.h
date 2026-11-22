/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_SVALNA_H
#define PLAYERBOTS_ICCACTIONS_SVALNA_H

#include <string>

#include "Action.h"
#include "ICCActions_RaidWide.h"
#include "ICCHelpers_Svalna.h"
#include "MovementActions.h"
#include "Position.h"
#include "RaidDirectTargeting.h"

class IccSvalnaSpearAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc valkyre spear";

    IccSvalnaSpearAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSvalnaHurlAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc sister svalna hurl";

    IccSvalnaHurlAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSvalnaInterruptAction : public Action
{
public:
    static constexpr char const* Name = "icc sister svalna interrupt";

    IccSvalnaInterruptAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSvalnaCastTankDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "icc sister svalna cast tank defensive";

    IccSvalnaCastTankDefensiveAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSvalnaTauntAction : public RaidDirectAttackAction
{
public:
    static constexpr char const* Name = "icc sister svalna taunt";

    IccSvalnaTauntAction(PlayerbotAI* botAI) : RaidDirectAttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSvalnaTargetAction : public IccDirectAttackAction
{
public:
    static constexpr char const* Name = "icc sister svalna target";

    IccSvalnaTargetAction(PlayerbotAI* botAI) : IccDirectAttackAction(botAI, Name, &GetSsTarget) {}
};

class IccSsWalkAction : public MovementAction
{
public:
    IccSsWalkAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

protected:
    Position const* WalkInFlight();
    bool WalkTo(Position const& spot, MovementPriority priority);
    void DropWalk() { walking = false; }

private:
    bool BookedOnWalkSpot();

    Position walkSpot;
    uint32 walkIssuedMs = 0;
    bool walking = false;
};

class IccSvalnaGauntletDodgeAction : public IccSsWalkAction
{
public:
    static constexpr char const* Name = "icc sister svalna gauntlet dodge";

    IccSvalnaGauntletDodgeAction(PlayerbotAI* botAI) : IccSsWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSvalnaGauntletInterruptAction : public Action
{
public:
    static constexpr char const* Name = "icc sister svalna gauntlet interrupt";

    IccSvalnaGauntletInterruptAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSvalnaGauntletCastTankDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "icc sister svalna gauntlet cast tank defensive";

    IccSvalnaGauntletCastTankDefensiveAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSvalnaGauntletTauntAction : public RaidDirectAttackAction
{
public:
    static constexpr char const* Name = "icc sister svalna gauntlet taunt";

    IccSvalnaGauntletTauntAction(PlayerbotAI* botAI) : RaidDirectAttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSvalnaGauntletTargetAction : public IccDirectAttackAction
{
public:
    static constexpr char const* Name = "icc sister svalna gauntlet target";

    IccSvalnaGauntletTargetAction(PlayerbotAI* botAI) : IccDirectAttackAction(botAI, Name, &GetSsGauntletTarget) {}
};

#endif
