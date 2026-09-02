/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_FREYA_H
#define PLAYERBOTS_ULDMULTIPLIERS_FREYA_H

#include "Define.h"
#include "Multiplier.h"

// Freya picks every DPS bot's target in code so the trio wave can be split three ways. Without this
// the generic picker reclaims those targets on alternating ticks and the split never holds. The main
// tank is also fenced off generic tank assist, which would otherwise walk it off Freya onto a
// loose Storm Lasher.
class FreyaDisableAutomaticTargetingMultiplier : public Multiplier
{
public:
    FreyaDisableAutomaticTargetingMultiplier(PlayerbotAI* ai)
        : Multiplier(ai, "freya disable automatic targeting")
    {
    }
    float GetValue(Action* action) override;
};

// Backstop for the trio sync: the target split normally steers bots off a member that is too far
// ahead, so this only catches damage the targeting cannot steer - a swing mid-animation, a DoT
// already ticking.
class FreyaTrioSyncMultiplier : public Multiplier
{
public:
    FreyaTrioSyncMultiplier(PlayerbotAI* ai) : Multiplier(ai, "freya trio sync") {}
    float GetValue(Action* action) override;
};

// Freya: the lasher pack is AoE'd down together right up to the finish, and then the AoE has to stop
// or the whole pile detonates at once. This is the "stop at low health" half of that - single-target
// damage keeps running, which is what staggers the blasts.
class FreyaLasherFinishAoeMultiplier : public Multiplier
{
public:
    FreyaLasherFinishAoeMultiplier(PlayerbotAI* ai) : Multiplier(ai, "freya lasher finish aoe") {}
    float GetValue(Action* action) override;
};

// Freya: the spread node only owns the tick until the bot reaches its slot - it returns false once it
// is standing on one, and the generic chase underneath then walks it straight back to whatever it is
// attacking, which on a lasher wave is usually Freya on the far side of the room. Holding the slot is
// the half of the spread that actually keeps the raid apart. Heals are exempt: the main tank has no
// slot and stands wherever the pull left the boss, so a healer that cannot step toward it loses it.
class FreyaLasherSpreadHoldMultiplier : public Multiplier
{
public:
    FreyaLasherSpreadHoldMultiplier(PlayerbotAI* ai) : Multiplier(ai, "freya lasher spread hold") {}
    float GetValue(Action* action) override;
};

// Freya: a hunter holds one trap at a time and they share a 30s category cooldown, so the generic
// Explosive Trap node is what stops Frost Trap ever going down. Damage is not what the wave needs;
// -50% movement on something that outruns the raid at 8.0 yd/s is.
class FreyaLasherTrapReserveMultiplier : public Multiplier
{
public:
    FreyaLasherTrapReserveMultiplier(PlayerbotAI* ai) : Multiplier(ai, "freya lasher trap reserve") {}
    float GetValue(Action* action) override;
};

// Freya hard mode: Ground Tremor interrupts the whole raid on a 2s telegraph and school-locks whoever
// it cuts for 10s, so a cast that cannot land inside the telegraph is thrown away. Heals are held too,
// unlike at Ignis: a Chain Heal delayed under 2s beats a resto shaman with no Nature for 10.
class FreyaGroundTremorCastGateMultiplier : public Multiplier
{
public:
    FreyaGroundTremorCastGateMultiplier(PlayerbotAI* ai) : Multiplier(ai, "freya ground tremor cast gate") {}
    float GetValue(Action* action) override;

private:
    // Runs against every candidate action, so the boss lookup is memoised for the rest of the tick.
    int32 EvaluateWindow();

    uint32 cachedAtMs = 0;
    int32 cachedRemainingMs = 0;
};

#endif
