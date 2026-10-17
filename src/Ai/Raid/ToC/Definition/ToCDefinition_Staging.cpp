#include "ToCDefinitions.h"

#include "PlayerbotAI.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Staging.h"

using namespace TrialOfTheCrusaderHelpers;

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
void DefineStaging(EncounterBuilder& e)
{
    // follow and stay walk a parked bot back to its player. out of combat tank and dps assist pull on a
    // skull, and Anub'arak turns attackable once anyone is inside 80 yd
    e.Block("toc staging hold", Role::Any, IsToCStagingHeld,
            Family::Follow | Family::CombatFormationMove | Family::SetBehind | Family::TankFace | Family::RearFlank |
                Family::MoveRandom | Family::TankAssist | Family::DpsAssist,
            {"return to stay position", "return"});
    // eating blinds a bot for 12-18 s
    e.Block("toc staging rest guard", Role::Any, IsToCStagingRestHeld, 0, {"food", "drink"});
    // a druid tank drops bear form out of combat and nothing shifts him back
    e.Block("toc staging tank form guard", Role::Tank, IsToCStagingHeld, 0, {"caster form"});
}
}  // namespace

EncounterDefinition const& ToCStagingDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::None), ToCRaidWideGate, nullptr,
                                                &DefineStaging);
    return definition;
}
