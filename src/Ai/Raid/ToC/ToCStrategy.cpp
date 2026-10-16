#include "ToCStrategy.h"
#include "ToCDefinitions.h"
#include "ToCHelpers_Shared.h"
#include "ToCRaidMultipliers.h"
#include "ToCRaidTriggers.h"

// Once per strategy object, so a player's later rti order sticks until the strategy is rebuilt.
void RaidTrialOfTheCrusaderStrategy::OnTick()
{
    if (legacyRtiChecked)
        return;

    legacyRtiChecked = true;
    TrialOfTheCrusaderHelpers::ToCRestoreLegacyRti(botAI);
}

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

// A trace credits the first rule or multiplier that zeroes an action, so each encounter's rules
// follow its own stem multipliers.
void RaidTrialOfTheCrusaderStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    AddToCGormokMultipliers(botAI, multipliers);
    AddToCJormungarsMultipliers(botAI, multipliers);
    AddToCIcehowlMultipliers(botAI, multipliers);
    AddToCNorthrendBeastsMultipliers(botAI, multipliers);
    ToCNorthrendBeastsDefinition().AddMultipliers(botAI, multipliers);
    AddToCJaraxxusMultipliers(botAI, multipliers);
    ToCJaraxxusDefinition().AddMultipliers(botAI, multipliers);
    AddToCAnubarakMultipliers(botAI, multipliers);
    ToCAnubarakDefinition().AddMultipliers(botAI, multipliers);
    AddToCFactionChampionsMultipliers(botAI, multipliers);
    ToCFactionChampionsDefinition().AddMultipliers(botAI, multipliers);
    AddToCFactionChampionsDefenceMultipliers(botAI, multipliers);
    AddToCTwinValkyrMultipliers(botAI, multipliers);
    ToCTwinValkyrDefinition().AddMultipliers(botAI, multipliers);
    multipliers.push_back(new ToCBurstWindowMultiplier(botAI));
}
