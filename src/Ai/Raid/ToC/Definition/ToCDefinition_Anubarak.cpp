#include "ToCDefinitions.h"

#include "AiObjectContext.h"
#include "Creature.h"
#include "Map.h"
#include "Player.h"
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

// The whole pull: a side tank on him or on a scarab when a wave lands leaves the wave on the healers.
// The ready node holds it on its patch.
bool AnubarakSideTankHolds(PlayerbotAI* botAI)
{
    return AnubarakEngaged(botAI) && GetAnubarakBurrowerTankSide(botAI->GetBot()) >= 0;
}

// A flying sphere is out of combat, so never the dps target, and assist would take the shooter off it
// every tick. A DK waiting on its grip spot keeps assist.
bool AnubarakSphereShooterHolds(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!AnubarakEngaged(botAI))
        return false;

    AnubarakGripDuty grip;
    return (PlayerbotAI::IsRangedDps(bot) && GetAnubarakSphereToShoot(bot)) ||
           (GetAnubarakGripDuty(bot, grip) && grip.step != AnubarakGripStep::Hold);
}

// An untouched sphere is a critter out of combat, which invalid target drops 0.2 s after every Attack()
bool AnubarakShooterOnDutySphere(PlayerbotAI* botAI)
{
    if (!AnubarakEngaged(botAI))
        return false;

    Creature* sphere = GetAnubarakDutySphere(botAI->GetBot());
    return sphere && IsFrostSphereFlying(sphere) &&
           botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get() == sphere;
}

// Walking off the spot to reach him moves where the gripped sphere lands
bool AnubarakGripHolds(PlayerbotAI* botAI)
{
    AnubarakGripDuty grip;
    return AnubarakEngaged(botAI) && GetAnubarakGripDuty(botAI->GetBot(), grip) &&
           grip.step != AnubarakGripStep::Kill;
}

// Tank face walks the tank round him, through whichever patch lies on the way
bool AnubarakPickupTankStands(PlayerbotAI* botAI)
{
    return AnubarakPickupTankHolds(botAI) && AnubarakTankPatchLatched(botAI->GetBot());
}

// The DK rotation spends Death Grip on a loose burrower, and the tank patch waits out its cooldown
bool AnubarakGripOwed(PlayerbotAI* botAI)
{
    AnubarakGripDuty grip;
    return AnubarakEngaged(botAI) && GetAnubarakGripDuty(botAI->GetBot(), grip);
}

// Heroic phase 2: the main tank leaves the scarabs on others, so Acid-Drenched Mandibles spreads and he
// comes back up without a stack on him
bool AnubarakMainTankLeavesScarabs(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return AnubarakEngaged(botAI) && bot->GetMap()->IsHeroic() && IsAnubarakPickupTank(bot) &&
           GetAnubarakPhase(botAI) == AnubarakPhase::Submerged;
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
    e.OwnTargeting("anubarak main tank scarab guard", Role::Any, AnubarakMainTankLeavesScarabs, Family::TankAssist);

    // reach goes by name, so the walk to a battle res isn't blocked
    e.Block("anubarak spike dodge hold", Role::Any, IsAnubarakDodgeHeld,
            Family::SetBehind | Family::RearFlank | Family::CombatFormationMove | Family::Follow | Family::Flee |
                Family::RunAway | Family::MoveRandom | Family::Charge | Family::Blink | Family::Disengage,
            {"reach melee", "reach spell"});
    // any other walk replaces the leg's, and he stops wherever the tank stops
    e.Block("anubarak boss drag hold", Role::Any, IsAnubarakDragInFlight,
            Family::SetBehind | Family::TankFace | Family::CombatFormationMove | Family::Follow | Family::Charge,
            {"reach melee"});
    e.Block("anubarak grip spot hold", Role::Any, AnubarakGripHolds,
            Family::SetBehind | Family::RearFlank | Family::CombatFormationMove | Family::Follow | Family::Charge,
            {"reach melee"});
    e.Block("anubarak pickup tank stand guard", Role::Any, AnubarakPickupTankStands, Family::TankFace);
    // the melee stand walks it off the patch, and behind a held burrower is its patch
    e.Block("anubarak melee burrower stand hold", Role::Any, AnubarakMeleeBurrowerStandApplies,
            Family::SetBehind | Family::RearFlank, {"reach melee"});
    // heroic phase 2: the raid holds its gather, so no walk takes anyone across the spike's run
    e.Block("anubarak gather hold", Role::Any, AnubarakGathers,
            Family::SetBehind | Family::RearFlank | Family::CombatFormationMove | Family::Follow | Family::Charge,
            {"reach melee", "reach spell", "reach party member to heal"});
    // healers hold their stand while he is up; reach still walks one to a heal target out of range
    e.Block("anubarak healer stand hold", Role::Heal, AnubarakHealerStandApplies,
            Family::CombatFormationMove | Family::Follow);
    // the escort keeps the kiter in Hand of Protection's reach, whatever its target or heal target
    e.Block("anubarak hop escort hold", Role::Any, IsAnubarakEscort,
            Family::SetBehind | Family::RearFlank | Family::CombatFormationMove | Family::Follow | Family::Charge,
            {"reach melee", "reach spell", "reach party member to heal"});
    e.Block("anubarak main tank scarab taunt guard", Role::Any, AnubarakMainTankLeavesScarabs, Family::Taunt);
    // the class triggers send Misdirection to the main tank and Tricks to a melee DPS, so a burrower's
    // threat goes to whoever hits it; the encounter's redirect sends both to the side tanks
    e.Block("anubarak redirect guard", Role::Any, AnubarakEngaged, 0,
            {"misdirection on main tank", "tricks of the trade", "tricks of the trade on main tank"});
    // he obeys taunt DR, so two tanks trading taunts soon leave him immune to both
    e.Block("anubarak taunt guard", Role::Any, AnubarakTauntWouldSteal, Family::Taunt);
    e.Block("anubarak sphere shooter drop guard", Role::Any, AnubarakShooterOnDutySphere, Family::DropTarget);
    e.Block("anubarak grip reserve", Role::Any, AnubarakGripOwed, 0, {"death grip"});
    // the pickup casts its grip directly; the rotation's would yank a burrower the tank just taunted
    e.Block("anubarak side tank grip reserve", Role::Any, AnubarakSideTankHolds, 0, {"death grip"});
    // the class chain's Righteous Defense off the main tank taunts him too; the pickup casts its own
    e.Block("anubarak side tank righteous defense reserve", Role::Any, AnubarakSideTankHolds, 0,
            {"righteous defense"});
    // the ghouls taunt burrowers off their side tanks and off the patches, where they submerge to full
    e.Block("anubarak no ghouls", Role::Any, AnubarakEngaged, 0, {"army of the dead"});

    e.Multiplier<AnubarakThreatCapMultiplier>(Family::Spell | Family::Melee);
    e.Multiplier<AnubarakSphereGuardMultiplier>(Family::Spell | Family::Attack);
    e.Multiplier<AnubarakBurrowerGuardMultiplier>(Family::Spell | Family::Attack);
}
}  // namespace

EncounterDefinition const& ToCAnubarakDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::Anubarak), ToCGate,
                                                "anub-arak", &DefineAnubarak);
    return definition;
}
