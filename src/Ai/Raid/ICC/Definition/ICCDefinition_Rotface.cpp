/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_Rotface.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_Rotface.h"
#include "ICCMultipliers_Rotface.h"
#include "ICCSeams.h"
#include "ICCTriggers_Rotface.h"
#include "RaidTankDefensive.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
void DefineRotface(EncounterBuilder& e)
{
    e.Node<IccRotfaceDodgeTrigger, IccRotfaceDodgeAction>(ACTION_EMERGENCY + 6, EncounterRow::Mover);
    e.Node<IccRotfaceFreedomTrigger, IccRotfaceFreedomAction>(ACTION_RAID + 12);
    e.Node<IccRotfaceTotemTrigger, IccRotfaceTotemAction>(ACTION_RAID + 11);
    e.Node<IccRotfaceTankDefensiveTrigger, IccRotfaceCastTankDefensiveAction>(ACTION_RAID + 10);
    e.Node<IccRotfaceDispelTrigger, IccRotfaceDispelAction>(ACTION_RAID + 9);
    e.Node<IccRotfaceOozeTauntTrigger, IccRotfaceOozeTauntAction>(ACTION_RAID + 8);
    e.Node<IccRotfaceKiteTrigger, IccRotfaceKiteAction>(ACTION_RAID + 7, EncounterRow::Mover);
    e.Node<IccRotfaceCarrierTrigger, IccRotfaceCarrierAction>(ACTION_RAID + 6, EncounterRow::Mover);
    e.Node<IccRotfaceCureWalkTrigger, IccRotfaceCureWalkAction>(ACTION_RAID + 5, EncounterRow::Mover);
    e.Node<IccRotfaceTargetTrigger, IccRotfaceTargetAction>(ACTION_RAID + 4);
    e.Node<IccRotfaceStandTrigger, IccRotfaceStandAction>(ACTION_RAID + 3, EncounterRow::Mover);

    e.LiveMultiplier<IccRotfaceOozeSplashMultiplier>(Family::Spell);
    e.LiveMultiplier<IccRotfaceSelfCureMultiplier>(Family::Spell);

    e.OwnTargeting("icc rotface targeting", Role::Any, &IccResolverPicks<&GetRfTarget>,
                   Family::DpsAssist | Family::TankAssist | Family::DpsAoe);
    e.OwnMovement("icc rotface stand hold", Role::Any, &IsRfHoldingStand, Family::Attack,
                  {IccRotfaceStandAction::Name, IccRotfaceDodgeAction::Name});
    e.OwnMovement("icc rotface kite hold", Role::Any, &IsRfKiteHeld, Family::Attack,
                  {IccRotfaceKiteAction::Name, IccRotfaceDodgeAction::Name});
    e.OwnMovement("icc rotface carrier hold", Role::Any, &IsRfCarrierHeld, Family::Attack,
                  {IccRotfaceCarrierAction::Name, IccRotfaceDodgeAction::Name});
    e.OwnMovement("icc rotface cure walk hold", Role::Any, &IsRfCureWalkHeld, Family::Attack,
                  {IccRotfaceCureWalkAction::Name, IccRotfaceDodgeAction::Name});
    e.OwnMovement("icc rotface hazard hold", Role::Any, &IsRfHazardHeld, Family::Attack);
    e.Block("icc rotface spell movers", Role::Any, &IsRfMovementOwned,
            Family::Charge | Family::Blink | Family::Disengage);
    e.Block("icc rotface avoid aoe", Role::Any, &IsRfLive, Family::AvoidAoe);
    e.Block("icc rotface kiter taunts", Role::Any, &IsRfKiter, Family::Taunt);
    e.Block("icc rotface pulls", Role::Any, &IsRfLive, 0, {"death grip", "army of the dead"});
    e.Block("icc rotface cures", Role::Any, &IsRfLive, 0,
            {"cure disease", "cure disease on party", "abolish disease", "abolish disease on party",
             "cleanse disease", "cleanse disease on party", "purify disease", "purify disease on party",
             "cleanse spirit", "cleanse spirit disease on party", "cure toxins", "cure toxins disease on party",
             "stoneform", "cleansing totem"});
    e.Block("icc rotface tank defensive hold", Role::Tank, &IsRfTankDefensiveHeld, 0, HeldTankDefensiveNames());

    e.Tick(&TickRfEncounter);
}
}

EncounterDefinition const& IccRotfaceDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_ROTFACE, BossStateGate, "rotface", &DefineRotface);
    return definition;
}
