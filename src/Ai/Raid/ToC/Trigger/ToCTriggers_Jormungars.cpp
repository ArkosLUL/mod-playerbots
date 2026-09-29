#include "ToCTriggers_Jormungars.h"
#include "ToCHelpers_Jormungars.h"
#include "ToCHelpers_NorthrendBeasts.h"
#include "Playerbots.h"
#include "Strategy.h"

using namespace TrialOfTheCrusaderHelpers;

// Not IsMainTank: on heroic the main tank can be holding Gormok or Icehowl while the worms are up.
bool NorthrendWormsMobileTankDutyTrigger::IsActive()
{
    return GetBeastsTankDuty(botAI) == BeastsTankDuty::WormMobile;
}

bool NorthrendWormsStationaryTankDutyTrigger::IsActive()
{
    return GetBeastsTankDuty(botAI) == BeastsTankDuty::WormStationary;
}

bool NorthrendWormsRedirectThreatTrigger::IsActive()
{
    Player* holder = nullptr;
    return GetWormRedirectTarget(botAI, holder) != nullptr;
}

bool NorthrendWormsMisplacedTrigger::IsActive()
{
    WormMovePlan plan;
    return GetWormMovePlan(botAI, plan) && plan.reason != WormMoveReason::None;
}

void AddToCJormungarsTriggerNodes(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("northrend worms mobile tank duty", {
        NextAction("northrend worms tank hold mobile worm", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("northrend worms stationary tank duty", {
        NextAction("northrend worms tank hold stationary worm", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("northrend worms redirect threat", {
        NextAction("northrend worms redirect threat", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("northrend worms misplaced", {
        NextAction("northrend worms reposition", ACTION_EMERGENCY + 6) }));
}
