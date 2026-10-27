/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_PUTRICIDE_H
#define PLAYERBOTS_ICCTRIGGERS_PUTRICIDE_H

#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Trigger.h"

class IccPutricideVolatileOozeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc putricide volatile ooze";

    IccPutricideVolatileOozeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccPutricideGasCloudTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc putricide gas cloud";

    IccPutricideGasCloudTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccPutricideGrowingOozePuddleTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc putricide growing ooze puddle";

    IccPutricideGrowingOozePuddleTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccPutricideMutatedPlagueTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc putricide mutated plague";

    IccPutricideMutatedPlagueTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccPutricideMalleableGooTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc putricide malleable goo";

    IccPutricideMalleableGooTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccPutricideAbominationTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc putricide abomination";

    IccPutricideAbominationTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
