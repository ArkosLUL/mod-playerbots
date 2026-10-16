/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "LastMovementValue.h"
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

    // She turns and walks while the bot is on its way, and IsWaitingForLastMove holds an equal
    // priority walk for its whole length, up to 5s. So a walk aimed where she was steps down one
    // priority to let the new point through. A forced dodge still blocks it.
    LastMovement& lastMove = AI_VALUE(LastMovement&, "last movement");
    MovementPriority const held = lastMove.priority;
    bool const reaim = held == MovementPriority::MOVEMENT_COMBAT && bot->isMoving() && FlankPointMoved(bot, point);
    if (reaim)
        lastMove.priority = MovementPriority::MOVEMENT_NORMAL;

    if (!MoveTo(ONYXIA_MAP_ID, point.GetPositionX(), point.GetPositionY(), point.GetPositionZ(), false, false, false,
                false, MovementPriority::MOVEMENT_COMBAT, true))
    {
        if (reaim)
            lastMove.priority = held;

        return false;
    }

    MarkFlankIssued(bot, point);
    return true;
}

bool OnyxiaAntiFearAction::FearWindowActive() { return OnyHelpers::FearWindowActive(botAI); }
