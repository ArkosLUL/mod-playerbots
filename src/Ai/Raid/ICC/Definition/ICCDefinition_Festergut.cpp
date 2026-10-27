/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_Festergut.h"
#include "ICCEncounterGate.h"
#include "ICCMultipliers_Festergut.h"
#include "ICCTriggers_Festergut.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;

namespace
{
void DefineFestergut(EncounterBuilder& e)
{
    e.Node<IccFestergutGroupPositionTrigger, IccFestergutGroupPositionAction>(ACTION_MOVE + 4);
    e.Node<IccFestergutSporeTrigger, IccFestergutSporeAction>(ACTION_MOVE + 5);
    e.Node<IccFestergutAvoidMalleableGooTrigger, IccFestergutAvoidMalleableGooAction>(ACTION_RAID + 7);

    e.Multiplier<IccFestergutMultiplier>(Family::AnyAction);
}
}  // namespace

EncounterDefinition const& IccFestergutDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_FESTERGUT, IccPassThroughGate, "festergut", &DefineFestergut);
    return definition;
}
