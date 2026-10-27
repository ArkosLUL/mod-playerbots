/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_Rotface.h"
#include "ICCEncounterGate.h"
#include "ICCMultipliers_Rotface.h"
#include "ICCTriggers_Rotface.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;

namespace
{
void DefineRotface(EncounterBuilder& e)
{
    e.Node<IccRotfaceTankPositionTrigger, IccRotfaceTankPositionAction>(ACTION_RAID + 5);
    e.Node<IccRotfaceGroupPositionTrigger, IccRotfaceGroupPositionAction>(ACTION_RAID + 6);
    e.Node<IccRotfaceMoveAwayFromExplosionTrigger, IccRotfaceMoveAwayFromExplosionAction>(ACTION_RAID + 7);
    e.Node<IccRotfaceAvoidVileGasTrigger, IccRotfaceAvoidVileGasAction>(ACTION_RAID + 8);

    e.Multiplier<IccRotfaceMultiplier>(Family::AnyAction);
}
}  // namespace

EncounterDefinition const& IccRotfaceDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_ROTFACE, IccPassThroughGate, "rotface", &DefineRotface);
    return definition;
}
