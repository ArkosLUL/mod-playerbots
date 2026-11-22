/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_Gunship.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_Gunship.h"
#include "ICCMultipliers_Gunship.h"
#include "ICCSeams.h"
#include "ICCTriggers_Gunship.h"
#include "RaidTankDefensive.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
void DefineGunship(EncounterBuilder& e)
{
    e.Node<IccGunshipDodgeTrigger, IccGunshipDodgeAction>(ACTION_EMERGENCY + 6, EncounterRow::Mover);
    e.Node<IccGunshipTankDefensiveTrigger, IccGunshipTankDefensiveAction>(ACTION_RAID + 8);
    e.Node<IccGunshipRocketJumpTrigger, IccGunshipRocketJumpAction>(ACTION_RAID + 7, EncounterRow::Mover);
    e.Node<IccGunshipTauntTrigger, IccGunshipTauntAction>(ACTION_RAID + 6);
    e.Node<IccGunshipCannonNearTrigger, IccGunshipEnterCannonAction>(ACTION_RAID + 5, EncounterRow::Mover);
    e.Node<IccInCannonTrigger, IccCannonFireAction>(ACTION_RAID + 4);
    e.Node<IccGunshipTargetTrigger, IccGunshipTargetAction>(ACTION_RAID + 3);
    e.Node<IccGunshipStandTrigger, IccGunshipStandAction>(ACTION_RAID + 2, EncounterRow::Mover);
    e.Node<IccGunshipRocketPackSetupTrigger, IccGunshipRocketPackSetupAction>(ACTION_RAID + 1, EncounterRow::Mover);

    e.LiveMultiplier<IccGunshipDeckGuardMultiplier>(Family::Reach | Family::ReachHeal | Family::Charge);

    e.Exclusive("icc gunship in flight", Role::Any, &GsbInFlight, 0, {IccGunshipRocketJumpAction::Name});
    e.Exclusive("icc gunship dodge", Role::Any, &GsbDodging, 0, {IccGunshipDodgeAction::Name});
    e.Block("icc gunship hazard band", Role::Any, &GsbNearHazard,
            Family::Reach | Family::ReachHeal | Family::Charge | Family::TankFace | Family::SetBehind |
                Family::RearFlank);
    e.Block("icc gunship deck movers", Role::Any, &GsbLive,
            Family::Follow | Family::CombatFormationMove | Family::Flee | Family::RunAway | Family::MoveRandom |
                Family::MoveOutOfCollision | Family::AvoidAoe | Family::Blink | Family::Disengage);
    e.OwnMovement("icc gunship stand hold", Role::Any, &GsbHoldsStand, Family::Attack);
    e.OwnTargeting("icc gunship targeting", Role::Any, &IccResolverPicks<&GsbTarget>);
    e.Block("icc gunship tank defensive hold", Role::Tank, &GsbTankDefensiveHeld, 0, HeldTankDefensiveNames());

    e.Tick(&GsbTick);
}
}

EncounterDefinition const& IccGunshipDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_GUNSHIP_BATTLE, BossStateGate, "icecrown-gunship-battle",
                                                &DefineGunship);
    return definition;
}
