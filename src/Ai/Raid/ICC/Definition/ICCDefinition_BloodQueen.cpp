/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_BloodQueen.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_BloodQueen.h"
#include "ICCMultipliers_BloodQueen.h"
#include "ICCSeams.h"
#include "ICCTriggers_BloodQueen.h"
#include "RaidDefensive.h"
#include "RaidTankDefensive.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
void DefineBloodQueen(EncounterBuilder& e)
{
    e.Node<IccBqlDodgeTrigger, IccBqlDodgeAction>(ACTION_EMERGENCY + 6, EncounterRow::Mover);
    e.Node<IccBqlBiteTrigger, IccBqlBiteAction>(ACTION_EMERGENCY + 5, EncounterRow::Mover);
    e.Node<IccBqlShadowsTrigger, IccBqlShadowsAction>(ACTION_EMERGENCY + 4, EncounterRow::Mover);
    e.Node<IccBqlPactTrigger, IccBqlPactAction>(ACTION_EMERGENCY + 3, EncounterRow::Mover);
    e.Node<IccBqlMassDispelTrigger, IccBqlMassDispelAction>(ACTION_RAID + 12);
    e.Node<IccBqlWhirlDefensiveTrigger, IccBqlCastWhirlDefensiveAction>(ACTION_RAID + 11);
    e.Node<IccBqlWhirlCoverTrigger, IccBqlCastWhirlCoverAction>(ACTION_RAID + 10);
    e.Node<IccBqlTankDefensiveTrigger, IccBqlCastTankDefensiveAction>(ACTION_RAID + 9);
    e.Node<IccBqlAntiFearTrigger, IccBqlAntiFearAction>(ACTION_RAID + 8);
    e.Node<IccBqlFrenzyCcTrigger, IccBqlFrenzyCcAction>(ACTION_RAID + 7);
    e.Node<IccBqlTauntTrigger, IccBqlTauntAction>(ACTION_RAID + 6);
    e.Node<IccBqlBrakeTrigger, IccBqlBrakeStopAction>(ACTION_RAID + 5);
    e.Node<IccBqlTargetTrigger, IccBqlTargetAction>(ACTION_RAID + 4);
    e.Node<IccBqlStandTrigger, IccBqlStandAction>(ACTION_RAID + 3, EncounterRow::Mover);

    e.LiveMultiplier<IccBqlBrakeMultiplier>(Family::Spell | Family::Melee);
    e.LiveMultiplier<IccBqlSplashMultiplier>(Family::Spell);
    e.LiveMultiplier<IccBqlAntiFearTotemGuardMultiplier>(Family::Spell);

    e.OwnTargeting("icc bql targeting", Role::Any, &IccResolverPicks<&GetBqlTarget>,
                   Family::DpsAssist | Family::TankAssist | Family::DpsAoe);
    e.Block("icc bql brake hold", Role::Any, &IsBqlBraked,
            Family::DpsAssist | Family::TankAssist | Family::DpsAoe | Family::AggressiveTarget |
                Family::AttackAnything | Family::AttackLeastHp | Family::AttackRti | Family::Follow |
                Family::MoveRandom);
    e.Exclusive("icc bql bite hold", Role::Any, &IsBqlBiting, 0, {IccBqlBiteAction::Name});
    e.Exclusive("icc bql shadows hold", Role::Any, &IsBqlShadowCarrier, 0, {IccBqlShadowsAction::Name});
    e.OwnMovement("icc bql pact hold", Role::Any, &IsBqlPactMember, Family::Attack, {IccBqlPactAction::Name});
    e.OwnMovement("icc bql stand hold", Role::Any, &IsBqlHoldingStand, Family::Attack,
                  {IccBqlStandAction::Name, IccBqlDodgeAction::Name, "reach party member to resurrect"});
    e.OwnMovement("icc bql tank hold", Role::Any, &IsBqlHoldingTankStand, Family::Attack,
                  {IccBqlStandAction::Name, IccBqlDodgeAction::Name});
    e.Block("icc bql spell movers", Role::Any, &IsBqlMovementOwned, Family::Charge | Family::Blink | Family::Disengage);
    e.Block("icc bql avoid aoe", Role::Any, &IsBqlLive, Family::AvoidAoe);
    e.Block("icc bql taunts", Role::Tank, &IsBqlLive, Family::Taunt);
    e.Block("icc bql pulls", Role::Any, &IsBqlLive, 0, {"death grip"});
    e.Block("icc bql tank form", Role::Tank, &IsBqlLive, 0, {"caster form"});
    e.Block("icc bql tank defensive hold", Role::Tank, &IsBqlTankDefensiveHeld, 0, HeldTankDefensiveNames());
    e.Block("icc bql whirl cover hold", Role::Any, &IsBqlWhirlCoverHeld, 0, RaidCoverClassNodes());

    e.Tick(&TickBqlEncounter);
}
}

EncounterDefinition const& IccBloodQueenDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_BLOOD_QUEEN_LANA_THEL, BossStateGate,
                                                "blood-queen-lana-thel", &DefineBloodQueen);
    return definition;
}
