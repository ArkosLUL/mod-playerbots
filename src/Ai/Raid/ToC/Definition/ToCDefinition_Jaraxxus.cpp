#include "ToCDefinitions.h"

#include "PlayerbotAI.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Jaraxxus.h"

using namespace TrialOfTheCrusaderHelpers;

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
bool JaraxxusDpsTargetOwned(PlayerbotAI* botAI) { return GetJaraxxusDpsTarget(botAI) != nullptr; }

bool JaraxxusTankTargetOwned(PlayerbotAI* botAI)
{
    return JaraxxusMainTankHolds(botAI) || JaraxxusAssistTankHolds(botAI, 0) || JaraxxusAssistTankHolds(botAI, 1);
}

void DefineJaraxxus(EncounterBuilder& e)
{
    e.OwnTargeting("jaraxxus dps target guard", Role::Any, JaraxxusDpsTargetOwned, Family::DpsAssist);
    // a tank between holds keeps tank assist, the only target source it has then
    e.OwnTargeting("jaraxxus tank target guard", Role::Any, JaraxxusTankTargetOwned, Family::TankAssist);
}
}  // namespace

EncounterDefinition const& ToCJaraxxusDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::Jaraxxus), ToCGate,
                                                "lord-jaraxxus", &DefineJaraxxus);
    return definition;
}
