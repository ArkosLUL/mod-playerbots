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
// A Dominate Mind victim this far from the bot or its target is in reach of a cleave or an area spell.
constexpr float LDW_SPLASH_HOLD_RANGE = 15.0f;
}

float IccLadyDeathwhisperMultiplier::GetValue(Action* action)
{
    // Heals carry the Aoe threat type too.
    if (dynamic_cast<CastHealingSpellAction*>(action) || !IsRaidAreaDamage(botAI, action))
        return 1.0f;

    // Cached per ms: asked for every area action the bot weighs, and a bot never ticks twice in one ms.
    uint32 const now = getMSTime();
    if (now != _checkedMs)
    {
        _checkedMs = now;
        _victimNear = LdwVictimInSplash(botAI, LDW_SPLASH_HOLD_RANGE);
    }

    return _victimNear ? 0.0f : 1.0f;
}
