/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "Creature.h"
#include "ICCActions_Dogs.h"
#include "ICCData.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_Shared.h"
#include "ICCMultipliers_Dogs.h"
#include "ICCTriggers_Dogs.h"
#include "PlayerbotAI.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;

namespace
{
void TickDogs(PlayerbotAI* botAI)
{
    if (Creature* dog = UnitGateLiveUnit<NPC_STINKY, NPC_PRECIOUS>(botAI))
        IccMarkPull(botAI->GetBot(), dog, "stinky-and-precious");
}

void DefineDogs(EncounterBuilder& e)
{
    e.Node<IccDogsTrigger, IccDogsTankPositionAction>(ACTION_RAID + 3);

    e.LiveMultiplier<IccDogsMultiplier>(Family::AnyAction);

    e.Tick(&TickDogs);
}
}  // namespace

EncounterDefinition const& IccDogsDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_NONE, UnitGate<NPC_STINKY, NPC_PRECIOUS>,
                                                "stinky-and-precious", &DefineDogs);
    return definition;
}
