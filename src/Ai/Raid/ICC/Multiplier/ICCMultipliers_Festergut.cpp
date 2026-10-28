/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_Festergut.h"

#include "Creature.h"
#include "GenericSpellActions.h"
#include "ICCHelpers_Festergut.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"

namespace
{
// His combat reach is 8: a tank's area spell from this close reaches him
constexpr float BRAKE_AREA_REACH = 14.0f;
}  // namespace

float IccFestergutBrakeMultiplier::GetValue(Action* action)
{
    if (!action || dynamic_cast<CastHealingSpellAction*>(action) || !IsFgBraked(botAI))
        return 1.0f;

    Creature* boss = GetFgBoss(botAI);
    if (!boss)
        return 1.0f;

    Unit* target = action->GetTarget();
    if (target == boss || (!target && AI_VALUE(Unit*, "current target") == boss))
        return 0.0f;

    return IsRaidAreaDamage(botAI, action) && bot->GetExactDist2d(boss) <= BRAKE_AREA_REACH ? 0.0f : 1.0f;
}
