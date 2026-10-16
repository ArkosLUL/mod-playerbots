/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OnyActions.h"
#include "OnyHelpers.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

using namespace OnyHelpers;

bool OnyxiaDodgeAction::Execute(Event /*event*/)
{
    uint8 const reasons = DodgeReasons(botAI);
    Position destination;
    if (!DodgeDestination(botAI, reasons, destination))
        return false;

    // Fireball lands on one neighbour for a cast's worth of damage; only a crowd is worth the cast.
    bool const urgent = reasons & (DodgeReason::Egg | DodgeReason::Breath | DodgeReason::Nova);
    if (urgent || FireballNeighbours(bot) >= 2)
        bot->InterruptNonMeleeSpells(true);

    return MoveTo(ONYXIA_MAP_ID, destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ(),
                  false, false, false, false, MovementPriority::MOVEMENT_FORCED, true);
}
