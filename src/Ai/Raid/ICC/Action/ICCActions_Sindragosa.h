/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_SINDRAGOSA_H
#define PLAYERBOTS_ICCACTIONS_SINDRAGOSA_H

#include <string>

#include "Action.h"
#include "ICCActions_RaidWide.h"
#include "ICCHelpers_Sindragosa.h"
#include "LastMovementValue.h"
#include "MovementActions.h"
#include "Position.h"
#include "RaidDirectTargeting.h"

class IccSindragosaBossWalkAction : public MovementAction
{
public:
    IccSindragosaBossWalkAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {}

protected:
    bool WalkFor(bool (*getter)(PlayerbotAI*, Position&), MovementPriority priority, bool interrupt, bool nitro);

private:
    Position const* WalkInFlight();
    bool WalkTo(Position const& spot, MovementPriority priority, bool interrupt);
    bool BookedOnWalkSpot();

    Position walkSpot;
    uint32 walkIssuedMs = 0;
    bool walking = false;
};

class IccSindragosaColdAction : public IccSindragosaBossWalkAction
{
public:
    static constexpr char const* Name = "icc sindragosa blistering cold";

    IccSindragosaColdAction(PlayerbotAI* botAI) : IccSindragosaBossWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSindragosaSplashAction : public IccSindragosaBossWalkAction
{
public:
    static constexpr char const* Name = "icc sindragosa splash";

    IccSindragosaSplashAction(PlayerbotAI* botAI) : IccSindragosaBossWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSindragosaBombHideAction : public IccSindragosaBossWalkAction
{
public:
    static constexpr char const* Name = "icc sindragosa bomb hide";

    IccSindragosaBombHideAction(PlayerbotAI* botAI) : IccSindragosaBossWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSindragosaBeaconAction : public IccSindragosaBossWalkAction
{
public:
    static constexpr char const* Name = "icc sindragosa frost beacon";

    IccSindragosaBeaconAction(PlayerbotAI* botAI) : IccSindragosaBossWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSindragosaCastTankDefensiveAction : public Action
{
public:
    static constexpr char const* Name = "icc sindragosa cast tank defensive";

    IccSindragosaCastTankDefensiveAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSindragosaTauntAction : public RaidDirectAttackAction
{
public:
    static constexpr char const* Name = "icc sindragosa taunt";

    IccSindragosaTauntAction(PlayerbotAI* botAI) : RaidDirectAttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSindragosaBuffetHideAction : public IccSindragosaBossWalkAction
{
public:
    static constexpr char const* Name = "icc sindragosa buffet hide";

    IccSindragosaBuffetHideAction(PlayerbotAI* botAI) : IccSindragosaBossWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSindragosaUnchainedAction : public IccSindragosaBossWalkAction
{
public:
    static constexpr char const* Name = "icc sindragosa unchained magic";

    IccSindragosaUnchainedAction(PlayerbotAI* botAI) : IccSindragosaBossWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSindragosaFreedomAction : public Action
{
public:
    static constexpr char const* Name = "icc sindragosa hand of freedom";

    IccSindragosaFreedomAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSindragosaCoverAction : public Action
{
public:
    static constexpr char const* Name = "icc sindragosa holder cover";

    IccSindragosaCoverAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSindragosaBeaconHealAction : public Action
{
public:
    static constexpr char const* Name = "icc sindragosa beacon heal";

    IccSindragosaBeaconHealAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSindragosaHoldAction : public Action
{
public:
    static constexpr char const* Name = "icc sindragosa hold";

    IccSindragosaHoldAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSindragosaTargetAction : public IccDirectAttackAction
{
public:
    static constexpr char const* Name = "icc sindragosa target";

    IccSindragosaTargetAction(PlayerbotAI* botAI) : IccDirectAttackAction(botAI, Name, &GetSgTarget) {}
};

class IccSindragosaStandAction : public IccSindragosaBossWalkAction
{
public:
    static constexpr char const* Name = "icc sindragosa stand";

    IccSindragosaStandAction(PlayerbotAI* botAI) : IccSindragosaBossWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

#endif
