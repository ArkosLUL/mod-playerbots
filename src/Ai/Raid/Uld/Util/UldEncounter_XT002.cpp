/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_XT002.h"

#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "Unit.h"

#include <algorithm>
#include <list>
#include <string>
#include <vector>

using namespace EncounterHelpers;

// XT-002 anchors. XT spawns at (886.28, -12.05) facing -x, so the tank spot sits just behind him and
// the boss settles roughly between the two lines. The ranged spot is pulled in from the value that
// was measured in-game because a 30yd caster clipped out of range there and walked in every tick.
const Position ULDUAR_XT002_MAINTANK_SPOT = Position(895.82f, -12.53954f, 409.68756f);
const Position ULDUAR_XT002_RANGED_SPOT = Position(866.0f, -12.5f, 409.8f);
// South-west of the raid, 15.6yd from the nearest formation slot and 19.2yd from the nearest Gravity
// Bomb cell, so an 8yd Searing Light reaches neither. North of the raid looks tempting and is not:
// findSmoothPath answers PATHFIND_NOPATH there from every melee position probed, while every point
// out here paths normally from all of them. 38yd from the melee stack, inside a 9s run.
const Position ULDUAR_XT002_SEARING_LIGHT_SPOT = Position(846.0f, -22.0f, 409.597f);
// Two origins so melee and ranged carriers do not drop Void Zones on top of each other. Each is the
// corner of its grid nearest the raid - roughly 30yd from the carrier's usual spot, which is the run
// that has to fit inside the 9s the debuff lasts.
const Position ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_MELEE = Position(871.5199f, -42.04216f, 409.80377f);
const Position ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_RANGED = Position(837.0746f, -41.01061f, 409.80362f);

// XT and his Heart both spend part of the fight carrying UNIT_FLAG_NOT_SELECTABLE, which drops them
// out of "possible targets" entirely (AttackersValue::IsPossibleTarget rejects the flag). Scanning
// the raw nearby-npc list instead keeps the encounter visible right through the Heart phases.
static Unit* GetFirstAliveNpcByEntry(PlayerbotAI* botAI, uint32 entry)
{
    auto const& npcs = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get();
    for (auto const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->IsAlive() && unit->GetEntry() == entry)
            return unit;
    }

    return nullptr;
}

Unit* GetXT002(PlayerbotAI* botAI) { return GetFirstAliveNpcByEntry(botAI, NPC_XT002); }

Unit* GetXT002ExposedHeart(PlayerbotAI* botAI)
{
    Unit* heart = GetFirstAliveNpcByEntry(botAI, NPC_HEART_OF_DECONSTRUCTOR);
    if (!heart || heart->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
        return nullptr;

    // The Heart is only worth hitting while it channels Exposed Heart - that aura is what transfers
    // its damage taken to XT.
    return heart->HasAura(SPELL_XT002_EXPOSED_HEART) ? heart : nullptr;
}

bool IsXT002Submerged(PlayerbotAI* botAI)
{
    Unit* xt002 = GetXT002(botAI);
    if (!xt002)
        return false;

    return xt002->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) || xt002->HasAura(SPELL_XT002_SUBMERGE);
}

uint32 GetXT002SearingLightSpellId(Player* bot)
{
    return bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL ? SPELL_XT002_SEARING_LIGHT_25
                                                                   : SPELL_XT002_SEARING_LIGHT_10;
}

uint32 GetXT002HeartbreakSpellId(Player* bot)
{
    return bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL ? SPELL_XT002_HEARTBREAK_25
                                                                    : SPELL_XT002_HEARTBREAK_10;
}

uint32 GetXT002GravityBombSpellId(Player* bot)
{
    return bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL ? SPELL_XT002_GRAVITY_BOMB_25
                                                                   : SPELL_XT002_GRAVITY_BOMB_10;
}

bool IsXT002PummellerTank(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI->IsTank(bot))
        return false;

    // Whoever holds XT keeps holding him, so the add belongs to the first assist tank and only falls
    // to the main tank when there is no second tank left.
    if (Player* assistTank = GetGroupAssistTank(bot, 0))
        return assistTank == bot;

    if (Player* mainTank = GetGroupMainTank(bot))
        return mainTank == bot;

    // Neither resolves only when this bot is the last tank standing, so it owns the add by default.
    return true;
}

bool IsXT002AddEngageable(PlayerbotAI* botAI, Unit* unit)
{
    if (!unit)
        return false;

    Unit* xt002 = GetXT002(botAI);
    if (!xt002)
        return true;

    return unit->GetExactDist2d(xt002) <= ULDUAR_XT002_ADD_LEASH_RADIUS;
}

Unit* GetXT002EngageableAdd(PlayerbotAI* botAI, Player* bot, uint32 entry, float botReach)
{
    Unit* nearest = nullptr;
    float nearestDistance = 0.0f;

    GuidVector const& npcs = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get();
    for (ObjectGuid const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != entry)
            continue;

        if (!IsXT002AddEngageable(botAI, unit))
            continue;

        float const distance = unit->GetExactDist2d(bot);
        if (distance > botReach)
            continue;

        if (!nearest || distance < nearestDistance)
        {
            nearest = unit;
            nearestDistance = distance;
        }
    }

    return nearest;
}

// The formation roster, in the order every bot derives identically. Same rule as Hodir's ring: the
// dead keep their slots, since indexing by the living shifts everyone behind a corpse and re-seats the
// whole formation mid-fight.
static bool BuildXT002RingMembers(Player* bot, std::vector<Player*>& out)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    out.clear();
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member->GetMapId() != bot->GetMapId())
            continue;

        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        if (!memberAI || memberAI->IsTank(member))
            continue;

        if (!memberAI->IsRangedDps(member) && !memberAI->IsHeal(member))
            continue;

        out.push_back(member);
    }

    if (out.empty())
        return false;

    // Healers ahead of ranged dps, then guid. Both keys read the same on every bot, so nobody has to be
    // told which slot is theirs - and healers land on the centre and inner ring, the slots still inside
    // 40 yd of the tank spot.
    std::sort(out.begin(), out.end(), [](Player* left, Player* right)
    {
        bool const leftHeal = GET_PLAYERBOT_AI(left)->IsHeal(left);
        bool const rightHeal = GET_PLAYERBOT_AI(right)->IsHeal(right);
        if (leftHeal != rightHeal)
            return leftHeal;
        return left->GetGUID() < right->GetGUID();
    });

    return true;
}

// Raw formation geometry for one slot, no ground or collision pass.
static Position XT002RingSlotPoint(size_t slot, size_t total)
{
    // Bearing between two fixed points, so the layout never rotates, and it runs towards the tank spot
    // so slot 1 is the one nearest both XT and the tank.
    float const baseAngle =
        std::atan2(ULDUAR_XT002_MAINTANK_SPOT.GetPositionY() - ULDUAR_XT002_RANGED_SPOT.GetPositionY(),
                   ULDUAR_XT002_MAINTANK_SPOT.GetPositionX() - ULDUAR_XT002_RANGED_SPOT.GetPositionX());

    size_t const inner = std::min<size_t>(ULDUAR_XT002_RANGED_RING_INNER_SLOTS, total > 0 ? total - 1 : 0);

    float radiusX = 0.0f;
    float radiusY = 0.0f;
    float angle = baseAngle;

    // Slot 0 stands on the centre, the point with the tank spot and the melee stack both nearest - so
    // it goes to a healer.
    if (slot && slot <= inner)
    {
        radiusX = ULDUAR_XT002_RANGED_RING_INNER_X;
        radiusY = ULDUAR_XT002_RANGED_RING_INNER_Y;
        angle = baseAngle + 2.0f * static_cast<float>(M_PI) * static_cast<float>(slot - 1) / static_cast<float>(inner);
    }
    else if (slot)
    {
        size_t const outerCount = total - inner - 1;
        size_t const outerSlot = slot - inner - 1;
        radiusX = ULDUAR_XT002_RANGED_RING_OUTER_X;
        radiusY = ULDUAR_XT002_RANGED_RING_OUTER_Y;
        angle = baseAngle +
                2.0f * static_cast<float>(M_PI) * static_cast<float>(outerSlot) / static_cast<float>(outerCount);
    }

    angle = Position::NormalizeOrientation(angle);

    return Position(ULDUAR_XT002_RANGED_SPOT.GetPositionX() + std::cos(angle) * radiusX,
                    ULDUAR_XT002_RANGED_SPOT.GetPositionY() + ULDUAR_XT002_RANGED_RING_OFFSET_Y +
                        std::sin(angle) * radiusY,
                    ULDUAR_XT002_RANGED_SPOT.GetPositionZ());
}

bool GetXT002RangedSlot(PlayerbotAI* botAI, Player* bot, Position& out)
{
    std::vector<Player*> members;
    if (!BuildXT002RingMembers(bot, members))
        return false;

    size_t slot = members.size();
    for (size_t i = 0; i < members.size(); ++i)
        if (members[i] == bot)
            slot = i;

    if (slot >= members.size())
        return false;

    size_t const total = members.size();

    // Every ranged bot and healer asks for this every tick, so the grid scan waits until puddles can
    // actually exist - nothing drops one until XT carries Heartbreak.
    std::list<Creature*> voidZones;
    if (IsXT002HeartbreakActive(botAI))
        bot->GetCreatureListWithEntryInGrid(voidZones, PB_NPC_XT002_VOID_ZONE, ULDUAR_XT002_VOID_ZONE_SEARCH_RADIUS);

    // Deal out the slots that are not sitting in Consumption and index into those, rather than letting
    // a displaced bot walk forward to the next slot: every bot sees the same puddles, so this way they
    // all agree on the new layout instead of two of them stepping onto one spot. A bot with no slot
    // left keeps its own - "xt002 avoid hazard action" outranks the anchor and moves it off the puddle.
    std::vector<Position> clear;
    for (size_t i = 0; i < total; ++i)
    {
        Position const candidate = XT002RingSlotPoint(i, total);
        bool blocked = false;
        for (Creature* voidZone : voidZones)
        {
            if (voidZone->GetExactDist2d(candidate.GetPositionX(), candidate.GetPositionY()) <
                ULDUAR_XT002_VOID_ZONE_RADIUS)
            {
                blocked = true;
                break;
            }
        }

        if (!blocked)
            clear.push_back(candidate);
    }

    out = ValidateFloorPoint(bot, slot < clear.size() ? clear[slot] : XT002RingSlotPoint(slot, total));

    // The slot index and the size of the formation it was cut from, so a layout that re-seated is
    // readable without re-deriving the sort from the roster.
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "xt002.slot",
                             std::to_string(slot) + "/" + std::to_string(total) + " " +
                                 RaidObs::DescribeDerived(out));

    return true;
}

// Tested against where the formation says bots belong, not where they are standing: a carrier picks
// its destination while the raid is still walking, so the slots are what it has to miss.
bool XT002PointClearOfFormation(Player* bot, float x, float y, float clearance)
{
    std::vector<Player*> members;
    if (!BuildXT002RingMembers(bot, members))
        return true;

    for (size_t i = 0; i < members.size(); ++i)
    {
        if (members[i] == bot)
            continue;

        if (XT002RingSlotPoint(i, members.size()).GetExactDist2d(x, y) < clearance)
            return false;
    }

    return true;
}
