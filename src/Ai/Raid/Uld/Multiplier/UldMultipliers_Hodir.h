/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_HODIR_H
#define PLAYERBOTS_ULDMULTIPLIERS_HODIR_H

#include "Multiplier.h"

class HodirBreakerAoeHoldMultiplier : public Multiplier
{
public:
    HodirBreakerAoeHoldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "hodir breaker aoe hold multiplier") {}

    float GetValue(Action* action) override;
};

#endif
