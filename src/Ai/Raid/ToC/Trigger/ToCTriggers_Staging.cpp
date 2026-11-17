#include "ToCTriggers_Staging.h"

#include "Playerbots.h"
#include "Strategy.h"
#include "ToCHelpers_Staging.h"

using namespace TrialOfTheCrusaderHelpers;

bool ToCStagingStandTrigger::IsActive()
{
    ToCStagingRead const read = GetToCStagingRead(botAI);
    return read.held && read.walk;
}

void AddToCStagingTriggerNodes(std::vector<TriggerNode*>& triggers)
{
    // the walk holds the tick while in flight, the stand gives it back once parked
    triggers.push_back(new TriggerNode("toc staging stand", {
        NextAction("toc move to staging stand", ACTION_RAID) }));
}
