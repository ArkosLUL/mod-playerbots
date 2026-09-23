#include "ToCMultipliers_FactionChampions.h"
#include "ToCEncounterGate.h"
#include "GenericSpellActions.h"
#include "Playerbots.h"

float FactionChampionsSuppressAoeMultiplier::GetValue(Action* action)
{
    // Champions stack a damage-reduction aura when several are hit by the same AoE, so bots single-
    // target. AoE heals are exempt so healers can still raid-heal through the pile.
    if (!action || action->getThreatType() != Action::ActionThreatType::Aoe ||
        dynamic_cast<CastHealingSpellAction*>(action))
    {
        return 1.0f;
    }

    return ToCEncounterIsLive(botAI, ToCEncounter::FactionChampions) ? 0.0f : 1.0f;
}

void AddToCFactionChampionsMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new FactionChampionsSuppressAoeMultiplier(botAI));
}

ToCBurstWindow ToCFactionChampionsBurstWindow(PlayerbotAI* /*botAI*/)
{
    return {};
}
