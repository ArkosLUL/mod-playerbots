/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_KOLOGARN_H
#define PLAYERBOTS_ULDMULTIPLIERS_KOLOGARN_H

#include "Define.h"
#include "Multiplier.h"

// Kologarn selects every bot's target in code, per role, so the generic target pickers have to be
// shut out entirely - otherwise "dps target" (which falls back to a smart-target strategy when no
// raid icon is set, so it is never null) fights the role assignment on alternating ticks.
class KologarnDisableAutomaticTargetingMultiplier : public Multiplier
{
public:
    KologarnDisableAutomaticTargetingMultiplier(PlayerbotAI* ai)
        : Multiplier(ai, "kologarn disable automatic targeting")
    {
    }
    float GetValue(Action* action) override;
};

// Stone Grip victims ride the right arm as stunned passengers; movement orders only fight the ride.
class KologarnMultiplier : public Multiplier
{
public:
    KologarnMultiplier(PlayerbotAI* ai) : Multiplier(ai, "kologarn") {}
    float GetValue(Action* action) override;
};

#endif
