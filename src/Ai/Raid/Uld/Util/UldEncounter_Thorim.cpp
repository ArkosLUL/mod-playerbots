/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_Thorim.h"

#include "Creature.h"
#include "DBCEnums.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "Map.h"
#include "MotionMaster.h"
#include "PetDefines.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "RtiTargetValue.h"
#include "Timer.h"
#include "UldScripts.h"
#include "Unit.h"
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <list>
#include <mutex>
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
const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_1 = Position(2219.5417f, -264.77167f, 412.17548f);
const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_2 = Position(2217.446f, -275.85248f, 412.17548f);
const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_5_YARDS_1 = Position(2217.8877f, -295.01193f, 412.13434f);
const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_1 = Position(2212.193f, -307.44992f, 412.1348f);
const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_2 = Position(2212.1353f, -318.20795f, 412.1348f);
const Position ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_3 = Position(2212.1956f, -328.0144f, 412.1348f);
// Both doorways are only ~18 yd wide (x 2126-2144), so 1 and 5 stay near the middle; 2 to 4 run up
// the east side of the chamber, which is where the mesh smooths to anyway. Z is what navprobe
// settles each point to, since the walk moves on exact waypoints and does not correct height.
const Position ULDUAR_THORIM_BALCONY_1 = Position(2141.0f, -408.0f, 438.247f);
const Position ULDUAR_THORIM_BALCONY_2 = Position(2151.0f, -396.0f, 438.247f);
const Position ULDUAR_THORIM_BALCONY_3 = Position(2152.0f, -365.0f, 438.744f);
const Position ULDUAR_THORIM_BALCONY_4 = Position(2151.0f, -332.0f, 438.247f);
const Position ULDUAR_THORIM_BALCONY_5 = Position(2137.5f, -318.0f, 438.222f);
const Position ULDUAR_THORIM_JUMP_START_POINT = Position(2137.137f, -291.19025f, 438.24753f, 1.7059844f);
const Position ULDUAR_THORIM_JUMP_END_POINT = Position(2137.8818f, -278.18942f, 419.66653f);
// Z as navprobe settles each point, for the same reason as the waypoints above.
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
const Position ULDUAR_THORIM_PHASE2_OFFTANK_SPOT = Position(2115.0f, -247.0f, 419.458f);

namespace
{

// Keyed by instance: trigger, action and multiplier each hold their own helper instance, so the state
// they must agree on is defined here and nowhere else.
//
// Not thread_local. A map is updated by one thread at a time but is never pinned to one, and
// MapUpdate.Threads is 6 here, so per-thread copies hand the same instance a fresh state whenever the
// pool reassigns it: the melee slot gets re-picked out of whatever that copy holds, and the latched ring
// bearing is struck again off a boss that has drifted. That is the ring flipping between three points
// several times a second. In a trace it reads as exactly six thorim.slot rows per bot, one per thread
// that ever ticked the pull. References into an unordered_map survive rehashing, so the lock only has to
// cover the lookup.
std::mutex thorimStatesMutex;
std::unordered_map<uint32 /*instanceId*/, ThorimEncounterState> thorimStates;

ThorimEncounterState& ThorimStateFor(Player* bot)
{
    std::lock_guard<std::mutex> guard(thorimStatesMutex);
    return thorimStates[bot->GetInstanceId()];
}

struct GauntletWaypoint
{
    Position const* position;
    // How close the walker has to be to count as standing at this waypoint. Straight out of the
    // original hand-tuned lane walk, where the wider spots are the ones with room around them.
    float radius;
};

std::array<GauntletWaypoint, ULDUAR_THORIM_GAUNTLET_WAYPOINTS> const leftLaneWaypoints = {{
    {&ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_1, 6.0f},
    {&ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_2, 6.0f},
    {&ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_5_YARDS_1, 5.0f},
    {&ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_1, 10.0f},
    {&ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_2, 10.0f},
    {&ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_3, 10.0f},
}};

std::array<GauntletWaypoint, ULDUAR_THORIM_GAUNTLET_WAYPOINTS> const rightLaneWaypoints = {{
    {&ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_1, 6.0f},
    {&ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_2, 6.0f},
    {&ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_5_YARDS_1, 5.0f},
    {&ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_1, 10.0f},
    {&ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_2, 10.0f},
    {&ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_3, 10.0f},
}};

std::array<GauntletWaypoint, ULDUAR_THORIM_GAUNTLET_WAYPOINTS> const& LaneWaypoints(bool leftLane)
{
    return leftLane ? leftLaneWaypoints : rightLaneWaypoints;
}

// Walked north, so y rises across the chain and the last entry is the jump start.
std::array<Position const*, ULDUAR_THORIM_BALCONY_WAYPOINTS> const balconyWaypoints = {{
    &ULDUAR_THORIM_BALCONY_1,
    &ULDUAR_THORIM_BALCONY_2,
    &ULDUAR_THORIM_BALCONY_3,
    &ULDUAR_THORIM_BALCONY_4,
    &ULDUAR_THORIM_BALCONY_5,
    &ULDUAR_THORIM_JUMP_START_POINT,
}};

ThorimEncounterState* FindState(Player const* bot)
{
    if (!bot)
        return nullptr;

    std::lock_guard<std::mutex> guard(thorimStatesMutex);

    auto const itr = thorimStates.find(bot->GetInstanceId());
    return itr == thorimStates.end() ? nullptr : &itr->second;
}

// Both halves of the gate, in one place so the two cannot drift apart. Everything guarded by it
// either sweeps the grid or writes raid-wide state, so a raid parked on another boss reaching it is
// not free - and by distance alone Hodir's room does.
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

bool MemberCounts(Player const* member, uint32 instanceId)
{
    return member && member->IsAlive() && member->GetMapId() == ULDUAR_MAP_ID &&
           member->GetInstanceId() == instanceId;
}

// Who counts when handing out a formation slot. In the instance, alive or not - liveness is left out
// on purpose. Both pickers number their members in group order, so a member dropping out of the count
// renumbers everyone behind them and the whole formation shuffles over one death.
bool HoldsFormationSlot(Player const* member, uint32 instanceId)
{
    return member && member->GetMapId() == ULDUAR_MAP_ID && member->GetInstanceId() == instanceId;
}

// One predicate for the trigger and the action alike. PlayerbotAI::IsRanged() already reports true
// for healers, but the two sides used to test different things and could hand a healer two different
// slots on alternating ticks.
bool TakesRangedSpot(Player* member)
{
    return member && !PlayerbotAI::IsTank(member) &&
           (PlayerbotAI::IsRanged(member) || PlayerbotAI::IsHeal(member));
}

bool TakesMeleeSlot(Player* member)
{
    // No tank, whatever its index says. A ring point is measured off the boss, so a tank standing on one
    // orbits him instead of walking him to the anchor. A third tank waits on the off-tank bearing.
    return member && !TakesRangedSpot(member) && !PlayerbotAI::IsTank(member);
}

// Least-loaded slot rather than first-free, so six melee land three-and-three instead of piling onto
// one bearing once the ring is full.
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

        // Not MemberCounts: a slot dropped while a bot is dead comes back as whichever is least
        // loaded, which swings its bearing a quarter turn the moment it is rezzed.
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

// Same shape as EnsureMeleeSlot and for the same reason. This used to be a round robin counted over
// group order on every call, which renumbers the whole camp behind anyone who dies.
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

// Another member's squad, read straight off the split. GetThorimSquad only answers for the asking bot.
// No entry is the arena, same as there.
ThorimSquad SquadOf(ThorimEncounterState const& state, Player const* member)
{
    auto const itr = state.squads.find(member->GetGUID());
    return itr == state.squads.end() ? ThorimSquad::Arena : static_cast<ThorimSquad>(itr->second);
}

// Least-loaded like the two above, but a tie goes to the spot nearest the bot, not the lowest index.
// The arena squad starts on a ring round the landing point, so an index pick walks some of them
// straight through it.
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
    return session && session->IsBot();
}

// Struck once per pull. Every pick is bot-only: a human in the raid still occupies their role, but
// nothing in this module can walk them anywhere, so spending a gauntlet slot on one just leaves the
// corridor a body short.
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

    // Nobody here yet means the raid is still zoning in; try again rather than latching an empty split.
    if (roster.empty())
        return;

    // The whole split hangs off which tank holds the arena, and GetMainTankGuid's fallback only sees
    // tanks that are already alive and in the instance. Latch while the raid is still being summoned
    // in and the main tank reads as nobody, which sends the raid's only tank down the corridor and
    // leaves the arena untanked.
    if (PlayerbotAI::GetMainTankGuid(group).IsEmpty())
        return;

    bool const twentyFive = bot->GetRaidDifficulty() == Difficulty::RAID_DIFFICULTY_25MAN_NORMAL;
    uint32 healerQuota = twentyFive ? 2 : 1;

    // The corridor is a race: Sif's channel runs 150s and phase 1 opens 20s after the pull, so the
    // squad has about 170s to get a hit on Thorim from up top or hard mode is gone. Seven dps put ten
    // bodies down there measuring 23.4k dps against roughly 3.9M of health the walk is gated on, which
    // does not finish in time. Arena keeps the rest and the cap below still floors it.
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

    // A one-tank raid has no tank to spare. The IsMainTank guard below is only as good as whatever
    // GetMainTankGuid resolves to, and when that reads wrong this is what still keeps the arena
    // tanked - the adds spread onto the ranged and healers within seconds otherwise.
    size_t tankCount = 0;
    for (Player* member : roster)
        if (PlayerbotAI::IsTank(member))
            ++tankCount;

    // The main tank holds the arena whatever the quota says: he is the one thing the adds and the
    // phase 2 pickup both need to still be standing there.
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

    // Bots only. A human is never picked above, so filing them here would file every one of them under
    // the arena - including one who walks the whole corridor. TickHumanSquads reads theirs off
    // position instead.
    for (Player* member : roster)
        if (IsBotPlayer(member))
            state.squads[member->GetGUID()] =
                static_cast<uint8>(gauntlet.count(member->GetGUID()) ? ThorimSquad::Gauntlet : ThorimSquad::Arena);

    state.squadsAssigned = true;
}

// Where the humans in the raid actually are, for the trace and nothing else - every consumer of
// GetThorimSquad runs inside a trigger or an action, which only ever evaluate for a bot. The split
// itself is still struck once and left alone; this only labels the people it cannot place.
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

        // The same boundary the arena leash and the pet leash use, so "in the arena" means one thing
        // across the encounter. Anywhere else near Thorim is the corridor or the hallway above it.
        state.squads[member->GetGUID()] =
            static_cast<uint8>(ThorimInArenaBox(member) ? ThorimSquad::Arena : ThorimSquad::Gauntlet);
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

// Anchoring the ring on the tank's bearing stops it rotating as the boss shuffles, so a recompute
// does not shuffle everyone. The spot, not the tank standing on it: he is walked there and parked,
// and reading him live means a step sideways or a death swings the whole ring behind him.
float RingAnchorBearing(Unit* boss)
{
    return std::atan2(ULDUAR_THORIM_PHASE2_TANK_SPOT.GetPositionY() - boss->GetPositionY(),
                      ULDUAR_THORIM_PHASE2_TANK_SPOT.GetPositionX() - boss->GetPositionX());
}

float SlotBearing(float anchor, uint8 slot)
{
    // Three slots at 90 degree steps starting a quarter turn off the tank, which leaves the whole
    // tank side of the boss empty for the two of them.
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

// The margin on top of the spell's own 75 degree arc covers Thorim re-orienting onto the orb between
// the tick that picks a way out and the tick a bot finishes walking it.
bool InLightningChargeCone(float bearing, float coneBearing)
{
    float const halfWidth = ULDUAR_THORIM_LIGHTNING_CHARGE_CONE_ANGLE / 2.0f + ULDUAR_THORIM_LIGHTNING_CHARGE_MARGIN;
    return AbsAngleDelta(bearing, coneBearing) <= halfWidth;
}

// How far this bot sits off its latched bearing so the Lightning Charge cone misses it. Only the slot
// the cone actually covers moves, and only to the edge: turning all three together cost every melee
// bot an 11 yd run per charge and another one back when the orb went dark, and every charge that did
// hit a melee bot hit one that was still running.
void LightningChargeOffset(PlayerbotAI* botAI, Player* bot, Unit* boss, float bearing, float& offset)
{
    offset = 0.0f;

    ThorimEncounterState* state = FindState(bot);
    if (!state)
        return;

    ThorimEncounterState::RingOffset& held = state->ringOffsets[bot->GetGUID()];
    offset = held.offset;

    // Held while nothing is lit. Snapping back to the latched bearing is a second run for nothing -
    // the slot is only ever wrong again when a new orb draws a new cone.
    Unit* orb = ThorimChargedThunderOrb(botAI, SPELL_THORIM_LIGHTNING_ORB_VISUAL);
    if (!orb || held.orb == orb->GetGUID())
        return;

    held.orb = orb->GetGUID();
    held.offset = 0.0f;
    offset = 0.0f;

    // Measured off the latched bearing every time, never off wherever the last cone left it. Chaining
    // them lets the three slots drift out of their 90 degree spacing until two share a point, which is
    // what the old rigid whole-ring turn was really buying.
    float const coneBearing = BearingFromBoss(boss, orb->GetPositionX(), orb->GetPositionY());
    if (!InLightningChargeCone(bearing, coneBearing))
        return;

    // Out by the nearer edge. Costs a median 4 yd against the 11 yd every slot walked before, and the
    // price is that a displaced slot can end up 4.5 yd off a neighbour instead of 11.3 - inside Chain
    // Lightning's 10 yd jump. The melee ring already chains through the bots stacked on each slot, and
    // a chain that stays in melee was the one that killed nobody.
    float const halfWidth = ULDUAR_THORIM_LIGHTNING_CHARGE_CONE_ANGLE / 2.0f +
                            ULDUAR_THORIM_LIGHTNING_CHARGE_MARGIN + ULDUAR_THORIM_RING_CONE_CLEARANCE;
    float const low = Position::NormalizeOrientation(coneBearing - halfWidth);
    float const high = Position::NormalizeOrientation(coneBearing + halfWidth);
    float const exit = AbsAngleDelta(high, bearing) <= AbsAngleDelta(low, bearing) ? high : low;

    held.offset = Position::NormalizeOrientation(exit - bearing);
    offset = held.offset;
}

// Every live Blizzard zone plus the bunny, swept once per instance per interval. The bunny marks where
// the next zone drops within 2s, and the zones behind it are where the damage is. Positions rather than
// units because that is all the tests want, and a stale guid would need re-resolving on every bearing.
std::vector<Position> const& ThorimBlizzardSpots(Player* bot)
{
    ThorimEncounterState& state = ThorimStateFor(bot);
    if (state.blizzardScanMs && GetMSTimeDiffToNow(state.blizzardScanMs) < ULDUAR_THORIM_ENCOUNTER_SCAN_INTERVAL_MS)
        return state.blizzardSpots;

    state.blizzardScanMs = getMSTime();
    state.blizzardSpots.clear();

    std::list<Creature*> bunnies;
    bot->GetCreatureListWithEntryInGrid(bunnies, NPC_SIF_BLIZZARD, ULDUAR_THORIM_BLIZZARD_SCAN_RANGE);
    for (Creature* bunny : bunnies)
        if (bunny && bunny->IsAlive())
            state.blizzardSpots.emplace_back(bunny->GetPositionX(), bunny->GetPositionY(), bunny->GetPositionZ());

    for (uint32 const zoneSpell : {SPELL_SIF_BLIZZARD_ZONE_10, SPELL_SIF_BLIZZARD_ZONE_25})
        for (Position const& zone : GetDynamicObjectPositions(bot, ULDUAR_THORIM_BLIZZARD_SCAN_RANGE, zoneSpell))
            state.blizzardSpots.push_back(zone);

    return state.blizzardSpots;
}

bool RingBearingClearOfBlizzard(Unit* boss, float bearing, std::vector<Position> const& zones)
{
    float const x = boss->GetPositionX() + std::cos(bearing) * ULDUAR_THORIM_MELEE_RING_RADIUS;
    float const y = boss->GetPositionY() + std::sin(bearing) * ULDUAR_THORIM_MELEE_RING_RADIUS;

    for (Position const& zone : zones)
        if (zone.GetExactDist2d(x, y) < ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE)
            return false;

    return true;
}

// Whether the spot we have been handed is worth breaking the deadband for. The deadband is there to
// stop a bot chasing a ring point that drifts with the boss, not to hold it in fire, and at 8 yd both
// slides come out under it: a whole cone dodge is a 5 yd chord or less. Both halves of each test
// matter - if the spot is no better there is nothing to buy by walking to it.
bool RingSpotBeatsTheDeadband(PlayerbotAI* botAI, Player* bot, Position const& spot)
{
    // Cone first. It lands for 20k in one millisecond against a Blizzard tick's 3k, and it was the
    // half nothing checked: nine cone hits over two pulls with a safe spot under 5 yd away.
    if (Unit* boss = GetThorim(botAI))
    {
        if (Unit* orb = ThorimChargedThunderOrb(botAI, SPELL_THORIM_LIGHTNING_ORB_VISUAL))
        {
            float const coneBearing = BearingFromBoss(boss, orb->GetPositionX(), orb->GetPositionY());
            if (InLightningChargeCone(BearingFromBoss(boss, bot->GetPositionX(), bot->GetPositionY()), coneBearing) &&
                !InLightningChargeCone(BearingFromBoss(boss, spot.GetPositionX(), spot.GetPositionY()), coneBearing))
                return true;
        }
    }

    std::vector<Position> const& zones = ThorimBlizzardSpots(bot);
    if (zones.empty())
        return false;

    bool standingInOne = false;
    for (Position const& zone : zones)
    {
        if (zone.GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) < ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE)
            return false;

        if (zone.GetExactDist2d(bot->GetPositionX(), bot->GetPositionY()) < ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE)
            standingInOne = true;
    }

    return standingInOne;
}

// How far this bot slides around the ring to get off a Blizzard. Sliding rather than fleeing: the old
// answer was the generic MoveAwayFromCreature, which takes the furthest of eight rays out to 30 yd, so
// every accepted flee asked for the full 30 and dumped a melee bot a median 35 yd from the boss - and
// then took another tick within 6s anyway 23-58% of the time, because the zones sit on a loop and
// running outward lands on a different arc of it. The ring is never fully covered, worst case 22% clear
// over 1726 sampled snapshots and blocked outright in none of them, and the nearest clear bearing is a
// median 3-7 yd of arc away.
void BlizzardRingOffset(PlayerbotAI* botAI, Player* bot, Unit* boss, float bearing, float& offset)
{
    offset = 0.0f;

    ThorimEncounterState& state = ThorimStateFor(bot);
    std::vector<Position> const& zones = ThorimBlizzardSpots(bot);
    if (zones.empty())
    {
        state.blizzardOffsets.erase(bot->GetGUID());
        return;
    }

    // Resolved before the hold, not after. A held offset is a rotation off whatever bearing comes in,
    // so one solved while nothing was lit will happily turn a fresh cone-safe bearing back under the
    // orb - that is three of the cone hits, one of them from 56 degrees off cone to 1.4.
    Unit* orb = ThorimChargedThunderOrb(botAI, SPELL_THORIM_LIGHTNING_ORB_VISUAL);
    float const coneBearing = orb ? BearingFromBoss(boss, orb->GetPositionX(), orb->GetPositionY()) : 0.0f;

    auto const held = state.blizzardOffsets.find(bot->GetGUID());
    bool const haveHeld = held != state.blizzardOffsets.end();

    // Hold what we already walked to while it is still clear. Re-solving every tick against a zone
    // that dropped somewhere new has the bot sliding in place, and a moving bot casts nothing.
    if (haveHeld)
    {
        float const current = Position::NormalizeOrientation(bearing + held->second);
        if (RingBearingClearOfBlizzard(boss, current, zones) &&
            !(orb && InLightningChargeCone(current, coneBearing)))
        {
            offset = held->second;
            return;
        }
    }

    // Measured off the bearing that came in, never off wherever the last zone left us. The incoming
    // bearing already has the cone offset on it, so searching from there is what keeps the two in step.
    //
    // Whichever side the last answer was on gets tried first. Fixed order instead had one new zone
    // flip the answer clean across the ring: Assasin swung 99 degrees, 12 yd of arc, in 0.64s.
    // Offsets are normalised to [0, 2pi), so past pi is the counter-clockwise side.
    int8 const firstWay = haveHeld && held->second > float(M_PI) ? -1 : 1;

    float const step = 0.0349f;  // 2 degrees
    for (uint8 tick = 0; tick <= 90; ++tick)
    {
        for (int8 turn = 0; turn < 2; ++turn)
        {
            int8 const way = turn ? -firstWay : firstWay;
            float const candidate = Position::NormalizeOrientation(bearing + way * tick * step);
            if (RingBearingClearOfBlizzard(boss, candidate, zones) &&
                // A cone is 20k in the instant it lands and a Blizzard tick is about 3k, so the cone
                // wins the tie: a bearing that clears the zones but sits under a lit orb is no answer.
                !(orb && InLightningChargeCone(candidate, coneBearing)))
            {
                offset = Position::NormalizeOrientation(candidate - bearing);
                state.blizzardOffsets[bot->GetGUID()] = offset;
                return;
            }

            // Both ways are the same point at tick 0.
            if (!tick)
                break;
        }
    }

    state.blizzardOffsets.erase(bot->GetGUID());
}

// Raw ring geometry is exactly the shape that lands off the navmesh, and MoveTo would then fail
// without telling anyone. The arena floor also has a hole south of y = -288, which is what the melee
// range re-check catches.
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

// The seven Thunder Orbs, in the order the shelter table below indexes them. Fixed props, one per
// pillar bunny at the same x,y, so this is only ever used to turn the lit orb into a table index.
std::array<Position, 7> const ULDUAR_THORIM_THUNDER_ORB_SPOTS = {
    Position(2145.50f, -222.62f, 433.30f), Position(2164.20f, -233.47f, 433.30f),
    Position(2164.55f, -293.00f, 433.30f), Position(2105.04f, -292.56f, 433.30f),
    Position(2092.95f, -263.00f, 433.30f), Position(2104.94f, -233.44f, 433.30f),
    Position(2124.30f, -222.60f, 433.30f)};

// Where a camp slot stands while the orb that covers it is lit, one row per (orb, slot) pair the cone
// actually reaches. Only five of the seven orbs reach the camp at all, and orbs 5 and 6 point away from
// it entirely, so they have no rows.
//
// Solved offline and every row navprobed, point and path: 42.5 degrees off the cone bearing at every
// settled boss position five traces show, 22 yd off the tank spot so the melee ring cannot bridge Chain
// Lightning in, inside 32 of the boss so the shorter nukes still reach, and 9 from every other body
// standing at the time - Chain Lightning jumps 8.0 centre to centre. Longest run is 25.6 yd, about
// 3.7s, against a 4.9s worst measured warning, but the run is not capped at runtime: a bot commits to
// its shelter from wherever it happens to be, and starting from a previous orb's shelter has been
// measured at 60 yd.
//
// Clearance from Sif's Blizzard track is a preference here, not a rule, and three of orb 2's four rows
// cannot have it: the pocket that is both off that cone and 11 yd clear of the track runs to about 97
// square yards, and four points 9 yd apart do not fit in it. Those three sit 7.6-8.2 yd out, inside the
// measured 9.8 reach. Right trade anyway - a cone is 20k in one instant against a 3k tick, and standing
// on a shelter was only 14% of the camp's Blizzard damage. Walking to one was 63%.
//
// Two rows are under 3 yd, because those slots sit at 34-37 degrees off cone at home - only just inside
// the 37.5 arc, so the edge is close. They still walk: the camp's trigger moves at 1 yd, not the ring's
// 3.
struct ThorimShelterSpot
{
    uint8 orbIndex;
    uint8 slot;
    Position spot;
};

std::array<ThorimShelterSpot, 12> const ULDUAR_THORIM_SHELTER_SPOTS = {{
    {0, 4, Position(2142.00f, -255.35f, 419.771f)},
    {0, 5, Position(2113.00f, -227.35f, 420.293f)},
    {1, 3, Position(2132.00f, -276.35f, 419.755f)},
    {1, 4, Position(2130.50f, -262.85f, 419.905f)},
    {1, 5, Position(2119.50f, -226.85f, 420.146f)},
    {2, 0, Position(2116.50f, -283.35f, 419.509f)},
    {2, 1, Position(2107.50f, -283.35f, 420.104f)},
    {2, 2, Position(2114.50f, -274.35f, 419.562f)},
    {2, 3, Position(2140.00f, -241.35f, 419.502f)},
    {3, 0, Position(2129.50f, -278.35f, 419.702f)},
    {3, 1, Position(2126.00f, -269.85f, 419.761f)},
    {6, 5, Position(2133.50f, -246.85f, 419.652f)},
}};

// Which of the seven the lit orb is. Refuses a loose match rather than picking the nearest: a wrong
// index is a bot walking confidently into the cone, and staying home is only as bad as today.
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

// The camp's cone test. Its own margin, not the ring's: see the header for why 2.5 rather than 15.
bool InLightningChargeConeRanged(float bearing, float coneBearing)
{
    float const halfWidth =
        ULDUAR_THORIM_LIGHTNING_CHARGE_CONE_ANGLE / 2.0f + ULDUAR_THORIM_LIGHTNING_CHARGE_RANGED_MARGIN;
    return AbsAngleDelta(bearing, coneBearing) <= halfWidth;
}

// Where this camp slot stands right now: its spot, or its shelter while an orb covering that spot is
// lit. Sticky per orb - once we are sheltered for this orb we stay sheltered until a different one
// lights, which is 15s away at the soonest. There is deliberately no snap home when it goes dark: that
// is a second run for nothing, and it would put the bot back in the open right as the next cone is
// picked. Same latch the melee ring's cone offset already runs on.
// Reports whether it sheltered, and off which orb, purely so the caller can note it.
bool ThorimRangedSpot(PlayerbotAI* botAI, Player* bot, Unit* boss, uint8 slot, Position& out, uint8& orbIndex)
{
    out = RangedSpot(slot);

    // Resolved before the state reference is taken, because it goes through ThorimStateFor itself.
    Unit* orb = ThorimChargedThunderOrb(botAI, SPELL_THORIM_LIGHTNING_ORB_VISUAL);

    ThorimEncounterState::RangedShelter& held = ThorimStateFor(bot).rangedShelters[bot->GetGUID()];

    if (orb && held.orb != orb->GetGUID())
    {
        held.orb = orb->GetGUID();
        held.sheltered = false;

        // An orb we cannot place is left unlatched, so the next tick tries again instead of holding a
        // bad index for the whole 15s.
        if (!ThorimThunderOrbIndex(orb, held.orbIndex))
            held.orb.Clear();
    }

    Position shelter;

    // Re-tested every tick while the orb is lit and we have not committed, so a boss that drifts into
    // covering this slot part way through the warning still gets an answer. Only ever false to true.
    if (orb && !held.sheltered && held.orb == orb->GetGUID())
    {
        float const bearing = BearingFromBoss(boss, out.GetPositionX(), out.GetPositionY());
        float const coneBearing = BearingFromBoss(boss, orb->GetPositionX(), orb->GetPositionY());
        if (InLightningChargeConeRanged(bearing, coneBearing) && ThorimShelterFor(held.orbIndex, slot, shelter))
            held.sheltered = true;
    }

    if (!held.sheltered)
        return false;

    // The slot can still be reassigned under us by a death, and the new one may have no row for this
    // orb. Home is the only safe answer then.
    if (!ThorimShelterFor(held.orbIndex, slot, shelter))
    {
        held.sheltered = false;
        return false;
    }

    out = shelter;
    orbIndex = held.orbIndex;
    return true;
}

// The slot's bearing off Thorim, struck the first time the bot asks and then left alone for the phase.
// Anchored on the tank's spot rather than the tank himself: he is walked there and parked, and reading
// him live only hands the ring one more input that moves.
float LatchedRingBearing(Player* bot, Unit* boss, uint8 slot)
{
    ThorimEncounterState& state = ThorimStateFor(bot);
    auto const itr = state.ringBearings.find(bot->GetGUID());
    if (itr != state.ringBearings.end())
        return itr->second;

    float const bearing = SlotBearing(RingAnchorBearing(boss), slot);
    state.ringBearings[bot->GetGUID()] = bearing;
    return bearing;
}

// A melee bot parked 25 yd from the boss at zero DPS is worse off than an unspread one, so the static
// trio is only taken when it is actually in range.
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

// The blast wave has no world object behind it, so a trace has nothing to sweep for and no way to
// tell afterwards which lane was hot when somebody died in it. This scan is the only thing that
// knows.
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

    // The Colossus and the corridor adds are trash to the instance script, so nothing else opens a
    // trace for the half of the fight that most often loses it. Named after Thorim: the gauntlet and
    // the arena are one attempt. This has to happen before the first lane is noted below, or the
    // hazard record has no trace to land in.
    if (!state.gauntletTraced && bot->IsInCombat())
    {
        state.gauntletTraced = true;
        RaidObs::MarkPull(bot->GetMap(), GetThorim(botAI));
    }

    Unit* colossus = GetThorimRunicColossus(botAI);

    // EVENT_RC_RUNIC_SMASH is cancelled in JustEngagedWith, so once he is tanked the corridor is safe
    // and the squad has no reason to keep favouring a lane.
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

}  // namespace

bool NearThorimEncounter(Player const* bot)
{
    if (!bot || bot->GetDistance(ULDUAR_THORIM_NEAR_ARENA_CENTER) > ULDUAR_THORIM_ENCOUNTER_PROXIMITY)
        return false;

    // Under the wing ceiling covers the arena and the corridor. Above it, only the box does - which is
    // the half that was missing, and why every node keyed off this one went quiet the moment the squad
    // climbed the ramp.
    return bot->GetPositionZ() < ULDUAR_THORIM_WING_MAX_Z || OnThorimBalcony(bot);
}

Unit* GetThorim(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot)
        return nullptr;

    // Cached because the sight list it falls back to stops at AiPlayerbot.SightDistance, and the boss
    // is 100-145 yd from the upper hallway and up to 176 yd from the ramp - so a corridor bot on the
    // balcony could not see him, and with him went the split, the squad label and the walk down.
    // ObjectAccessor has no range of its own, so once the guid is struck the answer holds anywhere in
    // the wing. NearThorimEncounter is what still bounds the callers.
    ThorimEncounterState& state = ThorimStateFor(bot);
    if (state.bossGuid)
    {
        Unit* cached = botAI->GetUnit(state.bossGuid);
        if (cached && cached->IsAlive())
            return cached;

        state.bossGuid.Clear();
    }

    // Only until it is struck: the raid pulls from inside the arena, so this runs for the first tick
    // and then never again.
    Unit* boss = GetFirstAliveUnitByEntry(botAI, NPC_THORIM);
    if (boss)
        state.bossGuid = boss->GetGUID();

    return boss;
}

Unit* GetThorimRunicColossus(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot)
        return nullptr;

    // Cached because the multiplier asks on every melee action of every bot, and a 150 yd grid sweep
    // is far too heavy for that. One creature, so any bot's answer serves the whole instance.
    ThorimEncounterState& state = ThorimStateFor(bot);
    if (state.colossusScanMs && GetMSTimeDiffToNow(state.colossusScanMs) < ULDUAR_THORIM_ENCOUNTER_SCAN_INTERVAL_MS)
    {
        Unit* cached = botAI->GetUnit(state.colossusGuid);
        return cached && cached->IsAlive() ? cached : nullptr;
    }

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

    // No-LOS, because the arena pile routinely puts an add behind another one and a bot that cannot
    // see the Evoker this tick still has to count it as the thing to kill.
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

    // Phase 1. He and Sif fight from the balcony and neither can be touched from the floor, so holding
    // either is pure lost throughput - and worse than that, the balcony is above the arena box, which
    // is the state the arena target guard shuts a bot down for.
    return target != boss && target->GetEntry() != NPC_SIF;
}

namespace
{

// One priority tier. Holds whatever the bot already has if it is still in this tier, and only trades
// it for something meaningfully closer to `pivot` - the arena centre for the pile, the bot itself in
// the corridor.
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

}  // namespace

namespace
{

Unit* NoteThorimDpsTarget(Player* bot, Unit* target)
{
    if (bot)
        if (ThorimEncounterState* state = FindState(bot))
            state->dpsTargets[bot->GetGUID()] = target ? target->GetGUID() : ObjectGuid::Empty;

    return target;
}

}  // namespace

Unit* GetThorimDpsTarget(PlayerbotAI* botAI, Player* bot, Unit* currentTarget)
{
    if (!botAI || !bot || !NearThorimEncounter(bot))
        return nullptr;

    Unit* boss = GetThorim(botAI);
    if (!boss || !boss->IsAlive() || !boss->IsHostileTo(bot))
        return nullptr;

    // Phase 2 is one target and nothing else matters.
    if (boss->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
        return NoteThorimDpsTarget(bot, boss);

    ThorimEncounterTargets targets;
    GatherThorimEncounterTargets(botAI, targets);

    if (GetThorimSquad(botAI, bot) == ThorimSquad::Gauntlet)
    {
        // Acolytes heal the pack, then whatever is already swinging, then the two the corridor is
        // gated on. Walk-in trash last - it is only ever alive before the corridor opens, and the
        // range filter has already dropped anything 50 yd away.
        std::vector<std::vector<Unit*> const*> const tiers = {&targets.acolytes, &targets.guards,
                                                              &targets.trash};
        for (auto const* tier : tiers)
            if (Unit* pick = SelectThorimTierTarget(currentTarget, *tier, *bot))
                return NoteThorimDpsTarget(bot, pick);

        if (targets.colossus)
            return NoteThorimDpsTarget(bot, targets.colossus);

        if (targets.runeGiant)
            return NoteThorimDpsTarget(bot, targets.runeGiant);

        // Thorim is the last thing in the gauntlet, and hitting him is what ends phase 1 - boss_thorim
        // starts the jump on DamageTaken from a player above z 430. Without this the squad runs out of
        // targets the moment the Rune Giant dies and phase 2 waits on a human. Gated on the bot's own
        // height, not the squad, or a corridor bot at z 412 gets pulled 150 yd out of its lane.
        if (bot->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
            return NoteThorimDpsTarget(bot, boss);

        return NoteThorimDpsTarget(bot, nullptr);
    }

    // Arena. Acolytes first whoever is asking - they come with the wave and heal it back up. After
    // that the roles part company: a caster kills an Evoker from where it already stands, while a
    // melee sent across the room at one walks straight past the Champion doing 41% of the damage the
    // squad takes. Commoner is last either way; it barely hits and killing one buys nothing.
    bool const melee = PlayerbotAI::IsMelee(bot);
    using TierOrder = std::array<std::vector<Unit*> const*, 6>;
    TierOrder const meleeTiers = {&targets.acolytes, &targets.champions, &targets.warbringers,
                                  &targets.evokers, &targets.commoners, &targets.trash};
    TierOrder const rangedTiers = {&targets.acolytes, &targets.evokers, &targets.champions,
                                   &targets.warbringers, &targets.commoners, &targets.trash};
    TierOrder const& tiers = melee ? meleeTiers : rangedTiers;

    // Melee measure from themselves, exactly as the corridor branch above does, so a tier hands back
    // its nearest rather than whichever one is deepest in the pile. Ranged keep the centre, and that
    // shared pivot is what has them all focus the same unit.
    Position const& pivot = melee ? static_cast<Position const&>(*bot) : ULDUAR_THORIM_NEAR_ARENA_CENTER;

    // Two passes for melee. The first skips any tier with nothing within reach, so a Champion standing
    // on the bot beats an Evoker across the room; the second drops that test, so a melee bot with an
    // empty patch around it still commits to the raid's focus rather than standing idle. Ranged run
    // one pass - they have no reach limit to relax.
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

    // Both gates matter for a cached read: the entry survives the pull that wrote it, so without them
    // a stale guid from the last attempt would shut the generic picker down on a bot the encounter is
    // no longer steering.
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

    // Per bot and unlatched, because this is the half that pins: IsHighPriority short-circuits every
    // find-target strategy for anything in here, so one stale entry outranks the whole priority list
    // below it.
    botAI->GetAiObjectContext()->GetValue<GuidVector>("prioritized targets")->Set({});

    Group* group = bot->GetGroup();
    if (!group)
        return;

    ThorimEncounterState& state = ThorimStateFor(bot);
    if (state.marksCleared)
        return;

    state.marksCleared = true;

    // Skull, cross and moon are the three this encounter used to set. RtiTargetValue hands an icon
    // back before the smart picker runs, so a leftover one silently outranks everything chosen here.
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
    if (Unit* master = botAI->GetMaster())
        if (ThorimGauntletLaneIndex(master, index, lane))
            return true;

    return ThorimGauntletLaneIndex(bot, index, lane);
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

    // The left hand lights the bunnies that sit in the left lane, so the safe lane is always the one
    // opposite the hand that went up.
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

    // Cached the same way the Colossus is, and for the same reason: the balcony trigger asks once per
    // bot per tick and a 150 yd grid sweep at that rate is not worth one boolean.
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

bool ThorimBalconyOpen(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot || !NearThorimEncounter(bot))
        return false;

    // Above the floor line, which up here means the hallway and Thorim's platform rather than the
    // corridor. The corridor squad has its own node and must not be dragged into this one.
    if (bot->GetPositionZ() <= ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
        return false;

    Unit* boss = GetThorim(botAI);
    if (!boss || !boss->IsAlive())
        return false;

    // The Giant's death opens the second doors and sets _isHitAllowed, so it is the one event that
    // turns the hallway into the squad's next job. While it lives, killing it is.
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

    // Passing the waypoint's y counts as reaching it. The hallway runs one way, so a bot shoved north
    // of a point by a knockback or by the pile is already done with it, and sending it back south is
    // how the squad ends up walking the same ground twice.
    auto const reached = [bot](uint8 index)
    {
        Position const& here = GetThorimBalconyWaypoint(index);
        return bot->GetExactDist2d(&here) <= ULDUAR_THORIM_BALCONY_ARRIVE_TOLERANCE ||
               bot->GetPositionY() > here.GetPositionY();
    };

    // Down first. Going up is a judgement call and the latch exists to make it stick, but going down
    // is not one: standing 40 yd south of the point you claim to have passed means you have not passed
    // it. Without this a step held over from an earlier pull hands a bot a waypoint from the middle of
    // the chain while it is still on the ramp, and the straight line there crosses a Paralytic Field
    // bunny - ten of thirteen corridor bots in one trace, x 2141 up the middle instead of x 2151.
    while (step > 0 && !reached(step - 1))
        --step;

    // Runs one past the last waypoint. ULDUAR_THORIM_BALCONY_WAYPOINTS means arrived, and arrived has
    // to be a latch rather than a distance test: once the squad is on the platform it is fighting
    // Thorim and drifting off the mark, and re-anchoring it there every tick would fight "reach melee"
    // exactly the way the arena picker used to fight "dps assist".
    while (step < ULDUAR_THORIM_BALCONY_WAYPOINTS && reached(step))
        ++step;

    state->balconyStep[bot->GetGUID()] = step;
    return step;
}

bool ThorimBarrierBailLatched(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot || bot->GetMapId() != ULDUAR_MAP_ID)
        return false;

    if (!PlayerbotAI::IsMelee(bot) || PlayerbotAI::IsTank(bot))
        return false;

    if (!NearThorimEncounter(bot))
        return false;

    ThorimEncounterState& state = ThorimStateFor(bot);

    Unit* colossus = GetThorimRunicColossus(botAI);
    if (!colossus || !colossus->IsAlive() || !colossus->HasAura(SPELL_THORIM_RUNIC_BARRIER))
    {
        state.barrierBailing.erase(bot->GetGUID());
        return false;
    }

    bool const bailing = state.barrierBailing.count(bot->GetGUID()) > 0;
    if (bailing)
    {
        if (bot->GetHealthPct() >= ULDUAR_THORIM_BARRIER_RESUME_HEALTH_PCT)
        {
            state.barrierBailing.erase(bot->GetGUID());
            return false;
        }

        return true;
    }

    if (bot->GetHealthPct() > ULDUAR_THORIM_BARRIER_BAIL_HEALTH_PCT)
        return false;

    state.barrierBailing.insert(bot->GetGUID());
    return true;
}

bool ThorimSplitActive(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!NearThorimEncounter(bot))
        return false;

    Unit* boss = GetThorim(botAI);
    if (!boss || !boss->IsHostileTo(bot))
        return false;

    // Combat, so the leash stays off the raid while it walks in and clears the six trash; the balcony
    // height, so it lets go the instant he drops for phase 2.
    return boss->IsInCombat() && boss->GetPositionZ() >= ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD;
}

ThorimSquad GetThorimSquad(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot || bot->GetMapId() != ULDUAR_MAP_ID)
        return ThorimSquad::None;

    if (!NearThorimEncounter(bot))
        return ThorimSquad::None;

    // Deliberately not gated on combat: the corridor squad forms up at the gate before the pull, and
    // the side it is on has to be decided by then.
    if (!GetThorim(botAI))
        return ThorimSquad::None;

    AssignThorimSquads(bot);
    TickHumanSquads(bot);

    // The assignment above almost always happens before the pull, and RaidObs drops a note when no
    // session is open, so the split has to be written out again once there is a trace to write it to.
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

// How long before a pet that is still outside may be told again. MoveFollow restarts the walk, so a
// command every tick leaves the pet running on the spot and never arriving.
constexpr uint32 ULDUAR_THORIM_PET_RECALL_INTERVAL_MS = 2000;

}  // namespace

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
        // GetPet() would miss most of what matters here: a Frost death knight's Raise Dead, Feral
        // Spirits and Army are guardians, not pets. Totems are skipped the other way - a shaman drops
        // four, none of them move, and none of them can pull anything.
        if (!pet || !pet->IsAlive() || pet->IsTotem())
            continue;

        if (!pet->IsPet() && !pet->IsGuardian())
            continue;

        if (!pet->IsInWorld() || pet->GetMapId() != bot->GetMapId())
            continue;

        // The same boundary the owner is held to. A pet chasing an add that landed 24 yd out is doing
        // its job; this only catches the ones that have left the room.
        //
        // The corridor squad has no room to be held to - it walks 300 yd of gauntlet - so its pets get
        // the owner instead. Without this a pet sent at something it cannot reach walks the whole
        // corridor to get there and never comes back.
        bool home;
        if (squad == ThorimSquad::Arena)
            home = ThorimInArenaBox(pet) && pet->GetExactDist2d(centre.GetPositionX(), centre.GetPositionY()) <=
                                                ULDUAR_THORIM_ARENA_LEASH_RADIUS;
        else
            home = pet->GetExactDist2d(bot->GetPositionX(), bot->GetPositionY()) <=
                   ULDUAR_THORIM_PET_OWNER_LEASH_RADIUS;

        if (home)
            continue;

        // Time-based rather than a "already told it" latch, which would never fire twice for a pet
        // that strays, comes home and strays again.
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

    // A guardian can have no CharmInfo at all, and for those the stop and the walk above are the whole
    // recall. PetAI::CanAttack reads COMMAND_FOLLOW as "attack nothing while returning" and lets the
    // pet fight again the moment IsReturning clears, so this holds it only for the walk home.
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

    // The radius is the working leash; the box is the backstop, because it is the box the boss script
    // actually scans.
    float const distance = bot->GetExactDist2d(ULDUAR_THORIM_NEAR_ARENA_CENTER.GetPositionX(),
                                               ULDUAR_THORIM_NEAR_ARENA_CENTER.GetPositionY());

    // Melee are the only ones with a reason to leave the pile, so they get the tighter radius and
    // everyone else keeps the box backstop - their anchor already holds them at 10-14 yd.
    bool const melee = !botAI->IsTank(bot) && !botAI->IsRanged(bot);
    float const leash = melee ? ULDUAR_THORIM_ARENA_MELEE_LEASH : ULDUAR_THORIM_ARENA_LEASH_RADIUS;

    return distance > leash || !ThorimInArenaBox(bot);
}

namespace
{

// Drawn from the latched arena squad in group roster order, never from the survivors: bots die in
// here, and ranking by who is still standing means one death renumbers everyone behind the corpse and
// the whole formation shuffles mid-fight.
//
// Shared by the ring and by the Charge Orb dodge, so the two can never disagree about who is slot 3.
bool ThorimArenaRingOrder(PlayerbotAI* botAI, Player* bot, size_t& slot, size_t& total)
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
        if (!memberAI || !memberAI->IsRanged(member) || memberAI->IsTank(member))
            continue;

        ringMembers.push_back(member);
    }

    if (ringMembers.empty())
        return false;

    // Ranged dps ahead of healers, ties on guid. Both keys read the same on every bot, so nobody has
    // to be told which slot is theirs.
    std::sort(ringMembers.begin(), ringMembers.end(), [](Player* left, Player* right)
    {
        bool const leftDps = GET_PLAYERBOT_AI(left)->IsRangedDps(left);
        bool const rightDps = GET_PLAYERBOT_AI(right)->IsRangedDps(right);
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

// Settles a computed arena destination onto ground the bot can reach, and says whether it is still a
// usable spot. Raw ring geometry is exactly the shape that lands off the navmesh, and MoveTo would
// then fail without telling anyone.
//
// False means "use the centre instead". The collision walk drags the destination back towards the
// bot, so a bot standing outside the pit - at the gate, or up on the north rim - gets handed its own
// position. That spot then latches as "arrived", which frees the anchor guard to stop every mover,
// while still being outside the leash, which stops the chase and the target pick. The bot never acts
// again.
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

bool GetThorimArenaRingSlot(PlayerbotAI* botAI, Player* bot, Position& out)
{
    size_t slot = 0;
    size_t total = 0;
    if (!ThorimArenaRingOrder(botAI, bot, slot, total))
        return false;

    // Bearing from the gate to the centre, so slot 0 lands on the far side of the room from the
    // corridor rather than in its mouth. Two fixed points, so the layout never rotates.
    Position const& centre = ULDUAR_THORIM_NEAR_ARENA_CENTER;
    Position const& gate = ULDUAR_THORIM_NEAR_ENTRANCE_POSITION;
    float const baseAngle = std::atan2(centre.GetPositionY() - gate.GetPositionY(),
                                       centre.GetPositionX() - gate.GetPositionX());

    size_t const inner = std::min<size_t>(ULDUAR_THORIM_ARENA_RING_INNER_SLOTS, total);

    // Nobody stands on the centre itself: that is the tank's spot and the pile of adds on him.
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

// Writes the nearest spot clear of the charged Thunder Orb, or returns false when `from` is already
// clear and there is nothing to do.
//
// Straight out from the orb rather than across the room: the shortest way out costs the fewest casts,
// and it keeps each bot's bearing, so the ring comes out of this still spread.
bool ThorimPushOutOfOrbField(PlayerbotAI* botAI, Player* bot, Position const& from, Position& out)
{
    Unit* orb = ThorimChargedThunderOrb(botAI, SPELL_THORIM_CHARGE_ORB);
    if (!orb)
        return false;

    // Measured in 2D on purpose. Lightning Shock's 35 yd is a 3D radius from an orb hanging 13.5 yd
    // overhead, so what has to be cleared on the floor is the circle underneath it, not the sphere.
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

    // Every orb is 42 yd from the centre, so the centre clears the field by 5.7 yd and is always a
    // usable fallback - both for a spot the leash rejects and for one the collision walk dragged back
    // inside the field on the way into a wall.
    bool safe = ThorimSettleArenaPoint(bot, spot);
    if (safe && spot.GetExactDist2d(orb->GetPositionX(), orb->GetPositionY()) < ULDUAR_THORIM_CHARGED_ORB_RADIUS)
        safe = false;

    out = safe ? spot : centre;
    return true;
}

}  // namespace

bool GetThorimArenaAnchor(PlayerbotAI* botAI, Player* bot, Position& out)
{
    if (!botAI || !bot)
        return false;

    if (GetThorimSquad(botAI, bot) != ThorimSquad::Arena)
        return false;

    Unit* boss = GetThorim(botAI);
    if (!boss || !boss->IsAlive() || !boss->IsHostileTo(bot))
        return false;

    // Phase 1 only. Once he drops to the floor the phase 2 ring owns the room.
    if (boss->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
        return false;

    if (botAI->IsTank(bot))
    {
        out = ULDUAR_THORIM_NEAR_ARENA_CENTER;
    }
    else if (botAI->IsRanged(bot))
    {
        if (!GetThorimArenaRingSlot(botAI, bot, out))
            return false;
    }
    // Melee form up on the tank spot before the pull and run free once the fight starts: adds land up
    // to 24 yd out and pinning melee would cost every one of those swings. The tighter melee leash is
    // what keeps them near the pile instead.
    else if (botAI->GetState() == BOT_STATE_COMBAT)
    {
        return false;
    }
    else
    {
        out = ULDUAR_THORIM_NEAR_ARENA_CENTER;
    }

    // The anchor is cleared of the orb field, not just the bot: the field burns for 15s, and an anchor
    // left inside it walks the bot straight back in the moment the dodge lets go.
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

    // Arriving is what lets the anchor guard switch the movers off, so a spot the leash would reject
    // must never count as arrival - otherwise the bot is frozen and out of bounds at the same time,
    // and nothing left running can fix either half.
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

    // Phase 1 only. Charge Orb stops firing once Thorim drops to the floor, and the orb that lights up
    // after that is Lightning Charge, which has its own node and its own answer.
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

    // Deadbanded, because the escape point is derived from the bot's own drifting coordinates: latched
    // at full precision it would note a new destination on every tick of the walk.
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

    // He fights the whole gauntlet from his balcony and only drops to the arena floor for phase 2.
    return boss->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD;
}

bool ThorimPhase2ElapsedMs(PlayerbotAI* botAI, uint32& elapsedMs)
{
    if (!ThorimPhase2Active(botAI))
        return false;

    Unit* boss = GetThorim(botAI);
    if (!boss || !boss->IsInCombat())
        return false;

    // Write once, so a second ask this tick reads the same clock. Never 0, since 0 means not started.
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

    // Latched here, where the "go" answer shows up. Only ever adds, so a second ask this tick agrees.
    state.openingReleased.insert(bot->GetGUID());
    return false;
}

bool ThorimBalconyHoldSpot(PlayerbotAI* botAI, Player* bot, Position& out)
{
    if (!botAI || !bot || !TakesRangedSpot(bot) || GetThorimSquad(botAI, bot) != ThorimSquad::Gauntlet)
        return false;

    // Nothing to hand out once the wait is over for everyone. Also keeps a bot that only gets up here
    // late from being given a slot it never uses.
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

// The arena squad's side of the same wait, on the floor.
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

    // Whoever he is swinging at owns the anchor, ahead of anything the group ordering says.
    // GetMainTankGuid reads the raid frame's main tank flag without a tank check, so a human wearing it
    // takes that slot and the two bot tanks fall to assist index 0 and index 1 - and index 1 used to end
    // up in the melee ring, which is a point measured off the boss. That is a tank orbiting him at 8 yd
    // instead of walking him anywhere, and it left him 26 yd off the anchor for a whole pull.
    if (PlayerbotAI::IsTank(bot))
    {
        Unit* boss = GetThorim(botAI);
        if (boss && boss->GetVictim() == bot)
            return ThorimPhase2Role::MainTank;
    }

    if (PlayerbotAI::IsMainTank(bot))
        return ThorimPhase2Role::MainTank;

    if (PlayerbotAI::IsAssistTankOfIndex(bot, 0))
        return ThorimPhase2Role::OffTank;

    if (TakesRangedSpot(bot))
        return ThorimPhase2Role::Ranged;

    // Every other tank waits on the off-tank bearing. Never the ring, for the reason above.
    if (PlayerbotAI::IsTank(bot))
        return ThorimPhase2Role::OffTank;

    return ThorimPhase2Role::MeleeRing;
}

ThorimPhase2Role GetThorimPhase2Role(PlayerbotAI* botAI, Player* bot)
{
    ThorimPhase2Role const role = ResolveThorimPhase2Role(botAI, bot);

    // Probed here rather than at the call sites, so trigger and action cannot disagree about what was
    // decided. NoteDerived only writes when the answer changes, so a settled raid emits nothing.
    RaidObs::NoteDerived(bot, "thorim.p2role", ThorimRoleName(role));
    return role;
}

bool TryGetThorimPhase2Spot(PlayerbotAI* botAI, Player* bot, ThorimPhase2Role role, Position& position)
{
    if (!botAI || !bot)
        return false;

    // Nothing up on the balcony has a spot down here yet. There is no walkable link between the two,
    // so handing one out sends the bot back through the hallway, down the ramp and the whole corridor
    // - about 300 yards - instead of over the edge. The balcony node drops it; this waits for that.
    if (bot->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
        return false;

    if (role == ThorimPhase2Role::MainTank)
    {
        position = ULDUAR_THORIM_PHASE2_TANK_SPOT;
        return true;
    }

    Unit* boss = GetThorim(botAI);

    if (role == ThorimPhase2Role::Ranged)
    {
        EnsureRangedSlot(bot);

        uint8 slot = 0;
        if (!RangedSlotOf(bot, slot))
            return false;

        position = RangedSpot(slot);

        // No boss, no bearing, so no shelter - the spot on its own is still the right answer.
        if (!boss)
            return true;

        // Before the shelter latch, which then does not run during the wait. It picks the lit orb up
        // on its first call after, since that orb is new to it.
        Position opening;
        if (ThorimOpeningSpot(botAI, bot, opening))
        {
            position = opening;
            return true;
        }

        uint8 orbIndex = 0;
        bool const sheltered = ThorimRangedSpot(botAI, bot, boss, slot, position, orbIndex);

        // Whether a cone actually moved this bot, and off which orb. A trace otherwise only shows that
        // a camp bot walked, which is what it does when nothing is lit either.
        RaidObs::NoteDerived(bot, "thorim.shelter",
                             "slot " + std::to_string(uint32(slot)) +
                                 (sheltered ? " orb " + std::to_string(uint32(orbIndex)) : " home"));
        return true;
    }

    if (!boss)
        return false;

    if (role == ThorimPhase2Role::OffTank)
    {
        // Once the swap hands him the boss he owns the anchor, same as the main tank. The ring point
        // below is measured off the boss, so a tank towing him by it never arrives - it moves with
        // every step, which is what walked Thorim 35 yd into the ranged camp.
        if (boss->GetVictim() == bot)
        {
            position = ULDUAR_THORIM_PHASE2_TANK_SPOT;
            return true;
        }

        // Just off the main tank's bearing: inside taunt range for the Unbalancing Strike swap, and
        // deliberately not rotated for Lightning Charge - moving a tank drags the boss.
        float const bearing = Position::NormalizeOrientation(RingAnchorBearing(boss) +
                                                             ULDUAR_THORIM_OFFTANK_BEARING_OFFSET);
        if (RingPoint(bot, boss, bearing, position))
            return true;

        position = ULDUAR_THORIM_PHASE2_OFFTANK_SPOT;
        return true;
    }

    if (role != ThorimPhase2Role::MeleeRing)
        return false;

    EnsureMeleeSlot(bot);

    uint8 slot = 0;
    if (!MeleeSlotOf(bot, slot))
        return false;

    // Bearing latched, cone offset held past the orb that set it, and the boss is the only live term
    // left - so the point only moves when a new cone lands on this slot. It used to be recomputed from
    // four things that all drifted on their own, and the destination flipped between six points
    // several times a second.
    float const bearing = LatchedRingBearing(bot, boss, slot);

    float offset = 0.0f;
    LightningChargeOffset(botAI, bot, boss, bearing, offset);

    // Off the cone-adjusted bearing rather than the latched one, so the two slides compose instead of
    // the second one undoing the first.
    float blizzard = 0.0f;
    BlizzardRingOffset(botAI, bot, boss, Position::NormalizeOrientation(bearing + offset), blizzard);

    // Whole degrees, so a ring that is holding writes one line for the phase. The point flipped between
    // two bearings 144 degrees apart with the boss stationary and no orb lit, which none of the three
    // terms below should allow, and ringBearings is the one of them that is not otherwise traced.
    RaidObs::NoteDerived(bot, "thorim.ringspot",
                         "slot " + std::to_string(uint32(slot)) + " bearing " +
                             std::to_string(int32(bearing * 180.0f / float(M_PI))) + " offset " +
                             std::to_string(int32(offset * 180.0f / float(M_PI))) + " blizzard " +
                             std::to_string(int32(blizzard * 180.0f / float(M_PI))));

    if (RingPoint(bot, boss, Position::NormalizeOrientation(bearing + offset + blizzard), position))
        return true;

    return StaticMeleeSpot(boss, slot, position);
}

bool ThorimRingWantsMove(PlayerbotAI* botAI, Player* bot, Position const& spot)
{
    if (!bot)
        return false;

    ThorimEncounterState const* state = FindState(bot);

    // Wider once arrived, so a ring recomputed off a moving boss does not have the bot sliding in
    // place. A moving bot casts nothing.
    float const tolerance = state && state->ringArrived.count(bot->GetGUID())
                                ? ULDUAR_THORIM_RING_REPOSITION_TOLERANCE
                                : ULDUAR_THORIM_RING_ARRIVE_TOLERANCE;

    // Note: the escape is ored on outside the tolerance, not nested in the arrived branch. Nested, a
    // bot standing in fire gets one move order and then re-latches on the next tick because it is
    // still inside the 3 yd arrive test - so a refused order is never retried, and 40-60% of them
    // come back refused.
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

// 2D distance from a point to the walk between two positions, ends included. Own name, so it cannot
// clash with Hodir's file static copy in a unity build.
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

    // Copied, since the lookups below go back through the shared state.
    std::vector<Position> const zones = ThorimBlizzardSpots(bot);
    if (zones.empty())
        return false;

    std::vector<HazardCircle> circles;
    circles.reserve(zones.size());
    for (Position const& zone : zones)
        circles.emplace_back(zone, ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE);

    // Toward home or the shelter, so the walk back is short once the zone expires.
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
                                                             2.0f, static_cast<float>(M_PI) / 8.0f, &spot, accept);
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

    // Same floor test TryGetThorimPhase2Spot uses to refuse a spot: a bot still up on the balcony is
    // not settled in a ring it cannot be standing in. The guard this feeds zeroes nearly every mover,
    // so a stale latch up there froze two bots on the hallway for three minutes.
    if (bot->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
        return false;

    ThorimEncounterState const* state = FindState(bot);
    return state && state->ringArrived.count(bot->GetGUID()) > 0;
}

Unit* ThorimChargedThunderOrb(PlayerbotAI* botAI, uint32 markerSpell)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot)
        return nullptr;

    // One 150 yd grid sweep per instance per interval, shared by every bot, so no two of them
    // disagree about which orb is lit. A slot each, because a single one keyed on the marker turns
    // into a rescan on every call the moment both markers are being asked for.
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

    // The orbs are non-attackable pillar props, so they never appear in the target values.
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

    // Written once. Clearing it before the sweep and putting it back after emitted a pair of notes
    // every rescan even when the answer had not moved.
    cachedGuid = found ? found->GetGUID() : ObjectGuid::Empty;
    return found;
}

bool ThorimEncounterStateIsStale(PlayerbotAI* botAI)
{
    // The gate used to come free: GetThorim was sight-limited, so a raid parked on Hodir never found
    // him. It is not any more, and without this that raid reads an idle full-health Thorim as a reset
    // on every tick.
    if (!NearThorimEncounter(botAI ? botAI->GetBot() : nullptr))
        return false;

    Unit* boss = GetThorim(botAI);
    if (!boss)
        return false;

    ThorimEncounterState* state = FindState(botAI->GetBot());
    if (!state)
        return false;

    // Full health alone is a trap: he watches the whole gauntlet from his balcony untouched.
    // JustEngagedWith fires at the pull, so combat state is what separates the two.
    if (boss->GetHealth() < boss->GetMaxHealth() || boss->IsInCombat())
    {
        state->engagedSeen = true;

        // A live pull is what arms the next round of resets. Nobody clears anything while he is up,
        // so this is empty on all but the first tick after a wipe.
        state->resetDone.clear();

        // Still up on the balcony means phase 2 has not started this pull, whatever the last one left.
        if (boss->GetPositionZ() >= ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
            state->phase2StartMs = 0;

        return false;
    }

    // Idle before the raid has ever pulled him is the gate, not a reset. Reading it as one clears the
    // squad split the corridor just formed and the next tick forms it again: 178 rounds of it during a
    // single Hodir pull, which is where this was found.
    if (!state->engagedSeen)
        return false;

    // Per bot, not raid-wide. Every member has its own latches to drop and each has to get a turn.
    return state->resetDone.count(botAI->GetBot()->GetGUID()) == 0;
}

bool ThorimBotHasEncounterState(Player* bot)
{
    if (!bot)
        return false;

    std::lock_guard<std::mutex> guard(thorimStatesMutex);

    auto const itr = thorimStates.find(bot->GetInstanceId());
    if (itr == thorimStates.end())
        return false;

    ThorimEncounterState const* state = &itr->second;

    // Everything the reset below drops. Leave one out and a bot holding only that never trips the
    // trigger, which is how balconyStep got to ride into the next pull unnoticed.
    return state->meleeSlots.count(bot->GetGUID()) || state->ringArrived.count(bot->GetGUID()) ||
           state->barrierBailing.count(bot->GetGUID()) || state->squads.count(bot->GetGUID()) ||
           state->followMasterStripped.count(bot->GetGUID()) ||
           state->arenaAnchorArrived.count(bot->GetGUID()) || state->balconyStep.count(bot->GetGUID()) ||
           state->ringBearings.count(bot->GetGUID()) || state->ringOffsets.count(bot->GetGUID()) ||
           state->orbEscapes.count(bot->GetGUID()) || state->dpsTargets.count(bot->GetGUID()) ||
           state->petRecalls.count(bot->GetGUID()) || state->openingSlots.count(bot->GetGUID()) ||
           state->balconyHoldSlots.count(bot->GetGUID()) || state->openingReleased.count(bot->GetGUID()) ||
           state->runicSmashSide;
}

void ResetThorimEncounterState(Player* bot, bool clearInstance)
{
    if (!bot)
        return;

    // Held across the whole reset, not just the lookup: clearInstance drops the entry other threads
    // hold pointers into, and nothing below reaches back through FindState to re-lock.
    std::lock_guard<std::mutex> guard(thorimStatesMutex);

    if (clearInstance)
    {
        thorimStates.erase(bot->GetInstanceId());
        return;
    }

    auto const itr = thorimStates.find(bot->GetInstanceId());
    if (itr == thorimStates.end())
        return;

    ThorimEncounterState* state = &itr->second;

    // Two halves, and they run on different schedules. This one is this bot's own latches and it has
    // to run for every member: they are what decide where a bot thinks it already walked to.
    state->meleeSlots.erase(bot->GetGUID());
    state->ringArrived.erase(bot->GetGUID());
    state->barrierBailing.erase(bot->GetGUID());
    state->followMasterStripped.erase(bot->GetGUID());
    state->arenaAnchorArrived.erase(bot->GetGUID());
    state->orbEscapes.erase(bot->GetGUID());
    state->petRecalls.erase(bot->GetGUID());
    state->balconyStep.erase(bot->GetGUID());
    state->dpsTargets.erase(bot->GetGUID());
    state->ringBearings.erase(bot->GetGUID());
    state->ringOffsets.erase(bot->GetGUID());
    state->blizzardOffsets.erase(bot->GetGUID());
    state->rangedSlots.erase(bot->GetGUID());
    state->rangedShelters.erase(bot->GetGUID());
    state->openingSlots.erase(bot->GetGUID());
    state->balconyHoldSlots.erase(bot->GetGUID());
    state->openingReleased.erase(bot->GetGUID());

    // Per pet rather than clearing the map: the rest of it belongs to the other bots in the instance,
    // who are not resetting.
    for (Unit* pet : bot->m_Controlled)
        if (pet)
            state->petRecallMs.erase(pet->GetGUID());

    // Marks this bot done for the cycle. ThorimEncounterStateIsStale reads it, and wipes it the next
    // time he is in combat.
    bool const firstThisCycle = state->resetDone.empty();
    state->resetDone.insert(bot->GetGUID());
    if (!firstThisCycle)
        return;

    // The other half is one answer for the whole instance - the lane the squad walks, the split, the
    // scan caches - so it goes with whichever bot notices first. Running it per bot would clear the
    // squad split 25 times and AssignThorimSquads would re-form it 25 times behind us.
    state->runicSmashSide = 0;
    state->runicSmashSeenMs = 0;
    state->smashScanMs = 0;
    state->gauntletTraced = false;
    state->chargedOrbGuid = ObjectGuid::Empty;
    state->orbScanMs = 0;
    state->lightningOrbGuid = ObjectGuid::Empty;
    state->lightningOrbScanMs = 0;
    state->ringOffsets.clear();
    state->blizzardOffsets.clear();
    state->rangedShelters.clear();
    state->blizzardSpots.clear();
    state->blizzardScanMs = 0;
    state->bossGuid.Clear();
    state->colossusGuid.Clear();
    state->colossusScanMs = 0;
    state->runeGiantGuid.Clear();
    state->runeGiantScanMs = 0;
    state->squads.clear();
    state->squadsAssigned = false;
    state->squadsNoted = false;
    state->humanSquadScanMs = 0;
    state->marksCleared = false;
    state->phase2StartMs = 0;
    state->openingReleased.clear();
}
