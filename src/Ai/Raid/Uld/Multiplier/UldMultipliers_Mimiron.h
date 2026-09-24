/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_MIMIRON_H
#define PLAYERBOTS_ULDMULTIPLIERS_MIMIRON_H

#include "Define.h"
#include "Multiplier.h"

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
