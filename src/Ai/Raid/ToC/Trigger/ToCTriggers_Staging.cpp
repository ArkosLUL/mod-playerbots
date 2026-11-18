#include "ToCTriggers_Staging.h"

#include "Playerbots.h"
#include "ToCHelpers_Staging.h"

using namespace TrialOfTheCrusaderHelpers;

bool ToCStagingStandTrigger::IsActive()
{
    ToCStagingRead const read = GetToCStagingRead(botAI);
    return read.held && read.walk;
}
