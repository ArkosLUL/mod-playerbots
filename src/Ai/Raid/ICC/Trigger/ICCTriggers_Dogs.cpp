/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_Dogs.h"
#include "ICCData.h"
#include "ICCHelpers_Shared.h"
#include "Playerbots.h"
#include "Trigger.h"

bool IccDogsTrigger::IsActive()
{
    Unit* dog = AI_VALUE2(Unit*, "find target", "stinky");
    if (!dog)
        dog = AI_VALUE2(Unit*, "find target", "precious");
    if (!dog)
        return false;

    IccMarkPull(bot, dog, "stinky-and-precious");
    return true;
}
