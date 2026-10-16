#include "ToCDefinitions.h"

#include "PlayerbotAI.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_NorthrendBeasts.h"

using namespace TrialOfTheCrusaderHelpers;

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
bool BeastsDpsTargetOwned(PlayerbotAI* botAI) { return GetBeastsDpsTarget(botAI).unit != nullptr; }

bool BeastsTankTargetOwned(PlayerbotAI* botAI)
{
    BeastsTankDuty const duty = GetBeastsTankDuty(botAI);
    return duty != BeastsTankDuty::None && GetBeastOfDuty(botAI, duty) != nullptr;
}

void DefineNorthrendBeasts(EncounterBuilder& e)
{
    e.OwnTargeting("northrend beasts dps target guard", Role::Any, BeastsDpsTargetOwned, Family::DpsAssist);
    // a tank with no beast up for its duty keeps tank assist, the only target source it has then
    e.OwnTargeting("northrend beasts tank target guard", Role::Any, BeastsTankTargetOwned, Family::TankAssist);
}
}  // namespace

EncounterDefinition const& ToCNorthrendBeastsDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::NorthrendBeasts), ToCGate,
                                                "northrend-beasts", &DefineNorthrendBeasts);
    return definition;
}
