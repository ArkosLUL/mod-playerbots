/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_FrostGiant.h"
#include "ICCHelpers_FrostGiant.h"
#include "ICCHelpers_Shared.h"
#include "Playerbots.h"
#include "Trigger.h"

bool IccRottingFrostGiantTankPositionTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "rotting frost giant");
    if (!boss)
        return false;

    IccMarkPull(bot, boss, "rotting-frost-giant");
    IccStripExperienced(bot);

    return true;
}
