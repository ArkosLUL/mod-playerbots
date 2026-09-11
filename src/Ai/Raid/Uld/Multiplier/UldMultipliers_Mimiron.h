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

// Mimiron phase 1: the rogue's own Tricks target picker hands the buff to the hardest-hitting melee
// during the opener, because its "am I about to pull" test is false for a rogue who has not swung
// yet. That bot then pulls the MK II off the tank 2 to 3 s later - measured in all three of one
// night's pulls. "mimiron redirect threat action" owns the redirect here instead.
class MimironGenericRedirectGuardMultiplier : public Multiplier
{
public:
    MimironGenericRedirectGuardMultiplier(PlayerbotAI* ai)
        : Multiplier(ai, "mimiron generic redirect guard") {}
    float GetValue(Action* action) override;
};

// Mimiron hard mode: eating or drinking stops the bot thinking for up to 18 s, so one that sits down
// near the fire does not dodge it. Clean ground only, because the top-up between phases is worth
// keeping: healers gained 30 to 40 % mana across one handover.
class MimironDrinkGuardMultiplier : public Multiplier
{
public:
    MimironDrinkGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "mimiron drink guard") {}
    float GetValue(Action* action) override;
};

// Mimiron phase 1: the main tank's anchor is 53 yd from where he picks the boss up, and "reach
// melee" is ACTION_HIGH against a formation at ACTION_RAID, so the two traded him back and forth
// for the whole walk. Held only while the boss is his, which is also when he does not need it - it
// is following him. Lose aggro and this lifts, so he can run back and taunt.
//
// Tank face too. It steps him about 5.6 yd off the anchor, just past the formation's tolerance, and
// the formation walks him back, over and over. The MK II has no frontal ability, so pointing it away
// from the raid buys nothing.
//
// Phase 3 as well, for "reach melee" only: the Aerial Command Unit is out of reach in the air, and
// the tank chasing it under itself drifted the whole fight 14 yd off the centre spot.
class MimironTankAnchorGuardMultiplier : public Multiplier
{
public:
    MimironTankAnchorGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "mimiron tank anchor guard") {}
    float GetValue(Action* action) override;
};

// Mimiron hard mode: the Frost Bomb flee stops at its clearance and the trigger lets go just inside
// it, so any reach move then walks the bot back into the blast, and it ping-pongs on the edge for the
// whole 10 s fuse. A healer can be out of range of a far tank for that long; the blast is 47k against
// a pool half that.
class MimironFrostBombGuardMultiplier : public Multiplier
{
public:
    MimironFrostBombGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "mimiron frost bomb guard") {}
    float GetValue(Action* action) override;
};

// Mimiron hard mode, phase 3: bots cannot aim AoE away from a unit, so damage AoE is held near the
// Emergency Fire Bots the raid is keeping alive to put the fire out. Heals are left alone.
class MimironFireBotAoeGuardMultiplier : public Multiplier
{
public:
    MimironFireBotAoeGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "mimiron fire bot aoe guard") {}
    float GetValue(Action* action) override;
};

// Mimiron phase 1: the main tank's own cooldowns go out only through the Plasma Blast window claim.
// The class nodes fire on health, so they spent Divine Protection mid window after a healer external
// had already covered it, and the next window got nothing and killed the tank.
class MimironPlasmaDefensiveHoldMultiplier : public Multiplier
{
public:
    MimironPlasmaDefensiveHoldMultiplier(PlayerbotAI* ai)
        : Multiplier(ai, "mimiron plasma defensive hold") {}
    float GetValue(Action* action) override;
};

#endif
