/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_HODIR_H
#define PLAYERBOTS_ULDMULTIPLIERS_HODIR_H

#include "Multiplier.h"

#include <limits>
#include <unordered_map>

class HodirBreakerAoeHoldMultiplier : public Multiplier
{
public:
    HodirBreakerAoeHoldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "hodir breaker aoe hold multiplier") {}

    float GetValue(Action* action) override;
};

class HodirStormGatherCastHoldMultiplier : public Multiplier
{
public:
    HodirStormGatherCastHoldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "hodir storm gather cast hold") {}

    float GetValue(Action* action) override;

private:
    std::unordered_map<Action*, uint32> spellIds;
    uint32 cachedAtMs = 0;
    int32 cachedSlackMs = std::numeric_limits<int32>::max();
};

#endif
