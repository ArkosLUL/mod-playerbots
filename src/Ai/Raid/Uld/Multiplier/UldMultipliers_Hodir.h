/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_HODIR_H
#define PLAYERBOTS_ULDMULTIPLIERS_HODIR_H

#include "Multiplier.h"

// No AoE while the bot's target is an ice block. A wave puts up eight of them, so the AoE rules fire
// on the count and the breaker spends its cast on everything but its own block: one channelled
// Blizzard through a whole 9s mage block and never touched it.
class HodirBreakerAoeHoldMultiplier : public Multiplier
{
public:
    HodirBreakerAoeHoldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "hodir breaker aoe hold multiplier") {}

    float GetValue(Action* action) override;
};

#endif
