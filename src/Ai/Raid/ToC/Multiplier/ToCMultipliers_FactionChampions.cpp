#include "ToCMultipliers_FactionChampions.h"
#include "ToCHelpers_FactionChampions.h"
#include "GenericSpellActions.h"
#include "Playerbots.h"

using namespace TrialOfTheCrusaderHelpers;

float FactionChampionsSuppressAoeMultiplier::GetValue(Action* action)
{
    // Only gate AoE while champions are alive; the other ToC bosses share this strategy and must keep
    // their AoE (e.g. Northrend Beasts add waves, Anub'arak burrowers/scarabs).
    if (!action || !FactionChampionsEncounterActive(botAI))
        return 1.0f;

    // Champions stack a damage-reduction aura when several are hit by the same AoE, so bots single-
    // target. AoE heals are exempt so healers can still raid-heal through the pile.
    if (action->getThreatType() == Action::ActionThreatType::Aoe && !dynamic_cast<CastHealingSpellAction*>(action))
        return 0.0f;

    return 1.0f;
}

void AddToCFactionChampionsMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new FactionChampionsSuppressAoeMultiplier(botAI));
}

ToCBurstWindow ToCFactionChampionsBurstWindow(PlayerbotAI* /*botAI*/)
{
    return {};
}
