/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OSMultipliers.h"
#include "ChooseTargetActions.h"
#include "DKActions.h"
#include "DruidActions.h"
#include "DruidBearActions.h"
#include "FollowActions.h"
#include "GenericActions.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "OSActions.h"
#include "OSShared.h"
#include "OSTriggers.h"
#include "PaladinActions.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "ScriptedCreature.h"
#include "WarriorActions.h"

float SartharionMultiplier::GetValue(Action* action)
{
    // The off-tank parks every landed drake away from the raid, so redirecting at the main tank -
    // who is standing on Sartharion - is the opposite of what the off-tank logic is for. ForceThreat
    // fixates the drake, so the redirect cannot actually move it; the cost is the wasted cooldown.
    // Hold it for the boss, where the main tank is the right sink. Only these two actions, never the
    // shared BuffOnMainTankAction base - that would also kill Beacon of Light and Earth Shield.
    //
    // Ahead of the boss lookup: "find target" only resolves units that already have this bot on their
    // threat list, and a bot that never got on Sartharion's (rezzed mid-fight, joined late, opened on
    // a drake) is exactly the one most likely to be swinging at a drake.
    if (dynamic_cast<CastMisdirectionOnMainTankAction*>(action) ||
        dynamic_cast<CastTricksOfTheTradeOnMainTankAction*>(action))
    {
        Unit* currentTarget = AI_VALUE(Unit*, "current target");
        if (currentTarget && ObsidianSanctumHelpers::IsDrakeEntry(currentTarget->GetEntry()))
        {
            return 0.0f;
        }
    }

    Unit* boss = AI_VALUE2(Unit*, "find target", "sartharion");
    if (!boss) { return 1.0f; }

    Unit* target = action->GetTarget();

    if (botAI->IsDps(bot) && dynamic_cast<DpsAssistAction*>(action))
    {
        return 0.0f;
    }

    if (botAI->IsMainTank(bot) && target && target != boss &&
        (dynamic_cast<TankAssistAction*>(action) || dynamic_cast<CastTauntAction*>(action) || dynamic_cast<CastDarkCommandAction*>(action) ||
         dynamic_cast<CastHandOfReckoningAction*>(action) || dynamic_cast<CastGrowlAction*>(action)))
    {
        return 0.0f;
    }

    if (botAI->IsAssistTank(bot) && target && target == boss &&
        (dynamic_cast<CastTauntAction*>(action) || dynamic_cast<CastDarkCommandAction*>(action) ||
         dynamic_cast<CastHandOfReckoningAction*>(action) || dynamic_cast<CastGrowlAction*>(action)))
    {
        return 0.0f;
    }
    return 1.0f;
}
