/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_LichKingLate.h"

#include <string>

#include "Playerbots.h"
#include "Position.h"

bool IccLichKingDefileTrigger::IsActive()
{
    Position spot;
    return GetLkDefileWalk(botAI, spot);
}

bool IccLichKingVileSpiritsTrigger::IsActive()
{
    Position spot;
    return GetLkSoakGuard(botAI) != 0 || IsLkTrapDue(botAI) || GetLkVileWalk(botAI, spot);
}

bool IccLichKingFrostmourneTrigger::IsActive()
{
    Position spot;
    return GetLkFrostmourneWalk(botAI, spot);
}

bool IccLichKingSiphonTauntTrigger::IsActive() { return GetLkSiphonTauntTarget(botAI) != nullptr; }

bool IccLichKingValkyrControlTrigger::IsActive()
{
    uint32 spellId = 0;
    bool stun = false;
    return GetLkValkyrControl(botAI, spellId, stun) != nullptr;
}

bool IccLichKingHarvestSoulTrigger::IsActive()
{
    Unit* target = nullptr;
    std::string spell;
    LkHarvestDuty const duty = GetLkHarvestDuty(botAI, target, spell);
    return target && (duty == LkHarvestDuty::Kick || duty == LkHarvestDuty::Heal || duty == LkHarvestDuty::Dispel);
}

bool IccLichKingStackTrigger::IsActive()
{
    Position spot;
    return GetLkStackWalk(botAI, spot);
}
