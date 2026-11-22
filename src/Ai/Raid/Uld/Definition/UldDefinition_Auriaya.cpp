/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldDefinitions.h"

#include "Strategy.h"
#include "UldActions_Auriaya.h"
#include "UldEncounterGate.h"
#include "UldEncounter_Auriaya.h"
#include "UldMultipliers_Auriaya.h"
#include "UldTriggers_Auriaya.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
void DefineAuriaya(EncounterBuilder& e)
{
    e.Node<AuriayaFallFromFloorTrigger, AuriayaFallFromFloorAction>(ACTION_RAID + 4);

    e.Node<AuriayaSentryTauntTrigger, AuriayaSentryTauntAction>(ACTION_RAID + 3, EncounterRow::Mover);
    e.Node<AuriayaMainTankSentryTrigger, AuriayaMainTankSentryAction>(ACTION_RAID + 3);

    e.Node<AuriayaSeepingEssenceTrigger, AuriayaSeepingEssenceAction>(ACTION_RAID + 2, EncounterRow::Mover);
    e.Node<AuriayaAntiFearTrigger, AuriayaAntiFearAction>(ACTION_RAID + 2);
    e.Node<AuriayaSetDpsPriorityTrigger, AuriayaSetDpsPriorityAction>(ACTION_RAID + 1);
    e.Node<AuriayaOffTankTargetTrigger, AuriayaOffTankTargetAction>(ACTION_RAID + 1);
    e.Node<AuriayaRaidPositionTrigger, AuriayaRaidPositionAction>(ACTION_RAID, EncounterRow::Mover);

    e.OwnTargeting("auriaya movement guard multiplier", Role::NonTank, IsAuriayaEngaged, Family::DpsAssist);

    e.OwnTargeting("auriaya off tank target guard", Role::Tank, AuriayaOffTankHoldsSentries, Family::TankAssist);

    e.OwnTargeting("auriaya main tank target guard", Role::MainTank, AuriayaMainTankAlone, Family::TankAssist);

    e.OwnMovement("auriaya movement guard multiplier", Role::MainTank | Role::Ranged, IsAuriayaEngaged,
                  Family::Attack | Family::Reach | Family::ReachHeal);

    e.Multiplier<AuriayaAntiFearTotemGuardMultiplier>(Family::Spell);
}
}

EncounterDefinition const& UldAuriayaDefinition()
{
    static EncounterDefinition const definition(ULD_BOSS_AURIAYA, BossStateGate, "auriaya", &DefineAuriaya);
    return definition;
}
