/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxDefinitions.h"

#include "NaxxActions.h"
#include "NaxxTriggers.h"
#include "Playerbots.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
bool GrobbulusUp(PlayerbotAI* botAI)
{
    return botAI->GetAiObjectContext()->GetValue<Unit*>("find target", "grobbulus")->Get() != nullptr;
}

void DefineGrobbulus(EncounterBuilder& e)
{
    e.Node<MutatingInjectionMeleeTrigger, GrobbulusMoveAwayAction>(ACTION_RAID + 2);
    e.Node<MutatingInjectionRangedTrigger, GrobbulusGoBehindAction>(ACTION_RAID + 2);
    e.Node<MutatingInjectionRemovedTrigger, GrobblulusMoveCenterAction>(ACTION_RAID + 1);
    e.Node<GrobbulusCloudTrigger, GrobbulusRotateAction>(ACTION_RAID + 1);

    e.Block("grobbulus", Role::MainTank, GrobbulusUp, Family::AvoidAoe);
    e.Block("grobbulus", Role::Any, GrobbulusUp,
            Family::CombatFormationMove | Family::TankFace | Family::SetBehind);
}
}  // namespace

EncounterDefinition const& NaxxGrobbulusDefinition()
{
    static EncounterDefinition const definition(NAXX_BOSS_GROBBULUS, BossStateGate, "grobbulus", &DefineGrobbulus);
    return definition;
}
