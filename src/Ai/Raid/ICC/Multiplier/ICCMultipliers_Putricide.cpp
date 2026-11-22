/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_Putricide.h"

#include "Creature.h"
#include "GenericSpellActions.h"
#include "ICCHelpers_Putricide.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"

namespace
{
constexpr float BRAKE_AREA_REACH = 18.0f;
}

float IccPutricideBrakeMultiplier::GetValue(Action* action)
{
    if (!action || dynamic_cast<CastHealingSpellAction*>(action) || !IsPpBraked(botAI))
        return 1.0f;

    Creature* boss = GetPpBoss(botAI);
    if (!boss)
        return 1.0f;

    Unit* target = action->GetTarget();
    if (target == boss || (!target && AI_VALUE(Unit*, "current target") == boss))
        return 0.0f;

    return IsRaidAreaDamage(botAI, action, _splash) && bot->GetExactDist2d(boss) <= BRAKE_AREA_REACH ? 0.0f : 1.0f;
}
