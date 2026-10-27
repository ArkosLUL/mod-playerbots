/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_LichKing.h"
#include "ICCEncounterGate.h"
#include "ICCMultipliers_LichKing.h"
#include "ICCTriggers_LichKing.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;

namespace
{
void DefineLichKing(EncounterBuilder& e)
{
    e.Node<IccLichKingShadowTrapTrigger, IccLichKingShadowTrapAction>(ACTION_RAID + 6);
    e.Node<IccLichKingNecroticPlagueTrigger, IccLichKingNecroticPlagueAction>(ACTION_RAID + 3);
    e.Node<IccLichKingWinterTrigger, IccLichKingWinterAction>(ACTION_RAID + 5);
    e.Node<IccLichKingAddsTrigger, IccLichKingAddsAction>(ACTION_RAID + 2);
    e.Node<IccLichKingSpiritBombTrigger, IccLichKingSpiritBombAction>(ACTION_RAID + 7);

    e.Multiplier<IccLichKingAddsMultiplier>(Family::AnyAction);
    e.Multiplier<IccLichKingSpiritBombMultiplier>(Family::AnyAction);
}
}  // namespace

EncounterDefinition const& IccLichKingDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_THE_LICH_KING, IccPassThroughGate,
                                                "the-lich-king", &DefineLichKing);
    return definition;
}
