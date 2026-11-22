/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_Deathwhisper.h"

#include "GenericSpellActions.h"
#include "ICCHelpers_Deathwhisper.h"
#include "PlayerbotAI.h"
#include "RaidCombatHelpers.h"
#include "Timer.h"

namespace
{
constexpr float LDW_SPLASH_HOLD_RANGE = 15.0f;
}

float IccLadyDeathwhisperMultiplier::GetValue(Action* action)
{
    if (dynamic_cast<CastHealingSpellAction*>(action) || !IsRaidAreaDamage(botAI, action, _splash))
        return 1.0f;

    uint32 const now = getMSTime();
    if (now != _checkedMs)
    {
        _checkedMs = now;
        _victimNear = LdwVictimInSplash(botAI, LDW_SPLASH_HOLD_RANGE);
    }

    return _victimNear ? 0.0f : 1.0f;
}

float IccLadyDeathwhisperBrakeMultiplier::GetValue(Action* action)
{
    if (!action || dynamic_cast<CastHealingSpellAction*>(action) || !LdwThreatBraked(botAI))
        return 1.0f;

    Creature* boss = LdwBoss(botAI);
    if (!boss)
        return 1.0f;

    Unit* target = action->GetTarget();
    if (target == boss || (!target && AI_VALUE(Unit*, "current target") == boss))
        return 0.0f;

    return IsRaidAreaDamage(botAI, action, _splash) && bot->GetExactDist2d(boss) <= LDW_BRAKE_AREA_REACH ? 0.0f : 1.0f;
}
