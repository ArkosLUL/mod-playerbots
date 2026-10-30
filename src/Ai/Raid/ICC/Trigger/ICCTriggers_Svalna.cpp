/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_Svalna.h"

#include "Playerbots.h"

bool IccSvalnaSpearTrigger::IsActive() { return GetSsSpearToPull(botAI) != nullptr; }

bool IccSvalnaHurlTrigger::IsActive() { return GetSsSpearToThrow(botAI) != nullptr; }

bool IccSvalnaInterruptTrigger::IsActive()
{
    char const* spell = nullptr;
    return GetSsInterrupt(botAI, spell) != nullptr;
}

bool IccSvalnaTankDefensiveTrigger::IsActive() { return GetSsTankDefensive(botAI, "ss.defensive") != 0; }

bool IccSvalnaTauntTrigger::IsActive() { return GetSsCaptainToTaunt(botAI) != nullptr; }

bool IccSvalnaGauntletDodgeTrigger::IsActive() { return IsSsGauntletInDanger(botAI); }

bool IccSvalnaGauntletInterruptTrigger::IsActive()
{
    char const* spell = nullptr;
    return GetSsGauntletInterrupt(botAI, spell) != nullptr;
}

bool IccSvalnaGauntletTankDefensiveTrigger::IsActive()
{
    return GetSsGauntletTankDefensive(botAI, "ss.gauntlet.defensive") != 0;
}

bool IccSvalnaGauntletTauntTrigger::IsActive() { return GetSsGauntletTauntTarget(botAI) != nullptr; }
