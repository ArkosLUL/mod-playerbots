/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCDEFINITIONS_H
#define PLAYERBOTS_ICCDEFINITIONS_H

#include <vector>

#include "RaidEncounter.h"

EncounterDefinition const& IccMarrowgarDefinition();
EncounterDefinition const& IccDeathwhisperDefinition();
EncounterDefinition const& IccFrostGiantDefinition();
EncounterDefinition const& IccGunshipDefinition();
EncounterDefinition const& IccSaurfangDefinition();
EncounterDefinition const& IccDogsDefinition();
EncounterDefinition const& IccFestergutDefinition();
EncounterDefinition const& IccRotfaceDefinition();
EncounterDefinition const& IccPutricideDefinition();
EncounterDefinition const& IccBloodPrincesDefinition();
EncounterDefinition const& IccBloodQueenDefinition();
EncounterDefinition const& IccSvalnaDefinition();
EncounterDefinition const& IccValithriaDefinition();
EncounterDefinition const& IccSindragosaDefinition();
EncounterDefinition const& IccLichKingDefinition();

// Every ICC encounter in node order. Don't reorder: equal relevances pop in insertion order. The contexts
// and the strategy's tick walk it too.
inline std::vector<EncounterDefinition const*> const& IccEncounterDefinitions()
{
    static std::vector<EncounterDefinition const*> const definitions = {
        &IccMarrowgarDefinition(),
        &IccDeathwhisperDefinition(),
        &IccFrostGiantDefinition(),
        &IccGunshipDefinition(),
        &IccSaurfangDefinition(),
        &IccDogsDefinition(),
        &IccFestergutDefinition(),
        &IccRotfaceDefinition(),
        &IccPutricideDefinition(),
        &IccBloodPrincesDefinition(),
        &IccBloodQueenDefinition(),
        &IccSvalnaDefinition(),
        &IccValithriaDefinition(),
        &IccSindragosaDefinition(),
        &IccLichKingDefinition(),
    };
    return definitions;
}

#endif
