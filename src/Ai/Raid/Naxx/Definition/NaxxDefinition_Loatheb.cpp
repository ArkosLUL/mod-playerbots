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
void DefineLoatheb(EncounterBuilder& e)
{
    e.Node<LoathebTrigger, LoathebPositionAction>(ACTION_RAID + 1);
    e.Node<LoathebTrigger, LoathebChooseTargetAction>(ACTION_RAID + 1);

    // Every action, because it sets "neglect threat" and ThreatMultiplier clears that on each read.
    e.Multiplier<LoathebGenericMultiplier>(Family::AnyAction);
}
}  // namespace

EncounterDefinition const& NaxxLoathebDefinition()
{
    static EncounterDefinition const definition(NAXX_BOSS_LOATHEB, BossStateGate, "loatheb", &DefineLoatheb);
    return definition;
}
