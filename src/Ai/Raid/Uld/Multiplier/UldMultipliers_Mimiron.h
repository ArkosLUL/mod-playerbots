/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_MIMIRON_H
#define PLAYERBOTS_ULDMULTIPLIERS_MIMIRON_H

#include "Define.h"
#include "Multiplier.h"

// Mimiron: "mimiron set dps priority" owns every non-tank's target, so the generic picker has to
// stand down or it drags bots back onto whatever is nearest each tick.
class MimironTargetGuardMultiplier : public Multiplier
{
public:
    MimironTargetGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "mimiron target guard") {}
    float GetValue(Action* action) override;
};

// Mimiron: a gap-closer moves the bot in a straight line and consults nothing about the ground, so it
// must not fire while the bot is under orders to dodge something that kills.
class MimironChargeGuardMultiplier : public Multiplier
{
public:
    MimironChargeGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "mimiron charge guard") {}
    float GetValue(Action* action) override;
};

// Mimiron phase 1: Misdirection and Tricks of the Trade both hand their threat to the main tank by
// name, which is the one tank the Plasma Blast swap is trying to move off.
class MimironThreatRedirectGuardMultiplier : public Multiplier
{
public:
    MimironThreatRedirectGuardMultiplier(PlayerbotAI* ai)
        : Multiplier(ai, "mimiron threat redirect guard") {}
    float GetValue(Action* action) override;
};

#endif
