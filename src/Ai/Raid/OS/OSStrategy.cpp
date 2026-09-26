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

    // Outside Sartharion's gate: a drake pulled on its own starts an encounter of its own, which
    // closes his, and it still opens a Twilight portal with its acolyte or eggs behind it.
    triggers.push_back(new TriggerNode("twilight portal exit",
                                       { NextAction("exit twilight portal", ACTION_RAID + 1) }));
    triggers.push_back(new TriggerNode("sartharion dps",
                                       { NextAction("sartharion attack priority", ACTION_RAID) }));
}

void RaidOsStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    OsSartharionDefinition().AddMultipliers(botAI, multipliers);
}
