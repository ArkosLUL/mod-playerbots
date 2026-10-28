/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_DOGS_H
#define PLAYERBOTS_ICCTRIGGERS_DOGS_H

#include "ICCHelpers_Dogs.h"
#include "ICCTriggers_RaidWide.h"
#include "Trigger.h"

class IccDogsTankDefensiveTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc dogs tank defensive";

    IccDogsTankDefensiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccDogsDecimateCoverTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc dogs decimate cover";

    IccDogsDecimateCoverTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccDogsSwapTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc dogs swap";

    IccDogsSwapTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccDogsYieldTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc dogs yield";

    IccDogsYieldTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccDogsTargetTrigger : public IccDirectTargetTrigger
{
public:
    static constexpr char const* Name = "icc dogs target";

    IccDogsTargetTrigger(PlayerbotAI* botAI) : IccDirectTargetTrigger(botAI, Name, &GetDogsTarget) {}
};

#endif
