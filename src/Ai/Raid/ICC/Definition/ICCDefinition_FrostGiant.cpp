/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_FrostGiant.h"
#include "ICCEncounterGate.h"
#include "ICCTriggers_FrostGiant.h"
#include "Strategy.h"

namespace
{
void DefineFrostGiant(EncounterBuilder& e)
{
    e.Node<IccRottingFrostGiantTankPositionTrigger, IccRottingFrostGiantTankPositionAction>(ACTION_RAID + 5);
}
}  // namespace

EncounterDefinition const& IccFrostGiantDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_NONE, IccPassThroughGate,
                                                "rotting-frost-giant", &DefineFrostGiant);
    return definition;
}
