/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "Creature.h"
#include "ICCActions_FrostGiant.h"
#include "ICCData.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_Shared.h"
#include "ICCTriggers_FrostGiant.h"
#include "PlayerbotAI.h"
#include "Strategy.h"

namespace
{
void TickFrostGiant(PlayerbotAI* botAI)
{
    if (Creature* giant = UnitGateLiveUnit<NPC_ROTTING_FROST_GIANT_10, NPC_ROTTING_FROST_GIANT_25>(botAI))
        IccMarkPull(botAI->GetBot(), giant, "rotting-frost-giant");
}

void DefineFrostGiant(EncounterBuilder& e)
{
    e.Node<IccRottingFrostGiantTankPositionTrigger, IccRottingFrostGiantTankPositionAction>(ACTION_RAID + 5);

    e.Tick(&TickFrostGiant);
}
}  // namespace

EncounterDefinition const& IccFrostGiantDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_NONE,
                                                UnitGate<NPC_ROTTING_FROST_GIANT_10, NPC_ROTTING_FROST_GIANT_25>,
                                                "rotting-frost-giant", &DefineFrostGiant);
    return definition;
}
