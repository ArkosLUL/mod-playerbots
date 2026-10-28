/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_Saurfang.h"

#include "GenericSpellActions.h"
#include "ICCHelpers_Saurfang.h"
#include "PaladinActions.h"
#include "PlayerbotAI.h"
#include "RaidCombatHelpers.h"

float IccDbsAreaDamageMultiplier::GetValue(Action* action)
{
    // Heals carry the Aoe threat type too.
    if (dynamic_cast<CastHealingSpellAction*>(action) || !IsRaidAreaDamage(botAI, action))
        return 1.0f;

    return PlayerbotAI::IsTank(bot) || PlayerbotAI::IsTank(bot, true) ? 1.0f : 0.0f;
}

float IccDbsBeaconDutyMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<CastBeaconOfLightOnMainTankAction*>(action) &&
        !dynamic_cast<CastBeaconOfLightOnTankAction*>(action))
        return 1.0f;

    return IsDbsBeaconDutyHeld(botAI) ? 0.0f : 1.0f;
}
