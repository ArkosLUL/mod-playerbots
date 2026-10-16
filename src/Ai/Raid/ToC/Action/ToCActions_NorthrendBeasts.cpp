#include "ToCActions_NorthrendBeasts.h"

#include "ToCHelpers_NorthrendBeasts.h"

using namespace TrialOfTheCrusaderHelpers;

bool NorthrendBeastsDpsTargetAction::Execute(Event /*event*/)
{
    return AttackDirect(GetBeastsDpsTarget(botAI).unit);
}
