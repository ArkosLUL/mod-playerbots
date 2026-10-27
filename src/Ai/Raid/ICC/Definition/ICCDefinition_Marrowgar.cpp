/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_Marrowgar.h"
#include "ICCEncounterGate.h"
#include "ICCTriggers_Marrowgar.h"
#include "Strategy.h"

namespace
{
void DefineMarrowgar(EncounterBuilder& e)
{
    e.Node<IccLmTrigger, IccLmTankPositionAction>(ACTION_RAID + 5);
    e.Node<IccLmTrigger, IccSpikeAction>(ACTION_RAID + 3);
}
}  // namespace

EncounterDefinition const& IccMarrowgarDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_LORD_MARROWGAR, BossStateGate,
                                                "lord-marrowgar", &DefineMarrowgar);
    return definition;
}
