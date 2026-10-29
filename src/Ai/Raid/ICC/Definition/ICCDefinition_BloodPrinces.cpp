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
    e.Node<IccBpcTankDefensiveTrigger, IccBpcCastTankDefensiveAction>(ACTION_RAID + 10);
    e.Node<IccBpcBallTrigger, IccBpcBallAction>(ACTION_RAID + 9, EncounterRow::Mover);
    e.Node<IccBpcPrinceTauntTrigger, IccBpcPrinceTauntAction>(ACTION_RAID + 8);
    e.Node<IccBpcNucleusTrigger, IccBpcNucleusAction>(ACTION_RAID + 7, EncounterRow::Mover);
    e.Node<IccBpcSoakTrigger, IccBpcSoakAction>(ACTION_RAID + 6, EncounterRow::Mover);
    // Above the stand: a bot takes a new pick before it walks back to its spot
    e.Node<IccBpcTargetTrigger, IccBpcTargetAction>(ACTION_RAID + 5);
    e.Node<IccBpcStandTrigger, IccBpcStandAction>(ACTION_RAID + 4, EncounterRow::Mover);

    e.LiveMultiplier<IccBpcSplashMultiplier>(Family::Spell);

    e.OwnTargeting("icc bpc targeting", Role::Any, &IccResolverPicks<&GetBpcTarget>,
                   Family::DpsAssist | Family::TankAssist | Family::DpsAoe);
    // Every node is in range of what its bot works on, and reach would walk a node off the spread
    e.OwnMovement("icc bpc stand hold", Role::Any, &IsBpcHoldingStand, Family::Attack,
                  {IccBpcStandAction::Name, IccBpcDodgeAction::Name, IccBpcBallAction::Name, IccBpcSoakAction::Name,
                   "reach party member to resurrect"});
    // The princes come to the tank on its spot
    e.OwnMovement("icc bpc tank hold", Role::Tank, &IsBpcHoldingTankStand, Family::Attack,
                  {IccBpcStandAction::Name, IccBpcDodgeAction::Name, IccBpcBallAction::Name,
                   IccBpcNucleusAction::Name});
    // Reach would walk a soaker back to its target before the ball passes
    e.OwnMovement("icc bpc soak hold", Role::Any, &IsBpcSoakHeld, Family::Attack,
                  {IccBpcSoakAction::Name, IccBpcDodgeAction::Name});
    // Heroic: every second on the move stacks Shadow Prison, so nothing but the encounter and reach moves a bot
    e.OwnMovement("icc bpc prison hold", Role::Any, &IsBpcPrisonHeld,
                  Family::Attack | Family::Reach | Family::ReachHeal,
                  {IccBpcStandAction::Name, IccBpcDodgeAction::Name, IccBpcBallAction::Name, IccBpcSoakAction::Name,
                   IccBpcNucleusAction::Name});
    e.Block("icc bpc spell movers", Role::Any, &IsBpcMovementOwned, Family::Charge | Family::Blink | Family::Disengage);
    // avoid aoe flees a Shock Vortex 5 yd at most, against the dodge, and flees the Ball of Inferno Flame
    // the soak walks into
    e.Block("icc bpc avoid aoe", Role::Any, &IsBpcLive, Family::AvoidAoe);
    // The princes obey taunt diminishing returns, so only the recovery taunt spends a tank's taunt
    e.Block("icc bpc taunts", Role::Tank, &IsBpcLive, Family::Taunt);
    // A grip shares the princes' taunt DR, and a ghoul's hit takes a nucleus off its tank
    e.Block("icc bpc pulls", Role::Any, &IsBpcLive, 0, {"death grip", "army of the dead"});
    // A bomb nobody has hit yet is out of combat, and drop target clears it from the shooter
    e.Block("icc bpc bomb drop guard", Role::Any, &IsBpcBombShooting, Family::DropTarget);
    // The tank buttons wait for Keleseth's empowered Shadow Lance or a low tank
    e.Block("icc bpc tank defensive hold", Role::Tank, &IsBpcTankDefensiveHeld, 0, HeldTankDefensiveNames());

    e.Tick(&TickBpcEncounter);
}
}  // namespace

EncounterDefinition const& IccBloodPrincesDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_BLOOD_PRINCE_COUNCIL, BossStateGate,
                                                "blood-prince-council", &DefineBloodPrinces);
    return definition;
}
