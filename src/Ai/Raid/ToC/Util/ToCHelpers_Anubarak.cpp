#include "ToCHelpers_Anubarak.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <initializer_list>
#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Creature.h"
#include "EncounterHelpers.h"
#include "GameObject.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Playerbots.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Shared.h"
#include "ToCHelpers_Staging.h"
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

// Heroic kite: one step this far out on each of these headings, judged by the spike's lane to it
constexpr float ANUBARAK_KITE_LANE_STEP = 10.0f;
constexpr uint32 ANUBARAK_KITE_LANE_HEADINGS = 16;
// Lanes this far off everything are all as good, so the step furthest from the spike wins
constexpr float ANUBARAK_KITE_LANE_CAP = 20.0f;
// A leg starting inside an obstacle's clearance passes while it gets no closer than this past its start
constexpr float ANUBARAK_KITE_LANE_START_SLACK = 0.5f;
// Gathered bots dodge a spike lane within 7 yd and stand on 3 yd slots round the gather point
constexpr float ANUBARAK_KITE_GATHER_CLEARANCE = ANUBARAK_SPIKE_LANE_HALF_WIDTH + ANUBARAK_GATHER_SPREAD + 2.0f;

constexpr float ANUBARAK_SCARAB_PICKUP_RANGE = 30.0f;
constexpr float ANUBARAK_PENETRATING_COLD_HEAL_PCT = 90.0f;

// The floor is about 60 yd round the room centre. The Web Door is 83 yd out, and anyone who never
// landed is still up near z 395.
constexpr float ANUBARAK_PIT_RADIUS = 60.0f;
constexpr float ANUBARAK_PIT_CEILING_Z = 200.0f;
constexpr float ANUBARAK_SUBMERGE_SPOT_SEARCH = 40.0f;
constexpr float ANUBARAK_SUBMERGE_SPOT_FLOOR = 35.0f;
// Far from every patch first: the spike summoned on him runs a long way before it can reach one
constexpr float ANUBARAK_SUBMERGE_CLEARANCES[] = {30.0f, 25.0f, ANUBARAK_SUBMERGE_PATCH_CLEARANCE,
                                                  ANUBARAK_SUBMERGE_PATCH_CLEARANCE_MIN};
constexpr float ANUBARAK_TANK_STEP_SEARCH = 15.0f;
// Challenging Roar and Challenging Shout reach 10 yd
constexpr float ANUBARAK_SIDE_SHOUT_RADIUS = 10.0f;
// A human side tank takes the other side once he stands this close to its spot, this much nearer it
// than to his own, for the dwell
constexpr float ANUBARAK_HUMAN_SIDE_REACH = 8.0f;
constexpr float ANUBARAK_HUMAN_SIDE_MARGIN = 4.0f;
constexpr uint32 ANUBARAK_HUMAN_SIDE_DWELL_MS = 3000;
// A human side tank's burrower still loose this long after it came out goes to the bot side tank
constexpr uint32 ANUBARAK_HUMAN_PICKUP_GRACE_MS = 5000;

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
    // Heroic: a speed boost still ready or running goes before any immunity
    bool boostLeft = false;
};

enum class GripMode : uint8
{
    None = 0,
    Active = 1,
    // A grip didn't land, so the rest of the pull shoots its tank patches
    Failed = 2
};

struct GripDuty
{
    ObjectGuid dk;
    ObjectGuid sphere;
    uint8 side = 0;
    // Where the DK grips from: the side's spot, or up to ANUBARAK_GRIP_STEP_OUT off it
    Position stand;
};

struct GripCast
{
    bool valid = false;
    ObjectGuid dk;
    ObjectGuid sphere;
    uint32 castMs = 0;
    bool landed = false;
};

struct BurrowerExit
{
    uint8 side = 0;
    uint32 seenMs = 0;
};

struct SideNeed
{
    bool open = false;
    uint32 sinceMs = 0;
    // The grip gave up on this need, the shooters take it until it closes
    bool shoot = false;
    bool idleValid = false;
    uint32 idleMs = 0;
};

// Heroic: the kiter's stage, memoised per instance per ms
struct KiteStageRead
{
    bool valid = false;
    uint32 ms = 0;
    ObjectGuid kiter;
    AnubarakKiteStage stage = AnubarakKiteStage::None;
    AnubarakKiteBoost boost;
    // A boost ready or one running
    bool boostLeft = false;
    bool immunityComing = false;
};

struct AnubarakState
{
    RaidObs::ObsValue<uint32> phase{"anub.phase"};
    RaidObs::ObsValue<uint32> patches{"anub.patches"};
    RaidObs::ObsValue<uint32> flying{"anub.flying"};
    RaidObs::ObsValue<ObjectGuid> patch0{"anub.patch0"};
    RaidObs::ObsValue<ObjectGuid> patch1{"anub.patch1"};
    RaidObs::ObsValue<ObjectGuid> focusBurrower{"anub.focus"};
    RaidObs::ObsValue<ObjectGuid> side0{"anub.side0"};
    RaidObs::ObsValue<ObjectGuid> side1{"anub.side1"};
    RaidObs::ObsValue<uint32> wave{"anub.wave"};
    RaidObs::ObsValue<uint32> focusDue{"anub.focusdue"};
    RaidObs::ObsValue<uint32> grip{"anub.grip"};
    RaidObs::ObsValue<uint32> allowance{"anub.allow"};
    RaidObs::ObsGuidMap<uint32> burrowerSide{"anub.bside"};

    // 0 is a real getMSTime value, hence the valid flags
    bool engagedSeen = false;
    uint32 lastEngagedMs = 0;
    bool swarm = false;
    // The pull or the last emerge: the script submerges him 80 s after either
    uint32 surfacedMs = 0;
    uint32 submergedMs = 0;
    float surfacedPct = 100.0f;
    bool submergeSpotValid = false;
    Position submergeSpot;
    ObjectGuid mainTank;
    ObjectGuid sideTanks[2];
    // A human side tank has stood at the other side's spot since then
    bool humanSwapValid = false;
    uint32 humanSwapMs = 0;
    // Side tanks that died this pull, for the battle res
    std::vector<ObjectGuid> deadSideTanks;
    // Last marked player seen, so its anub.kite closes when the mark moves on
    ObjectGuid kiter;

    // Every burrower seen this pull: the side of the exit it came out of, and when
    std::unordered_map<ObjectGuid, BurrowerExit> exits;
    bool waveSeen = false;
    uint32 lastWaveMs = 0;
    bool ownerMemoValid = false;
    uint32 ownerMemoMs = 0;
    std::unordered_map<ObjectGuid, uint8> owners;

    // Health lost by him and the burrowers, per sample, over the last 10 s
    bool damageSampled = false;
    uint32 damageSampleMs = 0;
    std::unordered_map<ObjectGuid, uint32> lastHealth;
    std::deque<std::pair<uint32, uint64>> damage;
    bool focusMemoValid = false;
    uint32 focusMemoMs = 0;
    bool focusAhead = false;
    // The focus burrower when the focus came due; it stays due until this one dies
    ObjectGuid focusDueBurrower;

    // Heroic: kite spheres this submerge may spend, and the ones it has
    uint32 kiteAllowance = 0;
    std::vector<ObjectGuid> kiteSpheres;
    KiteStageRead kiteStage;
    // Where he went down, for the heroic phase 2 gather
    bool submergePosValid = false;
    Position submergePos;

    SideNeed needs[2];
    GripMode gripMode = GripMode::None;
    GripCast grips[2];
    std::vector<GripDuty> gripDuties;
    // A sphere that landed on a DK, and the side it was gripped for. A gripped sphere drifts back toward
    // its route while it dies, so its patch can come down nearer the other side's spot.
    std::unordered_map<ObjectGuid, uint8> grippedFor;

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
    state.humanSwapValid = false;
    state.deadSideTanks.clear();
    state.patch0 = ObjectGuid::Empty;
    state.patch1 = ObjectGuid::Empty;
    state.focusBurrower = ObjectGuid::Empty;
    state.wave = 0;
    state.focusDue = 0;
    state.grip = 0;
    state.allowance = 0;
    state.burrowerSide.clear();
    state.surfacedPct = 100.0f;
    state.exits.clear();
    state.waveSeen = false;
    state.ownerMemoValid = false;
    state.owners.clear();
    state.damageSampled = false;
    state.lastHealth.clear();
    state.damage.clear();
    state.focusMemoValid = false;
    state.focusAhead = false;
    state.focusDueBurrower.Clear();
    state.kiteAllowance = 0;
    state.kiteSpheres.clear();
    state.kiteStage = KiteStageRead();
    state.submergePosValid = false;
    state.needs[0] = SideNeed();
    state.needs[1] = SideNeed();
    state.gripMode = GripMode::None;
    state.grips[0] = GripCast();
    state.grips[1] = GripCast();
    state.gripDuties.clear();
    state.grippedFor.clear();
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

// Living tanks on the map other than the main tank, bots first, so a human only takes a side no bot
// can. Each in IsAssistTankOfIndex's order: assistants first.
std::vector<ObjectGuid> RankSideTanks(Player* bot, Group* group, ObjectGuid const& mainTank)
{
    // Bot assistants, other bots, human assistants, other humans
    std::vector<ObjectGuid> ranks[4];
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !member->IsInMap(bot) || member->GetGUID() == mainTank ||
            !IsAnubarakTankPlayer(member))
            continue;

        uint8 const rank = (GET_PLAYERBOT_AI(member) ? 0 : 2) + (group->IsAssistant(member->GetGUID()) ? 0 : 1);
        ranks[rank].push_back(member->GetGUID());
    }

    std::vector<ObjectGuid> roster;
    for (std::vector<ObjectGuid> const& rank : ranks)
        roster.insert(roster.end(), rank.begin(), rank.end());

    return roster;
}

// Each side keeps its tank for the pull while it lives. Re-reading the assist tank index every tick
// swaps sides whenever IsTank blinks. placed says which sides took a new tank.
void RefreshSideTanks(Player* bot, AnubarakState& state, bool placed[2])
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
    {
        if (held.IsEmpty() || std::find(roster.begin(), roster.end(), held) != roster.end())
            continue;

        Player* gone = ObjectAccessor::GetPlayer(*bot, held);
        if (gone && !gone->IsAlive() &&
            std::find(state.deadSideTanks.begin(), state.deadSideTanks.end(), held) == state.deadSideTanks.end())
            state.deadSideTanks.push_back(held);

        held.Clear();
    }

    state.deadSideTanks.erase(std::remove_if(state.deadSideTanks.begin(), state.deadSideTanks.end(),
                                             [bot](ObjectGuid const& guid)
                                             {
                                                 Player* tank = ObjectAccessor::GetPlayer(*bot, guid);
                                                 return !tank || tank->IsAlive();
                                             }),
                              state.deadSideTanks.end());

    for (uint8 side = 0; side < 2; ++side)
    {
        if (!state.sideTanks[side].IsEmpty())
            continue;

        for (ObjectGuid const& guid : roster)
        {
            if (guid != state.sideTanks[1 - side])
            {
                state.sideTanks[side] = guid;
                placed[side] = true;
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
    {
        if (GET_PLAYERBOT_AI(last))
        {
            RaidObs::NoteDerived(last, "anub.kite", "none");
            RaidObs::NoteDerived(last, "anub.kitestage", "none");
        }
    }

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

// The side's patch, else its side point moved with the other side's patch, so a patch shot off its
// spot drags the other side's spot along and the pair keeps its spacing
Position SideSpot(std::vector<Creature*> const& spheres, ObjectGuid const held[2], uint8 side)
{
    Creature* own = FindSphere(spheres, held[side]);
    if (IsPermafrostPatch(own))
        return own->GetPosition();

    Position const point = GetAnubarakTankSidePoint(side);
    Creature* other = FindSphere(spheres, held[1 - side]);
    if (!IsPermafrostPatch(other))
        return point;

    Position const otherPoint = GetAnubarakTankSidePoint(1 - side);
    return Position(other->GetPositionX() + point.GetPositionX() - otherPoint.GetPositionX(),
                    other->GetPositionY() + point.GetPositionY() - otherPoint.GetPositionY(), point.GetPositionZ());
}

// Within reach of the spot and clearly nearer it than the other one
bool StandsAtSpot(Player* player, Position const& spot, Position const& otherSpot)
{
    float const dist = Dist2d(player->GetPosition(), spot);
    return dist <= ANUBARAK_HUMAN_SIDE_REACH &&
           dist + ANUBARAK_HUMAN_SIDE_MARGIN <= Dist2d(player->GetPosition(), otherSpot);
}

// The side whose human tank stands at the other side's spot, else -1. Never between two humans: there's
// no bot side tank to move.
int8 HumanOnOtherSpot(Player* bot, ObjectGuid const sides[2], Position const spots[2])
{
    Player* tanks[2];
    for (uint8 side = 0; side < 2; ++side)
        tanks[side] = sides[side].IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(*bot, sides[side]);

    for (uint8 side = 0; side < 2; ++side)
    {
        Player* other = tanks[1 - side];
        if (tanks[side] && !GET_PLAYERBOT_AI(tanks[side]) && (!other || GET_PLAYERBOT_AI(other)) &&
            StandsAtSpot(tanks[side], spots[1 - side], spots[side]))
            return static_cast<int8>(side);
    }

    return -1;
}

// Bots can't tell a human his side, so it's the tank spot he stands at and the bot side tank takes the
// other. At once when a side just took a new tank, else after he has stood there for the dwell, so
// walking past doesn't swap.
void FollowHumanSideTank(Player* bot, AnubarakState& state, std::vector<Creature*> const& spheres,
                         bool const placed[2], uint32 now)
{
    ObjectGuid const held[2] = {state.patch0.Get(), state.patch1.Get()};
    Position const spots[2] = {SideSpot(spheres, held, 0), SideSpot(spheres, held, 1)};
    if (HumanOnOtherSpot(bot, state.sideTanks, spots) < 0)
    {
        state.humanSwapValid = false;
        return;
    }

    if (!placed[0] && !placed[1])
    {
        if (!state.humanSwapValid)
        {
            state.humanSwapValid = true;
            state.humanSwapMs = now;
        }

        if (getMSTimeDiff(state.humanSwapMs, now) < ANUBARAK_HUMAN_SIDE_DWELL_MS)
            return;
    }

    std::swap(state.sideTanks[0], state.sideTanks[1]);
    state.humanSwapValid = false;
}

// Within the latch of this side's spot and nearer it than the other side's, so a patch 14 yd off
// never latches to the wrong side
bool ServesSide(Unit* sphere, Position const& spot, Position const& otherSpot)
{
    float const dist = Dist2d(sphere->GetPosition(), spot);
    return dist <= ANUBARAK_TANK_PATCH_LATCH && dist <= Dist2d(sphere->GetPosition(), otherSpot);
}

int8 GrippedSide(AnubarakState const& state, ObjectGuid const& guid)
{
    auto const it = state.grippedFor.find(guid);
    return it == state.grippedFor.end() ? -1 : static_cast<int8>(it->second);
}

// A gripped sphere is the side's it was gripped for, wherever it drifted to. Any other by ServesSide.
bool PatchFor(AnubarakState const& state, Unit* sphere, uint8 side, Position const& spot, Position const& otherSpot)
{
    int8 const gripped = GrippedSide(state, sphere->GetGUID());
    if (gripped >= 0)
        return gripped == side && Dist2d(sphere->GetPosition(), spot) <= ANUBARAK_TANK_PATCH_LATCH;

    return ServesSide(sphere, spot, otherSpot);
}

bool FallingFor(AnubarakState const& state, std::vector<Creature*> const& spheres, uint8 side, Position const& spot,
                Position const& otherSpot)
{
    return std::any_of(spheres.begin(), spheres.end(), [&](Creature* sphere)
                       { return IsFrostSphereFalling(sphere) && PatchFor(state, sphere, side, spot, otherSpot); });
}

// Holds each side's patch while it lasts. Never both sides on one patch, and a gripped one only where it
// keeps the gap to the other side's.
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

        Position const point = SideSpot(spheres, held, side);
        Position const otherPoint = SideSpot(spheres, held, 1 - side);
        Creature* other = FindSphere(spheres, held[1 - side]);
        Creature* nearest = nullptr;
        float nearestDist = 0.0f;
        bool nearestGripped = false;
        for (Creature* sphere : spheres)
        {
            if (!IsPermafrostPatch(sphere) || sphere == other || !PatchFor(state, sphere, side, point, otherPoint))
                continue;

            bool const gripped = GrippedSide(state, sphere->GetGUID()) == static_cast<int8>(side);
            if (gripped && other && Dist2d(sphere->GetPosition(), other->GetPosition()) < ANUBARAK_TANK_PATCH_GAP)
                continue;

            float const dist = Dist2d(sphere->GetPosition(), point);
            if (!nearest || (gripped && !nearestGripped) || (gripped == nearestGripped && dist < nearestDist))
            {
                nearest = sphere;
                nearestDist = dist;
                nearestGripped = gripped;
            }
        }

        if (nearest)
            held[side] = nearest->GetGUID();
    }

    for (auto it = state.grippedFor.begin(); it != state.grippedFor.end();)
    {
        bool const done = !FindSphere(spheres, it->first) || it->first == held[0] || it->first == held[1];
        it = done ? state.grippedFor.erase(it) : std::next(it);
    }

    state.patch0 = held[0];
    state.patch1 = held[1];
}

// A burrower is seen first at its exit: the two north exits feed side 0, the two south side 1. A new
// one more than 10 s after the last opens a wave.
void TrackWaves(Player* bot, AnubarakState& state, uint32 now)
{
    for (Creature* burrower : ResolveAlive(bot, state.burrowers))
    {
        ObjectGuid const guid = burrower->GetGUID();
        if (state.exits.count(guid))
            continue;

        uint8 const side = burrower->GetPositionY() >= ANUBARAK_ROOM_CENTER.GetPositionY() ? 0 : 1;
        state.exits[guid] = BurrowerExit{side, now};
        if (!state.waveSeen || getMSTimeDiff(state.lastWaveMs, now) > ANUBARAK_WAVE_GAP_MS)
        {
            state.waveSeen = true;
            state.lastWaveMs = now;
            state.wave = state.wave.Get() + 1;
        }
    }
}

// When the next wave spawns, the earliest the script allows. None while he is down, and the wave due
// after his submerge comes 5 s after the emerge instead.
uint32 NextWaveMs(AnubarakState const& state)
{
    uint32 const emerge = state.submergedMs + ANUBARAK_SUBMERGED_MS + ANUBARAK_WAVE_FIRST_MS;
    if (state.phaseValue == AnubarakPhase::Submerged)
        return emerge;

    bool const sinceSurface =
        state.waveSeen && static_cast<int32>(state.lastWaveMs - state.surfacedMs) >= 0;
    uint32 const next = sinceSurface ? state.lastWaveMs + ANUBARAK_WAVE_INTERVAL_MS
                                     : state.surfacedMs + ANUBARAK_WAVE_FIRST_MS;
    if (state.phaseValue != AnubarakPhase::Surface)
        return next;

    uint32 const submerge = state.surfacedMs + ANUBARAK_SURFACED_MS;
    return static_cast<int32>(next - submerge) > 0 ? submerge + ANUBARAK_SUBMERGED_MS + ANUBARAK_WAVE_FIRST_MS
                                                   : next;
}

int32 MsUntilWave(AnubarakState const& state, uint32 now) { return static_cast<int32>(NextWaveMs(state) - now); }

// Health lost by him and the burrowers since the last sample, a dead burrower's last health included.
// A burrower back from its submerge regains health, which counts as nothing.
void SampleDamage(Player* bot, AnubarakState& state, Creature* boss, uint32 now)
{
    if (state.damageSampled && getMSTimeDiff(state.damageSampleMs, now) < ANUBARAK_DAMAGE_SAMPLE_MS)
        return;

    state.damageSampled = true;
    state.damageSampleMs = now;

    std::unordered_map<ObjectGuid, uint32> health;
    uint64 dealt = 0;
    auto const take = [&](Creature* unit)
    {
        uint32 const hp = unit->GetHealth();
        health[unit->GetGUID()] = hp;
        auto const last = state.lastHealth.find(unit->GetGUID());
        if (last != state.lastHealth.end() && last->second > hp)
            dealt += last->second - hp;
    };

    take(boss);
    for (Creature* burrower : ResolveAlive(bot, state.burrowers))
        take(burrower);

    for (auto const& [guid, last] : state.lastHealth)
        if (!health.count(guid))
            dealt += last;

    state.lastHealth = std::move(health);
    state.damage.emplace_back(now, dealt);
    while (state.damage.size() > 1 && getMSTimeDiff(state.damage.front().first, now) > ANUBARAK_DAMAGE_WINDOW_MS)
        state.damage.pop_front();
}

// Per second over the window, 0 until it spans 3 s. The oldest sample only marks where the window starts.
float DamageRate(AnubarakState const& state)
{
    if (state.damage.size() < 2)
        return 0.0f;

    uint32 const span = getMSTimeDiff(state.damage.front().first, state.damage.back().first);
    if (span < ANUBARAK_DAMAGE_MIN_SPAN_MS)
        return 0.0f;

    uint64 dealt = 0;
    for (auto it = std::next(state.damage.begin()); it != state.damage.end(); ++it)
        dealt += it->second;

    return static_cast<float>(dealt) * IN_MILLISECONDS / span;
}

// The share of the remaining spheres this submerge's kites may spend: what's left after the missing
// tank patches, over the submerges still to come at his loss per surfaced phase, at least one
void ComputeKiteAllowance(Player* bot, AnubarakState& state, std::vector<Creature*> const& spheres, Creature* boss)
{
    state.kiteSpheres.clear();

    uint32 flying = 0;
    for (Creature* sphere : spheres)
        if (IsFrostSphereFlying(sphere))
            ++flying;

    uint32 missing = 0;
    bool const tenMan = !bot->GetMap()->Is25ManRaid();
    for (uint8 side = 0; side < 2; ++side)
        if (!IsPermafrostPatch(FindSphere(spheres, side == 0 ? state.patch0.Get() : state.patch1.Get())) &&
            !(tenMan && state.sideTanks[side].IsEmpty()))
            ++missing;

    uint32 const spare = flying > missing ? flying - missing : 0;
    float const pct = boss->GetHealthPct();
    float const loss = std::max(state.surfacedPct - pct, 1.0f);
    uint32 const submerges =
        pct > ANUBARAK_SWARM_HEALTH_PCT ? static_cast<uint32>(std::ceil((pct - ANUBARAK_SWARM_HEALTH_PCT) / loss)) : 1;

    state.kiteAllowance = spare ? std::max<uint32>(1, spare / std::max<uint32>(submerges, 1)) : 0;
    state.allowance = state.kiteAllowance;
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
        state->surfacedPct = boss->GetHealthPct();
        state->submergeSpotValid = false;
    }

    bool const submerging = state->phaseValue == AnubarakPhase::Submerged && before == AnubarakPhase::Surface;
    if (submerging)
    {
        state->submergedMs = now;
        state->submergePosValid = true;
        state->submergePos = boss->GetPosition();
    }

    // Hit rows on spheres say who downed each one, the pet or the splash. Asked again on every phase
    // edge, since the trace can open after the first read.
    if (state->phaseValue != before)
        RaidObs::LogHitsOn(bot, static_cast<uint32>(ToCNpcs::NPC_FROST_SPHERE));

    TrackBossStillness(*state, boss, now);
    if (state->phaseValue == AnubarakPhase::Submerged)
        state->drag = DragLegLatch();

    std::vector<Creature*> const spheres = ResolveSpheres(bot, *state);
    RefreshTankPatches(*state, spheres);

    bool placed[2] = {false, false};
    RefreshSideTanks(bot, *state, placed);
    FollowHumanSideTank(bot, *state, spheres, placed, now);
    state->side0 = state->sideTanks[0];
    state->side1 = state->sideTanks[1];

    TrackWaves(bot, *state, now);
    SampleDamage(bot, *state, boss, now);
    if (submerging && AnubarakHeroic(bot))
        ComputeKiteAllowance(bot, *state, spheres, boss);

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

std::vector<Position> PatchPositions(Player* bot, AnubarakState const& state)
{
    std::vector<Position> patches;
    for (Creature* sphere : ResolveSpheres(bot, state))
        if (IsPermafrostPatch(sphere))
            patches.push_back(sphere->GetPosition());

    return patches;
}

// Heroic phase 2: 5 yd past where he went down, away from the tank patches, so the spike starts there
// and runs for the kiter away from the raid. Never further out than the floor radius.
bool GatherCentre(Player* bot, AnubarakState const& state, Position& out)
{
    if (state.phaseValue != AnubarakPhase::Submerged || !state.submergePosValid || !AnubarakHeroic(bot))
        return false;

    Position const& down = state.submergePos;
    Position from = ANUBARAK_ROOM_CENTER;
    uint32 count = 0;
    float sumX = 0.0f;
    float sumY = 0.0f;
    for (uint8 side = 0; side < 2; ++side)
    {
        if (Creature* patch = LatchedTankPatch(bot, state, side))
        {
            sumX += patch->GetPositionX();
            sumY += patch->GetPositionY();
            ++count;
        }
    }

    if (count)
        from.Relocate(sumX / count, sumY / count);

    Vec2 away = Delta(from, down);
    float const length = Length(away);
    Position gather = length > 1.0f ? Offset(down, {away.x / length, away.y / length}, ANUBARAK_GATHER_PAST) : down;

    Vec2 const fromCenter = Delta(ANUBARAK_ROOM_CENTER, gather);
    float const radius = Length(fromCenter);
    if (radius > ANUBARAK_GATHER_FLOOR)
        gather = Offset(ANUBARAK_ROOM_CENTER, {fromCenter.x / radius, fromCenter.y / radius}, ANUBARAK_GATHER_FLOOR);

    gather.m_positionZ = down.GetPositionZ();
    out = gather;
    return true;
}

bool TankWalkClear(Position const& from, Position const& to, std::vector<Position> const& patches)
{
    return std::none_of(patches.begin(), patches.end(), [&](Position const& patch)
                        {
                            return Dist2d(to, patch) < ANUBARAK_TANK_KEEPOUT ||
                                   SegmentDistance2d(from, to, patch) < ANUBARAK_PERMAFROST_SLOW_REACH;
                        });
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

// What the heroic spike's run keeps clear of: every patch, and the phase 2 gather
struct LaneObstacle
{
    Position pos;
    float clearance;
};

std::vector<LaneObstacle> LaneObstacles(Player* kiter, AnubarakState const& state)
{
    std::vector<LaneObstacle> obstacles;
    for (Position const& patch : PatchPositions(kiter, state))
        obstacles.push_back({patch, ANUBARAK_SPIKE_ROUTE_CLEARANCE});

    Position gather;
    if (GatherCentre(kiter, state, gather))
        obstacles.push_back({gather, ANUBARAK_KITE_GATHER_CLEARANCE});

    return obstacles;
}

// Twice the signed area of a, b, p; its sign says which side of a to b p is on
float Cross(Position const& a, Position const& b, Position const& p)
{
    Vec2 const ab = Delta(a, b);
    Vec2 const ap = Delta(a, p);
    return ab.x * ap.y - ab.y * ap.x;
}

// A flat triangle reads as holding every point, so one under a square yard holds none
bool InsideTriangle(Position const& a, Position const& b, Position const& c, Position const& p)
{
    if (std::fabs(Cross(a, b, c)) < 2.0f)
        return false;

    float const d1 = Cross(a, b, p);
    float const d2 = Cross(b, c, p);
    float const d3 = Cross(c, a, p);
    bool const negative = d1 < 0.0f || d2 < 0.0f || d3 < 0.0f;
    bool const positive = d1 > 0.0f || d2 > 0.0f || d3 > 0.0f;
    return !(negative && positive);
}

// A leg that starts inside the clearance only has to head away
bool LegClears(Position const& from, Position const& to, LaneObstacle const& obstacle)
{
    float const need =
        std::min(obstacle.clearance, Dist2d(from, obstacle.pos) - ANUBARAK_KITE_LANE_START_SLACK);
    return SegmentDistance2d(from, to, obstacle.pos) >= need;
}

// The spike chases the kiter, so its run to the step stays inside the triangle spike, kiter, step.
// walked is false for a Blink, which leaves no trail for the spike to cut along.
bool LaneClears(Position const& spike, Position const& kiter, Position const& step,
                std::vector<LaneObstacle> const& obstacles, bool walked)
{
    return std::none_of(obstacles.begin(), obstacles.end(), [&](LaneObstacle const& obstacle)
                        {
                            return InsideTriangle(spike, kiter, step, obstacle.pos) ||
                                   !LegClears(spike, step, obstacle) || (walked && !LegClears(kiter, step, obstacle));
                        });
}

// The spike's lane past the obstacle it comes closest to, less that one's clearance, capped
float LaneScore(Position const& spike, Position const& kiter, Position const& step,
                std::vector<LaneObstacle> const& obstacles)
{
    float score = ANUBARAK_KITE_LANE_CAP - ANUBARAK_SPIKE_ROUTE_CLEARANCE;
    for (LaneObstacle const& obstacle : obstacles)
    {
        float const lane = InsideTriangle(spike, kiter, step, obstacle.pos)
                               ? 0.0f
                               : SegmentDistance2d(spike, step, obstacle.pos);
        score = std::min(score, lane - obstacle.clearance);
    }

    return score;
}

constexpr uint32 SPRINT_RANKS[] = {11305, 8696, 2983};
constexpr uint32 DASH_RANKS[] = {33357, 9821, 1850};
constexpr uint32 HAND_OF_PROTECTION_RANKS[] = {10278, 5599, 1022};
constexpr uint32 SPELL_BLINK = 1953;
constexpr uint32 SPELL_DIVINE_SHIELD = 642;
constexpr uint32 SPELL_CIRCLE_SUMMON = 48018;
constexpr uint32 SPELL_CIRCLE_TELEPORT = 48020;
// On the warlock while its circle is within the teleport's 40 yd
constexpr uint32 SPELL_CIRCLE_IN_RANGE = 62388;

template <size_t N>
uint32 KnownRank(Player* player, uint32 const (&ranks)[N])
{
    for (uint32 rank : ranks)
        if (player->HasSpell(rank))
            return rank;

    return 0;
}

// Known, instant, off cooldown and affordable
bool ReadyInstant(Player* player, uint32 spellId)
{
    SpellInfo const* info = spellId && player->HasSpell(spellId) ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    if (!info || player->HasSpellCooldown(spellId) || info->CalcCastTime(player) ||
        info->PowerType >= static_cast<uint32>(MAX_POWERS))
        return false;

    return info->CalcPowerCost(player, info->GetSchoolMask()) <=
           static_cast<int32>(player->GetPower(Powers(info->PowerType)));
}

// Sprint, Dash and Nitro Boosts share one speed aura type, so only the fastest counts
bool SpeedBoostRunning(Player* kiter)
{
    if (kiter->HasAura(NITRO_BOOSTS_SPEED_SPELL))
        return true;

    for (uint32 rank : SPRINT_RANKS)
        if (kiter->HasAura(rank))
            return true;

    for (uint32 rank : DASH_RANKS)
        if (kiter->HasAura(rank))
            return true;

    return false;
}

GameObject* WarlockCircle(Player* player)
{
    return player->getClass() == CLASS_WARLOCK ? player->GetGameObject(SPELL_CIRCLE_SUMMON) : nullptr;
}

// The next speed boost in the chain: Sprint, Dash, Nitro Boosts, Blink the way it runs when the spike's
// lane to the landing stays clear, then the teleport back to a circle well behind the spike
bool NextKiteBoost(Player* kiter, Creature* spike, std::vector<LaneObstacle> const& obstacles,
                   AnubarakKiteBoost& out)
{
    out = AnubarakKiteBoost();
    if (!SpeedBoostRunning(kiter))
    {
        uint32 rank = 0;
        if (kiter->getClass() == CLASS_ROGUE)
            rank = KnownRank(kiter, SPRINT_RANKS);
        else if (kiter->getClass() == CLASS_DRUID && kiter->GetShapeshiftForm() == FORM_CAT)
            rank = KnownRank(kiter, DASH_RANKS);

        if (rank && ReadyInstant(kiter, rank))
        {
            out.spellId = rank;
            return true;
        }

        if (NitroBoostsReady(kiter))
        {
            out.nitro = true;
            return true;
        }
    }

    if (kiter->getClass() == CLASS_MAGE && ReadyInstant(kiter, SPELL_BLINK))
    {
        float const facing = kiter->GetOrientation();
        Position const landing =
            Offset(kiter->GetPosition(), {std::cos(facing), std::sin(facing)}, ANUBARAK_BLINK_DISTANCE);
        if (Dist2d(landing, ANUBARAK_ROOM_CENTER) <= ANUBARAK_PIT_RADIUS - ANUBARAK_BLINK_DISTANCE / 2.0f &&
            Dist2d(landing, spike->GetPosition()) > Dist2d(kiter->GetPosition(), spike->GetPosition()) &&
            LaneClears(spike->GetPosition(), kiter->GetPosition(), landing, obstacles, false))
        {
            out.spellId = SPELL_BLINK;
            out.face = true;
            out.facing = facing;
            return true;
        }
    }

    GameObject* circle = WarlockCircle(kiter);
    if (circle && kiter->HasAura(SPELL_CIRCLE_IN_RANGE) && ReadyInstant(kiter, SPELL_CIRCLE_TELEPORT) &&
        Dist2d(circle->GetPosition(), spike->GetPosition()) >=
            Dist2d(kiter->GetPosition(), spike->GetPosition()) + ANUBARAK_CIRCLE_TELEPORT_GAIN)
    {
        out.spellId = SPELL_CIRCLE_TELEPORT;
        return true;
    }

    return false;
}

// Hand of Protection from a paladin bot in reach, or the kiter's own Divine Shield or Hand
bool ImmunityComing(Player* kiter)
{
    if (kiter->HasAura(SPELL_FORBEARANCE))
        return false;

    if (kiter->getClass() == CLASS_PALADIN && GET_PLAYERBOT_AI(kiter) &&
        (ReadyInstant(kiter, SPELL_DIVINE_SHIELD) || ReadyInstant(kiter, KnownRank(kiter, HAND_OF_PROTECTION_RANKS))))
        return true;

    bool coming = false;
    ForEachGroupMember(kiter, [&](Player* member)
    {
        PlayerbotAI* ai = member != kiter && member->IsAlive() ? GET_PLAYERBOT_AI(member) : nullptr;
        coming = ai && member->getClass() == CLASS_PALADIN && !member->HasAura(SPELL_MARK) &&
                 member->IsWithinDistInMap(kiter, ANUBARAK_HOP_RANGE) && member->IsWithinLOSInMap(kiter) &&
                 ai->CanCastSpell("hand of protection", kiter);
        return coming;
    });

    return coming;
}

int32 ImmunityLeftMs(Unit* kiter)
{
    int32 left = 0;
    for (uint32 spell : ANUBARAK_KITE_IMMUNITIES)
        if (Aura const* aura = kiter->GetAura(spell))
            left = std::max(left, aura->GetDuration());

    return left;
}

// Heroic only. Runs until the spike gains within 15 yd, then the next boost; immune it keeps running and
// turns for its patch 2 s before the end; with nothing left a patch once the spike is within 9 yd.
KiteStageRead const& ReadKiteStage(Player* kiter, Creature* spike, AnubarakState& state)
{
    KiteStageRead& read = state.kiteStage;
    uint32 const now = getMSTime();
    if (read.valid && read.ms == now && read.kiter == kiter->GetGUID())
        return read;

    read = KiteStageRead();
    read.valid = true;
    read.ms = now;
    read.kiter = kiter->GetGUID();

    bool const bot = GET_PLAYERBOT_AI(kiter) != nullptr;
    bool const hasBoost = bot && NextKiteBoost(kiter, spike, LaneObstacles(kiter, state), read.boost);
    read.boostLeft = hasBoost || SpeedBoostRunning(kiter);
    read.immunityComing = ImmunityComing(kiter);

    float const dist = Dist2d(spike->GetPosition(), kiter->GetPosition());
    bool const gaining = spike->GetSpeed(MOVE_RUN) > kiter->GetSpeed(MOVE_RUN);
    if (AnubarakKiterImmune(kiter))
        read.stage = ImmunityLeftMs(kiter) <= static_cast<int32>(ANUBARAK_KITE_IMMUNE_LEFT_MS)
                         ? AnubarakKiteStage::Patch
                         : AnubarakKiteStage::Immune;
    else if (hasBoost && gaining && dist <= ANUBARAK_KITE_BOOST_RANGE)
        read.stage = AnubarakKiteStage::Boost;
    else if (!read.boostLeft && !read.immunityComing && dist <= ANUBARAK_KITE_IMMUNITY_RANGE)
        read.stage = AnubarakKiteStage::Patch;
    else
        read.stage = AnubarakKiteStage::Run;

    if (bot)
    {
        char const* const names[] = {"none", "run", "boost", "immune", "patch"};
        uint32 const boostId = read.boost.nitro ? NITRO_BOOSTS_USE_SPELL : read.boost.spellId;
        RaidObs::NoteDerived(kiter, "anub.kitestage",
                             read.stage == AnubarakKiteStage::Boost
                                 ? "boost:" + std::to_string(boostId)
                                 : std::string(names[static_cast<uint8>(read.stage)]));
    }

    return read;
}

struct LaneStep
{
    Position raw;
    bool clear;
    float score;
    float fromSpike;
};

// Heroic run with no patch: a 10 yd step that keeps the spike behind and no nearer, inside the ring
// radius (or no further out than the kiter already is), its lane clear of every obstacle. The widest
// lane wins, then the step furthest from the spike; a warlock keeps its circle within teleport reach.
// None clear: the widest lane anyway. None at all: the plain ring step.
Position LaneStepStand(Player* kiter, Creature* spike, std::vector<LaneObstacle> const& obstacles)
{
    Position const& from = kiter->GetPosition();
    Position const& spikePos = spike->GetPosition();
    float const gap = Dist2d(from, spikePos);
    float const radius = std::max(ANUBARAK_KITE_RING_RADIUS, Dist2d(from, ANUBARAK_ROOM_CENTER));
    GameObject* circle = WarlockCircle(kiter);

    std::vector<LaneStep> steps;
    for (uint32 i = 0; i < ANUBARAK_KITE_LANE_HEADINGS; ++i)
    {
        float const angle = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) /
                            static_cast<float>(ANUBARAK_KITE_LANE_HEADINGS);
        Position const raw = Offset(from, {std::cos(angle), std::sin(angle)}, ANUBARAK_KITE_LANE_STEP);
        float const fromSpike = Dist2d(raw, spikePos);
        if (fromSpike < gap || !SpikeBehind(kiter, spike, raw) || Dist2d(raw, ANUBARAK_ROOM_CENTER) > radius)
            continue;

        bool const clear = LaneClears(spikePos, from, raw, obstacles, true) &&
                           (!circle || Dist2d(raw, circle->GetPosition()) <= ANUBARAK_CIRCLE_TELEPORT_RANGE);
        steps.push_back({raw, clear, LaneScore(spikePos, from, raw, obstacles), fromSpike});
    }

    std::sort(steps.begin(), steps.end(), [](LaneStep const& a, LaneStep const& b)
              {
                  if (a.clear != b.clear)
                      return a.clear;
                  if (a.score != b.score)
                      return a.score > b.score;
                  return a.fromSpike > b.fromSpike;
              });

    // Best first, so the floor raycasts stop at the first step that lands on the floor
    for (LaneStep const& step : steps)
    {
        Position valid;
        if (OnFloor(kiter, step.raw, valid))
            return valid;
    }

    return RingStand(kiter, spike);
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
                         std::vector<Position> const& patches, float clearance)
{
    for (Position const& patch : patches)
        if (SegmentDistance2d(spike, kiter, patch) < clearance || SegmentDistance2d(kiter, stand, patch) < clearance)
            return true;

    return false;
}

// After a fail the spike restarts from the kite patch it teleported onto, and its first test reaches
// 11.5 yd plus its 5.25 yd first walk, so heroic never kites onto a patch that close to a tank patch
bool NearTankPatch(Position const& patch, std::vector<Position> const& tankSpots)
{
    return std::any_of(tankSpots.begin(), tankSpots.end(), [&patch](Position const& tank)
                       { return Dist2d(patch, tank) < ANUBARAK_SUBMERGE_PATCH_CLEARANCE_MIN; });
}

// seekPatch false runs the ring without looking for a patch: heroic until the kite stage says patch.
// lane is what heroic's run keeps the spike off.
void ComputeKiteStand(Player* kiter, Creature* spike, std::vector<Creature*> const& spheres,
                      ObjectGuid const tankPatches[2], bool heroic, bool burrowers,
                      std::vector<LaneObstacle> const& lane, AnubarakKiteStand const* previous, bool seekPatch,
                      AnubarakKiteStand& out)
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
        if (!seekPatch)
            break;

        if (!IsPermafrostPatch(sphere))
            continue;

        bool const tankPatch = sphere->GetGUID() == tankPatches[0] || sphere->GetGUID() == tankPatches[1];
        if (heroic && !tankPatch && NearTankPatch(sphere->GetPosition(), tankSpots))
            continue;

        Vec2 away = Delta(spikePos, sphere->GetPosition());
        float const length = Length(away);
        // The spike is on it, so it's already being used up
        if (length < 0.5f)
            continue;

        away = {away.x / length, away.y / length};
        Position const raw = Offset(sphere->GetPosition(), away, ANUBARAK_KITE_STAND_OFFSET);
        candidates.push_back({sphere, away, kiter->GetExactDist2d(raw.GetPositionX(), raw.GetPositionY()), tankPatch});
    }

    Creature* chosenPatch = nullptr;
    Position chosenStand;
    float const routeClearance = heroic ? ANUBARAK_SPIKE_ROUTE_CLEARANCE : ANUBARAK_SPIKE_PATCH_REACH;

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
                    SpikeRouteNearPatch(spikePos, kiter->GetPosition(), stand, tankSpots, routeClearance))
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

        // Tank patches last: a held burrower off Permafrost submerges back to full, and a stand past a
        // tank patch puts its side tank in the spike's lane. Heroic never: it gets no spheres back.
        if (heroic)
            firstOf({StandTier::NonTankClear, StandTier::NonTank});
        else if (burrowers)
            firstOf({StandTier::NonTank, StandTier::Tank});
        else
            firstOf({StandTier::Any});
    }

    if (!chosenPatch)
    {
        out.stand = heroic ? LaneStepStand(kiter, spike, lane) : RingStand(kiter, spike);
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

char const* const SIDE_LABELS[2] = {"0", "1"};
char const* const GRIP_LABELS[2] = {"grip0", "grip1"};

// A living bot side tank, side 0 first, else whichever side is held
ObjectGuid FirstBotSideTank(Player* bot, ObjectGuid const sides[2])
{
    for (uint8 side = 0; side < 2; ++side)
        if (Player* tank = sides[side].IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(*bot, sides[side]))
            if (tank->IsAlive() && GET_PLAYERBOT_AI(tank))
                return sides[side];

    return sides[0].IsEmpty() ? sides[1] : sides[0];
}

// Who holds him: the main tank while alive. Else the side tank he's on, so a human who taunted him keeps
// him and no bot taunts him back, else the first bot side tank.
ObjectGuid PickupGuid(Player* bot, AnubarakState const& state)
{
    Player* main = state.mainTank.IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(*bot, state.mainTank);
    if (main && main->IsAlive())
        return state.mainTank;

    Creature* boss = GetAnubarak(bot);
    Unit* victim = boss ? boss->GetVictim() : nullptr;
    if (victim && (victim->GetGUID() == state.sideTanks[0] || victim->GetGUID() == state.sideTanks[1]))
        return victim->GetGUID();

    return FirstBotSideTank(bot, state.sideTanks);
}

// A DK side tank grips for its own side, else the lowest-guid DK bot holding no tank duty, else, before
// any burrower is up, the other side's DK side tank. Only one whose grip is back within the wait.
Player* PickGripper(Player* bot, AnubarakState const& state, uint8 side, ObjectGuid const& busy)
{
    ObjectGuid const pickup = PickupGuid(bot, state);
    auto const eligible = [&](Player* member)
    {
        return member && member->IsAlive() && GET_PLAYERBOT_AI(member) && member->getClass() == CLASS_DEATH_KNIGHT &&
               member->HasSpell(SPELL_DEATH_GRIP) && !member->HasAura(SPELL_MARK) && InAnubarakPit(member) &&
               member->GetGUID() != pickup && member->GetGUID() != busy &&
               member->GetSpellCooldownDelay(SPELL_DEATH_GRIP) <= ANUBARAK_GRIP_COOLDOWN_WAIT_MS;
    };

    Player* own = state.sideTanks[side].IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(*bot, state.sideTanks[side]);
    if (eligible(own))
        return own;

    Player* best = nullptr;
    ForEachGroupMember(bot, [&](Player* member)
    {
        if (eligible(member) && member->GetGUID() != state.sideTanks[0] && member->GetGUID() != state.sideTanks[1] &&
            (!best || member->GetGUID() < best->GetGUID()))
            best = member;

        return false;
    });

    if (best)
        return best;

    uint8 const otherSide = 1 - side;
    Player* other =
        state.sideTanks[otherSide].IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(*bot, state.sideTanks[otherSide]);
    return eligible(other) && !AnyBurrower(bot, state) ? other : nullptr;
}

// Nearest flying sphere within reach of the spot, the last pick while it still is
template <typename Taken>
Creature* GripSphere(std::vector<GripDuty> const& previous, std::vector<Creature*> const& flying, Position const& spot,
                     uint8 side, float within, Taken const& taken)
{
    auto const reach = [&](Creature* sphere)
    {
        return !taken(sphere) &&
               sphere->GetExactDist(spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ()) <= within;
    };

    for (GripDuty const& duty : previous)
        if (duty.side == side)
            if (Creature* held = FindSphere(flying, duty.sphere))
                if (reach(held))
                    return held;

    Creature* best = nullptr;
    float bestDist = 0.0f;
    for (Creature* sphere : flying)
    {
        if (!reach(sphere))
            continue;

        float const dist = sphere->GetExactDist(spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ());
        if (!best || dist < bestDist)
        {
            best = sphere;
            bestDist = dist;
        }
    }

    return best;
}

// The spot, else up to ANUBARAK_GRIP_STEP_OUT toward a sphere out of reach of it, never nearer the other
// side's spot than the spot is, so the two patches keep their gap
Position GripStand(Player* bot, Position const& spot, Position const& otherSpot, Creature* sphere)
{
    if (sphere->GetExactDist(spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ()) <= ANUBARAK_GRIP_RANGE)
        return spot;

    Vec2 toward = Delta(spot, sphere->GetPosition());
    float const length = Length(toward);
    if (length < 1.0f)
        return spot;

    toward = {toward.x / length, toward.y / length};
    Vec2 away = Delta(otherSpot, spot);
    float const awayLength = Length(away);
    if (awayLength >= 1.0f)
    {
        away = {away.x / awayLength, away.y / awayLength};
        float const along = toward.x * away.x + toward.y * away.y;
        if (along < 0.0f)
        {
            toward = {toward.x - along * away.x, toward.y - along * away.y};
            float const sideways = Length(toward);
            if (sideways < 0.1f)
                return spot;

            toward = {toward.x / sideways, toward.y / sideways};
        }
    }

    // Only as far as the sphere needs: it flies about 14 yd up
    float const height = sphere->GetPositionZ() - spot.GetPositionZ();
    float const flat = std::sqrt(std::max(ANUBARAK_GRIP_RANGE * ANUBARAK_GRIP_RANGE - height * height, 0.0f));
    float const step = std::clamp(length - flat + 1.0f, 0.0f, ANUBARAK_GRIP_STEP_OUT);
    return ValidateFloorPoint(bot, Offset(spot, toward, step));
}

// True while a grip serves the side. The sphere it pulls is reserved, and the side's shooters join only
// once it has landed on the DK. A grip still in the air 3 s on switches the pull to shooting. No grip
// takes a sphere already near the other side's spot, or one another side's DK has picked.
template <typename Taken>
bool ServeSideByGrip(Player* bot, AnubarakState& state, uint8 side, Position const& spot, Position const& otherSpot,
                     std::vector<Creature*> const& spheres, std::vector<Creature*> const& flying,
                     std::vector<GripDuty> const& previousGrips, Taken const& taken,
                     std::vector<std::pair<char const*, Creature*>>& needs, std::vector<Creature*>& reserved,
                     uint32 now)
{
    SideNeed& need = state.needs[side];
    if (state.gripMode == GripMode::Failed || need.shoot)
        return false;

    GripCast& cast = state.grips[side];
    if (cast.valid)
    {
        Creature* sphere = FindSphere(spheres, cast.sphere);
        Player* dk = ObjectAccessor::GetPlayer(*bot, cast.dk);
        if (IsFrostSphereFlying(sphere) && dk && dk->IsAlive())
        {
            if (Dist2d(sphere->GetPosition(), dk->GetPosition()) <= ANUBARAK_GRIP_LANDED &&
                sphere->GetPositionZ() - dk->GetPositionZ() <= ANUBARAK_GRIP_LANDED_HEIGHT)
                cast.landed = true;

            if (cast.landed || getMSTimeDiff(cast.castMs, now) < ANUBARAK_GRIP_JUDGE_MS)
            {
                state.gripDuties.push_back({cast.dk, sphere->GetGUID(), side, dk->GetPosition()});
                if (cast.landed)
                {
                    state.grippedFor[sphere->GetGUID()] = side;
                    needs.emplace_back(SIDE_LABELS[side], sphere);
                }
                else
                    reserved.push_back(sphere);

                return true;
            }

            state.gripMode = GripMode::Failed;
            state.grip = static_cast<uint32>(GripMode::Failed);
            cast = GripCast();
            return false;
        }

        cast = GripCast();
    }

    ObjectGuid busy;
    for (GripDuty const& duty : state.gripDuties)
        busy = duty.dk;

    Player* dk = PickGripper(bot, state, side, busy);
    if (!dk)
        return false;

    auto const blocked = [&](Creature* sphere)
    {
        return taken(sphere) || Dist2d(sphere->GetPosition(), otherSpot) <= ANUBARAK_SIDE_SHOOT_NEAR ||
               std::any_of(previousGrips.begin(), previousGrips.end(), [&](GripDuty const& duty)
                           { return duty.side != side && duty.sphere == sphere->GetGUID(); });
    };

    Position stand = spot;
    Creature* sphere = GripSphere(previousGrips, flying, spot, side, ANUBARAK_GRIP_RANGE, blocked);
    if (!sphere)
    {
        sphere = GripSphere(previousGrips, flying, spot, side, ANUBARAK_GRIP_RANGE + ANUBARAK_GRIP_STEP_OUT, blocked);
        if (sphere)
            stand = GripStand(bot, spot, otherSpot, sphere);
    }

    uint32 const cooldown = dk->GetSpellCooldownDelay(SPELL_DEATH_GRIP);
    bool const idle =
        !sphere && !cooldown && Dist2d(dk->GetPosition(), spot) <= ANUBARAK_TANK_PATCH_ARRIVE;
    if (!idle)
        need.idleValid = false;
    else if (!need.idleValid)
    {
        need.idleValid = true;
        need.idleMs = now;
    }
    else if (getMSTimeDiff(need.idleMs, now) >= ANUBARAK_GRIP_IDLE_MS)
    {
        need.shoot = true;
        return false;
    }

    if (state.gripMode == GripMode::None)
    {
        state.gripMode = GripMode::Active;
        state.grip = static_cast<uint32>(GripMode::Active);
    }

    state.gripDuties.push_back({dk->GetGUID(), sphere ? sphere->GetGUID() : ObjectGuid::Empty, side, stand});
    if (sphere)
        reserved.push_back(sphere);

    return true;
}

// Shooting a tank patch: a sphere near the spot at once, the nearest within reach of it after the wait,
// never one close enough to the other side's patch to pull both sides into one Spider Frenzy. Near only
// takes the first kind.
template <typename Taken>
Creature* SideSphere(AnubarakState const& state, uint8 side, Position const& spot, Position const* other,
                     std::vector<Creature*> const& flying, std::vector<SphereDuty> const& previous, Taken const& taken,
                     uint32 now, bool nearOnly)
{
    auto const fits = [&](Creature* sphere, float within)
    {
        return !taken(sphere) && Dist2d(sphere->GetPosition(), spot) <= within &&
               (!other || Dist2d(sphere->GetPosition(), *other) >= ANUBARAK_TANK_PATCH_GAP);
    };

    // Spheres drift, so the nearest one swaps back and forth and a side's two shooters split on two
    float const keep = nearOnly ? ANUBARAK_SIDE_SHOOT_NEAR : ANUBARAK_SIDE_SHOOT_FAR;
    for (SphereDuty const& duty : previous)
        if (std::strcmp(duty.label, SIDE_LABELS[side]) == 0)
            if (Creature* held = FindSphere(flying, duty.sphere))
                if (fits(held, keep))
                    return held;

    float const within = !nearOnly && getMSTimeDiff(state.needs[side].sinceMs, now) >= ANUBARAK_SIDE_SHOOT_WAIT_MS
                             ? ANUBARAK_SIDE_SHOOT_FAR
                             : ANUBARAK_SIDE_SHOOT_NEAR;
    Creature* best = nullptr;
    float bestDist = 0.0f;
    for (Creature* sphere : flying)
    {
        if (!fits(sphere, within))
            continue;

        float const dist = Dist2d(sphere->GetPosition(), spot);
        if (!best || dist < bestDist)
        {
            best = sphere;
            bestDist = dist;
        }
    }

    return best;
}

// Sides missing their tank patch. 10H shoots one per side tank, the rest go to the kites.
uint32 MissingTankPatches(Player* bot, AnubarakState const& state)
{
    bool const tenManHeroic = AnubarakHeroic(bot) && !bot->GetMap()->Is25ManRaid();
    uint32 missing = 0;
    for (uint8 side = 0; side < 2; ++side)
        if (!LatchedTankPatch(bot, state, side) && !(tenManHeroic && state.sideTanks[side].IsEmpty()))
            ++missing;

    return missing;
}

void ComputeDuties(Player* bot, AnubarakState& state)
{
    std::vector<SphereDuty> const previous = std::move(state.duties);
    state.duties.clear();
    std::vector<GripDuty> const previousGrips = std::move(state.gripDuties);
    state.gripDuties.clear();

    // Bots only: a player never reads the assignment, and would leave a need unserved. One walking a
    // dodge would spend the duty walking. No warlock: its pet follows it onto the sphere.
    std::vector<SphereShooter> shooters;
    ForEachGroupMember(bot, [&](Player* member)
    {
        if (!member->IsAlive() || !GET_PLAYERBOT_AI(member) || !PlayerbotAI::IsRangedDps(member) ||
            member->getClass() == CLASS_WARLOCK || member->HasAura(SPELL_MARK) || !InAnubarakPit(member))
            return false;

        Position spot;
        bool const held = state.dodgeHolds.count(member->GetGUID()) && GetAnubarakDodgeHold(member, spot);
        if (held && Dist2d(member->GetPosition(), spot) > ANUBARAK_DODGE_HOLD_TRIGGER)
            return false;

        shooters.push_back({member, held});
        return false;
    });

    std::sort(shooters.begin(), shooters.end(), [](SphereShooter const& a, SphereShooter const& b)
              { return a.player->GetGUID() < b.player->GetGUID(); });

    std::vector<Creature*> const spheres = ResolveSpheres(bot, state);
    std::vector<Creature*> flying;
    for (Creature* sphere : spheres)
        if (IsFrostSphereFlying(sphere))
            flying.push_back(sphere);

    std::vector<std::pair<char const*, Creature*>> needs;
    std::vector<Creature*> reserved;
    auto const taken = [&needs, &reserved](Creature* sphere)
    {
        return std::find(reserved.begin(), reserved.end(), sphere) != reserved.end() ||
               std::any_of(needs.begin(), needs.end(),
                           [sphere](std::pair<char const*, Creature*> const& need) { return need.second == sphere; });
    };

    bool const heroic = AnubarakHeroic(bot);
    ObjectGuid const tankPatches[2] = {state.patch0.Get(), state.patch1.Get()};
    uint32 const now = getMSTime();

    // Tank patches first: a lost one is refilled ahead of any kite
    if (state.phaseValue == AnubarakPhase::Surface || state.phaseValue == AnubarakPhase::Swarm)
    {
        bool const tenManHeroic = heroic && !bot->GetMap()->Is25ManRaid();

        // A grip in flight or landed stays its side's, whichever side picks first
        for (GripCast const& cast : state.grips)
            if (cast.valid)
                if (Creature* sphere = FindSphere(flying, cast.sphere))
                    reserved.push_back(sphere);

        Position const spots[2] = {SideSpot(spheres, tankPatches, 0), SideSpot(spheres, tankPatches, 1)};
        bool open[2] = {false, false};
        for (uint8 side = 0; side < 2; ++side)
        {
            SideNeed& need = state.needs[side];
            if (LatchedTankPatch(bot, state, side) || FallingFor(state, spheres, side, spots[side], spots[1 - side]) ||
                (tenManHeroic && state.sideTanks[side].IsEmpty()))
            {
                need = SideNeed();
                continue;
            }

            if (!need.open)
            {
                need.open = true;
                need.sinceMs = now;
            }

            open[side] = true;
        }

        // Where the other side's patch is, or is about to come down
        auto const otherOf = [&](uint8 side, Position& other)
        {
            if (Creature* patch = LatchedTankPatch(bot, state, 1 - side))
            {
                other = patch->GetPosition();
                return true;
            }

            if (std::any_of(state.gripDuties.begin(), state.gripDuties.end(),
                            [side](GripDuty const& duty) { return duty.side != side; }))
            {
                other = spots[1 - side];
                return true;
            }

            bool found = false;
            for (auto const& [label, sphere] : needs)
            {
                if (std::strcmp(label, SIDE_LABELS[1 - side]) == 0)
                {
                    other = sphere->GetPosition();
                    found = true;
                }
            }

            return found;
        };

        // A sphere already near a side's spot is shot there and spends no grip. At the pull one always
        // hovers over the south spot, and gripping it to the north one drags it back across the middle.
        bool served[2] = {false, false};
        for (uint8 side = 0; side < 2; ++side)
        {
            if (!open[side] || state.grips[side].valid)
                continue;

            Position other;
            bool const hasOther = otherOf(side, other);
            if (Creature* sphere =
                    SideSphere(state, side, spots[side], hasOther ? &other : nullptr, flying, previous, taken, now, true))
            {
                needs.emplace_back(SIDE_LABELS[side], sphere);
                served[side] = true;
            }
        }

        for (uint8 side = 0; side < 2; ++side)
        {
            if (!open[side] || served[side])
                continue;

            if (ServeSideByGrip(bot, state, side, spots[side], spots[1 - side], spheres, flying, previousGrips, taken,
                                needs, reserved, now))
                continue;

            Position other;
            bool const hasOther = otherOf(side, other);
            if (Creature* sphere =
                    SideSphere(state, side, spots[side], hasOther ? &other : nullptr, flying, previous, taken, now, false))
                needs.emplace_back(SIDE_LABELS[side], sphere);
        }
    }
    else
    {
        state.needs[0] = SideNeed();
        state.needs[1] = SideNeed();
        state.grips[0] = GripCast();
        state.grips[1] = GripCast();
    }

    // A sphere already falling near the kiter lands as a patch in 1.5 s, so don't spend another
    Player* target = GetSpikeTarget(bot);
    Creature* spike = ResolveCreature(bot, state.spikeGuid);
    // Heroic shoots the kite's patch only once the kiter will need one: immune, out of everything, or
    // the spike close with nothing left to stop it
    bool kiteWantsPatch = !heroic;
    if (heroic && target && spike && spike->IsAlive())
    {
        KiteStageRead const& read = ReadKiteStage(target, spike, state);
        kiteWantsPatch = read.stage == AnubarakKiteStage::Immune || read.stage == AnubarakKiteStage::Patch ||
                         (!read.boostLeft && !read.immunityComing &&
                          Dist2d(spike->GetPosition(), target->GetPosition()) <= ANUBARAK_KITE_BOOST_RANGE);
    }

    if (kiteWantsPatch && target && spike && spike->IsAlive() && !FallingNear(spheres, target->GetPosition()))
    {
        AnubarakKiteStand stand;
        ComputeKiteStand(target, spike, spheres, tankPatches, heroic, AnyBurrower(bot, state),
                         LaneObstacles(target, state), nullptr, true, stand);
        if (stand.branch == AnubarakKiteBranch::Ring)
        {
            std::vector<Position> tankSpots;
            for (uint8 side = 0; side < 2; ++side)
                if (Creature* patch = LatchedTankPatch(bot, state, side))
                    tankSpots.push_back(patch->GetPosition());

            std::vector<Creature*> open;
            for (Creature* sphere : flying)
                if (!taken(sphere) && !(heroic && NearTankPatch(sphere->GetPosition(), tankSpots)))
                    open.push_back(sphere);

            Creature* sphere = nullptr;
            if (!heroic)
                sphere = KiteSphere(open, target, spike);
            else
            {
                // Heroic spends this submerge's share, keeping the spheres the missing tank patches need
                for (ObjectGuid const& guid : state.kiteSpheres)
                    if (!sphere)
                        sphere = FindSphere(open, guid);

                if (!sphere && state.kiteSpheres.size() < state.kiteAllowance &&
                    open.size() > MissingTankPatches(bot, state))
                {
                    sphere = KiteSphere(open, target, spike);
                    if (sphere)
                        state.kiteSpheres.push_back(sphere->GetGUID());
                }
            }

            if (sphere)
                needs.emplace_back("kite", sphere);
        }
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

    if (shooters.empty())
        return;

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
    claim.boostLeft = AnubarakHeroic(bot) && ReadKiteStage(kiter, spike, *state).boostLeft;
    claim.need = !immune && !claim.boostLeft && !kiter->HasAura(SPELL_FORBEARANCE) &&
                 spikeDist <= ANUBARAK_KITE_PROTECT_RANGE;

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

    if (claim.kiter == bot->GetGUID() && claim.spikeInImmunityRange && !claim.boostLeft &&
        claim.protector.IsEmpty() && !AnubarakKiterImmune(bot))
        return "self";

    return "none";
}

// Once per instance per ms. One on a usable side tank stays with it, so a side tank never taunts one off
// the other. The rest go by exit, at most two a side while both side tanks are usable, and a dead or
// marked side tank's to the other, as do all of one holding him while he's up. A human side tank's
// still loose 5 s after it came out goes to a bot side tank, past two.
std::unordered_map<ObjectGuid, uint8> const& BurrowerOwners(Player* bot, AnubarakState& state)
{
    uint32 const now = getMSTime();
    if (state.ownerMemoValid && state.ownerMemoMs == now)
        return state.owners;

    state.ownerMemoValid = true;
    state.ownerMemoMs = now;
    state.owners.clear();

    ObjectGuid const pickup = state.phaseValue == AnubarakPhase::Submerged ? ObjectGuid::Empty : PickupGuid(bot, state);
    bool usable[2];
    bool human[2];
    for (uint8 side = 0; side < 2; ++side)
    {
        Player* tank = state.sideTanks[side].IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(*bot, state.sideTanks[side]);
        usable[side] = tank && tank->IsAlive() && !tank->HasAura(SPELL_MARK) && tank->GetGUID() != pickup;
        human[side] = usable[side] && !GET_PLAYERBOT_AI(tank);
    }

    std::vector<Creature*> burrowers = ResolveAlive(bot, state.burrowers);
    std::sort(burrowers.begin(), burrowers.end(), [](Creature* a, Creature* b) { return a->GetGUID() < b->GetGUID(); });

    uint8 count[2] = {0, 0};
    std::vector<Creature*> free;
    for (Creature* burrower : burrowers)
    {
        Unit* victim = burrower->GetVictim();
        ObjectGuid const victimGuid = victim ? victim->GetGUID() : ObjectGuid::Empty;
        int8 const onSide = victimGuid.IsEmpty()            ? -1
                            : victimGuid == state.sideTanks[0] ? 0
                            : victimGuid == state.sideTanks[1] ? 1
                                                               : -1;
        if (onSide >= 0 && usable[onSide])
        {
            state.owners[burrower->GetGUID()] = static_cast<uint8>(onSide);
            ++count[onSide];
        }
        else
            free.push_back(burrower);
    }

    for (Creature* burrower : free)
    {
        auto const exit = state.exits.find(burrower->GetGUID());
        bool const known = exit != state.exits.end();
        uint8 owner = known ? exit->second.side : 0;
        if (!usable[owner] && usable[1 - owner])
            owner = 1 - owner;
        else if (usable[0] && usable[1] && count[owner] >= ANUBARAK_BURROWERS_PER_SIDE &&
                 count[1 - owner] < ANUBARAK_BURROWERS_PER_SIDE)
            owner = 1 - owner;

        if (human[owner] && usable[1 - owner] && !human[1 - owner] && known &&
            getMSTimeDiff(exit->second.seenMs, now) >= ANUBARAK_HUMAN_PICKUP_GRACE_MS)
            owner = 1 - owner;

        state.owners[burrower->GetGUID()] = owner;
        ++count[owner];
    }

    for (auto it = state.burrowerSide.begin(); it != state.burrowerSide.end();)
        it = state.owners.count(it->first) ? std::next(it) : state.burrowerSide.erase(it);

    for (auto const& [guid, owner] : state.owners)
        state.burrowerSide[guid] = owner;

    return state.owners;
}

constexpr uint32 REBIRTH_RANKS[] = {48477, 26994, 20748, 20747, 20742, 20739, 20484};

// Known and off cooldown. Not CanCastSpell: in cat or bear form that says no, and the action shifts out
// first.
bool RebirthReady(Player* player)
{
    if (player->getClass() != CLASS_DRUID)
        return false;

    for (uint32 rank : REBIRTH_RANKS)
        if (player->HasSpell(rank))
            return !player->HasSpellCooldown(rank);

    return false;
}

bool IsFocusableBurrower(Creature* burrower)
{
    return burrower && burrower->IsAlive() && !burrower->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) &&
           (UnitOnPermafrost(burrower) || burrower->GetHealthPct() < ANUBARAK_FOCUS_OFF_FROST_PCT);
}

// Time to the next wave, heroic phase 2 counted at its share: the deadline after the submerge is the
// wave 5 s after the emerge
float FocusAvailableMs(Player* bot, AnubarakState const& state, uint32 now)
{
    float const untilWave = static_cast<float>(MsUntilWave(state, now));
    float const untilSubmerge = static_cast<float>(static_cast<int32>(state.surfacedMs + ANUBARAK_SURFACED_MS - now));
    if (state.phaseValue != AnubarakPhase::Surface || !AnubarakHeroic(bot) || untilSubmerge < 0.0f ||
        untilWave <= untilSubmerge)
        return untilWave;

    float const down = std::min(untilWave - untilSubmerge, static_cast<float>(ANUBARAK_SUBMERGED_MS));
    return untilWave - down * (1.0f - ANUBARAK_FOCUS_SUBMERGED_SHARE);
}

// Once per instance per ms. While he is down, always; while he is up, once every burrower alive would
// take the raid's burrower rate, plus the margin, to kill before the next wave lands. One off Permafrost
// counts too, since it still has to die before then, and a submerged one comes back at full health.
// Once due it stays due until the focus it came due on dies, so a started kill is finished.
bool FocusAhead(Player* bot, AnubarakState& state)
{
    uint32 const now = getMSTime();
    if (state.focusMemoValid && state.focusMemoMs == now)
        return state.focusAhead;

    state.focusMemoValid = true;
    state.focusMemoMs = now;

    uint64 remaining = 0;
    for (Creature* burrower : ResolveAlive(bot, state.burrowers))
        remaining += burrower->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) ? burrower->GetMaxHealth() : burrower->GetHealth();

    Creature* held = ResolveCreature(bot, state.focusDueBurrower);
    bool ahead = false;
    if (remaining && held && held->IsAlive())
        ahead = true;
    else if (remaining && state.phaseValue == AnubarakPhase::Submerged)
        ahead = true;
    else if (remaining)
    {
        float const rate = DamageRate(state) * ANUBARAK_FOCUS_RATE_SHARE;
        float const killMs = rate > 0.0f ? static_cast<float>(remaining) * IN_MILLISECONDS / rate : 0.0f;
        ahead = rate > 0.0f && killMs + ANUBARAK_FOCUS_MARGIN_MS >= FocusAvailableMs(bot, state, now);
    }

    if (!ahead || !held || !held->IsAlive())
        state.focusDueBurrower = ahead ? state.focusBurrower.Get() : ObjectGuid::Empty;

    state.focusAhead = ahead;
    state.focusDue = ahead ? 1 : 0;
    return ahead;
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
// nearest one in reach, the tanks' pickup reach by default
Creature* NearestDpsScarab(Player* bot, AnubarakState const& state, float reach = ANUBARAK_SCARAB_PICKUP_RANGE)
{
    Creature* nearest = nullptr;
    float nearestDist = reach;
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

// The two sides' spots: a latched patch, else the side's spot off the other one. False with neither
// patch down, so he isn't dragged toward a spot no patch has come to yet.
bool AnubarakSideSpots(Player* bot, Position& spot0, Position& spot1)
{
    Creature* side0 = GetAnubarakTankPatch(bot, 0);
    Creature* side1 = GetAnubarakTankPatch(bot, 1);
    if (!side0 && !side1)
        return false;

    spot0 = side0 ? side0->GetPosition() : GetAnubarakTankSpot(bot, 0);
    spot1 = side1 ? side1->GetPosition() : GetAnubarakTankSpot(bot, 1);
    return true;
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

Position GetAnubarakTankSpot(Player* bot, uint8 side)
{
    AnubarakState const* state = bot && side < 2 ? LatchesFor(bot) : nullptr;
    if (!state)
        return GetAnubarakTankSidePoint(side > 1 ? 0 : side);

    ObjectGuid const held[2] = {state->patch0.Get(), state->patch1.Get()};
    return SideSpot(ResolveSpheres(bot, *state), held, side);
}

Position GetAnubarakBossAnchor(Player* bot)
{
    Position anchor;
    return GetAnubarakDragAnchor(bot, anchor) ? anchor : ANUBARAK_ROOM_CENTER;
}

Position GetAnubarakPickupStand(Player* bot)
{
    Position const anchor = GetAnubarakBossAnchor(bot);
    return Position(anchor.GetPositionX() - ANUBARAK_PICKUP_STAND_OFFSET, anchor.GetPositionY(),
                    anchor.GetPositionZ());
}

bool GetAnubarakDragAnchor(Player* bot, Position& out)
{
    Position spot0;
    Position spot1;
    if (!AnubarakSideSpots(bot, spot0, spot1))
        return false;

    out = Position((spot0.GetPositionX() + spot1.GetPositionX()) / 2.0f,
                   (spot0.GetPositionY() + spot1.GetPositionY()) / 2.0f,
                   (spot0.GetPositionZ() + spot1.GetPositionZ()) / 2.0f);
    return true;
}

std::vector<Position> GetAnubarakDragPoints(Player* bot, Creature* boss)
{
    std::vector<Position> points;
    Position anchor;
    Position spot0;
    Position spot1;
    if (!boss || !GetAnubarakDragAnchor(bot, anchor) || !AnubarakSideSpots(bot, spot0, spot1))
        return points;

    points.push_back(anchor);

    // Permafrost leaves no gap on the line between the patches, so a leg refused at the anchor can
    // still end short of it on his side
    Vec2 along = Delta(spot0, spot1);
    float const length = Length(along);
    if (length < 1.0f)
        return points;

    Vec2 normal = {-along.y / length, along.x / length};
    Vec2 const toBoss = Delta(anchor, boss->GetPosition());
    if (normal.x * toBoss.x + normal.y * toBoss.y < 0.0f)
        normal = {-normal.x, -normal.y};

    for (float offset : {4.0f, 7.5f})
        points.push_back(Offset(anchor, normal, offset));

    return points;
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
        // navprobe-clean out to 35 yd, so he never ends up against a wall
        auto const onFloor = [](float x, float y)
        { return ANUBARAK_ROOM_CENTER.GetExactDist2d(x, y) <= ANUBARAK_SUBMERGE_SPOT_FLOOR; };

        // Within the nearest clear ring, the spot toward a point 40 yd past the tank, away from the tank
        // patches, so the spike starts on the far side from both
        Position prefer = ANUBARAK_ROOM_CENTER;
        std::vector<Position> tankPatches;
        for (uint8 side = 0; side < 2; ++side)
            if (Creature* patch = LatchedTankPatch(bot, *state, side))
                tankPatches.push_back(patch->GetPosition());

        if (!tankPatches.empty())
        {
            Position centroid(0.0f, 0.0f, 0.0f);
            for (Position const& patch : tankPatches)
                centroid.Relocate(centroid.GetPositionX() + patch.GetPositionX() / tankPatches.size(),
                                  centroid.GetPositionY() + patch.GetPositionY() / tankPatches.size());

            Vec2 const away = Delta(centroid, bot->GetPosition());
            float const length = Length(away);
            if (length > 1.0f)
                prefer = Offset(bot->GetPosition(), {away.x / length, away.y / length},
                                ANUBARAK_SUBMERGE_SPOT_SEARCH);
        }

        state->submergeSpotValid = false;
        HazardSweepCache cache;
        for (float clearance : ANUBARAK_SUBMERGE_CLEARANCES)
        {
            std::vector<HazardCircle> hazards;
            for (Position const& patch : patches)
                hazards.emplace_back(patch, clearance);

            Position const spot = FindNearestPositionClearOfHazards(bot, hazards, ANUBARAK_SUBMERGE_SPOT_SEARCH, 2.0f,
                                                                    static_cast<float>(M_PI) / 8.0f, &prefer,
                                                                    onFloor, &cache);
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

    // Slowed 80% the tank crawls and he stops short, so no leg ends in a patch or walks through one
    if (!TankWalkClear(bot->GetPosition(), spot, PatchPositions(bot, *state)))
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

bool GetAnubarakTankStepOut(Player* bot, Creature* boss, Position& out)
{
    AnubarakState const* state = bot && boss ? ReadPhase(GET_PLAYERBOT_AI(bot)) : nullptr;
    if (!state)
        return false;

    std::vector<Position> const patches = PatchPositions(bot, *state);
    bool const inside = UnitOnPermafrost(bot) ||
                        std::any_of(patches.begin(), patches.end(), [bot](Position const& patch)
                                    { return Dist2d(bot->GetPosition(), patch) < ANUBARAK_PERMAFROST_SLOW_REACH; });
    if (!inside)
        return false;

    std::vector<HazardCircle> hazards;
    for (Position const& patch : patches)
        hazards.emplace_back(patch, ANUBARAK_TANK_KEEPOUT);

    // He follows past 12 yd, so the step stays inside that when it can and he keeps his place
    Position const bossPos = boss->GetPosition();
    float const reach = boss->GetCombatReach() + bot->GetCombatReach();
    auto const nearHim = [&bossPos, reach](float x, float y) { return bossPos.GetExactDist2d(x, y) <= reach; };

    HazardSweepCache cache;
    Position spot = FindNearestPositionClearOfHazards(bot, hazards, ANUBARAK_TANK_STEP_SEARCH, 1.5f,
                                                      static_cast<float>(M_PI) / 8.0f, &bossPos, nearHim, &cache);
    if (spot == Position())
        spot = FindNearestPositionClearOfHazards(bot, hazards, ANUBARAK_TANK_STEP_SEARCH, 1.5f,
                                                 static_cast<float>(M_PI) / 8.0f, &bossPos, {}, &cache);

    if (spot == Position())
        return false;

    out = spot;
    return true;
}

bool AnubarakTankPatchLatched(Player* bot)
{
    return GetAnubarakTankPatch(bot, 0) || GetAnubarakTankPatch(bot, 1);
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
    return state && PickupGuid(bot, *state) == bot->GetGUID();
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

    Position const points[2] = {GetAnubarakTankSidePoint(0), GetAnubarakTankSidePoint(1)};
    if (HumanOnOtherSpot(bot, tanks.side, points) >= 0)
        std::swap(tanks.side[0], tanks.side[1]);

    Player* main = mainTank.IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(*bot, mainTank);
    tanks.pickup = main && main->IsAlive() ? mainTank : FirstBotSideTank(bot, tanks.side);
    return tanks;
}

std::vector<Creature*> GetAnubarakBurrowers(Player* bot)
{
    AnubarakState const* state = Sweep(bot);
    return state ? ResolveAlive(bot, state->burrowers) : std::vector<Creature*>();
}

bool AnubarakSideTankNeedsDefensive(Player* bot)
{
    return bot && bot->IsAlive() && GetAnubarakBurrowerTankSide(bot) >= 0 &&
           (bot->GetAuraCount(SPELL_EXPOSE_WEAKNESS) >= ANUBARAK_SIDE_DEFENSIVE_STACKS ||
            bot->GetHealthPct() < ANUBARAK_SIDE_DEFENSIVE_PCT);
}

Player* GetAnubarakSideTankToRes(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    AnubarakState const* state = bot ? ReadPhase(botAI) : nullptr;
    if (!state || state->deadSideTanks.empty() || !RebirthReady(bot) || bot->HasAura(SPELL_MARK))
        return nullptr;

    // One druid per corpse: the lowest guid with Rebirth ready
    Player* first = nullptr;
    ForEachGroupMember(bot, [&](Player* member)
    {
        if (member->IsAlive() && GET_PLAYERBOT_AI(member) && !member->HasAura(SPELL_MARK) && RebirthReady(member) &&
            (!first || member->GetGUID() < first->GetGUID()))
            first = member;

        return false;
    });

    if (first != bot)
        return nullptr;

    for (ObjectGuid const& guid : state->deadSideTanks)
    {
        Player* tank = ObjectAccessor::GetPlayer(*bot, guid);
        // A pending Soulstone or Reincarnation brings it back without the Rebirth
        if (tank && !tank->IsAlive() && tank->getDeathState() == DeathState::Corpse && !tank->isResurrectRequested() &&
            (tank->HasPlayerFlag(PLAYER_FLAGS_GHOST) || !tank->GetUInt32Value(PLAYER_SELF_RES_SPELL)))
            return tank;
    }

    return nullptr;
}

bool AnubarakWaveImminent(PlayerbotAI* botAI)
{
    AnubarakState const* state = ReadPhase(botAI);
    if (!state || state->phaseValue == AnubarakPhase::Submerged)
        return false;

    int32 const until = MsUntilWave(*state, getMSTime());
    return until <= static_cast<int32>(ANUBARAK_WAVE_READY_MS);
}

AnubarakBurrowerPick GetAnubarakBurrowerPick(PlayerbotAI* botAI)
{
    AnubarakBurrowerPick pick;
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    int8 const side = GetAnubarakBurrowerTankSide(bot);
    AnubarakState* state = side >= 0 ? ReadPhase(botAI) : nullptr;
    if (!state)
        return pick;

    pick.side = static_cast<uint8>(side);
    pick.hold = GetAnubarakTankSpot(bot, pick.side);

    std::unordered_map<ObjectGuid, uint8> const& owners = BurrowerOwners(bot, *state);
    Unit* current = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    std::unordered_map<Player*, uint8> onMember;
    bool looseOnHealer = false;
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

        auto const owner = owners.find(burrower->GetGUID());
        if (owner == owners.end() || owner->second != pick.side)
        {
            if (bot->GetExactDist2d(burrower) <= ANUBARAK_SIDE_SHOUT_RADIUS)
                pick.otherNear = true;

            continue;
        }

        Player* victim = burrower->GetVictim() ? burrower->GetVictim()->ToPlayer() : nullptr;
        if (victim)
            ++onMember[victim];

        if (bot->GetExactDist2d(burrower) <= ANUBARAK_SIDE_SHOUT_RADIUS)
            ++pick.looseNear;

        bool const onHealer = victim && PlayerbotAI::IsHeal(victim);
        float const dist = Dist2d(burrower->GetPosition(), pick.hold);
        if (!pick.loose || (onHealer && !looseOnHealer) || (onHealer == looseOnHealer && dist < looseDist))
        {
            pick.loose = burrower;
            looseOnHealer = onHealer;
            looseDist = dist;
        }
    }

    uint8 most = 0;
    for (auto const& [member, count] : onMember)
    {
        if (count > most || (count == most && pick.rescue && member->GetGUID() < pick.rescue->GetGUID()))
        {
            pick.rescue = member;
            most = count;
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
    AnubarakState* state = ReadPhase(botAI);
    if (!state)
        return nullptr;

    // The sphere and grip nodes outrank this one, so with a pick here the bot would swap targets every
    // tick. A DK waiting on its spot keeps hitting him from there.
    if (PlayerbotAI::IsRangedDps(bot) && GetAnubarakSphereToShoot(bot))
        return NoteDpsPick(bot, nullptr, "none");

    AnubarakGripDuty grip;
    if (GetAnubarakGripDuty(bot, grip) && grip.step != AnubarakGripStep::Hold)
        return NoteDpsPick(bot, nullptr, "none");

    // Read even under a skull, so the anub.focus latch stays current
    Creature* burrower = GetAnubarakFocusBurrower(bot);
    bool const ahead = burrower && FocusAhead(bot, *state);

    if (Unit* skull = GetToCHumanSkull(botAI, &IsAnubarakSkullTarget))
        return NoteDpsPick(bot, skull, "skull");

    // Gathered melee hold their place, so they take the scarabs that come to the raid and leave the
    // burrowers on the patches to the ranged
    if (state->phaseValue == AnubarakPhase::Submerged && PlayerbotAI::IsMelee(bot) && AnubarakGathers(botAI))
        if (Creature* scarab = NearestDpsScarab(bot, *state, ANUBARAK_GATHER_MELEE_SCARAB))
            return NoteDpsPick(bot, scarab, "scarab");

    if (ahead)
        return NoteDpsPick(bot, burrower, "burrower");

    Creature* boss = GetAnubarak(bot);
    if (boss && !boss->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
        return NoteDpsPick(bot, boss, "boss");

    if (burrower)
        return NoteDpsPick(bot, burrower, "burrower");

    if (Creature* scarab = state->phaseValue == AnubarakPhase::Submerged ? NearestDpsScarab(bot, *state) : nullptr)
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

static AnubarakState* ResolveDuties(Player* bot)
{
    AnubarakState* state = bot ? ReadPhase(GET_PLAYERBOT_AI(bot)) : nullptr;
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

    return state;
}

Creature* GetAnubarakSphereToShoot(Player* bot)
{
    if (!bot || !PlayerbotAI::IsRangedDps(bot))
        return nullptr;

    AnubarakState* state = ResolveDuties(bot);
    if (!state)
        return nullptr;

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

bool GetAnubarakGripDuty(Player* bot, AnubarakGripDuty& out)
{
    out = AnubarakGripDuty();
    if (!bot || bot->getClass() != CLASS_DEATH_KNIGHT)
        return false;

    AnubarakState* state = ResolveDuties(bot);
    GripDuty const* duty = nullptr;
    if (state)
        for (GripDuty const& candidate : state->gripDuties)
            if (candidate.dk == bot->GetGUID())
                duty = &candidate;

    if (!duty)
    {
        RaidObs::NoteDerived(bot, "anub.gripstep", "none");
        RaidObs::NoteDerived(bot, "anub.sphere", "none");
        return false;
    }

    out.side = duty->side;
    out.spot = duty->stand;
    Creature* sphere = ResolveCreature(bot, duty->sphere);
    out.sphere = IsFrostSphereFlying(sphere) ? sphere : nullptr;

    // In reach from where it stands, inside the step-out and no nearer the other side: the stand follows
    // the sphere, so walking on would only chase it
    Position const sideSpot = GetAnubarakTankSpot(bot, duty->side);
    Position const otherSpot = GetAnubarakTankSpot(bot, 1 - duty->side);
    bool const ready = out.sphere && !bot->GetSpellCooldownDelay(SPELL_DEATH_GRIP);
    bool const reachesFromHere =
        ready && bot->GetExactDist(out.sphere) <= ANUBARAK_GRIP_RANGE &&
        Dist2d(bot->GetPosition(), sideSpot) <= ANUBARAK_GRIP_STEP_OUT + ANUBARAK_TANK_PATCH_ARRIVE &&
        Dist2d(bot->GetPosition(), otherSpot) + ANUBARAK_TANK_PATCH_ARRIVE >= Dist2d(sideSpot, otherSpot);

    GripCast const& cast = state->grips[duty->side];
    char const* name = "hold";
    if (cast.valid && cast.dk == bot->GetGUID())
    {
        out.step = cast.landed ? AnubarakGripStep::Kill : AnubarakGripStep::Hold;
        name = cast.landed ? "kill" : "wait";
    }
    else if (reachesFromHere)
    {
        out.step = AnubarakGripStep::Cast;
        name = "cast";
    }
    else if (Dist2d(bot->GetPosition(), out.spot) > ANUBARAK_TANK_PATCH_ARRIVE)
    {
        out.step = AnubarakGripStep::Walk;
        name = "walk";
    }
    else if (ready)
    {
        out.step = AnubarakGripStep::Cast;
        name = "cast";
    }
    else
        out.step = AnubarakGripStep::Hold;

    RaidObs::NoteDerived(bot, "anub.gripstep", name);
    RaidObs::NoteDerived(bot, "anub.sphere", out.step == AnubarakGripStep::Kill ? SIDE_LABELS[duty->side]
                                                                               : GRIP_LABELS[duty->side]);
    return true;
}

void NoteAnubarakGripCast(Player* bot, Creature* sphere)
{
    AnubarakGripDuty duty;
    AnubarakState* state = sphere && GetAnubarakGripDuty(bot, duty) ? ResolveDuties(bot) : nullptr;
    if (!state)
        return;

    GripCast& cast = state->grips[duty.side];
    cast.valid = true;
    cast.dk = bot->GetGUID();
    cast.sphere = sphere->GetGUID();
    cast.castMs = getMSTime();
    cast.landed = false;
}

Creature* GetAnubarakDutySphere(Player* bot)
{
    if (Creature* sphere = GetAnubarakSphereToShoot(bot))
        return sphere;

    AnubarakGripDuty grip;
    return GetAnubarakGripDuty(bot, grip) ? grip.sphere : nullptr;
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
    bool const heroic = AnubarakHeroic(bot);
    bool const seekPatch = !heroic || GetAnubarakKiteStage(bot) == AnubarakKiteStage::Patch;
    ComputeKiteStand(bot, spike, ResolveSpheres(bot, *state), tankPatches, heroic, AnyBurrower(bot, *state),
                     LaneObstacles(bot, *state), seekPatch ? previous : nullptr, seekPatch, out);
    RaidObs::NoteDerived(bot, "anub.kite", KiteBranchName(out.branch));
    return true;
}

AnubarakKiteStage GetAnubarakKiteStage(Player* kiter, AnubarakKiteBoost* boost)
{
    if (!kiter || !kiter->HasAura(SPELL_MARK) || !AnubarakHeroic(kiter))
        return AnubarakKiteStage::None;

    AnubarakState* state = LatchesFor(kiter) ? anubarakStates.Find(kiter->GetInstanceId()) : nullptr;
    Creature* spike = state ? GetPursuingSpike(kiter) : nullptr;
    if (!spike)
        return AnubarakKiteStage::None;

    KiteStageRead const& read = ReadKiteStage(kiter, spike, *state);
    if (boost)
        *boost = read.boost;

    return read.stage;
}

static AnubarakState const* GatherState(Player* bot)
{
    AnubarakState const* state = bot && AnubarakHeroic(bot) ? ReadPhase(GET_PLAYERBOT_AI(bot)) : nullptr;
    if (!state || state->phaseValue != AnubarakPhase::Submerged || !state->submergePosValid ||
        bot->HasAura(SPELL_MARK) || GetAnubarakBurrowerTankSide(bot) >= 0 || bot->GetGUID() == state->mainTank)
        return nullptr;

    return state;
}

bool GetAnubarakGatherSpot(Player* bot, Position& out)
{
    AnubarakState const* state = GatherState(bot);
    Position gather;
    if (!state || !GatherCentre(bot, *state, gather))
        return false;

    // A slot per bot round the point, so twenty bots don't stand on one spot
    float const angle = static_cast<float>(bot->GetGUID().GetCounter() % 12) * 30.0f * ANUBARAK_DEG_TO_RAD;
    out = Offset(gather, {std::cos(angle), std::sin(angle)}, ANUBARAK_GATHER_SPREAD);
    return true;
}

bool AnubarakGathers(PlayerbotAI* botAI) { return botAI && GatherState(botAI->GetBot()); }

bool AnubarakDemonicCircleDue(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || bot->getClass() != CLASS_WARLOCK || !bot->IsAlive() || !bot->HasSpell(SPELL_CIRCLE_SUMMON) ||
        bot->isMoving() || bot->HasAura(SPELL_MARK) || !AnubarakHeroic(bot))
        return false;

    if (AnubarakEngaged(botAI))
    {
        AnubarakPhase const phase = GetAnubarakPhase(botAI);
        if (phase != AnubarakPhase::Surface && phase != AnubarakPhase::Swarm)
            return false;
    }
    else if (GetToCStagingBoss(botAI) != ToCStagingBoss::Anubarak || !IsToCStagingHeld(botAI) ||
             IsToCStagingRestHeld(botAI))
        return false;

    Aura const* circle = bot->GetAura(SPELL_CIRCLE_SUMMON, bot->GetGUID());
    return !circle || (circle->GetDuration() >= 0 && circle->GetDuration() < ANUBARAK_CIRCLE_REFRESH_MS);
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
