/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_BloodQueen.h"
#include "HunterActions.h"
#include "ICCActions_BloodQueen.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "PriestActions.h"
#include "ReachTargetActions.h"

float IccBqlMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "blood-queen lana'thel");
    if (!boss)
        return 1.0f;

    Aura* aura2 = botAI->GetAura("Swarming Shadows", bot);
    Aura* aura = botAI->GetAura("Frenzied Bloodthirst", bot);

    if (PlayerbotAI::IsRanged(bot))
        if (dynamic_cast<AvoidAoeAction*>(action) || dynamic_cast<FleeAction*>(action) ||
            dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<CastDisengageAction*>(action))
            return 0.0f;

    // If bot has Pact of Darkfallen aura, return 0 for all other actions
    if (bot->HasAura(SPELL_PACT_OF_THE_DARKFALLEN))
    {
        if (dynamic_cast<IccBqlPactOfDarkfallenAction*>(action))
            return 1.0f; // Allow Pact of Darkfallen action
        else
            return 0.0f; // Cancel all other actions when we need to handle Pact of Darkfallen
    }

    // Air phase: block movement/chase actions, allow combat rotation (attacks/heals)
    if (!aura &&
        (dynamic_cast<AvoidAoeAction*>(action) || dynamic_cast<FleeAction*>(action) ||
         dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<CastDisengageAction*>(action) ||
         dynamic_cast<ReachMeleeAction*>(action) || dynamic_cast<ReachTargetAction*>(action)) &&
        IccBqlPhaseOf(bot, boss) == IccBqlPhase::Air)
        return 0.0f;

    // If bot has frenzied bloodthirst, allow highest priority for bite action
    if (aura) // If bot has frenzied bloodthirst
    {
        if (dynamic_cast<IccBqlVampiricBiteAction*>(action))
            return 1.0f;
        else
            return 0.0f;
    }

    if (aura2 && !aura)
    {
        if (dynamic_cast<IccBqlGroupPositionAction*>(action))
            return 1.0f;
        else
            return 0.0f;  // Cancel all other actions when we need to handle Swarming Shadows
    }

    if ((boss->GetExactDist2d(ICC_BQL_TANK_POSITION.GetPositionX(), ICC_BQL_TANK_POSITION.GetPositionY()) > 10.0f) &&
        PlayerbotAI::IsRanged(bot) && !((boss->GetPositionZ() - bot->GetPositionZ()) > 5.0f))
    {
        if (dynamic_cast<FleeAction*>(action) || dynamic_cast<CombatFormationMoveAction*>(action))
            return 0.0f;
    }

    return 1.0f;
}
