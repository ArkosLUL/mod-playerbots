/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_Valithria.h"
#include "ICCEncounterGate.h"
#include "ICCMultipliers_Valithria.h"
#include "ICCTriggers_Valithria.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;

namespace
{
void DefineValithria(EncounterBuilder& e)
{
    e.Node<IccValithriaGroupTrigger, IccValithriaGroupAction>(ACTION_RAID + 1);
    e.Node<IccValithriaZombieKiteTrigger, IccValithriaZombieKiteAction>(ACTION_EMERGENCY + 9);
    e.Node<IccValithriaPortalTrigger, IccValithriaPortalAction>(ACTION_RAID + 5);
    e.Node<IccValithriaHealTrigger, IccValithriaHealAction>(ACTION_RAID + 2);
    e.Node<IccValithriaDreamCloudTrigger, IccValithriaDreamCloudAction>(ACTION_RAID + 4);

    e.Multiplier<IccValithriaDreamCloudMultiplier>(Family::AnyAction);
}
}  // namespace

EncounterDefinition const& IccValithriaDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_VALITHRIA_DREAMWALKER, IccPassThroughGate,
                                                "valithria-dreamwalker", &DefineValithria);
    return definition;
}
