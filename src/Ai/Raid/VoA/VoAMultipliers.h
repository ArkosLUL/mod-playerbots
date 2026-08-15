/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef PLAYERBOTS_VOAMULTIPLIERS_H
#define PLAYERBOTS_VOAMULTIPLIERS_H

#include "Multiplier.h"

class Unit;

// Emalon the Storm Watcher

// While a non-tank must run out of Emalon's Lightning Nova PBAoE, suppress the movement/reach actions that
// would pull the bot back toward the boss, so the run-out (emalon lighting nova action) wins.
class EmalonLightningNovaMultiplier : public Multiplier
{
public:
    EmalonLightningNovaMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "emalon lightning nova multiplier") {}
    virtual float GetValue(Action* action);

private:
    // Answering the trigger costs a creature grid sweep, and a multiplier runs once per queued action,
    // so the answer is resolved once per tick and reused.
    bool NovaLive();

    uint32 cachedAtMs = 0;
    bool cachedNova = false;
};

// Hands each role's position and target to the Emalon nodes and keeps the generic engine off them.
class EmalonPositioningMultiplier : public Multiplier
{
public:
    EmalonPositioningMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "emalon positioning multiplier") {}
    float GetValue(Action* action) override;

private:
    // Resolving the boss and the minion list needs a grid sweep, and a multiplier runs once per queued
    // action, so the whole picture is resolved once per tick and reused.
    struct TickState
    {
        Unit* boss = nullptr;
        bool encounterActive = false;
        // Role lookups walk the group, and this runs once per queued action, so they are resolved
        // with the rest of the picture rather than a few dozen times a tick.
        bool mainTank = false;
        bool offTank = false;
        bool ringBot = false;
        bool dps = false;
    };

    TickState const& Snapshot();

    uint32 cachedAtMs = 0;
    TickState cached;
};

// Koralon the Flame Watcher

// While a non-tank stands in Koralon's Burning Breath cone, suppress the movement/reach actions that would
// pull the bot back into the cone, so the dodge (koralon burning breath action) wins.
class KoralonBurningBreathMultiplier : public Multiplier
{
public:
    KoralonBurningBreathMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "koralon burning breath multiplier") {}
    virtual float GetValue(Action* action);
};

// Toravon the Ice Watcher

// While a bot must dodge Freezing Ground or a Frozen Orb, suppress the movement/reach actions that would
// pull it back toward the boss, so the avoidance action wins.
class ToravonAvoidMultiplier : public Multiplier
{
public:
    ToravonAvoidMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "toravon avoid multiplier") {}
    virtual float GetValue(Action* action);
};

#endif
