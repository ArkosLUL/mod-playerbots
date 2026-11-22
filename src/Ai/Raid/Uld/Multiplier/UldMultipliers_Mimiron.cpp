/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_Mimiron.h"

#include "AttackAction.h"
#include "BurstCooldowns.h"
#include "ChooseTargetActions.h"
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
#include "RaidSplash.h"
#include "RaidTankDefensive.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "UldActions.h"
#include "UldEncounter_Mimiron.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTargets.h"
#include "UldTriggers.h"
#include "VehicleActions.h"

#include <set>
#include <string>

using namespace EncounterHelpers;

float MimironTankAnchorGuardMultiplier::GetValue(Action* action)
{
    if (!action || !PlayerbotAI::IsMainTank(bot))
        return 1.0f;

    bool const reachMelee = action->getName() == "reach melee";
    if (!dynamic_cast<TankFaceAction*>(action) && !reachMelee)
        return 1.0f;

    if (reachMelee && IsMimironAcuAirborne(botAI, bot))
        return 0.0f;

    if (!IsMimironHardModeActive(botAI) || !MimironPhase1Active(botAI))
        return 1.0f;

    Unit* leviathanMkII = GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII);
    return leviathanMkII && leviathanMkII->GetVictim() == bot &&
                   IsMimironTankDragReady(botAI, bot)
               ? 0.0f
               : 1.0f;
}

float MimironFireBotAoeGuardMultiplier::GetValue(Action* action)
{
    if (!action || !SpellSplashes(botAI, action))
        return 1.0f;

    std::vector<ObjectGuid> const kept = GetMimironKeptFireBots(botAI, bot);
    if (kept.empty())
        return 1.0f;

    Unit* target = AI_VALUE(Unit*, "current target");
    for (ObjectGuid const& guid : kept)
    {
        Unit* fireBot = botAI->GetUnit(guid);
        if (!fireBot || !fireBot->IsAlive())
            continue;

        if (bot->GetExactDist2d(fireBot) < ULDUAR_MIMIRON_FIREBOT_AOE_CLEARANCE ||
            (target && target->GetExactDist2d(fireBot) < ULDUAR_MIMIRON_FIREBOT_AOE_CLEARANCE))
            return 0.0f;
    }

    return 1.0f;
}

float MimironPlasmaDefensiveHoldMultiplier::GetValue(Action* action)
{
    if (!action || !PlayerbotAI::IsMainTank(bot) || !IsHeldTankDefensive(action->getName()))
        return 1.0f;

    return IsMimironEngaged(botAI) && MimironPhase1Active(botAI) ? 0.0f : 1.0f;
}
