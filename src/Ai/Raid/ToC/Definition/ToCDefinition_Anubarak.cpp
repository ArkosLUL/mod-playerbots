#include "ToCDefinitions.h"

#include "AiObjectContext.h"
#include "Creature.h"
#include "PlayerbotAI.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Anubarak.h"
#include "ToCMultipliers_Anubarak.h"

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

// An untouched sphere is a critter out of combat, which invalid target drops 0.2 s after every Attack()
bool AnubarakShooterOnDutySphere(PlayerbotAI* botAI)
{
    if (!AnubarakEngaged(botAI))
        return false;

    Creature* sphere = GetAnubarakSphereToShoot(botAI->GetBot());
    return sphere && IsFrostSphereFlying(sphere) &&
           botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get() == sphere;
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

    // reach goes by name, so the walk to a battle res isn't blocked
    e.Block("anubarak spike dodge hold", Role::Any, IsAnubarakDodgeHeld,
            Family::SetBehind | Family::RearFlank | Family::CombatFormationMove | Family::Follow | Family::Flee |
                Family::RunAway | Family::MoveRandom | Family::Charge | Family::Blink | Family::Disengage,
            {"reach melee", "reach spell"});
    // any other walk replaces the leg's, and he stops wherever the tank stops
    e.Block("anubarak boss drag hold", Role::Any, IsAnubarakDragInFlight,
            Family::SetBehind | Family::TankFace | Family::CombatFormationMove | Family::Follow | Family::Charge,
            {"reach melee"});
    // he obeys taunt DR, so two tanks trading taunts soon leave him immune to both
    e.Block("anubarak taunt guard", Role::Any, AnubarakTauntWouldSteal, Family::Taunt);
    e.Block("anubarak sphere shooter drop guard", Role::Any, AnubarakShooterOnDutySphere, Family::DropTarget);

    e.Multiplier<AnubarakThreatCapMultiplier>(Family::Spell | Family::Melee);
    e.Multiplier<AnubarakSphereGuardMultiplier>(Family::Spell | Family::Attack);
}
}  // namespace

EncounterDefinition const& ToCAnubarakDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::Anubarak), ToCGate,
                                                "anub-arak", &DefineAnubarak);
    return definition;
}
