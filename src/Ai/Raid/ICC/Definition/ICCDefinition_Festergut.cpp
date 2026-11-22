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
void DefineFestergut(EncounterBuilder& e)
{
    e.Node<IccFestergutGooTrigger, IccFestergutDodgeGooAction>(ACTION_EMERGENCY + 6, EncounterRow::Mover);
    e.Node<IccFestergutBloatClearTrigger, IccFestergutBloatClearAction>(ACTION_RAID + 11);
    e.Node<IccFestergutPungentDefensiveTrigger, IccFestergutCastPungentDefensiveAction>(ACTION_RAID + 10);
    e.Node<IccFestergutPungentCoverTrigger, IccFestergutCastPungentCoverAction>(ACTION_RAID + 9);
    e.Node<IccFestergutTankDefensiveTrigger, IccFestergutCastTankDefensiveAction>(ACTION_RAID + 8);
    e.Node<IccFestergutBloatSwapTrigger, IccFestergutBloatSwapAction>(ACTION_RAID + 7);
    e.Node<IccFestergutBrakeTrigger, IccFestergutBrakeStopAction>(ACTION_RAID + 6);
    e.Node<IccFestergutTargetTrigger, IccFestergutTargetAction>(ACTION_RAID + 2);
    e.Node<IccFestergutStandTrigger, IccFestergutStandAction>(ACTION_RAID + 1, EncounterRow::Mover);

    e.LiveMultiplier<IccFestergutBrakeMultiplier>(Family::Spell | Family::Melee);

    e.OwnTargeting("icc festergut targeting", Role::Any, &IccResolverPicks<&GetFgTarget>,
                   Family::DpsAssist | Family::TankAssist | Family::DpsAoe);
    e.Block("icc festergut brake hold", Role::Tank, &IsFgBraked,
            Family::DpsAssist | Family::TankAssist | Family::DpsAoe | Family::AggressiveTarget |
                Family::AttackAnything | Family::AttackLeastHp | Family::AttackRti);
    e.OwnMovement("icc festergut stand hold", Role::Any, &IsFgHoldingStand, Family::Attack | Family::ReachHeal,
                  {IccFestergutStandAction::Name, IccFestergutDodgeGooAction::Name,
                   "reach party member to resurrect"});
    e.Block("icc festergut spell movers", Role::Any, &IsFgHoldingStand,
            Family::Charge | Family::Blink | Family::Disengage);
    e.Block("icc festergut taunts", Role::Tank, &IsFgFight, Family::Taunt);
    e.Block("icc festergut tank form", Role::Tank, &IsFgFight, 0, {"caster form"});
    e.Block("icc festergut tank defensive hold", Role::Tank, &IsFgTankDefensiveHeld, 0,
            HeldTankDefensiveNames());
    e.Block("icc festergut pungent cover hold", Role::Any, &IsFgPungentCoverHeld, 0, RaidCoverClassNodes());

    e.Tick(&TickFgEncounter);
}
}

EncounterDefinition const& IccFestergutDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_FESTERGUT, BossStateGate, "festergut", &DefineFestergut);
    return definition;
}
