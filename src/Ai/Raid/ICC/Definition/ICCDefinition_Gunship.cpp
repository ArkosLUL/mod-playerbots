/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_Gunship.h"
#include "ICCEncounterGate.h"
#include "ICCMultipliers_Gunship.h"
#include "ICCTriggers_Gunship.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;

namespace
{
void DefineGunship(EncounterBuilder& e)
{
    e.Node<IccGunshipCannonNearTrigger, IccGunshipEnterCannonAction>(ACTION_RAID + 6);
    e.Node<IccInCannonTrigger, IccCannonFireAction>(ACTION_RAID + 5);
    e.Node<IccGunshipRocketJumpTrigger, IccGunshipRocketJumpAction>(ACTION_RAID + 4);
    e.Node<IccGunshipRocketPackSetupTrigger, IccGunshipRocketPackSetupAction>(ACTION_RAID + 2);

    e.Multiplier<IccGunshipMultiplier>(Family::AnyAction);
}
}  // namespace

EncounterDefinition const& IccGunshipDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_GUNSHIP_BATTLE, IccPassThroughGate,
                                                "icecrown-gunship-battle", &DefineGunship);
    return definition;
}
