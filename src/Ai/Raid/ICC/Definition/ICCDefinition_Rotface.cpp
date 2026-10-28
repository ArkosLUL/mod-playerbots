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
    e.Node<IccRotfaceTankDefensiveTrigger, IccRotfaceCastTankDefensiveAction>(ACTION_RAID + 9);
    e.Node<IccRotfaceDispelTrigger, IccRotfaceDispelAction>(ACTION_RAID + 8);
    e.Node<IccRotfaceOozeTauntTrigger, IccRotfaceOozeTauntAction>(ACTION_RAID + 7);
    e.Node<IccRotfaceKiteTrigger, IccRotfaceKiteAction>(ACTION_RAID + 6, EncounterRow::Mover);
    e.Node<IccRotfaceCarrierTrigger, IccRotfaceCarrierAction>(ACTION_RAID + 5, EncounterRow::Mover);
    // Above the stand: a bot takes a new pick before it walks back to its spot
    e.Node<IccRotfaceTargetTrigger, IccRotfaceTargetAction>(ACTION_RAID + 4);
    e.Node<IccRotfaceStandTrigger, IccRotfaceStandAction>(ACTION_RAID + 3, EncounterRow::Mover);

    e.LiveMultiplier<IccRotfaceOozeSplashMultiplier>(Family::Spell);
    e.LiveMultiplier<IccRotfaceSelfCureMultiplier>(Family::Spell);

    // Every rule keys on him being in combat, which comes seconds before his boss state moves
    e.OwnTargeting("icc rotface targeting", Role::Any, &IccResolverPicks<&GetRfTarget>,
                   Family::DpsAssist | Family::TankAssist | Family::DpsAoe);
    // Every slot is in range of him, and reach would walk a slot off the spread
    e.OwnMovement("icc rotface stand hold", Role::Any, &IsRfHoldingStand, Family::Attack,
                  {IccRotfaceStandAction::Name, IccRotfaceDodgeAction::Name});
    e.OwnMovement("icc rotface kite hold", Role::Tank, &IsRfKiteHeld, Family::Attack,
                  {IccRotfaceKiteAction::Name, IccRotfaceDodgeAction::Name});
    e.OwnMovement("icc rotface carrier hold", Role::Any, &IsRfCarrierHeld, Family::Attack,
                  {IccRotfaceCarrierAction::Name, IccRotfaceDodgeAction::Name});
    // Generic movers would walk a bot back onto an explosion mark still in flight or into the spray
    e.OwnMovement("icc rotface hazard hold", Role::Any, &IsRfHazardHeld, Family::Attack);
    e.Block("icc rotface spell movers", Role::Any, &IsRfMovementOwned,
            Family::Charge | Family::Blink | Family::Disengage);
    // avoid aoe sees the Sticky Ooze puddles and flees them 5 yd at most, against the dodge
    e.Block("icc rotface avoid aoe", Role::Any, &IsRfLive, Family::AvoidAoe);
    // A Big Ooze obeys taunt diminishing returns, so only the ooze taunt spends the kiter's
    e.Block("icc rotface kiter taunts", Role::Tank, &IsRfKiter, Family::Taunt);
    // He and both oozes are grip immune, so a grip only wastes the global cooldown, and the ghouls taunt the
    // oozes
    e.Block("icc rotface pulls", Role::Any, &IsRfLive, 0, {"death grip", "army of the dead"});
    // Removing Mutated Infection spawns its Little Ooze on the spot: only the dispel duty cures it. Stoneform
    // and Cleansing Totem strip diseases too.
    e.Block("icc rotface cures", Role::Any, &IsRfLive, 0,
            {"cure disease", "cure disease on party", "abolish disease", "abolish disease on party",
             "cleanse disease", "cleanse disease on party", "purify disease", "purify disease on party",
             "cleanse spirit", "cleanse spirit disease on party", "cure toxins", "cure toxins disease on party",
             "stoneform", "cleansing totem"});
    // The tank buttons wait for a full-strength Big Ooze or a low tank
    e.Block("icc rotface tank defensive hold", Role::Tank, &IsRfTankDefensiveHeld, 0, HeldTankDefensiveNames());

    e.Tick(&TickRfEncounter);
}
}  // namespace

EncounterDefinition const& IccRotfaceDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_ROTFACE, BossStateGate, "rotface", &DefineRotface);
    return definition;
}
