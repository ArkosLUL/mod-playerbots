/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_Dogs.h"
#include "ICCData.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_Dogs.h"
#include "ICCMultipliers_Dogs.h"
#include "ICCSeams.h"
#include "ICCTriggers_Dogs.h"
#include "RaidTankDefensive.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
void DefineDogs(EncounterBuilder& e)
{
    e.Node<IccDogsTankDefensiveTrigger, IccDogsCastTankDefensiveAction>(ACTION_RAID + 6);
    e.Node<IccDogsDecimateCoverTrigger, IccDogsCastDecimateCoverAction>(ACTION_RAID + 5);
    e.Node<IccDogsSwapTrigger, IccDogsSwapAction>(ACTION_RAID + 4);
    e.Node<IccDogsYieldTrigger, IccDogsYieldAction>(ACTION_RAID + 3);
    e.Node<IccDogsTargetTrigger, IccDogsTargetAction>(ACTION_RAID + 2);

    e.LiveMultiplier<IccDogsYieldMultiplier>(Family::Spell | Family::Melee);

    e.OwnTargeting("icc dogs targeting", Role::Any, &IccResolverPicks<&GetDogsTarget>,
                   Family::DpsAssist | Family::TankAssist | Family::DpsAoe);
    // A yielding tank's resolver hands it a zombie or nothing, so no picker may give it the dog back
    e.Block("icc dogs yield hold", Role::Tank, &IsDogsYielding,
            Family::DpsAssist | Family::TankAssist | Family::DpsAoe | Family::AggressiveTarget |
                Family::AttackAnything | Family::AttackLeastHp | Family::AttackRti);
    // The free tank's generic taunt would take the dog straight back off its partner
    e.Block("icc dogs taunts", Role::Tank, &IsDogsLive, Family::Taunt);
    // The tank buttons wait for Decimate or a low tank
    e.Block("icc dogs tank defensive hold", Role::Tank, &IsDogsTankDefensiveHeld, 0, HeldTankDefensiveNames());

    e.Tick(&TickDogsEncounter);
}
}  // namespace

EncounterDefinition const& IccDogsDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_NONE, UnitGate<NPC_STINKY, NPC_PRECIOUS>,
                                                "stinky-and-precious", &DefineDogs);
    return definition;
}
