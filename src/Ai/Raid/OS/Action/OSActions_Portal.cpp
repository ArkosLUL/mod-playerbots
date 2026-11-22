/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OSActions.h"
#include "OSTriggers.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "Random.h"
#include "Timer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

using namespace OsHelpers;

bool EnterTwilightPortalAction::Execute(Event /*event*/)
{
    GameObject* portal = bot->FindNearestGameObject(GoId::TwilightPortal, 100.0f);
    if (!portal)
        return false;

    if (!WaveClearsY(portal->GetPositionY(), ClassifyTsunamiWave(bot)))
        return false;

    if (!portal->IsAtInteractDistance(bot))
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

    if (!WaveClearsY(portal->GetPositionY(), ClassifyTsunamiWave(bot)))
        return false;

    if (!portal->IsAtInteractDistance(bot))
        return MoveTo(portal, fmaxf(portal->GetInteractionDistance() - 1.0f, 0.0f),
                      MovementPriority::MOVEMENT_COMBAT);

    WorldPacket data(CMSG_GAMEOBJ_USE);
    data << portal->GetGUID();
    bot->GetSession()->HandleGameObjectUseOpcode(data);

    return true;
}
