/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_SindragosaAdds.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_SindragosaAdds.h"
#include "ICCMultipliers_SindragosaAdds.h"
#include "ICCSeams.h"
#include "ICCTriggers_SindragosaAdds.h"
#include "RaidTankDefensive.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
void DefineSindragosaDrakes(EncounterBuilder& e)
{
    e.Node<IccSgDrakeDodgeTrigger, IccSgDrakeDodgeAction>(ACTION_EMERGENCY + 6, EncounterRow::Mover);
    e.Node<IccSgDrakeTankDefensiveTrigger, IccSgDrakeCastTankDefensiveAction>(ACTION_RAID + 9);
    e.Node<IccSgDrakeAntiFearTrigger, IccSgDrakeAntiFearAction>(ACTION_RAID + 8);
    e.Node<IccSgDrakeBrakeTrigger, IccSgDrakeBrakeStopAction>(ACTION_RAID + 5);
    // Above the stand: a bot takes a new pick before it walks back to its spot
    e.Node<IccSgDrakeTargetTrigger, IccSgDrakeTargetAction>(ACTION_RAID + 4);
    e.Node<IccSgDrakeStandTrigger, IccSgDrakeStandAction>(ACTION_RAID + 3, EncounterRow::Mover);

    e.LiveMultiplier<IccSgDrakeBrakeMultiplier>(Family::Spell | Family::Melee);
    // The earth totem slot holds one totem, so Tremor stays down only while the shaman's others are held
    e.LiveMultiplier<IccSgDrakeAntiFearTotemGuardMultiplier>(Family::Spell);

    e.OwnTargeting("icc sindragosa drake targeting", Role::Any, &IccResolverPicks<&GetSgDrakeTarget>,
                   Family::DpsAssist | Family::TankAssist | Family::DpsAoe);
    // A braked bot's resolver hands it nothing, so no picker may give it the drake back, and with no target
    // nothing may walk it off to its master
    e.Block("icc sindragosa drake brake hold", Role::Any, &IsSgDrakeBraked,
            Family::DpsAssist | Family::TankAssist | Family::DpsAoe | Family::AggressiveTarget |
                Family::AttackAnything | Family::AttackLeastHp | Family::AttackRti | Family::Follow |
                Family::MoveRandom);
    // Every stand is in reach of its drake, and reach would walk a bot off it while the drake stays put
    e.OwnMovement("icc sindragosa drake stand hold", Role::Any, &IsSgDrakeHoldingStand, Family::Attack,
                  {IccSgDrakeStandAction::Name, IccSgDrakeDodgeAction::Name, "reach party member to resurrect"});
    e.Block("icc sindragosa drake spell movers", Role::Any, &IsSgDrakeMovementOwned,
            Family::Charge | Family::Blink | Family::Disengage);
    // avoid aoe flees a puddle a few yards at most, against the dodge
    e.Block("icc sindragosa drake avoid aoe", Role::Any, &IsSgDrakesLive, Family::AvoidAoe);
    // A tank holding no drake would taunt one off its holder and turn its breath on the raid
    e.Block("icc sindragosa drake taunts", Role::Tank, &IsSgDrakeTauntHeld, Family::Taunt);
    // A braked tank has no target, and a druid's non-combat nodes drop bear form
    e.Block("icc sindragosa drake tank form", Role::Tank, &IsSgDrakesLive, 0, {"caster form"});
    // The tank buttons wait for a low tank or Rimefang's Frost Breath
    e.Block("icc sindragosa drake tank defensive hold", Role::Tank, &IsSgDrakeTankDefensiveHeld, 0,
            HeldTankDefensiveNames());

    e.Tick(&TickSgDrakesEncounter);
}
}  // namespace

EncounterDefinition const& IccSindragosaDrakesDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_NONE,
                                                UnitGate<NPC_SG_DRAKE_RIMEFANG, NPC_SG_DRAKE_SPINESTALKER>,
                                                "sindragosa", &DefineSindragosaDrakes);
    return definition;
}
