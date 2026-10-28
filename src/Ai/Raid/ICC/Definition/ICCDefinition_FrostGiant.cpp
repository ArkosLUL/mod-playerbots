/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_FrostGiant.h"
#include "ICCData.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_FrostGiant.h"
#include "ICCTriggers_FrostGiant.h"
#include "Strategy.h"

namespace Role = RaidEncounterRules::Role;

namespace
{
void DefineFrostGiant(EncounterBuilder& e)
{
    e.Node<IccRottingFrostGiantPlagueTrigger, IccRottingFrostGiantPlagueAction>(ACTION_RAID + 5, EncounterRow::Mover);

    // A carrier stays by its partner until it ticks, and nothing walks a clean bot back into a carrier's reach.
    e.Exclusive("rotting frost giant plague", Role::Any, &RfgPlagueDuty, 0, {IccRottingFrostGiantPlagueAction::Name});

    e.Tick(&RfgTick);
}
}  // namespace

EncounterDefinition const& IccFrostGiantDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_NONE,
                                                UnitGate<NPC_ROTTING_FROST_GIANT_10, NPC_ROTTING_FROST_GIANT_25>,
                                                "rotting-frost-giant", &DefineFrostGiant);
    return definition;
}
