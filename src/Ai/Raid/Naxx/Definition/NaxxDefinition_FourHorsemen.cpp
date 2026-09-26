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
void DefineFourHorsemen(EncounterBuilder& e)
{
    e.Node<HorsemanAttractorsTrigger, HorsemanAttractAlternativelyAction>(ACTION_RAID + 1);
    e.Node<HorsemanExceptAttractorsTrigger, HorsemanAttactInOrderAction>(ACTION_RAID + 1);

    // Only live for the pull window, so it can outrank the attractor rotation while it lasts.
    e.Node<FourhorsemanRedirectThreatTrigger, FourhorsemanRedirectThreatAction>(ACTION_RAID + 4);

    // Every action, because it sets "neglect threat" and ThreatMultiplier clears that on each read.
    e.Multiplier<FourhorsemanGenericMultiplier>(Family::AnyAction);
}
}  // namespace

EncounterDefinition const& NaxxFourHorsemenDefinition()
{
    static EncounterDefinition const definition(NAXX_BOSS_HORSEMAN, BossStateGate, "four horsemen",
                                                &DefineFourHorsemen);
    return definition;
}
