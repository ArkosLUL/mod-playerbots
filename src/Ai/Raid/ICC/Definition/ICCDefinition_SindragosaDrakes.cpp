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
    e.Node<IccSgDrakeTargetTrigger, IccSgDrakeTargetAction>(ACTION_RAID + 4);
    e.Node<IccSgDrakeStandTrigger, IccSgDrakeStandAction>(ACTION_RAID + 3, EncounterRow::Mover);

    e.LiveMultiplier<IccSgDrakeBrakeMultiplier>(Family::Spell | Family::Melee);
    e.LiveMultiplier<IccSgDrakeAntiFearTotemGuardMultiplier>(Family::Spell);

    e.OwnTargeting("icc sindragosa drake targeting", Role::Any, &IccResolverPicks<&GetSgDrakeTarget>,
                   Family::DpsAssist | Family::TankAssist | Family::DpsAoe);
    e.Block("icc sindragosa drake brake hold", Role::Any, &IsSgDrakeBraked,
            Family::DpsAssist | Family::TankAssist | Family::DpsAoe | Family::AggressiveTarget |
                Family::AttackAnything | Family::AttackLeastHp | Family::AttackRti | Family::Follow |
                Family::MoveRandom);
    e.OwnMovement("icc sindragosa drake stand hold", Role::Any, &IsSgDrakeHoldingStand, Family::Attack,
                  {IccSgDrakeStandAction::Name, IccSgDrakeDodgeAction::Name, "reach party member to resurrect"});
    e.Block("icc sindragosa drake spell movers", Role::Any, &IsSgDrakeMovementOwned,
            Family::Charge | Family::Blink | Family::Disengage);
    e.Block("icc sindragosa drake avoid aoe", Role::Any, &IsSgDrakesLive, Family::AvoidAoe);
    e.Block("icc sindragosa drake taunts", Role::Tank, &IsSgDrakeTauntHeld, Family::Taunt);
    e.Block("icc sindragosa drake tank form", Role::Tank, &IsSgDrakesLive, 0, {"caster form"});
    e.Block("icc sindragosa drake tank defensive hold", Role::Tank, &IsSgDrakeTankDefensiveHeld, 0,
            HeldTankDefensiveNames());

    e.Tick(&TickSgDrakesEncounter);
}
}

EncounterDefinition const& IccSindragosaDrakesDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_NONE,
                                                UnitGate<NPC_SG_DRAKE_RIMEFANG, NPC_SG_DRAKE_SPINESTALKER>,
                                                "sindragosa", &DefineSindragosaDrakes);
    return definition;
}
