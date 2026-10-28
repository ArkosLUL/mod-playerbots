/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_Marrowgar.h"

#include "Position.h"

bool IccLmStandTrigger::IsActive()
{
    Position spot;
    return GetLmStandWalk(botAI, spot);
}

bool IccLmColdflameTrigger::IsActive() { return IsLmInColdflame(botAI); }

bool IccLmTankDefensiveTrigger::IsActive() { return GetLmTankDefensive(botAI, "lm.defensive") != 0; }

bool IccLmStormCoverTrigger::IsActive() { return GetLmStormCover(botAI) != RaidCover::None; }
