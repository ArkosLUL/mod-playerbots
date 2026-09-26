/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_NAXXDEFINITIONS_H
#define PLAYERBOTS_NAXXDEFINITIONS_H

#include <vector>

#include "RaidEncounter.h"

// Boss ids from naxxramas.h in AC, which lives in AC's scripts directory and so can't be included
// from here. These are the indices InstanceScript::GetBossState is keyed by.
enum NaxxEncounterId
{
    NAXX_BOSS_PATCHWERK = 0,
    NAXX_BOSS_GROBBULUS = 1,
    NAXX_BOSS_GLUTH = 2,
    NAXX_BOSS_NOTH = 3,
    NAXX_BOSS_HEIGAN = 4,
    NAXX_BOSS_LOATHEB = 5,
    NAXX_BOSS_ANUB = 6,
    NAXX_BOSS_FAERLINA = 7,
    NAXX_BOSS_MAEXXNA = 8,
    NAXX_BOSS_THADDIUS = 9,
    NAXX_BOSS_RAZUVIOUS = 10,
    NAXX_BOSS_GOTHIK = 11,
    NAXX_BOSS_HORSEMAN = 12,
    NAXX_BOSS_SAPPHIRON = 13,
    NAXX_BOSS_KELTHUZAD = 14
};

EncounterDefinition const& NaxxAnubrekhanDefinition();
EncounterDefinition const& NaxxFaerlinaDefinition();
EncounterDefinition const& NaxxMaexxnaDefinition();

// Every Naxxramas boss with a definition. The contexts walk this.
inline std::vector<EncounterDefinition const*> const& NaxxEncounterDefinitions()
{
    static std::vector<EncounterDefinition const*> const definitions = {
        &NaxxAnubrekhanDefinition(),
        &NaxxFaerlinaDefinition(),
        &NaxxMaexxnaDefinition(),
    };
    return definitions;
}

#endif
