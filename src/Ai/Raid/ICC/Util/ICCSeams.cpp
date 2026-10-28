/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCSeams.h"

#include "ICCDefinitions.h"
#include "ICCHelpers_Deathwhisper.h"
#include "ICCHelpers_Festergut.h"
#include "ICCHelpers_Marrowgar.h"
#include "ICCHelpers_Saurfang.h"
#include "RaidEncounter.h"

namespace
{
// One row per encounter that fills a hook, in IccEncounterDefinitions() order. Its functions live in the
// encounter's own stem files.
std::vector<IccEncounterHooks> const& HookRows()
{
    static std::vector<IccEncounterHooks> const rows = {
        {&IccMarrowgarDefinition, nullptr, &GetLmOpenerBosses, nullptr, nullptr},
        {&IccDeathwhisperDefinition, &IccDeathwhisperBurstWindow},
        {&IccSaurfangDefinition, &IccSaurfangBurstWindow, &GetDbsOpenerBosses, nullptr, nullptr},
        {&IccFestergutDefinition, nullptr, nullptr, &IccFestergutResistSchools, nullptr},
    };
    return rows;
}
}  // namespace

IccEncounterHooks const* IccLiveHooks(PlayerbotAI* botAI)
{
    for (IccEncounterHooks const& row : HookRows())
        if (row.definition().Live(botAI))
            return &row;

    return nullptr;
}

RaidBurstWindow IccLiveBurstWindow(PlayerbotAI* botAI)
{
    IccEncounterHooks const* live = IccLiveHooks(botAI);
    return live && live->burstWindow ? live->burstWindow(botAI) : RaidBurstWindow();
}

std::vector<uint32> const& IccOpenerBosses(PlayerbotAI* botAI)
{
    static std::vector<uint32> const none;
    IccEncounterHooks const* live = IccLiveHooks(botAI);
    return live && live->openerBosses ? live->openerBosses(botAI) : none;
}

uint8 IccResistWantedSchools(PlayerbotAI* botAI, uint8 previous)
{
    IccEncounterHooks const* live = IccLiveHooks(botAI);
    return live && live->resistSchools ? live->resistSchools(botAI, previous) : 0;
}

bool IccResistFireWantsMastery(PlayerbotAI* botAI)
{
    IccEncounterHooks const* live = IccLiveHooks(botAI);
    return live && live->fireWantsMastery && live->fireWantsMastery(botAI);
}

bool IccResistAuraWanted(PlayerbotAI* botAI)
{
    IccEncounterHooks const* live = IccLiveHooks(botAI);
    return live && live->resistSchools;
}
