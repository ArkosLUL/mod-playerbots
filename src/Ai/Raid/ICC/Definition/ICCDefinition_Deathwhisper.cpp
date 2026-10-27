/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_Deathwhisper.h"
#include "ICCEncounterGate.h"
#include "ICCMultipliers_Deathwhisper.h"
#include "ICCTriggers_Deathwhisper.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;

namespace
{
void DefineDeathwhisper(EncounterBuilder& e)
{
    e.Node<IccDarkReckoningTrigger, IccDarkReckoningAction>(ACTION_MOVE + 5);
    e.Node<IccLadyDeathwhisperTrigger, IccRangedPositionLadyDeathwhisperAction>(ACTION_MOVE + 2);
    e.Node<IccLadyDeathwhisperTrigger, IccAddsLadyDeathwhisperAction>(ACTION_RAID + 3);
    e.Node<IccLadyDeathwhisperTrigger, IccShadeLadyDeathwhisperAction>(ACTION_RAID + 4);

    e.LiveMultiplier<IccLadyDeathwhisperMultiplier>(Family::AnyAction);
}
}  // namespace

EncounterDefinition const& IccDeathwhisperDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_LADY_DEATHWHISPER, BossStateGate,
                                                "lady-deathwhisper", &DefineDeathwhisper);
    return definition;
}
