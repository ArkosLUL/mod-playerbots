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

    // Every Ulduar trigger name starts with its encounter. No prefix here is a prefix of another, so
    // first match wins. `sara` is Yogg-Saron's phase-one form and the one name that does not lead with
    // the encounter.
    constexpr EncounterPrefix ENCOUNTER_PREFIXES[] = {
        {"mimiron", ULD_BOSS_MIMIRON},
        {"yogg-saron", ULD_BOSS_YOGGSARON},
        {"sara", ULD_BOSS_YOGGSARON},
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
    // First entry wins, which is why `sara` sitting after `yogg-saron` matters: both name the same
    // encounter and only one of them reads as a boss.
    for (EncounterPrefix const& entry : ENCOUNTER_PREFIXES)
        if (entry.bossId == bossId)
            return entry.prefix;

    return nullptr;
}
