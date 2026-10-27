/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_Saurfang.h"
#include "ChooseTargetActions.h"
#include "DKActions.h"
#include "DruidActions.h"
#include "DruidBearActions.h"
#include "FollowActions.h"
#include "HunterActions.h"
#include "ICCActions_Saurfang.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "PaladinActions.h"
#include "PlayerbotAI.h"
#include "PriestActions.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "WarriorActions.h"

float IccAddsDbsMultiplier::GetValue(Action* action)
{
    // The boss lookup runs only for an action this can zero, since it is asked about every one.
    bool const blocked =
        dynamic_cast<DpsAoeAction*>(action) || dynamic_cast<CastHurricaneAction*>(action) ||
        dynamic_cast<CastVolleyAction*>(action) || dynamic_cast<CastBlizzardAction*>(action) ||
        dynamic_cast<CastStarfallAction*>(action) || dynamic_cast<FanOfKnivesAction*>(action) ||
        dynamic_cast<CastWhirlwindAction*>(action) || dynamic_cast<CastMindSearAction*>(action) ||
        dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<FollowAction*>(action) ||
        dynamic_cast<FleeAction*>(action) || dynamic_cast<CastArmyOfTheDeadAction*>(action) ||
        (dynamic_cast<ReachSpellAction*>(action) && PlayerbotAI::IsRanged(bot));
    bool const tank = PlayerbotAI::IsTank(bot);
    if (!blocked && !tank)
        return 1.0f;

    Unit* boss = AI_VALUE2(Unit*, "find target", "deathbringer saurfang");
    if (!boss)
        return 1.0f;

    if (blocked)
        return 0.0f;

    if (tank)
    {
        Aura* aura = botAI->GetAura("rune of blood", bot);
        if (aura)
        {
            if (dynamic_cast<CastTauntAction*>(action) ||
                dynamic_cast<CastDarkCommandAction*>(action) ||
                dynamic_cast<CastHandOfReckoningAction*>(action) ||
                dynamic_cast<CastGrowlAction*>(action))
                return 0.0f;

            if (dynamic_cast<MovementAction*>(action))
                return 1.0f;

            return 0.0f;
        }
    }

    return 1.0f;
}
