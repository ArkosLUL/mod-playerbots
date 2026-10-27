/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_Deathwhisper.h"
#include "ICCHelpers_Deathwhisper.h"
#include "Playerbots.h"
#include "Trigger.h"

bool IccDarkReckoningTrigger::IsActive()
{
    if (bot->HasAura(SPELL_DARK_RECKONING))
        return true;

    return false;
}

bool IccLadyDeathwhisperTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "lady deathwhisper");
    if (!boss)
        return false;

    return true;
}
