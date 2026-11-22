/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OSStrategy.h"
#include "OSDefinitions.h"
#include "Strategy.h"

void RaidOsStrategy::OnTick() { OsSartharionDefinition().OnTick(botAI); }

void RaidOsStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    OsSartharionDefinition().AddTriggerNodes(triggers);

    triggers.push_back(new TriggerNode("twilight portal exit",
                                       { NextAction("exit twilight portal", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("sartharion dps",
                                       { NextAction("sartharion attack priority", ACTION_RAID) }));
}

void RaidOsStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    OsSartharionDefinition().AddMultipliers(botAI, multipliers);
}
