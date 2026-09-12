/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_Ignis.h"

#include "AttackAction.h"
#include "BurstCooldowns.h"
#include "ChooseTargetActions.h"
#include "Creature.h"
#include "EncounterHelpers.h"
#include "FollowActions.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "PaladinActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "PriestActions.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "UldActions.h"
#include "UldData.h"
#include "UldEncounter_Ignis.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTriggers.h"
#include "VehicleActions.h"

#include <set>
#include <string>

using namespace EncounterHelpers;

// Ignis the Furnace Master
float IgnisMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != ULDUAR_MAP_ID)
        return 1.0f;

    // Slag Pot is a vehicle ride: the victim is held in place for the full duration, so movement
    // orders only fight the ride and leave the bot facing the wrong way when it drops.
    bool const slagPotRide = dynamic_cast<MovementAction*>(action) && IsIgnisSlagPotVictim(bot);

    // The construct tanks are deliberately parked on a Scorched Ground patch - that is what stacks
    // Heat on the construct - so the generic dodge would undo the kite every tick. The main tank is
    // exempt for the opposite reason: his arc rotation already steps him clear of every patch he
    // drops, and a dodge on top of it would drag Ignis across the room.
    //
    // Matched by type, not getName(): that returns a copy of the name, and this runs on every action
    // of every bot for the whole instance.
    bool const parkedTank = dynamic_cast<IgnisScorchedGroundAction*>(action) &&
                            (GetIgnisConstructTankIndex(botAI, bot) >= 0 || botAI->IsMainTank(bot));

    // GetIgnis walks the grid, and this runs against every action of every bot on the tick, so it is
    // asked last - only once something Ignis-specific would actually be blocked.
    if (!slagPotRide && !parkedTank)
        return 1.0f;

    return GetIgnis(botAI) ? 0.0f : 1.0f;
}

float IgnisTankMovementMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != ULDUAR_MAP_ID)
        return 1.0f;

    // Type first: it rules out almost every action for free, while the role check below walks the
    // group, and this runs on every action of every bot for the whole instance.
    if (!dynamic_cast<ReachTargetAction*>(action) && !dynamic_cast<CastReachTargetSpellAction*>(action) &&
        !dynamic_cast<FollowAction*>(action) && !dynamic_cast<FleeAction*>(action))
    {
        return 1.0f;
    }

    // Only the three roles the encounter places. Everyone else keeps every generic mover, which is
    // also what keeps the ranged half spread and in range without an anchor of their own.
    if (!botAI->IsMainTank(bot) && GetIgnisConstructTankIndex(botAI, bot) < 0)
        return 1.0f;

    return IsIgnisEngaged(botAI) ? 0.0f : 1.0f;
}

float IgnisDisableDefaultTargetingMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != ULDUAR_MAP_ID)
        return 1.0f;

    if (!dynamic_cast<DpsAssistAction*>(action) && !dynamic_cast<TankAssistAction*>(action) &&
        !dynamic_cast<AttackRtiTargetAction*>(action))
    {
        return 1.0f;
    }

    return IsIgnisEngaged(botAI) ? 0.0f : 1.0f;
}

float IgnisFlameJetsHoldCastMultiplier::GetValue(Action* action)
{
    CastSpellAction* spellAction = dynamic_cast<CastSpellAction*>(action);
    if (!spellAction || dynamic_cast<CastMeleeSpellAction*>(action) || bot->GetMapId() != ULDUAR_MAP_ID)
        return 1.0f;

    uint32 const now = getMSTime();
    if (now != cachedAtMs || !cachedAtMs)
    {
        cachedAtMs = now;
        cachedRemainingMs = EvaluateWindow();
    }

    if (cachedRemainingMs <= 0)
        return 1.0f;

    uint32 const spellId = AI_VALUE2(uint32, "spell id", spellAction->getSpell());
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return 1.0f;

    // Anything that still lands before the knockback is worth starting - only the casts Flame Jets
    // would eat are held, so healers keep their short heals through the window.
    uint32 const castTime = spellInfo->CalcCastTime();
    if (castTime == 0 && !spellInfo->IsChanneled())
        return 1.0f;

    return static_cast<int32>(castTime) < cachedRemainingMs ? 1.0f : 0.0f;
}

int32 IgnisFlameJetsHoldCastMultiplier::EvaluateWindow()
{
    Unit* boss = GetIgnisIf(botAI, [](Creature const* ignis) { return IsIgnisFlameJetsCasting(ignis); });
    if (!boss)
        return 0;

    Spell* jets = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);

    return jets ? jets->GetCastTimeRemaining() : 0;
}
