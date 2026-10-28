/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_MARROWGAR_H
#define PLAYERBOTS_ICCTRIGGERS_MARROWGAR_H

#include "ICCHelpers_Marrowgar.h"
#include "ICCTriggers_RaidWide.h"
#include "Trigger.h"

class IccLmTargetTrigger : public IccDirectTargetTrigger
{
public:
    static constexpr char const* Name = "icc lm target";

    IccLmTargetTrigger(PlayerbotAI* botAI) : IccDirectTargetTrigger(botAI, Name, &GetLmTarget) {}
};

class IccLmStandTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lm stand";

    IccLmStandTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLmColdflameTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lm coldflame";

    IccLmColdflameTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLmTankDefensiveTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lm tank defensive";

    IccLmTankDefensiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLmStormCoverTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lm storm cover";

    IccLmStormCoverTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
