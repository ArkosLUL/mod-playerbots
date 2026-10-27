#include "ToCActions_Shared.h"
#include "ToCData.h"
#include "ToCHelpers_Shared.h"
#include "Playerbots.h"

#include <algorithm>

using namespace TrialOfTheCrusaderHelpers;

bool AvoidCreatureClusterAction::FleeFromCreatureCluster(uint32 entry)
{
    // Scan wider than the trigger radius so the escape vector runs from the centre of the whole cluster,
    // not the nearest patch, and does not push the bot from one patch straight into the next.
    constexpr float clusterRadius = 15.0f;
    Position center;
    if (!GetCreatureClusterCenter(bot, entry, clusterRadius, center))
        return false;

    bot->CastStop();

    constexpr float fleeDistance = 12.0f;
    constexpr uint32 minInterval = 500;
    return FleePosition(center, fleeDistance, minInterval);
}

bool ToCMainTankHoldAction::DragBossToAnchor(Unit* boss, Position const& anchor)
{
    if (boss->GetVictim() != bot)
        return false;

    const float distToPosition = bot->GetExactDist2d(anchor.GetPositionX(), anchor.GetPositionY());
    if (distToPosition <= 12.0f)
        return false;

    const float dX = anchor.GetPositionX() - bot->GetPositionX();
    const float dY = anchor.GetPositionY() - bot->GetPositionY();
    const float moveDist = std::min(5.0f, distToPosition);
    const float moveX = bot->GetPositionX() + (dX / distToPosition) * moveDist;
    const float moveY = bot->GetPositionY() + (dY / distToPosition) * moveDist;

    return MoveTo(TRIAL_OF_THE_CRUSADER_MAP_ID, moveX, moveY, anchor.GetPositionZ(),
                  false, false, false, false, MovementPriority::MOVEMENT_COMBAT, true, true);
}
