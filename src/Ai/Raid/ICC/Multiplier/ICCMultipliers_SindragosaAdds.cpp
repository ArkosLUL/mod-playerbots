/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_SindragosaAdds.h"

#include "Creature.h"
#include "GenericSpellActions.h"
#include "ICCHelpers_SindragosaAdds.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"

namespace
{
// A drake's combat reach is 9: an area spell from this close reaches it
constexpr float BRAKE_AREA_REACH = 19.0f;
}  // namespace

float IccSgDrakeBrakeMultiplier::GetValue(Action* action)
{
    if (!action || dynamic_cast<CastHealingSpellAction*>(action) || !IsSgDrakeBraked(botAI))
        return 1.0f;

    Creature* drake = GetSgDrakeBrakeTarget(botAI);
    if (!drake)
        return 1.0f;

    Unit* target = action->GetTarget();
    if (target == drake || (!target && AI_VALUE(Unit*, "current target") == drake))
        return 0.0f;

    return IsRaidAreaDamage(botAI, action, _splash) && bot->GetExactDist2d(drake) <= BRAKE_AREA_REACH ? 0.0f : 1.0f;
}

bool IccSgDrakeAntiFearTotemGuardMultiplier::FearWindowActive() { return IsSgSpinestalkerFearWindow(botAI); }
