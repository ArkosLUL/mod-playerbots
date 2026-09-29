#include "ToCTriggers_Gormok.h"
#include "ToCHelpers_Gormok.h"
#include "ToCHelpers_NorthrendBeasts.h"
#include "Playerbots.h"
#include "Strategy.h"

using namespace TrialOfTheCrusaderHelpers;

bool GormokTankDutyTrigger::IsActive()
{
    return GetBeastsTankDuty(botAI) == BeastsTankDuty::Gormok &&
           GetEngagedBeast(botAI, NorthrendBeast::Gormok);
}

bool GormokSnoboldOnRaidTrigger::IsActive()
{
    return GetGormokSnoboldPick(botAI) != nullptr;
}

bool GormokTankSwapNeededTrigger::IsActive()
{
    return GetGormokSwapTauntTarget(botAI) != nullptr;
}

bool GormokTankDefensiveTrigger::IsActive()
{
    return GormokTankNeedsDefensive(botAI);
}

bool GormokSnobolledTrigger::IsActive()
{
    return GetGormokForSnoboldCarrier(botAI) != nullptr;
}

bool GormokStompRangeTrigger::IsActive()
{
    return GetGormokStompThreat(botAI) != nullptr;
}

bool GormokFireBombIncomingTrigger::IsActive()
{
    return IsInFireBombImpact(botAI);
}

void AddToCGormokTriggerNodes(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("gormok tank duty", {
        NextAction("gormok tank hold boss", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("gormok snobold on raid", {
        NextAction("gormok focus snobold", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode("gormok snobolled", {
        NextAction("gormok bring snobold to melee", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode("gormok stomp range", {
        NextAction("gormok leave stomp range", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode("gormok tank swap needed", {
        NextAction("gormok tank swap taunt", ACTION_RAID + 5) }));

    triggers.push_back(new TriggerNode("gormok tank defensive", {
        NextAction("gormok tank defensive", ACTION_RAID + 6) }));

    triggers.push_back(new TriggerNode("gormok fire bomb incoming", {
        NextAction("gormok dodge fire bomb", ACTION_EMERGENCY + 2) }));
}
