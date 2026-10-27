/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_BLOODQUEEN_H
#define PLAYERBOTS_ICCTRIGGERS_BLOODQUEEN_H

#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Trigger.h"

class IccBqlGroupPositionTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bql group position";

    IccBqlGroupPositionTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccBqlPactOfDarkfallenTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bql pact of darkfallen";

    IccBqlPactOfDarkfallenTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccBqlVampiricBiteTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc bql vampiric bite";

    IccBqlVampiricBiteTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
