/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_Svalna.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_Svalna.h"
#include "ICCSeams.h"
#include "ICCTriggers_Svalna.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
void DefineSvalna(EncounterBuilder& e)
{
    e.Node<IccSvalnaSpearTrigger, IccSvalnaSpearAction>(ACTION_EMERGENCY + 5, EncounterRow::Mover);
    e.Node<IccSvalnaHurlTrigger, IccSvalnaHurlAction>(ACTION_RAID + 7, EncounterRow::Mover);
    e.Node<IccSvalnaTauntTrigger, IccSvalnaTauntAction>(ACTION_RAID + 6);
    e.Node<IccSvalnaTargetTrigger, IccSvalnaTargetAction>(ACTION_RAID + 5);

    e.OwnTargeting("icc sister svalna targeting", Role::Any, &IccResolverPicks<&GetSsTarget>,
                   Family::DpsAssist | Family::TankAssist | Family::DpsAoe);

    e.Tick(&TickSsEncounter);
}
}  // namespace

EncounterDefinition const& IccSvalnaDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_SISTER_SVALNA, BossStateGate, "sister-svalna",
                                                &DefineSvalna);
    return definition;
}
