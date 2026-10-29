/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_SVALNA_H
#define PLAYERBOTS_ICCTRIGGERS_SVALNA_H

#include "ICCHelpers_Svalna.h"
#include "ICCTriggers_RaidWide.h"
#include "Trigger.h"

class IccSvalnaSpearTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc valkyre spear";

    IccSvalnaSpearTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSvalnaHurlTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sister svalna hurl";

    IccSvalnaHurlTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSvalnaTauntTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc sister svalna taunt";

    IccSvalnaTauntTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccSvalnaTargetTrigger : public IccDirectTargetTrigger
{
public:
    static constexpr char const* Name = "icc sister svalna target";

    IccSvalnaTargetTrigger(PlayerbotAI* botAI) : IccDirectTargetTrigger(botAI, Name, &GetSsTarget) {}
};

#endif
