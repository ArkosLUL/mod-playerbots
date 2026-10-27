/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERS_DEATHWHISPER_H
#define PLAYERBOTS_ICCTRIGGERS_DEATHWHISPER_H

#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Trigger.h"

class IccDarkReckoningTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc dark reckoning";

    IccDarkReckoningTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IccLadyDeathwhisperTrigger : public Trigger
{
public:
    static constexpr char const* Name = "icc lady deathwhisper";

    IccLadyDeathwhisperTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
