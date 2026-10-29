/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_Valithria.h"

#include "GenericSpellActions.h"
#include "ICCHelpers_Valithria.h"
#include "PlayerbotAI.h"
#include "RaidCombatHelpers.h"

float IccValithriaSplashMultiplier::GetValue(Action* action)
{
    // Heals carry the Aoe threat type too.
    if (dynamic_cast<CastHealingSpellAction*>(action) || !IsRaidAreaDamage(botAI, action))
        return 1.0f;

    return IsVdSplashHeld(botAI) ? 0.0f : 1.0f;
}
