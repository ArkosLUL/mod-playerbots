/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#ifndef PLAYERBOTS_VOAMULTIPLIERS_H
#define PLAYERBOTS_VOAMULTIPLIERS_H

#include "Multiplier.h"

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
