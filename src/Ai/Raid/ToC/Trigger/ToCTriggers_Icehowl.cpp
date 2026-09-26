#include "ToCTriggers_Icehowl.h"
#include "ToCHelpers_Icehowl.h"
#include "ToCHelpers_NorthrendBeasts.h"
#include "Playerbots.h"
#include "Strategy.h"

using namespace TrialOfTheCrusaderHelpers;

bool IcehowlTankDutyTrigger::IsActive() { return GetBeastsTankDuty(botAI) == BeastsTankDuty::Icehowl; }

bool IcehowlChargeIncomingTrigger::IsActive()
{
    return DistanceToIcehowlCharge(botAI, bot->GetPositionX(), bot->GetPositionY()) < ICEHOWL_CHARGE_TRIGGER;
}

bool IcehowlFrothingRageTrigger::IsActive()
{
    Unit* icehowl = GetEngagedBeast(botAI, NorthrendBeast::Icehowl);
    return icehowl && icehowl->GetVictim() == bot && HasIcehowlFrothingRage(icehowl);
}

void AddToCIcehowlTriggerNodes(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("icehowl tank duty", {
        NextAction("icehowl tank hold boss", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("icehowl charge incoming", {
        NextAction("icehowl clear charge path", ACTION_EMERGENCY + 8) }));

    triggers.push_back(new TriggerNode("icehowl frothing rage", {
        NextAction("icehowl tank defensive", ACTION_RAID + 6) }));
}
