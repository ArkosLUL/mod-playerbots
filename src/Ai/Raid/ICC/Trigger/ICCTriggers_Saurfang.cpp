/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_Saurfang.h"

#include "Creature.h"
#include "ICCSeams.h"
#include "Playerbots.h"
#include "Position.h"

bool IccDbsRuneSwapTrigger::IsActive() { return IsDbsSwapTaunter(botAI); }

bool IccDbsRuneYieldTrigger::IsActive()
{
    Creature* boss = GetDbsBoss(botAI);
    return boss && IsDbsRuneYielding(botAI) &&
           (bot->GetVictim() == boss || AI_VALUE(Unit*, "current target") == boss);
}

bool IccDbsBeastTauntTrigger::IsActive() { return GetDbsBeastToTaunt(botAI) != nullptr; }

bool IccDbsBeastControlTrigger::IsActive()
{
    char const* spell = nullptr;
    return GetDbsControlDuty(botAI, spell) != nullptr;
}

bool IccDbsMarkHealTrigger::IsActive()
{
    bool beacon = false;
    return GetDbsMarkWork(botAI, beacon) != nullptr;
}

bool IccDbsProtectTrigger::IsActive() { return GetDbsProtectTarget(botAI) != nullptr; }

bool IccDbsTankDefensiveTrigger::IsActive() { return GetDbsTankDefensive(botAI, "dbs.defensive") != 0; }

bool IccDbsMarkDefensiveTrigger::IsActive()
{
    RaidPredictedHit hit;
    return GetDbsMarkHit(botAI, hit) && static_cast<bool>(GetRaidPersonalDefensive(botAI, ICC_DEFENSIVE_SCOPE, &hit));
}

bool IccDbsStandTrigger::IsActive()
{
    Position spot;
    return GetDbsStandWalk(botAI, spot);
}
