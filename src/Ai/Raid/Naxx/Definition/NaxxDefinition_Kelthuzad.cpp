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
void DefineKelthuzad(EncounterBuilder& e)
{
    e.Node<KelthuzadTrigger, KelthuzadMisdirectBossToMainTankAction>(ACTION_RAID + 3);
    e.Node<KelthuzadTrigger, KelthuzadPositionAction>(ACTION_RAID + 2);
    e.Node<KelthuzadTrigger, KelthuzadChooseTargetAction>(ACTION_RAID + 1);

    // Emergency priority so the flee beats every P2 positioning action by construction.
    e.Node<KelthuzadShadowFissureTrigger, KelthuzadFleeShadowFissureAction>(ACTION_EMERGENCY + 6);
    e.Node<KelthuzadChainsTrigger, KelthuzadCycloneChainedAction>(ACTION_EMERGENCY + 5);

    e.Multiplier<KelthuzadGenericMultiplier>(Family::AnyAction);
}
}  // namespace

EncounterDefinition const& NaxxKelthuzadDefinition()
{
    static EncounterDefinition const definition(NAXX_BOSS_KELTHUZAD, BossStateGate, "kel'thuzad", &DefineKelthuzad);
    return definition;
}
