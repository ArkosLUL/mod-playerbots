/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounterGate.h"

namespace
{
    struct EncounterPrefix
    {
        char const* prefix;
        uint32 bossId;
    };

    // Every Ulduar trigger name starts with its encounter.
    constexpr EncounterPrefix ENCOUNTER_PREFIXES[] = {
        {"mimiron", ULD_BOSS_MIMIRON},
    };
}

bool UldEncounterOfTrigger(std::string const& triggerName, uint32& bossId)
{
    for (EncounterPrefix const& entry : ENCOUNTER_PREFIXES)
    {
        if (triggerName.rfind(entry.prefix, 0) == 0)
        {
            bossId = entry.bossId;
            return true;
        }
    }

    return false;
}

char const* UldEncounterName(uint32 bossId)
{
    for (EncounterPrefix const& entry : ENCOUNTER_PREFIXES)
        if (entry.bossId == bossId)
            return entry.prefix;

    return nullptr;
}
