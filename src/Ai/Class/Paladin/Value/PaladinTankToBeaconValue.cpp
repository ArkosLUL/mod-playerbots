/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "PaladinTankToBeaconValue.h"

#include "Group.h"
#include "Playerbots.h"

namespace
{
// Health percentage the new candidate has to beat the current one by before the buff moves. Beacon
// and Sacred Shield both cost a global, so without a margin two tanks trading damage keep the
// paladin re-buffing instead of healing.
constexpr float SWAP_MARGIN_PCT = 30.0f;

// Health difference below which two tanks count as equally hurt. Otherwise the pull, where everyone
// is at 100%, is decided by group order and the swap margin then keeps the buff there.
constexpr float TIE_PCT = 1.0f;
}  // namespace

Unit* PaladinTankToBeaconValue::Calculate()
{
    Player* lowest = nullptr;
    Player* previous = nullptr;

    Unit* mainTank = AI_VALUE(Unit*, "main tank");
    ObjectGuid const mainTankGuid = mainTank ? mainTank->GetGUID() : ObjectGuid::Empty;

    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* player = ref->GetSource();
            if (!player || !player->IsInWorld() || !player->IsAlive() || !player->IsInCombat())
                continue;

            if (!botAI->IsTank(player))
                continue;

            if (player->GetMapId() != bot->GetMapId() ||
                bot->GetDistance(player) >= sPlayerbotAIConfig.healDistance ||
                !bot->IsWithinLOSInMap(player))
                continue;

            if (player->GetGUID() == lastTank)
                previous = player;

            if (!lowest || player->GetHealthPct() < lowest->GetHealthPct() - TIE_PCT)
                lowest = player;
            else if (player->GetGUID() == mainTankGuid &&
                     player->GetHealthPct() < lowest->GetHealthPct() + TIE_PCT)
                lowest = player;
        }
    }

    if (!lowest)
    {
        lastTank.Clear();
        return AI_VALUE(Unit*, "main tank");
    }

    if (previous && previous != lowest && lowest->GetHealthPct() > previous->GetHealthPct() - SWAP_MARGIN_PCT)
        lowest = previous;

    lastTank = lowest->GetGUID();
    return lowest;
}
