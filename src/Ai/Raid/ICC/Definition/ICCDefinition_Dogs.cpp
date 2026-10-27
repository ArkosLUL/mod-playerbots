/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_Dogs.h"
#include "ICCEncounterGate.h"
#include "ICCMultipliers_Dogs.h"
#include "ICCTriggers_Dogs.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;

namespace
{
void DefineDogs(EncounterBuilder& e)
{
    e.Node<IccDogsTrigger, IccDogsTankPositionAction>(ACTION_RAID + 3);

    e.Multiplier<IccDogsMultiplier>(Family::AnyAction);
}
}  // namespace

EncounterDefinition const& IccDogsDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_NONE, IccPassThroughGate, "stinky-and-precious", &DefineDogs);
    return definition;
}
