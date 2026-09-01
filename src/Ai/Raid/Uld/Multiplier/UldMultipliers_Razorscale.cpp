/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_Razorscale.h"

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
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "UldActions.h"
#include "UldData.h"
#include "UldEncounter_Razorscale.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTriggers.h"
#include "VehicleActions.h"

#include <set>
#include <string>

using namespace EncounterHelpers;

namespace
{

// Everything generic that can walk a bot back onto a patch it has just cleared. "avoid aoe" is on the
// list because it sits at ACTION_EMERGENCY, outranks every Razorscale node, and picks its bearing
// with no knowledge of the other patches on the floor. CastReachTargetSpellAction is a CastSpellAction
// rather than a MovementAction, so it cannot be caught by a base-class filter.
bool IsRazorscaleGenericMover(Action* action)
{
    return dynamic_cast<ReachTargetAction*>(action) || dynamic_cast<CastReachTargetSpellAction*>(action) ||
           dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<RearFlankAction*>(action) ||
           dynamic_cast<FollowAction*>(action) || dynamic_cast<AvoidAoeAction*>(action);
}

}

float RazorscaleMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != ULDUAR_MAP_ID)
        return 1.0f;

    if (!IsRazorscaleGenericMover(action))
        return 1.0f;

    // Asked last, because it is the expensive half.
    uint32 const now = getMSTime();
    if (now != cachedAtMs || !cachedAtMs)
    {
        cachedAtMs = now;
        cachedBlocked = MoversBlocked();
    }

    return cachedBlocked ? 0.0f : 1.0f;
}

bool RazorscaleMultiplier::MoversBlocked()
{
    // Scoped to a live dodge only: held permanently this is the freeze bug, where a silently-failing
    // MoveTo strands the bot for the rest of the fight.
    if (RazorscaleBossHelper::FindDevouringFlameNear(botAI, RazorscaleBossHelper::DEVOURING_FLAME_CLEAR_RADIUS))
        return true;

    // Standing clear, but the spot the movers would walk to is on fire. Ranged never has to close, and
    // holding them here would fight the grounded stack-up for no gain.
    if (!botAI->IsMelee(bot))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    return RazorscaleBossHelper::DevouringFlameBlocks(bot, target->GetPositionX(), target->GetPositionY());
}
