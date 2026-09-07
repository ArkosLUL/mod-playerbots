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

// Mimiron hard mode: the generic "avoid aoe" reacts to Firefighter's fire nodes and does it badly.
// It flees to the damage aura's own 3 yd radius, which in a field whose chains grow in 7 yd steps
// lands the bot on the next node; it takes the movement lock doing so and then returns false, so the
// Mimiron dodge is issued into a held lock; and both route through FleePosition, which refuses for a
// second after any other flee. Measured across two pulls: 642 evaluations, zero wins, 619 accepted
// moves, and seven out of ten Mimiron flame dodges refused behind it.
class MimironAvoidAoeGuardMultiplier : public Multiplier
{
public:
    MimironAvoidAoeGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "mimiron avoid aoe guard") {}
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
