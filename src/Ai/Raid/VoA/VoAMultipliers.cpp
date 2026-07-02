/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "VoAMultipliers.h"
#include "VoATriggers.h"
#include "FollowActions.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "ReachTargetActions.h"

// Koralon the Flame Watcher

float KoralonBurningBreathMultiplier::GetValue(Action* action)
{
    // Only clamp movement while the bot is actually caught in the Burning Breath cone
    KoralonBurningBreathTrigger burningBreathTrigger(botAI);
    if (!burningBreathTrigger.IsActive())
        return 1.0f;

    if (dynamic_cast<CastReachTargetSpellAction*>(action) ||
        dynamic_cast<ReachTargetAction*>(action) ||
        dynamic_cast<CombatFormationMoveAction*>(action) ||
        dynamic_cast<FollowAction*>(action))
        return 0.0f;

    return 1.0f;
}

// Toravon the Ice Watcher

float ToravonAvoidMultiplier::GetValue(Action* action)
{
    // Only clamp movement while the bot actually needs to dodge Freezing Ground or a Frozen Orb
    ToravonFreezingGroundTrigger freezingGroundTrigger(botAI);
    ToravonFrozenOrbAvoidTrigger frozenOrbTrigger(botAI);
    if (!freezingGroundTrigger.IsActive() && !frozenOrbTrigger.IsActive())
        return 1.0f;

    if (dynamic_cast<CastReachTargetSpellAction*>(action) ||
        dynamic_cast<ReachTargetAction*>(action) ||
        dynamic_cast<CombatFormationMoveAction*>(action) ||
        dynamic_cast<FollowAction*>(action))
        return 0.0f;

    return 1.0f;
}
