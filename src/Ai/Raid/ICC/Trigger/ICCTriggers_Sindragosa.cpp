/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_Sindragosa.h"

#include <string>

#include "Playerbots.h"
#include "Position.h"

bool IccSindragosaColdTrigger::IsActive()
{
    Position spot;
    return GetSgColdWalk(botAI, spot);
}

bool IccSindragosaSplashTrigger::IsActive()
{
    Position spot;
    return GetSgSplashWalk(botAI, spot);
}

bool IccSindragosaBombHideTrigger::IsActive()
{
    Position spot;
    return GetSgBombHideWalk(botAI, spot);
}

bool IccSindragosaBeaconTrigger::IsActive()
{
    Position spot;
    return GetSgBeaconWalk(botAI, spot);
}

bool IccSindragosaTankDefensiveTrigger::IsActive() { return GetSgTankDefensive(botAI, "sg.defensive") != 0; }

bool IccSindragosaTauntTrigger::IsActive()
{
    bool swap = false;
    return GetSgTauntTarget(botAI, swap) != nullptr;
}

bool IccSindragosaBuffetHideTrigger::IsActive()
{
    Position spot;
    return GetSgBuffetHideWalk(botAI, spot);
}

bool IccSindragosaUnchainedTrigger::IsActive()
{
    Position spot;
    return GetSgUnchainedWalk(botAI, spot);
}

bool IccSindragosaFreedomTrigger::IsActive() { return GetSgFreedomTarget(botAI) != nullptr; }

bool IccSindragosaCoverTrigger::IsActive()
{
    std::string spell;
    return GetSgCoverTarget(botAI, spell) != nullptr;
}

bool IccSindragosaBeaconHealTrigger::IsActive()
{
    std::string spell;
    return GetSgBeaconHealTarget(botAI, spell) != nullptr;
}

bool IccSindragosaHoldTrigger::IsActive() { return GetSgHoldDue(botAI) != 0; }

bool IccSindragosaStandTrigger::IsActive()
{
    Position spot;
    return GetSgStandWalk(botAI, spot) || (IsSgStandHalted(botAI) && bot->isMoving());
}
