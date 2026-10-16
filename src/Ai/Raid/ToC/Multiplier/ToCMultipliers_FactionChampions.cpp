#include "ToCMultipliers_FactionChampions.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_FactionChampions.h"
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

float FactionChampionsCcGuardMultiplier::GetValue(Action* action)
{
    CastCrowdControlSpellAction* cc = dynamic_cast<CastCrowdControlSpellAction*>(action);
    if (!cc || !ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions))
        return 1.0f;

    uint8 icon = 0;
    Unit* assigned = FactionChampionsCcTarget(botAI, icon);
    // GetTarget only after the assignment test, Polymorph's cc target scans every attacker
    if (!assigned || action->GetTarget() != assigned)
        return 0.0f;

    uint32 const spellId = AI_VALUE2(uint32, "spell id", cc->getSpell());
    return FactionChampionsCcCastPays(botAI, assigned, spellId) ? 1.0f : 0.0f;
}

void AddToCFactionChampionsMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new FactionChampionsSuppressAoeMultiplier(botAI));
    multipliers.push_back(new FactionChampionsThreatRedirectVetoMultiplier(botAI));
    multipliers.push_back(new FactionChampionsAntiFearTotemGuardMultiplier(botAI));
    multipliers.push_back(new FactionChampionsCcGuardMultiplier(botAI));
}

// Lust and every cooldown go at the pull, each early kill makes the rest easier
ToCBurstWindow ToCFactionChampionsBurstWindow(PlayerbotAI* /*botAI*/)
{
    return {true, true};
}
