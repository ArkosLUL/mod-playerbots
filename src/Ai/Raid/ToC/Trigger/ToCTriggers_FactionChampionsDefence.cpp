#include "ToCTriggers_FactionChampionsDefence.h"
#include "ToCHelpers_FactionChampions.h"
#include "ToCHelpers_FactionChampionsDefence.h"
#include "ToCHelpers_Shared.h"
#include "Playerbots.h"
#include "Strategy.h"

using namespace TrialOfTheCrusaderHelpers;

bool FactionChampionsCrowdControlTrigger::IsActive()
{
    uint32 spellId = 0;
    return FactionChampionsCcTarget(botAI, spellId) != nullptr;
}

bool FactionChampionsPeelTrigger::IsActive()
{
    uint32 spellId = 0;
    return FactionChampionsPeelTarget(botAI, spellId) != nullptr;
}

bool FactionChampionsAvoidAoeTrigger::IsActive() { return FactionChampionsInAoe(botAI); }

bool FactionChampionsMassDispelTrigger::IsActive() { return FactionChampionsMassDispelTarget(botAI) != nullptr; }

bool FactionChampionsDispelCcTrigger::IsActive()
{
    char const* spell = nullptr;
    return FactionChampionsDispelCcTarget(botAI, spell) != nullptr;
}

bool FactionChampionsKillTotemTrigger::IsActive()
{
    Unit* totem = FactionChampionsTotemTarget(botAI);
    return totem && (AI_VALUE(Unit*, "current target") != totem || ToCPetOffPick(botAI, totem));
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

bool FactionChampionsDeathGripTrigger::IsActive()
{
    Position stand;
    return FactionChampionsGripTarget(botAI, stand) != nullptr;
}

bool FactionChampionsChainGripTrigger::IsActive() { return FactionChampionsOpenerGripDue(botAI); }

bool FactionChampionsSnareTrigger::IsActive()
{
    uint32 spellId = 0;
    return FactionChampionsSnareTarget(botAI, spellId) != nullptr;
}

bool FactionChampionsProtectTrigger::IsActive()
{
    uint32 spellId = 0;
    return FactionChampionsProtectTarget(botAI, spellId) != nullptr;
}

bool FactionChampionsBattleResTrigger::IsActive()
{
    uint32 spellId = 0;
    return FactionChampionsBattleResTarget(botAI, spellId) != nullptr;
}

void AddToCFactionChampionsDefenceTriggerNodes(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("faction champions avoid aoe", {
        NextAction("faction champions avoid aoe", ACTION_EMERGENCY + 6) }));

    triggers.push_back(new TriggerNode("faction champions protect", {
        NextAction("faction champions protect", ACTION_EMERGENCY + 4) }));

    // over the pack tank nodes, so the DK tank leaves the pack for it
    triggers.push_back(new TriggerNode("faction champions chain grip", {
        NextAction("faction champions chain grip", ACTION_RAID + 4.6f) }));

    triggers.push_back(new TriggerNode("faction champions death grip", {
        NextAction("faction champions death grip", ACTION_RAID + 4.5f) }));

    triggers.push_back(new TriggerNode("faction champions battle res", {
        NextAction("faction champions battle res", ACTION_RAID + 4.2f) }));

    triggers.push_back(new TriggerNode("faction champions mass dispel", {
        NextAction("faction champions mass dispel", ACTION_RAID + 5) }));

    triggers.push_back(new TriggerNode("faction champions dispel cc", {
        NextAction("faction champions dispel cc", ACTION_RAID + 3.5f) }));

    triggers.push_back(new TriggerNode("faction champions crowd control", {
        NextAction("faction champions crowd control", ACTION_RAID + 3.4f) }));

    triggers.push_back(new TriggerNode("faction champions peel", {
        NextAction("faction champions peel", ACTION_RAID + 3.3f) }));

    triggers.push_back(new TriggerNode("faction champions snare", {
        NextAction("faction champions snare", ACTION_RAID + 3.2f) }));

    triggers.push_back(new TriggerNode("faction champions kill totem", {
        NextAction("faction champions kill totem", ACTION_RAID + 2.7f) }));

    triggers.push_back(new TriggerNode("faction champions physical switch", {
        NextAction("faction champions physical switch", ACTION_RAID + 2.5f) }));

    triggers.push_back(new TriggerNode("faction champions purge kill target", {
        NextAction("faction champions purge kill target", ACTION_RAID) }));
}
