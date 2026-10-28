/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCActions_FrostGiant.h"

#include <cmath>

#include "ICCHelpers_FrostGiant.h"
#include "LastMovementValue.h"
#include "Playerbots.h"

namespace
{
constexpr float RFG_DODGE_SWEEP = 25.0f;
// A walk already headed this close to the partner is left running.
constexpr float RFG_REAIM = 2.0f;
}  // namespace

bool IccRottingFrostGiantPlagueAction::Execute(Event /*event*/)
{
    if (Player* mate = RfgPairWalk(botAI))
    {
        // Beside the mate nothing is left to do, so casts go on.
        if (bot->GetExactDist2d(mate) <= RFG_PAIR_DISTANCE + 1.0f)
            return false;

        LastMovement& last = AI_VALUE(LastMovement&, "last movement");
        if (bot->isMoving() && last.msTime &&
            mate->GetExactDist2d(last.lastMoveToX, last.lastMoveToY) <= RFG_PAIR_DISTANCE + RFG_REAIM)
            return true;

        return MoveTo(mate, RFG_PAIR_DISTANCE - 1.0f, MovementPriority::MOVEMENT_COMBAT);
    }

    std::vector<EncounterHelpers::HazardCircle> const carriers = RfgCarrierHazards(botAI);
    if (carriers.empty())
        return false;

    // Without a preferred point the sweep takes a fixed compass heading, often away from the giant.
    Unit* target = AI_VALUE(Unit*, "current target");
    Position const prefer = target ? target->GetPosition() : bot->GetPosition();
    Position const spot = EncounterHelpers::FindNearestPositionClearOfHazards(
        bot, carriers, RFG_DODGE_SWEEP, 2.0f, static_cast<float>(M_PI) / 8.0f, &prefer);
    if (!spot.GetPositionX() && !spot.GetPositionY())
        return false;

    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false, false,
                  false, MovementPriority::MOVEMENT_COMBAT);
}
