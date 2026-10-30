/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_Valithria.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_Valithria.h"
#include "ICCMultipliers_Valithria.h"
#include "ICCSeams.h"
#include "ICCTriggers_Valithria.h"
#include "RaidTankDefensive.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
void DefineValithria(EncounterBuilder& e)
{
    e.Node<IccValithriaDodgeTrigger, IccValithriaDodgeAction>(ACTION_EMERGENCY + 6, EncounterRow::Mover);
    // Above the kite, which wins every tick a kiter walks, so a slowed kiter can free itself
    e.Node<IccValithriaFreedomTrigger, IccValithriaFreedomAction>(ACTION_EMERGENCY + 5.5f);
    e.Node<IccValithriaKiteTrigger, IccValithriaKiteAction>(ACTION_EMERGENCY + 5, EncounterRow::Mover);
    e.Node<IccValithriaInterruptTrigger, IccValithriaInterruptAction>(ACTION_RAID + 15);
    e.Node<IccValithriaTankDefensiveTrigger, IccValithriaCastTankDefensiveAction>(ACTION_RAID + 13);
    e.Node<IccValithriaLustTrigger, IccValithriaLustAction>(ACTION_RAID + 12);
    e.Node<IccValithriaGuardianSpiritTrigger, IccValithriaGuardianSpiritAction>(ACTION_RAID + 11);
    e.Node<IccValithriaBeaconTrigger, IccValithriaBeaconAction>(ACTION_RAID + 10);
    e.Node<IccValithriaDreamTrigger, IccValithriaDreamAction>(ACTION_RAID + 9, EncounterRow::Mover);
    e.Node<IccValithriaPortalTrigger, IccValithriaPortalAction>(ACTION_RAID + 8, EncounterRow::Mover);
    e.Node<IccValithriaTauntTrigger, IccValithriaTauntAction>(ACTION_RAID + 7);
    e.Node<IccValithriaSnareTrigger, IccValithriaSnareAction>(ACTION_RAID + 6);
    e.Node<IccValithriaVoidTrigger, IccValithriaVoidAction>(ACTION_RAID + 5, EncounterRow::Mover);
    e.Node<IccValithriaTargetTrigger, IccValithriaTargetAction>(ACTION_RAID + 4);
    e.Node<IccValithriaHealTrigger, IccValithriaHealAction>(ACTION_RAID + 3, EncounterRow::Mover);

    e.LiveMultiplier<IccValithriaSplashMultiplier>(Family::Spell);

    e.OwnTargeting("icc valithria targeting", Role::Any, &IccResolverPicks<&GetVdTarget>,
                   Family::DpsAssist | Family::TankAssist | Family::DpsAoe);
    // A bot in Dream State is phased away from everything but the clouds and the other dreamers
    e.Exclusive("icc valithria dream hold", Role::Any, &IsVdDreaming, 0, {IccValithriaDreamAction::Name});
    // A zombie swing one-shots all but a tank, so a kiter does nothing but kite and dodge
    e.Exclusive("icc valithria kite hold", Role::Any, &IsVdKiteHeld, 0,
                {IccValithriaKiteAction::Name, IccValithriaDodgeAction::Name});
    // Reach toward a far heal target would walk a portal healer off its portal. A zombie can chase a healer
    // holding a claim, so the kite passes too.
    e.OwnMovement("icc valithria portal hold", Role::Heal, &IsVdHoldingPortal, Family::Attack,
                  {IccValithriaPortalAction::Name, IccValithriaKiteAction::Name, IccValithriaDodgeAction::Name,
                   IccValithriaVoidAction::Name, IccValithriaHealAction::Name});
    e.Block("icc valithria spell movers", Role::Any, &IsVdMovementOwned,
            Family::Charge | Family::Blink | Family::Disengage);
    // avoid aoe flees 5 yd at most, against the dodges
    e.Block("icc valithria avoid aoe", Role::Any, &IsVdLive, Family::AvoidAoe);
    // The tanks own abominations and take worms and zombies off healers; a generic taunt would pull a zombie
    e.Block("icc valithria taunts", Role::Tank, &IsVdLive, Family::Taunt);
    // Follow would walk a bot to its master across the room
    e.Block("icc valithria follow", Role::Any, &IsVdLive, Family::Follow);
    // The tank buttons wait for an abomination to have the tank low
    e.Block("icc valithria tank defensive hold", Role::Tank, &IsVdTankDefensiveHeld, 0, HeldTankDefensiveNames());
    // Beacon of Light sits on one target per paladin, so the generic nodes would take it back to the tank
    e.Block("icc valithria beacon hold", Role::Heal, &IsVdBeaconOnHer, 0,
            {"beacon of light on main tank", "beacon of light on tank"});

    e.Tick(&TickVdEncounter);
}
}  // namespace

EncounterDefinition const& IccValithriaDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_VALITHRIA_DREAMWALKER, VdGate, "valithria-dreamwalker",
                                                &DefineValithria);
    return definition;
}
