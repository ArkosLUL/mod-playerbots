/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_Thorim.h"

#include "Creature.h"
#include "DBCEnums.h"
#include "EncounterHelpers.h"
#include "GameObject.h"
#include "Group.h"
#include "LastMovementValue.h"
#include "Map.h"
#include "MotionMaster.h"
#include "PathGenerator.h"
#include "PetDefines.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"
#include "RaidDifficulty.h"
#include "RaidHumanTankCall.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "RaidTankDefensive.h"
#include "RtiTargetValue.h"
#include "Timer.h"
#include "UldScripts.h"
#include "UldTargets.h"
#include "Unit.h"
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <list>
#include <string>
#include <vector>

using namespace EncounterHelpers;

const Position ULDUAR_THORIM_NEAR_ARENA_CENTER = Position(2134.9854f, -263.11853f, 419.8465f);
const Position ULDUAR_THORIM_NEAR_ENTRANCE_POSITION = Position(2172.4355f, -258.27957f, 418.47162f);
const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_1 = Position(2237.6187f, -265.08844f, 412.17548f);
const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_2 = Position(2237.2498f, -275.81122f, 412.17548f);
const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_5_YARDS_1 = Position(2236.895f, -294.62448f, 412.1348f);
const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_1 = Position(2242.1162f, -310.15308f, 412.1348f);
const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_2 = Position(2242.018f, -318.66003f, 412.1348f);
const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_3 = Position(2242.1904f, -329.0533f, 412.1348f);
const Position ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_2_YARDS_1 = Position(2237.20f, -360.50f, 412.1348f);
const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_1 = Position(2219.5417f, -264.77167f, 412.17548f);
const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_2 = Position(2217.446f, -275.85248f, 412.17548f);
const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_5_YARDS_1 = Position(2217.8877f, -295.01193f, 412.13434f);
const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_1 = Position(2212.193f, -307.44992f, 412.1348f);
const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_2 = Position(2212.1353f, -318.20795f, 412.1348f);
const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_3 = Position(2212.1956f, -328.0144f, 412.1348f);
const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_2_YARDS_1 = Position(2219.50f, -360.57f, 412.1348f);
const Position ULDUAR_THORIM_BALCONY_1 = Position(2141.0f, -408.0f, 438.247f);
const Position ULDUAR_THORIM_BALCONY_2 = Position(2151.0f, -396.0f, 438.247f);
const Position ULDUAR_THORIM_BALCONY_3 = Position(2152.0f, -365.0f, 438.744f);
const Position ULDUAR_THORIM_BALCONY_4 = Position(2151.0f, -332.0f, 438.247f);
const Position ULDUAR_THORIM_BALCONY_5 = Position(2137.5f, -318.0f, 438.222f);
const Position ULDUAR_THORIM_JUMP_START_POINT = Position(2137.137f, -291.19025f, 438.24753f, 1.7059844f);
const Position ULDUAR_THORIM_JUMP_END_POINT = Position(2137.8818f, -278.18942f, 419.66653f);
const Position ULDUAR_THORIM_BALCONY_HOLD1_SPOT = Position(2126.0f, -294.0f, 438.247f);
const Position ULDUAR_THORIM_BALCONY_HOLD2_SPOT = Position(2138.0f, -296.0f, 438.247f);
const Position ULDUAR_THORIM_BALCONY_HOLD3_SPOT = Position(2150.0f, -297.0f, 438.247f);
const Position ULDUAR_THORIM_BALCONY_HOLD4_SPOT = Position(2132.0f, -307.0f, 438.243f);
const Position ULDUAR_THORIM_BALCONY_HOLD5_SPOT = Position(2144.0f, -307.0f, 438.243f);
const Position ULDUAR_THORIM_BALCONY_HOLD6_SPOT = Position(2138.0f, -318.0f, 438.222f);
const Position ULDUAR_THORIM_PHASE2_TANK_SPOT = Position(2110.7483f, -252.65265f, 419.440f);
const Position ULDUAR_THORIM_PHASE2_RANGE1_SPOT = Position(2123.00f, -282.00f, 419.528f);
const Position ULDUAR_THORIM_PHASE2_RANGE2_SPOT = Position(2124.50f, -270.50f, 419.729f);
const Position ULDUAR_THORIM_PHASE2_RANGE3_SPOT = Position(2137.50f, -269.00f, 419.845f);
const Position ULDUAR_THORIM_PHASE2_RANGE4_SPOT = Position(2132.50f, -257.50f, 419.845f);
const Position ULDUAR_THORIM_PHASE2_RANGE5_SPOT = Position(2142.00f, -250.50f, 419.691f);
const Position ULDUAR_THORIM_PHASE2_RANGE6_SPOT = Position(2131.50f, -245.00f, 419.612f);
const Position ULDUAR_THORIM_PHASE2_OPENING1_SPOT = Position(2114.0f, -232.0f, 420.146f);
const Position ULDUAR_THORIM_PHASE2_OPENING2_SPOT = Position(2128.0f, -226.0f, 420.146f);
const Position ULDUAR_THORIM_PHASE2_OPENING3_SPOT = Position(2140.0f, -232.0f, 419.337f);
const Position ULDUAR_THORIM_PHASE2_OPENING4_SPOT = Position(2146.0f, -244.0f, 419.531f);
const Position ULDUAR_THORIM_PHASE2_MELEE1_SPOT = Position(2118.75f, -252.65f, 419.596f);
const Position ULDUAR_THORIM_PHASE2_MELEE2_SPOT = Position(2110.75f, -244.65f, 419.359f);
const Position ULDUAR_THORIM_PHASE2_MELEE3_SPOT = Position(2110.75f, -260.65f, 419.485f);
const Position ULDUAR_THORIM_PHASE2_OFFTANK_SPOT = Position(2107.25f, -252.65f, 419.371f);
const Position ULDUAR_THORIM_RUNE_GIANT_HOLD_SPOT = Position(2187.79f, -432.78f, 426.39f);

namespace
{

RaidInstanceState<ThorimEncounterState> thorimStates;

ThorimEncounterState& ThorimStateFor(Player* bot) { return thorimStates.For(bot->GetInstanceId()); }

Position const THORIM_RUNIC_COLOSSUS_SPAWN(2227.5f, -396.179f, 412.176f);

struct GauntletWaypoint
{
    Position const* position;
    float radius;
};

std::array<GauntletWaypoint, ULDUAR_THORIM_GAUNTLET_WAYPOINTS> const leftLaneWaypoints = {{
    {&ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_1, 6.0f},
    {&ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_2, 6.0f},
    {&ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_5_YARDS_1, 5.0f},
    {&ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_1, 10.0f},
    {&ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_2, 10.0f},
    {&ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_3, 10.0f},
    {&ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_2_YARDS_1, 2.0f},
}};

std::array<GauntletWaypoint, ULDUAR_THORIM_GAUNTLET_WAYPOINTS> const rightLaneWaypoints = {{
    {&ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_1, 6.0f},
    {&ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_2, 6.0f},
    {&ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_5_YARDS_1, 5.0f},
    {&ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_1, 10.0f},
    {&ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_2, 10.0f},
    {&ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_3, 10.0f},
    {&ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_2_YARDS_1, 2.0f},
}};

std::array<GauntletWaypoint, ULDUAR_THORIM_GAUNTLET_WAYPOINTS> const& LaneWaypoints(bool leftLane)
{
    return leftLane ? leftLaneWaypoints : rightLaneWaypoints;
}

std::array<Position const*, ULDUAR_THORIM_BALCONY_WAYPOINTS> const balconyWaypoints = {{
    &ULDUAR_THORIM_BALCONY_1,
    &ULDUAR_THORIM_BALCONY_2,
    &ULDUAR_THORIM_BALCONY_3,
    &ULDUAR_THORIM_BALCONY_4,
    &ULDUAR_THORIM_BALCONY_5,
    &ULDUAR_THORIM_JUMP_START_POINT,
}};

bool ThorimBalconyWaypointReached(Player const* bot, uint8 index)
{
    if (bot->GetPositionZ() <= ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
        return false;

    Position const& here = GetThorimBalconyWaypoint(index);
    return bot->GetExactDist2d(&here) <= ULDUAR_THORIM_BALCONY_ARRIVE_TOLERANCE ||
           bot->GetPositionY() > here.GetPositionY();
}

ThorimEncounterState* FindState(Player const* bot)
{
    return bot ? thorimStates.Find(bot->GetInstanceId()) : nullptr;
}

bool OnThorimBalcony(WorldObject const* who)
{
    if (!who)
        return false;

    float const x = who->GetPositionX();
    float const y = who->GetPositionY();
    float const z = who->GetPositionZ();

    return z >= ULDUAR_THORIM_WING_MAX_Z && z <= ULDUAR_THORIM_BALCONY_BOX_MAX_Z &&
           x >= ULDUAR_THORIM_BALCONY_BOX_MIN_X && x <= ULDUAR_THORIM_BALCONY_BOX_MAX_X &&
           y >= ULDUAR_THORIM_BALCONY_BOX_MIN_Y && y <= ULDUAR_THORIM_BALCONY_BOX_MAX_Y;
}

bool PastThorimPortcullis(Player const* who)
{
    if (!NearThorimEncounter(who))
        return false;

    if (OnThorimBalcony(who))
        return true;

    float const x = who->GetPositionX();
    float const y = who->GetPositionY();
    return y < ULDUAR_THORIM_ARENA_BOX_MIN_Y ||
           (x > ULDUAR_THORIM_ARENA_BOX_MAX_X && y < ULDUAR_THORIM_ARENA_BOX_MAX_Y);
}

bool HumanInGauntlet(Player const* member)
{
    return member->GetDistance(ULDUAR_THORIM_NEAR_ENTRANCE_POSITION) < ULDUAR_THORIM_GATE_RADIUS ||
           PastThorimPortcullis(member);
}

bool MemberCounts(Player const* member, uint32 instanceId)
{
    return member && member->IsAlive() && member->GetMapId() == ULDUAR_MAP_ID &&
           member->GetInstanceId() == instanceId;
}

bool HoldsFormationSlot(Player const* member, uint32 instanceId)
{
    return member && member->GetMapId() == ULDUAR_MAP_ID && member->GetInstanceId() == instanceId;
}

bool TakesRangedSpot(Player* member)
{
    return member && !PlayerbotAI::IsTank(member) &&
           (PlayerbotAI::IsRanged(member) || PlayerbotAI::IsHeal(member));
}

bool TakesMeleeSlot(Player* member)
{
    return member && !TakesRangedSpot(member) && !PlayerbotAI::IsTank(member);
}

void EnsureMeleeSlot(Player* bot)
{
    ThorimEncounterState& state = ThorimStateFor(bot);

    Group* group = bot->GetGroup();
    if (!group)
    {
        state.meleeSlots[bot->GetGUID()] = 0;
        return;
    }

    uint32 const instanceId = bot->GetInstanceId();

    std::unordered_set<ObjectGuid> present;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();

        if (HoldsFormationSlot(member, instanceId) && TakesMeleeSlot(member))
            present.insert(member->GetGUID());
    }

    for (auto itr = state.meleeSlots.begin(); itr != state.meleeSlots.end();)
        itr = present.count(itr->first) ? std::next(itr) : state.meleeSlots.erase(itr);

    if (state.meleeSlots.count(bot->GetGUID()))
        return;

    std::array<uint8, ULDUAR_THORIM_MELEE_SLOTS> load = {};
    for (auto const& assignment : state.meleeSlots)
        if (assignment.second < ULDUAR_THORIM_MELEE_SLOTS)
            ++load[assignment.second];

    uint8 chosen = 0;
    for (uint8 slot = 1; slot < ULDUAR_THORIM_MELEE_SLOTS; ++slot)
        if (load[slot] < load[chosen])
            chosen = slot;

    state.meleeSlots[bot->GetGUID()] = chosen;
}

void EnsureRangedSlot(Player* bot)
{
    ThorimEncounterState& state = ThorimStateFor(bot);

    Group* group = bot->GetGroup();
    if (!group)
    {
        state.rangedSlots[bot->GetGUID()] = 0;
        return;
    }

    uint32 const instanceId = bot->GetInstanceId();

    std::unordered_set<ObjectGuid> present;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (HoldsFormationSlot(member, instanceId) && TakesRangedSpot(member))
            present.insert(member->GetGUID());
    }

    for (auto itr = state.rangedSlots.begin(); itr != state.rangedSlots.end();)
        itr = present.count(itr->first) ? std::next(itr) : state.rangedSlots.erase(itr);

    if (state.rangedSlots.count(bot->GetGUID()))
        return;

    std::array<uint8, ULDUAR_THORIM_RANGED_SLOTS> load = {};
    for (auto const& assignment : state.rangedSlots)
        if (assignment.second < ULDUAR_THORIM_RANGED_SLOTS)
            ++load[assignment.second];

    uint8 chosen = 0;
    for (uint8 slot = 1; slot < ULDUAR_THORIM_RANGED_SLOTS; ++slot)
        if (load[slot] < load[chosen])
            chosen = slot;

    state.rangedSlots[bot->GetGUID()] = chosen;
}

bool RangedSlotOf(Player* bot, uint8& slot)
{
    ThorimEncounterState const* state = FindState(bot);
    if (!state)
        return false;

    auto const itr = state->rangedSlots.find(bot->GetGUID());
    if (itr == state->rangedSlots.end())
        return false;

    slot = itr->second;
    return true;
}

std::array<Position const*, ULDUAR_THORIM_OPENING_SLOTS> const openingSpots = {{
    &ULDUAR_THORIM_PHASE2_OPENING1_SPOT,
    &ULDUAR_THORIM_PHASE2_OPENING2_SPOT,
    &ULDUAR_THORIM_PHASE2_OPENING3_SPOT,
    &ULDUAR_THORIM_PHASE2_OPENING4_SPOT,
}};

std::array<Position const*, ULDUAR_THORIM_BALCONY_HOLD_SLOTS> const balconyHoldSpots = {{
    &ULDUAR_THORIM_BALCONY_HOLD1_SPOT,
    &ULDUAR_THORIM_BALCONY_HOLD2_SPOT,
    &ULDUAR_THORIM_BALCONY_HOLD3_SPOT,
    &ULDUAR_THORIM_BALCONY_HOLD4_SPOT,
    &ULDUAR_THORIM_BALCONY_HOLD5_SPOT,
    &ULDUAR_THORIM_BALCONY_HOLD6_SPOT,
}};

ThorimSquad SquadOf(ThorimEncounterState const& state, Player const* member)
{
    auto const itr = state.squads.find(member->GetGUID());
    return itr == state.squads.end() ? ThorimSquad::Arena : static_cast<ThorimSquad>(itr->second);
}

template <size_t N, typename TakesSlot>
void EnsureNearestSlot(Player* bot, RaidObs::ObsGuidMap<uint8>& slots, std::array<Position const*, N> const& spots,
                       TakesSlot const& takesSlot)
{
    Group* group = bot->GetGroup();
    if (!group)
    {
        slots[bot->GetGUID()] = 0;
        return;
    }

    uint32 const instanceId = bot->GetInstanceId();

    std::unordered_set<ObjectGuid> present;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (HoldsFormationSlot(member, instanceId) && takesSlot(member))
            present.insert(member->GetGUID());
    }

    for (auto itr = slots.begin(); itr != slots.end();)
        itr = present.count(itr->first) ? std::next(itr) : slots.erase(itr);

    if (slots.count(bot->GetGUID()))
        return;

    std::array<uint8, N> load = {};
    for (auto const& assignment : slots)
        if (assignment.second < N)
            ++load[assignment.second];

    uint8 chosen = 0;
    for (uint8 slot = 1; slot < N; ++slot)
    {
        bool const nearer = bot->GetExactDist2d(spots[slot]) < bot->GetExactDist2d(spots[chosen]);
        if (load[slot] < load[chosen] || (load[slot] == load[chosen] && nearer))
            chosen = slot;
    }

    slots[bot->GetGUID()] = chosen;
}

void EnsureOpeningSlot(Player* bot)
{
    ThorimEncounterState& state = ThorimStateFor(bot);
    EnsureNearestSlot(bot, state.openingSlots, openingSpots,
                      [&state](Player* member)
                      { return TakesRangedSpot(member) && SquadOf(state, member) != ThorimSquad::Gauntlet; });
}

void EnsureBalconyHoldSlot(Player* bot)
{
    ThorimEncounterState& state = ThorimStateFor(bot);
    EnsureNearestSlot(bot, state.balconyHoldSlots, balconyHoldSpots,
                      [&state](Player* member)
                      { return TakesRangedSpot(member) && SquadOf(state, member) == ThorimSquad::Gauntlet; });
}

bool SlotOf(RaidObs::ObsGuidMap<uint8> const& slots, Player* bot, uint8& slot)
{
    auto const itr = slots.find(bot->GetGUID());
    if (itr == slots.end())
        return false;

    slot = itr->second;
    return true;
}

bool IsBotPlayer(Player const* member)
{
    WorldSession const* session = member ? member->GetSession() : nullptr;
    return session && session->IsHeadless();
}

void AssignThorimSquads(Player* bot)
{
    ThorimEncounterState& state = ThorimStateFor(bot);
    if (state.squadsAssigned)
        return;

    Group* group = bot->GetGroup();
    if (!group)
    {
        state.squads[bot->GetGUID()] = static_cast<uint8>(ThorimSquad::Arena);
        state.squadsAssigned = true;
        return;
    }

    uint32 const instanceId = bot->GetInstanceId();

    std::vector<Player*> roster;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (MemberCounts(member, instanceId))
            roster.push_back(member);
    }

    if (roster.empty())
        return;

    if (PlayerbotAI::GetMainTankGuid(group).IsEmpty())
        return;

    bool const twentyFive = IsRaid25Man(bot);
    uint32 healerQuota = twentyFive ? 2 : 1;

    uint32 dpsQuota = twentyFive ? 9 : 4;

    size_t const floorSize = static_cast<size_t>(ULDUAR_THORIM_ARENA_MIN_MEMBERS);
    size_t const cap = roster.size() > floorSize ? roster.size() - floorSize : 0;

    std::unordered_set<ObjectGuid> gauntlet;
    auto take = [&gauntlet, cap](Player* member)
    {
        if (gauntlet.size() >= cap)
            return false;

        gauntlet.insert(member->GetGUID());
        return true;
    };

    Player* gauntletTank = nullptr;
    for (Player* member : roster)
        if (IsBotPlayer(member) && PlayerbotAI::IsAssistTankOfIndex(member, 0))
        {
            gauntletTank = member;
            break;
        }

    if (!gauntletTank)
        for (Player* member : roster)
            if (IsBotPlayer(member) && PlayerbotAI::IsTank(member) && !PlayerbotAI::IsMainTank(member))
            {
                gauntletTank = member;
                break;
            }

    size_t tankCount = 0;
    for (Player* member : roster)
        if (PlayerbotAI::IsTank(member))
            ++tankCount;

    if (gauntletTank && tankCount > 1 && !PlayerbotAI::IsMainTank(gauntletTank))
        take(gauntletTank);
    else
        ++dpsQuota;

    for (Player* member : roster)
    {
        if (!healerQuota)
            break;

        if (!IsBotPlayer(member) || gauntlet.count(member->GetGUID()) || PlayerbotAI::IsMainTank(member))
            continue;

        if (!PlayerbotAI::IsHeal(member) || !take(member))
            continue;

        --healerQuota;
    }

    for (Player* member : roster)
    {
        if (!dpsQuota)
            break;

        if (!IsBotPlayer(member) || gauntlet.count(member->GetGUID()) || PlayerbotAI::IsMainTank(member))
            continue;

        if (!PlayerbotAI::IsDps(member) || !take(member))
            continue;

        --dpsQuota;
    }

    for (Player* member : roster)
        if (IsBotPlayer(member))
            state.squads[member->GetGUID()] =
                static_cast<uint8>(gauntlet.count(member->GetGUID()) ? ThorimSquad::Gauntlet : ThorimSquad::Arena);

    state.squadsAssigned = true;
}

void TickHumanSquads(Player* bot)
{
    ThorimEncounterState& state = ThorimStateFor(bot);
    if (state.humanSquadScanMs &&
        GetMSTimeDiffToNow(state.humanSquadScanMs) < ULDUAR_THORIM_ENCOUNTER_SCAN_INTERVAL_MS)
        return;

    state.humanSquadScanMs = getMSTime();

    Group* group = bot->GetGroup();
    if (!group)
        return;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || IsBotPlayer(member) || member->GetMapId() != ULDUAR_MAP_ID)
            continue;

        state.squads[member->GetGUID()] =
            static_cast<uint8>(HumanInGauntlet(member) ? ThorimSquad::Gauntlet : ThorimSquad::Arena);
    }
}

bool MeleeSlotOf(Player* bot, uint8& slot)
{
    ThorimEncounterState const* state = FindState(bot);
    if (!state)
        return false;

    auto const itr = state->meleeSlots.find(bot->GetGUID());
    if (itr == state->meleeSlots.end())
        return false;

    slot = itr->second;
    return true;
}

float RingAnchorBearing(Unit* boss)
{
    float const x =
        (ULDUAR_THORIM_PHASE2_TANK_SPOT.GetPositionX() + ULDUAR_THORIM_PHASE2_OFFTANK_SPOT.GetPositionX()) / 2.0f;
    float const y =
        (ULDUAR_THORIM_PHASE2_TANK_SPOT.GetPositionY() + ULDUAR_THORIM_PHASE2_OFFTANK_SPOT.GetPositionY()) / 2.0f;
    return std::atan2(y - boss->GetPositionY(), x - boss->GetPositionX());
}

float SlotBearing(float anchor, uint8 slot)
{
    return Position::NormalizeOrientation(anchor + (static_cast<float>(M_PI) / 2.0f) * static_cast<float>(slot + 1));
}

float AbsAngleDelta(float first, float second)
{
    float delta = std::fabs(Position::NormalizeOrientation(first) - Position::NormalizeOrientation(second));
    if (delta > static_cast<float>(M_PI))
        delta = 2.0f * static_cast<float>(M_PI) - delta;

    return delta;
}

float BearingFromBoss(Unit* boss, float x, float y)
{
    return std::atan2(y - boss->GetPositionY(), x - boss->GetPositionX());
}

bool InLightningChargeCone(float bearing, float coneBearing)
{
    float const halfWidth = ULDUAR_THORIM_LIGHTNING_CHARGE_CONE_ANGLE / 2.0f + ULDUAR_THORIM_LIGHTNING_CHARGE_MARGIN;
    return AbsAngleDelta(bearing, coneBearing) <= halfWidth;
}

std::vector<Position> const& ThorimBlizzardSpots(Player* bot)
{
    ThorimEncounterState& state = ThorimStateFor(bot);
    if (state.blizzardScanMs && GetMSTimeDiffToNow(state.blizzardScanMs) < ULDUAR_THORIM_ENCOUNTER_SCAN_INTERVAL_MS)
        return state.blizzardSpots;

    state.blizzardScanMs = getMSTime();
    state.blizzardBunnies.clear();

    std::list<Creature*> bunnies;
    bot->GetCreatureListWithEntryInGrid(bunnies, NPC_SIF_BLIZZARD, ULDUAR_THORIM_BLIZZARD_SCAN_RANGE);
    for (Creature* bunny : bunnies)
        if (bunny && bunny->IsAlive())
            state.blizzardBunnies.emplace_back(bunny->GetPositionX(), bunny->GetPositionY(), bunny->GetPositionZ());

    state.blizzardSpots = state.blizzardBunnies;

    for (uint32 const zoneSpell : {SPELL_SIF_BLIZZARD_ZONE_10, SPELL_SIF_BLIZZARD_ZONE_25})
        for (Position const& zone : GetDynamicObjectPositions(bot, ULDUAR_THORIM_BLIZZARD_SCAN_RANGE, zoneSpell))
            state.blizzardSpots.push_back(zone);

    return state.blizzardSpots;
}

float RingSlotPointsApart(ThorimEncounterState const& state, uint8 first, uint8 second)
{
    float const delta = AbsAngleDelta(state.ringSlots[first].composed, state.ringSlots[second].composed);
    return 2.0f * ULDUAR_THORIM_MELEE_RING_RADIUS * std::sin(delta / 2.0f);
}

bool RingBodyBridgesChain(Player* bot, Position const& spot)
{
    Group* group = bot->GetGroup();
    ThorimEncounterState const* state = FindState(bot);
    uint8 ownSlot = 0;
    if (!group || !state || !state->ringLatched || !MeleeSlotOf(bot, ownSlot) ||
        bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) <= ULDUAR_THORIM_RING_CHAIN_MIN_STEP)
        return false;

    float const jump = ULDUAR_THORIM_CHAIN_LIGHTNING_JUMP + ULDUAR_THORIM_RING_CHAIN_MARGIN;
    bool bridged = false;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member == bot || !MemberCounts(member, bot->GetInstanceId()) || !member->IsInWorld() ||
            member->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
            continue;

        uint8 slot = 0;
        bool const ringBody = MeleeSlotOf(member, slot);
        if (ringBody && slot == ownSlot)
            continue;

        float const apart = ringBody ? RingSlotPointsApart(*state, ownSlot, slot) : member->GetExactDist(spot);
        if (apart <= jump + bot->GetCombatReach() + member->GetCombatReach())
            return false;

        if (bot->IsWithinDist(member, jump))
            bridged = true;
    }

    return bridged;
}

void RingStepChainBodies(Player* bot, Unit* boss, std::vector<HazardCircle>& slots, std::vector<HazardCircle>& camp)
{
    Group* group = bot->GetGroup();
    ThorimEncounterState const* state = FindState(bot);
    uint8 ownSlot = 0;
    if (!group || !state || !state->ringLatched || !MeleeSlotOf(bot, ownSlot))
        return;

    float const jump = ULDUAR_THORIM_CHAIN_LIGHTNING_JUMP + ULDUAR_THORIM_RING_CHAIN_MARGIN + bot->GetCombatReach();
    std::array<bool, ULDUAR_THORIM_MELEE_SLOTS> pointAdded{};
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member == bot || !MemberCounts(member, bot->GetInstanceId()) || !member->IsInWorld() ||
            member->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
            continue;

        float const clearance = jump + member->GetCombatReach();
        uint8 slot = 0;
        if (!MeleeSlotOf(member, slot))
        {
            if (!PlayerbotAI::IsTank(member))
                camp.emplace_back(member->GetPosition(), clearance);

            continue;
        }

        if (slot == ownSlot)
            continue;

        slots.emplace_back(member->GetPosition(), clearance);
        if (pointAdded[slot])
            continue;

        pointAdded[slot] = true;
        float const bearing = state->ringSlots[slot].composed;
        slots.emplace_back(Position(boss->GetPositionX() + std::cos(bearing) * ULDUAR_THORIM_MELEE_RING_RADIUS,
                                    boss->GetPositionY() + std::sin(bearing) * ULDUAR_THORIM_MELEE_RING_RADIUS,
                                    boss->GetPositionZ()),
                           clearance);
    }
}

bool RingSpotBeatsTheDeadband(PlayerbotAI* botAI, Player* bot, Position const& spot)
{
    bool spotInCone = false;
    if (Unit* boss = GetThorim(botAI))
    {
        if (Unit* orb = ThorimChargedThunderOrb(botAI, SPELL_THORIM_LIGHTNING_ORB_VISUAL))
        {
            float const coneBearing = BearingFromBoss(boss, orb->GetPositionX(), orb->GetPositionY());
            spotInCone = InLightningChargeCone(BearingFromBoss(boss, spot.GetPositionX(), spot.GetPositionY()), coneBearing);
            if (!spotInCone &&
                InLightningChargeCone(BearingFromBoss(boss, bot->GetPositionX(), bot->GetPositionY()), coneBearing))
                return true;
        }
    }

    bool standingInOne = false;
    for (Position const& zone : ThorimBlizzardSpots(bot))
    {
        if (zone.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) < ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE)
            return false;

        if (zone.GetExactDist2d(bot->GetPositionX(), bot->GetPositionY()) < ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE)
            standingInOne = true;
    }

    return standingInOne || (!spotInCone && RingBodyBridgesChain(bot, spot));
}

bool RingPoint(Player* bot, Unit* boss, float bearing, Position& out)
{
    float x = boss->GetPositionX() + std::cos(bearing) * ULDUAR_THORIM_MELEE_RING_RADIUS;
    float y = boss->GetPositionY() + std::sin(bearing) * ULDUAR_THORIM_MELEE_RING_RADIUS;
    float z = bot->GetMapWaterOrGroundLevel(x, y, boss->GetPositionZ());
    if (z <= INVALID_HEIGHT)
        z = boss->GetPositionZ();

    bot->GetMap()->CheckCollisionAndGetValidCoords(bot, bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(),
                                                   x, y, z, false);

    Position const candidate(x, y, z);
    if (boss->GetDistance(candidate) > boss->GetMeleeRange(bot))
        return false;

    out = candidate;
    return true;
}

Position const& RangedSpot(uint8 slot)
{
    static Position const* const spots[ULDUAR_THORIM_RANGED_SLOTS] = {
        &ULDUAR_THORIM_PHASE2_RANGE1_SPOT, &ULDUAR_THORIM_PHASE2_RANGE2_SPOT, &ULDUAR_THORIM_PHASE2_RANGE3_SPOT,
        &ULDUAR_THORIM_PHASE2_RANGE4_SPOT, &ULDUAR_THORIM_PHASE2_RANGE5_SPOT, &ULDUAR_THORIM_PHASE2_RANGE6_SPOT};

    return *spots[std::min<uint8>(slot, ULDUAR_THORIM_RANGED_SLOTS - 1)];
}

std::array<Position, 7> const ULDUAR_THORIM_THUNDER_ORB_SPOTS = {
    Position(2145.50f, -222.62f, 433.30f), Position(2164.20f, -233.47f, 433.30f),
    Position(2164.55f, -293.00f, 433.30f), Position(2105.04f, -292.56f, 433.30f),
    Position(2092.95f, -263.00f, 433.30f), Position(2104.94f, -233.44f, 433.30f),
    Position(2124.30f, -222.60f, 433.30f)};

struct ThorimShelterSpot
{
    uint8 orbIndex;
    uint8 slot;
    Position spot;
};

std::array<ThorimShelterSpot, 13> const ULDUAR_THORIM_SHELTER_SPOTS = {{
    {0, 4, Position(2142.00f, -255.35f, 419.771f)},
    {0, 5, Position(2113.00f, -227.35f, 420.293f)},
    {1, 3, Position(2132.00f, -276.35f, 419.755f)},
    {1, 4, Position(2130.50f, -262.85f, 419.905f)},
    {1, 5, Position(2119.50f, -226.85f, 420.146f)},
    {2, 0, Position(2116.50f, -283.35f, 419.509f)},
    {2, 1, Position(2107.50f, -283.35f, 420.104f)},
    {2, 2, Position(2114.50f, -274.35f, 419.562f)},
    {2, 3, Position(2140.00f, -241.35f, 419.502f)},
    {2, 4, Position(2130.00f, -224.00f, 420.146f)},
    {3, 0, Position(2129.50f, -278.35f, 419.702f)},
    {3, 1, Position(2126.00f, -269.85f, 419.761f)},
    {6, 5, Position(2133.50f, -246.85f, 419.652f)},
}};

bool ThorimThunderOrbIndex(Unit* orb, uint8& index)
{
    for (size_t i = 0; i < ULDUAR_THORIM_THUNDER_ORB_SPOTS.size(); ++i)
    {
        if (ULDUAR_THORIM_THUNDER_ORB_SPOTS[i].GetExactDist2d(orb->GetPositionX(), orb->GetPositionY()) <=
            ULDUAR_THORIM_THUNDER_ORB_MATCH_RADIUS)
        {
            index = static_cast<uint8>(i);
            return true;
        }
    }

    return false;
}

bool ThorimShelterFor(uint8 orbIndex, uint8 slot, Position& out)
{
    for (ThorimShelterSpot const& row : ULDUAR_THORIM_SHELTER_SPOTS)
    {
        if (row.orbIndex == orbIndex && row.slot == slot)
        {
            out = row.spot;
            return true;
        }
    }

    return false;
}

bool InLightningChargeConeRanged(float bearing, float coneBearing)
{
    float const halfWidth =
        ULDUAR_THORIM_LIGHTNING_CHARGE_CONE_ANGLE / 2.0f + ULDUAR_THORIM_LIGHTNING_CHARGE_RANGED_MARGIN;
    return AbsAngleDelta(bearing, coneBearing) <= halfWidth;
}

static float ThorimCampConeReach(Player* bot)
{
    ThorimEncounterState const* state = FindState(bot);
    if (!state || !state->lightningOrbLitMs)
        return 0.0f;

    uint32 const warningMs = ULDUAR_THORIM_LIGHTNING_CHARGE_WARNING_MS - ULDUAR_THORIM_ENCOUNTER_SCAN_INTERVAL_MS;
    uint32 const elapsedMs = GetMSTimeDiffToNow(state->lightningOrbLitMs);
    if (elapsedMs >= warningMs)
        return 0.0f;

    return bot->GetSpeed(MOVE_RUN) * static_cast<float>(warningMs - elapsedMs) / 1000.0f;
}

static bool ThorimCampShelterInTime(Player* bot, Position const& shelter)
{
    float reach = ThorimCampConeReach(bot);
    if (!bot->HasAura(SPELL_SIF_BLIZZARD_ZONE_10) && !bot->HasAura(SPELL_SIF_BLIZZARD_ZONE_25) &&
        ThorimWalkUnderBlizzard(bot, *bot, shelter))
        reach *= ULDUAR_THORIM_BLIZZARD_SPEED_FACTOR;

    return bot->GetExactDist2d(&shelter) <= reach;
}

static bool ThorimCampConeStep(Player* bot, Unit* boss, Unit* orb, Position const& shelter, Position& out)
{
    float const reach = std::min(ULDUAR_THORIM_CAMP_CONE_STEP_RADIUS, ThorimCampConeReach(bot));
    if (reach < ULDUAR_THORIM_SWEEP_STEP)
        return false;

    std::vector<HazardCircle> circles;
    circles.emplace_back(boss->GetPosition(), ULDUAR_THORIM_CAMP_CONE_STEP_MIN_BOSS_RANGE);
    for (Position const& zone : ThorimBlizzardSpots(bot))
        circles.emplace_back(zone, ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE);

    float const coneBearing = BearingFromBoss(boss, orb->GetPositionX(), orb->GetPositionY());
    float const halfWidth = ULDUAR_THORIM_LIGHTNING_CHARGE_CONE_ANGLE / 2.0f + ULDUAR_THORIM_CONE_STEP_MARGIN;
    bool const inZone = ThorimSpotUnderBlizzard(bot, *bot);
    auto const accept = [bot, boss, coneBearing, halfWidth, inZone](float x, float y)
    {
        if (x < ULDUAR_THORIM_ARENA_BOX_MIN_X || x > ULDUAR_THORIM_ARENA_BOX_MAX_X ||
            y < ULDUAR_THORIM_ARENA_BOX_MIN_Y || y > ULDUAR_THORIM_ARENA_BOX_MAX_Y ||
            boss->GetExactDist2d(x, y) > ULDUAR_THORIM_CAMP_MAX_BOSS_RANGE ||
            AbsAngleDelta(BearingFromBoss(boss, x, y), coneBearing) <= halfWidth)
            return false;

        return inZone || !ThorimWalkUnderBlizzard(bot, *bot, Position(x, y, bot->GetPositionZ()));
    };

    Position const spot = FindNearestPositionClearOfHazards(bot, circles, reach, ULDUAR_THORIM_SWEEP_STEP,
                                                            ULDUAR_THORIM_SWEEP_ANGLE_STEP, &shelter, accept);
    if (spot == Position())
        return false;

    out = spot;
    return true;
}

bool ThorimRangedSpot(PlayerbotAI* botAI, Player* bot, Unit* boss, uint8 slot, Position& out, uint8& orbIndex,
                      bool& stepped)
{
    out = RangedSpot(slot);
    stepped = false;

    Unit* orb = ThorimChargedThunderOrb(botAI, SPELL_THORIM_LIGHTNING_ORB_VISUAL);

    ThorimEncounterState::RangedShelter& held = ThorimStateFor(bot).rangedShelters[bot->GetGUID()];

    if (!orb)
    {
        held = {};
        return false;
    }

    if (held.orb != orb->GetGUID())
    {
        held.orb = orb->GetGUID();
        held.sheltered = false;
        held.stepSought = false;
        held.stepped = false;

        if (!ThorimThunderOrbIndex(orb, held.orbIndex))
            held.orb.Clear();
    }

    Position shelter;

    if (!held.sheltered && held.orb == orb->GetGUID())
    {
        float const bearing = BearingFromBoss(boss, out.GetPositionX(), out.GetPositionY());
        float const coneBearing = BearingFromBoss(boss, orb->GetPositionX(), orb->GetPositionY());
        if (InLightningChargeConeRanged(bearing, coneBearing) && ThorimShelterFor(held.orbIndex, slot, shelter))
            held.sheltered = true;
    }

    if (!held.sheltered)
        return false;

    if (!ThorimShelterFor(held.orbIndex, slot, shelter))
    {
        held.sheltered = false;
        return false;
    }

    if (!held.stepSought && !bot->HasUnitState(UNIT_STATE_ROOT) &&
        bot->GetExactDist2d(&shelter) > ULDUAR_THORIM_PHASE2_SPOT_TOLERANCE && ThorimCampSpotInLitCone(botAI, *bot) &&
        !ThorimCampShelterInTime(bot, shelter))
    {
        held.stepSought = true;
        held.stepped = ThorimCampConeStep(bot, boss, orb, shelter, held.step);
    }

    out = held.stepped ? held.step : shelter;
    orbIndex = held.orbIndex;
    stepped = held.stepped;
    return true;
}

bool RingBearingSeparated(float candidate, uint8 slot,
                          std::array<float, ULDUAR_THORIM_MELEE_SLOTS> const& taken, float anchor)
{
    if (AbsAngleDelta(candidate, anchor) < ULDUAR_THORIM_RING_SLOT_SEPARATION)
        return false;

    for (uint8 other = 0; other < ULDUAR_THORIM_MELEE_SLOTS; ++other)
        if (other != slot && AbsAngleDelta(candidate, taken[other]) < ULDUAR_THORIM_RING_SLOT_SEPARATION)
            return false;

    return true;
}

float RingPointTankClearance(Unit* boss, float bearing)
{
    float const x = boss->GetPositionX() + std::cos(bearing) * ULDUAR_THORIM_MELEE_RING_RADIUS;
    float const y = boss->GetPositionY() + std::sin(bearing) * ULDUAR_THORIM_MELEE_RING_RADIUS;
    return std::min(ULDUAR_THORIM_PHASE2_TANK_SPOT.GetExactDist2d(x, y),
                    ULDUAR_THORIM_PHASE2_OFFTANK_SPOT.GetExactDist2d(x, y));
}

void RelatchRingAnchor(ThorimEncounterState& state, Unit* boss)
{
    Unit* const victim = boss->GetVictim();
    Player* const holder = victim ? victim->ToPlayer() : nullptr;
    if (boss->isMoving() || !holder || !PlayerbotAI::IsTank(holder))
        return;

    float const tankClearance = ULDUAR_THORIM_CHAIN_LIGHTNING_JUMP + 2.0f * DEFAULT_COMBAT_REACH;
    float const anchor = RingAnchorBearing(boss);
    float held = std::numeric_limits<float>::max();
    float fresh = std::numeric_limits<float>::max();
    for (uint8 slot = 0; slot < ULDUAR_THORIM_MELEE_SLOTS; ++slot)
    {
        held = std::min(held, RingPointTankClearance(boss, state.ringSlots[slot].latched));
        fresh = std::min(fresh, RingPointTankClearance(boss, SlotBearing(anchor, slot)));
    }

    if (held >= tankClearance || fresh < held + ULDUAR_THORIM_RING_ANCHOR_GAIN)
        return;

    std::array<uint8, ULDUAR_THORIM_MELEE_SLOTS> order{};
    for (uint8 slot = 0; slot < ULDUAR_THORIM_MELEE_SLOTS; ++slot)
        order[slot] = slot;

    std::array<uint8, ULDUAR_THORIM_MELEE_SLOTS> best = order;
    float bestWorst = std::numeric_limits<float>::max();
    float bestTotal = std::numeric_limits<float>::max();
    do
    {
        float worst = 0.0f;
        float total = 0.0f;
        for (uint8 slot = 0; slot < ULDUAR_THORIM_MELEE_SLOTS; ++slot)
        {
            float const turn = AbsAngleDelta(state.ringSlots[slot].composed, SlotBearing(anchor, order[slot]));
            worst = std::max(worst, turn);
            total += turn;
        }

        if (worst < bestWorst || (worst <= bestWorst && total < bestTotal))
        {
            best = order;
            bestWorst = worst;
            bestTotal = total;
        }
    } while (std::next_permutation(order.begin(), order.end()));

    state.ringAnchor = anchor;
    for (uint8 slot = 0; slot < ULDUAR_THORIM_MELEE_SLOTS; ++slot)
    {
        state.ringSlots[slot].latched = SlotBearing(anchor, best[slot]);
        state.ringSlots[slot].composed = state.ringSlots[slot].latched;
    }

    std::vector<ObjectGuid> const arrived(state.ringArrived.begin(), state.ringArrived.end());
    for (ObjectGuid const& guid : arrived)
        state.ringArrived.erase(guid);
}

void SolveRingBearings(PlayerbotAI* botAI, Player* bot, Unit* boss)
{
    ThorimEncounterState& state = ThorimStateFor(bot);

    if (!state.ringLatched)
    {
        state.ringAnchor = RingAnchorBearing(boss);
        for (uint8 slot = 0; slot < ULDUAR_THORIM_MELEE_SLOTS; ++slot)
        {
            state.ringSlots[slot].latched = SlotBearing(state.ringAnchor, slot);
            state.ringSlots[slot].composed = state.ringSlots[slot].latched;
        }

        state.ringLatched = true;
    }

    Unit* orb = ThorimChargedThunderOrb(botAI, SPELL_THORIM_LIGHTNING_ORB_VISUAL);
    ObjectGuid const orbGuid = orb ? orb->GetGUID() : ObjectGuid::Empty;

    if (orbGuid == state.ringOrb && state.ringSolveMs &&
        GetMSTimeDiffToNow(state.ringSolveMs) < ULDUAR_THORIM_ENCOUNTER_SCAN_INTERVAL_MS)
        return;

    state.ringOrb = orbGuid;
    state.ringSolveMs = getMSTime();

    RelatchRingAnchor(state, boss);

    float const coneBearing = orb ? BearingFromBoss(boss, orb->GetPositionX(), orb->GetPositionY()) : 0.0f;

    std::array<float, ULDUAR_THORIM_MELEE_SLOTS> taken{};
    for (uint8 slot = 0; slot < ULDUAR_THORIM_MELEE_SLOTS; ++slot)
        taken[slot] = state.ringSlots[slot].latched;

    float const step = 0.0349f;
    for (uint8 slot = 0; slot < ULDUAR_THORIM_MELEE_SLOTS; ++slot)
    {
        float const base = state.ringSlots[slot].latched;

        int8 const firstWay =
            Position::NormalizeOrientation(state.ringSlots[slot].composed - base) > float(M_PI) ? -1 : 1;

        float answer = base;
        bool found = false;
        for (uint8 relax = 0; relax < 2 && !found; ++relax)
        {
            for (uint8 tick = 0; tick <= 90 && !found; ++tick)
            {
                for (int8 turn = 0; turn < 2; ++turn)
                {
                    float const candidate = Position::NormalizeOrientation(
                        base + static_cast<float>((turn ? -firstWay : firstWay) * tick) * step);

                    bool ok = !orb || !InLightningChargeCone(candidate, coneBearing);
                    if (ok && relax < 1)
                        ok = RingBearingSeparated(candidate, slot, taken, state.ringAnchor);

                    if (ok)
                    {
                        answer = candidate;
                        found = true;
                        break;
                    }

                    if (!tick)
                        break;
                }
            }
        }

        taken[slot] = answer;
        state.ringSlots[slot].composed = answer;
    }
}

float RingSlotBearing(PlayerbotAI* botAI, Player* bot, Unit* boss, uint8 slot)
{
    SolveRingBearings(botAI, bot, boss);

    ThorimEncounterState const* state = FindState(bot);
    if (!state || slot >= ULDUAR_THORIM_MELEE_SLOTS)
        return SlotBearing(RingAnchorBearing(boss), slot);

    return state->ringSlots[slot].composed;
}

bool StaticMeleeSpot(Unit* boss, uint8 slot, Position& out)
{
    static Position const* const spots[ULDUAR_THORIM_MELEE_SLOTS] = {
        &ULDUAR_THORIM_PHASE2_MELEE1_SPOT, &ULDUAR_THORIM_PHASE2_MELEE2_SPOT, &ULDUAR_THORIM_PHASE2_MELEE3_SPOT};

    if (slot >= ULDUAR_THORIM_MELEE_SLOTS)
        return false;

    Position const& spot = *spots[slot];
    if (boss->GetDistance(spot) > ULDUAR_THORIM_MELEE_RING_RADIUS + ULDUAR_THORIM_RING_ARRIVE_TOLERANCE)
        return false;

    out = spot;
    return true;
}

void NoteRunicSmashLane(Player* bot, Unit* colossus, bool leftLane, uint32 spellId)
{
    if (!RaidObs::Active())
        return;

    RaidObs::NoteHazard(bot->GetMap(), spellId, colossus->GetPosition(), "lane",
                        leftLane ? "\"side\":\"left\"" : "\"side\":\"right\"",
                        ULDUAR_THORIM_ENCOUNTER_SCAN_INTERVAL_MS);
}

void TickRunicSmash(PlayerbotAI* botAI, Player* bot)
{
    if (!NearThorimEncounter(bot))
        return;

    ThorimEncounterState& state = ThorimStateFor(bot);
    if (state.smashScanMs && GetMSTimeDiffToNow(state.smashScanMs) < ULDUAR_THORIM_ENCOUNTER_SCAN_INTERVAL_MS)
        return;

    state.smashScanMs = getMSTime();

    if (!state.gauntletTraced && bot->IsInCombat())
    {
        state.gauntletTraced = true;
        RaidObs::MarkPull(bot->GetMap(), GetThorim(botAI));
    }

    Unit* colossus = GetThorimRunicColossus(botAI);

    if (!colossus || !colossus->IsAlive() || colossus->IsInCombat())
    {
        state.runicSmashSide = 0;
        state.runicSmashSeenMs = 0;
        return;
    }

    if (colossus->FindCurrentSpellBySpellId(SPELL_THORIM_RUNIC_SMASH_LEFT))
    {
        state.runicSmashSide = SPELL_THORIM_RUNIC_SMASH_LEFT;
        state.runicSmashSeenMs = state.smashScanMs;
        NoteRunicSmashLane(bot, colossus, true, SPELL_THORIM_RUNIC_SMASH_LEFT);
    }
    else if (colossus->FindCurrentSpellBySpellId(SPELL_THORIM_RUNIC_SMASH_RIGHT))
    {
        state.runicSmashSide = SPELL_THORIM_RUNIC_SMASH_RIGHT;
        state.runicSmashSeenMs = state.smashScanMs;
        NoteRunicSmashLane(bot, colossus, false, SPELL_THORIM_RUNIC_SMASH_RIGHT);
    }
}

}

bool NearThorimEncounter(Player const* bot)
{
    if (!bot || bot->GetDistance(ULDUAR_THORIM_NEAR_ARENA_CENTER) > ULDUAR_THORIM_ENCOUNTER_PROXIMITY)
        return false;

    return bot->GetPositionZ() < ULDUAR_THORIM_WING_MAX_Z || OnThorimBalcony(bot);
}

Unit* GetThorim(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot)
        return nullptr;

    ThorimEncounterState& state = ThorimStateFor(bot);
    if (state.bossGuid)
    {
        Unit* cached = botAI->GetUnit(state.bossGuid);
        if (cached && cached->IsAlive())
            return cached;

        state.bossGuid.Clear();
    }

    Unit* boss = GetUldUnitByEntry(botAI, NPC_THORIM);
    if (boss)
        state.bossGuid = boss->GetGUID();

    return boss;
}

Unit* GetThorimRunicColossus(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot)
        return nullptr;

    ThorimEncounterState& state = ThorimStateFor(bot);
    if (state.colossusGuid)
    {
        Unit* cached = botAI->GetUnit(state.colossusGuid);
        if (cached && cached->IsAlive())
            return cached;
    }

    if (bot->GetExactDist(THORIM_RUNIC_COLOSSUS_SPAWN) > ULDUAR_THORIM_COLOSSUS_SEARCH_RANGE)
        return nullptr;

    if (state.colossusScanMs && GetMSTimeDiffToNow(state.colossusScanMs) < ULDUAR_THORIM_ENCOUNTER_SCAN_INTERVAL_MS)
        return nullptr;

    state.colossusScanMs = getMSTime();
    state.colossusGuid.Clear();

    Unit* colossus = bot->FindNearestCreature(NPC_RUNIC_COLOSSUS, ULDUAR_THORIM_COLOSSUS_SEARCH_RANGE, true);
    if (colossus)
        state.colossusGuid = colossus->GetGUID();

    return colossus;
}

void GatherThorimEncounterTargets(PlayerbotAI* botAI, ThorimEncounterTargets& out)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot)
        return;

    GuidVector const& units = botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get();
    for (ObjectGuid const& guid : units)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || !unit->IsHostileTo(bot))
            continue;

        if (bot->GetDistance(unit) > ULDUAR_THORIM_DPS_TARGET_RANGE)
            continue;

        switch (unit->GetEntry())
        {
            case NPC_DARK_RUNE_ACOLYTE_I:
            case NPC_DARK_RUNE_ACOLYTE_G:
                out.acolytes.push_back(unit);
                break;
            case NPC_DARK_RUNE_EVOKER:
                out.evokers.push_back(unit);
                break;
            case NPC_DARK_RUNE_CHAMPION:
                out.champions.push_back(unit);
                break;
            case NPC_DARK_RUNE_WARBRINGER:
                out.warbringers.push_back(unit);
                break;
            case NPC_DARK_RUNE_COMMONER:
                out.commoners.push_back(unit);
                break;
            case NPC_IRON_RING_GUARD:
            case NPC_IRON_HONOR_GUARD:
                out.guards.push_back(unit);
                break;
            case NPC_CAPTURED_MERCENARY_SOLDIER_ALLY:
            case NPC_CAPTURED_MERCENARY_SOLDIER_HORDE:
            case NPC_CAPTURED_MERCENARY_CAPTAIN_ALLY:
            case NPC_CAPTURED_MERCENARY_CAPTAIN_HORDE:
            case NPC_JORMUNGAR_BEHEMOT:
                out.trash.push_back(unit);
                break;
            case NPC_RUNIC_COLOSSUS:
                out.colossus = unit;
                break;
            case NPC_ANCIENT_RUNE_GIANT:
                out.runeGiant = unit;
                break;
            default:
                break;
        }
    }
}

bool ThorimDpsTargetAllowed(PlayerbotAI* botAI, Unit* target)
{
    if (!target || !target->IsAlive())
        return false;

    Unit* boss = GetThorim(botAI);
    if (!boss || boss->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
        return true;

    if (target == boss)
        return ThorimBalconyHitAllowed(botAI, botAI->GetBot());

    return target->GetEntry() != NPC_SIF;
}

bool ThorimBalconyHitAllowed(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot || bot->GetPositionZ() <= ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
        return false;

    if (!ThorimSplitActive(botAI) || GetThorimSquad(botAI, bot) != ThorimSquad::Gauntlet)
        return false;

    if (!ThorimBalconyWaypointReached(bot, ULDUAR_THORIM_BALCONY_WAYPOINTS - 2))
        return false;

    ThorimEncounterState const* state = FindState(bot);
    if (!state)
        return false;

    auto const itr = state->balconyStep.find(bot->GetGUID());
    return itr != state->balconyStep.end() && itr->second >= ULDUAR_THORIM_BALCONY_WAYPOINTS - 1;
}

namespace
{

Unit* SelectThorimTierTarget(Unit* currentTarget, std::vector<Unit*> const& candidates, Position const& pivot)
{
    Unit* selected = nullptr;
    for (Unit* candidate : candidates)
        if (candidate && candidate == currentTarget)
        {
            selected = candidate;
            break;
        }

    for (Unit* candidate : candidates)
    {
        if (!candidate || candidate == selected)
            continue;

        if (!selected)
        {
            selected = candidate;
            continue;
        }

        float const held = selected->GetExactDist2d(pivot.GetPositionX(), pivot.GetPositionY());
        float const offered = candidate->GetExactDist2d(pivot.GetPositionX(), pivot.GetPositionY());
        if (offered + ULDUAR_THORIM_TARGET_SWITCH_MARGIN < held)
            selected = candidate;
    }

    return selected;
}

Unit* NoteThorimDpsTarget(Player* bot, Unit* target)
{
    if (bot)
        if (ThorimEncounterState* state = FindState(bot))
            state->dpsTargets[bot->GetGUID()] = target ? target->GetGUID() : ObjectGuid::Empty;

    return target;
}

bool IsThorimGauntletMob(Unit const* unit)
{
    switch (unit->GetEntry())
    {
        case NPC_IRON_RING_GUARD:
        case NPC_IRON_HONOR_GUARD:
        case NPC_DARK_RUNE_ACOLYTE_G:
        case NPC_RUNIC_COLOSSUS:
        case NPC_ANCIENT_RUNE_GIANT:
            return true;
        default:
            return false;
    }
}

void DropIdleGauntletMobsWhileFighting(ThorimEncounterTargets& targets)
{
    auto const engaged = [](Unit* unit) { return unit && IsThorimGauntletMob(unit) && unit->IsInCombat(); };
    bool const fighting = std::any_of(targets.acolytes.begin(), targets.acolytes.end(), engaged) ||
                          std::any_of(targets.guards.begin(), targets.guards.end(), engaged) ||
                          engaged(targets.colossus) || engaged(targets.runeGiant);
    if (!fighting)
        return;

    auto const idle = [](Unit* unit) { return unit && IsThorimGauntletMob(unit) && !unit->IsInCombat(); };
    std::erase_if(targets.acolytes, idle);
    std::erase_if(targets.guards, idle);
    if (idle(targets.colossus))
        targets.colossus = nullptr;
    if (idle(targets.runeGiant))
        targets.runeGiant = nullptr;
}

}

Unit* GetThorimDpsTarget(PlayerbotAI* botAI, Player* bot, Unit* currentTarget)
{
    if (!botAI || !bot || !NearThorimEncounter(bot))
        return nullptr;

    Unit* boss = GetThorim(botAI);
    if (!boss || !boss->IsAlive() || !boss->IsHostileTo(bot))
        return nullptr;

    if (boss->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
    {
        if (GetThorimPhase2Role(botAI, bot) == ThorimPhase2Role::Ranged)
        {
            ThorimEncounterTargets leftovers;
            GatherThorimEncounterTargets(botAI, leftovers);

            std::vector<Unit*> inside;
            for (Unit* candidate : leftovers.evokers)
                if (ThorimInArenaBox(candidate))
                    inside.push_back(candidate);

            if (Unit* pick = SelectThorimTierTarget(currentTarget, inside, ULDUAR_THORIM_NEAR_ARENA_CENTER))
                return NoteThorimDpsTarget(bot, pick);
        }

        return NoteThorimDpsTarget(bot, boss);
    }

    ThorimEncounterTargets targets;
    GatherThorimEncounterTargets(botAI, targets);

    if (GetThorimSquad(botAI, bot) == ThorimSquad::Gauntlet)
    {
        if (Unit* giant = ThorimGauntletGiantHeld(botAI, bot))
            return NoteThorimDpsTarget(bot, giant);

        DropIdleGauntletMobsWhileFighting(targets);

        std::vector<std::vector<Unit*> const*> const tiers = {&targets.acolytes, &targets.guards,
                                                              &targets.trash};
        for (auto const* tier : tiers)
            if (Unit* pick = SelectThorimTierTarget(currentTarget, *tier, *bot))
                return NoteThorimDpsTarget(bot, pick);

        if (targets.colossus &&
            (targets.colossus->IsInCombat() || ThorimGauntletColossusPull(botAI, bot) == targets.colossus))
            return NoteThorimDpsTarget(bot, targets.colossus);

        if (targets.runeGiant)
            return NoteThorimDpsTarget(bot, targets.runeGiant);

        if (ThorimBalconyHitAllowed(botAI, bot))
            return NoteThorimDpsTarget(bot, boss);

        return NoteThorimDpsTarget(bot, nullptr);
    }

    bool const melee = PlayerbotAI::IsMelee(bot);
    using TierOrder = std::array<std::vector<Unit*> const*, 6>;
    TierOrder const meleeTiers = {&targets.acolytes, &targets.champions, &targets.warbringers,
                                  &targets.evokers, &targets.commoners, &targets.trash};
    TierOrder const rangedTiers = {&targets.acolytes, &targets.evokers, &targets.champions,
                                   &targets.warbringers, &targets.commoners, &targets.trash};
    TierOrder const& tiers = melee ? meleeTiers : rangedTiers;

    Position const& pivot = melee ? static_cast<Position const&>(*bot) : ULDUAR_THORIM_NEAR_ARENA_CENTER;

    for (int pass = 0; pass < (melee ? 2 : 1); ++pass)
    {
        bool const limitReach = melee && pass == 0;

        for (auto const* tier : tiers)
        {
            std::vector<Unit*> inside;
            for (Unit* candidate : *tier)
            {
                if (!ThorimInArenaBox(candidate))
                    continue;

                if (limitReach && bot->GetExactDist2d(candidate) > ULDUAR_THORIM_MELEE_TARGET_REACH)
                    continue;

                inside.push_back(candidate);
            }

            if (Unit* pick = SelectThorimTierTarget(currentTarget, inside, pivot))
                return NoteThorimDpsTarget(bot, pick);
        }
    }

    return NoteThorimDpsTarget(bot, nullptr);
}

bool ThorimHasDpsTarget(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot || !NearThorimEncounter(bot))
        return false;

    Unit* boss = GetThorim(botAI);
    if (!boss || !boss->IsAlive())
        return false;

    ThorimEncounterState const* state = FindState(bot);
    if (!state)
        return false;

    auto const itr = state->dpsTargets.find(bot->GetGUID());
    return itr != state->dpsTargets.end() && !itr->second.IsEmpty();
}

void ThorimClearStaleMarks(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot)
        return;

    botAI->GetAiObjectContext()->GetValue<GuidVector>("prioritized targets")->Set({});

    Group* group = bot->GetGroup();
    if (!group)
        return;

    ThorimEncounterState& state = ThorimStateFor(bot);
    if (state.marksCleared)
        return;

    state.marksCleared = true;

    for (int8 const icon : {RtiTargetValue::skullIndex, RtiTargetValue::crossIndex, RtiTargetValue::moonIndex})
        if (group->GetTargetIcon(icon))
            group->SetTargetIcon(icon, bot->GetGUID(), ObjectGuid::Empty);
}

Position const& GetThorimGauntletWaypoint(bool leftLane, uint8 index)
{
    return *LaneWaypoints(leftLane)[std::min<uint8>(index, ULDUAR_THORIM_GAUNTLET_WAYPOINTS - 1)].position;
}

bool ThorimGauntletLaneIndexInLane(WorldObject const* who, bool leftLane, uint8& index)
{
    if (!who)
        return false;

    auto const& lane = LaneWaypoints(leftLane);

    bool standing = false;
    uint8 nearest = 0;
    float nearestDistance = std::numeric_limits<float>::max();

    for (uint8 i = 0; i < ULDUAR_THORIM_GAUNTLET_WAYPOINTS; ++i)
    {
        float const distance = who->GetDistance(*lane[i].position);
        if (distance < lane[i].radius)
            standing = true;

        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearest = i;
        }
    }

    if (!standing)
        return false;

    index = nearest;
    return true;
}

bool ThorimGauntletLaneIndex(WorldObject const* who, uint8& index, bool& leftLane)
{
    if (ThorimGauntletLaneIndexInLane(who, true, index))
    {
        leftLane = true;
        return true;
    }

    if (ThorimGauntletLaneIndexInLane(who, false, index))
    {
        leftLane = false;
        return true;
    }

    return false;
}

bool ThorimResolveGauntletIndex(PlayerbotAI* botAI, Player* bot, uint8& index)
{
    if (!botAI || !bot)
        return false;

    bool lane = false;
    if (Player* leader = ThorimGauntletLeader(botAI))
        if (ThorimGauntletLaneIndex(leader, index, lane))
            return true;

    return ThorimGauntletLaneIndex(bot, index, lane);
}

namespace
{
constexpr uint32 THORIM_ARENA_LEVER_ENTRY = 194264;
constexpr float THORIM_LEVER_SEARCH_RANGE = 150.0f;
constexpr uint32 THORIM_FIRST_COLOSSUS_DOORS_ENTRY = 194557;
constexpr uint32 THORIM_SECOND_COLOSSUS_DOORS_ENTRY = 194558;

Position const THORIM_RUNE_GIANT_SPAWN(2134.57f, -440.318f, 438.331f);

bool OnThisInstance(Player const* member, Player const* bot)
{
    return member && member->IsInWorld() && member->GetMapId() == bot->GetMapId() &&
           member->GetInstanceId() == bot->GetInstanceId();
}

bool ThorimGauntletSquadWithin(Player* bot, ThorimEncounterState const& state, float distance)
{
    if (getMSTimeDiff(state.leadStepMs, getMSTime()) >= ULDUAR_THORIM_LEAD_STALL_MS)
        return true;

    Group* group = bot->GetGroup();
    if (!group)
        return true;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member == bot || !OnThisInstance(member, bot) || !member->IsAlive() || !IsBotPlayer(member) ||
            PlayerbotAI::IsHeal(member) || SquadOf(state, member) != ThorimSquad::Gauntlet)
            continue;

        if (bot->GetDistance2d(member) > distance)
            return false;
    }

    return true;
}

bool ThorimColossusDoorsOpen(PlayerbotAI* botAI, Player* bot)
{
    ThorimEncounterState& state = ThorimStateFor(bot);
    GameObject* doors = state.colossusDoorsGuid ? botAI->GetGameObject(state.colossusDoorsGuid) : nullptr;
    if (!doors)
    {
        doors = bot->FindNearestGameObject(THORIM_FIRST_COLOSSUS_DOORS_ENTRY, ULDUAR_THORIM_COLOSSUS_SEARCH_RANGE);
        state.colossusDoorsGuid = doors ? doors->GetGUID() : ObjectGuid::Empty;
    }

    return doors && doors->GetGoState() == GO_STATE_ACTIVE;
}

bool ThorimGiantDoorsOpen(PlayerbotAI* botAI, Player* bot)
{
    ThorimEncounterState& state = ThorimStateFor(bot);
    GameObject* doors = state.giantDoorsGuid ? botAI->GetGameObject(state.giantDoorsGuid) : nullptr;
    if (!doors)
    {
        doors = bot->FindNearestGameObject(THORIM_SECOND_COLOSSUS_DOORS_ENTRY, ULDUAR_THORIM_COLOSSUS_SEARCH_RANGE);
        state.giantDoorsGuid = doors ? doors->GetGUID() : ObjectGuid::Empty;
    }

    return doors && doors->GetGoState() == GO_STATE_ACTIVE;
}

bool ThorimGauntletLeaderFighting(PlayerbotAI* botAI, Player* bot)
{
    if (ThorimGauntletFighting(botAI, bot))
        return true;

    for (Unit* attacker : bot->getAttackers())
        if (attacker && attacker->IsAlive() && IsThorimGauntletMob(attacker))
            return true;

    return false;
}

bool IsThorimStartPackMob(Unit const* unit)
{
    switch (unit->GetEntry())
    {
        case NPC_JORMUNGAR_BEHEMOT:
        case NPC_CAPTURED_MERCENARY_SOLDIER_ALLY:
        case NPC_CAPTURED_MERCENARY_SOLDIER_HORDE:
        case NPC_CAPTURED_MERCENARY_CAPTAIN_ALLY:
        case NPC_CAPTURED_MERCENARY_CAPTAIN_HORDE:
        case NPC_DARK_RUNE_ACOLYTE_I:
            return true;
        default:
            return false;
    }
}

bool ThorimStartPackOn(Player* bot)
{
    for (Unit* attacker : bot->getAttackers())
        if (attacker && attacker->IsAlive() && IsThorimStartPackMob(attacker))
            return true;

    return false;
}

bool ThorimCorridorTrashNear(PlayerbotAI* botAI)
{
    ThorimEncounterTargets targets;
    GatherThorimEncounterTargets(botAI, targets);

    auto const corridorTrash = [](Unit* unit)
    { return unit->GetEntry() == NPC_IRON_RING_GUARD || unit->GetEntry() == NPC_DARK_RUNE_ACOLYTE_G; };

    return std::any_of(targets.guards.begin(), targets.guards.end(), corridorTrash) ||
           std::any_of(targets.acolytes.begin(), targets.acolytes.end(), corridorTrash);
}
}

Player* ThorimGauntletLeader(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    Group* group = bot ? bot->GetGroup() : nullptr;
    ThorimEncounterState const* state = FindState(bot);
    if (!group || !state || !state->squadsAssigned)
        return nullptr;

    if (ThorimPhase2Active(botAI))
        return nullptr;

    Player* tank = nullptr;
    Player* fallback = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!OnThisInstance(member, bot) || !member->IsAlive())
            continue;

        if (!IsBotPlayer(member))
        {
            if (HumanInGauntlet(member))
                return member;

            continue;
        }

        if (SquadOf(*state, member) != ThorimSquad::Gauntlet)
            continue;

        if (PlayerbotAI::IsTank(member))
            tank = tank ? tank : member;
        else if (!fallback && !PlayerbotAI::IsHeal(member))
            fallback = member;
    }

    return tank ? tank : fallback;
}

bool ThorimIsBotGauntletLeader(PlayerbotAI* botAI, Player* bot)
{
    return IsBotPlayer(bot) && ThorimGauntletLeader(botAI) == bot;
}

bool ThorimGauntletFighting(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot)
        return false;

    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    if (!target || !target->IsAlive() || !target->IsInCombat() || !target->IsHostileTo(bot))
        return false;

    return IsThorimGauntletMob(target) && bot->GetDistance(target) <= ULDUAR_THORIM_DPS_TARGET_RANGE;
}

bool ThorimTrailsGauntletLeader(PlayerbotAI* botAI, Player* bot, Player* leader)
{
    if (!bot || !leader)
        return false;

    bool const healer = PlayerbotAI::IsHeal(bot);
    float const trail = healer ? ULDUAR_THORIM_GAUNTLET_TETHER_DISTANCE : ULDUAR_THORIM_LEAD_FOLLOW_DISTANCE;
    if (bot->GetDistance2d(leader) <= trail)
        return false;

    return healer || (!ThorimGauntletFighting(botAI, bot) && !ThorimFightingStartPack(botAI, bot));
}

Position ThorimGauntletTrailSpot(Player* bot, Player* leader)
{
    Position const spot = leader->GetPosition();
    if (!PlayerbotAI::IsHeal(bot))
        return spot;

    PathGenerator path(bot);
    path.CalculatePath(spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false);
    if (!(path.GetPathType() & PATHFIND_NORMAL) || path.GetPath().size() < 2)
        return spot;

    path.ShortenPathUntilDist(G3D::Vector3(spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ()),
                              ULDUAR_THORIM_GAUNTLET_HEALER_STANDOFF);
    G3D::Vector3 const& end = path.GetPath().back();
    return Position(end.x, end.y, end.z);
}

bool ThorimFightingStartPack(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot || PlayerbotAI::IsHeal(bot))
        return false;

    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    if (!target || !target->IsAlive() || !target->IsInCombat() || !target->IsHostileTo(bot))
        return false;

    return IsThorimStartPackMob(target) && bot->GetDistance(target) <= ULDUAR_THORIM_DPS_TARGET_RANGE;
}

bool ThorimLeverUsable(Player* bot, ObjectGuid& leverGuid)
{
    ThorimEncounterState* state = FindState(bot);
    if (!state || state->leverPulled)
        return false;

    GameObject* lever = bot->FindNearestGameObject(THORIM_ARENA_LEVER_ENTRY, THORIM_LEVER_SEARCH_RANGE);

    bool const locked = lever && lever->HasGameObjectFlag(GO_FLAG_NOT_SELECTABLE);

    if (!locked && PastThorimPortcullis(bot))
    {
        state->leverPulled = true;
        state->leadStepMs = getMSTime();
        return false;
    }

    if (!lever || locked)
        return false;

    if (lever->GetGoState() != GO_STATE_READY)
    {
        state->leverPulled = true;
        state->leadStepMs = getMSTime();
        return false;
    }

    leverGuid = lever->GetGUID();
    return true;
}

bool ThorimLeverWaiting(PlayerbotAI* botAI, Player* bot, ObjectGuid& leverGuid)
{
    ThorimEncounterState const* state = FindState(bot);
    if (!state || state->leverPulled || GetThorimSquad(botAI, bot) != ThorimSquad::Gauntlet ||
        !ThorimSplitActive(botAI) || PastThorimPortcullis(bot) || !ThorimIsBotGauntletLeader(botAI, bot) ||
        ThorimStartPackOn(bot))
        return false;

    GameObject* lever = bot->FindNearestGameObject(THORIM_ARENA_LEVER_ENTRY, THORIM_LEVER_SEARCH_RANGE);
    if (!lever || !lever->HasGameObjectFlag(GO_FLAG_NOT_SELECTABLE))
        return false;

    leverGuid = lever->GetGUID();
    return true;
}

void ThorimNoteLeverPulled(Player* bot)
{
    if (ThorimEncounterState* state = FindState(bot))
    {
        state->leverPulled = true;
        state->leadStepMs = getMSTime();
    }
}

bool ThorimGauntletLeadStep(PlayerbotAI* botAI, Player* bot, Position& out)
{
    ThorimEncounterState* state = FindState(bot);
    if (!state || !state->leverPulled || state->leadNext >= ULDUAR_THORIM_GAUNTLET_WAYPOINTS)
        return false;

    if (bot->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD || ThorimPhase2Active(botAI))
        return false;

    bool leftLane = false;
    ThorimPreferredGauntletLane(botAI, leftLane);

    uint32 const now = getMSTime();

    if (PastThorimPortcullis(bot))
    {
        while (state->leadNext < ULDUAR_THORIM_GAUNTLET_WAYPOINTS &&
               bot->GetPositionY() < GetThorimGauntletWaypoint(leftLane, state->leadNext).GetPositionY())
        {
            ++state->leadNext;
            state->leadStepMs = now;
        }

        if (state->leadNext >= ULDUAR_THORIM_GAUNTLET_WAYPOINTS)
            return false;
    }

    uint8 index = 0;
    if (ThorimGauntletLaneIndexInLane(bot, leftLane, index) && index >= state->leadNext)
    {
        state->leadNext = index + 1;
        state->leadStepMs = now;
        if (state->leadNext >= ULDUAR_THORIM_GAUNTLET_WAYPOINTS)
            return false;
    }

    if (ThorimGauntletLeaderFighting(botAI, bot) ||
        !ThorimGauntletSquadWithin(bot, *state, ULDUAR_THORIM_LEAD_COHESION_DISTANCE))
        return false;

    out = GetThorimGauntletWaypoint(leftLane, state->leadNext);
    return true;
}

Unit* ThorimGauntletLeadColossus(PlayerbotAI* botAI, Player* bot)
{
    ThorimEncounterState const* state = FindState(bot);
    if (!state || state->leadNext < ULDUAR_THORIM_GAUNTLET_WAYPOINTS ||
        bot->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD || ThorimPhase2Active(botAI) ||
        ThorimGauntletLeaderFighting(botAI, bot))
        return nullptr;

    Unit* colossus = GetThorimRunicColossus(botAI);
    if (!colossus || !colossus->IsAlive())
        return nullptr;

    if (bot->GetDistance(colossus) > ULDUAR_THORIM_LEAD_COLOSSUS_DISTANCE)
        return colossus;

    if (colossus->IsInCombat() || ThorimCorridorTrashNear(botAI) ||
        !ThorimGauntletSquadWithin(bot, *state, ULDUAR_THORIM_LEAD_COLOSSUS_GATHER_DISTANCE))
        return nullptr;

    return colossus;
}

Unit* ThorimGauntletColossusPull(PlayerbotAI* botAI, Player* bot)
{
    if (!ThorimIsBotGauntletLeader(botAI, bot))
        return nullptr;

    Unit* colossus = ThorimGauntletLeadColossus(botAI, bot);
    return colossus && bot->GetDistance(colossus) <= ULDUAR_THORIM_LEAD_COLOSSUS_DISTANCE ? colossus : nullptr;
}

Unit* ThorimGauntletLeadRuneGiant(PlayerbotAI* botAI, Player* bot)
{
    ThorimEncounterState const* state = FindState(bot);
    if (!state || state->leadNext < ULDUAR_THORIM_GAUNTLET_WAYPOINTS || !PastThorimPortcullis(bot) ||
        bot->GetExactDist(THORIM_RUNE_GIANT_SPAWN) > ULDUAR_THORIM_COLOSSUS_SEARCH_RANGE ||
        ThorimPhase2Active(botAI))
        return nullptr;

    Unit* giant = GetThorimAncientRuneGiant(botAI);
    if (!giant || giant->IsInCombat())
        return nullptr;

    if (!ThorimColossusDoorsOpen(botAI, bot))
        return nullptr;

    return giant;
}

Unit* ThorimGauntletGiantHeld(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot || ThorimPhase2Active(botAI) || !PastThorimPortcullis(bot) ||
        bot->GetExactDist(THORIM_RUNE_GIANT_SPAWN) > ULDUAR_THORIM_COLOSSUS_SEARCH_RANGE ||
        !ThorimIsBotGauntletLeader(botAI, bot))
        return nullptr;

    Unit* giant = GetThorimAncientRuneGiant(botAI);
    return giant && giant->IsInCombat() && giant->GetVictim() == bot ? giant : nullptr;
}

bool ThorimGauntletGiantHold(PlayerbotAI* botAI, Player* bot, Position& out)
{
    if (!ThorimGauntletGiantHeld(botAI, bot) ||
        bot->GetExactDist2d(&ULDUAR_THORIM_RUNE_GIANT_HOLD_SPOT) <= ULDUAR_THORIM_RUNE_GIANT_HOLD_TOLERANCE)
        return false;

    out = ULDUAR_THORIM_RUNE_GIANT_HOLD_SPOT;
    return true;
}

bool ThorimGiantWalkBooked(LastMovement const& booking)
{
    return booking.priority == MovementPriority::MOVEMENT_COMBAT &&
           std::hypot(booking.lastMoveToX - THORIM_RUNE_GIANT_SPAWN.GetPositionX(),
                      booking.lastMoveToY - THORIM_RUNE_GIANT_SPAWN.GetPositionY()) <=
               ULDUAR_THORIM_RUNE_GIANT_HOLD_TOLERANCE;
}

bool ThorimPreferredGauntletLane(PlayerbotAI* botAI, bool& useLeftLane)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || bot->GetMapId() != ULDUAR_MAP_ID)
        return false;

    TickRunicSmash(botAI, bot);

    ThorimEncounterState const* state = FindState(bot);
    if (!state || !state->runicSmashSide)
        return false;

    useLeftLane = state->runicSmashSide == SPELL_THORIM_RUNIC_SMASH_RIGHT;
    return true;
}

bool ThorimRunicSmashImminent(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || bot->GetMapId() != ULDUAR_MAP_ID)
        return false;

    TickRunicSmash(botAI, bot);

    ThorimEncounterState const* state = FindState(bot);
    if (!state || !state->runicSmashSeenMs)
        return false;

    return GetMSTimeDiffToNow(state->runicSmashSeenMs) < ULDUAR_THORIM_RUNIC_SMASH_LATCH_MS;
}

Position const& GetThorimBalconyWaypoint(uint8 index)
{
    return *balconyWaypoints[std::min<uint8>(index, ULDUAR_THORIM_BALCONY_WAYPOINTS - 1)];
}

Unit* GetThorimAncientRuneGiant(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot)
        return nullptr;

    ThorimEncounterState& state = ThorimStateFor(bot);
    if (state.runeGiantScanMs && GetMSTimeDiffToNow(state.runeGiantScanMs) < ULDUAR_THORIM_ENCOUNTER_SCAN_INTERVAL_MS)
    {
        Unit* cached = botAI->GetUnit(state.runeGiantGuid);
        return cached && cached->IsAlive() ? cached : nullptr;
    }

    state.runeGiantScanMs = getMSTime();
    state.runeGiantGuid.Clear();

    Unit* giant = bot->FindNearestCreature(NPC_ANCIENT_RUNE_GIANT, ULDUAR_THORIM_COLOSSUS_SEARCH_RANGE, true);
    if (giant)
        state.runeGiantGuid = giant->GetGUID();

    return giant;
}

bool ThorimOnBalconySide(Player const* bot)
{
    if (bot->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
        return true;

    float const x = bot->GetPositionX();
    float const y = bot->GetPositionY();
    return x >= ULDUAR_THORIM_RAMP_BOX_MIN_X && x <= ULDUAR_THORIM_RAMP_BOX_MAX_X &&
           y >= ULDUAR_THORIM_RAMP_BOX_MIN_Y && y <= ULDUAR_THORIM_RAMP_BOX_MAX_Y;
}

bool ThorimBalconyOpen(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot || !NearThorimEncounter(bot))
        return false;

    if (!ThorimOnBalconySide(bot))
        return false;

    if (bot->GetPositionZ() <= ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD && !ThorimGiantDoorsOpen(botAI, bot))
        return false;

    Unit* boss = GetThorim(botAI);
    if (!boss || !boss->IsAlive())
        return false;

    return GetThorimAncientRuneGiant(botAI) == nullptr;
}

uint8 ThorimAdvanceBalconyStep(Player* bot)
{
    ThorimEncounterState* state = FindState(bot);
    if (!state)
        return 0;

    auto const& steps = state->balconyStep;
    auto const itr = steps.find(bot->GetGUID());
    uint8 step = itr == steps.end() ? 0 : itr->second;

    while (step > 0 && !ThorimBalconyWaypointReached(bot, step - 1))
        --step;

    while (step < ULDUAR_THORIM_BALCONY_WAYPOINTS && ThorimBalconyWaypointReached(bot, step))
        ++step;

    state->balconyStep[bot->GetGUID()] = step;
    return step;
}

namespace
{
Unit* ThorimBarrierBailColossus(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot || bot->GetMapId() != ULDUAR_MAP_ID)
        return nullptr;

    if (!PlayerbotAI::IsMelee(bot) || PlayerbotAI::IsTank(bot) || !NearThorimEncounter(bot) ||
        GetThorimSquad(botAI, bot) != ThorimSquad::Gauntlet)
        return nullptr;

    return GetThorimRunicColossus(botAI);
}

bool ThorimColossusShielded(Unit* colossus)
{
    return colossus && colossus->IsAlive() && colossus->HasAura(SPELL_THORIM_RUNIC_BARRIER);
}
}

void ThorimTickBarrierBail(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot || bot->GetMapId() != ULDUAR_MAP_ID)
        return;

    if (!PlayerbotAI::IsMelee(bot) || PlayerbotAI::IsTank(bot) || !NearThorimEncounter(bot))
        return;

    ThorimEncounterState& state = ThorimStateFor(bot);
    if (!ThorimColossusShielded(ThorimBarrierBailColossus(botAI, bot)))
    {
        state.barrierBailing.erase(bot->GetGUID());
        return;
    }

    if (state.barrierBailing.count(bot->GetGUID()))
    {
        if (bot->GetHealthPct() >= ULDUAR_THORIM_BARRIER_RESUME_HEALTH_PCT)
            state.barrierBailing.erase(bot->GetGUID());

        return;
    }

    if (bot->GetHealthPct() <= ULDUAR_THORIM_BARRIER_BAIL_HEALTH_PCT)
        state.barrierBailing.insert(bot->GetGUID());
}

namespace
{

Player* ElectThorimFrostAuraPaladin(Group* group)
{
    Player* best = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        if (!memberAI || !member->IsAlive() || member->GetMapId() != ULDUAR_MAP_ID ||
            member->getClass() != CLASS_PALADIN ||
            !memberAI->GetAiObjectContext()->GetValue<uint32>("spell id", "frost resistance aura")->Get())
            continue;

        if (!best || member->GetGUID() < best->GetGUID())
            best = member;
    }

    return best;
}

}

void ThorimTickFrostAuraPaladin(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !NearThorimEncounter(bot))
        return;

    Group* group = bot->GetGroup();
    if (!group)
        return;

    ThorimEncounterState& state = ThorimStateFor(bot);
    uint32 const now = getMSTime();
    if (state.frostAuraScanMs &&
        getMSTimeDiff(state.frostAuraScanMs, now) < ULDUAR_THORIM_ENCOUNTER_SCAN_INTERVAL_MS)
        return;

    state.frostAuraScanMs = now;

    if (!state.sifJoined.Get() && !ThorimBlizzardSpots(bot).empty())
        state.sifJoined = true;

    Player* paladin = state.sifJoined.Get() ? ElectThorimFrostAuraPaladin(group) : nullptr;
    state.frostAuraPaladin = paladin ? paladin->GetGUID() : ObjectGuid::Empty;
}

bool IsThorimFrostAuraPaladin(Player const* bot)
{
    ThorimEncounterState const* state = FindState(bot);
    return bot && state && state->frostAuraPaladin.Get() == bot->GetGUID();
}

void ThorimTickHumanTankCalls(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || bot->GetMapId() != ULDUAR_MAP_ID || !ThorimPhase2Active(botAI))
        return;

    Unit* boss = GetThorim(botAI);
    if (!boss)
        return;

    constexpr uint32 scanIntervalMs = 250;
    ThorimEncounterState& state = ThorimStateFor(bot);
    uint32 const now = getMSTime();
    if (getMSTimeDiff(state.humanCallScanMs, now) < scanIntervalMs)
        return;

    state.humanCallScanMs = now;

    Unit* victim = boss->GetVictim();
    Player* holder = victim ? victim->ToPlayer() : nullptr;
    ObjectGuid const holderGuid = holder ? holder->GetGUID() : ObjectGuid::Empty;
    if (holderGuid != state.humanCallHolder)
    {
        state.humanCallHolder = holderGuid;
        state.humanCallTurnDone = false;
    }

    if (!holder || IsBotPlayer(holder) || holder->HasAura(SPELL_UNBALANCING_STRIKE))
        return;

    uint32 const spellId = ReadyTankDefensiveSpell(holder, true, 0, 0);
    if (!spellId)
        return;

    if (holder->GetHealthPct() < ULDUAR_THORIM_HUMAN_CALL_LOW_PCT)
    {
        CallHumanTankDefensive(bot, holder, spellId, 0, "you are low", "thorim.humancall");
        return;
    }

    uint32 const stacks = boss->GetAuraCount(SPELL_THORIM_LIGHTNING_CHARGE_BUFF);
    if (!state.humanCallTurnDone && stacks >= ULDUAR_THORIM_HUMAN_CALL_STACKS)
        state.humanCallTurnDone = CallHumanTankDefensive(
            bot, holder, spellId, 0, "Thorim hits hard (" + std::to_string(stacks) + " stacks)", "thorim.humancall");
}

bool ThorimBarrierBailLatched(PlayerbotAI* botAI, Player* bot)
{
    if (!ThorimColossusShielded(ThorimBarrierBailColossus(botAI, bot)))
        return false;

    ThorimEncounterState const* state = FindState(bot);
    return state && state->barrierBailing.count(bot->GetGUID());
}

bool ThorimSplitActive(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!NearThorimEncounter(bot))
        return false;

    Unit* boss = GetThorim(botAI);
    if (!boss || !boss->IsHostileTo(bot))
        return false;

    return boss->IsInCombat() && boss->GetPositionZ() >= ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD;
}

ThorimSquad GetThorimSquad(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot || bot->GetMapId() != ULDUAR_MAP_ID)
        return ThorimSquad::None;

    if (!NearThorimEncounter(bot))
        return ThorimSquad::None;

    if (!GetThorim(botAI))
        return ThorimSquad::None;

    AssignThorimSquads(bot);
    TickHumanSquads(bot);

    if (ThorimEncounterState* live = FindState(bot); live && live->squadsAssigned && !live->squadsNoted &&
        RaidObs::Active())
    {
        live->squadsNoted = true;
        for (auto const& assignment : live->squads)
            RaidObs::NoteAssignment(assignment.first, "thorim.squad", std::to_string(assignment.second));
    }

    ThorimEncounterState const* state = FindState(bot);
    if (!state)
        return ThorimSquad::None;

    auto const itr = state->squads.find(bot->GetGUID());
    return itr == state->squads.end() ? ThorimSquad::Arena : static_cast<ThorimSquad>(itr->second);
}

bool ThorimGauntletSquadOnClock(PlayerbotAI* botAI, Player* bot)
{
    return ThorimSplitActive(botAI) && GetThorimSquad(botAI, bot) == ThorimSquad::Gauntlet;
}

bool ThorimCorridorFleeHeld(PlayerbotAI* botAI, Player* bot)
{
    return PastThorimPortcullis(bot) && ThorimGauntletSquadOnClock(botAI, bot);
}

bool ThorimInArenaBox(WorldObject const* who)
{
    if (!who)
        return false;

    float const x = who->GetPositionX();
    float const y = who->GetPositionY();

    return x > ULDUAR_THORIM_ARENA_BOX_MIN_X && x < ULDUAR_THORIM_ARENA_BOX_MAX_X &&
           y > ULDUAR_THORIM_ARENA_BOX_MIN_Y && y < ULDUAR_THORIM_ARENA_BOX_MAX_Y &&
           who->GetPositionZ() < ULDUAR_THORIM_ARENA_BOX_MAX_Z;
}

namespace
{

constexpr uint32 ULDUAR_THORIM_PET_RECALL_INTERVAL_MS = 2000;

}

bool ThorimStrayPets(PlayerbotAI* botAI, Player* bot, std::vector<Unit*>& out)
{
    out.clear();

    if (!botAI || !bot || !ThorimSplitActive(botAI))
        return false;

    ThorimSquad const squad = GetThorimSquad(botAI, bot);
    if (squad == ThorimSquad::None)
        return false;

    ThorimEncounterState const* state = FindState(bot);
    if (!state)
        return false;

    Position const& centre = ULDUAR_THORIM_NEAR_ARENA_CENTER;

    for (Unit* pet : bot->m_Controlled)
    {
        if (!pet || !pet->IsAlive() || pet->IsTotem())
            continue;

        if (!pet->IsPet() && !pet->IsGuardian())
            continue;

        if (!pet->IsInWorld() || pet->GetMapId() != bot->GetMapId())
            continue;

        bool home;
        if (squad == ThorimSquad::Arena)
            home = ThorimInArenaBox(pet) && pet->GetExactDist2d(centre.GetPositionX(), centre.GetPositionY()) <=
                                                ULDUAR_THORIM_ARENA_LEASH_RADIUS;
        else
            home = pet->GetExactDist2d(bot->GetPositionX(), bot->GetPositionY()) <=
                   ULDUAR_THORIM_PET_OWNER_LEASH_RADIUS;

        if (home)
            continue;

        auto const sent = state->petRecallMs.find(pet->GetGUID());
        if (sent != state->petRecallMs.end() &&
            GetMSTimeDiffToNow(sent->second) < ULDUAR_THORIM_PET_RECALL_INTERVAL_MS)
            continue;

        out.push_back(pet);
    }

    return !out.empty();
}

void ThorimRecallPet(Player* bot, Unit* pet)
{
    if (!bot || !pet)
        return;

    ThorimEncounterState* state = FindState(bot);
    if (!state)
        return;

    state->petRecallMs[pet->GetGUID()] = getMSTime();
    state->petRecalls[bot->GetGUID()] = pet->GetGUID();

    pet->AttackStop();
    pet->CastStop();
    pet->GetMotionMaster()->MoveFollow(bot, PET_FOLLOW_DIST, pet->GetFollowAngle());

    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
        return;

    charmInfo->SetCommandState(COMMAND_FOLLOW);
    charmInfo->SetIsCommandAttack(false);
    charmInfo->SetIsAtStay(false);
    charmInfo->SetIsReturning(true);
    charmInfo->SetIsCommandFollow(true);
    charmInfo->SetIsFollowing(false);
    charmInfo->RemoveStayPosition();
}

bool ThorimArenaLeashBreached(PlayerbotAI* botAI, Player* bot)
{
    if (!bot || !ThorimSplitActive(botAI))
        return false;

    if (GetThorimSquad(botAI, bot) != ThorimSquad::Arena)
        return false;

    float const distance = bot->GetExactDist2d(ULDUAR_THORIM_NEAR_ARENA_CENTER.GetPositionX(),
                                               ULDUAR_THORIM_NEAR_ARENA_CENTER.GetPositionY());

    bool const melee = !PlayerbotAI::IsTank(bot) && !PlayerbotAI::IsRanged(bot);
    float const leash = melee ? ULDUAR_THORIM_ARENA_MELEE_LEASH : ULDUAR_THORIM_ARENA_LEASH_RADIUS;

    return distance > leash || !ThorimInArenaBox(bot);
}

namespace
{

bool ThorimArenaRingOrder(Player* bot, size_t& slot, size_t& total)
{
    Group* group = bot->GetGroup();
    ThorimEncounterState const* state = FindState(bot);
    if (!group || !state)
        return false;

    std::vector<Player*> ringMembers;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member->GetMapId() != bot->GetMapId())
            continue;

        auto const squad = state->squads.find(member->GetGUID());
        if (squad == state->squads.end() || static_cast<ThorimSquad>(squad->second) != ThorimSquad::Arena)
            continue;

        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        if (!memberAI || !PlayerbotAI::IsRanged(member) || PlayerbotAI::IsTank(member))
            continue;

        ringMembers.push_back(member);
    }

    if (ringMembers.empty())
        return false;

    std::sort(ringMembers.begin(), ringMembers.end(), [](Player* left, Player* right)
    {
        bool const leftDps = PlayerbotAI::IsRangedDps(left);
        bool const rightDps = PlayerbotAI::IsRangedDps(right);
        if (leftDps != rightDps)
            return leftDps;
        return left->GetGUID() < right->GetGUID();
    });

    for (size_t i = 0; i < ringMembers.size(); ++i)
        if (ringMembers[i] == bot)
        {
            slot = i;
            total = ringMembers.size();
            return true;
        }

    return false;
}

bool ThorimSettleArenaPoint(Player* bot, Position& spot)
{
    Position const& centre = ULDUAR_THORIM_NEAR_ARENA_CENTER;

    float x = spot.GetPositionX();
    float y = spot.GetPositionY();
    float z = bot->GetMapWaterOrGroundLevel(x, y, centre.GetPositionZ());
    if (z <= INVALID_HEIGHT)
        z = centre.GetPositionZ();

    bot->GetMap()->CheckCollisionAndGetValidCoords(bot, bot->GetPositionX(), bot->GetPositionY(),
                                                   bot->GetPositionZ(), x, y, z, false);

    spot = Position(x, y, z);
    return spot.GetExactDist2d(centre.GetPositionX(), centre.GetPositionY()) <=
           ULDUAR_THORIM_ARENA_LEASH_RADIUS;
}

bool GetThorimArenaRingSlot(Player* bot, Position& out)
{
    size_t slot = 0;
    size_t total = 0;
    if (!ThorimArenaRingOrder(bot, slot, total))
        return false;

    Position const& centre = ULDUAR_THORIM_NEAR_ARENA_CENTER;
    Position const& gate = ULDUAR_THORIM_NEAR_ENTRANCE_POSITION;
    float const baseAngle = std::atan2(centre.GetPositionY() - gate.GetPositionY(),
                                       centre.GetPositionX() - gate.GetPositionX());

    size_t const inner = std::min<size_t>(ULDUAR_THORIM_ARENA_RING_INNER_SLOTS, total);

    float radius = ULDUAR_THORIM_ARENA_RING_INNER;
    float angle = baseAngle + 2.0f * static_cast<float>(M_PI) * static_cast<float>(slot) / static_cast<float>(inner);

    if (slot >= inner)
    {
        size_t const outerCount = total - inner;
        radius = ULDUAR_THORIM_ARENA_RING_OUTER;
        angle = baseAngle +
                2.0f * static_cast<float>(M_PI) * static_cast<float>(slot - inner) / static_cast<float>(outerCount);
    }

    angle = Position::NormalizeOrientation(angle);

    Position spot(centre.GetPositionX() + std::cos(angle) * radius,
                  centre.GetPositionY() + std::sin(angle) * radius, centre.GetPositionZ());

    out = ThorimSettleArenaPoint(bot, spot) ? spot : centre;
    return true;
}

bool ThorimPushOutOfOrbField(PlayerbotAI* botAI, Player* bot, Position const& from, Position& out)
{
    Unit* orb = ThorimChargedThunderOrb(botAI, SPELL_THORIM_CHARGE_ORB);
    if (!orb)
        return false;

    float const danger = ULDUAR_THORIM_CHARGED_ORB_RADIUS + ULDUAR_THORIM_CHARGED_ORB_MARGIN;
    float const gap = from.GetExactDist2d(orb->GetPositionX(), orb->GetPositionY());
    if (gap > danger)
        return false;

    Position const& centre = ULDUAR_THORIM_NEAR_ARENA_CENTER;
    float bearing = std::atan2(centre.GetPositionY() - orb->GetPositionY(),
                               centre.GetPositionX() - orb->GetPositionX());
    if (gap > 1.0f)
        bearing = std::atan2(from.GetPositionY() - orb->GetPositionY(),
                             from.GetPositionX() - orb->GetPositionX());

    Position spot(orb->GetPositionX() + std::cos(bearing) * danger,
                  orb->GetPositionY() + std::sin(bearing) * danger, centre.GetPositionZ());

    bool safe = ThorimSettleArenaPoint(bot, spot);
    if (safe && spot.GetExactDist2d(orb->GetPositionX(), orb->GetPositionY()) < ULDUAR_THORIM_CHARGED_ORB_RADIUS)
        safe = false;

    out = safe ? spot : centre;
    return true;
}

}

bool GetThorimArenaAnchor(PlayerbotAI* botAI, Player* bot, Position& out)
{
    if (!botAI || !bot)
        return false;

    if (GetThorimSquad(botAI, bot) != ThorimSquad::Arena)
        return false;

    Unit* boss = GetThorim(botAI);
    if (!boss || !boss->IsAlive() || !boss->IsHostileTo(bot))
        return false;

    if (boss->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
        return false;

    if (!boss->IsInCombat())
        return false;

    if (PlayerbotAI::IsTank(bot))
    {
        out = ULDUAR_THORIM_NEAR_ARENA_CENTER;
    }
    else if (PlayerbotAI::IsRanged(bot))
    {
        if (!GetThorimArenaRingSlot(bot, out))
            return false;
    }
    else if (botAI->GetState() == BOT_STATE_COMBAT)
    {
        return false;
    }
    else
    {
        out = ULDUAR_THORIM_NEAR_ARENA_CENTER;
    }

    Position moved;
    if (ThorimPushOutOfOrbField(botAI, bot, out, moved))
        out = moved;

    return true;
}

bool ThorimArenaAnchorNeedsMove(PlayerbotAI* /*botAI*/, Player* bot, Position const& spot)
{
    if (!bot)
        return false;

    ThorimEncounterState& state = ThorimStateFor(bot);
    float const distance = bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY());

    bool const spotIsLeashed =
        spot.GetExactDist2d(ULDUAR_THORIM_NEAR_ARENA_CENTER.GetPositionX(),
                            ULDUAR_THORIM_NEAR_ARENA_CENTER.GetPositionY()) <=
        ULDUAR_THORIM_ARENA_LEASH_RADIUS;

    if (state.arenaAnchorArrived.count(bot->GetGUID()))
    {
        if (spotIsLeashed && distance <= ULDUAR_THORIM_RING_REPOSITION_TOLERANCE)
            return false;

        state.arenaAnchorArrived.erase(bot->GetGUID());
        return true;
    }

    if (!spotIsLeashed || distance > ULDUAR_THORIM_RING_ARRIVE_TOLERANCE)
        return true;

    state.arenaAnchorArrived.insert(bot->GetGUID());
    return false;
}

bool ThorimArenaAnchorSettled(PlayerbotAI* botAI, Player* bot)
{
    if (!bot)
        return false;

    Position anchor;
    if (!GetThorimArenaAnchor(botAI, bot, anchor))
        return false;

    ThorimEncounterState const* state = FindState(bot);
    return state && state->arenaAnchorArrived.count(bot->GetGUID()) > 0;
}

bool ThorimChargedOrbEscape(PlayerbotAI* botAI, Player* bot, Position& out)
{
    if (!botAI || !bot || GetThorimSquad(botAI, bot) != ThorimSquad::Arena)
        return false;

    Unit* boss = GetThorim(botAI);
    if (!boss || !boss->IsAlive() || boss->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
        return false;

    Position const here(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
    return ThorimPushOutOfOrbField(botAI, bot, here, out);
}

void ThorimNoteOrbEscape(Player* bot, Position const& spot)
{
    if (!bot)
        return;

    ThorimEncounterState& state = ThorimStateFor(bot);
    auto const noted = state.orbEscapes.find(bot->GetGUID());
    if (noted != state.orbEscapes.end() &&
        noted->second.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) <= 1.0f)
        return;

    state.orbEscapes[bot->GetGUID()] = spot;
}

void ThorimNoteFollowMasterStripped(Player* bot)
{
    if (!bot)
        return;

    ThorimStateFor(bot).followMasterStripped.insert(bot->GetGUID());
}

bool ThorimFollowMasterStripped(Player const* bot)
{
    ThorimEncounterState const* state = FindState(bot);
    return state && state->followMasterStripped.count(bot->GetGUID()) > 0;
}

bool ThorimPhase2Active(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!NearThorimEncounter(bot))
        return false;

    Unit* boss = GetThorim(botAI);
    if (!boss || !boss->IsInWorld() || boss->IsDuringRemoveFromWorld() || !boss->IsHostileTo(bot))
        return false;

    return boss->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD;
}

bool ThorimPhase2ElapsedMs(PlayerbotAI* botAI, uint32& elapsedMs)
{
    if (!ThorimPhase2Active(botAI))
        return false;

    Unit* boss = GetThorim(botAI);
    if (!boss || !boss->IsInCombat())
        return false;

    ThorimEncounterState& state = ThorimStateFor(botAI->GetBot());
    if (!state.phase2StartMs.Get())
        state.phase2StartMs = std::max<uint32>(getMSTime(), 1);

    elapsedMs = GetMSTimeDiffToNow(state.phase2StartMs.Get());
    return true;
}

bool ThorimPhase2OpeningHold(PlayerbotAI* botAI, Player* bot, Position const& holdSpot)
{
    uint32 elapsed = 0;
    if (!bot || !ThorimPhase2ElapsedMs(botAI, elapsed))
        return false;

    ThorimEncounterState& state = ThorimStateFor(bot);
    if (state.openingReleased.count(bot->GetGUID()))
        return false;

    if (elapsed < ULDUAR_THORIM_OPENING_HOLD_MIN_MS)
        return true;

    Unit* boss = GetThorim(botAI);
    bool const atAnchor =
        boss && boss->GetExactDist2d(&ULDUAR_THORIM_PHASE2_TANK_SPOT) <= ULDUAR_THORIM_OPENING_SETTLED_RADIUS;

    if (elapsed < ULDUAR_THORIM_OPENING_HOLD_MAX_MS && !atAnchor && !ThorimCampSpotInLitCone(botAI, holdSpot))
        return true;

    state.openingReleased.insert(bot->GetGUID());
    return false;
}

bool ThorimBalconyHoldSpot(PlayerbotAI* botAI, Player* bot, Position& out)
{
    if (!botAI || !bot || !TakesRangedSpot(bot) || GetThorimSquad(botAI, bot) != ThorimSquad::Gauntlet)
        return false;

    uint32 elapsed = 0;
    if (!ThorimPhase2ElapsedMs(botAI, elapsed) || elapsed >= ULDUAR_THORIM_OPENING_HOLD_MAX_MS)
        return false;

    EnsureBalconyHoldSlot(bot);

    uint8 slot = 0;
    if (!SlotOf(ThorimStateFor(bot).balconyHoldSlots, bot, slot))
        return false;

    Position const& spot = *balconyHoldSpots[std::min<uint8>(slot, ULDUAR_THORIM_BALCONY_HOLD_SLOTS - 1)];
    if (!ThorimPhase2OpeningHold(botAI, bot, spot))
        return false;

    out = spot;
    return true;
}

static bool ThorimOpeningSpot(PlayerbotAI* botAI, Player* bot, Position& out)
{
    if (ThorimEncounterState const* state = FindState(bot); !state || state->openingReleased.count(bot->GetGUID()))
        return false;

    uint32 elapsed = 0;
    if (!ThorimPhase2ElapsedMs(botAI, elapsed) || elapsed >= ULDUAR_THORIM_OPENING_HOLD_MAX_MS)
        return false;

    if (GetThorimSquad(botAI, bot) == ThorimSquad::Gauntlet)
        return false;

    EnsureOpeningSlot(bot);

    uint8 slot = 0;
    if (!SlotOf(ThorimStateFor(bot).openingSlots, bot, slot))
        return false;

    Position const& spot = *openingSpots[std::min<uint8>(slot, ULDUAR_THORIM_OPENING_SLOTS - 1)];
    if (!ThorimPhase2OpeningHold(botAI, bot, spot))
        return false;

    out = spot;
    return true;
}

static char const* ThorimRoleName(ThorimPhase2Role role)
{
    switch (role)
    {
        case ThorimPhase2Role::MainTank:
            return "maintank";
        case ThorimPhase2Role::OffTank:
            return "offtank";
        case ThorimPhase2Role::Ranged:
            return "ranged";
        case ThorimPhase2Role::MeleeRing:
            return "melee";
        default:
            return "none";
    }
}

static ThorimPhase2Role ResolveThorimPhase2Role(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot)
        return ThorimPhase2Role::None;

    if (PlayerbotAI::IsTank(bot))
        return IsThorimAnchorTank(bot) ? ThorimPhase2Role::MainTank : ThorimPhase2Role::OffTank;

    if (TakesRangedSpot(bot))
        return ThorimPhase2Role::Ranged;

    return ThorimPhase2Role::MeleeRing;
}

ThorimPhase2Role GetThorimPhase2Role(PlayerbotAI* botAI, Player* bot)
{
    ThorimPhase2Role const role = ResolveThorimPhase2Role(botAI, bot);

    RaidObs::NoteDerived(bot, "thorim.p2role", ThorimRoleName(role));
    return role;
}

namespace
{

bool ThorimTankCanAnchor(Player* member)
{
    return member && GET_PLAYERBOT_AI(member) && member->IsAlive() && member->GetMapId() == ULDUAR_MAP_ID &&
           PlayerbotAI::IsTank(member) && member->GetPositionZ() <= ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD;
}

Player* ElectThorimAnchorTank(PlayerbotAI* botAI, Group* group, ObjectGuid held)
{
    Unit* boss = GetThorim(botAI);
    Player* holdsBoss = nullptr;
    Player* lowest = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!ThorimTankCanAnchor(member))
            continue;

        if (member->GetGUID() == held)
            return member;

        if (boss && boss->GetVictim() == member)
            holdsBoss = member;

        if (!lowest || member->GetGUID() < lowest->GetGUID())
            lowest = member;
    }

    return holdsBoss ? holdsBoss : lowest;
}

}

void ThorimTickAnchorTank(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !NearThorimEncounter(bot))
        return;

    ThorimEncounterState& state = ThorimStateFor(bot);

    if (!ThorimPhase2Active(botAI))
    {
        state.anchorTank = ObjectGuid::Empty;
        state.anchorTankScanMs = 0;
        return;
    }

    Group* group = bot->GetGroup();
    if (!group)
        return;

    uint32 const now = getMSTime();
    if (state.anchorTankScanMs &&
        getMSTimeDiff(state.anchorTankScanMs, now) < ULDUAR_THORIM_ENCOUNTER_SCAN_INTERVAL_MS)
        return;

    state.anchorTankScanMs = now;

    Player* anchor = ElectThorimAnchorTank(botAI, group, state.anchorTank.Get());
    state.anchorTank = anchor ? anchor->GetGUID() : ObjectGuid::Empty;
}

bool IsThorimAnchorTank(Player const* bot)
{
    ThorimEncounterState const* state = FindState(bot);
    return bot && state && state->anchorTank.Get() == bot->GetGUID();
}

bool TryGetThorimPhase2Spot(PlayerbotAI* botAI, Player* bot, ThorimPhase2Role role, Position& position)
{
    if (!botAI || !bot)
        return false;

    if (bot->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
        return false;

    Unit* boss = GetThorim(botAI);

    if (role == ThorimPhase2Role::Ranged)
    {
        EnsureRangedSlot(bot);

        uint8 slot = 0;
        if (!RangedSlotOf(bot, slot))
            return false;

        position = RangedSpot(slot);

        if (!boss)
            return true;

        Position opening;
        if (ThorimOpeningSpot(botAI, bot, opening))
        {
            position = opening;
            return true;
        }

        uint8 orbIndex = 0;
        bool stepped = false;
        bool const sheltered = ThorimRangedSpot(botAI, bot, boss, slot, position, orbIndex, stepped);

        RaidObs::NoteDerived(bot, "thorim.shelter",
                             "slot " + std::to_string(uint32(slot)) +
                                 (sheltered ? " orb " + std::to_string(uint32(orbIndex)) + (stepped ? " step" : "")
                                            : " home"));
        return true;
    }

    if (role == ThorimPhase2Role::OffTank || role == ThorimPhase2Role::MainTank)
    {
        Unit* const victim = boss ? boss->GetVictim() : nullptr;
        Player* const holder = victim ? victim->ToPlayer() : nullptr;
        bool const tankHolds = holder && PlayerbotAI::IsTank(holder);
        if (role == ThorimPhase2Role::MainTank && boss && victim != bot && !tankHolds && !ClassTauntReady(botAI) &&
            !bot->HasAura(SPELL_UNBALANCING_STRIKE))
            return false;

        position = (victim == bot || (role == ThorimPhase2Role::MainTank && !tankHolds))
                       ? ULDUAR_THORIM_PHASE2_TANK_SPOT
                       : ULDUAR_THORIM_PHASE2_OFFTANK_SPOT;
        return true;
    }

    if (!boss)
        return false;

    if (role != ThorimPhase2Role::MeleeRing)
        return false;

    EnsureMeleeSlot(bot);

    uint8 slot = 0;
    if (!MeleeSlotOf(bot, slot))
        return false;

    float const bearing = RingSlotBearing(botAI, bot, boss, slot);

    ThorimEncounterState const* ringState = FindState(bot);
    float const latched = ringState ? ringState->ringSlots[slot].latched : bearing;
    RaidObs::NoteDerived(bot, "thorim.ringspot",
                         "slot " + std::to_string(uint32(slot)) + " bearing " +
                             std::to_string(int32(latched * 180.0f / float(M_PI))) + " solved " +
                             std::to_string(int32(bearing * 180.0f / float(M_PI))));

    if (RingPoint(bot, boss, bearing, position))
        return true;

    return StaticMeleeSpot(boss, slot, position);
}

bool ThorimTankSpotNeedsMove(PlayerbotAI* botAI, Player* bot, Position const& spot)
{
    Unit* const boss = GetThorim(botAI);
    if (boss && boss->GetVictim() == bot)
        return bot->GetExactDist2d(&spot) > ULDUAR_THORIM_PHASE2_HOLDER_TOLERANCE;

    return bot->GetDistance(spot) > ULDUAR_THORIM_PHASE2_SPOT_TOLERANCE;
}

bool ThorimAtTankSpots(Player const* bot)
{
    return bot && bot->GetExactDist2d(&ULDUAR_THORIM_PHASE2_TANK_SPOT) <= ULDUAR_THORIM_SWAP_TANK_SPOT_RADIUS;
}

namespace
{

uint8 ThorimPetRingSlot(PlayerbotAI* botAI, Player* bot)
{
    uint8 slot = 0;
    if (GetThorimPhase2Role(botAI, bot) == ThorimPhase2Role::MeleeRing && MeleeSlotOf(bot, slot))
        return slot;

    return static_cast<uint8>(bot->GetGUID().GetCounter() % ULDUAR_THORIM_MELEE_SLOTS);
}

}

bool ThorimPetsOffRingSlot(PlayerbotAI* botAI, Player* bot, std::vector<Unit*>& out)
{
    out.clear();

    if (!botAI || !bot || !ThorimPhase2Active(botAI))
        return false;

    Unit* boss = GetThorim(botAI);
    ThorimEncounterState const* state = FindState(bot);
    if (!boss || !state || !state->ringLatched)
        return false;

    float const bearing = state->ringSlots[ThorimPetRingSlot(botAI, bot)].composed;

    for (Unit* pet : bot->m_Controlled)
    {
        if (!pet || !pet->IsAlive() || pet->IsTotem())
            continue;

        if (!pet->IsPet() && !pet->IsGuardian())
            continue;

        if (pet->GetVictim() != boss)
            continue;

        if (AbsAngleDelta(BearingFromBoss(boss, pet->GetPositionX(), pet->GetPositionY()), bearing) <=
            ULDUAR_THORIM_PET_SLOT_TOLERANCE)
            continue;

        auto const sent = state->petRecallMs.find(pet->GetGUID());
        if (sent != state->petRecallMs.end() &&
            GetMSTimeDiffToNow(sent->second) < ULDUAR_THORIM_PET_RECALL_INTERVAL_MS)
            continue;

        out.push_back(pet);
    }

    return !out.empty();
}

void ThorimSendPetToRingSlot(PlayerbotAI* botAI, Player* bot, Unit* pet)
{
    if (!botAI || !bot || !pet)
        return;

    Unit* boss = GetThorim(botAI);
    ThorimEncounterState* state = FindState(bot);
    if (!boss || !state || !state->ringLatched)
        return;

    state->petRecallMs[pet->GetGUID()] = getMSTime();

    float const relative = Position::NormalizeOrientation(
        state->ringSlots[ThorimPetRingSlot(botAI, bot)].composed - boss->GetOrientation());

    pet->GetMotionMaster()->MoveChase(boss, ChaseRange(0.0f, 0.0f),
                                      ChaseAngle(relative, ULDUAR_THORIM_PET_SLOT_TOLERANCE));
}

static bool RingWalkCrossesLitCone(PlayerbotAI* botAI, Player* bot, Position const& spot)
{
    Unit* boss = GetThorim(botAI);
    Unit* orb = boss ? ThorimChargedThunderOrb(botAI, SPELL_THORIM_LIGHTNING_ORB_VISUAL) : nullptr;
    if (!orb)
        return false;

    float const coneBearing = BearingFromBoss(boss, orb->GetPositionX(), orb->GetPositionY());
    float const bearing = BearingFromBoss(boss, bot->GetPositionX(), bot->GetPositionY());
    float const stepEdge = ULDUAR_THORIM_LIGHTNING_CHARGE_CONE_ANGLE / 2.0f + ULDUAR_THORIM_CONE_STEP_MARGIN;
    if (AbsAngleDelta(bearing, coneBearing) <= stepEdge)
        return false;

    bool const insideRingLine = InLightningChargeCone(bearing, coneBearing);
    float const dx = spot.GetPositionX() - bot->GetPositionX();
    float const dy = spot.GetPositionY() - bot->GetPositionY();
    uint32 const samples =
        static_cast<uint32>(std::sqrt(dx * dx + dy * dy) / ULDUAR_THORIM_RING_CONE_WALK_SAMPLE) + 1;
    for (uint32 i = 1; i <= samples; ++i)
    {
        float const t = static_cast<float>(i) / static_cast<float>(samples);
        float const sample = BearingFromBoss(boss, bot->GetPositionX() + dx * t, bot->GetPositionY() + dy * t);
        bool const entered = insideRingLine ? AbsAngleDelta(sample, coneBearing) <= stepEdge
                                            : InLightningChargeCone(sample, coneBearing);
        if (entered)
            return true;
    }

    return false;
}

bool ThorimRingWantsMove(PlayerbotAI* botAI, Player* bot, Position const& spot)
{
    if (!bot)
        return false;

    bool const coneHold = RingWalkCrossesLitCone(botAI, bot, spot);
    RaidObs::NoteDerived(bot, "thorim.ringconehold", coneHold ? "hold" : "none");
    if (coneHold)
        return false;

    ThorimEncounterState const* state = FindState(bot);

    float const tolerance = state && state->ringArrived.count(bot->GetGUID())
                                ? ULDUAR_THORIM_RING_REPOSITION_TOLERANCE
                                : ULDUAR_THORIM_RING_ARRIVE_TOLERANCE;

    return bot->GetDistance(spot) > tolerance || RingSpotBeatsTheDeadband(botAI, bot, spot);
}

void ThorimRingMarkArrived(Player* bot)
{
    if (bot)
        ThorimStateFor(bot).ringArrived.insert(bot->GetGUID());
}

void ThorimRingClearArrived(Player* bot)
{
    if (bot)
        ThorimStateFor(bot).ringArrived.erase(bot->GetGUID());
}

bool ThorimSpotUnderBlizzard(Player* bot, Position const& spot)
{
    if (!bot)
        return false;

    for (Position const& zone : ThorimBlizzardSpots(bot))
        if (zone.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) < ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE)
            return true;

    return false;
}

static float ThorimSegmentDistance2d(Position const& point, Position const& from, Position const& to)
{
    float const dx = to.GetPositionX() - from.GetPositionX();
    float const dy = to.GetPositionY() - from.GetPositionY();
    float const lengthSq = dx * dx + dy * dy;

    float t = 0.0f;
    if (lengthSq > 0.0f)
    {
        t = ((point.GetPositionX() - from.GetPositionX()) * dx + (point.GetPositionY() - from.GetPositionY()) * dy) /
            lengthSq;
        t = std::clamp(t, 0.0f, 1.0f);
    }

    float const nearestX = from.GetPositionX() + dx * t;
    float const nearestY = from.GetPositionY() + dy * t;
    return std::sqrt((point.GetPositionX() - nearestX) * (point.GetPositionX() - nearestX) +
                     (point.GetPositionY() - nearestY) * (point.GetPositionY() - nearestY));
}

static std::array<Position, 9> const ULDUAR_THORIM_SIF_BLIZZARD_PATH = {
    Position(2108.7f, -280.04f, 419.42f), Position(2104.6f, -268.5f, 419.4f), Position(2104.3f, -256.3f, 419.4f),
    Position(2109.3f, -246.4f, 419.4f),   Position(2117.9f, -238.6f, 419.4f), Position(2128.8f, -232.1f, 419.4f),
    Position(2151.9f, -237.5f, 419.4f),   Position(2164.9f, -256.3f, 419.4f), Position(2161.5f, -280.0f, 419.4f)};

static std::vector<std::pair<Position, Position>> ThorimBlizzardPathAhead(Player* bot)
{
    ThorimBlizzardSpots(bot);

    std::vector<std::pair<Position, Position>> legs;
    for (Position const& bunny : ThorimStateFor(bot).blizzardBunnies)
    {
        size_t from = 0;
        float nearest = std::numeric_limits<float>::max();
        for (size_t leg = 0; leg + 1 < ULDUAR_THORIM_SIF_BLIZZARD_PATH.size(); ++leg)
        {
            float const distance = ThorimSegmentDistance2d(bunny, ULDUAR_THORIM_SIF_BLIZZARD_PATH[leg],
                                                           ULDUAR_THORIM_SIF_BLIZZARD_PATH[leg + 1]);
            if (distance < nearest)
            {
                nearest = distance;
                from = leg;
            }
        }

        legs.emplace_back(bunny, ULDUAR_THORIM_SIF_BLIZZARD_PATH[from + 1]);
        for (size_t leg = from + 1; leg + 1 < ULDUAR_THORIM_SIF_BLIZZARD_PATH.size(); ++leg)
            legs.emplace_back(ULDUAR_THORIM_SIF_BLIZZARD_PATH[leg], ULDUAR_THORIM_SIF_BLIZZARD_PATH[leg + 1]);
    }

    return legs;
}

bool ThorimWalkUnderBlizzard(Player* bot, Position const& from, Position const& to)
{
    if (!bot)
        return false;

    for (Position const& zone : ThorimBlizzardSpots(bot))
        if (ThorimSegmentDistance2d(zone, from, to) < ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE)
            return true;

    return false;
}

bool ThorimCampSpotInLitCone(PlayerbotAI* botAI, Position const& spot)
{
    Unit* boss = GetThorim(botAI);
    if (!boss)
        return false;

    Unit* orb = ThorimChargedThunderOrb(botAI, SPELL_THORIM_LIGHTNING_ORB_VISUAL);
    if (!orb)
        return false;

    return InLightningChargeConeRanged(BearingFromBoss(boss, spot.GetPositionX(), spot.GetPositionY()),
                                       BearingFromBoss(boss, orb->GetPositionX(), orb->GetPositionY()));
}

bool ThorimCampBlizzardEscape(PlayerbotAI* botAI, Player* bot, Position& out)
{
    if (!botAI || !bot)
        return false;

    Unit* boss = GetThorim(botAI);
    if (!boss)
        return false;

    std::vector<Position> const zones = ThorimBlizzardSpots(bot);
    if (zones.empty())
        return false;

    std::vector<HazardCircle> circles;
    circles.reserve(zones.size());
    for (Position const& zone : zones)
        circles.emplace_back(zone, ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE);

    Position spot;
    if (!TryGetThorimPhase2Spot(botAI, bot, ThorimPhase2Role::Ranged, spot))
        spot = bot->GetPosition();

    Unit* orb = ThorimChargedThunderOrb(botAI, SPELL_THORIM_LIGHTNING_ORB_VISUAL);
    float const coneBearing = orb ? BearingFromBoss(boss, orb->GetPositionX(), orb->GetPositionY()) : 0.0f;

    auto const accept = [boss, orb, coneBearing](float x, float y)
    {
        if (boss->GetExactDist2d(x, y) > ULDUAR_THORIM_CAMP_MAX_BOSS_RANGE)
            return false;

        return !orb || !InLightningChargeConeRanged(BearingFromBoss(boss, x, y), coneBearing);
    };

    Position const clear = FindNearestPositionClearOfHazards(bot, circles, ULDUAR_THORIM_CAMP_BLIZZARD_ESCAPE_RADIUS,
                                                             ULDUAR_THORIM_SWEEP_STEP, ULDUAR_THORIM_SWEEP_ANGLE_STEP,
                                                             &spot, accept);
    if (clear == Position())
        return false;

    out = clear;
    return true;
}

bool ThorimMeleeRingSettled(PlayerbotAI* botAI, Player* bot)
{
    if (!bot || GetThorimPhase2Role(botAI, bot) != ThorimPhase2Role::MeleeRing)
        return false;

    if (!ThorimPhase2Active(botAI))
        return false;

    if (bot->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
        return false;

    ThorimEncounterState const* state = FindState(bot);
    return state && (state->ringArrived.count(bot->GetGUID()) || state->ringStepped.count(bot->GetGUID()));
}

bool ThorimRingMeleeOnFloor(PlayerbotAI* botAI, Player* bot)
{
    return bot && bot->GetPositionZ() <= ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD && ThorimPhase2Active(botAI) &&
           GetThorimPhase2Role(botAI, bot) == ThorimPhase2Role::MeleeRing;
}

bool ThorimRingBlizzardCovered(Player* bot, Position const& ringPoint)
{
    if (!bot)
        return false;

    bool const covered = ThorimSpotUnderBlizzard(bot, ringPoint) ||
                         (bot->GetExactDist2d(&ringPoint) > ULDUAR_THORIM_RING_ARRIVE_TOLERANCE &&
                          !ThorimSpotUnderBlizzard(bot, *bot) && ThorimWalkUnderBlizzard(bot, *bot, ringPoint));

    ThorimEncounterState& state = ThorimStateFor(bot);
    if (covered != (state.ringStepped.count(bot->GetGUID()) > 0))
    {
        if (covered)
            state.ringStepped.insert(bot->GetGUID());
        else
            state.ringStepped.erase(bot->GetGUID());

        state.ringArrived.erase(bot->GetGUID());
    }

    return covered;
}

bool ThorimRingBlizzardStep(PlayerbotAI* botAI, Player* bot, Position& out)
{
    if (!ThorimRingMeleeOnFloor(botAI, bot))
        return false;

    Unit* boss = GetThorim(botAI);
    if (!boss)
        return false;

    Unit* orb = ThorimChargedThunderOrb(botAI, SPELL_THORIM_LIGHTNING_ORB_VISUAL);
    float const coneBearing = orb ? BearingFromBoss(boss, orb->GetPositionX(), orb->GetPositionY()) : 0.0f;
    auto const outOfCone = [boss, orb, coneBearing](float x, float y)
    { return !orb || !InLightningChargeCone(BearingFromBoss(boss, x, y), coneBearing); };

    if (!ThorimSpotUnderBlizzard(bot, *bot) && outOfCone(bot->GetPositionX(), bot->GetPositionY()))
        return false;

    Position ring;
    if (!TryGetThorimPhase2Spot(botAI, bot, ThorimPhase2Role::MeleeRing, ring) || !ThorimRingBlizzardCovered(bot, ring))
        return false;

    std::vector<Position> const zones = ThorimBlizzardSpots(bot);
    std::vector<HazardCircle> circles;
    circles.reserve(zones.size());
    for (Position const& zone : zones)
        circles.emplace_back(zone, ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE);

    float const tankClearance = ULDUAR_THORIM_CHAIN_LIGHTNING_JUMP + 2.0f * DEFAULT_COMBAT_REACH;
    std::vector<HazardCircle> otherSlots;
    std::vector<HazardCircle> camp;
    RingStepChainBodies(bot, boss, otherSlots, camp);
    std::vector<std::pair<Position, Position>> const pathAhead = ThorimBlizzardPathAhead(bot);

    struct StepPass
    {
        float radius;
        bool pastRing;
        bool clearOfTanks;
        bool clearOfSlots;
        bool clearOfCamp;
        bool clearOfPath;
    };

    auto const clearOf = [](std::vector<HazardCircle> const& bodies, float x, float y)
    {
        for (HazardCircle const& body : bodies)
            if (body.first.GetExactDist2d(x, y) <= body.second)
                return false;

        return true;
    };

    auto const clearOfPath = [&pathAhead](float x, float y)
    {
        Position const spot(x, y, 0.0f);
        for (std::pair<Position, Position> const& leg : pathAhead)
            if (ThorimSegmentDistance2d(spot, leg.first, leg.second) < ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE)
                return false;

        return true;
    };

    float const stepRadius = ULDUAR_THORIM_RING_BLIZZARD_STEP_RADIUS;
    float const lastRadius = ULDUAR_THORIM_RING_BLIZZARD_LAST_STEP_RADIUS;
    HazardSweepCache cache;
    for (StepPass const pass : {StepPass{lastRadius, true, true, true, true, true},
                                StepPass{lastRadius, true, true, true, false, true},
                                StepPass{lastRadius, true, true, false, false, true},
                                StepPass{lastRadius, true, false, false, false, true},
                                StepPass{stepRadius, true, true, true, true, false},
                                StepPass{stepRadius, true, true, true, false, false},
                                StepPass{stepRadius, true, true, false, false, false},
                                StepPass{stepRadius, true, false, false, false, false},
                                StepPass{lastRadius, false, false, false, false, false}})
    {
        if ((pass.clearOfCamp && camp.empty()) || (pass.clearOfSlots && !pass.clearOfCamp && otherSlots.empty()) ||
            (pass.clearOfPath && pathAhead.empty()))
            continue;

        auto const accept = [boss, pass, tankClearance, &outOfCone, &clearOf, &clearOfPath, &otherSlots,
                             &camp](float x, float y)
        {
            if (x < ULDUAR_THORIM_ARENA_BOX_MIN_X || x > ULDUAR_THORIM_ARENA_BOX_MAX_X ||
                y < ULDUAR_THORIM_ARENA_BOX_MIN_Y || y > ULDUAR_THORIM_ARENA_BOX_MAX_Y || !outOfCone(x, y))
                return false;

            if (pass.pastRing && boss->GetExactDist2d(x, y) < ULDUAR_THORIM_MELEE_RING_RADIUS)
                return false;

            if ((pass.clearOfSlots && !clearOf(otherSlots, x, y)) || (pass.clearOfCamp && !clearOf(camp, x, y)) ||
                (pass.clearOfPath && !clearOfPath(x, y)))
                return false;

            return !pass.clearOfTanks ||
                   (ULDUAR_THORIM_PHASE2_TANK_SPOT.GetExactDist2d(x, y) >= tankClearance &&
                    ULDUAR_THORIM_PHASE2_OFFTANK_SPOT.GetExactDist2d(x, y) >= tankClearance);
        };

        Position const spot = FindNearestPositionClearOfHazards(bot, circles, pass.radius, ULDUAR_THORIM_SWEEP_STEP,
                                                                ULDUAR_THORIM_SWEEP_ANGLE_STEP, &ring, accept, &cache);
        if (spot != Position())
        {
            out = spot;
            return true;
        }
    }

    if (!orb)
        return false;

    float const halfWidth = ULDUAR_THORIM_LIGHTNING_CHARGE_CONE_ANGLE / 2.0f + ULDUAR_THORIM_CONE_STEP_MARGIN;
    auto const clearOfLitCone = [boss, coneBearing, halfWidth](float x, float y)
    { return AbsAngleDelta(BearingFromBoss(boss, x, y), coneBearing) > halfWidth; };

    if (clearOfLitCone(bot->GetPositionX(), bot->GetPositionY()))
        return false;

    auto const coneExit = [&clearOfLitCone](float x, float y)
    {
        return x >= ULDUAR_THORIM_ARENA_BOX_MIN_X && x <= ULDUAR_THORIM_ARENA_BOX_MAX_X &&
               y >= ULDUAR_THORIM_ARENA_BOX_MIN_Y && y <= ULDUAR_THORIM_ARENA_BOX_MAX_Y && clearOfLitCone(x, y);
    };

    for (float const zoneClearance : {ULDUAR_THORIM_BLIZZARD_ZONE_REACH, 0.0f})
    {
        for (HazardCircle& circle : circles)
            circle.second = zoneClearance;

        Position const spot = FindNearestPositionClearOfHazards(bot, circles, lastRadius, ULDUAR_THORIM_SWEEP_STEP,
                                                                ULDUAR_THORIM_SWEEP_ANGLE_STEP, &ring, coneExit, &cache);
        if (spot != Position())
        {
            out = spot;
            return true;
        }
    }

    return false;
}

bool ThorimRingSteppedOut(PlayerbotAI* botAI, Player* bot)
{
    ThorimEncounterState const* state = bot ? FindState(bot) : nullptr;
    return state && state->ringStepped.count(bot->GetGUID()) &&
           bot->GetPositionZ() <= ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD && ThorimPhase2Active(botAI);
}

bool GetThorimLightningChargeHit(PlayerbotAI* botAI, RaidPredictedHit& hit)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !bot->IsAlive() || PlayerbotAI::IsTank(bot))
        return false;

    ThorimEncounterState const* state = FindState(bot);
    if (!state || !state->lightningOrbLitMs)
        return false;

    uint32 const startMs = state->lightningOrbLitMs + ULDUAR_THORIM_LIGHTNING_CHARGE_WARNING_MS -
                           ULDUAR_THORIM_ENCOUNTER_SCAN_INTERVAL_MS;

    // A start already past wraps getMSTimeDiff to a huge value, so a stale latch fails closed here.
    if (getMSTimeDiff(getMSTime(), startMs) > RAID_PERSONAL_LEAD_MS)
        return false;

    if (!ThorimPhase2Active(botAI))
        return false;

    Unit* boss = GetThorim(botAI);
    Unit* orb = ThorimChargedThunderOrb(botAI, SPELL_THORIM_LIGHTNING_ORB_VISUAL);
    if (!boss || !orb)
        return false;

    float const coneBearing = BearingFromBoss(boss, orb->GetPositionX(), orb->GetPositionY());
    if (!InLightningChargeConeRanged(BearingFromBoss(boss, bot->GetPositionX(), bot->GetPositionY()),
                                     coneBearing))
        return false;

    hit.startMs = startMs;
    hit.damage = ULDUAR_THORIM_LIGHTNING_CHARGE_DAMAGE;
    hit.school = SPELL_SCHOOL_MASK_NATURE;
    return true;
}

bool ThorimTankOnAnchor(PlayerbotAI* botAI, Player* bot)
{
    if (!bot || !ThorimPhase2Active(botAI))
        return false;

    ThorimPhase2Role const role = GetThorimPhase2Role(botAI, bot);
    if (role != ThorimPhase2Role::MainTank && role != ThorimPhase2Role::OffTank)
        return false;

    if (bot->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
        return false;

    Position spot;
    return TryGetThorimPhase2Spot(botAI, bot, role, spot) &&
           bot->GetDistance(spot) <= ULDUAR_THORIM_RING_ARRIVE_TOLERANCE;
}

Unit* ThorimUnreachableBy(PlayerbotAI* botAI, Player* bot)
{
    if (!bot || !ThorimPhase2Active(botAI))
        return nullptr;

    Unit* boss = GetThorim(botAI);
    Creature* thorim = boss && boss->IsAlive() && boss->IsInCombat() ? boss->ToCreature() : nullptr;
    if (!thorim)
        return nullptr;

    ObjectGuid const flagged = thorim->GetCannotReachTarget();
    ThorimEncounterState& state = ThorimStateFor(bot);
    if (flagged != state.unreachable.Get())
        state.unreachableSinceMs = getMSTime();
    state.unreachable = flagged;

    if (!flagged || flagged != bot->GetGUID() || bot->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
        return nullptr;

    if (GetMSTimeDiffToNow(state.unreachableSinceMs) < ULDUAR_THORIM_UNREACH_GRACE_MS)
        return nullptr;

    return thorim;
}

bool ThorimUnreachableStep(Player* bot, Unit* boss, Position& out)
{
    if (!bot || !boss)
        return false;

    float const step = boss->GetCombatReach() + bot->GetCombatReach() + ULDUAR_THORIM_UNREACH_STEP_MARGIN;
    float const clear = step - ULDUAR_THORIM_UNREACH_ARRIVED;
    if (boss->GetExactDist2d(bot) >= clear)
        return false;

    float const away = boss->GetAngle(bot);
    float const turn = static_cast<float>(M_PI) / 6.0f;
    Position const& centre = ULDUAR_THORIM_NEAR_ARENA_CENTER;
    bool found = false;
    for (int const k : {0, 1, -1, 2, -2, 3, -3, 4, -4})
    {
        float const bearing = away + turn * static_cast<float>(k);
        float x = boss->GetPositionX() + step * std::cos(bearing);
        float y = boss->GetPositionY() + step * std::sin(bearing);
        if (centre.GetExactDist2d(x, y) > ULDUAR_THORIM_UNREACH_ARENA_RADIUS)
            continue;

        float z = bot->GetMapWaterOrGroundLevel(x, y, bot->GetPositionZ());
        if (z <= INVALID_HEIGHT)
            z = bot->GetPositionZ();

        if (!bot->GetMap()->CheckCollisionAndGetValidCoords(bot, bot->GetPositionX(), bot->GetPositionY(),
                                                            bot->GetPositionZ(), x, y, z, false) ||
            boss->GetExactDist2d(x, y) < clear)
            continue;

        Position const spot(x, y, z);
        if (!found)
        {
            out = spot;
            found = true;
        }

        if (!ThorimSpotUnderBlizzard(bot, spot))
        {
            out = spot;
            return true;
        }
    }

    return found;
}

Unit* ThorimChargedThunderOrb(PlayerbotAI* botAI, uint32 markerSpell)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot)
        return nullptr;

    ThorimEncounterState& state = ThorimStateFor(bot);
    bool const lightning = markerSpell == SPELL_THORIM_LIGHTNING_ORB_VISUAL;
    RaidObs::ObsValue<ObjectGuid>& cachedGuid = lightning ? state.lightningOrbGuid : state.chargedOrbGuid;
    uint32& scanMs = lightning ? state.lightningOrbScanMs : state.orbScanMs;

    if (scanMs && GetMSTimeDiffToNow(scanMs) < ULDUAR_THORIM_ENCOUNTER_SCAN_INTERVAL_MS)
    {
        Unit* cached = botAI->GetUnit(cachedGuid);
        return cached && cached->HasAura(markerSpell) ? cached : nullptr;
    }

    scanMs = getMSTime();

    Unit* found = nullptr;
    std::list<Creature*> orbs;
    bot->GetCreatureListWithEntryInGrid(orbs, NPC_THORIM_THUNDER_ORB, ULDUAR_THORIM_LIGHTNING_CHARGE_RANGE);
    for (Creature* orb : orbs)
    {
        if (orb && orb->HasAura(markerSpell))
        {
            found = orb;
            break;
        }
    }

    if (lightning)
    {
        if (found && cachedGuid.Get().IsEmpty())
            state.lightningOrbLitMs = getMSTime();
        else if (!found)
            state.lightningOrbLitMs = 0;
    }

    cachedGuid = found ? found->GetGUID() : ObjectGuid::Empty;
    return found;
}

bool ThorimEncounterStateIsStale(PlayerbotAI* botAI)
{
    if (!NearThorimEncounter(botAI ? botAI->GetBot() : nullptr))
        return false;

    Unit* boss = GetThorim(botAI);
    if (!boss)
        return false;

    ThorimEncounterState* state = FindState(botAI->GetBot());
    if (!state)
        return false;

    if (boss->GetHealth() < boss->GetMaxHealth() || boss->IsInCombat())
    {
        state->engagedSeen = true;

        state->resetDone.clear();

        if (boss->GetPositionZ() >= ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
            state->phase2StartMs = 0;

        return false;
    }

    if (!state->engagedSeen)
        return false;

    return state->resetDone.count(botAI->GetBot()->GetGUID()) == 0;
}

bool ThorimBotHasEncounterState(Player* bot)
{
    ThorimEncounterState const* state = FindState(bot);
    if (!state)
        return false;

    // Has to cover everything the reset below drops, or a bot holding only the missing latch keeps it next pull.
    return state->meleeSlots.count(bot->GetGUID()) || state->ringArrived.count(bot->GetGUID()) ||
           state->ringStepped.count(bot->GetGUID()) || state->barrierBailing.count(bot->GetGUID()) ||
           state->squads.count(bot->GetGUID()) || state->followMasterStripped.count(bot->GetGUID()) ||
           state->arenaAnchorArrived.count(bot->GetGUID()) || state->balconyStep.count(bot->GetGUID()) ||
           state->orbEscapes.count(bot->GetGUID()) || state->dpsTargets.count(bot->GetGUID()) ||
           state->petRecalls.count(bot->GetGUID()) || state->openingSlots.count(bot->GetGUID()) ||
           state->balconyHoldSlots.count(bot->GetGUID()) || state->openingReleased.count(bot->GetGUID()) ||
           state->runicSmashSide;
}

void ResetThorimEncounterState(Player* bot, bool clearInstance)
{
    if (!bot)
        return;

    if (clearInstance)
    {
        thorimStates.Reset(bot->GetInstanceId());
        return;
    }

    ThorimEncounterState* state = FindState(bot);
    if (!state)
        return;

    state->meleeSlots.erase(bot->GetGUID());
    state->ringArrived.erase(bot->GetGUID());
    state->ringStepped.erase(bot->GetGUID());
    state->barrierBailing.erase(bot->GetGUID());
    state->followMasterStripped.erase(bot->GetGUID());
    state->arenaAnchorArrived.erase(bot->GetGUID());
    state->orbEscapes.erase(bot->GetGUID());
    state->petRecalls.erase(bot->GetGUID());
    state->balconyStep.erase(bot->GetGUID());
    state->dpsTargets.erase(bot->GetGUID());
    state->rangedSlots.erase(bot->GetGUID());
    state->rangedShelters.erase(bot->GetGUID());
    state->openingSlots.erase(bot->GetGUID());
    state->balconyHoldSlots.erase(bot->GetGUID());
    state->openingReleased.erase(bot->GetGUID());

    for (Unit* pet : bot->m_Controlled)
        if (pet)
            state->petRecallMs.erase(pet->GetGUID());

    bool const firstThisCycle = state->resetDone.empty();
    state->resetDone.insert(bot->GetGUID());
    if (!firstThisCycle)
        return;

    state->runicSmashSide = 0;
    state->runicSmashSeenMs = 0;
    state->smashScanMs = 0;
    state->gauntletTraced = false;
    state->leverPulled = false;
    state->leadNext = 0;
    state->leadStepMs = 0;
    state->chargedOrbGuid = ObjectGuid::Empty;
    state->orbScanMs = 0;
    state->lightningOrbGuid = ObjectGuid::Empty;
    state->lightningOrbScanMs = 0;
    state->ringSlots = {};
    state->ringAnchor = 0.0f;
    state->ringLatched = false;
    state->ringOrb.Clear();
    state->ringSolveMs = 0;
    state->rangedShelters.clear();
    state->blizzardSpots.clear();
    state->blizzardBunnies.clear();
    state->blizzardScanMs = 0;
    state->bossGuid.Clear();
    state->colossusGuid.Clear();
    state->colossusScanMs = 0;
    state->runeGiantGuid.Clear();
    state->runeGiantScanMs = 0;
    state->squads.clear();
    state->squadsAssigned = false;
    state->sifJoined = false;
    state->squadsNoted = false;
    state->humanSquadScanMs = 0;
    state->marksCleared = false;
    state->phase2StartMs = 0;
    state->openingReleased.clear();
    state->unreachable = ObjectGuid::Empty;
    state->unreachableSinceMs = 0;
}
