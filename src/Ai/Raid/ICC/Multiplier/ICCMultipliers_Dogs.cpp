/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_Dogs.h"

#include "Creature.h"
#include "GenericSpellActions.h"
#include "ICCHelpers_Dogs.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"

namespace
{
// The dogs' combat reach is 9: a tank's area spell from this close reaches one
constexpr float YIELD_AREA_REACH = 15.0f;
}  // namespace

float IccDogsYieldMultiplier::GetValue(Action* action)
{
    if (!action || dynamic_cast<CastHealingSpellAction*>(action))
        return 1.0f;

    Creature* dog = GetDogsBoss(botAI);
    if (!dog || !IsDogsYielding(botAI))
        return 1.0f;

    Unit* target = action->GetTarget();
    if (target == dog || (!target && AI_VALUE(Unit*, "current target") == dog))
        return 0.0f;

    return IsRaidAreaDamage(botAI, action) && bot->GetExactDist2d(dog) <= YIELD_AREA_REACH ? 0.0f : 1.0f;
}
