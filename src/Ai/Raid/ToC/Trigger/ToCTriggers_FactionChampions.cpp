#include "ToCTriggers_FactionChampions.h"
#include "ToCData.h"
#include "ToCHelpers_FactionChampions.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "Strategy.h"

using namespace TrialOfTheCrusaderHelpers;

bool FactionChampionsMarkTargetsTrigger::IsActive()
{
    return EncounterHelpers::IsMechanicTrackerBot(bot, TRIAL_OF_THE_CRUSADER_MAP_ID) &&
           FactionChampionsMarksPending(botAI);
}

bool FactionChampionsAntiFearTrigger::FearWindowActive() { return FactionChampionsFearWindowActive(botAI); }

bool FactionChampionsShouldFocusTrigger::IsActive()
{
    if (!FactionChampionsFocusBot(botAI))
        return false;

    Unit* killTarget = FactionChampionsKillTarget(botAI);
    return killTarget && AI_VALUE(Unit*, "current target") != killTarget;
}

bool FactionChampionsCcIconTrigger::IsActive() { return FactionChampionsCcIconPending(botAI); }

bool FactionChampionsKillTargetHealingTrigger::IsActive() { return FactionChampionsCounterspellDuty(botAI); }

bool ToCRestoreRtiCcTrigger::IsActive() { return FactionChampionsRtiCcRestorePending(botAI); }

void AddToCFactionChampionsTriggerNodes(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("faction champions mark targets", {
        NextAction("faction champions mark targets", ACTION_RAID + 4) }));

    triggers.push_back(new TriggerNode("faction champions anti fear", {
        NextAction("faction champions anti fear", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode("faction champions should focus", {
        NextAction("faction champions focus priority", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode("faction champions cc icon", {
        NextAction("faction champions set cc icon", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("faction champions kill target healing", {
        NextAction("faction champions counterspell kill target", ACTION_INTERRUPT + 3) }));

    triggers.push_back(new TriggerNode("toc restore rti cc", {
        NextAction("toc restore rti cc", ACTION_RAID + 1) }));
}
