/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxDefinitions.h"

#include "NaxxActions.h"
#include "NaxxMultipliers.h"
#include "NaxxTriggers.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;

namespace
{
void DefineHeigan(EncounterBuilder& e)
{
    e.Node<HeiganMeleeTrigger, HeiganDanceMeleeAction>(ACTION_RAID + 1);
    e.Node<HeiganRangedTrigger, HeiganDanceRangedAction>(ACTION_RAID + 1);

    // Priority: dispel Decrepit Fever ASAP (tank first) during Phase 1.
    e.Node<HeiganDecrepitFeverTrigger, HeiganDispelDecrepitFeverAction>(ACTION_RAID + 5);

    e.Multiplier<HeiganDanceMultiplier>(Family::AnyAction);
}
}  // namespace

EncounterDefinition const& NaxxHeiganDefinition()
{
    static EncounterDefinition const definition(NAXX_BOSS_HEIGAN, BossStateGate, "heigan the unclean", &DefineHeigan);
    return definition;
}
