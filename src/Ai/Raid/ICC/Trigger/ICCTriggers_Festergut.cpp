/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_Festergut.h"

#include "Creature.h"
#include "ICCSeams.h"
#include "Playerbots.h"
#include "Position.h"
#include "RaidDefensive.h"

bool IccFestergutGooTrigger::IsActive() { return IsFgInGoo(botAI); }

bool IccFestergutPungentDefensiveTrigger::IsActive()
{
    RaidPredictedHit hit;
    return GetFgPungentHit(botAI, hit, true) &&
           static_cast<bool>(GetRaidPersonalDefensive(botAI, ICC_DEFENSIVE_SCOPE, &hit));
}

bool IccFestergutPungentCoverTrigger::IsActive() { return GetFgPungentCover(botAI) != RaidCover::None; }

bool IccFestergutTankDefensiveTrigger::IsActive() { return GetFgTankDefensive(botAI, "fg.defensive") != 0; }

bool IccFestergutBloatSwapTrigger::IsActive() { return IsFgSwapTaunter(botAI); }

bool IccFestergutBrakeTrigger::IsActive()
{
    Creature* boss = GetFgBoss(botAI);
    return boss && IsFgBraked(botAI) && (bot->GetVictim() == boss || AI_VALUE(Unit*, "current target") == boss);
}

bool IccFestergutStandTrigger::IsActive()
{
    Position spot;
    return GetFgStandWalk(botAI, spot);
}
