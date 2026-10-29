/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_Putricide.h"

#include "Creature.h"
#include "Playerbots.h"
#include "Position.h"

bool IccPutricideDodgeTrigger::IsActive() { return GetPpHazardKind(botAI) != nullptr; }

bool IccPutricideSwapTrigger::IsActive()
{
    uint32 defensive = 0;
    bool taunt = false;
    return IsPpSwapTaunter(botAI, defensive, taunt) && (taunt || defensive);
}

bool IccPutricideTankDefensiveTrigger::IsActive() { return GetPpTankDefensive(botAI, "pp.defensive") != 0; }

bool IccPutricideKiteTrigger::IsActive() { return IsPpKiter(botAI); }

bool IccPutricidePlagueTrigger::IsActive()
{
    Position spot;
    char const* branch = nullptr;
    return GetPpPlagueWalk(botAI, spot, branch);
}

bool IccPutricideStackTrigger::IsActive()
{
    Position spot;
    return GetPpStackWalk(botAI, spot);
}

bool IccPutricideAbominationTrigger::IsActive()
{
    PpAbomPlan plan;
    return GetPpAbomPlan(botAI, plan);
}

bool IccPutricideBrakeTrigger::IsActive()
{
    Creature* boss = IsPpBraked(botAI) ? GetPpBoss(botAI) : nullptr;
    return boss && (bot->GetVictim() == boss || AI_VALUE(Unit*, "current target") == boss);
}

bool IccPutricideStandTrigger::IsActive()
{
    Position spot;
    return GetPpStandWalk(botAI, spot);
}
