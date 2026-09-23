#include "ToCActions_FactionChampions.h"
#include "ToCData.h"
#include "ToCHelpers_FactionChampions.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

bool FactionChampionsFocusPriorityAction::Execute(Event /*event*/)
{
    Unit* priority = GetPriorityFactionChampion(botAI);
    if (!priority)
        return false;

    // One designated bot owns the raid markers so they do not flicker between bots: skull on the kill
    // target (the shared focus mark), moon on a second healer for the per-class "cc" strategy to lock.
    if (IsMechanicTrackerBot(bot, TRIAL_OF_THE_CRUSADER_MAP_ID))
    {
        MarkTargetWithSkull(bot, priority);
        SetRtiTarget(botAI, "skull", priority);

        if (Unit* ccHealer = GetCcFactionChampionHealer(botAI, priority))
            SetRtiCcTarget(botAI, "moon", ccHealer);
    }

    if (AI_VALUE(Unit*, "current target") != priority)
        return Attack(priority);

    return false;
}
