#include "ToCStrategy.h"
#include "ToCRaidMultipliers.h"
#include "ToCRaidTriggers.h"

// Don't reorder: equal relevances pop in insertion order.
void RaidTrialOfTheCrusaderStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    AddToCGormokTriggerNodes(triggers);
    AddToCJormungarsTriggerNodes(triggers);
    AddToCIcehowlTriggerNodes(triggers);
    AddToCNorthrendBeastsTriggerNodes(triggers);
    AddToCJaraxxusTriggerNodes(triggers);
    AddToCAnubarakTriggerNodes(triggers);
    AddToCFactionChampionsTriggerNodes(triggers);
    AddToCFactionChampionsDefenceTriggerNodes(triggers);
    AddToCTwinValkyrTriggerNodes(triggers);
}

void RaidTrialOfTheCrusaderStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    AddToCGormokMultipliers(botAI, multipliers);
    AddToCJormungarsMultipliers(botAI, multipliers);
    AddToCIcehowlMultipliers(botAI, multipliers);
    AddToCNorthrendBeastsMultipliers(botAI, multipliers);
    AddToCJaraxxusMultipliers(botAI, multipliers);
    AddToCAnubarakMultipliers(botAI, multipliers);
    AddToCFactionChampionsMultipliers(botAI, multipliers);
    AddToCFactionChampionsDefenceMultipliers(botAI, multipliers);
    AddToCTwinValkyrMultipliers(botAI, multipliers);
    multipliers.push_back(new ToCBurstWindowMultiplier(botAI));
}
