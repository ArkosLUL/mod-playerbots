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
void DefineAnubrekhan(EncounterBuilder& e)
{
    e.Node<AnubrekhanTrigger, AnubrekhanRedirectThreatAction>(ACTION_RAID + 3);
    e.Node<AnubrekhanTrigger, AnubrekhanPositionAction>(ACTION_RAID + 2);
    e.Node<AnubrekhanTrigger, AnubrekhanChooseTargetAction>(ACTION_RAID + 1);

    // The swarm is the one window where losing the formation wipes the raid, so holding it outranks
    // everything else the engine might want to do.
    e.Node<AnubrekhanLocustSwarmTrigger, AnubrekhanPositionAction>(ACTION_EMERGENCY + 5);

    e.Multiplier<AnubrekhanGenericMultiplier>(Family::AnyMovement);
}
}  // namespace

EncounterDefinition const& NaxxAnubrekhanDefinition()
{
    static EncounterDefinition const definition(NAXX_BOSS_ANUB, BossStateGate, "anub'rekhan", &DefineAnubrekhan);
    return definition;
}
