/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_Putricide.h"
#include "ICCEncounterGate.h"
#include "ICCMultipliers_Putricide.h"
#include "ICCTriggers_Putricide.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;

namespace
{
void DefinePutricide(EncounterBuilder& e)
{
    e.Node<IccPutricideVolatileOozeTrigger, IccPutricideVolatileOozeAction>(ACTION_RAID + 4);
    e.Node<IccPutricideGasCloudTrigger, IccPutricideGasCloudAction>(ACTION_RAID + 5);
    e.Node<IccPutricideGrowingOozePuddleTrigger, IccPutricideGrowingOozePuddleAction>(ACTION_RAID + 3);
    e.Node<IccPutricideMutatedPlagueTrigger, IccPutricideMutatedPlagueAction>(ACTION_RAID + 3);
    e.Node<IccPutricideMalleableGooTrigger, IccPutricideAvoidMalleableGooAction>(ACTION_RAID + 6);
    e.Node<IccPutricideAbominationTrigger, IccPutricideAbominationAction>(ACTION_RAID + 7);

    e.Multiplier<IccAddsPutricideMultiplier>(Family::AnyAction);
}
}  // namespace

EncounterDefinition const& IccPutricideDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_PROFESSOR_PUTRICIDE, IccPassThroughGate,
                                                "professor-putricide", &DefinePutricide);
    return definition;
}
