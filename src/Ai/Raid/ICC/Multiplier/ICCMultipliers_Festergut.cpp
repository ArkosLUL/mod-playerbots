/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_Festergut.h"
#include "DKActions.h"
#include "DruidBearActions.h"
#include "FollowActions.h"
#include "HunterActions.h"
#include "ICCActions_Festergut.h"
#include "ICCScripts.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "PaladinActions.h"
#include "PlayerbotAI.h"
#include "PriestActions.h"
#include "ReachTargetActions.h"
#include "WarriorActions.h"

float IccFestergutMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "festergut");
    if (!boss)
        return 1.0f;

    if (dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<FollowAction*>(action))
        return 0.0f;

    if (dynamic_cast<FleeAction*>(action))
        return 0.0f;

    if (dynamic_cast<CastDisengageAction*>(action) || dynamic_cast<CastBlinkBackAction*>(action))
        return 0.0f;

    if (PlayerbotAI::IsTank(bot))
    {
        Aura* aura = botAI->GetAura("gastric bloat", bot, false, true);
        if (aura && aura->GetStackAmount() >= 6)
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

    if (dynamic_cast<IccFestergutSporeAction*>(action))
        return 1.0f;

    if (bot->HasAura(SPELL_GAS_SPORE))
    {
        if (dynamic_cast<MovementAction*>(action) || dynamic_cast<FleeAction*>(action) ||
            dynamic_cast<ReachSpellAction*>(action))
            return 0.0f;
    }

    // Hold position during the 8s malleable goo wait window so the bot can
    // keep DPS/heal rotations running without drifting back into the impact.
    // Avoid action itself is whitelisted (may still need to dodge new goos).
    {
        auto const& waitMap = IcecrownHelpers::IccState(bot->GetMap()->GetInstanceId()).festergutGooWaitUntil;
        auto it = waitMap.find(bot->GetGUID());
        if (it != waitMap.end() && getMSTime() < it->second)
        {
            if (dynamic_cast<IccFestergutAvoidMalleableGooAction*>(action))
                return 1.0f;

            if (dynamic_cast<MovementAction*>(action))
                return 0.0f;
        }
    }

    return 1.0f;
}
