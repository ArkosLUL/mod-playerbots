#include "ToCDefinitions.h"

#include "PlayerbotAI.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Anubarak.h"

using namespace TrialOfTheCrusaderHelpers;

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
bool AnubarakDpsTargetOwned(PlayerbotAI* botAI) { return GetAnubarakDpsTarget(botAI) != nullptr; }

// The boss hold trigger's test
bool AnubarakPickupTankHolds(PlayerbotAI* botAI)
{
    if (!AnubarakEngaged(botAI) || !IsAnubarakPickupTank(botAI->GetBot()))
        return false;

    AnubarakPhase const phase = GetAnubarakPhase(botAI);
    return phase == AnubarakPhase::Surface || phase == AnubarakPhase::Swarm;
}

// Only while the hold node has one for it: a side tank with nothing to take keeps tank assist
bool AnubarakSideTankHolds(PlayerbotAI* botAI)
{
    return AnubarakEngaged(botAI) && GetAnubarakBurrowerPick(botAI);
}

// A flying sphere is out of combat, so never the dps target, and assist would take the shooter off it
// every tick
bool AnubarakSphereShooterHolds(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return AnubarakEngaged(botAI) && PlayerbotAI::IsRangedDps(bot) && GetAnubarakSphereToShoot(bot);
}

void DefineAnubarak(EncounterBuilder& e)
{
    e.OwnTargeting("anubarak dps target guard", Role::Any, AnubarakDpsTargetOwned, Family::DpsAssist);
    e.OwnTargeting("anubarak pickup tank target guard", Role::Any, AnubarakPickupTankHolds,
                   Family::DpsAssist | Family::TankAssist);
    e.OwnTargeting("anubarak side tank target guard", Role::Any, AnubarakSideTankHolds,
                   Family::DpsAssist | Family::TankAssist);
    e.OwnTargeting("anubarak sphere shooter target guard", Role::Any, AnubarakSphereShooterHolds,
                   Family::DpsAssist);
}
}  // namespace

EncounterDefinition const& ToCAnubarakDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::Anubarak), ToCGate,
                                                "anub-arak", &DefineAnubarak);
    return definition;
}
