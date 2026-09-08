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
// must not fire while the bot is under orders to dodge something that kills. "reach melee" is the
// same move without the spell and was missed for exactly that reason - it is a ReachTargetAction, not
// a CastReachTargetSpellAction - so a melee bot that had just been thrown 12 yd clear of the fire was
// walked back into it at relevance 21 on the next tick, and spent 11 % of phase 1 in range instead of
// the 23 to 33 % it managed before the dodge worked at all.
class MimironChargeGuardMultiplier : public Multiplier
{
public:
    MimironChargeGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "mimiron charge guard") {}
    float GetValue(Action* action) override;
};

// Mimiron: the generic unstacker and the Mimiron formation both move the same ranged bot, and on
// Firefighter they disagreed by construction - the wedge deals rows 6 yd apart and phase 1 set the
// unstack threshold to the same 6, so every bot that reached its slot was immediately judged too
// close and shoved off it. Accepted unstack moves went from 51 a pull to 261, ranged spent 42 % of
// phase 1 walking instead of 26 %, and output fell 13 to 17 %.
//
// Only while the bot is standing on its slot, which is also the window the formation declines to act
// in: inside the tolerance nothing moves the bot, outside it the formation owns the correction. A bot
// with no slot - melee mid-phase - keeps the unstacker untouched.
class MimironFormationGuardMultiplier : public Multiplier
{
public:
    MimironFormationGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "mimiron formation guard") {}
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
