/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_BloodPrinces.h"

#include "Playerbots.h"
#include "Position.h"

bool IccBpcDodgeTrigger::IsActive() { return IsBpcInVortex(botAI); }

bool IccBpcTankDefensiveTrigger::IsActive() { return GetBpcTankDefensive(botAI, "bpc.defensive") != 0; }

bool IccBpcBallTrigger::IsActive()
{
    Position spot;
    return GetBpcBallWalk(botAI, spot);
}

bool IccBpcPrinceTauntTrigger::IsActive() { return GetBpcPrinceToTaunt(botAI) != nullptr; }

bool IccBpcNucleusTrigger::IsActive()
{
    Position spot;
    return GetBpcNucleusToTaunt(botAI) || GetBpcNucleusWalk(botAI, spot);
}

bool IccBpcSoakTrigger::IsActive()
{
    Position spot;
    return GetBpcSoakWalk(botAI, spot);
}

bool IccBpcStandTrigger::IsActive()
{
    Position spot;
    return GetBpcStandWalk(botAI, spot);
}
