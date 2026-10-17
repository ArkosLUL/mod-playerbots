#include "ToCActions_Staging.h"

#include "ToCHelpers_Staging.h"

using namespace TrialOfTheCrusaderHelpers;

bool ToCMoveToStagingStandAction::Execute(Event /*event*/)
{
    ToCStagingRead const read = GetToCStagingRead(botAI);
    if (!read.held || !read.walk)
    {
        DropWalk();
        return false;
    }

    if (Position const* spot = WalkInFlight())
        if (spot->GetExactDist2d(read.spot) <= STAGING_STAND_MOVED)
            return true;

    return WalkTo(read.spot, MovementPriority::MOVEMENT_NORMAL);
}
