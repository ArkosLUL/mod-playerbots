/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "VoAMultipliers.h"
#include "VoATriggers.h"
#include "ChooseTargetActions.h"
#include "DKActions.h"
#include "DruidBearActions.h"
#include "FollowActions.h"
#include "HunterActions.h"
#include "MovementActions.h"
#include "PaladinActions.h"
#include "Playerbots.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "VoAHelpers.h"
#include "WarriorActions.h"

using namespace VoaHelpers;

// Emalon the Storm Watcher

bool EmalonLightningNovaMultiplier::NovaLive()
{
    uint32 const now = getMSTime();
    if (now != cachedAtMs || !cachedAtMs)
    {
        cachedAtMs = now;
        EmalonLightingNovaTrigger lightningNovaTrigger(botAI);
        cachedNova = lightningNovaTrigger.IsActive();
    }
    return cachedNova;
}

float EmalonLightningNovaMultiplier::GetValue(Action* action)
{
    // Only clamp movement while the bot actually needs to run out of the Lightning Nova PBAoE
    if (!NovaLive())
        return 1.0f;

    if (dynamic_cast<CastReachTargetSpellAction*>(action) ||
        dynamic_cast<ReachTargetAction*>(action) ||
        dynamic_cast<CombatFormationMoveAction*>(action) ||
        dynamic_cast<FollowAction*>(action))
        return 0.0f;

    return 1.0f;
}

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

// Emalon the Storm Watcher, positioning

EmalonPositioningMultiplier::TickState const& EmalonPositioningMultiplier::Snapshot()
{
    uint32 const now = getMSTime();
    if (now == cachedAtMs && cachedAtMs)
        return cached;

    cachedAtMs = now;
    cached = TickState();
    cached.encounterActive = EmalonEncounterActive(bot);
    if (!cached.encounterActive)
        return cached;

    cached.boss = GetEmalon(bot);
    cached.mainTank = botAI->IsMainTank(bot);
    cached.offTank = IsOffTank(bot);
    cached.dps = botAI->IsDps(bot);
    cached.ringBot = !botAI->IsTank(bot) && (botAI->IsHeal(bot) || botAI->IsRanged(bot));

    return cached;
}

float EmalonPositioningMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    TickState const& state = Snapshot();
    if (!state.encounterActive)
        return 1.0f;

    Unit* boss = state.boss;
    Unit* target = action->GetTarget();

    // Closing on a party member to heal has to stay live. It is the one reach action whose target is
    // friendly, which is what the hostile-target test below relies on to let it through.
    if (dynamic_cast<ReachPartyMemberToHealAction*>(action))
        return 1.0f;

    // "emalon attack priority" owns target selection for the whole fight, not only inside the
    // overcharge window: a partial scope is what leaves a bot flipping between the boss and a fresh
    // minion every tick without ever landing a cast.
    if (state.dps && (dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action)))
        return 0.0f;

    // Ranged and healers own a ring slot. The formation movers go for the whole fight, and the reach
    // movers go whenever the thing they would walk to is in this encounter - the boss, because that
    // walks them inside the nova, and a minion, because the far side of the ring is 50yd from the
    // off-tank camp and would empty half the ring into it.
    if (state.ringBot)
    {
        if (dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<FollowAction*>(action))
            return 0.0f;

        // The boss test needs him resolved: a null boss against a null target would compare equal and
        // suppress the movers on every action that has no target at all.
        if (((boss && target == boss) || IsTempestMinion(target)) &&
            (dynamic_cast<ReachTargetAction*>(action) || dynamic_cast<CastReachTargetSpellAction*>(action)))
        {
            return 0.0f;
        }
    }

    if (state.mainTank && boss && target && target != boss &&
        (dynamic_cast<TankAssistAction*>(action) || dynamic_cast<CastTauntAction*>(action) ||
         dynamic_cast<CastDarkCommandAction*>(action) || dynamic_cast<CastHandOfReckoningAction*>(action) ||
         dynamic_cast<CastGrowlAction*>(action)))
    {
        return 0.0f;
    }

    // Once the main tank is on the boss nothing else may move him. "emalon main tank hold" releases
    // the tick as soon as it is inside its tolerance, the reach and formation movers then nudge him
    // out of it, and the hold drags him back next tick - that is the oscillation. Gated on melee range
    // so the pull itself still closes normally.
    if (state.mainTank && boss && bot->IsWithinMeleeRange(boss) &&
        (dynamic_cast<ReachTargetAction*>(action) || dynamic_cast<CastReachTargetSpellAction*>(action) ||
         dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<FollowAction*>(action)))
    {
        return 0.0f;
    }

    if (state.offTank)
    {
        // Emalon belongs to the main tank alone: two tanks trading aggro walk him through the ring.
        // The concrete target-picking classes are named one by one on purpose - EmalonOffTankHoldAction
        // is itself an AttackAction and GetTarget() reports the current target, so casting to the base
        // would zero the one action that can switch the off-tank away and strand him on the boss.
        if (boss && target == boss &&
            (dynamic_cast<CastTauntAction*>(action) || dynamic_cast<CastDarkCommandAction*>(action) ||
             dynamic_cast<CastHandOfReckoningAction*>(action) || dynamic_cast<CastGrowlAction*>(action) ||
             dynamic_cast<TankAssistAction*>(action) || dynamic_cast<AggressiveTargetAction*>(action) ||
             dynamic_cast<AttackAnythingAction*>(action)))
        {
            return 0.0f;
        }

        // "emalon offtank hold" already walks at anything outside taunt range and holds the camp
        // otherwise, so the generic movers can only drag him off it.
        if (IsTempestMinion(target) &&
            (dynamic_cast<ReachTargetAction*>(action) || dynamic_cast<CastReachTargetSpellAction*>(action) ||
             dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<FollowAction*>(action)))
        {
            return 0.0f;
        }
    }

    // The generic node only ever knows the main tank, and here every redirect belongs to the off-tank.
    // Cast on the concrete actions, never on the shared BuffOnMainTankAction base, which also carries
    // Beacon, Earth Shield, Thorns and Lifebloom.
    if (dynamic_cast<CastTricksOfTheTradeOnMainTankAction*>(action) ||
        dynamic_cast<CastMisdirectionOnMainTankAction*>(action))
    {
        return 0.0f;
    }

    return 1.0f;
}
