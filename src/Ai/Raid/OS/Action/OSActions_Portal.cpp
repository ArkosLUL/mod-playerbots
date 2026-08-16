/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */


#include "OSActions.h"
#include "OSTriggers.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "Random.h"
#include "Timer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

using namespace OsHelpers;

// Twilight Realm entry and exit.

bool EnterTwilightPortalAction::Execute(Event /*event*/)
{
    // The only unclamped destinations in this strategy, and they do not need clamping: the in-fight
    // portal is fixed at (3247.29, 529.80), 17yd from the southern Safe Area trigger and well inside
    // its Range Marker.
    GameObject* portal = bot->FindNearestGameObject(GoId::TwilightPortal, 100.0f);
    if (!portal)
        return false;

    // The portal is a fixed coordinate 2.2yd off the left wave line at 532, so a bot standing on clear
    // ground will still walk into a lane to click it. Reading the wave against the bot's own X is enough:
    // one that has passed the bot runs at 12 yd/s against its 7, so it sweeps the ground in between and
    // is gone before the bot gets there.
    if (!WaveClearsY(portal->GetPositionY(), ClassifyTsunamiWave(bot)))
        return false;

    if (!portal->IsAtInteractDistance(bot))
        // Explicit priority: the WorldObject overload defaults to MOVEMENT_NORMAL, which loses to
        // every hold in this strategy, so the walk to the portal never finished.
        return MoveTo(portal, fmaxf(portal->GetInteractionDistance() - 1.0f, 0.0f),
                      MovementPriority::MOVEMENT_COMBAT);

    WorldPacket data(CMSG_GAMEOBJ_USE);
    data << portal->GetGUID();
    bot->GetSession()->HandleGameObjectUseOpcode(data);

    return true;
}

bool ExitTwilightPortalAction::Execute(Event /*event*/)
{
    GameObject* portal = bot->FindNearestGameObject(GoId::NormalPortal, 100.0f);
    if (!portal)
        return false;

    // Same coordinate as the way in, and the same lane. The classification reaches through the phase wall
    // for this one - a shifted bot borrows the eyes of a groupmate still on the platform.
    if (!WaveClearsY(portal->GetPositionY(), ClassifyTsunamiWave(bot)))
        return false;

    if (!portal->IsAtInteractDistance(bot))
        // Explicit priority: the WorldObject overload defaults to MOVEMENT_NORMAL, which loses to
        // every hold in this strategy, so the walk to the portal never finished.
        return MoveTo(portal, fmaxf(portal->GetInteractionDistance() - 1.0f, 0.0f),
                      MovementPriority::MOVEMENT_COMBAT);

    WorldPacket data(CMSG_GAMEOBJ_USE);
    data << portal->GetGUID();
    bot->GetSession()->HandleGameObjectUseOpcode(data);

    return true;
}
