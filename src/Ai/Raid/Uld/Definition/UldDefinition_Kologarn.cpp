/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldDefinitions.h"

#include "BossAuraActions.h"
#include "BossAuraTriggers.h"
#include "BossResistanceMultipliers.h"
#include "PlayerbotAI.h"
#include "Strategy.h"
#include "UldActions_Kologarn.h"
#include "UldEncounterGate.h"
#include "UldEncounter_Kologarn.h"
#include "UldTriggers_Kologarn.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
bool KologarnOwnsTargets(PlayerbotAI* botAI)
{
    return botAI->GetState() != BOT_STATE_NON_COMBAT && KologarnEncounterActive(botAI);
}

bool KologarnStoneGripRide(PlayerbotAI* botAI) { return IsKologarnStoneGripped(botAI->GetBot()) && GetKologarn(botAI); }

void DefineKologarn(EncounterBuilder& e)
{
    e.Node<KologarnBodyTankTrigger, KologarnBodyTankAction>(ACTION_RAID);
    e.Node<KologarnOffTankTrigger, KologarnOffTankAction>(ACTION_RAID);
    e.Node<KologarnDpsTargetTrigger, KologarnDpsTargetAction>(ACTION_RAID);

    e.Node<KologarnRubbleTankTrigger, KologarnRubbleTankAction>(ACTION_RAID + 1);
    e.Node<KologarnRubbleSlowdownTrigger, KologarnRubbleSlowdownAction>(ACTION_RAID);

    e.Node<KologarnSmashSwapTrigger, KologarnSmashSwapAction>(ACTION_RAID + 2);

    e.Node(
        "kologarn nature resistance trigger",
        [](PlayerbotAI* botAI) -> Trigger* { return new BossNatureResistanceTrigger(botAI, "kologarn"); },
        "kologarn nature resistance action",
        [](PlayerbotAI* botAI) -> Action* { return new BossNatureResistanceAction(botAI, "kologarn"); }, ACTION_RAID);
    e.Node<KologarnFallFromFloorTrigger, KologarnFallFromFloorAction>(ACTION_RAID + 1);

    e.Node<KologarnBodyUncoveredTrigger, KologarnBodyUncoveredAction>(ACTION_EMERGENCY + 1);
    e.Node<KologarnEyebeamTrigger, KologarnEyebeamAction>(ACTION_EMERGENCY);

    e.OwnTargeting("kologarn disable automatic targeting", Role::Any, KologarnOwnsTargets,
                   Family::DpsAssist | Family::TankAssist | Family::DebuffOnAttacker);

    e.Exclusive("kologarn", Role::Any, KologarnStoneGripRide);

    e.Multiplier<BossNatureAspectHoldMultiplier>(Family::Spell, "kologarn");
}
}

EncounterDefinition const& UldKologarnDefinition()
{
    static EncounterDefinition const definition(ULD_BOSS_KOLOGARN, BossStateGate, "kologarn", &DefineKologarn);
    return definition;
}
