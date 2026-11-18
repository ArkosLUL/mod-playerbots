#include "ToCTriggers_FactionChampions.h"
#include "ToCHelpers_FactionChampions.h"
#include "ToCHelpers_FactionChampionsDefence.h"
#include "Playerbots.h"
#include "RaidDirectTargeting.h"

using namespace TrialOfTheCrusaderHelpers;

bool FactionChampionsAntiFearTrigger::FearWindowActive() { return FactionChampionsFearWindowActive(botAI); }

bool FactionChampionsShouldFocusTrigger::IsActive()
{
    if (!FactionChampionsFocusBot(botAI))
        return false;

    Unit* killTarget = FactionChampionsKillTarget(botAI);
    return killTarget &&
           (AI_VALUE(Unit*, "current target") != killTarget || RaidPetOffPick(botAI, killTarget));
}

bool FactionChampionsKillTargetHealingTrigger::IsActive() { return FactionChampionsCounterspellDuty(botAI); }

bool FactionChampionsPetAutocastTrigger::IsActive()
{
    bool enable = false;
    return FactionChampionsPetAutocastToggle(botAI, enable) != 0;
}
