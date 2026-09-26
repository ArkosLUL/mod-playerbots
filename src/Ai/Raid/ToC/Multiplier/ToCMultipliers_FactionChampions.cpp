#include "ToCMultipliers_FactionChampions.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_FactionChampions.h"
#include "ChooseTargetActions.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "Playerbots.h"
#include "RogueActions.h"

using namespace TrialOfTheCrusaderHelpers;

float FactionChampionsSuppressAoeMultiplier::GetValue(Action* action)
{
    // AoE heals land on the raid, not the champions, so they stay
    if (!action || action->getThreatType() != Action::ActionThreatType::Aoe ||
        dynamic_cast<CastHealingSpellAction*>(action))
    {
        return 1.0f;
    }

    return ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions) ? 0.0f : 1.0f;
}

float FactionChampionsTargetGuardMultiplier::GetValue(Action* action)
{
    // Assist makes its own pick and flips bots off the kill target. Tank assist even leaves the skull
    // once a tank, itself included, holds it.
    if (!dynamic_cast<DpsAssistAction*>(action) && !dynamic_cast<TankAssistAction*>(action))
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return 1.0f;

    if (!FactionChampionsFocusBot(botAI))
        return 1.0f;

    return FactionChampionsKillTarget(botAI) ? 0.0f : 1.0f;
}

float FactionChampionsThreatRedirectVetoMultiplier::GetValue(Action* action)
{
    bool const mainTankRedirect = dynamic_cast<CastMisdirectionOnMainTankAction*>(action) ||
                                  dynamic_cast<CastTricksOfTheTradeOnMainTankAction*>(action);
    if (!mainTankRedirect && !dynamic_cast<CastTricksOfTheTradeAction*>(action))
        return 1.0f;

    if (!ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return 1.0f;

    if (mainTankRedirect)
        return 0.0f;

    // Tricks on a melee dps still gives its 15% damage, so only veto it when it picks the tank
    Unit* mainTank = AI_VALUE(Unit*, "main tank");
    return mainTank && AI_VALUE(Unit*, "tricks of the trade target") == mainTank ? 0.0f : 1.0f;
}

bool FactionChampionsAntiFearTotemGuardMultiplier::FearWindowActive()
{
    return FactionChampionsFearWindowActive(botAI);
}

void AddToCFactionChampionsMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new FactionChampionsSuppressAoeMultiplier(botAI));
    multipliers.push_back(new FactionChampionsTargetGuardMultiplier(botAI));
    multipliers.push_back(new FactionChampionsThreatRedirectVetoMultiplier(botAI));
    multipliers.push_back(new FactionChampionsAntiFearTotemGuardMultiplier(botAI));
}

// Lust and every cooldown go at the pull, each early kill makes the rest easier
ToCBurstWindow ToCFactionChampionsBurstWindow(PlayerbotAI* /*botAI*/)
{
    return {true, true};
}
