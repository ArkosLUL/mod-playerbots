/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_Svalna.h"
#include "ICCEncounterGate.h"
#include "ICCTriggers_Svalna.h"
#include "Strategy.h"

namespace
{
void DefineSvalna(EncounterBuilder& e)
{
    e.Node<IccValkyreSpearTrigger, IccValkyreSpearAction>(ACTION_EMERGENCY + 5);
    e.Node<IccSisterSvalnaTrigger, IccSisterSvalnaAction>(ACTION_RAID + 5);
}
}  // namespace

EncounterDefinition const& IccSvalnaDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_SISTER_SVALNA, IccPassThroughGate,
                                                "sister-svalna", &DefineSvalna);
    return definition;
}
