/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OnyStrategy.h"

#include "OnyDefinitions.h"

void RaidOnyxiaStrategy::OnTick() { OnyxiaDefinition().OnTick(botAI); }

void RaidOnyxiaStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    OnyxiaDefinition().AddTriggerNodes(triggers);
}

void RaidOnyxiaStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    OnyxiaDefinition().AddMultipliers(botAI, multipliers);
}
