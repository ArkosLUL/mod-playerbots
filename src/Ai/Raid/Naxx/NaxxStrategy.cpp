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

    // Kel'Thuzad
    triggers.push_back(
        new TriggerNode("kel'thuzad",
        {
            NextAction("kel'thuzad misdirect boss to main tank", ACTION_RAID + 3),
            NextAction("kel'thuzad position", ACTION_RAID + 2),
            NextAction("kel'thuzad choose target", ACTION_RAID + 1)
        })
    );

    // Emergency priority so the flee beats every P2 positioning action by construction.
    triggers.push_back(new TriggerNode("kel'thuzad shadow fissure",
        { NextAction("kel'thuzad flee shadow fissure", ACTION_EMERGENCY + 6) }
    ));

    triggers.push_back(new TriggerNode("kel'thuzad chains",
        { NextAction("kel'thuzad cyclone chained", ACTION_EMERGENCY + 5) }
    ));

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

    // sapphiron
    triggers.push_back(new TriggerNode("sapphiron ground",
        { NextAction("sapphiron ground position", ACTION_RAID + 1) }
    ));

    triggers.push_back(new TriggerNode("sapphiron flight",
        { NextAction("sapphiron flight position", ACTION_RAID + 1) }
    ));

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
    multipliers.push_back(new SapphironGenericMultiplier(botAI));
    NaxxRazuviousDefinition().AddMultipliers(botAI, multipliers);
    multipliers.push_back(new KelthuzadGenericMultiplier(botAI));
    NaxxAnubrekhanDefinition().AddMultipliers(botAI, multipliers);
    NaxxFourHorsemenDefinition().AddMultipliers(botAI, multipliers);
    NaxxGluthDefinition().AddMultipliers(botAI, multipliers);
    NaxxGothikDefinition().AddMultipliers(botAI, multipliers);
    multipliers.push_back(new NaxxThreatRedirectMultiplier(botAI));
    multipliers.push_back(new NaxxBurstWindowMultiplier(botAI));
    NaxxNothDefinition().AddMultipliers(botAI, multipliers);
}
