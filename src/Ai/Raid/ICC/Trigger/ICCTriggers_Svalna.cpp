/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_Svalna.h"
#include "ICCHelpers_Svalna.h"
#include "Playerbots.h"
#include "Trigger.h"

bool IccValkyreSpearTrigger::IsActive()
{
    // Check if there's a spear nearby
    if (bot->FindNearestCreature(NPC_SPEAR, 100.0f))
        return true;

    return false;
}

bool IccSisterSvalnaTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sister svalna");
    if (!boss)
        return false;

    if (bot->GetExactDist2d(boss) > 30.0f)
        return false;

    return IccSvalnaShielded(bot, boss);
}
