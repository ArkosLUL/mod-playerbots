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
const Position ULDUAR_THORIM_PHASE2_TANK_SPOT = Position(2134.8572f, -287.0291f, 419.4935f);
const Position ULDUAR_THORIM_PHASE2_RANGE1_SPOT = Position(2112.8752f, -267.69305f, 419.52814f);
const Position ULDUAR_THORIM_PHASE2_RANGE2_SPOT = Position(2134.1296f, -257.3316f, 419.8462f);
const Position ULDUAR_THORIM_PHASE2_RANGE3_SPOT = Position(2156.798f, -267.57434f, 419.52722f);
const Position ULDUAR_THORIM_PHASE2_MELEE1_SPOT = Position(2142.9f, -278.0f, 419.64f);
const Position ULDUAR_THORIM_PHASE2_MELEE2_SPOT = Position(2134.9f, -270.0f, 419.85f);
const Position ULDUAR_THORIM_PHASE2_MELEE3_SPOT = Position(2126.9f, -278.0f, 419.64f);
const Position ULDUAR_THORIM_PHASE2_OFFTANK_SPOT = Position(2137.9f, -287.0f, 419.48f);

namespace
{

// One map per map-update thread, keyed by instance: a bot is only ever updated from its own map's
// thread, so this needs no lock. Trigger, action and multiplier each hold their own helper instance,
// so the state they must agree on is defined here and nowhere else.
thread_local std::unordered_map<uint32, ThorimEncounterState> thorimStates;

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
    // A third tank has nothing to hold and is left in the ring rather than stacked on the other two.
    return member && !TakesRangedSpot(member) && !PlayerbotAI::IsMainTank(member) &&
           !PlayerbotAI::IsAssistTankOfIndex(member, 0);
}

// Least-loaded slot rather than first-free, so six melee land three-and-three instead of piling onto
// one bearing once the ring is full.
void EnsureMeleeSlot(Player* bot)
{
    ThorimEncounterState& state = thorimStates[bot->GetInstanceId()];

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
        if (MemberCounts(member, instanceId) && TakesMeleeSlot(member))
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
    ThorimEncounterState& state = thorimStates[bot->GetInstanceId()];
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
    ThorimEncounterState& state = thorimStates[bot->GetInstanceId()];
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

// Anchoring the ring on the bearing to the tank stops it rotating as the boss shuffles, so a
// recompute does not shuffle everyone. With no living main tank the static tank spot stands in,
// rather than letting the ring spin the instant a tank dies.
float RingAnchorBearing(PlayerbotAI* botAI, Player* bot, Unit* boss)
{
    Position anchor = ULDUAR_THORIM_PHASE2_TANK_SPOT;
    if (Player* mainTank = GetGroupMainTank(bot))
        if (mainTank->IsAlive())
            anchor = mainTank->GetPosition();

    return std::atan2(anchor.GetPositionY() - boss->GetPositionY(), anchor.GetPositionX() - boss->GetPositionX());
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

// Smallest rotation of the whole ring that clears every occupied slot out of the cone. Rigid, never
// per-bot: letting each slot take its own shortest way out swings the two on opposite edges toward
// each other, which trades a Lightning Charge death for a Chain Lightning one.
bool RingRotation(PlayerbotAI* botAI, Player* bot, Unit* boss, float& rotation)
{
    rotation = 0.0f;

    Unit* orb = ThorimChargedThunderOrb(botAI, SPELL_THORIM_LIGHTNING_ORB_VISUAL);
    if (!orb)
        return false;

    ThorimEncounterState const* state = FindState(bot);
    if (!state)
        return false;

    std::vector<uint8> occupied;
    for (auto const& assignment : state->meleeSlots)
        if (assignment.second < ULDUAR_THORIM_MELEE_SLOTS &&
            std::find(occupied.begin(), occupied.end(), assignment.second) == occupied.end())
            occupied.push_back(assignment.second);

    if (occupied.empty())
        return false;

    float const coneBearing =
        std::atan2(orb->GetPositionY() - boss->GetPositionY(), orb->GetPositionX() - boss->GetPositionX());
    float const halfWidth = ULDUAR_THORIM_LIGHTNING_CHARGE_CONE_ANGLE / 2.0f + ULDUAR_THORIM_LIGHTNING_CHARGE_MARGIN;
    float const anchor = RingAnchorBearing(botAI, bot, boss);

    auto clears = [&](float candidate)
    {
        for (uint8 slot : occupied)
            if (AbsAngleDelta(SlotBearing(anchor + candidate, slot), coneBearing) <= halfWidth)
                return false;

        return true;
    };

    if (clears(0.0f))
        return false;

    // The three slots span 180 degrees and leave the tank side open, so the 105 degree blocked arc
    // always fits somewhere and this search always terminates with an answer.
    float const step = static_cast<float>(M_PI) / 36.0f;  // 5 degrees
    for (float magnitude = step; magnitude <= static_cast<float>(M_PI) + step; magnitude += step)
    {
        if (clears(magnitude))
        {
            rotation = magnitude;
            return true;
        }

        if (clears(-magnitude))
        {
            rotation = -magnitude;
            return true;
        }
    }

    return false;
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

    ThorimEncounterState& state = thorimStates[bot->GetInstanceId()];
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
    ThorimEncounterState& state = thorimStates[bot->GetInstanceId()];
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
    ThorimEncounterState& state = thorimStates[bot->GetInstanceId()];
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

    ThorimEncounterState& state = thorimStates[bot->GetInstanceId()];
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
    ThorimEncounterState& state = thorimStates[bot->GetInstanceId()];
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

    // Runs one past the last waypoint. ULDUAR_THORIM_BALCONY_WAYPOINTS means arrived, and arrived has
    // to be a latch rather than a distance test: once the squad is on the platform it is fighting
    // Thorim and drifting off the mark, and re-anchoring it there every tick would fight "reach melee"
    // exactly the way the arena picker used to fight "dps assist".
    while (step < ULDUAR_THORIM_BALCONY_WAYPOINTS)
    {
        Position const& here = GetThorimBalconyWaypoint(step);

        // Passing the waypoint's y counts as reaching it. The hallway runs one way, so a bot shoved
        // north of a point by a knockback or by the pile is already done with it, and sending it back
        // south is how the squad ends up walking the same ground twice.
        bool const reached = bot->GetExactDist2d(&here) <= ULDUAR_THORIM_BALCONY_ARRIVE_TOLERANCE ||
                             bot->GetPositionY() > here.GetPositionY();
        if (!reached)
            break;

        ++step;
    }

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

    ThorimEncounterState& state = thorimStates[bot->GetInstanceId()];

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

    ThorimEncounterState& state = thorimStates[bot->GetInstanceId()];
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
    ThorimEncounterState& state = thorimStates[bot->GetInstanceId()];
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

    thorimStates[bot->GetInstanceId()].followMasterStripped.insert(bot->GetGUID());
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

ThorimPhase2Role GetThorimPhase2Role(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot)
        return ThorimPhase2Role::None;

    if (PlayerbotAI::IsMainTank(bot))
        return ThorimPhase2Role::MainTank;

    if (PlayerbotAI::IsAssistTankOfIndex(bot, 0))
        return ThorimPhase2Role::OffTank;

    if (TakesRangedSpot(bot))
        return ThorimPhase2Role::Ranged;

    return ThorimPhase2Role::MeleeRing;
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

    if (role == ThorimPhase2Role::Ranged)
    {
        Group* group = bot->GetGroup();
        if (!group)
            return false;

        uint32 slot = 0;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !TakesRangedSpot(member))
                continue;

            if (member->GetGUID() == bot->GetGUID())
                break;

            slot = (slot + 1) % 3;
        }

        static Position const* const rangedSpots[3] = {&ULDUAR_THORIM_PHASE2_RANGE1_SPOT,
                                                       &ULDUAR_THORIM_PHASE2_RANGE2_SPOT,
                                                       &ULDUAR_THORIM_PHASE2_RANGE3_SPOT};
        position = *rangedSpots[slot];
        return true;
    }

    Unit* boss = GetThorim(botAI);
    if (!boss)
        return false;

    if (role == ThorimPhase2Role::OffTank)
    {
        // Just off the main tank's bearing: inside taunt range for the Unbalancing Strike swap, and
        // deliberately not rotated for Lightning Charge - moving a tank drags the boss.
        float const bearing = Position::NormalizeOrientation(RingAnchorBearing(botAI, bot, boss) +
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

    float rotation = 0.0f;
    RingRotation(botAI, bot, boss, rotation);

    if (RingPoint(bot, boss, SlotBearing(RingAnchorBearing(botAI, bot, boss) + rotation, slot), position))
        return true;

    return StaticMeleeSpot(boss, slot, position);
}

bool ThorimRingNeedsMove(PlayerbotAI* botAI, Player* bot, Position const& spot)
{
    if (!bot)
        return false;

    ThorimEncounterState& state = thorimStates[bot->GetInstanceId()];
    float const distance = bot->GetDistance(spot);

    if (state.ringArrived.count(bot->GetGUID()))
    {
        if (distance <= ULDUAR_THORIM_RING_REPOSITION_TOLERANCE)
            return false;

        state.ringArrived.erase(bot->GetGUID());
        return true;
    }

    if (distance > ULDUAR_THORIM_RING_ARRIVE_TOLERANCE)
        return true;

    state.ringArrived.insert(bot->GetGUID());
    return false;
}

bool ThorimMeleeRingSettled(PlayerbotAI* botAI, Player* bot)
{
    if (!bot || GetThorimPhase2Role(botAI, bot) != ThorimPhase2Role::MeleeRing)
        return false;

    if (!ThorimPhase2Active(botAI))
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
    // disagree about which orb is lit. The two markers never overlap - Charge Orb only fires while
    // Thorim is on the balcony - so keying the cache on the marker costs nothing in practice.
    ThorimEncounterState& state = thorimStates[bot->GetInstanceId()];
    if (state.orbScanMs && state.orbScanSpell == markerSpell &&
        GetMSTimeDiffToNow(state.orbScanMs) < ULDUAR_THORIM_ENCOUNTER_SCAN_INTERVAL_MS)
    {
        Unit* cached = botAI->GetUnit(state.chargedOrbGuid);
        if (cached && cached->HasAura(markerSpell))
            return cached;

        return nullptr;
    }

    state.orbScanMs = getMSTime();
    state.orbScanSpell = markerSpell;
    state.chargedOrbGuid = ObjectGuid::Empty;

    // The orbs are non-attackable pillar props, so they never appear in the target values.
    std::list<Creature*> orbs;
    bot->GetCreatureListWithEntryInGrid(orbs, NPC_THORIM_THUNDER_ORB, ULDUAR_THORIM_LIGHTNING_CHARGE_RANGE);
    for (Creature* orb : orbs)
    {
        if (!orb || !orb->HasAura(markerSpell))
            continue;

        state.chargedOrbGuid = orb->GetGUID();
        return orb;
    }

    return nullptr;
}

bool ThorimLightningChargeActive(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !ThorimPhase2Active(botAI))
        return false;

    Unit* boss = GetThorim(botAI);
    if (!boss)
        return false;

    float rotation = 0.0f;
    return RingRotation(botAI, bot, boss, rotation);
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
        return false;
    }

    // Idle before the raid has ever pulled him is the gate, not a reset. Reading it as one clears the
    // squad split the corridor just formed and the next tick forms it again: 178 rounds of it during a
    // single Hodir pull, which is where this was found.
    return state->engagedSeen;
}

bool ThorimBotHasEncounterState(Player* bot)
{
    ThorimEncounterState const* state = FindState(bot);
    if (!state)
        return false;

    return state->meleeSlots.count(bot->GetGUID()) || state->ringArrived.count(bot->GetGUID()) ||
           state->barrierBailing.count(bot->GetGUID()) || state->squads.count(bot->GetGUID()) ||
           state->followMasterStripped.count(bot->GetGUID()) ||
           state->arenaAnchorArrived.count(bot->GetGUID()) || state->runicSmashSide;
}

void ResetThorimEncounterState(Player* bot, bool clearInstance)
{
    if (!bot)
        return;

    if (clearInstance)
    {
        thorimStates.erase(bot->GetInstanceId());
        return;
    }

    ThorimEncounterState* state = FindState(bot);
    if (!state)
        return;

    state->meleeSlots.erase(bot->GetGUID());
    state->ringArrived.erase(bot->GetGUID());
    state->barrierBailing.erase(bot->GetGUID());
    state->followMasterStripped.erase(bot->GetGUID());
    state->arenaAnchorArrived.erase(bot->GetGUID());

    // The lane preference is raid-wide, so it goes with the first bot that notices the encounter is
    // back at its start rather than surviving into the next pull.
    state->runicSmashSide = 0;
    state->runicSmashSeenMs = 0;
    state->smashScanMs = 0;
    state->gauntletTraced = false;
    state->chargedOrbGuid = ObjectGuid::Empty;
    state->orbScanMs = 0;
    state->orbScanSpell = 0;
    state->orbEscapes.erase(bot->GetGUID());
    state->petRecalls.erase(bot->GetGUID());

    // Per pet rather than clearing the map: the rest of it belongs to the other bots in the instance,
    // who are not resetting.
    for (Unit* pet : bot->m_Controlled)
        if (pet)
            state->petRecallMs.erase(pet->GetGUID());
    state->bossGuid.Clear();
    state->colossusGuid.Clear();
    state->colossusScanMs = 0;
    state->runeGiantGuid.Clear();
    state->runeGiantScanMs = 0;
    state->balconyStep.erase(bot->GetGUID());

    // Raid-wide too, and it has to go together with the flag or the next pull reuses the old split.
    state->squads.clear();
    state->squadsAssigned = false;
    state->squadsNoted = false;
    state->humanSquadScanMs = 0;
    state->marksCleared = false;
    state->dpsTargets.erase(bot->GetGUID());

    // Back to "never pulled him", so the state this just cleared does not read as stale all over again
    // on the next tick.
    state->engagedSeen = false;
}
