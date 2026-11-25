#include "ToCDefinitions.h"

#include "PlayerbotAI.h"
#include "Strategy.h"
#include "ToCActions_Staging.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Staging.h"
#include "ToCTriggers_Staging.h"

using namespace TrialOfTheCrusaderHelpers;

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
void DefineStaging(EncounterBuilder& e)
{
    e.Node<ToCStagingStandTrigger, ToCMoveToStagingStandAction>(ACTION_RAID);

    e.Block("toc staging hold", Role::Any, IsToCStagingHeld,
            Family::Follow | Family::CombatFormationMove | Family::SetBehind | Family::TankFace | Family::RearFlank |
                Family::MoveRandom | Family::TankAssist | Family::DpsAssist,
            {"return to stay position", "return"});
    e.Block("toc staging rest guard", Role::Any, IsToCStagingRestHeld, 0, {"eat", "drink"});
    e.Block("toc staging tank form guard", Role::Tank, IsToCStagingHeld, 0, {"caster form"});
    e.Block("toc staging ritual guard", Role::Any, IsToCStagingHeld, 0,
            {"ritual of souls", "join ritual of souls", "use soulwell"});
}
}

EncounterDefinition const& ToCStagingDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::None), ToCRaidWideGate, nullptr,
                                                &DefineStaging);
    return definition;
}
