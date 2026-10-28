/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_Saurfang.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_Saurfang.h"
#include "ICCMultipliers_Saurfang.h"
#include "ICCSeams.h"
#include "ICCTriggers_Saurfang.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
void DefineSaurfang(EncounterBuilder& e)
{
    e.Node<IccDbsRuneSwapTrigger, IccDbsRuneSwapAction>(ACTION_RAID + 7);
    e.Node<IccDbsRuneYieldTrigger, IccDbsRuneYieldAction>(ACTION_RAID + 6);
    e.Node<IccDbsBeastTauntTrigger, IccDbsBeastTauntAction>(ACTION_RAID + 5);
    e.Node<IccDbsBeastControlTrigger, IccDbsBeastControlAction>(ACTION_RAID + 4);
    e.Node<IccDbsMarkHealTrigger, IccDbsMarkHealAction>(ACTION_RAID + 3);
    // Above the stand: a bot takes a new pick before it walks back to its spot
    e.Node<IccDbsTargetTrigger, IccDbsTargetAction>(ACTION_RAID + 2);
    e.Node<IccDbsStandTrigger, IccDbsStandAction>(ACTION_RAID + 1, EncounterRow::Mover);

    e.LiveMultiplier<IccDbsAreaDamageMultiplier>(Family::Spell);
    e.LiveMultiplier<IccDbsBeaconDutyMultiplier>(Family::Spell);

    e.OwnTargeting("icc dbs targeting", Role::Any, &IccResolverPicks<&GetDbsTarget>,
                   Family::DpsAssist | Family::TankAssist | Family::DpsAoe);
    // The rune tank's resolver hands it nothing but a beast, so no picker may give it him back
    e.Block("icc dbs rune yield", Role::Tank, &IsDbsRuneYielding,
            Family::DpsAssist | Family::TankAssist | Family::DpsAoe | Family::AggressiveTarget |
                Family::AttackAnything | Family::AttackLeastHp | Family::AttackRti);
    // A slot walked off by reach breaks the Blood Nova spread, and every slot is in reach already
    e.OwnMovement("icc dbs slot hold", Role::Any, &IsDbsHoldingSpreadStand, Family::Attack,
                  {IccDbsStandAction::Name, "reach party member to resurrect"});
    e.OwnMovement("icc dbs tank hold", Role::Tank, &IsDbsHoldingTankStand, Family::Attack | Family::Reach,
                  {IccDbsStandAction::Name});
    e.Block("icc dbs spell movers", Role::Any, &IsDbsHoldingStand, Family::Charge | Family::Blink | Family::Disengage);
    // He obeys taunt diminishing returns, so only the swap and the beast taunt spend a tank's taunt
    e.Block("icc dbs taunts", Role::Tank, &IsDbsLive, Family::Taunt);
    // Its ghouls taunt the beasts, which then feed him Blood Power
    e.Block("icc dbs army", Role::Any, &IsDbsArmyHeld, 0, {"army of the dead"});
    // A yielding tank has no target, and a druid's non-combat nodes drop bear form
    e.Block("icc dbs tank form", Role::Tank, &IsDbsLive, 0, {"caster form"});

    e.Tick(&TickDbsEncounter);
}
}  // namespace

EncounterDefinition const& IccSaurfangDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_DEATHBRINGER_SAURFANG, BossStateGate,
                                                "deathbringer-saurfang", &DefineSaurfang);
    return definition;
}
