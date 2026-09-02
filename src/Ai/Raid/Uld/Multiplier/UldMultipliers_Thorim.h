/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_THORIM_H
#define PLAYERBOTS_ULDMULTIPLIERS_THORIM_H

#include "Define.h"
#include "Multiplier.h"

// Thorim: Runic Barrier answers every melee swing with 2000 arcane, so a bot that has backed out on
// the health band must not keep walking back in. Only the swing and the chase are held - the target
// is deliberately kept, and everything that is not a melee hit carries on from out there.
class ThorimRunicBarrierMultiplier : public Multiplier
{
public:
    ThorimRunicBarrierMultiplier(PlayerbotAI* ai) : Multiplier(ai, "thorim runic barrier") {}
    float GetValue(Action* action) override;
};

// Thorim's arena squad has to keep one living body inside the box boss_thorim.cpp scans, or he
// summons the Lightning Orb and the raid dies. Unlike every other guard here this one does NOT exempt
// AttackAction or ReachTargetAction: corridor mobs sit ~92 yd from the arena centre, inside the 100 yd
// sight cap, so the chase is exactly what walks a bot out of the box. The window is "currently
// outside" and closes on re-entry, so it cannot become a permanent freeze.
class ThorimArenaLeashMultiplier : public Multiplier
{
public:
    ThorimArenaLeashMultiplier(PlayerbotAI* ai) : Multiplier(ai, "thorim arena leash") {}
    float GetValue(Action* action) override;
};

// The encounter picks every non-tank target in phase 1, so the generic pickers have to stand down.
// Without this they do not lose - they alternate: the trigger below goes quiet the moment the bot
// already holds the pick, the engine falls through to "dps assist" at 50, and that retargets. Next
// tick the raid node wins it back. A traced arena squad switched target twice a second all fight and
// its melee were in range of their own target 30-46% of the time, against 59-60% in the corridor,
// where the two happen to agree and nothing oscillates.
class ThorimDisableAutomaticTargetingMultiplier : public Multiplier
{
public:
    ThorimDisableAutomaticTargetingMultiplier(PlayerbotAI* ai)
        : Multiplier(ai, "thorim disable automatic targeting")
    {
    }
    float GetValue(Action* action) override;
};

// Without this the leash and the chase take turns: the leash walks the bot back, the picker still
// holds the corridor mob, and it walks straight out again. Zeroing the attack drops the target instead.
class ThorimArenaTargetGuardMultiplier : public Multiplier
{
public:
    ThorimArenaTargetGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "thorim arena target guard") {}
    float GetValue(Action* action) override;
};

// Thorim: the corridor squad once it is up on the platform. Two jobs, one window. It holds Thorim as
// a target from 130 yd out - the generic picker reads threat, not range, so an untouchable boss is
// still in the list - and then "reach melee" walks it up the middle of the hallway into the Paralytic
// Field bunnies. And when nothing else is driving, "follow" walks it the only walkable way down to
// the master, which is 300 yd back through the corridor rather than over the edge.
//
// The movement half is gated on the balcony node being live, so a bot up here always has one mover
// left and cannot be stranded.
class ThorimBalconyGuardMultiplier : public Multiplier
{
public:
    ThorimBalconyGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "thorim balcony guard") {}
    float GetValue(Action* action) override;
};

// Thorim: the phase 1 arena formation, once a bot has reached its spot. Same shape as the phase 2
// guard and for the same reason - the generic movers are what smear the squad east across the arena
// until it is standing in the corridor mouth, where boss_thorim_arena_npcs::CanAIAttack stops seeing
// it and the adds re-roll onto a healer. The window is "settled on the anchor", so a bot that gets
// knocked off it is free to walk back.
class ThorimArenaAnchorGuardMultiplier : public Multiplier
{
public:
    ThorimArenaAnchorGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "thorim arena anchor guard") {}
    float GetValue(Action* action) override;
};

// Thorim: every melee DPS gets the "behind" strategy from AiFactory, so once the phase 2 ring node
// yields, SetBehindTargetAction walks all three stacks back into one arc behind the boss - and Chain
// Lightning jumps 5 yd here, so one arc is one chain. Scoped to a settled ring holder rather than the
// whole phase, because a permanent movement freeze is the Void Reaver failure.
class ThorimMovementGuardMultiplier : public Multiplier
{
public:
    ThorimMovementGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "thorim movement guard") {}
    float GetValue(Action* action) override;
};

#endif
