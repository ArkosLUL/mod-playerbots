/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_BloodPrinces.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_BloodPrinces.h"
#include "ICCMultipliers_BloodPrinces.h"
#include "ICCSeams.h"
#include "ICCTriggers_BloodPrinces.h"
#include "RaidTankDefensive.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
void DefineBloodPrinces(EncounterBuilder& e)
{
    e.Node<IccBpcDodgeTrigger, IccBpcDodgeAction>(ACTION_EMERGENCY + 6, EncounterRow::Mover);
    e.Node<IccBpcFlameDefensiveTrigger, IccBpcCastFlameDefensiveAction>(ACTION_RAID + 13);
    e.Node<IccBpcLanceDefensiveTrigger, IccBpcCastLanceDefensiveAction>(ACTION_RAID + 12);
    e.Node<IccBpcDispelTrigger, IccBpcDispelAction>(ACTION_RAID + 11);
    e.Node<IccBpcTankDefensiveTrigger, IccBpcCastTankDefensiveAction>(ACTION_RAID + 10);
    e.Node<IccBpcBallTrigger, IccBpcBallAction>(ACTION_RAID + 9, EncounterRow::Mover);
    e.Node<IccBpcPrinceTauntTrigger, IccBpcPrinceTauntAction>(ACTION_RAID + 8);
    e.Node<IccBpcNucleusTrigger, IccBpcNucleusAction>(ACTION_RAID + 7, EncounterRow::Mover);
    e.Node<IccBpcSoakTrigger, IccBpcSoakAction>(ACTION_RAID + 6, EncounterRow::Mover);
    e.Node<IccBpcTargetTrigger, IccBpcTargetAction>(ACTION_RAID + 5);
    e.Node<IccBpcStandTrigger, IccBpcStandAction>(ACTION_RAID + 4, EncounterRow::Mover);
    e.Node<IccBpcBrakeTrigger, IccBpcBrakeStopAction>(ACTION_RAID + 3);

    e.LiveMultiplier<IccBpcSplashMultiplier>(Family::Spell);
    e.LiveMultiplier<IccBpcBrakeMultiplier>(Family::Spell | Family::Melee);
    e.LiveMultiplier<IccBpcDirectHitMultiplier>(Family::Spell);

    e.OwnTargeting("icc bpc targeting", Role::Any, &IccResolverPicks<&GetBpcTarget>,
                   Family::DpsAssist | Family::TankAssist | Family::DpsAoe);
    e.Block("icc bpc brake hold", Role::Any, &IsBpcBraked,
            Family::DpsAssist | Family::TankAssist | Family::DpsAoe | Family::AggressiveTarget |
                Family::AttackAnything | Family::AttackLeastHp | Family::AttackRti | Family::Follow |
                Family::MoveRandom);
    e.OwnMovement("icc bpc stand hold", Role::Any, &IsBpcHoldingStand, Family::Attack,
                  {IccBpcStandAction::Name, IccBpcDodgeAction::Name, IccBpcBallAction::Name, IccBpcSoakAction::Name,
                   "reach party member to resurrect"});
    e.OwnMovement("icc bpc tank hold", Role::Any, &IsBpcHoldingTankStand, Family::Attack,
                  {IccBpcStandAction::Name, IccBpcDodgeAction::Name, IccBpcBallAction::Name,
                   IccBpcNucleusAction::Name});
    e.OwnMovement("icc bpc soak hold", Role::Any, &IsBpcSoakHeld, Family::Attack,
                  {IccBpcSoakAction::Name, IccBpcDodgeAction::Name});
    e.OwnMovement("icc bpc prison hold", Role::Any, &IsBpcPrisonHeld,
                  Family::Attack | Family::Reach | Family::ReachHeal,
                  {IccBpcStandAction::Name, IccBpcDodgeAction::Name, IccBpcBallAction::Name, IccBpcSoakAction::Name,
                   IccBpcNucleusAction::Name});
    e.Block("icc bpc spell movers", Role::Any, &IsBpcMovementOwned, Family::Charge | Family::Blink | Family::Disengage);
    e.Block("icc bpc avoid aoe", Role::Any, &IsBpcLive, Family::AvoidAoe);
    e.Block("icc bpc taunts", Role::Tank, &IsBpcLive, Family::Taunt);
    e.Block("icc bpc pulls", Role::Any, &IsBpcLive, 0, {"death grip", "army of the dead"});
    e.Block("icc bpc stand-in threat", Role::Any, &IsBpcStandIn, 0,
            {"soulshatter", "fade", "feign death", "invisibility", "misdirection on main tank"});
    e.Block("icc bpc drop guard", Role::Any, &IsBpcDirectHitPick, Family::DropTarget);
    e.Block("icc bpc tank defensive hold", Role::Tank, &IsBpcTankDefensiveHeld, 0, HeldTankDefensiveNames());
    e.Block("icc bpc tank form", Role::Tank, &IsBpcLive, 0, {"caster form"});

    e.Tick(&TickBpcEncounter);
}
}

EncounterDefinition const& IccBloodPrincesDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_BLOOD_PRINCE_COUNCIL, BossStateGate,
                                                "blood-prince-council", &DefineBloodPrinces);
    return definition;
}
