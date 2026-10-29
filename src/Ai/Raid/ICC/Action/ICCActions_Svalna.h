/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_SVALNA_H
#define PLAYERBOTS_ICCACTIONS_SVALNA_H

#include "ICCActions_RaidWide.h"
#include "ICCHelpers_Svalna.h"
#include "MovementActions.h"
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

#endif
