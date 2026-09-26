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
void DefineRazuvious(EncounterBuilder& e)
{
    e.Node<RazuviousTankTrigger, RazuviousUseObedienceCrystalAction>(ACTION_RAID + 1);
    e.Node<RazuviousNontankTrigger, RazuviousTargetAction>(ACTION_RAID + 1);

    // Every action, because it sets "neglect threat" and ThreatMultiplier clears that on each read.
    e.Multiplier<InstructorRazuviousGenericMultiplier>(Family::AnyAction);
}
}  // namespace

EncounterDefinition const& NaxxRazuviousDefinition()
{
    static EncounterDefinition const definition(NAXX_BOSS_RAZUVIOUS, BossStateGate, "instructor razuvious",
                                                &DefineRazuvious);
    return definition;
}
