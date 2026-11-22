#include "ToCDefinitions.h"

#include "PlayerbotAI.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Jormungars.h"
#include "ToCHelpers_NorthrendBeasts.h"
#include "ToCMultipliers_NorthrendBeasts.h"

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

bool WormEngaged(PlayerbotAI* botAI) { return (GetBeastsStageMask(botAI) & BEASTS_STAGE_WORMS) != 0; }

void DefineNorthrendBeasts(EncounterBuilder& e)
{
    e.LiveMultiplier<NorthrendBeastsControlTankMovementMultiplier>(Family::AnyMovement);
    e.LiveMultiplier<NorthrendBeastsTauntGuardMultiplier>(Family::Spell);

    e.OwnTargeting("northrend beasts dps target guard", Role::Any, BeastsDpsTargetOwned, Family::DpsAssist);
    e.OwnTargeting("northrend beasts tank target guard", Role::Any, BeastsTankTargetOwned, Family::TankAssist);

    e.Block("northrend worms spot hold", Role::Any, IsWormSpotHeld,
            Family::SetBehind | Family::RearFlank | Family::CombatFormationMove | Family::Follow | Family::Flee |
                Family::RunAway | Family::MoveRandom | Family::Charge | Family::Blink | Family::Disengage,
            {"reach melee", "reach spell"});
    e.Block("northrend worms pool reach guard", Role::Any, WormReachCrossesPool, 0, {"reach melee", "reach spell"});
    e.Block("northrend worms spew reach guard", Role::Any, WormReachIntoSpew, Family::Charge,
            {"reach melee", "reach spell"});
    e.Block("northrend worms follow guard", Role::Any, WormEngaged, Family::Follow);
}
}

EncounterDefinition const& ToCNorthrendBeastsDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::NorthrendBeasts), ToCGate,
                                                "northrend-beasts", &DefineNorthrendBeasts);
    return definition;
}
