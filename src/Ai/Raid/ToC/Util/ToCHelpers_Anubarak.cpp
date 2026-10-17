#include "ToCHelpers_Anubarak.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <list>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Playerbots.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Shared.h"
#include "Unit.h"

using namespace EncounterHelpers;

namespace TrialOfTheCrusaderHelpers
{
const Position ANUBARAK_ROOM_CENTER = {745.0f, 135.0f, 142.6f};
}

namespace
{
using namespace TrialOfTheCrusaderHelpers;

// Some bot reads the phase every tick of a pull, so a gap this long means the last pull ended unseen
constexpr uint32 ANUBARAK_PULL_GAP_MS = 5000;
// HealthBelowPct(30) in the script
constexpr float ANUBARAK_SWARM_HEALTH_PCT = 30.0f;
// How far ValidateFloorPoint may pull a stand back before it counts as off the floor
constexpr float ANUBARAK_FLOOR_XY_TOLERANCE = 1.5f;
constexpr float ANUBARAK_FLOOR_Z_TOLERANCE = 3.0f;
// A stand turned 40 deg round its patch moves at most 2 * 8.5 * sin(20 deg)
constexpr float ANUBARAK_KITE_ROTATION_SLACK = 5.9f;
// Detour corners sit this far outside the slow reach, so the leg along a square side stays clear
constexpr float ANUBARAK_DETOUR_MARGIN = 0.5f;
constexpr float ANUBARAK_PATH_CLIP_SLACK = 0.1f;
constexpr float ANUBARAK_DEG_TO_RAD = static_cast<float>(M_PI) / 180.0f;

constexpr float ANUBARAK_SCARAB_PICKUP_RANGE = 30.0f;
constexpr float ANUBARAK_PENETRATING_COLD_HEAL_PCT = 90.0f;

// The floor is about 60 yd round the room centre. The Web Door is 83 yd out, and anyone who never
// landed is still up near z 395.
constexpr float ANUBARAK_PIT_RADIUS = 60.0f;
constexpr float ANUBARAK_PIT_CEILING_Z = 200.0f;
constexpr float ANUBARAK_SUBMERGE_SPOT_SEARCH = 30.0f;

// His chase stops on the 400 ms recheck after the tank does, half of it on average
constexpr float ANUBARAK_DRAG_STOP_LAG_S = 0.2f;
constexpr float ANUBARAK_DRAG_SPOT_ARRIVE = 1.0f;
// isMoving can miss a short step between two reads, so a still boss that drifts this far moved
constexpr float ANUBARAK_STILL_DRIFT = 0.3f;

// A patch landing this close to the spike, or to him before it spawns on him, burns before a kite can use it
constexpr float ANUBARAK_STOCK_SPIKE_CLEARANCE = ANUBARAK_SUBMERGE_PATCH_CLEARANCE_MIN;
// The stand sits this far inside the reach, so a drifting sphere doesn't walk the shooter every tick
constexpr float ANUBARAK_SPHERE_STAND_SLACK = 1.0f;

// A claimed protector with no immunity on the kiter by then passes the claim on
constexpr uint32 ANUBARAK_PROTECT_CLAIM_MS = 1500;

std::vector<uint32> const SWEEP_ENTRIES = {
    static_cast<uint32>(ToCNpcs::NPC_FROST_SPHERE),
    static_cast<uint32>(ToCNpcs::NPC_PURSUING_SPIKE),
    static_cast<uint32>(ToCNpcs::NPC_NERUBIAN_BURROWER),
    static_cast<uint32>(ToCNpcs::NPC_SWARM_SCARAB),
};

struct SphereDuty
{
    ObjectGuid shooter;
    ObjectGuid sphere;
    char const* label = "none";
};

struct StrikeDuty
{
    ObjectGuid caster;
    ObjectGuid interrupter;
};

struct DragLegLatch
{
    bool valid = false;
    ObjectGuid tank;
    Position point;
    Position tankSpot;
    uint32 plannedMs = 0;
    // He was planned on standing still, so only a stop after moving means he followed
    bool bossMoved = false;
    bool arrived = false;
    uint32 arrivedMs = 0;
};

struct DodgeHold
{
    Position spot;
    ObjectGuid kiter;
    uint32 setMs = 0;
    // A lane dodge can start further out than the release distance, so it waits for the spike to recede
    // past where it was
    float spikeDistAtSet = 0.0f;
};

struct ThreatCapLatch
{
    bool capped = false;
    bool memoValid = false;
    uint32 memoMs = 0;
    bool value = false;
};

struct ProtectClaim
{
    bool memoValid = false;
    uint32 memoMs = 0;
    ObjectGuid kiter;
    ObjectGuid protector;
    uint32 claimMs = 0;
    // Paladins whose claim lapsed this mark window
    std::vector<ObjectGuid> passed;
    bool need = false;
    bool spikeInImmunityRange = false;
};

struct AnubarakState
{
    RaidObs::ObsValue<uint32> phase{"anub.phase"};
    RaidObs::ObsValue<uint32> patches{"anub.patches"};
    RaidObs::ObsValue<uint32> flying{"anub.flying"};
    RaidObs::ObsValue<ObjectGuid> patch0{"anub.patch0"};
    RaidObs::ObsValue<ObjectGuid> patch1{"anub.patch1"};
    RaidObs::ObsValue<ObjectGuid> focusBurrower{"anub.focus"};

    // 0 is a real getMSTime value, hence the valid flags
    bool engagedSeen = false;
    uint32 lastEngagedMs = 0;
    bool swarm = false;
    // The pull or the last emerge: the script submerges him 80 s after either
    uint32 surfacedMs = 0;
    bool submergeSpotValid = false;
    Position submergeSpot;
    ObjectGuid mainTank;
    ObjectGuid sideTanks[2];
    // Last marked player seen, so its anub.kite closes when the mark moves on
    ObjectGuid kiter;

    bool phaseMemoValid = false;
    uint32 phaseMemoMs = 0;
    AnubarakPhase phaseValue = AnubarakPhase::None;

    bool sweepMemoValid = false;
    uint32 sweepMemoMs = 0;
    std::vector<ObjectGuid> spheres;
    std::vector<ObjectGuid> burrowers;
    std::vector<ObjectGuid> scarabs;
    ObjectGuid spikeGuid;

    bool dutyMemoValid = false;
    uint32 dutyMemoMs = 0;
    ObjectGuid dutyGroup;
    std::vector<SphereDuty> duties;

    bool strikeMemoValid = false;
    uint32 strikeMemoMs = 0;
    ObjectGuid strikeGroup;
    std::vector<StrikeDuty> strikes;

    bool bossStillValid = false;
    uint32 bossStillMs = 0;
    Position bossStillPos;
    DragLegLatch drag;
    float submergeSpotClearance = 0.0f;

    std::unordered_map<ObjectGuid, DodgeHold> dodgeHolds;
    std::unordered_map<ObjectGuid, ThreatCapLatch> threatCaps;
    ProtectClaim protect;
};

RaidInstanceState<AnubarakState> anubarakStates;

struct Vec2
{
    float x;
    float y;
};

Vec2 Delta(Position const& from, Position const& to)
{
    return {to.GetPositionX() - from.GetPositionX(), to.GetPositionY() - from.GetPositionY()};
}

float Length(Vec2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }

Vec2 Rotate(Vec2 v, float angle)
{
    float const c = std::cos(angle);
    float const s = std::sin(angle);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}

Position Offset(Position const& from, Vec2 dir, float distance)
{
    return Position(from.GetPositionX() + dir.x * distance, from.GetPositionY() + dir.y * distance,
                    from.GetPositionZ());
}

float Dist2d(Position const& a, Position const& b) { return a.GetExactDist2d(b.GetPositionX(), b.GetPositionY()); }

float SegmentDistance2d(Position const& a, Position const& b, Position const& point)
{
    Vec2 const ab = Delta(a, b);
    Vec2 const ap = Delta(a, point);
    float const lengthSq = ab.x * ab.x + ab.y * ab.y;
    float t = lengthSq > 0.0f ? (ap.x * ab.x + ap.y * ab.y) / lengthSq : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    return point.GetExactDist2d(a.GetPositionX() + ab.x * t, a.GetPositionY() + ab.y * t);
}

float AngleBetween(Vec2 a, Vec2 b)
{
    float const diff = std::atan2(b.y, b.x) - std::atan2(a.y, a.x);
    return std::fabs(std::remainder(diff, 2.0f * static_cast<float>(M_PI)));
}

bool InToCInstance(Player* bot)
{
    return bot && bot->IsInWorld() && bot->GetMapId() == TRIAL_OF_THE_CRUSADER_MAP_ID && bot->GetInstanceId();
}

bool AnubarakHeroic(Player* bot)
{
    Map* map = bot->GetMap();
    return map && map->IsHeroic();
}

Creature* ResolveCreature(Player* bot, ObjectGuid guid)
{
    if (guid.IsEmpty())
        return nullptr;

    Map* map = bot->GetMap();
    return map ? map->GetCreature(guid) : nullptr;
}

// fn returns true to stop. No group: the bot alone.
template <typename Fn>
void ForEachGroupMember(Player* bot, Fn&& fn)
{
    Group* group = bot->GetGroup();
    if (!group)
    {
        fn(bot);
        return;
    }

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsInMap(bot) && fn(member))
            return;
    }
}

// One grid visit per instance per ms. From the boss when he resolves, so a bot that never landed in
// the pit can't blank it for everyone.
AnubarakState* Sweep(Player* bot)
{
    if (!InToCInstance(bot))
        return nullptr;

    AnubarakState& state = anubarakStates.For(bot->GetInstanceId());
    uint32 const now = getMSTime();
    if (state.sweepMemoValid && state.sweepMemoMs == now)
        return &state;

    state.sweepMemoValid = true;
    state.sweepMemoMs = now;
    state.spheres.clear();
    state.burrowers.clear();
    state.scarabs.clear();
    state.spikeGuid.Clear();

    WorldObject* origin = GetAnubarak(bot);
    if (!origin)
        origin = bot;

    std::list<Creature*> creatures;
    origin->GetCreatureListWithEntryInGrid(creatures, SWEEP_ENTRIES, ANUBARAK_ROOM_RADIUS);
    for (Creature* creature : creatures)
    {
        if (!creature->IsAlive())
            continue;

        switch (static_cast<ToCNpcs>(creature->GetEntry()))
        {
            case ToCNpcs::NPC_FROST_SPHERE:
                state.spheres.push_back(creature->GetGUID());
                break;
            case ToCNpcs::NPC_NERUBIAN_BURROWER:
                state.burrowers.push_back(creature->GetGUID());
                break;
            case ToCNpcs::NPC_SWARM_SCARAB:
                state.scarabs.push_back(creature->GetGUID());
                break;
            case ToCNpcs::NPC_PURSUING_SPIKE:
                state.spikeGuid = creature->GetGUID();
                break;
            default:
                break;
        }
    }

    return &state;
}

std::vector<Creature*> ResolveAlive(Player* bot, std::vector<ObjectGuid> const& guids)
{
    std::vector<Creature*> creatures;
    creatures.reserve(guids.size());
    for (ObjectGuid const& guid : guids)
        if (Creature* creature = ResolveCreature(bot, guid))
            if (creature->IsAlive())
                creatures.push_back(creature);

    return creatures;
}

std::vector<Creature*> ResolveSpheres(Player* bot, AnubarakState const& state)
{
    return ResolveAlive(bot, state.spheres);
}

Creature* FindSphere(std::vector<Creature*> const& spheres, ObjectGuid guid)
{
    if (guid.IsEmpty())
        return nullptr;

    for (Creature* sphere : spheres)
        if (sphere->GetGUID() == guid)
            return sphere;

    return nullptr;
}

bool FallingNear(std::vector<Creature*> const& spheres, Position const& point)
{
    for (Creature* sphere : spheres)
        if (IsFrostSphereFalling(sphere) &&
            sphere->GetExactDist2d(point.GetPositionX(), point.GetPositionY()) <= ANUBARAK_TANK_PATCH_LATCH)
            return true;

    return false;
}

bool AnyBurrower(Player* bot, AnubarakState const& state) { return !ResolveAlive(bot, state.burrowers).empty(); }

// A shooter out here can neither reach nor see a sphere, and its need would never pass on
bool InAnubarakPit(Player* player)
{
    return player->GetPositionZ() < ANUBARAK_PIT_CEILING_Z &&
           Dist2d(player->GetPosition(), ANUBARAK_ROOM_CENTER) <= ANUBARAK_PIT_RADIUS;
}

void ClearPullLatches(AnubarakState& state)
{
    state.swarm = false;
    state.phaseValue = AnubarakPhase::None;
    state.phaseMemoValid = false;
    state.dutyMemoValid = false;
    state.strikeMemoValid = false;
    state.submergeSpotValid = false;
    state.sideTanks[0].Clear();
    state.sideTanks[1].Clear();
    state.patch0 = ObjectGuid::Empty;
    state.patch1 = ObjectGuid::Empty;
    state.focusBurrower = ObjectGuid::Empty;
    state.duties.clear();
    state.bossStillValid = false;
    state.drag = DragLegLatch();
    state.dodgeHolds.clear();
    state.threatCaps.clear();
    state.protect = ProtectClaim();
}

void TrackBossStillness(AnubarakState& state, Creature* boss, uint32 now)
{
    bool const drifted =
        state.bossStillValid && Dist2d(boss->GetPosition(), state.bossStillPos) > ANUBARAK_STILL_DRIFT;
    if (boss->isMoving() || drifted)
    {
        state.bossStillValid = false;
        if (state.drag.valid)
            state.drag.bossMoved = true;

        return;
    }

    if (!state.bossStillValid)
    {
        state.bossStillValid = true;
        state.bossStillMs = now;
        state.bossStillPos = boss->GetPosition();
    }
}

bool BossStillFor(AnubarakState const& state, uint32 now, uint32 ms)
{
    return state.bossStillValid && getMSTimeDiff(state.bossStillMs, now) >= ms;
}

// The explicit main tank, else the first living tank, as GetMainTankGuid picks but with the spec
// asked too, so it can't blink
ObjectGuid StableMainTankGuid(Group* group)
{
    for (Group::MemberSlot const& slot : group->GetMemberSlots())
        if (slot.flags & MEMBER_FLAG_MAINTANK)
            return slot.guid;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && IsAnubarakTankPlayer(member))
            return member->GetGUID();
    }

    return ObjectGuid::Empty;
}

// Living tanks on the map other than the main tank, in IsAssistTankOfIndex's order: assistants first
std::vector<ObjectGuid> RankSideTanks(Player* bot, Group* group, ObjectGuid const& mainTank)
{
    std::vector<ObjectGuid> roster;
    std::vector<ObjectGuid> others;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !member->IsInMap(bot) || member->GetGUID() == mainTank ||
            !IsAnubarakTankPlayer(member))
            continue;

        (group->IsAssistant(member->GetGUID()) ? roster : others).push_back(member->GetGUID());
    }
    roster.insert(roster.end(), others.begin(), others.end());
    return roster;
}

// Each side keeps its tank for the pull while it lives. Re-reading the assist tank index every tick
// swaps sides whenever IsTank blinks.
void RefreshSideTanks(Player* bot, AnubarakState& state)
{
    Group* group = bot->GetGroup();
    if (!group)
    {
        state.mainTank.Clear();
        state.sideTanks[0].Clear();
        state.sideTanks[1].Clear();
        return;
    }

    state.mainTank = StableMainTankGuid(group);
    std::vector<ObjectGuid> const roster = RankSideTanks(bot, group, state.mainTank);

    for (ObjectGuid& held : state.sideTanks)
        if (std::find(roster.begin(), roster.end(), held) == roster.end())
            held.Clear();

    for (uint8 side = 0; side < 2; ++side)
    {
        if (!state.sideTanks[side].IsEmpty())
            continue;

        for (ObjectGuid const& guid : roster)
        {
            if (guid != state.sideTanks[1 - side])
            {
                state.sideTanks[side] = guid;
                break;
            }
        }
    }
}

// anub.kite only changes while the kite runs, so the kiter's next window would open on the branch its
// last one ended on
void TrackKiter(Player* bot, AnubarakState& state, Player* target)
{
    ObjectGuid const guid = target ? target->GetGUID() : ObjectGuid::Empty;
    if (guid == state.kiter)
        return;

    if (Player* last = state.kiter.IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(*bot, state.kiter))
        if (GET_PLAYERBOT_AI(last))
            RaidObs::NoteDerived(last, "anub.kite", "none");

    state.kiter = guid;
}

// The boss only ever casts Leeching Swarm below 30% while surfaced, so any of these means phase 3
bool SwarmStarted(Player* bot, Creature* boss)
{
    if (boss->GetHealthPct() < ANUBARAK_SWARM_HEALTH_PCT)
        return true;

    uint32 const swarm = sSpellMgr->GetSpellIdForDifficulty(SPELL_LEECHING_SWARM, boss);
    if (boss->FindCurrentSpellBySpellId(swarm))
        return true;

    bool carried = false;
    ForEachGroupMember(bot, [&](Player* member)
    {
        carried = member->HasAura(swarm);
        return carried;
    });

    return carried;
}

// Holds each side's patch while it lasts. Never both sides on one patch.
void RefreshTankPatches(AnubarakState& state, std::vector<Creature*> const& spheres)
{
    ObjectGuid held[2] = {state.patch0.Get(), state.patch1.Get()};
    for (ObjectGuid& guid : held)
        if (!IsPermafrostPatch(FindSphere(spheres, guid)))
            guid.Clear();

    for (uint8 side = 0; side < 2; ++side)
    {
        if (!held[side].IsEmpty())
            continue;

        Position const point = GetAnubarakTankSidePoint(side);
        ObjectGuid const other = held[1 - side];
        Creature* nearest = nullptr;
        float nearestDist = ANUBARAK_TANK_PATCH_LATCH;
        for (Creature* sphere : spheres)
        {
            if (!IsPermafrostPatch(sphere) || sphere->GetGUID() == other)
                continue;

            float const dist = sphere->GetExactDist2d(point.GetPositionX(), point.GetPositionY());
            if (dist <= nearestDist)
            {
                nearest = sphere;
                nearestDist = dist;
            }
        }

        if (nearest)
            held[side] = nearest->GetGUID();
    }

    state.patch0 = held[0];
    state.patch1 = held[1];
}

// Null while he isn't engaged. Refreshes phase, latches and probes once per instance per ms.
AnubarakState* ReadPhase(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!InToCInstance(bot) || !AnubarakEngaged(botAI))
        return nullptr;

    Creature* boss = GetAnubarak(bot);
    AnubarakState* state = boss ? Sweep(bot) : nullptr;
    if (!state)
        return nullptr;

    uint32 const now = getMSTime();
    if (state->phaseMemoValid && state->phaseMemoMs == now)
        return state;

    // Gated triggers never read the phase while he's out of combat, so a wipe shows only as this gap
    if (!state->engagedSeen || getMSTimeDiff(state->lastEngagedMs, now) > ANUBARAK_PULL_GAP_MS)
    {
        state->engagedSeen = true;
        ClearPullLatches(*state);
    }

    state->lastEngagedMs = now;
    state->phaseMemoValid = true;
    state->phaseMemoMs = now;

    AnubarakPhase const before = state->phaseValue;
    if (boss->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) || boss->HasAura(SPELL_SUBMERGE_ANUB))
        state->phaseValue = AnubarakPhase::Submerged;
    else
    {
        if (!state->swarm && SwarmStarted(bot, boss))
            state->swarm = true;

        state->phaseValue = state->swarm ? AnubarakPhase::Swarm : AnubarakPhase::Surface;
    }

    if (state->phaseValue == AnubarakPhase::Surface &&
        (before == AnubarakPhase::None || before == AnubarakPhase::Submerged))
    {
        state->surfacedMs = now;
        state->submergeSpotValid = false;
    }

    TrackBossStillness(*state, boss, now);
    if (state->phaseValue == AnubarakPhase::Submerged)
        state->drag = DragLegLatch();

    RefreshSideTanks(bot, *state);

    std::vector<Creature*> const spheres = ResolveSpheres(bot, *state);
    RefreshTankPatches(*state, spheres);

    uint32 patchCount = 0;
    uint32 flyingCount = 0;
    for (Creature* sphere : spheres)
    {
        if (IsPermafrostPatch(sphere))
            ++patchCount;
        else if (IsFrostSphereFlying(sphere))
            ++flyingCount;
    }

    TrackKiter(bot, *state, GetSpikeTarget(bot));
    state->phase = static_cast<uint32>(state->phaseValue);
    state->patches = patchCount;
    state->flying = flyingCount;

    return state;
}

// A bot refreshes the latches through its own read. A player gets whatever the last bot read left.
AnubarakState const* LatchesFor(Player* player)
{
    if (!InToCInstance(player))
        return nullptr;

    if (PlayerbotAI* botAI = GET_PLAYERBOT_AI(player))
        return ReadPhase(botAI);

    return anubarakStates.Find(player->GetInstanceId());
}

Creature* LatchedTankPatch(Player* bot, AnubarakState const& state, uint8 side)
{
    Creature* patch = ResolveCreature(bot, side == 0 ? state.patch0.Get() : state.patch1.Get());
    return IsPermafrostPatch(patch) ? patch : nullptr;
}

bool SpikeBehind(Player* kiter, Creature* spike, Position const& stand)
{
    return spike->GetExactDist2d(stand.GetPositionX(), stand.GetPositionY()) >
           kiter->GetExactDist2d(stand.GetPositionX(), stand.GetPositionY());
}

bool OnFloor(Player* kiter, Position const& raw, Position& valid)
{
    valid = ValidateFloorPoint(kiter, raw);
    return Dist2d(valid, raw) <= ANUBARAK_FLOOR_XY_TOLERANCE &&
           std::fabs(valid.GetPositionZ() - raw.GetPositionZ()) <= ANUBARAK_FLOOR_Z_TOLERANCE;
}

// Past the patch on the far side from the spike, turned round the patch until it lands on the floor.
// The floor check may pull it back toward the kiter, never into the slow reach.
bool FloorCheckedStand(Player* kiter, Position const& patch, Vec2 away, Position& stand)
{
    static float const rotations[] = {0.0f, 20.0f, -20.0f, 40.0f, -40.0f};
    for (float degrees : rotations)
    {
        Vec2 const dir = Rotate(away, degrees * ANUBARAK_DEG_TO_RAD);
        if (OnFloor(kiter, Offset(patch, dir, ANUBARAK_KITE_STAND_OFFSET), stand) &&
            Dist2d(stand, patch) >= ANUBARAK_PERMAFROST_SLOW_REACH)
            return true;
    }

    return false;
}

// Walking from a to target cuts deeper into the patch's slow reach than either end already is
bool PathClips(Position const& a, Position const& target, Position const& patch)
{
    float const limit = std::min({ANUBARAK_PERMAFROST_SLOW_REACH + 1.0f, Dist2d(target, patch), Dist2d(a, patch)});
    return SegmentDistance2d(a, target, patch) < limit - ANUBARAK_PATH_CLIP_SLACK;
}

// Next corner of a square round the patch, its sides just outside the slow reach. Legs along a side
// never clip, so the kiter goes near corner, far corner, stand. A point straight out to the side
// won't do: the leg from there to the stand still cuts to 6 yd of the patch.
Position DetourWaypoint(Player* kiter, Position const& patch, Position const& stand)
{
    Position const from = kiter->GetPosition();
    Vec2 u = Delta(patch, stand);
    float const length = Length(u);
    u = length > 0.0f ? Vec2{u.x / length, u.y / length} : Vec2{1.0f, 0.0f};
    Vec2 const perp = {-u.y, u.x};
    Vec2 const toKiter = Delta(patch, from);
    float const kiterSide = toKiter.x * perp.x + toKiter.y * perp.y >= 0.0f ? 1.0f : -1.0f;
    float const half = ANUBARAK_PERMAFROST_SLOW_REACH + 1.0f + ANUBARAK_DETOUR_MARGIN;

    for (float side : {kiterSide, -kiterSide})
    {
        Position const across = Offset(patch, perp, side * half);
        for (float along : {half, -half})
        {
            Position const corner = Offset(across, u, along);
            Position valid;
            if (!PathClips(from, corner, patch) && OnFloor(kiter, corner, valid))
                return valid;
        }
    }

    // No clean corner: step straight out from the patch first
    Vec2 out = toKiter;
    float const outLength = Length(out);
    out = outLength > 0.0f ? Vec2{out.x / outLength, out.y / outLength} : Vec2{-u.x, -u.y};
    return ValidateFloorPoint(kiter, Offset(patch, out, half));
}

// Tangential step round the room, away from the spike's bearing
Position RingStand(Player* kiter, Creature* spike)
{
    Position const& center = ANUBARAK_ROOM_CENTER;
    Vec2 const fromCenter = Delta(center, kiter->GetPosition());
    Vec2 const bearingVec = Length(fromCenter) > 1.0f ? fromCenter : Delta(spike->GetPosition(), kiter->GetPosition());
    Vec2 const spikeVec = Delta(center, spike->GetPosition());

    float const bearing = std::atan2(bearingVec.y, bearingVec.x);
    float const spikeBearing = std::atan2(spikeVec.y, spikeVec.x);
    float const opening = std::remainder(bearing - spikeBearing, 2.0f * static_cast<float>(M_PI));
    float const step = ANUBARAK_KITE_RING_STEP_DEGREES * ANUBARAK_DEG_TO_RAD * (opening >= 0.0f ? 1.0f : -1.0f);

    Vec2 const dir = {std::cos(bearing + step), std::sin(bearing + step)};
    return ValidateFloorPoint(kiter, Offset(center, dir, ANUBARAK_KITE_RING_RADIUS));
}

struct StandCandidate
{
    Creature* patch;
    Vec2 away;
    float rawDist;
    bool tankPatch;
};

enum class StandTier : uint8
{
    Any,
    NonTank,
    // A non-tank patch whose spike route misses every tank patch too
    NonTankClear,
    Tank
};

// The spike follows the kiter, so it runs roughly spike to kiter, then kiter to the stand
bool SpikeRouteNearPatch(Position const& spike, Position const& kiter, Position const& stand,
                         std::vector<Position> const& patches)
{
    for (Position const& patch : patches)
        if (SegmentDistance2d(spike, kiter, patch) < ANUBARAK_SPIKE_PATCH_REACH ||
            SegmentDistance2d(kiter, stand, patch) < ANUBARAK_SPIKE_PATCH_REACH)
            return true;

    return false;
}

void ComputeKiteStand(Player* kiter, Creature* spike, std::vector<Creature*> const& spheres,
                      ObjectGuid const tankPatches[2], bool heroic, bool burrowers,
                      AnubarakKiteStand const* previous, AnubarakKiteStand& out)
{
    Position const spikePos = spike->GetPosition();

    std::vector<Position> tankSpots;
    for (uint8 side = 0; side < 2; ++side)
    {
        Creature* patch = FindSphere(spheres, tankPatches[side]);
        if (IsPermafrostPatch(patch))
            tankSpots.push_back(patch->GetPosition());
    }

    std::vector<StandCandidate> candidates;
    for (Creature* sphere : spheres)
    {
        if (!IsPermafrostPatch(sphere))
            continue;

        Vec2 away = Delta(spikePos, sphere->GetPosition());
        float const length = Length(away);
        // The spike is on it, so it's already being used up
        if (length < 0.5f)
            continue;

        away = {away.x / length, away.y / length};
        Position const raw = Offset(sphere->GetPosition(), away, ANUBARAK_KITE_STAND_OFFSET);
        bool const tankPatch = sphere->GetGUID() == tankPatches[0] || sphere->GetGUID() == tankPatches[1];
        candidates.push_back({sphere, away, kiter->GetExactDist2d(raw.GetPositionX(), raw.GetPositionY()), tankPatch});
    }

    Creature* chosenPatch = nullptr;
    Position chosenStand;

    if (previous && !previous->patch.IsEmpty())
    {
        auto const held = std::find_if(candidates.begin(), candidates.end(), [previous](StandCandidate const& c)
                                       { return c.patch->GetGUID() == previous->patch; });
        Position fresh;
        if (held != candidates.end() && FloorCheckedStand(kiter, held->patch->GetPosition(), held->away, fresh) &&
            SpikeBehind(kiter, spike, fresh))
        {
            Position const patchPos = held->patch->GetPosition();
            // A detour's stand is a waypoint with no bearing worth comparing, so it keeps the patch only
            if (previous->branch == AnubarakKiteBranch::Detour)
            {
                chosenPatch = held->patch;
                chosenStand = fresh;
            }
            else if ((previous->branch == AnubarakKiteBranch::Patch || previous->branch == AnubarakKiteBranch::Hold) &&
                     AngleBetween(Delta(patchPos, previous->stand), Delta(patchPos, fresh)) <=
                         ANUBARAK_KITE_REAIM_DEGREES * ANUBARAK_DEG_TO_RAD &&
                     SpikeBehind(kiter, spike, previous->stand))
            {
                chosenPatch = held->patch;
                chosenStand = previous->stand;
            }
        }
    }

    if (!chosenPatch)
    {
        std::sort(candidates.begin(), candidates.end(),
                  [](StandCandidate const& a, StandCandidate const& b) { return a.rawDist < b.rawDist; });

        // Floor checks are raycasts, so walk outward and stop once nothing further can beat the best
        auto const nearest = [&](StandTier tier)
        {
            float bestDist = 0.0f;
            for (StandCandidate const& candidate : candidates)
            {
                if (tier != StandTier::Any && candidate.tankPatch != (tier == StandTier::Tank))
                    continue;

                if (chosenPatch && candidate.rawDist > bestDist + ANUBARAK_KITE_ROTATION_SLACK)
                    break;

                Position const raw = Offset(candidate.patch->GetPosition(), candidate.away, ANUBARAK_KITE_STAND_OFFSET);
                if (spike->GetExactDist2d(raw.GetPositionX(), raw.GetPositionY()) + ANUBARAK_KITE_ROTATION_SLACK <=
                    candidate.rawDist - ANUBARAK_KITE_ROTATION_SLACK)
                    continue;

                Position stand;
                if (!FloorCheckedStand(kiter, candidate.patch->GetPosition(), candidate.away, stand) ||
                    !SpikeBehind(kiter, spike, stand))
                    continue;

                if (tier == StandTier::NonTankClear &&
                    SpikeRouteNearPatch(spikePos, kiter->GetPosition(), stand, tankSpots))
                    continue;

                float const dist = kiter->GetExactDist2d(stand.GetPositionX(), stand.GetPositionY());
                if (!chosenPatch || dist < bestDist)
                {
                    chosenPatch = candidate.patch;
                    chosenStand = stand;
                    bestDist = dist;
                }
            }
        };

        auto const firstOf = [&](std::initializer_list<StandTier> tiers)
        {
            for (StandTier tier : tiers)
                if (!chosenPatch)
                    nearest(tier);
        };

        // Tank patches last: heroic never gets spheres back, a held burrower off Permafrost submerges
        // back to full, and a stand past a tank patch puts its side tank in the spike's lane
        if (heroic)
            firstOf({StandTier::NonTankClear, StandTier::NonTank, StandTier::Tank});
        else if (burrowers)
            firstOf({StandTier::NonTank, StandTier::Tank});
        else
            firstOf({StandTier::Any});
    }

    if (!chosenPatch)
    {
        out.stand = RingStand(kiter, spike);
        out.patch.Clear();
        out.branch = AnubarakKiteBranch::Ring;
        return;
    }

    Position const patchPos = chosenPatch->GetPosition();
    out.patch = chosenPatch->GetGUID();
    if (kiter->GetExactDist2d(chosenStand.GetPositionX(), chosenStand.GetPositionY()) <= ANUBARAK_KITE_ARRIVE)
    {
        out.stand = chosenStand;
        out.branch = AnubarakKiteBranch::Hold;
    }
    else if (PathClips(kiter->GetPosition(), chosenStand, patchPos))
    {
        out.stand = DetourWaypoint(kiter, patchPos, chosenStand);
        out.branch = AnubarakKiteBranch::Detour;
    }
    else
    {
        out.stand = chosenStand;
        out.branch = AnubarakKiteBranch::Patch;
    }
}

char const* KiteBranchName(AnubarakKiteBranch branch)
{
    switch (branch)
    {
        case AnubarakKiteBranch::Patch:
            return "patch";
        case AnubarakKiteBranch::Detour:
            return "detour";
        case AnubarakKiteBranch::Ring:
            return "ring";
        case AnubarakKiteBranch::Hold:
            return "hold";
        default:
            return "none";
    }
}

// Prefers a sphere whose patch would put the kiter ahead of the spike at its stand
Creature* KiteSphere(std::vector<Creature*> const& flying, Player* target, Creature* spike)
{
    Creature* best = nullptr;
    bool bestAhead = false;
    float bestDist = 0.0f;
    for (Creature* sphere : flying)
    {
        Vec2 away = Delta(spike->GetPosition(), sphere->GetPosition());
        float const length = Length(away);
        bool ahead = false;
        if (length > 0.5f)
        {
            away = {away.x / length, away.y / length};
            ahead = SpikeBehind(target, spike, Offset(sphere->GetPosition(), away, ANUBARAK_KITE_STAND_OFFSET));
        }

        float const dist = target->GetExactDist2d(sphere->GetPositionX(), sphere->GetPositionY());
        if (!best || (ahead && !bestAhead) || (ahead == bestAhead && dist < bestDist))
        {
            best = sphere;
            bestAhead = ahead;
            bestDist = dist;
        }
    }

    return best;
}

struct SphereShooter
{
    Player* player;
    // On its dodge spot: it shoots only what it reaches from there
    bool parked;
};

template <typename Taken>
Creature* StockSphere(std::vector<Creature*> const& flying, std::vector<SphereDuty> const& previous,
                      Position const& spikeFrom, Creature* spike, Player* target, Taken const& taken)
{
    auto const qualifies = [&](Creature* sphere)
    {
        if (taken(sphere) || Dist2d(sphere->GetPosition(), spikeFrom) <= ANUBARAK_STOCK_SPIKE_CLEARANCE)
            return false;

        return !spike || !target ||
               SegmentDistance2d(spike->GetPosition(), target->GetPosition(), sphere->GetPosition()) >
                   ANUBARAK_SPIKE_LANE_CLEARANCE;
    };

    // Spheres drift, so the nearest one swaps back and forth and its shooters with it
    for (SphereDuty const& duty : previous)
        if (std::strcmp(duty.label, "stock") == 0)
            if (Creature* held = FindSphere(flying, duty.sphere))
                if (qualifies(held))
                    return held;

    Creature* best = nullptr;
    float bestDist = 0.0f;
    for (Creature* sphere : flying)
    {
        if (!qualifies(sphere))
            continue;

        float const dist = Dist2d(sphere->GetPosition(), ANUBARAK_ROOM_CENTER);
        if (!best || dist < bestDist)
        {
            best = sphere;
            bestDist = dist;
        }
    }

    return best;
}

void ComputeDuties(Player* bot, AnubarakState& state)
{
    std::vector<SphereDuty> const previous = std::move(state.duties);
    state.duties.clear();

    // Bots only: a player never reads the assignment, and would leave a need unserved. One walking a
    // dodge would spend the duty walking.
    std::vector<SphereShooter> shooters;
    ForEachGroupMember(bot, [&](Player* member)
    {
        if (!member->IsAlive() || !GET_PLAYERBOT_AI(member) || !PlayerbotAI::IsRangedDps(member) ||
            member->HasAura(SPELL_MARK) || !InAnubarakPit(member))
            return false;

        Position spot;
        bool const held = state.dodgeHolds.count(member->GetGUID()) && GetAnubarakDodgeHold(member, spot);
        if (held && Dist2d(member->GetPosition(), spot) > ANUBARAK_DODGE_HOLD_TRIGGER)
            return false;

        shooters.push_back({member, held});
        return false;
    });

    if (shooters.empty())
        return;

    std::sort(shooters.begin(), shooters.end(), [](SphereShooter const& a, SphereShooter const& b)
              { return a.player->GetGUID() < b.player->GetGUID(); });

    std::vector<Creature*> const spheres = ResolveSpheres(bot, state);
    std::vector<Creature*> flying;
    for (Creature* sphere : spheres)
        if (IsFrostSphereFlying(sphere))
            flying.push_back(sphere);

    std::vector<std::pair<char const*, Creature*>> needs;
    auto const taken = [&needs](Creature* sphere)
    {
        return std::any_of(needs.begin(), needs.end(),
                           [sphere](std::pair<char const*, Creature*> const& need) { return need.second == sphere; });
    };

    bool const heroic = AnubarakHeroic(bot);
    ObjectGuid const tankPatches[2] = {state.patch0.Get(), state.patch1.Get()};

    // A sphere already falling near the kiter lands as a patch in 1.5 s, so don't spend another
    Player* target = GetSpikeTarget(bot);
    Creature* spike = ResolveCreature(bot, state.spikeGuid);
    if (target && spike && spike->IsAlive() && !FallingNear(spheres, target->GetPosition()))
    {
        AnubarakKiteStand stand;
        ComputeKiteStand(target, spike, spheres, tankPatches, heroic, AnyBurrower(bot, state), nullptr, stand);
        if (stand.branch == AnubarakKiteBranch::Ring)
            if (Creature* sphere = KiteSphere(flying, target, spike))
                needs.emplace_back("kite", sphere);
    }

    // A kite comes every 5.5 s or so, so patches are made ahead of it. Phase 2 only: a stock patch in
    // phase 1 would crowd the submerge spot.
    if (state.phaseValue == AnubarakPhase::Submerged)
    {
        uint32 ready = 0;
        for (Creature* sphere : spheres)
        {
            ObjectGuid const guid = sphere->GetGUID();
            if (IsFrostSphereFalling(sphere) ||
                (IsPermafrostPatch(sphere) && guid != tankPatches[0] && guid != tankPatches[1]))
                ++ready;
        }

        uint8 const stock = heroic ? ANUBARAK_P2_PATCH_STOCK_HEROIC : ANUBARAK_P2_PATCH_STOCK;
        bool const spikeUp = spike && spike->IsAlive();
        Creature* boss = GetAnubarak(bot);
        if (ready < stock && (spikeUp || boss))
        {
            Position const from = spikeUp ? spike->GetPosition() : boss->GetPosition();
            if (Creature* sphere = StockSphere(flying, previous, from, spikeUp ? spike : nullptr, target, taken))
                needs.emplace_back("stock", sphere);
        }
    }

    if (state.phaseValue == AnubarakPhase::Surface || state.phaseValue == AnubarakPhase::Swarm)
    {
        size_t available = flying.size() - needs.size();
        for (uint8 side = 0; side < 2; ++side)
        {
            Position const point = GetAnubarakTankSidePoint(side);
            if (LatchedTankPatch(bot, state, side) || FallingNear(spheres, point))
                continue;

            if (heroic && available <= static_cast<size_t>(ANUBARAK_HEROIC_KITE_RESERVE))
                continue;

            // A sphere further out lands as a patch the latch never takes
            Creature* nearest = nullptr;
            float nearestDist = ANUBARAK_TANK_PATCH_LATCH;
            for (Creature* sphere : flying)
            {
                if (taken(sphere))
                    continue;

                float const dist = sphere->GetExactDist2d(point.GetPositionX(), point.GetPositionY());
                if (dist <= nearestDist)
                {
                    nearest = sphere;
                    nearestDist = dist;
                }
            }

            if (nearest)
            {
                needs.emplace_back(side == 0 ? "0" : "1", nearest);
                --available;
            }
        }
    }

    auto const assigned = [&state](ObjectGuid guid)
    {
        return std::any_of(state.duties.begin(), state.duties.end(),
                           [guid](SphereDuty const& duty) { return duty.shooter == guid; });
    };
    auto const eligible = [](SphereShooter const& shooter, Creature* sphere)
    { return !shooter.parked || AnubarakSphereInReach(shooter.player, sphere); };
    auto const staffed = [&state](char const* label, ObjectGuid sphere)
    {
        return std::count_if(state.duties.begin(), state.duties.end(), [label, sphere](SphereDuty const& duty)
                             { return duty.sphere == sphere && std::strcmp(duty.label, label) == 0; });
    };

    // Latched shooters first, every need, so a vacancy further up never takes one from a need below
    for (auto const& [label, sphere] : needs)
    {
        ObjectGuid const sphereGuid = sphere->GetGUID();
        for (SphereDuty const& duty : previous)
        {
            if (staffed(label, sphereGuid) >= ANUBARAK_SHOOTERS_PER_NEED)
                break;

            if (duty.sphere != sphereGuid || std::strcmp(duty.label, label) != 0 || assigned(duty.shooter))
                continue;

            auto const shooter = std::find_if(shooters.begin(), shooters.end(), [&duty](SphereShooter const& s)
                                              { return s.player->GetGUID() == duty.shooter; });
            if (shooter != shooters.end() && eligible(*shooter, sphere))
                state.duties.push_back({duty.shooter, sphereGuid, label});
        }
    }

    for (auto const& [label, sphere] : needs)
    {
        ObjectGuid const sphereGuid = sphere->GetGUID();
        while (staffed(label, sphereGuid) < ANUBARAK_SHOOTERS_PER_NEED)
        {
            // Sorted by guid, so a strict comparison leaves a tie on the lower guid
            SphereShooter const* nearest = nullptr;
            float nearestDist = 0.0f;
            for (SphereShooter const& shooter : shooters)
            {
                if (assigned(shooter.player->GetGUID()) || !eligible(shooter, sphere))
                    continue;

                float const dist = shooter.player->GetExactDist2d(sphere->GetPositionX(), sphere->GetPositionY());
                if (!nearest || dist < nearestDist)
                {
                    nearest = &shooter;
                    nearestDist = dist;
                }
            }

            if (!nearest)
                break;

            state.duties.push_back({nearest->player->GetGUID(), sphereGuid, label});
        }
    }
}

// One interrupter per casting burrower, so overlapping casts never wait on one bot. The victim first,
// it's in melee already, then the lowest guid with one ready in range. Never a marked bot: it's kiting.
void ComputeStrikeDuties(Player* bot, AnubarakState& state)
{
    state.strikes.clear();

    std::vector<Creature*> casters;
    for (Creature* burrower : ResolveAlive(bot, state.burrowers))
        if (burrower->FindCurrentSpellBySpellId(SPELL_SHADOW_STRIKE))
            casters.push_back(burrower);

    if (casters.empty())
        return;

    std::sort(casters.begin(), casters.end(),
              [](Creature* a, Creature* b) { return a->GetGUID() < b->GetGUID(); });

    auto const picked = [&state](ObjectGuid guid)
    {
        return std::any_of(state.strikes.begin(), state.strikes.end(),
                           [guid](StrikeDuty const& duty) { return duty.interrupter == guid; });
    };

    std::vector<Creature*> open;
    for (Creature* caster : casters)
    {
        Unit* victim = caster->GetVictim();
        Player* victimPlayer = victim ? victim->ToPlayer() : nullptr;
        if (victimPlayer && victimPlayer->IsAlive() && !victimPlayer->HasAura(SPELL_MARK) &&
            !picked(victimPlayer->GetGUID()) && AnubarakReadyInterrupt(victimPlayer, caster))
            state.strikes.push_back({caster->GetGUID(), victimPlayer->GetGUID()});
        else
            open.push_back(caster);
    }

    if (open.empty())
        return;

    std::vector<Player*> members;
    ForEachGroupMember(bot, [&](Player* member)
    {
        if (member->IsAlive() && !member->HasAura(SPELL_MARK))
            members.push_back(member);

        return false;
    });

    std::sort(members.begin(), members.end(), [](Player* a, Player* b) { return a->GetGUID() < b->GetGUID(); });

    for (Creature* caster : open)
    {
        for (Player* member : members)
        {
            if (picked(member->GetGUID()) || !member->IsWithinDistInMap(caster, ANUBARAK_INTERRUPT_RANGE) ||
                !AnubarakReadyInterrupt(member, caster))
                continue;

            state.strikes.push_back({caster->GetGUID(), member->GetGUID()});
            break;
        }
    }
}

// Once per instance per ms. The claim stands for the mark window: one paladin covers the kiter, and
// passes it on only when its Hand of Protection hasn't landed 1.5 s in.
ProtectClaim const* DeriveProtection(Player* bot)
{
    AnubarakState* state = bot ? ReadPhase(GET_PLAYERBOT_AI(bot)) : nullptr;
    if (!state)
        return nullptr;

    ProtectClaim& claim = state->protect;
    uint32 const now = getMSTime();
    if (claim.memoValid && claim.memoMs == now)
        return &claim;

    claim.memoValid = true;
    claim.memoMs = now;
    claim.need = false;
    claim.spikeInImmunityRange = false;

    Creature* spike = GetPursuingSpike(bot);
    Player* kiter = spike ? GetSpikeTarget(bot) : nullptr;
    ObjectGuid const kiterGuid = kiter ? kiter->GetGUID() : ObjectGuid::Empty;
    if (kiterGuid != claim.kiter)
    {
        claim.kiter = kiterGuid;
        claim.protector.Clear();
        claim.passed.clear();
    }

    if (!kiter)
        return &claim;

    float const spikeDist = Dist2d(spike->GetPosition(), kiter->GetPosition());
    bool const immune = AnubarakKiterImmune(kiter);
    claim.spikeInImmunityRange = spikeDist <= ANUBARAK_KITE_IMMUNITY_RANGE;
    claim.need = !immune && !kiter->HasAura(SPELL_FORBEARANCE) && spikeDist <= ANUBARAK_KITE_PROTECT_RANGE;

    if (!claim.protector.IsEmpty() && !immune)
    {
        Player* protector = ObjectAccessor::GetPlayer(*bot, claim.protector);
        // The spike fell back: the claim lapses, and the same paladin takes it again next time
        if (!claim.need)
            claim.protector.Clear();
        else if (!protector || !protector->IsAlive() ||
                 getMSTimeDiff(claim.claimMs, now) >= ANUBARAK_PROTECT_CLAIM_MS)
        {
            claim.passed.push_back(claim.protector);
            claim.protector.Clear();
        }
    }

    if (!claim.need || !claim.protector.IsEmpty())
        return &claim;

    std::vector<Player*> paladins;
    ForEachGroupMember(bot, [&](Player* member)
    {
        if (member->IsAlive() && member->getClass() == CLASS_PALADIN && GET_PLAYERBOT_AI(member) &&
            !member->HasAura(SPELL_MARK) &&
            std::find(claim.passed.begin(), claim.passed.end(), member->GetGUID()) == claim.passed.end())
            paladins.push_back(member);

        return false;
    });

    std::sort(paladins.begin(), paladins.end(), [](Player* a, Player* b) { return a->GetGUID() < b->GetGUID(); });

    // CanCastSpell lets an out-of-range target through, so range and sight are asked here
    for (Player* paladin : paladins)
    {
        if (!paladin->IsWithinDistInMap(kiter, ANUBARAK_HOP_RANGE) || !paladin->IsWithinLOSInMap(kiter) ||
            !GET_PLAYERBOT_AI(paladin)->CanCastSpell("hand of protection", kiter))
            continue;

        claim.protector = paladin->GetGUID();
        claim.claimMs = now;
        break;
    }

    return &claim;
}

char const* ProtectRole(Player* bot, ProtectClaim const& claim)
{
    if (claim.need && claim.protector == bot->GetGUID())
        return "hop";

    if (claim.kiter == bot->GetGUID() && claim.spikeInImmunityRange && claim.protector.IsEmpty() &&
        !AnubarakKiterImmune(bot))
        return "self";

    return "none";
}

// Up for grabs: on someone holding no burrower side, which includes the boss tank
bool IsLooseBurrower(Creature* burrower, Player* bot)
{
    Unit* victim = burrower->GetVictim();
    if (!victim || victim == bot)
        return false;

    Player* victimPlayer = victim->ToPlayer();
    return !victimPlayer || GetAnubarakBurrowerTankSide(victimPlayer) < 0;
}

bool IsFocusableBurrower(Creature* burrower)
{
    return burrower && burrower->IsAlive() && !burrower->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) &&
           UnitOnPermafrost(burrower);
}

// No frost spheres: heroic has six for the pull, and only sphere duty spends them
bool IsAnubarakSkullTarget(Unit* unit)
{
    uint32 const entry = unit->GetEntry();
    return unit->IsCreature() && (entry == static_cast<uint32>(ToCNpcs::NPC_ANUBARAK) ||
                                  entry == static_cast<uint32>(ToCNpcs::NPC_NERUBIAN_BURROWER) ||
                                  entry == static_cast<uint32>(ToCNpcs::NPC_SWARM_SCARAB));
}

// Each scarab puts 20000 threat on a random player, so there's no tank to gather on: per bot, the
// nearest one in the tanks' pickup reach
Creature* NearestDpsScarab(Player* bot, AnubarakState const& state)
{
    Creature* nearest = nullptr;
    float nearestDist = ANUBARAK_SCARAB_PICKUP_RANGE;
    for (Creature* scarab : ResolveAlive(bot, state.scarabs))
    {
        if (scarab->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) || scarab->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE) ||
            !scarab->IsInCombat())
            continue;

        float const dist = bot->GetExactDist2d(scarab);
        if (dist <= nearestDist)
        {
            nearest = scarab;
            nearestDist = dist;
        }
    }

    return nearest;
}

Unit* NoteDpsPick(Player* bot, Unit* pick, char const* branch)
{
    RaidObs::NoteDerived(bot, "anub.dps", branch);
    return pick;
}
}

namespace TrialOfTheCrusaderHelpers
{

Creature* GetAnubarak(Player* bot)
{
    if (!InToCInstance(bot))
        return nullptr;

    InstanceScript* instance = bot->GetInstanceScript();
    if (!instance)
        return nullptr;

    Creature* anubarak = ResolveCreature(bot, instance->GetGuidData(TOC_DATA_ANUBARAK));
    return anubarak && anubarak->IsAlive() ? anubarak : nullptr;
}

bool AnubarakEngaged(PlayerbotAI* botAI) { return ToCEncounterIsLive(botAI, ToCEncounter::Anubarak); }

AnubarakPhase GetAnubarakPhase(PlayerbotAI* botAI)
{
    AnubarakState const* state = ReadPhase(botAI);
    return state ? state->phaseValue : AnubarakPhase::None;
}

bool AnubarakSubmerged(PlayerbotAI* botAI) { return GetAnubarakPhase(botAI) == AnubarakPhase::Submerged; }

bool AnubarakLeechingSwarmActive(PlayerbotAI* botAI) { return GetAnubarakPhase(botAI) == AnubarakPhase::Swarm; }

Creature* GetPursuingSpike(Player* bot)
{
    AnubarakState const* state = Sweep(bot);
    Creature* spike = state ? ResolveCreature(bot, state->spikeGuid) : nullptr;
    return spike && spike->IsAlive() ? spike : nullptr;
}

Player* GetSpikeTarget(Player* bot)
{
    if (!bot)
        return nullptr;

    Player* target = nullptr;
    ForEachGroupMember(bot, [&](Player* member)
    {
        if (member->IsAlive() && member->HasAura(SPELL_MARK))
            target = member;

        return target != nullptr;
    });

    return target;
}

bool AnubarakSpikeLaneCrosses(Player* bot, Position const& spot)
{
    Creature* spike = bot ? GetPursuingSpike(bot) : nullptr;
    Player* target = spike ? GetSpikeTarget(bot) : nullptr;
    return target &&
           SegmentDistance2d(spike->GetPosition(), target->GetPosition(), spot) < ANUBARAK_SPIKE_LANE_CLEARANCE;
}

bool IsFrostSphereFlying(Unit* sphere)
{
    return sphere && sphere->IsAlive() && sphere->GetEntry() == static_cast<uint32>(ToCNpcs::NPC_FROST_SPHERE) &&
           sphere->HasAura(SPELL_FROST_SPHERE) && !sphere->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
}

bool IsFrostSphereFalling(Unit* sphere)
{
    return sphere && sphere->IsAlive() && sphere->GetEntry() == static_cast<uint32>(ToCNpcs::NPC_FROST_SPHERE) &&
           sphere->HasAura(SPELL_FROST_SPHERE) && sphere->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
}

bool IsPermafrostPatch(Unit* sphere)
{
    return sphere && sphere->IsAlive() && sphere->GetEntry() == static_cast<uint32>(ToCNpcs::NPC_FROST_SPHERE) &&
           !sphere->HasAura(SPELL_FROST_SPHERE);
}

std::vector<Creature*> GetFrostSpheres(Player* bot)
{
    AnubarakState const* state = Sweep(bot);
    return state ? ResolveSpheres(bot, *state) : std::vector<Creature*>();
}

bool UnitOnPermafrost(Unit* unit)
{
    return unit && unit->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_PERMAFROST, unit));
}

Position GetAnubarakTankSidePoint(uint8 side)
{
    float const offset = side == 0 ? ANUBARAK_TANK_PATCH_OFFSET : -ANUBARAK_TANK_PATCH_OFFSET;
    return Position(ANUBARAK_ROOM_CENTER.GetPositionX(), ANUBARAK_ROOM_CENTER.GetPositionY() + offset,
                    ANUBARAK_ROOM_CENTER.GetPositionZ());
}

Creature* GetAnubarakTankPatch(Player* bot, uint8 side)
{
    if (!bot || side > 1)
        return nullptr;

    AnubarakState const* state = ReadPhase(GET_PLAYERBOT_AI(bot));
    return state ? LatchedTankPatch(bot, *state, side) : nullptr;
}

Position GetAnubarakBossAnchor(Player* bot)
{
    Position anchor;
    return GetAnubarakDragAnchor(bot, anchor) ? anchor : ANUBARAK_ROOM_CENTER;
}

bool GetAnubarakDragAnchor(Player* bot, Position& out)
{
    Creature* side0 = GetAnubarakTankPatch(bot, 0);
    Creature* side1 = GetAnubarakTankPatch(bot, 1);
    if (!side0 || !side1)
        return false;

    out = Position((side0->GetPositionX() + side1->GetPositionX()) / 2.0f,
                   (side0->GetPositionY() + side1->GetPositionY()) / 2.0f,
                   (side0->GetPositionZ() + side1->GetPositionZ()) / 2.0f);
    return true;
}

bool GetAnubarakSubmergeSpot(Player* bot, Position& out)
{
    AnubarakState* state = bot ? ReadPhase(GET_PLAYERBOT_AI(bot)) : nullptr;
    if (!state || state->phaseValue != AnubarakPhase::Surface ||
        getMSTimeDiff(state->surfacedMs, getMSTime()) < ANUBARAK_SUBMERGE_PREP_MS)
        return false;

    std::vector<Position> patches;
    for (Creature* sphere : ResolveSpheres(bot, *state))
        if (IsPermafrostPatch(sphere))
            patches.push_back(sphere->GetPosition());

    if (patches.empty())
        return false;

    auto const clear = [&patches](Position const& spot, float clearance)
    {
        return std::none_of(patches.begin(), patches.end(),
                            [&spot, clearance](Position const& patch) { return Dist2d(spot, patch) < clearance; });
    };

    // Latched for the window, the sweep raycasts every candidate. Redone once a patch lands closer than
    // the clearance the spot was found with.
    if (!state->submergeSpotValid || !clear(state->submergeSpot, state->submergeSpotClearance))
    {
        // navprobe-clean out to the kite ring, so he never ends up against a wall
        auto const onFloor = [](float x, float y)
        { return ANUBARAK_ROOM_CENTER.GetExactDist2d(x, y) <= ANUBARAK_KITE_RING_RADIUS; };

        state->submergeSpotValid = false;
        HazardSweepCache cache;
        for (float clearance : {ANUBARAK_SUBMERGE_PATCH_CLEARANCE, ANUBARAK_SUBMERGE_PATCH_CLEARANCE_MIN})
        {
            std::vector<HazardCircle> hazards;
            for (Position const& patch : patches)
                hazards.emplace_back(patch, clearance);

            Position const spot = FindNearestPositionClearOfHazards(bot, hazards, ANUBARAK_SUBMERGE_SPOT_SEARCH, 2.0f,
                                                                    static_cast<float>(M_PI) / 8.0f,
                                                                    &ANUBARAK_ROOM_CENTER, onFloor, &cache);
            if (spot == Position())
                continue;

            state->submergeSpotValid = true;
            state->submergeSpot = spot;
            state->submergeSpotClearance = clearance;
            break;
        }
    }

    if (!state->submergeSpotValid)
        return false;

    out = state->submergeSpot;
    return true;
}

AnubarakDragLeg GetAnubarakDragLeg(Player* bot, Creature* boss, Position const& point, bool submerge)
{
    AnubarakDragLeg leg;
    AnubarakState* state = bot && boss ? ReadPhase(GET_PLAYERBOT_AI(bot)) : nullptr;
    if (!state)
        return leg;

    DragLegLatch& latch = state->drag;
    if (latch.valid && latch.tank != bot->GetGUID())
        latch = DragLegLatch();

    if (boss->GetVictim() != bot)
    {
        latch = DragLegLatch();
        return leg;
    }

    uint32 const now = getMSTime();
    if (latch.valid)
    {
        if (Dist2d(boss->GetPosition(), latch.point) <= ANUBARAK_DRAG_DONE)
        {
            latch = DragLegLatch();
            return leg;
        }

        bool const onSpot = bot->GetExactDist2d(latch.tankSpot.GetPositionX(), latch.tankSpot.GetPositionY()) <=
                            ANUBARAK_DRAG_SPOT_ARRIVE;
        if (onSpot && !latch.arrived)
        {
            latch.arrived = true;
            latch.arrivedMs = now;
        }

        // He can still stay put (a cast, a stun), so a tank waiting on its spot judges after the settle time
        bool const followed =
            latch.bossMoved || (latch.arrived && getMSTimeDiff(latch.arrivedMs, now) >= ANUBARAK_DRAG_SETTLE_MS);
        bool const settled = followed && BossStillFor(*state, now, ANUBARAK_DRAG_SETTLE_MS);
        if (!settled && getMSTimeDiff(latch.plannedMs, now) < ANUBARAK_DRAG_LEG_MS)
        {
            leg.step = onSpot ? AnubarakDragStep::Settle : AnubarakDragStep::Walk;
            leg.tankSpot = latch.tankSpot;
            return leg;
        }

        // Judged or dropped. A moved point is judged here too, never mid walk.
        latch = DragLegLatch();
    }

    float const start = submerge ? ANUBARAK_DRAG_DONE : ANUBARAK_DRAG_START;
    float const toPoint = Dist2d(boss->GetPosition(), point);
    if (toPoint <= start || !BossStillFor(*state, now, ANUBARAK_DRAG_SETTLE_MS))
        return leg;

    // He starts past both reaches plus contact and catches up at his speed over the bot's, then runs on
    // until his next recheck once the bot stops. The bot stands far enough out that he stops on the point.
    // The hold action walks the leg backwards, so the bot moves at its backward speed.
    float const bossSpeed = boss->GetSpeed(MOVE_RUN);
    float const botSpeed = bot->GetSpeed(MOVE_RUN_BACK);
    if (bossSpeed <= 0.0f || botSpeed <= 0.0f)
        return leg;

    float const chaseStart = boss->GetCombatReach() + bot->GetCombatReach() + CONTACT_DISTANCE;
    float const speedRatio = bossSpeed / botSpeed;
    float const stopLag = bossSpeed * ANUBARAK_DRAG_STOP_LAG_S;
    float const lead = chaseStart + std::max(toPoint - stopLag, 0.0f) / speedRatio;

    Vec2 dir = Delta(boss->GetPosition(), point);
    dir = {dir.x / toPoint, dir.y / toPoint};
    Position const spot = ValidateFloorPoint(bot, Offset(boss->GetPosition(), dir, lead));
    // Floor check pulled it inside his reach, so he'd never follow and the leg would replan forever
    if (Dist2d(spot, boss->GetPosition()) <= chaseStart)
        return leg;

    latch.valid = true;
    latch.tank = bot->GetGUID();
    latch.point = point;
    latch.tankSpot = spot;
    latch.plannedMs = now;

    bool const onSpot = bot->GetExactDist2d(latch.tankSpot.GetPositionX(), latch.tankSpot.GetPositionY()) <=
                        ANUBARAK_DRAG_SPOT_ARRIVE;
    if (onSpot)
    {
        latch.arrived = true;
        latch.arrivedMs = now;
    }

    leg.step = onSpot ? AnubarakDragStep::Settle : AnubarakDragStep::Walk;
    leg.tankSpot = latch.tankSpot;
    return leg;
}

void ClearAnubarakDrag(Player* bot)
{
    if (!InToCInstance(bot))
        return;

    AnubarakState* state = anubarakStates.Find(bot->GetInstanceId());
    if (state && state->drag.valid && state->drag.tank == bot->GetGUID())
        state->drag = DragLegLatch();
}

bool IsAnubarakDragInFlight(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!InToCInstance(bot))
        return false;

    AnubarakState const* state = anubarakStates.Find(bot->GetInstanceId());
    if (!state || !state->drag.valid || state->drag.tank != bot->GetGUID() ||
        getMSTimeDiff(state->drag.plannedMs, getMSTime()) >= ANUBARAK_DRAG_LEG_MS)
        return false;

    Creature* boss = GetAnubarak(bot);
    return boss && boss->GetVictim() == bot && Dist2d(boss->GetPosition(), state->drag.point) > ANUBARAK_DRAG_DONE;
}

bool AnubarakTauntWouldSteal(PlayerbotAI* botAI)
{
    if (!botAI || !AnubarakEngaged(botAI))
        return false;

    Player* bot = botAI->GetBot();
    Creature* boss = GetAnubarak(bot);
    if (!boss || botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get() != boss)
        return false;

    Unit* victim = boss->GetVictim();
    if (!victim || victim == bot)
        return false;

    Player* holder = victim->ToPlayer();
    return holder && IsAnubarakTankPlayer(holder);
}

bool AnubarakThreatCapped(Player* bot)
{
    if (!IsToCDpsBot(bot))
        return false;

    AnubarakState* state = ReadPhase(GET_PLAYERBOT_AI(bot));
    if (!state || (state->phaseValue != AnubarakPhase::Surface && state->phaseValue != AnubarakPhase::Swarm))
        return false;

    ThreatCapLatch& cap = state->threatCaps[bot->GetGUID()];
    uint32 const now = getMSTime();
    if (cap.memoValid && cap.memoMs == now)
        return cap.value;

    cap.memoValid = true;
    cap.memoMs = now;
    cap.value = false;

    Creature* boss = GetAnubarak(bot);
    Unit* victim = boss ? boss->GetVictim() : nullptr;
    Player* tank = victim ? victim->ToPlayer() : nullptr;
    if (!tank || tank == bot || !IsAnubarakTankPlayer(tank))
        return false;

    // The "threat" AI value is a share in a uint8 and wraps past 255%
    float const own = boss->GetThreatMgr().GetThreat(bot);
    float const held = boss->GetThreatMgr().GetThreat(tank);
    if (!cap.capped && own > 0.0f && own >= ANUBARAK_THREAT_CAP * held)
        cap.capped = true;
    else if (cap.capped && own < ANUBARAK_THREAT_RELEASE * held)
        cap.capped = false;

    cap.value = cap.capped;
    return cap.value;
}

int8 GetAnubarakBurrowerTankSide(Player* player)
{
    AnubarakState const* state = LatchesFor(player);
    if (!state)
        return -1;

    for (uint8 side = 0; side < 2; ++side)
        if (state->sideTanks[side] == player->GetGUID())
            return side;

    return -1;
}

bool IsAnubarakPickupTank(Player* bot)
{
    AnubarakState const* state = bot && bot->IsAlive() ? LatchesFor(bot) : nullptr;
    if (!state)
        return false;

    Player* main = state->mainTank.IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(*bot, state->mainTank);
    if (main && main->IsAlive())
        return main == bot;

    return state->sideTanks[0] == bot->GetGUID();
}

bool IsAnubarakTankPlayer(Player* player)
{
    return player && (PlayerbotAI::IsTank(player) || PlayerbotAI::IsTank(player, true));
}

AnubarakPrepullTanks GetAnubarakPrepullTanks(Player* bot)
{
    AnubarakPrepullTanks tanks;
    Group* group = bot ? bot->GetGroup() : nullptr;
    if (!group)
        return tanks;

    ObjectGuid const mainTank = StableMainTankGuid(group);
    std::vector<ObjectGuid> const sides = RankSideTanks(bot, group, mainTank);
    for (size_t side = 0; side < 2 && side < sides.size(); ++side)
        tanks.side[side] = sides[side];

    Player* main = mainTank.IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(*bot, mainTank);
    tanks.pickup = main && main->IsAlive() ? mainTank : tanks.side[0];
    return tanks;
}

std::vector<Creature*> GetAnubarakBurrowers(Player* bot)
{
    AnubarakState const* state = Sweep(bot);
    return state ? ResolveAlive(bot, state->burrowers) : std::vector<Creature*>();
}

AnubarakBurrowerPick GetAnubarakBurrowerPick(PlayerbotAI* botAI)
{
    AnubarakBurrowerPick pick;
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    int8 const side = GetAnubarakBurrowerTankSide(bot);
    if (side < 0)
        return pick;

    pick.side = static_cast<uint8>(side);
    Creature* patch = GetAnubarakTankPatch(bot, pick.side);
    pick.hold = patch ? patch->GetPosition() : GetAnubarakTankSidePoint(pick.side);

    Unit* current = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    float looseDist = 0.0f;
    for (Creature* burrower : GetAnubarakBurrowers(bot))
    {
        // Submerged: it comes back up where it went down, 10 s later
        if (burrower->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            continue;

        if (burrower->GetVictim() == bot)
        {
            if (!pick.held || burrower == current)
                pick.held = burrower;

            continue;
        }

        if (!IsLooseBurrower(burrower, bot))
            continue;

        float const dist = burrower->GetExactDist2d(pick.hold.GetPositionX(), pick.hold.GetPositionY());
        if (!pick.loose || dist < looseDist)
        {
            pick.loose = burrower;
            looseDist = dist;
        }
    }

    return pick;
}

Creature* GetAnubarakFocusBurrower(Player* bot)
{
    AnubarakState* state = Sweep(bot);
    if (!state)
        return nullptr;

    Creature* latched = ResolveCreature(bot, state->focusBurrower.Get());
    if (IsFocusableBurrower(latched))
        return latched;

    Creature* lowest = nullptr;
    for (Creature* burrower : ResolveAlive(bot, state->burrowers))
    {
        if (!IsFocusableBurrower(burrower))
            continue;

        // Guid breaks a tie, so the pick never hangs on the sweep's order
        float const pct = burrower->GetHealthPct();
        if (!lowest || pct < lowest->GetHealthPct() ||
            (!(lowest->GetHealthPct() < pct) && burrower->GetGUID() < lowest->GetGUID()))
        {
            lowest = burrower;
        }
    }

    state->focusBurrower = lowest ? lowest->GetGUID() : ObjectGuid::Empty;
    return lowest;
}

Unit* GetAnubarakDpsTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!IsToCDpsBot(bot) || !AnubarakEngaged(botAI))
        return nullptr;

    // Read first: a new pull's first read clears the focus latch
    AnubarakPhase const phase = GetAnubarakPhase(botAI);

    // The sphere node outranks this one, so with a pick here a shooter would swap targets every tick
    if (PlayerbotAI::IsRangedDps(bot) && GetAnubarakSphereToShoot(bot))
        return NoteDpsPick(bot, nullptr, "none");

    // Read even under a skull, so the anub.focus latch stays current
    Creature* burrower = GetAnubarakFocusBurrower(bot);

    if (Unit* skull = GetToCHumanSkull(botAI, &IsAnubarakSkullTarget))
        return NoteDpsPick(bot, skull, "skull");

    if (burrower)
        return NoteDpsPick(bot, burrower, "burrower");

    Creature* boss = GetAnubarak(bot);
    if (boss && !boss->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
        return NoteDpsPick(bot, boss, "boss");

    AnubarakState const* state = phase == AnubarakPhase::Submerged ? Sweep(bot) : nullptr;
    if (Creature* scarab = state ? NearestDpsScarab(bot, *state) : nullptr)
        return NoteDpsPick(bot, scarab, "scarab");

    return NoteDpsPick(bot, nullptr, "none");
}

Creature* GetAnubarakScarabToPickUp(Player* bot)
{
    AnubarakState const* state = Sweep(bot);
    if (!state)
        return nullptr;

    Creature* nearest = nullptr;
    float nearestDist = ANUBARAK_SCARAB_PICKUP_RANGE;
    for (Creature* scarab : ResolveAlive(bot, state->scarabs))
    {
        // The ten roaming at the pre-pull are the same entry, non-attackable and hitting nobody
        if (scarab->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) || scarab->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE))
            continue;

        Unit* victim = scarab->GetVictim();
        if (!victim || victim == bot)
            continue;

        Player* victimPlayer = victim->ToPlayer();
        if (victimPlayer && IsAnubarakTankPlayer(victimPlayer))
            continue;

        float const dist = bot->GetExactDist2d(scarab);
        if (dist <= nearestDist)
        {
            nearest = scarab;
            nearestDist = dist;
        }
    }

    return nearest;
}

Player* GetAnubarakPenetratingColdHealTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    uint32 const cold = sSpellMgr->GetSpellIdForDifficulty(SPELL_PENETRATING_COLD, bot);
    float const range = botAI->GetRange("heal");

    Player* lowest = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsInWorld() || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
            continue;

        if (member->GetHealthPct() >= ANUBARAK_PENETRATING_COLD_HEAL_PCT || !member->HasAura(cold))
            continue;

        if (!bot->IsWithinDistInMap(member, range) || !bot->IsWithinLOSInMap(member))
            continue;

        if (!lowest || member->GetHealthPct() < lowest->GetHealthPct())
            lowest = member;
    }

    return lowest;
}

Creature* GetAnubarakSphereToShoot(Player* bot)
{
    if (!bot || !PlayerbotAI::IsRangedDps(bot))
        return nullptr;

    AnubarakState* state = ReadPhase(GET_PLAYERBOT_AI(bot));
    if (!state)
        return nullptr;

    Group* group = bot->GetGroup();
    ObjectGuid const groupGuid = group ? group->GetGUID() : bot->GetGUID();
    uint32 const now = getMSTime();
    if (!state->dutyMemoValid || state->dutyMemoMs != now || state->dutyGroup != groupGuid)
    {
        state->dutyMemoValid = true;
        state->dutyMemoMs = now;
        state->dutyGroup = groupGuid;
        ComputeDuties(bot, *state);
    }

    for (SphereDuty const& duty : state->duties)
    {
        if (duty.shooter != bot->GetGUID())
            continue;

        Creature* sphere = ResolveCreature(bot, duty.sphere);
        if (!IsFrostSphereFlying(sphere))
            break;

        RaidObs::NoteDerived(bot, "anub.sphere", duty.label);
        return sphere;
    }

    RaidObs::NoteDerived(bot, "anub.sphere", "none");
    return nullptr;
}

bool AnubarakSphereInReach(Player* bot, Unit* sphere)
{
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    if (!botAI || !sphere)
        return false;

    float const reach = botAI->GetRange("spell") - ANUBARAK_SHOOT_MARGIN;
    return bot->IsWithinDist3d(sphere->GetPositionX(), sphere->GetPositionY(), sphere->GetPositionZ(), reach) &&
           bot->IsWithinLOSInMap(sphere);
}

bool GetAnubarakSphereStand(Player* bot, Unit* sphere, Position& out)
{
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    if (!botAI || !sphere)
        return false;

    // A flying sphere sits about 13.5 yd up, so the reach across the floor is the rest of the 3D reach
    float const reach = botAI->GetRange("spell") - ANUBARAK_SHOOT_MARGIN - ANUBARAK_SPHERE_STAND_SLACK;
    float const height = sphere->GetPositionZ() - bot->GetPositionZ();
    if (reach <= std::fabs(height))
        return false;

    float const across = std::sqrt(reach * reach - height * height);
    Position const ground(sphere->GetPositionX(), sphere->GetPositionY(), bot->GetPositionZ());
    Vec2 dir = Delta(ground, bot->GetPosition());
    float const length = Length(dir);
    if (length < ANUBARAK_SPHERE_STAND_SLACK)
        return false;

    dir = {dir.x / length, dir.y / length};
    // Already in reach means out of sight from here, so it closes half the gap instead
    float const offset = length > across ? across : length / 2.0f;

    Position stand;
    if (!OnFloor(bot, Offset(ground, dir, offset), stand))
        return false;

    Creature* spike = GetPursuingSpike(bot);
    if (spike && Dist2d(spike->GetPosition(), stand) < ANUBARAK_SPIKE_DANGER_RADIUS)
        return false;

    if (AnubarakSpikeLaneCrosses(bot, stand))
        return false;

    out = stand;
    return true;
}

bool GetAnubarakKiteStand(Player* bot, AnubarakKiteStand const* previous, AnubarakKiteStand& out)
{
    // Copied first, a caller may pass its latch as both previous and out
    AnubarakKiteStand const held = previous ? *previous : AnubarakKiteStand();
    previous = previous ? &held : nullptr;
    out = AnubarakKiteStand();
    if (!bot)
        return false;

    AnubarakState const* state = ReadPhase(GET_PLAYERBOT_AI(bot));
    Creature* spike = state ? GetPursuingSpike(bot) : nullptr;
    if (!spike)
    {
        RaidObs::NoteDerived(bot, "anub.kite", "none");
        return false;
    }

    ObjectGuid const tankPatches[2] = {state->patch0.Get(), state->patch1.Get()};
    ComputeKiteStand(bot, spike, ResolveSpheres(bot, *state), tankPatches, AnubarakHeroic(bot),
                     AnyBurrower(bot, *state), previous, out);
    RaidObs::NoteDerived(bot, "anub.kite", KiteBranchName(out.branch));
    return true;
}

bool AnubarakInSpikeDanger(Player* bot)
{
    if (!bot || bot->HasAura(SPELL_MARK) || !AnubarakEngaged(GET_PLAYERBOT_AI(bot)))
        return false;

    Creature* spike = GetPursuingSpike(bot);
    if (!spike)
        return false;

    if (bot->GetExactDist2d(spike->GetPositionX(), spike->GetPositionY()) < ANUBARAK_SPIKE_DANGER_RADIUS)
        return true;

    Player* target = GetSpikeTarget(bot);
    return target && SegmentDistance2d(spike->GetPosition(), target->GetPosition(), bot->GetPosition()) <
                         ANUBARAK_SPIKE_LANE_HALF_WIDTH;
}

bool GetAnubarakSpikeDodgeSpot(Player* bot, Position& out)
{
    Creature* spike = bot ? GetPursuingSpike(bot) : nullptr;
    if (!spike)
        return false;

    Position const spikePos = spike->GetPosition();
    Player* target = GetSpikeTarget(bot);
    Position const targetPos = target ? target->GetPosition() : spikePos;

    std::vector<HazardCircle> const spikeOnly = {HazardCircle(spikePos, ANUBARAK_SPIKE_CLEARANCE)};
    std::vector<HazardCircle> withPatches = spikeOnly;
    if (AnubarakHeroic(bot))
        for (Creature* sphere : GetFrostSpheres(bot))
            if (IsPermafrostPatch(sphere))
                withPatches.emplace_back(sphere->GetPosition(), ANUBARAK_PERMAFROST_SLOW_REACH);

    auto const clearOfLane = [&](float x, float y)
    {
        return !target || SegmentDistance2d(spikePos, targetPos, Position(x, y, 0.0f)) >= ANUBARAK_SPIKE_LANE_CLEARANCE;
    };

    // Heroic patches slow 80%, but a dodge into one still beats staying in the lane
    HazardSweepCache cache;
    Position spot = FindNearestPositionClearOfHazards(bot, withPatches, ANUBARAK_SPIKE_DODGE_SEARCH, 2.0f,
                                                      static_cast<float>(M_PI) / 8.0f, nullptr, clearOfLane, &cache);
    if (spot == Position() && withPatches.size() > spikeOnly.size())
        spot = FindNearestPositionClearOfHazards(bot, spikeOnly, ANUBARAK_SPIKE_DODGE_SEARCH, 2.0f,
                                                 static_cast<float>(M_PI) / 8.0f, nullptr, clearOfLane, &cache);

    if (spot == Position())
    {
        RaidObs::NoteDerived(bot, "anub.dodge", "none");
        return false;
    }

    bool const nearSpike = bot->GetExactDist2d(spikePos.GetPositionX(), spikePos.GetPositionY()) <
                           ANUBARAK_SPIKE_DANGER_RADIUS;
    RaidObs::NoteDerived(bot, "anub.dodge", nearSpike ? "spike" : "lane");
    out = spot;
    return true;
}

bool GetAnubarakDodgeHold(Player* bot, Position& spot)
{
    if (!InToCInstance(bot) || !bot->IsAlive() || bot->HasAura(SPELL_MARK) ||
        !AnubarakEngaged(GET_PLAYERBOT_AI(bot)))
        return false;

    AnubarakState const* state = anubarakStates.Find(bot->GetInstanceId());
    if (!state)
        return false;

    auto const it = state->dodgeHolds.find(bot->GetGUID());
    if (it == state->dodgeHolds.end())
        return false;

    DodgeHold const& hold = it->second;
    uint32 const heldMs = getMSTimeDiff(hold.setMs, getMSTime());
    if (heldMs >= ANUBARAK_DODGE_HOLD_MS ||
        (heldMs >= ANUBARAK_DODGE_WALK_MS && Dist2d(bot->GetPosition(), hold.spot) > ANUBARAK_DODGE_HOLD_TRIGGER))
        return false;

    // The spike idles 4 s between marks with nobody marked, so an empty kiter is a kiter too
    Creature* spike = GetPursuingSpike(bot);
    if (!spike)
        return false;

    Player* kiter = GetSpikeTarget(bot);
    if ((kiter ? kiter->GetGUID() : ObjectGuid::Empty) != hold.kiter)
        return false;

    float const spikeDist = Dist2d(spike->GetPosition(), hold.spot);
    if (spikeDist < ANUBARAK_SPIKE_DANGER_RADIUS ||
        spikeDist > std::max(ANUBARAK_DODGE_HOLD_RELEASE, hold.spikeDistAtSet))
        return false;

    if (kiter &&
        SegmentDistance2d(spike->GetPosition(), kiter->GetPosition(), hold.spot) < ANUBARAK_SPIKE_LANE_HALF_WIDTH)
        return false;

    spot = hold.spot;
    return true;
}

void SetAnubarakDodgeHold(Player* bot, Position const& spot)
{
    AnubarakState* state = bot ? ReadPhase(GET_PLAYERBOT_AI(bot)) : nullptr;
    Creature* spike = state ? GetPursuingSpike(bot) : nullptr;
    if (!spike)
        return;

    Player* kiter = GetSpikeTarget(bot);
    DodgeHold& hold = state->dodgeHolds[bot->GetGUID()];
    hold.spot = spot;
    hold.kiter = kiter ? kiter->GetGUID() : ObjectGuid::Empty;
    hold.setMs = getMSTime();
    hold.spikeDistAtSet = Dist2d(spike->GetPosition(), spot);
}

bool IsAnubarakDodgeHeld(PlayerbotAI* botAI)
{
    Position spot;
    return botAI && GetAnubarakDodgeHold(botAI->GetBot(), spot);
}

Player* GetAnubarakKiterToProtect(Player* bot)
{
    ProtectClaim const* claim = DeriveProtection(bot);
    if (!claim)
        return nullptr;

    char const* role = ProtectRole(bot, *claim);
    RaidObs::NoteDerived(bot, "anub.protect", role);
    if (std::strcmp(role, "hop") != 0)
        return nullptr;

    Player* kiter = ObjectAccessor::GetPlayer(*bot, claim->kiter);
    return kiter && kiter->IsAlive() ? kiter : nullptr;
}

bool AnubarakKiterNeedsImmunity(Player* bot)
{
    ProtectClaim const* claim = DeriveProtection(bot);
    if (!claim)
        return false;

    char const* role = ProtectRole(bot, *claim);
    RaidObs::NoteDerived(bot, "anub.protect", role);
    return std::strcmp(role, "self") == 0;
}

bool AnubarakKiterImmune(Unit* kiter)
{
    if (!kiter)
        return false;

    for (uint32 spell : ANUBARAK_KITE_IMMUNITIES)
        if (kiter->HasAura(spell))
            return true;

    return false;
}

Creature* GetAnubarakShadowStrikeDuty(Player* bot)
{
    AnubarakState* state = bot ? ReadPhase(GET_PLAYERBOT_AI(bot)) : nullptr;
    if (!state)
        return nullptr;

    Group* group = bot->GetGroup();
    ObjectGuid const groupGuid = group ? group->GetGUID() : bot->GetGUID();
    uint32 const now = getMSTime();
    if (!state->strikeMemoValid || state->strikeMemoMs != now || state->strikeGroup != groupGuid)
    {
        state->strikeMemoValid = true;
        state->strikeMemoMs = now;
        state->strikeGroup = groupGuid;
        ComputeStrikeDuties(bot, *state);
    }

    for (StrikeDuty const& duty : state->strikes)
    {
        if (duty.interrupter != bot->GetGUID())
            continue;

        Creature* caster = ResolveCreature(bot, duty.caster);
        if (caster && caster->IsAlive() && caster->FindCurrentSpellBySpellId(SPELL_SHADOW_STRIKE))
        {
            RaidObs::NoteDerived(bot, "anub.interrupter", "1");
            return caster;
        }
    }

    RaidObs::NoteDerived(bot, "anub.interrupter", "0");
    return nullptr;
}

char const* AnubarakReadyInterrupt(Player* bot, Unit* target)
{
    if (!bot || !target)
        return nullptr;

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
        return nullptr;

    // Holy Wrath stuns undead, and every burrower difficulty entry is undead
    static char const* const interrupts[] = {"kick",       "pummel",          "shield bash", "counterspell",
                                             "wind shear", "mind freeze",     "hammer of justice", "holy wrath",
                                             "shockwave",  "concussion blow", "bash",        "war stomp",
                                             "shadowfury"};

    for (char const* interrupt : interrupts)
        if (botAI->CanCastSpell(interrupt, target))
            return interrupt;

    return nullptr;
}

}
