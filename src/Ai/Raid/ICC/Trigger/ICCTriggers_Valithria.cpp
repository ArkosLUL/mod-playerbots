/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_Valithria.h"

#include "Creature.h"
#include "Playerbots.h"
#include "Position.h"

bool IccValithriaDodgeTrigger::IsActive() { return IsVdInDanger(botAI); }

bool IccValithriaKiteTrigger::IsActive() { return IsVdKiting(botAI); }

bool IccValithriaDreamTrigger::IsActive()
{
    Position dest;
    VdFlight const flight = GetVdFlight(botAI, dest);
    return flight != VdFlight::None && flight != VdFlight::Hover;
}

bool IccValithriaPortalTrigger::IsActive()
{
    Position spot;
    Creature* portal = nullptr;
    return GetVdPortalWalk(botAI, spot, portal);
}

bool IccValithriaTauntTrigger::IsActive() { return GetVdTankTaunt(botAI) != nullptr; }

bool IccValithriaSnareTrigger::IsActive()
{
    char const* spell = nullptr;
    return GetVdZombieToSnare(botAI, spell) != nullptr;
}

bool IccValithriaVoidTrigger::IsActive()
{
    Position spot;
    return GetVdVoidStep(botAI, spot);
}

bool IccValithriaHealTrigger::IsActive() { return GetVdHealTarget(botAI) != nullptr; }
