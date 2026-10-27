/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_Dogs.h"
#include "DKActions.h"
#include "DruidBearActions.h"
#include "FollowActions.h"
#include "ICCActions_Dogs.h"
#include "MovementActions.h"
#include "PaladinActions.h"
#include "PlayerbotAI.h"
#include "PriestActions.h"
#include "WarriorActions.h"

float IccDogsMultiplier::GetValue(Action* action)
{
    if (!AI_VALUE2(Unit*, "find target", "stinky") && !AI_VALUE2(Unit*, "find target", "precious"))
        return 1.0f;

    if (PlayerbotAI::IsTank(bot))
    {
        Aura* aura = botAI->GetAura("mortal wound", bot, false, true);
        if (aura && aura->GetStackAmount() >= 8)
        {
            if (dynamic_cast<MovementAction*>(action))
                return 1.0f;

            if (dynamic_cast<CastTauntAction*>(action) ||
                dynamic_cast<CastDarkCommandAction*>(action) ||
                dynamic_cast<CastHandOfReckoningAction*>(action) ||
                dynamic_cast<CastGrowlAction*>(action))
                return 0.0f;

            return 0.0f;
        }
    }

    if (dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<FollowAction*>(action))
        return 0.0f;

    return 1.0f;
}
