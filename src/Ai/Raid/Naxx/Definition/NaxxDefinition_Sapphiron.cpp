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
void DefineSapphiron(EncounterBuilder& e)
{
    e.Node<SapphironGroundTrigger, SapphironGroundPositionAction>(ACTION_RAID + 1);
    e.Node<SapphironFlightTrigger, SapphironFlightPositionAction>(ACTION_RAID + 1);

    e.Multiplier<SapphironGenericMultiplier>(Family::AnyAction);
}
}  // namespace

EncounterDefinition const& NaxxSapphironDefinition()
{
    static EncounterDefinition const definition(NAXX_BOSS_SAPPHIRON, BossStateGate, "sapphiron", &DefineSapphiron);
    return definition;
}
