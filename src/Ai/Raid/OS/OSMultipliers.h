/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_OSMULTIPLIERS_H
#define PLAYERBOTS_OSMULTIPLIERS_H

#include "Multiplier.h"

class Unit;

class SartharionMultiplier : public Multiplier
{
public:
    SartharionMultiplier(PlayerbotAI* ai) : Multiplier(ai, "sartharion") {}

    float GetValue(Action* action) override;

private:
    // Everything here needs a grid sweep, and a multiplier runs once per queued action, so the whole
    // encounter picture is resolved once per tick and reused.
    struct TickState
    {
        Unit* boss = nullptr;
        bool encounterActive = false;
        bool dodgeLive = false;
    };

    TickState const& Snapshot();

    uint32 cachedAtMs = 0;
    TickState cached;
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
