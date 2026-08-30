/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "VoAHelpers.h"

#include "Creature.h"
#include "Group.h"
#include "GroupReference.h"
#include "NaxxBossHelper.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "Unit.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <mutex>
#include <unordered_set>

using namespace EncounterHelpers;

namespace VoaHelpers
{

Position const OFFTANK_CAMP = { -187.000f, -288.000f, 91.919f, 0.0f };

namespace
{

std::vector<uint32> const EMALON_ENTRIES = { NPC_EMALON, NPC_EMALON_HEROIC };
std::vector<uint32> const MINION_ENTRIES = { NPC_TEMPEST_MINION, NPC_TEMPEST_MINION_HEROIC };

float DegToRad(float degrees) { return degrees * static_cast<float>(M_PI) / 180.0f; }

// Each band is the intersection of the swept X ranges over its whole Y span, inset 1.5yd, so a point
// that survives the clamp is on the navmesh rather than merely near it. Banded rather than
// interpolated for the same reason: a band was swept, an interpolated edge was not.
void ChamberXRange(float y, float& minX, float& maxX)
{
    if (y <= -304.0f)
    {
        minX = -239.5f;
        maxX = -198.0f;
    }
    else if (y <= -294.0f)
    {
        minX = -241.5f;
        maxX = -196.0f;
    }
    else if (y <= -284.0f)
    {
        // The waist, and the only band the off-tank camp fits in.
        minX = -252.5f;
        maxX = -185.0f;
    }
    else if (y <= -274.0f)
    {
        minX = -242.0f;
        maxX = -196.0f;
    }
    else
    {
        minX = -239.5f;
        maxX = -198.5f;
    }
}

void CollectMinions(Player* bot, std::list<Creature*>& out)
{
    if (bot)
        bot->GetCreatureListWithEntryInGrid(out, MINION_ENTRIES, ROOM_SEARCH_RADIUS);
}

}

Unit* GetEmalon(Player* bot)
{
    if (!bot || bot->GetMapId() != VOA_MAP_ID)
        return nullptr;

    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, EMALON_ENTRIES, ROOM_SEARCH_RADIUS);
    for (Creature* creature : found)
    {
        if (creature && creature->IsAlive() && InsideChamber(creature))
            return creature;
    }
    return nullptr;
}

bool EmalonEncounterActive(Player* bot)
{
    Unit* boss = GetEmalon(bot);
    return boss && boss->IsInCombat();
}

bool IsTempestMinion(Unit const* unit)
{
    if (!unit)
        return false;

    return unit->GetEntry() == NPC_TEMPEST_MINION || unit->GetEntry() == NPC_TEMPEST_MINION_HEROIC;
}

std::vector<Unit*> LivingMinions(Player* bot)
{
    std::vector<Unit*> result;
    std::list<Creature*> found;
    CollectMinions(bot, found);
    for (Creature* creature : found)
    {
        if (creature && creature->IsAlive() && InsideChamber(creature))
            result.push_back(creature);
    }
    return result;
}

Unit* OverchargedMinion(Player* bot)
{
    for (Unit* minion : LivingMinions(bot))
    {
        if (minion->HasAura(AURA_OVERCHARGE))
            return minion;
    }
    return nullptr;
}

Unit* MinionToPickUp(Player* bot)
{
    std::vector<Unit*> minions = LivingMinions(bot);
    if (minions.empty())
        return nullptr;

    // A minion that is not already beating on this tank comes first, even ahead of the overcharged
    // one: a respawn pops at one of the four spawn corners with an empty threat list and takes
    // whoever is nearest, so an uncollected one is on a healer within seconds. Everything else is
    // already held, and holding it is not urgent.
    Unit* overcharged = nullptr;
    Unit* nearestLoose = nullptr;
    Unit* nearest = nullptr;
    float bestLoose = 0.0f;
    float best = 0.0f;
    for (Unit* minion : minions)
    {
        if (minion->HasAura(AURA_OVERCHARGE))
            overcharged = minion;

        float const dist = bot->GetExactDist2d(minion);
        if (!nearest || dist < best)
        {
            nearest = minion;
            best = dist;
        }

        if (minion->GetVictim() == bot)
            continue;

        if (!nearestLoose || dist < bestLoose)
        {
            nearestLoose = minion;
            bestLoose = dist;
        }
    }

    if (nearestLoose)
        return nearestLoose;

    // Then the overcharged one: it gains 20% damage per stack, so it is what the tank most wants to
    // be facing, and keeping it selected puts the raid's skull on it.
    return overcharged ? overcharged : nearest;
}

bool InsideChamber(WorldObject const* object)
{
    if (!object)
        return false;

    return object->GetPositionX() >= ROOM_MIN_X && object->GetPositionX() <= ROOM_MAX_X &&
           object->GetPositionY() >= ROOM_MIN_Y && object->GetPositionY() <= ROOM_MAX_Y &&
           object->GetPositionZ() <= ROOM_MAX_Z;
}

Player* GetOffTank(PlayerbotAI* botAI, Player* bot)
{
    return GetGroupAssistTank(bot, 0);
}

bool IsOffTank(Player* bot)
{
    return PlayerbotAI::IsAssistTankOfIndex(bot, 0, true);
}

bool RequireOffTank(PlayerbotAI* botAI, Player* bot)
{
    if (GetOffTank(botAI, bot))
        return true;

    Group* group = bot ? bot->GetGroup() : nullptr;
    if (!group)
        return false;

    // Warned once per group rather than once per bot, or a 25-man raid logs the same line 25 times.
    static std::mutex mutex;
    static std::unordered_set<uint32> warned;

    std::lock_guard<std::mutex> guard(mutex);
    if (warned.insert(group->GetGUID().GetCounter()).second)
        LOG_WARN("playerbots", "Emalon: raid has no assist tank, every off-tank behaviour stays inactive");

    return false;
}

Player* RedirectTarget(PlayerbotAI* botAI, Player* bot)
{
    // Unlike the usual pull-window shape, the off-tank owns every redirect for the whole fight: four
    // minions are summoned on Reset and a dead one is back in 4s, so there is no window where the main
    // tank is the better answer. Emalon needs no help - single tank, no threat mechanic - while a
    // minion landing on a healer is what actually kills people here.
    if (!LivingMinions(bot).empty())
    {
        if (Player* offTank = GetOffTank(botAI, bot))
            return offTank;
    }

    return GetGroupMainTank(bot);
}

void ClampToChamber(float& x, float& y)
{
    // Y first: both X edges are functions of it, so clamping X against an out-of-range Y would pick
    // the wrong band.
    y = std::clamp(y, CHAMBER_MIN_Y, CHAMBER_MAX_Y);

    float minX = 0.0f;
    float maxX = 0.0f;
    ChamberXRange(y, minX, maxX);
    x = std::clamp(x, minX, maxX);
}

bool RingSlotFor(PlayerbotAI* botAI, Player* bot, Unit* boss, float& x, float& y)
{
    if (!botAI || !bot || !boss)
        return false;

    if (botAI->IsTank(bot))
        return false;

    bool const healer = botAI->IsHeal(bot);
    if (!healer && !botAI->IsRanged(bot))
        return false;

    NaxxRoleGroups groups = NaxxGetRoleGroups(botAI, bot);
    std::pair<size_t, size_t> const slot = NaxxGetSlotIndexAndCount(botAI, bot, groups);

    float const radius = healer ? HEALER_RING_RADIUS : RANGED_RING_RADIUS;
    float const arc = DegToRad(healer ? HEALER_RING_ARC_DEGREES : RANGED_RING_ARC_DEGREES);

    // Centred on the bearing from the boss to the entrance, which is +Y and therefore a constant. The
    // Naxx ring has to latch its equivalent because a boss standing near the room centre swings the
    // bearing between ticks; nothing swings this one, so there is no latch to get wrong.
    float const base = static_cast<float>(M_PI) / 2.0f;
    float const angle =
        base - arc / 2.0f + arc * (static_cast<float>(slot.first) + 0.5f) / static_cast<float>(slot.second);

    x = boss->GetPositionX() + std::cos(angle) * radius;
    y = boss->GetPositionY() + std::sin(angle) * radius;
    ClampToChamber(x, y);
    return true;
}

}
