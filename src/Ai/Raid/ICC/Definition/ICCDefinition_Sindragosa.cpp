/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_Sindragosa.h"
#include "ICCEncounterGate.h"
#include "ICCMultipliers_Sindragosa.h"
#include "ICCTriggers_Sindragosa.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;

namespace
{
void DefineSindragosa(EncounterBuilder& e)
{
    e.Node<IccSindragosaGroupPositionTrigger, IccSindragosaGroupPositionAction>(ACTION_RAID + 1);
    e.Node<IccSindragosaFrostBeaconTrigger, IccSindragosaFrostBeaconAction>(ACTION_RAID + 5);
    e.Node<IccSindragosaHotTrigger, IccSindragosaHotAction>(ACTION_RAID + 6);
    e.Node<IccSindragosaBlisteringColdTrigger, IccSindragosaBlisteringColdAction>(ACTION_EMERGENCY + 4);
    e.Node<IccSindragosaUnchainedMagicTrigger, IccSindragosaUnchainedMagicAction>(ACTION_RAID + 2);
    e.Node<IccSindragosaChilledToTheBoneTrigger, IccSindragosaChilledToTheBoneAction>(ACTION_RAID + 2);
    e.Node<IccSindragosaMysticBuffetTrigger, IccSindragosaMysticBuffetAction>(ACTION_RAID + 3);
    e.Node<IccSindragosaFrostBombTrigger, IccSindragosaFrostBombAction>(ACTION_RAID + 7);

    e.Multiplier<IccSindragosaMultiplier>(Family::AnyAction);
}
}  // namespace

EncounterDefinition const& IccSindragosaDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_SINDRAGOSA, IccPassThroughGate,
                                                "sindragosa", &DefineSindragosa);
    return definition;
}
