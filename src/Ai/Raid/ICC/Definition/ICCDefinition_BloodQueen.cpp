/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_BloodQueen.h"
#include "ICCEncounterGate.h"
#include "ICCMultipliers_BloodQueen.h"
#include "ICCTriggers_BloodQueen.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;

namespace
{
void DefineBloodQueen(EncounterBuilder& e)
{
    e.Node<IccBqlGroupPositionTrigger, IccBqlGroupPositionAction>(ACTION_RAID);
    e.Node<IccBqlPactOfDarkfallenTrigger, IccBqlPactOfDarkfallenAction>(ACTION_RAID + 1);
    e.Node<IccBqlVampiricBiteTrigger, IccBqlVampiricBiteAction>(ACTION_EMERGENCY + 5);

    e.Multiplier<IccBqlMultiplier>(Family::AnyAction);
}
}  // namespace

EncounterDefinition const& IccBloodQueenDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_BLOOD_QUEEN_LANA_THEL, IccPassThroughGate,
                                                "blood-queen-lana-thel", &DefineBloodQueen);
    return definition;
}
