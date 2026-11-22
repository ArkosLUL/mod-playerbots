/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_FREYA_H
#define PLAYERBOTS_ULDMULTIPLIERS_FREYA_H

#include "Define.h"
#include "Multiplier.h"
#include "ObjectGuid.h"
#include "RaidSplash.h"

// Nothing lands on a held Snaplasher: single target at it, or area damage near the caster or its target. Every
// direct hit renews Hardened Bark, so one stray Blizzard keeps the stacks a tank would die to. Once the bark is gone a
// tank may go at it, since only a tank taking it ends the hold.
class FreyaSnaplasherHoldMultiplier : public Multiplier
{
public:
    FreyaSnaplasherHoldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "freya snaplasher hold") {}
    float GetValue(Action* action) override;

private:
    // Asked for every candidate action, so the hold is read once a tick.
    uint32 cachedAtMs = 0;
    ObjectGuid cachedHeld;
    bool cachedFree = false;
    RaidSplashCache splash;
};

// Backstop for the trio sync: the target split already steers bots off a member that's too far ahead,
// so this only catches what targeting can't, like a swing mid-animation or a DoT already ticking.
class FreyaTrioSyncMultiplier : public Multiplier
{
public:
    FreyaTrioSyncMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "freya trio sync") {}
    float GetValue(Action* action) override;
};

// Stops AoE on the lasher pile at the finish, or it all detonates at once. Single-target keeps running,
// which is what staggers the blasts.
class FreyaLasherFinishAoeMultiplier : public Multiplier
{
public:
    FreyaLasherFinishAoeMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "freya lasher finish aoe") {}
    float GetValue(Action* action) override;
};

// A hunter has one trap down at a time on a shared 30s category cooldown, so the generic Explosive Trap
// would spend the Frost Trap. The wave needs -50% speed on lashers that outrun the raid, not damage.
class FreyaLasherTrapReserveMultiplier : public Multiplier
{
public:
    FreyaLasherTrapReserveMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "freya lasher trap reserve") {}
    float GetValue(Action* action) override;
};

// Army of the Dead costs one rune of each kind, and CanCastSpell doesn't check runes, so the DK whose turn it
// is kept trying, failing, and spending each rune on its rotation as it came back. Holds rune spenders until
// Army is out; runic power spells still run.
class FreyaArmyRuneReserveMultiplier : public Multiplier
{
public:
    FreyaArmyRuneReserveMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "freya army rune reserve") {}
    float GetValue(Action* action) override;

private:
    // The Army turn walks the group, so it's worked out once a tick, not once per candidate action.
    bool ReserveThisTick();

    uint32 cachedAtMs = 0;
    bool cachedReserve = false;
};

// Hard mode: Ground Tremor interrupts the raid on a 2s telegraph and school-locks whoever it cuts for
// 10s. Heals are held too, unlike Ignis: a Chain Heal held under 2s beats no Nature for 10.
class FreyaGroundTremorCastGateMultiplier : public Multiplier
{
public:
    FreyaGroundTremorCastGateMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "freya ground tremor cast gate") {}
    float GetValue(Action* action) override;

private:
    // Runs against every candidate action, so the boss lookup is memoised for the rest of the tick.
    int32 EvaluateWindow();

    uint32 cachedAtMs = 0;
    int32 cachedRemainingMs = 0;
};

#endif
