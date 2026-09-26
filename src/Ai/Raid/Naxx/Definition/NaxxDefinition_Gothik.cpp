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
void DefineGothik(EncounterBuilder& e)
{
    // The whole raid fights on the living side, so getting back across outranks whatever the bot was
    // shooting at.
    e.Node<GothikWrongSideTrigger, GothikStayOnLivingSideAction>(ACTION_RAID + 4);
    e.Node<GothikTrigger, GothikChooseTargetAction>(ACTION_RAID + 1);

    // Every action, because it sets "neglect threat" and ThreatMultiplier clears that on each read.
    e.Multiplier<GothikGenericMultiplier>(Family::AnyAction);
}
}  // namespace

EncounterDefinition const& NaxxGothikDefinition()
{
    static EncounterDefinition const definition(NAXX_BOSS_GOTHIK, BossStateGate, "gothik the harvester",
                                                &DefineGothik);
    return definition;
}
