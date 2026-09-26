/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxStrategy.h"
#include "NaxxDefinitions.h"
#include "NaxxMultipliers.h"

void RaidNaxxStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    NaxxGrobbulusDefinition().AddTriggerNodes(triggers);
    NaxxHeiganDefinition().AddTriggerNodes(triggers);
    NaxxKelthuzadDefinition().AddTriggerNodes(triggers);
    NaxxAnubrekhanDefinition().AddTriggerNodes(triggers);
    NaxxFaerlinaDefinition().AddTriggerNodes(triggers);
    NaxxMaexxnaDefinition().AddTriggerNodes(triggers);
    NaxxGothikDefinition().AddTriggerNodes(triggers);

    // Patchwerk
    // triggers.push_back(new TriggerNode("patchwerk tank",
    //     { NextAction("tank face", ACTION_RAID + 2) }
    // ));

    // triggers.push_back(new TriggerNode("patchwerk ranged",
    //     { NextAction("patchwerk ranged position", ACTION_RAID + 2) }
    // ));

    // triggers.push_back(new TriggerNode("patchwerk non-tank",
    //     { NextAction("rear flank", ACTION_RAID + 1) }
    // ));

    NaxxThaddiusDefinition().AddTriggerNodes(triggers);
    NaxxRazuviousDefinition().AddTriggerNodes(triggers);
    NaxxFourHorsemenDefinition().AddTriggerNodes(triggers);
    NaxxSapphironDefinition().AddTriggerNodes(triggers);
    NaxxGluthDefinition().AddTriggerNodes(triggers);
    NaxxLoathebDefinition().AddTriggerNodes(triggers);
    NaxxNothDefinition().AddTriggerNodes(triggers);
}

void RaidNaxxStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    NaxxGrobbulusDefinition().AddMultipliers(botAI, multipliers);
    NaxxHeiganDefinition().AddMultipliers(botAI, multipliers);
    NaxxLoathebDefinition().AddMultipliers(botAI, multipliers);
    NaxxThaddiusDefinition().AddMultipliers(botAI, multipliers);
    NaxxSapphironDefinition().AddMultipliers(botAI, multipliers);
    NaxxRazuviousDefinition().AddMultipliers(botAI, multipliers);
    NaxxKelthuzadDefinition().AddMultipliers(botAI, multipliers);
    NaxxAnubrekhanDefinition().AddMultipliers(botAI, multipliers);
    NaxxFourHorsemenDefinition().AddMultipliers(botAI, multipliers);
    NaxxGluthDefinition().AddMultipliers(botAI, multipliers);
    NaxxGothikDefinition().AddMultipliers(botAI, multipliers);
    multipliers.push_back(new NaxxThreatRedirectMultiplier(botAI));
    multipliers.push_back(new NaxxBurstWindowMultiplier(botAI));
    NaxxNothDefinition().AddMultipliers(botAI, multipliers);
}
