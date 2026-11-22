#include "ToCRaidStrategy.h"
#include "RaidDirectTargeting.h"
#include "ToCData.h"
#include "ToCDefinitions.h"
#include "ToCHelpers_FactionChampionsDefence.h"
#include "ToCHelpers_Staging.h"
#include "ToCMultipliers_Shared.h"

void RaidTrialOfTheCrusaderStrategy::OnTick()
{
    TrialOfTheCrusaderHelpers::FactionChampionsTakePullRequest(botAI);
    TrialOfTheCrusaderHelpers::UpdateToCStagingPark(botAI);

    if (_legacyRtiChecked)
        return;

    _legacyRtiChecked = true;
    RaidRestoreLegacyRti(botAI, TrialOfTheCrusaderHelpers::TRIAL_OF_THE_CRUSADER_MAP_ID);
}

void RaidTrialOfTheCrusaderStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    for (EncounterDefinition const* encounter : ToCEncounterDefinitions())
        encounter->AddTriggerNodes(triggers);

    triggers.push_back(new TriggerNode("pull", {
        NextAction("faction champions chain request", ACTION_RAID + 4.7f) }));
}

void RaidTrialOfTheCrusaderStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    for (EncounterDefinition const* encounter : ToCEncounterDefinitions())
        encounter->AddMultipliers(botAI, multipliers);

    multipliers.push_back(new RaidBurstWindowMultiplier(botAI, "toc burst window", &ToCLiveBurstWindow));
}
