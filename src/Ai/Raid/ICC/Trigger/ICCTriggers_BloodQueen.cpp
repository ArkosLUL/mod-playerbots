/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_BloodQueen.h"
#include "ICCHelpers_BloodQueen.h"
#include "ICCHelpers_Shared.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "Trigger.h"

bool IccBqlGroupPositionTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "blood-queen lana'thel");
    if (!boss)
        return false;

    return !IccAnyBloodPrincePresent(botAI);
}

bool IccBqlPactOfDarkfallenTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "blood-queen lana'thel");
    if (!boss)
        return false;

    if (IccAnyBloodPrincePresent(botAI))
        return false;

    Aura* aura = botAI->GetAura("Pact of the Darkfallen", bot);
    if (!aura)
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "bql.pact", "none");
        return false;
    }

    return true;
}

bool IccBqlVampiricBiteTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "blood-queen lana'thel");
    if (!boss)
        return false;

    if (IccAnyBloodPrincePresent(botAI))
        return false;

    Aura* aura = botAI->GetAura("Frenzied Bloodthirst", bot);
    if (!aura)
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "bql.bite", "none");
        return false;
    }

    return true;
}
