#include "ToCDefinitions.h"

#include "PlayerbotAI.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_FactionChampions.h"

using namespace TrialOfTheCrusaderHelpers;

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
// The focus trigger's test, so the rule and the node cover the same window
bool FactionChampionsFocusOwned(PlayerbotAI* botAI)
{
    return FactionChampionsFocusBot(botAI) && FactionChampionsKillTarget(botAI);
}

void DefineFactionChampions(EncounterBuilder& e)
{
    // Assist makes its own pick, so on any tick the settled focus node returns false it would take the
    // bot off the kill target. The name is what the reader counts vetoes under.
    e.OwnTargeting("faction champions target guard multiplier", Role::Any, FactionChampionsFocusOwned,
                   Family::DpsAssist | Family::TankAssist);
}
}  // namespace

EncounterDefinition const& ToCFactionChampionsDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::FactionChampions), ToCGate,
                                                "faction-champions", &DefineFactionChampions);
    return definition;
}
