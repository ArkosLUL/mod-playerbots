/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_OSMULTIPLIERS_H
#define PLAYERBOTS_OSMULTIPLIERS_H

#include "Multiplier.h"

// The parts of the "sartharion" guard that depend on the action's target, which no rule sees. Each is
// wrapped to the families it looks at, so GetValue only tests role and target.

// Taunts and tank assist, for the main tank: nothing but Sartharion.
class SartharionMainTankTargetMultiplier : public Multiplier
{
public:
    SartharionMainTankTargetMultiplier(PlayerbotAI* ai) : Multiplier(ai, "sartharion") {}
    float GetValue(Action* action) override;
};

// Sartharion belongs to the main tank alone: two tanks trading aggro spin him through the raid. So the
// off-tank is locked off him for the whole fight, not merely off the taunts. Only the concrete target
// pickers count, never AttackAction itself - OsOffTankHoldAction is one and GetTarget() reports the
// bot's current target, so the base would zero the one action that can switch the off-tank away and
// strand it on the boss.
class SartharionOffTankBossMultiplier : public Multiplier
{
public:
    SartharionOffTankBossMultiplier(PlayerbotAI* ai) : Multiplier(ai, "sartharion") {}
    float GetValue(Action* action) override;
};

// The generic melee arc is wrong against both: Sartharion's rear is Tail Lash, and a drake's rear is
// free ground the 90-120 degree band stops short of. "os sartharion flank" and "os drake rear" replace
// it. It has to be zeroed rather than left to lose on priority - the two release the tick once the bot
// is in position, and this would then walk it back out. Lava Blazes and whelps keep it.
class SartharionRearFlankMultiplier : public Multiplier
{
public:
    SartharionRearFlankMultiplier(PlayerbotAI* ai) : Multiplier(ai, "sartharion") {}
    float GetValue(Action* action) override;
};

// Sartharion is the one target ranged, healers and the off-tank never walk to. He parks 36.8yd from the
// raid's home hold and 50.5yd from its left one, both outside their 28.5yd spell range, and closing that
// gap means walking in behind him into a 30yd Tail Lash. The off-tank is on the list for a different
// reason: he is locked off the boss for the whole fight, and the pull leaves the boss in his "current
// target" before this strategy is live, which is enough for a reach mover to carry him into melee.
class SartharionBossReachMultiplier : public Multiplier
{
public:
    SartharionBossReachMultiplier(PlayerbotAI* ai) : Multiplier(ai, "sartharion") {}
    float GetValue(Action* action) override;
};

// Holds every offensive throughput cooldown until the raid commits, which is Tenebron at half health.
// The 30% enrage stays as a backstop for a run that somehow gets there without it. There is no hard
// enrage to race - the 15 minute berserk is scheduled into extraEvents but handled in the events
// switch, so it never fires in this build.
//
// This only permits. The shared "burst" gate still has to see a tank holding the target, and the
// drakes count as targets it cares about: instance_encounters gives them
// CREATURE_FLAG_EXTRA_DUNGEON_BOSS.
class SartharionBurstWindowMultiplier : public Multiplier
{
public:
    SartharionBurstWindowMultiplier(PlayerbotAI* ai) : Multiplier(ai, "sartharion burst window") {}
    float GetValue(Action* action) override;

private:
    // Sweeps for Tenebron, so it is cached for the rest of the tick rather than re-run per action.
    float EvaluateWindow();

    // boss_sartharion.cpp applies Berserk from the DamageTaken hook rather than on a timer, so the
    // health check is the fallback for the tick before the aura lands.
    static constexpr float SARTHARION_ENRAGE_PCT = 30.0f;

    uint32 cachedAtMs = 0;
    float cachedValue = 1.0f;
};

#endif
