/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCStrategy.h"

#include "ICCDefinitions.h"
#include "ICCSeams.h"
#include "RaidBurstWindow.h"

void RaidIccStrategy::OnTick()
{
    for (EncounterDefinition const* encounter : IccEncounterDefinitions())
        encounter->OnTick(botAI);
}

void RaidIccStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    for (EncounterDefinition const* encounter : IccEncounterDefinitions())
        encounter->AddTriggerNodes(triggers);
}

// Not the node order, and don't reorder: the engine stops at the first multiplier that zeroes an action and
// credits it with the veto, and some of these act inside GetValue.
void RaidIccStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    IccMarrowgarDefinition().AddMultipliers(botAI, multipliers);
    IccDeathwhisperDefinition().AddMultipliers(botAI, multipliers);
    IccFrostGiantDefinition().AddMultipliers(botAI, multipliers);
    IccSaurfangDefinition().AddMultipliers(botAI, multipliers);
    IccDogsDefinition().AddMultipliers(botAI, multipliers);
    IccFestergutDefinition().AddMultipliers(botAI, multipliers);
    IccRotfaceDefinition().AddMultipliers(botAI, multipliers);
    IccPutricideDefinition().AddMultipliers(botAI, multipliers);
    IccBloodPrincesDefinition().AddMultipliers(botAI, multipliers);
    IccBloodQueenDefinition().AddMultipliers(botAI, multipliers);
    IccSvalnaDefinition().AddMultipliers(botAI, multipliers);
    IccValithriaDefinition().AddMultipliers(botAI, multipliers);
    IccSindragosaDefinition().AddMultipliers(botAI, multipliers);
    IccLichKingDefinition().AddMultipliers(botAI, multipliers);
    IccGunshipDefinition().AddMultipliers(botAI, multipliers);
    IccRaidWideDefinition().AddMultipliers(botAI, multipliers);
    multipliers.push_back(new RaidBurstWindowMultiplier(botAI, "icc burst window", &IccLiveBurstWindow));
}
