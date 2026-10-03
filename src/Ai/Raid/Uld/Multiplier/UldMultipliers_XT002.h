/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_XT002_H
#define PLAYERBOTS_ULDMULTIPLIERS_XT002_H

#include "Define.h"
#include "Multiplier.h"

// XT-002: holds the offensive burst cooldowns until the fight's real damage window, which differs
// per mode - the first exposed Heart in hard mode, the sub-25% push in normal mode. Bloodlust is
// decided separately because it is a 10 minute cooldown and the fight is shorter than that, so its
// one use has to land on the longest phase rather than the first one.
class XT002BurstWindowMultiplier : public Multiplier
{
public:
    XT002BurstWindowMultiplier(PlayerbotAI* ai) : Multiplier(ai, "xt002 burst window") {}
    float GetValue(Action* action) override;

private:
    struct Window
    {
        bool allowAll;
        bool allowLust;
    };

    // Runs against every candidate action, so the encounter sweep is memoised for the rest of the tick.
    Window EvaluateWindow();

    uint32 cachedAtMs = 0;
    Window cachedValue = {true, true};
};

// XT-002 normal mode: the safety floor that keeps bots from killing the exposed Heart and flipping
// the raid into hard mode. Also stands down the generic DPS and tank targeting, which the encounter's
// own priority action replaces, the generic movers for anyone pinned in place - anchored ranged DPS
// and debuff carriers - a carrier's gap closers, which move it without being movers, and the
// class-generic threat redirects that would aim at the wrong tank.
class XT002TargetGuardMultiplier : public Multiplier
{
public:
    XT002TargetGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "xt002 target guard") {}
    float GetValue(Action* action) override;
};

#endif
