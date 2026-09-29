#include "ToCTriggers_FactionChampionsDefence.h"
#include "ToCHelpers_FactionChampionsDefence.h"
#include "Playerbots.h"
#include "Strategy.h"

using namespace TrialOfTheCrusaderHelpers;

bool FactionChampionsAvoidAoeTrigger::IsActive() { return FactionChampionsInAoe(botAI); }

bool FactionChampionsMassDispelTrigger::IsActive() { return FactionChampionsMassDispelTarget(botAI) != nullptr; }

bool FactionChampionsDispelCcTrigger::IsActive()
{
    char const* spell = nullptr;
    return FactionChampionsDispelCcTarget(botAI, spell) != nullptr;
}

bool FactionChampionsPhysicalSwitchTrigger::IsActive()
{
    Unit* target = FactionChampionsPhysicalSwitchTarget(botAI);
    return target && AI_VALUE(Unit*, "current target") != target;
}

bool FactionChampionsPurgeKillTargetTrigger::IsActive()
{
    char const* spell = nullptr;
    return FactionChampionsPurgeTarget(botAI, spell) != nullptr;
}

void AddToCFactionChampionsDefenceTriggerNodes(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("faction champions avoid aoe", {
        NextAction("faction champions avoid aoe", ACTION_EMERGENCY + 6) }));

    triggers.push_back(new TriggerNode("faction champions mass dispel", {
        NextAction("faction champions mass dispel", ACTION_RAID + 5) }));

    triggers.push_back(new TriggerNode("faction champions dispel cc", {
        NextAction("faction champions dispel cc", ACTION_RAID + 3.5f) }));

    triggers.push_back(new TriggerNode("faction champions physical switch", {
        NextAction("faction champions physical switch", ACTION_RAID + 2.5f) }));

    triggers.push_back(new TriggerNode("faction champions purge kill target", {
        NextAction("faction champions purge kill target", ACTION_RAID) }));
}
