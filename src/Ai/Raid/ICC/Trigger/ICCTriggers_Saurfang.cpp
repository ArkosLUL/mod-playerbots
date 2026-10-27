/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_Saurfang.h"
#include "ICCHelpers_Saurfang.h"
#include "ICCHelpers_Shared.h"
#include "Playerbots.h"
#include "Trigger.h"

bool IccDbsTrigger::IsActive()
{
    return AI_VALUE2(Unit*, "find target", "deathbringer saurfang") != nullptr;
}
