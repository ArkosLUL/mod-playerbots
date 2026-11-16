/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_BloodQueen.h"

#include "Creature.h"
#include "GenericSpellActions.h"
#include "ICCHelpers_BloodQueen.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"
#include "Timer.h"

namespace
{
// Her combat reach is 7.2: an area spell from this close reaches her
constexpr float BRAKE_AREA_REACH = 17.0f;
// A vampire this far from the bot or its target is in reach of a cleave or an area spell
constexpr float SPLASH_HOLD_RANGE = 15.0f;
}  // namespace

float IccBqlBrakeMultiplier::GetValue(Action* action)
{
    if (!action || dynamic_cast<CastHealingSpellAction*>(action) || !IsBqlBraked(botAI))
        return 1.0f;

    Creature* queen = GetBqlQueen(botAI);
    if (!queen)
        return 1.0f;

    Unit* target = action->GetTarget();
    if (target == queen || (!target && AI_VALUE(Unit*, "current target") == queen))
        return 0.0f;

    return IsRaidAreaDamage(botAI, action, _splash) && bot->GetExactDist2d(queen) <= BRAKE_AREA_REACH ? 0.0f : 1.0f;
}

float IccBqlSplashMultiplier::GetValue(Action* action)
{
    // Heals carry the Aoe threat type too
    if (!action || dynamic_cast<CastHealingSpellAction*>(action) || !IsRaidAreaDamage(botAI, action, _splash))
        return 1.0f;

    // Asked for every area action the bot weighs, and a bot never ticks twice in one ms
    uint32 const now = getMSTime();
    if (now != _checkedMs)
    {
        _checkedMs = now;
        _vampireNear = IsBqlVampireInSplash(botAI, SPLASH_HOLD_RANGE);
    }

    return _vampireNear ? 0.0f : 1.0f;
}

bool IccBqlAntiFearTotemGuardMultiplier::FearWindowActive() { return IsBqlFearWindow(botAI); }
