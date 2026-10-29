/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_Rotface.h"

#include "Playerbots.h"
#include "Position.h"

bool IccRotfaceDodgeTrigger::IsActive() { return GetRfHazardKind(botAI) != nullptr; }

bool IccRotfaceFreedomTrigger::IsActive()
{
    uint32 spellId = 0;
    return GetRfFreedom(botAI, spellId) != nullptr;
}

bool IccRotfaceTotemTrigger::IsActive() { return GetRfTotemSwap(botAI) != nullptr; }

bool IccRotfaceTankDefensiveTrigger::IsActive() { return GetRfTankDefensive(botAI, "rf.defensive") != 0; }

bool IccRotfaceDispelTrigger::IsActive()
{
    char const* spell = nullptr;
    return GetRfDispelTarget(botAI, spell) != nullptr;
}

bool IccRotfaceOozeTauntTrigger::IsActive() { return GetRfOozeToTaunt(botAI) != nullptr; }

bool IccRotfaceKiteTrigger::IsActive()
{
    Position spot;
    int8 dir = 0;
    bool wade = false;
    return GetRfKiteWalk(botAI, spot, dir, wade);
}

bool IccRotfaceCarrierTrigger::IsActive()
{
    Position spot;
    return GetRfCarrierWalk(botAI, spot);
}

bool IccRotfaceCureWalkTrigger::IsActive()
{
    Position spot;
    return GetRfCureWalk(botAI, spot);
}

bool IccRotfaceStandTrigger::IsActive()
{
    Position spot;
    return GetRfStandWalk(botAI, spot);
}
