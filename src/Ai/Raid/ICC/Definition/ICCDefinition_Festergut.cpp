/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_Festergut.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_Festergut.h"
#include "ICCMultipliers_Festergut.h"
#include "ICCSeams.h"
#include "ICCTriggers_Festergut.h"
#include "RaidDefensive.h"
#include "RaidTankDefensive.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
// Every row and rule reads the fight off his combat, not his boss state, which moves about 7 s in
void DefineFestergut(EncounterBuilder& e)
{
    e.Node<IccFestergutGooTrigger, IccFestergutDodgeGooAction>(ACTION_EMERGENCY + 6, EncounterRow::Mover);
    e.Node<IccFestergutPungentDefensiveTrigger, IccFestergutCastPungentDefensiveAction>(ACTION_RAID + 10);
    e.Node<IccFestergutPungentCoverTrigger, IccFestergutCastPungentCoverAction>(ACTION_RAID + 9);
    e.Node<IccFestergutTankDefensiveTrigger, IccFestergutCastTankDefensiveAction>(ACTION_RAID + 8);
    e.Node<IccFestergutBloatSwapTrigger, IccFestergutBloatSwapAction>(ACTION_RAID + 7);
    e.Node<IccFestergutBrakeTrigger, IccFestergutBrakeStopAction>(ACTION_RAID + 6);
    // Above the stand: a bot takes its pick before it walks back to its spot
    e.Node<IccFestergutTargetTrigger, IccFestergutTargetAction>(ACTION_RAID + 2);
    e.Node<IccFestergutStandTrigger, IccFestergutStandAction>(ACTION_RAID + 1, EncounterRow::Mover);

    e.LiveMultiplier<IccFestergutBrakeMultiplier>(Family::Spell | Family::Melee);

    e.OwnTargeting("icc festergut targeting", Role::Any, &IccResolverPicks<&GetFgTarget>,
                   Family::DpsAssist | Family::TankAssist | Family::DpsAoe);
    // A braked tank's resolver hands it nothing, so no picker may give it him back
    e.Block("icc festergut brake hold", Role::Tank, &IsFgBraked,
            Family::DpsAssist | Family::TankAssist | Family::DpsAoe | Family::AggressiveTarget |
                Family::AttackAnything | Family::AttackLeastHp | Family::AttackRti);
    // Every stand is in reach already, and reach would walk a slot off the Vile Gas spread
    e.OwnMovement("icc festergut stand hold", Role::Any, &IsFgHoldingStand, Family::Attack | Family::ReachHeal,
                  {IccFestergutStandAction::Name, IccFestergutDodgeGooAction::Name,
                   "reach party member to resurrect"});
    e.Block("icc festergut spell movers", Role::Any, &IsFgHoldingStand,
            Family::Charge | Family::Blink | Family::Disengage);
    // He obeys taunt diminishing returns, so only the swap spends a tank's taunt
    e.Block("icc festergut taunts", Role::Tank, &IsFgFight, Family::Taunt);
    // A braked tank has no target, and a druid's non-combat nodes drop bear form
    e.Block("icc festergut tank form", Role::Tank, &IsFgFight, 0, {"caster form"});
    // The tank buttons wait for Inhaled Blight or a low tank, the raid cooldowns for Pungent Blight
    e.Block("icc festergut tank defensive hold", Role::Tank, &IsFgTankDefensiveHeld, 0,
            HeldTankDefensiveNames());
    e.Block("icc festergut pungent cover hold", Role::Any, &IsFgPungentCoverHeld, 0, RaidCoverClassNodes());

    e.Tick(&TickFgEncounter);
}
}  // namespace

EncounterDefinition const& IccFestergutDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_FESTERGUT, BossStateGate, "festergut", &DefineFestergut);
    return definition;
}
