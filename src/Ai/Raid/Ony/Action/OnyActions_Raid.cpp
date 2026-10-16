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

bool OnyxiaChooseTargetAction::Execute(Event /*event*/)
{
    Unit* target = OnyxiaPickTarget(botAI);
    return target && Attack(target);
}

bool OnyxiaGroundSlotAction::Execute(Event /*event*/)
{
    Position slot;
    if (!GroundSlotFor(botAI, slot) || bot->GetExactDist2d(&slot) <= ONYXIA_SLOT_ARRIVAL)
        return false;

    return MoveTo(ONYXIA_MAP_ID, slot.GetPositionX(), slot.GetPositionY(), slot.GetPositionZ(), false, false, false,
                  false, MovementPriority::MOVEMENT_COMBAT, true);
}

bool OnyxiaAirSpotAction::Execute(Event /*event*/)
{
    Position spot;
    if (!AirSpotFor(botAI, spot) || bot->GetExactDist2d(&spot) <= ONYXIA_SLOT_ARRIVAL)
        return false;

    return MoveTo(ONYXIA_MAP_ID, spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false, false,
                  false, MovementPriority::MOVEMENT_COMBAT, true);
}

bool OnyxiaRearFlankAction::Execute(Event /*event*/)
{
    Position point;
    if (!RearFlankPointFor(botAI, point, false))
        return false;

    return MoveTo(ONYXIA_MAP_ID, point.GetPositionX(), point.GetPositionY(), point.GetPositionZ(), false, false, false,
                  false, MovementPriority::MOVEMENT_COMBAT, true);
}

bool OnyxiaAntiFearAction::FearWindowActive() { return OnyHelpers::FearWindowActive(botAI); }
