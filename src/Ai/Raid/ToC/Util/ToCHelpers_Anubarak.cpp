#include "ToCHelpers_Anubarak.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <functional>
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
#include "PaladinHelper.h"
#include "Player.h"
#include "Playerbots.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "ToCAnubarakPursuit.h"
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
// Gathered bots stand on 3 yd slots round the gather point and up to 6 yd off them, out of Impale's
// reach with a margin
constexpr float ANUBARAK_KITE_GATHER_CLEARANCE =
    ANUBARAK_GATHER_SPREAD + ANUBARAK_GATHER_ARRIVE + ANUBARAK_IMPALE_REACH + 3.5f;

// Heroic kite: the restart line runs from where the kiter is this far ahead
constexpr float ANUBARAK_KITE_LANE_HORIZON_S = 3.0f;
// A heading is judged by the spike's pursuit this long along it: 10 yd out, then round the ring
constexpr float ANUBARAK_KITE_PURSUIT_HORIZON_S = 6.0f;
// How far ahead a catch is looked for: 40 yd back at the slowest closing speed is under that
constexpr float ANUBARAK_KITE_CATCH_HORIZON_S = 20.0f;
// A step heading within this of the held one counts as the held one
constexpr float ANUBARAK_KITE_LANE_KEEP_DEGREES = 15.0f;
// Under Hand of Protection the spike sits on the kiter and Impales whoever stands near it, so a walk to
// a patch this close to the gather ranks after one that keeps off it
constexpr float ANUBARAK_KITE_GATHER_WALK_CLEARANCE = 12.0f;
// Heroic kite plan: judged this often, its catch looked for this far ahead. At the spike's top speed a
// kiter 40 yd ahead goes about 9 s, and the speed steps take 14 s to get there.
constexpr uint32 ANUBARAK_KITE_PLAN_JUDGE_MS = 250;
constexpr float ANUBARAK_KITE_PLAN_HORIZON_S = 40.0f;
// The spike's fail at the patch has to come this much ahead of the catch, and a patch kite runs on only
// while turning this much later would still make it
constexpr float ANUBARAK_KITE_PATCH_LEAD_S = 1.0f;
constexpr float ANUBARAK_KITE_PATCH_COMMIT_S = 1.5f;
// Hand of Protection goes out this long before the catch, an own immunity this long: Feign Death needs
// the hunter stopped first
constexpr float ANUBARAK_KITE_HAND_LEAD_S = 1.0f;
constexpr float ANUBARAK_KITE_OWN_LEAD_S = 0.6f;
// A run heading is held this long once picked, and after that while it ranks with the best; either way
// only while its catch stays within the slack of the best
constexpr uint32 ANUBARAK_KITE_HEADING_HOLD_MS = 1500;
constexpr float ANUBARAK_KITE_HEADING_CATCH_SLACK_S = 2.0f;
// Run headings whose catches lie within one band rank alike, so the tiebreaks still count
constexpr float ANUBARAK_KITE_CATCH_BAND_S = 2.0f;
// A Hand kite's steps keep it this close to its escort
constexpr float ANUBARAK_KITE_ESCORT_LEASH = 25.0f;
// An own kite's steps away from the gather rank in bands this wide
constexpr float ANUBARAK_KITE_GATHER_BAND = 5.0f;

// The spike's speed auras. The first two last 7 s and the next one goes out as each runs out.
constexpr uint32 SPELL_SPIKE_SPEED1 = 65920;
constexpr uint32 SPELL_SPIKE_SPEED2 = 65922;
constexpr uint32 SPELL_SPIKE_SPEED3 = 65923;

constexpr float ANUBARAK_SCARAB_PICKUP_RANGE = 30.0f;
constexpr float ANUBARAK_PENETRATING_COLD_HEAL_PCT = 90.0f;

// The floor is about 60 yd round the room centre. The Web Door is 83 yd out, and anyone who never
// landed is still up near z 395.
constexpr float ANUBARAK_PIT_RADIUS = 60.0f;
constexpr float ANUBARAK_PIT_CEILING_Z = 200.0f;
constexpr float ANUBARAK_SUBMERGE_SPOT_SEARCH = 40.0f;
constexpr float ANUBARAK_SUBMERGE_SPOT_FLOOR = 40.0f;
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
// The tank stands a few yards off his line to the point, so a patch this close to the line has the drag
// routed round it, through waypoints this far either side of it
constexpr float ANUBARAK_DRAG_DETOUR_TRIGGER = ANUBARAK_TANK_KEEPOUT + 3.0f;
constexpr float ANUBARAK_DRAG_DETOUR_STANDOFF = 12.0f;
// Held further off the anchor than this for this long with no leg, the drag tries a ring round it too
constexpr float ANUBARAK_DRAG_STALL_RANGE = 8.0f;
constexpr uint32 ANUBARAK_DRAG_STALL_MS = 5000;

// A patch landing this close to the spike, or to him before it spawns on him, burns before a kite can use it
constexpr float ANUBARAK_STOCK_SPIKE_CLEARANCE = ANUBARAK_SUBMERGE_PATCH_CLEARANCE_MIN;
// The stand sits this far inside the reach, so a drifting sphere doesn't walk the shooter every tick
constexpr float ANUBARAK_SPHERE_STAND_SLACK = 1.0f;

// A claimed protector with no immunity on the kiter by then passes the claim on
constexpr uint32 ANUBARAK_PROTECT_CLAIM_MS = 1500;

// The pull's first 10 s: the first hunter and the first rogue bot send theirs to the main tank so he
// holds him from the start, and the others keep theirs for the first wave
constexpr uint32 ANUBARAK_REDIRECT_PULL_MS = 10000;
constexpr float ANUBARAK_TRICKS_RANGE = 20.0f;
// A side tank walks back to its hold unless the spike could get there within its walk plus this
constexpr float ANUBARAK_SPIKE_WALK_MARGIN_S = 2.0f;

// The boss script's Frost Sphere spawns, AnubLocs 5 to 10, at floor height
const Position ANUBARAK_SPHERE_HOMES[] = {{786.64f, 108.25f, 142.6f}, {806.84f, 150.59f, 142.6f},
                                          {759.14f, 163.97f, 142.6f}, {744.37f, 119.52f, 142.6f},
                                          {710.02f, 120.82f, 142.6f}, {706.64f, 161.53f, 142.6f}};
constexpr uint8 ANUBARAK_SPHERE_HOME_COUNT = 6;
// Heroic refills: an open spot at most this far from the held patch, an open pair pulled in to this apart
constexpr float ANUBARAK_REFILL_MAX = 30.0f;
// Refill spots are pulled in to this far from the room centre, inside the navprobe-clean 45 yd ring
constexpr float ANUBARAK_REFILL_FLOOR = 45.0f;
// The raid point sits this far off the anchor, at right angles to the line through the tank spots
constexpr float ANUBARAK_RAID_POINT_OFFSET = 18.0f;
constexpr float ANUBARAK_RAID_POINT_SLACK = 0.5f;
// The slow's 7.5 yd plus a yard
constexpr float ANUBARAK_HEALER_PATCH_CLEARANCE = 8.5f;
constexpr uint32 ANUBARAK_HEALER_SLOT_TRIES = 12;
// Tank patches this far apart split the healers in two groups, each this far off its side tank, the
// group kept within the reach of the main tank too
constexpr float ANUBARAK_HEALER_SPLIT_GAP = 40.0f;
constexpr float ANUBARAK_HEALER_SIDE_OFFSET = 17.5f;
constexpr float ANUBARAK_HEALER_MAIN_TANK_REACH = 36.0f;
// The spike is summoned where he goes down, so that is kept this far from the raid point
constexpr float ANUBARAK_SUBMERGE_RAID_CLEARANCE = 25.0f;
// Heroic phase 2: the main tank's gather slot, off the raid point away from where he went down
constexpr float ANUBARAK_GATHER_TANK_EDGE = 6.0f;

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
    // The spike catches the kiter within the catch horizon, its speed steps in: no immunity goes on one
    // that doesn't
    bool spikeFaster = false;
    // The kiter gets to its patch's stand before the spike gets to it
    bool standReachable = false;
    // Heroic: the kite plan says the kiter casts its own Hand or immunity now
    bool heroic = false;
    bool selfDue = false;
};

// Heroic: the paladin bot keeping a kiter with no patch to end on inside Hand of Protection's reach
struct EscortRead
{
    bool memoValid = false;
    uint32 memoMs = 0;
    ObjectGuid paladin;
    bool spotValid = false;
    Position spot;
};

// The paladin bot casting Hand of Freedom on a kiter marked in Permafrost
struct FreedomRead
{
    bool memoValid = false;
    uint32 memoMs = 0;
    ObjectGuid kiter;
    ObjectGuid paladin;
};

// Written raw into anub.grip
enum class GripMode : uint8
{
    None = 0,
    Active = 1,
    // A side waits on a failed grip, shooting meanwhile
    Failed = 2
};

struct GripDuty
{
    ObjectGuid dk;
    ObjectGuid sphere;
    uint8 side = 0;
    // Where the DK grips from: the side's spot, up to ANUBARAK_GRIP_STEP_OUT off it, or FarGripStand's
    Position stand;
};

struct GripCast
{
    bool valid = false;
    ObjectGuid dk;
    ObjectGuid sphere;
    uint32 castMs = 0;
    // Where the DK stood: the sphere comes down there
    Position from;
    bool landed = false;
};

struct BurrowerExit
{
    uint8 side = 0;
    uint32 seenMs = 0;
    // Its spawn or its last emerge
    uint32 upMs = 0;
    bool down = false;
};

struct SideNeed
{
    bool open = false;
    uint32 sinceMs = 0;
    // Its last grip failed and no other has gone out yet
    bool gripFailed = false;
};

// Heroic: the kiter's stage, memoised per instance per ms
struct KiteStageRead
{
    bool valid = false;
    uint32 ms = 0;
    ObjectGuid kiter;
    AnubarakKiteStage stage = AnubarakKiteStage::None;
    // Set only when it goes out now
    AnubarakKiteBoost boost;
};

// Heroic: how the kite ends and how it gets there, kept for the mark and judged every 250 ms
struct KitePlan
{
    bool valid = false;
    ObjectGuid kiter;
    uint32 judgedMs = 0;
    AnubarakPursuit::KiteTerminal terminal = AnubarakPursuit::KiteTerminal::None;
    // Whose Hand of Protection ends or bridges it: the escort, or a marked paladin itself
    ObjectGuid hand;
    bool handLanded = false;
    // Seconds to the catch on the run it is on now, negative past the horizon
    float catchS = -1.0f;
    // A patch the kite may end on, its stand, and PatchMargin of the walk there now and after running on
    // ANUBARAK_KITE_PATCH_COMMIT_S
    bool patch = false;
    Position patchPos;
    Position stand;
    float patchMarginS = -1.0f;
    float patchLaterS = -1.0f;
    // The run's heading, held against flip-flops
    bool headingValid = false;
    AnubarakPursuit::Point heading;
    uint32 headingMs = 0;
};

struct MeleeStandLatch
{
    ObjectGuid burrower;
    // Where the burrower stood at the pick
    Position at;
    Position stand;
};

// What the refill spots were built from: the patches held, the spawn slots of the spheres left
struct RefillKey
{
    ObjectGuid held[2];
    uint8 homes = 0;
    bool firstPairDone = false;

    bool operator==(RefillKey const& other) const
    {
        return held[0] == other.held[0] && held[1] == other.held[1] && homes == other.homes &&
               firstPairDone == other.firstPairDone;
    }
};

struct AnubarakState
{
    RaidObs::ObsValue<uint32> phase{"anub.phase"};
    RaidObs::ObsValue<uint32> patches{"anub.patches"};
    RaidObs::ObsValue<uint32> flying{"anub.flying"};
    RaidObs::ObsValue<ObjectGuid> patch0{"anub.patch0"};
    RaidObs::ObsValue<ObjectGuid> patch1{"anub.patch1"};
    // Each side's burrower DPS focus
    RaidObs::ObsValue<ObjectGuid> focus0{"anub.focus0"};
    RaidObs::ObsValue<ObjectGuid> focus1{"anub.focus1"};
    RaidObs::ObsValue<ObjectGuid> side0{"anub.side0"};
    RaidObs::ObsValue<ObjectGuid> side1{"anub.side1"};
    RaidObs::ObsValue<uint32> wave{"anub.wave"};
    RaidObs::ObsValue<uint32> grip{"anub.grip"};
    RaidObs::ObsValue<uint32> allowance{"anub.allow"};
    RaidObs::ObsGuidMap<uint32> burrowerSide{"anub.bside"};
    // Each living DPS bot's side for the burrowers, split at the wave that set it
    RaidObs::ObsGuidMap<uint32> dpsSide{"anub.dside"};
    bool dpsSideValid = false;
    uint32 dpsSideWave = 0;

    // 0 is a real getMSTime value, hence the valid flags
    bool engagedSeen = false;
    uint32 lastEngagedMs = 0;
    uint32 pullMs = 0;
    bool swarm = false;
    // The pull or the last emerge: the script submerges him 80 s after either
    uint32 surfacedMs = 0;
    uint32 submergedMs = 0;
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
    // Heroic: the kiter that turned for its kite patch, until the mark moves on
    ObjectGuid kitePatchKiter;

    // Every burrower seen this pull: the side of the exit it came out of, and when
    std::unordered_map<ObjectGuid, BurrowerExit> exits;
    bool waveSeen = false;
    uint32 lastWaveMs = 0;
    bool ownerMemoValid = false;
    uint32 ownerMemoMs = 0;
    std::unordered_map<ObjectGuid, uint8> owners;

    // A melee bot's stand on its side's focus, held while that burrower lives and stays put
    std::unordered_map<ObjectGuid, MeleeStandLatch> meleeStands;

    // Heroic: tank patches this submerge's kites may still spend
    uint32 kiteAllowance = 0;
    KiteStageRead kiteStage;
    KitePlan kitePlan;
    // Where he went down, for the heroic phase 2 gather
    bool submergePosValid = false;
    Position submergePos;

    SideNeed needs[2];
    bool gripUsed = false;
    GripCast grips[2];
    std::vector<GripDuty> gripDuties;
    // A sphere that landed on a DK, and the side it was gripped for. A gripped sphere drifts back toward
    // its route while it dies, so its patch can come down nearer the other side's spot.
    std::unordered_map<ObjectGuid, uint8> grippedFor;
    // Gripped from past the step-out, so its patch serves its side however far from the spot it lands
    std::unordered_set<ObjectGuid> farGripped;

    // Each sphere's slot in ANUBARAK_SPHERE_HOMES, read at its first sample. Heroic never respawns one.
    std::unordered_map<ObjectGuid, uint8> sphereHomes;
    // Both sides have held a patch this pull
    bool firstPairDone = false;
    // Heroic after the first pair: an open side's spot by the spheres left
    bool refillKeyValid = false;
    RefillKey refillKey;
    bool refillValid[2] = {false, false};
    Position refill[2];
    RaidObs::ObsValue<Position> spot0{"anub.spot0"};
    RaidObs::ObsValue<Position> spot1{"anub.spot1"};

    // The healer stand while he is up, and heroic's phase 2 gather. Held for a submerge, so a tank patch
    // spent while he is down doesn't move the gather under the raid.
    bool raidPointValid = false;
    Position raidPointAnchor;
    Position raidPoint;
    bool raidPointHeld = false;
    uint32 raidPointHeldMs = 0;

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
    // Last time he stood near the anchor or a leg was latched, for the drag's stall guard
    bool dragFreeValid = false;
    uint32 dragFreeMs = 0;
    float submergeSpotClearance = 0.0f;

    std::unordered_map<ObjectGuid, DodgeHold> dodgeHolds;
    std::unordered_map<ObjectGuid, ThreatCapLatch> threatCaps;
    ProtectClaim protect;
    EscortRead escort;
    FreedomRead freedom;
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
    state.focus0 = ObjectGuid::Empty;
    state.focus1 = ObjectGuid::Empty;
    state.wave = 0;
    state.grip = 0;
    state.allowance = 0;
    state.burrowerSide.clear();
    state.dpsSide.clear();
    state.dpsSideValid = false;
    state.exits.clear();
    state.waveSeen = false;
    state.ownerMemoValid = false;
    state.owners.clear();
    state.meleeStands.clear();
    state.kiteAllowance = 0;
    state.kitePatchKiter.Clear();
    state.kiteStage = KiteStageRead();
    state.kitePlan = KitePlan();
    state.submergePosValid = false;
    state.needs[0] = SideNeed();
    state.needs[1] = SideNeed();
    state.gripUsed = false;
    for (GripCast& cast : state.grips)
        cast = GripCast();
    state.gripDuties.clear();
    state.grippedFor.clear();
    state.farGripped.clear();
    state.sphereHomes.clear();
    state.firstPairDone = false;
    state.refillKeyValid = false;
    state.refillValid[0] = false;
    state.refillValid[1] = false;
    state.raidPointValid = false;
    state.raidPointHeld = false;
    state.duties.clear();
    state.bossStillValid = false;
    state.drag = DragLegLatch();
    state.dragFreeValid = false;
    state.dodgeHolds.clear();
    state.threatCaps.clear();
    state.protect = ProtectClaim();
    state.escort = EscortRead();
    state.freedom = FreedomRead();
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
    state.kitePatchKiter.Clear();
    state.kitePlan = KitePlan();

    // A bot waits out a cast in progress before any action runs, its kite included
    PlayerbotAI* ai = target ? GET_PLAYERBOT_AI(target) : nullptr;
    if (ai && (target->GetCurrentSpell(CURRENT_GENERIC_SPELL) || target->GetCurrentSpell(CURRENT_CHANNELED_SPELL)))
        ai->RequestSpellInterrupt();
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

bool OnFloor(Player* bot, Position const& raw, Position& valid)
{
    valid = ValidateFloorPoint(bot, raw);
    return Dist2d(valid, raw) <= ANUBARAK_FLOOR_XY_TOLERANCE &&
           std::fabs(valid.GetPositionZ() - raw.GetPositionZ()) <= ANUBARAK_FLOOR_Z_TOLERANCE;
}

void RecordSphereHomes(AnubarakState& state, std::vector<Creature*> const& spheres)
{
    for (Creature* sphere : spheres)
    {
        if (!IsFrostSphereFlying(sphere) || state.sphereHomes.count(sphere->GetGUID()))
            continue;

        uint8 nearest = 0;
        for (uint8 slot = 1; slot < ANUBARAK_SPHERE_HOME_COUNT; ++slot)
            if (Dist2d(sphere->GetPosition(), ANUBARAK_SPHERE_HOMES[slot]) <
                Dist2d(sphere->GetPosition(), ANUBARAK_SPHERE_HOMES[nearest]))
                nearest = slot;

        state.sphereHomes[sphere->GetGUID()] = nearest;
    }
}

// Pulled in toward the room centre to the refill floor. False off the navmesh.
bool RefillOnFloor(Player* bot, Position raw, Position& out)
{
    Vec2 const fromCenter = Delta(ANUBARAK_ROOM_CENTER, raw);
    float const radius = Length(fromCenter);
    if (radius > ANUBARAK_REFILL_FLOOR)
        raw = Offset(ANUBARAK_ROOM_CENTER, {fromCenter.x / radius, fromCenter.y / radius}, ANUBARAK_REFILL_FLOOR);

    raw.m_positionZ = ANUBARAK_ROOM_CENTER.GetPositionZ();
    return OnFloor(bot, raw, out);
}

// Heroic after the first pair, so the patch comes down where the spheres still fly (they drift round
// their spawns). One side held: the open spot is on the line from the held patch to the nearest spawn
// past it on the open side (north is +y), 20 to 30 yd out. Both open: the nearest north and south spawns
// 20+ apart, pulled in until 30 apart. Run after the latches: a falling sphere keeps its spawn counted,
// so the spot it falls for holds until its patch has latched.
void UpdateRefillSpots(Player* bot, AnubarakState& state, std::vector<Creature*> const& spheres)
{
    RefillKey key;
    key.firstPairDone = state.firstPairDone;
    key.held[0] = state.patch0.Get();
    key.held[1] = state.patch1.Get();
    for (ObjectGuid& guid : key.held)
        if (!IsPermafrostPatch(FindSphere(spheres, guid)))
            guid.Clear();

    std::vector<uint8> homes;
    for (Creature* sphere : spheres)
    {
        auto const home = state.sphereHomes.find(sphere->GetGUID());
        if (home == state.sphereHomes.end() || !(IsFrostSphereFlying(sphere) || IsFrostSphereFalling(sphere)) ||
            (key.homes & (1u << home->second)))
            continue;

        key.homes = static_cast<uint8>(key.homes | (1u << home->second));
        homes.push_back(home->second);
    }

    if (state.refillKeyValid && state.refillKey == key)
        return;

    state.refillKeyValid = true;
    state.refillKey = key;
    state.refillValid[0] = false;
    state.refillValid[1] = false;
    if (!key.firstPairDone || !AnubarakHeroic(bot))
        return;

    bool const open[2] = {key.held[0].IsEmpty(), key.held[1].IsEmpty()};
    if (open[0] && open[1])
    {
        int8 north = -1;
        int8 south = -1;
        float apart = 0.0f;
        for (uint8 a : homes)
            for (uint8 b : homes)
            {
                Position const& n = ANUBARAK_SPHERE_HOMES[a];
                Position const& s = ANUBARAK_SPHERE_HOMES[b];
                float const dist = Dist2d(n, s);
                if (n.GetPositionY() <= s.GetPositionY() || dist < ANUBARAK_TANK_PATCH_GAP ||
                    (north >= 0 && dist >= apart))
                    continue;

                north = static_cast<int8>(a);
                south = static_cast<int8>(b);
                apart = dist;
            }

        if (north < 0)
            return;

        Position n = ANUBARAK_SPHERE_HOMES[north];
        Position s = ANUBARAK_SPHERE_HOMES[south];
        if (apart > ANUBARAK_REFILL_MAX)
        {
            Vec2 const toSouth = {(s.GetPositionX() - n.GetPositionX()) / apart,
                                  (s.GetPositionY() - n.GetPositionY()) / apart};
            float const pull = (apart - ANUBARAK_REFILL_MAX) / 2.0f;
            n = Offset(n, toSouth, pull);
            s = Offset(s, {-toSouth.x, -toSouth.y}, pull);
        }

        Position spots[2];
        if (RefillOnFloor(bot, n, spots[0]) && RefillOnFloor(bot, s, spots[1]) &&
            Dist2d(spots[0], spots[1]) >= ANUBARAK_TANK_PATCH_GAP)
            for (uint8 side = 0; side < 2; ++side)
            {
                state.refill[side] = spots[side];
                state.refillValid[side] = true;
            }

        return;
    }

    if (open[0] == open[1])
        return;

    uint8 const side = open[0] ? 0 : 1;
    Position const kept = FindSphere(spheres, key.held[1 - side])->GetPosition();
    int8 best = -1;
    float bestDist = 0.0f;
    for (uint8 slot : homes)
    {
        Position const& home = ANUBARAK_SPHERE_HOMES[slot];
        bool const past = side == 0 ? home.GetPositionY() > kept.GetPositionY()
                                    : home.GetPositionY() < kept.GetPositionY();
        float const dist = Dist2d(home, kept);
        if (!past || dist < ANUBARAK_TANK_PATCH_GAP || (best >= 0 && dist >= bestDist))
            continue;

        best = static_cast<int8>(slot);
        bestDist = dist;
    }

    if (best < 0)
        return;

    Vec2 const toward = Delta(kept, ANUBARAK_SPHERE_HOMES[best]);
    Position const raw = Offset(kept, {toward.x / bestDist, toward.y / bestDist},
                                std::min(bestDist, ANUBARAK_REFILL_MAX));
    Position spot;
    if (RefillOnFloor(bot, raw, spot) && Dist2d(spot, kept) >= ANUBARAK_TANK_PATCH_GAP)
    {
        state.refill[side] = spot;
        state.refillValid[side] = true;
    }
}

// The side's patch. Heroic after the first pair, an open side's refill spot. Else its side point moved
// with the other side's patch, so a patch shot off its spot drags the other side's spot along and the
// pair keeps its spacing.
Position SideSpot(AnubarakState const& state, std::vector<Creature*> const& spheres, ObjectGuid const held[2],
                  uint8 side)
{
    Creature* own = FindSphere(spheres, held[side]);
    if (IsPermafrostPatch(own))
        return own->GetPosition();

    if (state.refillValid[side])
        return state.refill[side];

    Position const point = GetAnubarakTankSidePoint(side);
    Creature* other = FindSphere(spheres, held[1 - side]);
    if (!IsPermafrostPatch(other))
        return point;

    Position const otherPoint = GetAnubarakTankSidePoint(1 - side);
    return Position(other->GetPositionX() + point.GetPositionX() - otherPoint.GetPositionX(),
                    other->GetPositionY() + point.GetPositionY() - otherPoint.GetPositionY(), point.GetPositionZ());
}

// At right angles to the line through the tank spots, on the room centre's side of the anchor, west on a
// tie. Follows the anchor while he is up, held from the first read of a submerge to the emerge.
void UpdateRaidPoint(Player* bot, AnubarakState& state, Position const& spot0, Position const& spot1)
{
    bool const down = state.phaseValue == AnubarakPhase::Submerged;
    if (down && state.raidPointValid && state.raidPointHeld && state.raidPointHeldMs == state.submergedMs)
        return;

    state.raidPointHeld = down;
    state.raidPointHeldMs = state.submergedMs;
    Position const anchor((spot0.GetPositionX() + spot1.GetPositionX()) / 2.0f,
                          (spot0.GetPositionY() + spot1.GetPositionY()) / 2.0f,
                          (spot0.GetPositionZ() + spot1.GetPositionZ()) / 2.0f);
    if (state.raidPointValid && Dist2d(anchor, state.raidPointAnchor) < ANUBARAK_RAID_POINT_SLACK)
        return;

    Vec2 const along = Delta(spot0, spot1);
    float const length = Length(along);
    Vec2 normal = length > 1.0f ? Vec2{-along.y / length, along.x / length} : Vec2{-1.0f, 0.0f};
    Vec2 const toCenter = Delta(anchor, ANUBARAK_ROOM_CENTER);
    float const lean = normal.x * toCenter.x + normal.y * toCenter.y;
    if (lean <= -1.0f || (lean < 1.0f && normal.x > 0.0f))
        normal = {-normal.x, -normal.y};

    state.raidPointValid = true;
    state.raidPointAnchor = anchor;
    state.raidPoint = ValidateFloorPoint(bot, Offset(anchor, normal, ANUBARAK_RAID_POINT_OFFSET));
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
    Position const spots[2] = {SideSpot(state, spheres, held, 0), SideSpot(state, spheres, held, 1)};
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

// Within the latch of this side's spot and nearer it than the other side's, so a patch far off its spot
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

// A gripped sphere is the side's it was gripped for, wherever it drifted to, within the latch of the spot
// unless it was gripped from far off. Any other by ServesSide.
bool PatchFor(AnubarakState const& state, Unit* sphere, uint8 side, Position const& spot, Position const& otherSpot)
{
    int8 const gripped = GrippedSide(state, sphere->GetGUID());
    if (gripped >= 0)
        return gripped == side && (state.farGripped.count(sphere->GetGUID()) ||
                                   Dist2d(sphere->GetPosition(), spot) <= ANUBARAK_TANK_PATCH_LATCH);

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

        Position const point = SideSpot(state, spheres, held, side);
        Position const otherPoint = SideSpot(state, spheres, held, 1 - side);
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
        if (done)
            state.farGripped.erase(it->first);

        it = done ? state.grippedFor.erase(it) : std::next(it);
    }

    state.patch0 = held[0];
    state.patch1 = held[1];
}

// A burrower is seen first at its exit: the two north exits feed side 0, the two south side 1. A new
// one more than 10 s after the last opens a wave. It comes up there, and again at each emerge.
void TrackWaves(Player* bot, AnubarakState& state, uint32 now)
{
    for (Creature* burrower : ResolveAlive(bot, state.burrowers))
    {
        ObjectGuid const guid = burrower->GetGUID();
        bool const down = burrower->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
        auto const known = state.exits.find(guid);
        if (known != state.exits.end())
        {
            BurrowerExit& exit = known->second;
            if (exit.down && !down)
                exit.upMs = now;

            exit.down = down;
            continue;
        }

        uint8 const side = burrower->GetPositionY() >= ANUBARAK_ROOM_CENTER.GetPositionY() ? 0 : 1;
        state.exits[guid] = BurrowerExit{side, now, now, down};
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

// One tank patch this submerge, while a sphere flies spare of the missing ones to replace it after
void ComputeKiteAllowance(Player* bot, AnubarakState& state, std::vector<Creature*> const& spheres)
{
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

    state.kiteAllowance = flying > missing ? 1 : 0;
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
        state->pullMs = now;
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
    {
        state->drag = DragLegLatch();
        state->dragFreeValid = false;
    }

    std::vector<Creature*> const spheres = ResolveSpheres(bot, *state);
    bool tankPatchLost = false;
    for (ObjectGuid const& guid : {state->patch0.Get(), state->patch1.Get()})
        tankPatchLost = tankPatchLost || (!guid.IsEmpty() && !IsPermafrostPatch(FindSphere(spheres, guid)));

    RecordSphereHomes(*state, spheres);
    RefreshTankPatches(*state, spheres);
    if (!state->patch0.Get().IsEmpty() && !state->patch1.Get().IsEmpty())
        state->firstPairDone = true;

    UpdateRefillSpots(bot, *state, spheres);

    bool placed[2] = {false, false};
    RefreshSideTanks(bot, *state, placed);
    FollowHumanSideTank(bot, *state, spheres, placed, now);
    state->side0 = state->sideTanks[0];
    state->side1 = state->sideTanks[1];

    ObjectGuid const held[2] = {state->patch0.Get(), state->patch1.Get()};
    Position const spots[2] = {SideSpot(*state, spheres, held, 0), SideSpot(*state, spheres, held, 1)};
    state->spot0 = spots[0];
    state->spot1 = spots[1];
    UpdateRaidPoint(bot, *state, spots[0], spots[1]);

    TrackWaves(bot, *state, now);
    if (submerging && AnubarakHeroic(bot))
        ComputeKiteAllowance(bot, *state, spheres);
    // Whatever spent it, a kite or a spike passing by, that was the submerge's one
    else if (tankPatchLost && state->phaseValue == AnubarakPhase::Submerged)
    {
        state->kiteAllowance = 0;
        state->allowance = 0;
    }

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

// Heroic phase 2: the raid point, where the healers already stand. He is dragged 25+ yd off it before he
// goes down, and the spike is summoned there.
bool GatherCentre(Player* bot, AnubarakState const& state, Position& out)
{
    if (state.phaseValue != AnubarakPhase::Submerged || !state.raidPointValid || !AnubarakHeroic(bot))
        return false;

    out = state.raidPoint;
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

// Permafrost's slow drops a moment after the kiter steps off the patch, so it never decides the race
float KiterRunSpeed(Player* kiter)
{
    float const speed = kiter->GetSpeed(MOVE_RUN);
    return UnitOnPermafrost(kiter) ? std::max(speed, playerBaseMoveSpeed[MOVE_RUN]) : speed;
}

AnubarakPursuit::Point ToPoint(Position const& pos) { return {pos.GetPositionX(), pos.GetPositionY()}; }

// The speed step comes off the aura it carries: none at all means it idles after a patch and restarts
// from the base speed
AnubarakPursuit::Spike PursuitSpike(Creature* spike)
{
    AnubarakPursuit::Spike out;
    out.pos = ToPoint(spike->GetPosition());
    out.speed = spike->GetSpeed(MOVE_RUN);
    if (spike->HasAura(SPELL_SPIKE_SPEED3))
        out.step = AnubarakPursuit::LAST_STEP;
    else if (Aura const* aura = spike->GetAura(SPELL_SPIKE_SPEED2))
    {
        out.step = 1;
        out.nextStepS = static_cast<float>(std::max(aura->GetDuration(), 0)) / IN_MILLISECONDS;
    }
    else if (Aura const* aura = spike->GetAura(SPELL_SPIKE_SPEED1))
        out.nextStepS = static_cast<float>(std::max(aura->GetDuration(), 0)) / IN_MILLISECONDS;

    return out;
}

// Round the room away from the spike's bearing, as RingStand turns
float RingTurn(Position const& kiter, Position const& spike)
{
    Vec2 const fromCenter = Delta(ANUBARAK_ROOM_CENTER, kiter);
    Vec2 const spikeVec = Delta(ANUBARAK_ROOM_CENTER, spike);
    float const opening = std::remainder(std::atan2(fromCenter.y, fromCenter.x) - std::atan2(spikeVec.y, spikeVec.x),
                                         2.0f * static_cast<float>(M_PI));
    return opening >= 0.0f ? 1.0f : -1.0f;
}

// The kiter's run from here along dir, then round the ring
AnubarakPursuit::KiterPath KiterRun(Player* kiter, Creature* spike, Vec2 dir)
{
    Position const& from = kiter->GetPosition();
    return AnubarakPursuit::MakeKiterPath(ToPoint(from), {dir.x, dir.y}, KiterRunSpeed(kiter),
                                          ToPoint(ANUBARAK_ROOM_CENTER), ANUBARAK_KITE_RING_RADIUS,
                                          RingTurn(from, spike->GetPosition()));
}

// How far off the spike and its lane a bot dodges, further while the spike outruns it
struct SpikeDodgeReach
{
    float danger;
    float clearance;
    float lane;
    float laneClearance;
};

SpikeDodgeReach DodgeReach(Player* bot, Creature* spike)
{
    if (spike->GetSpeed(MOVE_RUN) > bot->GetSpeed(MOVE_RUN))
        return {ANUBARAK_SPIKE_DANGER_RADIUS_FAST, ANUBARAK_SPIKE_CLEARANCE_FAST, ANUBARAK_SPIKE_LANE_HALF_WIDTH_FAST,
                ANUBARAK_SPIKE_LANE_CLEARANCE_FAST};

    return {ANUBARAK_SPIKE_DANGER_RADIUS, ANUBARAK_SPIKE_CLEARANCE, ANUBARAK_SPIKE_LANE_HALF_WIDTH,
            ANUBARAK_SPIKE_LANE_CLEARANCE};
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
    // A latched tank patch
    bool tank = false;
    bool gather = false;
};

std::vector<LaneObstacle> LaneObstacles(Player* kiter, AnubarakState const& state)
{
    std::vector<Position> tanks;
    for (uint8 side = 0; side < 2; ++side)
        if (Creature* patch = LatchedTankPatch(kiter, state, side))
            tanks.push_back(patch->GetPosition());

    std::vector<LaneObstacle> obstacles;
    for (Position const& patch : PatchPositions(kiter, state))
    {
        bool const tank = std::any_of(tanks.begin(), tanks.end(),
                                      [&patch](Position const& held) { return Dist2d(held, patch) < 0.5f; });
        obstacles.push_back({patch, ANUBARAK_SPIKE_ROUTE_CLEARANCE, tank, false});
    }

    Position gather;
    if (GatherCentre(kiter, state, gather))
        obstacles.push_back({gather, ANUBARAK_KITE_GATHER_CLEARANCE, false, true});

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

bool UnderHandOfProtection(Unit* unit)
{
    return std::any_of(std::begin(HAND_OF_PROTECTION_RANKS), std::end(HAND_OF_PROTECTION_RANKS),
                       [unit](uint32 rank) { return unit->HasAura(rank); });
}

// After a fail the spike restarts from the patch it teleported onto, and its first test reaches 11.5 yd
// plus its 5.25 yd first walk, so heroic never kites onto a stray patch that close to a tank patch
bool NearTankPatch(Position const& patch, std::vector<Position> const& tankSpots)
{
    return std::any_of(tankSpots.begin(), tankSpots.end(), [&patch](Position const& tank)
                       { return Dist2d(patch, tank) < ANUBARAK_SUBMERGE_PATCH_CLEARANCE_MIN; });
}

bool SideHoldsBurrower(Player* bot, AnubarakState const& state, uint8 side)
{
    ObjectGuid const& tank = state.sideTanks[side];
    if (tank.IsEmpty())
        return false;

    std::vector<Creature*> const burrowers = ResolveAlive(bot, state.burrowers);
    return std::any_of(burrowers.begin(), burrowers.end(), [&tank](Creature* burrower)
                       { return burrower->GetVictim() && burrower->GetVictim()->GetGUID() == tank; });
}

// Which patches a kite may end on, lower first: -1 never. Heroic takes a stray patch clear of the tank
// patches, and a tank patch while the submerge's allowance lasts, a side holding no burrower first, since
// one held off Permafrost submerges back to full. Normal takes any, a tank patch last while a burrower
// lives.
int8 KitePatchRank(Player* kiter, AnubarakState const& state, Creature* sphere, bool heroic, bool burrowers)
{
    if (!IsPermafrostPatch(sphere))
        return -1;

    int8 side = -1;
    std::vector<Position> tankSpots;
    for (uint8 s = 0; s < 2; ++s)
    {
        Creature* patch = LatchedTankPatch(kiter, state, s);
        if (!patch)
            continue;

        tankSpots.push_back(patch->GetPosition());
        if (patch == sphere)
            side = static_cast<int8>(s);
    }

    if (!heroic)
        return side >= 0 && burrowers ? 1 : 0;

    if (side < 0)
        return NearTankPatch(sphere->GetPosition(), tankSpots) ? -1 : 0;

    if (!state.kiteAllowance)
        return -1;

    return SideHoldsBurrower(kiter, state, static_cast<uint8>(side)) ? 1 : 0;
}

// The patch the kite ends on, its rank first, then the nearest the kiter, and its stand past it off the
// spike, not floor checked. False with none.
bool KitePatchStand(Player* kiter, Creature* spike, AnubarakState const& state, Position& stand,
                    Creature** patch = nullptr)
{
    bool const heroic = AnubarakHeroic(kiter);
    bool const burrowers = AnyBurrower(kiter, state);

    Creature* best = nullptr;
    int8 bestRank = 0;
    float bestDist = 0.0f;
    for (Creature* sphere : ResolveSpheres(kiter, state))
    {
        int8 const rank = KitePatchRank(kiter, state, sphere, heroic, burrowers);
        if (rank < 0)
            continue;

        float const dist = kiter->GetExactDist2d(sphere->GetPositionX(), sphere->GetPositionY());
        if (!best || rank < bestRank || (rank == bestRank && dist < bestDist))
        {
            best = sphere;
            bestRank = rank;
            bestDist = dist;
        }
    }

    if (!best)
        return false;

    Vec2 away = Delta(spike->GetPosition(), best->GetPosition());
    float const length = Length(away);
    // The spike is on it, so it's already being used up
    if (length < 0.5f)
        return false;

    stand = Offset(best->GetPosition(), {away.x / length, away.y / length}, ANUBARAK_KITE_STAND_OFFSET);
    if (patch)
        *patch = best;

    return true;
}

float WalkMs(Player* kiter, Position const& stand)
{
    float const speed = KiterRunSpeed(kiter);
    return speed > 0.0f ? Dist2d(kiter->GetPosition(), stand) / speed * IN_MILLISECONDS : 0.0f;
}

// ms is how long the spike's pursuit takes to close to Impale's reach, the kiter running the way it faces
// and then round the ring (away from the spike when it stands), the spike's speed steps in: a spike
// slower now still catches once it steps up. False when it doesn't within the horizon, ms the horizon
// then; 0 inside the reach.
// The way the kiter runs on: where it faces, away from the spike while it stands
Vec2 RunDir(Player* kiter, Creature* spike)
{
    Vec2 const away = Delta(spike->GetPosition(), kiter->GetPosition());
    float const length = Length(away);
    if (kiter->isMoving() || length < 0.1f)
        return {std::cos(kiter->GetOrientation()), std::sin(kiter->GetOrientation())};

    return {away.x / length, away.y / length};
}

bool ImpaleInMs(Player* kiter, Creature* spike, float& ms, float horizonS = ANUBARAK_KITE_CATCH_HORIZON_S)
{
    float const dist = Dist2d(spike->GetPosition(), kiter->GetPosition());
    if (dist <= ANUBARAK_IMPALE_REACH)
    {
        ms = 0.0f;
        return true;
    }

    AnubarakPursuit::Pursuit const run = AnubarakPursuit::Simulate(
        PursuitSpike(spike), KiterRun(kiter, spike, RunDir(kiter, spike)), horizonS, {}, ANUBARAK_IMPALE_REACH);
    ms = (run.catchS >= 0.0f ? run.catchS : horizonS) * IN_MILLISECONDS;
    return run.catchS >= 0.0f;
}

bool ChooseKitePatch(Player* kiter, Creature* spike, AnubarakState const& state, std::vector<Creature*> const& spheres,
                     Creature*& patch, Position& stand);

// The kite has a patch to end on, and the stand its walk is timed on: the one the kite would walk to,
// else KitePatchStand's
bool KiteWalkStand(Player* kiter, Creature* spike, AnubarakState const& state, Position& stand,
                   Creature** patchOut = nullptr)
{
    Creature* best = nullptr;
    if (!KitePatchStand(kiter, spike, state, stand, &best))
        return false;

    Creature* patch = nullptr;
    Position chosen;
    if (ChooseKitePatch(kiter, spike, state, ResolveSpheres(kiter, state), patch, chosen))
    {
        stand = chosen;
        best = patch;
    }

    if (patchOut)
        *patchOut = best;

    return true;
}

// The lane steps' headings: a 10 yd step that keeps the spike behind and no nearer, inside the ring
// radius or no further out than the kiter already is
std::vector<std::pair<Vec2, Position>> RunHeadings(Player* kiter, Creature* spike)
{
    Position const& from = kiter->GetPosition();
    float const gap = Dist2d(from, spike->GetPosition());
    float const radius = std::max(ANUBARAK_KITE_RING_RADIUS, Dist2d(from, ANUBARAK_ROOM_CENTER));
    std::vector<std::pair<Vec2, Position>> headings;
    for (uint32 i = 0; i < ANUBARAK_KITE_LANE_HEADINGS; ++i)
    {
        float const angle = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) /
                            static_cast<float>(ANUBARAK_KITE_LANE_HEADINGS);
        Vec2 const dir = {std::cos(angle), std::sin(angle)};
        Position const raw = Offset(from, dir, ANUBARAK_KITE_LANE_STEP);
        if (Dist2d(raw, spike->GetPosition()) < gap || !SpikeBehind(kiter, spike, raw) ||
            Dist2d(raw, ANUBARAK_ROOM_CENTER) > radius)
            continue;

        headings.emplace_back(dir, raw);
    }

    return headings;
}

float RunCatchS(AnubarakPursuit::Spike const& chase, AnubarakPursuit::KiterPath const& path)
{
    AnubarakPursuit::Pursuit const run =
        AnubarakPursuit::Simulate(chase, path, ANUBARAK_KITE_PLAN_HORIZON_S, {}, ANUBARAK_IMPALE_REACH);
    return run.catchS >= 0.0f ? run.catchS : ANUBARAK_KITE_PLAN_HORIZON_S;
}

// The latest catch over the run headings, the plan horizon when one outlasts it
float BestRunCatchS(Player* kiter, Creature* spike)
{
    AnubarakPursuit::Spike const chase = PursuitSpike(spike);
    float best = -1.0f;
    for (auto const& [dir, raw] : RunHeadings(kiter, spike))
        best = std::max(best, RunCatchS(chase, KiterRun(kiter, spike, dir)));

    if (best >= 0.0f)
        return best;

    float ms = 0.0f;
    ImpaleInMs(kiter, spike, ms, ANUBARAK_KITE_PLAN_HORIZON_S);
    return ms / IN_MILLISECONDS;
}

float HandLeftS(Unit* kiter)
{
    int32 left = 0;
    for (uint32 rank : HAND_OF_PROTECTION_RANKS)
        if (Aura const* aura = kiter->GetAura(rank))
            left = std::max(left, aura->GetDuration());

    return static_cast<float>(left) / IN_MILLISECONDS;
}

// PatchMargin of the walk to the stand now, round the patch when the straight walk clips its slow
float PatchWalkMargin(Player* kiter, Creature* spike, Position const& patch, Position const& stand)
{
    AnubarakPursuit::LegPath walk;
    walk.start = ToPoint(kiter->GetPosition());
    walk.speed = KiterRunSpeed(kiter);
    if (PathClips(kiter->GetPosition(), stand, patch))
        walk.legs.push_back(ToPoint(DetourWaypoint(kiter, patch, stand)));

    walk.legs.push_back(ToPoint(stand));
    return AnubarakPursuit::PatchMargin(PursuitSpike(spike), walk, ToPoint(patch), ANUBARAK_SPIKE_PATCH_REACH,
                                        ANUBARAK_IMPALE_REACH, HandLeftS(kiter), ANUBARAK_KITE_CATCH_HORIZON_S,
                                        ANUBARAK_KITE_PATCH_LEAD_S);
}

// The same walk, started after running on along dir for ANUBARAK_KITE_PATCH_COMMIT_S
float PatchLaterMargin(Player* kiter, Creature* spike, Position const& patch, Vec2 dir)
{
    return AnubarakPursuit::PatchMarginAfter(PursuitSpike(spike), KiterRun(kiter, spike, dir),
                                             ANUBARAK_KITE_PATCH_COMMIT_S, KiterRunSpeed(kiter), ToPoint(patch),
                                             ANUBARAK_KITE_STAND_OFFSET, ANUBARAK_SPIKE_PATCH_REACH,
                                             ANUBARAK_IMPALE_REACH, HandLeftS(kiter), ANUBARAK_KITE_CATCH_HORIZON_S,
                                             ANUBARAK_KITE_PATCH_LEAD_S);
}

// Known, ready and affordable, the GCD aside: CanCastSpell fails on that, and a pick made on it flips
bool HandReady(Player* paladin)
{
    return paladin->getClass() == CLASS_PALADIN && ReadyInstant(paladin, KnownRank(paladin, HAND_OF_PROTECTION_RANKS));
}

constexpr uint32 SPELL_ICE_BLOCK = 45438;
constexpr uint32 SPELL_HYPOTHERMIA = 41425;
constexpr uint32 VANISH_RANKS[] = {26889, 1857, 1856};
constexpr uint32 SPELL_FEIGN_DEATH = 5384;

// The kiter's own Divine Shield, Ice Block, Vanish or Feign Death ready now, else 0
uint32 OwnImmunitySpell(Player* kiter)
{
    uint32 spell = 0;
    switch (kiter->getClass())
    {
        case CLASS_PALADIN:
            spell = kiter->HasAura(SPELL_FORBEARANCE) ? 0 : SPELL_DIVINE_SHIELD;
            break;
        case CLASS_MAGE:
            spell = kiter->HasAura(SPELL_HYPOTHERMIA) ? 0 : SPELL_ICE_BLOCK;
            break;
        case CLASS_ROGUE:
            spell = KnownRank(kiter, VANISH_RANKS);
            break;
        case CLASS_HUNTER:
            spell = SPELL_FEIGN_DEATH;
            break;
        default:
            break;
    }

    return spell && GET_PLAYERBOT_AI(kiter) && ReadyInstant(kiter, spell) ? spell : 0;
}

std::vector<Player*> ProtectorOrder(Player* bot);

// Paladin bots with Hand of Protection ready for the kiter, the kiter first when it's one; none while it
// carries Forbearance
std::vector<Player*> ReadyHands(Player* kiter)
{
    std::vector<Player*> hands;
    if (kiter->HasAura(SPELL_FORBEARANCE))
        return hands;

    if (GET_PLAYERBOT_AI(kiter) && HandReady(kiter))
        hands.push_back(kiter);

    for (Player* paladin : ProtectorOrder(kiter))
        if (HandReady(paladin))
            hands.push_back(paladin);

    return hands;
}

bool HandKite(KitePlan const& plan)
{
    return (plan.terminal == AnubarakPursuit::KiteTerminal::Hand ||
            plan.terminal == AnubarakPursuit::KiteTerminal::PatchHand) &&
           !plan.handLanded && !plan.hand.IsEmpty();
}

// The planned hand still lands: on the kiter now, or its paladin alive, unmarked unless it's the kiter,
// with it ready and the kiter free of Forbearance
bool HandHolds(Player* kiter, KitePlan const& plan)
{
    if (plan.handLanded)
        return UnderHandOfProtection(kiter);

    Player* paladin = plan.hand.IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(*kiter, plan.hand);
    return paladin && paladin->IsAlive() && GET_PLAYERBOT_AI(paladin) &&
           (paladin == kiter || !paladin->HasAura(SPELL_MARK)) && !kiter->HasAura(SPELL_FORBEARANCE) &&
           HandReady(paladin);
}

char const* KiteTerminalName(AnubarakPursuit::KiteTerminal terminal)
{
    switch (terminal)
    {
        case AnubarakPursuit::KiteTerminal::Patch:
            return "patch";
        case AnubarakPursuit::KiteTerminal::PatchHand:
            return "patch+hand";
        case AnubarakPursuit::KiteTerminal::Hand:
            return "hand";
        case AnubarakPursuit::KiteTerminal::Own:
            return "own";
        default:
            return "none";
    }
}

// Heroic, every 250 ms. The terminal is picked at the mark and kept until it can't be had: the patch
// gone, the hand's paladin dead or without it, Forbearance on the kiter, the own immunity spent. A bridge
// whose hand can't come any more goes on as a patch kite.
KitePlan& JudgeKitePlan(Player* kiter, Creature* spike, AnubarakState& state)
{
    using AnubarakPursuit::KiteTerminal;
    KitePlan& plan = state.kitePlan;
    uint32 const now = getMSTime();
    bool const same = plan.valid && plan.kiter == kiter->GetGUID();
    if (same && getMSTimeDiff(plan.judgedMs, now) < ANUBARAK_KITE_PLAN_JUDGE_MS)
        return plan;

    if (!same)
    {
        plan = KitePlan();
        plan.valid = true;
        plan.kiter = kiter->GetGUID();
    }

    plan.judgedMs = now;
    plan.handLanded = plan.handLanded || (HandKite(plan) && UnderHandOfProtection(kiter));

    float catchMs = 0.0f;
    plan.catchS = ImpaleInMs(kiter, spike, catchMs) ? catchMs / IN_MILLISECONDS : -1.0f;

    Creature* patch = nullptr;
    plan.patch = KiteWalkStand(kiter, spike, state, plan.stand, &patch) && patch;
    if (plan.patch)
        plan.patchPos = patch->GetPosition();

    plan.patchMarginS = plan.patch ? PatchWalkMargin(kiter, spike, plan.patchPos, plan.stand) : -1.0f;
    plan.patchLaterS = plan.patch ? PatchLaterMargin(kiter, spike, plan.patchPos, RunDir(kiter, spike)) : -1.0f;

    bool keep = false;
    switch (plan.terminal)
    {
        case KiteTerminal::Patch:
            keep = plan.patch;
            break;
        case KiteTerminal::PatchHand:
            keep = plan.patch;
            if (keep && !plan.handLanded && !HandHolds(kiter, plan))
                plan.terminal = KiteTerminal::Patch;
            break;
        case KiteTerminal::Hand:
            keep = HandHolds(kiter, plan);
            break;
        case KiteTerminal::Own:
            keep = OwnImmunitySpell(kiter) != 0;
            break;
        default:
            break;
    }

    if (!keep)
    {
        std::vector<Player*> const hands = ReadyHands(kiter);
        AnubarakPursuit::KiteOptions options;
        options.catchS = BestRunCatchS(kiter, spike);
        options.emergeS = static_cast<float>(static_cast<int32>(state.submergedMs + ANUBARAK_SUBMERGED_MS - now)) /
                          IN_MILLISECONDS;
        options.patch = plan.patch;
        options.patchReachable = plan.patch && plan.patchMarginS >= 0.0f;
        options.hand = !hands.empty();
        options.handsReady = static_cast<int>(hands.size());
        options.own = OwnImmunitySpell(kiter) != 0;
        plan.terminal = AnubarakPursuit::PickTerminal(options);
        bool const hand = plan.terminal == KiteTerminal::Hand || plan.terminal == KiteTerminal::PatchHand;
        plan.hand = hand ? hands.front()->GetGUID() : ObjectGuid::Empty;
        plan.handLanded = false;
    }

    if (GET_PLAYERBOT_AI(kiter))
        RaidObs::NoteDerived(kiter, "anub.kiteplan", KiteTerminalName(plan.terminal));

    return plan;
}

// Heroic only. A patch kite runs on while turning ANUBARAK_KITE_PATCH_COMMIT_S later would still beat the
// catch, then turns; a bridge only once its hand is on. Turned, it stays turned while the patch lasts. A boost goes out once the spike, faster, gains within 15 yd, or is within 10 and catching up; a
// boost never holds back the turn.
KiteStageRead const& ReadKiteStage(Player* kiter, Creature* spike, AnubarakState& state)
{
    using AnubarakPursuit::KiteTerminal;
    KiteStageRead& read = state.kiteStage;
    uint32 const now = getMSTime();
    if (read.valid && read.ms == now && read.kiter == kiter->GetGUID())
        return read;

    read = KiteStageRead();
    read.valid = true;
    read.ms = now;
    read.kiter = kiter->GetGUID();

    bool const bot = GET_PLAYERBOT_AI(kiter) != nullptr;
    AnubarakKiteBoost boost;
    bool const hasBoost = bot && NextKiteBoost(kiter, spike, LaneObstacles(kiter, state), boost);

    KitePlan const& plan = JudgeKitePlan(kiter, spike, state);
    bool const patchKite = plan.patch && (plan.terminal == KiteTerminal::Patch ||
                                          (plan.terminal == KiteTerminal::PatchHand && plan.handLanded));
    if (plan.patch && state.kitePatchKiter == kiter->GetGUID())
        read.stage = AnubarakKiteStage::Patch;
    else if (patchKite && (plan.patchMarginS < 0.0f || plan.patchLaterS < 0.0f))
        read.stage = AnubarakKiteStage::Patch;
    else if (AnubarakKiterImmune(kiter))
        read.stage = AnubarakKiteStage::Immune;
    else
        read.stage = AnubarakKiteStage::Run;

    float const dist = Dist2d(spike->GetPosition(), kiter->GetPosition());
    bool const faster = spike->GetSpeed(MOVE_RUN) > KiterRunSpeed(kiter);
    if (hasBoost && plan.catchS >= 0.0f &&
        ((faster && dist <= ANUBARAK_KITE_BOOST_RANGE) || dist <= ANUBARAK_KITE_BOOST_CLOSE))
    {
        read.boost = boost;
        if (read.stage == AnubarakKiteStage::Run)
            read.stage = AnubarakKiteStage::Boost;
    }

    if (plan.patch && read.stage == AnubarakKiteStage::Patch)
        state.kitePatchKiter = kiter->GetGUID();

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

// What the kite plan asks of a run step
struct LaneGoal
{
    // A patch kite: turning after the commit delay on the step's heading still beats the catch
    bool patch = false;
    Position patchPos;
    // A Hand kite: the step stays within ANUBARAK_KITE_ESCORT_LEASH of its escort
    bool escort = false;
    Position escortPos;
    // An own kite: steps away from the gather, so the spike's next target starts further off
    bool away = false;
    Position gather;
};

struct LaneStep
{
    Position raw;
    Vec2 dir;
    bool goalOk;
    bool patchClear;
    // The line from the probe to the gather misses every tank patch: the spike's run there after a death
    // or a hand-off
    bool restartClear;
    bool gatherClear;
    bool leashed;
    float patchScore;
    float gatherScore;
    float fromSpike;
    int32 awayBand;
    float catchS;
    // The spike's closest pass by any patch over the pursuit horizon
    float patchPass;
};

// The goal, the spike's run clear of every patch, for an own kite the distance off the gather, then its
// run clear of the gather: what a held heading must still match the best on
int8 CompareLaneClass(LaneStep const& a, LaneStep const& b)
{
    if (a.goalOk != b.goalOk)
        return a.goalOk ? -1 : 1;
    if (a.patchClear != b.patchClear)
        return a.patchClear ? -1 : 1;
    if (a.awayBand != b.awayBand)
        return a.awayBand > b.awayBand ? -1 : 1;
    if (a.gatherClear != b.gatherClear)
        return a.gatherClear ? -1 : 1;
    return 0;
}

// Heroic run: a RunHeadings step judged by the spike's pursuit of the kiter along that heading and then
// round the ring, over ANUBARAK_KITE_PURSUIT_HORIZON_S for the passes and the plan horizon for the catch.
// Ranked by CompareLaneClass, then the latest catch (in bands), restart line clear, within the leash of a
// kite patch's stand, the widest pass by the patches, then by the gather. None clear of the patches: the
// widest patch margin alone. The held heading stays for ANUBARAK_KITE_HEADING_HOLD_MS, then while it
// ranks with the best, as long as its catch stays within the slack of the best and its spike passes no
// patch within the 11.5 yd test. None at all: the plain ring step.
Position LaneStepStand(Player* kiter, Creature* spike, std::vector<LaneObstacle> const& obstacles,
                       Position const* leash, LaneGoal const& goal, KitePlan* hold)
{
    Position const& from = kiter->GetPosition();
    Position const& spikePos = spike->GetPosition();
    AnubarakPursuit::Spike const chase = PursuitSpike(spike);

    std::vector<AnubarakPursuit::Point> points;
    std::vector<Position> tanks;
    bool anyGather = false;
    for (LaneObstacle const& obstacle : obstacles)
    {
        points.push_back(ToPoint(obstacle.pos));
        anyGather = anyGather || obstacle.gather;
        if (obstacle.tank)
            tanks.push_back(obstacle.pos);
    }

    float const cap = ANUBARAK_KITE_LANE_CAP - ANUBARAK_SPIKE_ROUTE_CLEARANCE;
    std::vector<LaneStep> steps;
    for (auto const& [dir, raw] : RunHeadings(kiter, spike))
    {
        AnubarakPursuit::KiterPath const path = KiterRun(kiter, spike, dir);
        AnubarakPursuit::Pursuit const run =
            AnubarakPursuit::Simulate(chase, path, ANUBARAK_KITE_PURSUIT_HORIZON_S, points, ANUBARAK_IMPALE_REACH);

        LaneStep step = {raw,  dir, true, true, true, true, true, cap, cap, Dist2d(raw, spikePos), 0,
                         0.0f, ANUBARAK_ROOM_RADIUS};
        for (size_t k = 0; k < obstacles.size(); ++k)
        {
            LaneObstacle const& obstacle = obstacles[k];
            // One already inside its clearance only has to be headed away from
            float const need = std::min(obstacle.clearance,
                                        Dist2d(spikePos, obstacle.pos) - ANUBARAK_KITE_LANE_START_SLACK);
            bool clear = run.spikeClosest[k] >= need;
            float const margin = run.spikeClosest[k] - obstacle.clearance;
            if (obstacle.gather)
            {
                step.gatherClear = step.gatherClear && clear;
                step.gatherScore = std::min(step.gatherScore, margin);
                continue;
            }

            // The kiter's own run kept out of the slow
            clear = clear && run.kiterClosest[k] >= std::min(ANUBARAK_TANK_KEEPOUT, Dist2d(from, obstacle.pos) -
                                                                                        ANUBARAK_KITE_LANE_START_SLACK);
            step.patchClear = step.patchClear && clear;
            step.patchScore = std::min(step.patchScore, margin);
            step.patchPass = std::min(step.patchPass, run.spikeClosest[k]);
        }

        step.catchS = RunCatchS(chase, path);
        if (goal.patch)
            step.goalOk = PatchLaterMargin(kiter, spike, goal.patchPos, dir) >= 0.0f;
        else if (goal.escort)
            step.goalOk = Dist2d(raw, goal.escortPos) <= ANUBARAK_KITE_ESCORT_LEASH;

        if (goal.away)
            step.awayBand = static_cast<int32>(Dist2d(raw, goal.gather) / ANUBARAK_KITE_GATHER_BAND);

        if (anyGather && !tanks.empty())
        {
            AnubarakPursuit::Point const ahead = path.At(ANUBARAK_KITE_LANE_HORIZON_S);
            Position const probe(ahead.x, ahead.y, from.GetPositionZ());
            for (LaneObstacle const& obstacle : obstacles)
                if (obstacle.gather)
                    for (Position const& tank : tanks)
                        if (SegmentDistance2d(probe, obstacle.pos, tank) < ANUBARAK_SPIKE_ROUTE_CLEARANCE)
                            step.restartClear = false;
        }

        step.leashed = !leash || Dist2d(raw, *leash) <= ANUBARAK_KITE_STAND_LEASH;
        steps.push_back(step);
    }

    std::sort(steps.begin(), steps.end(), [](LaneStep const& a, LaneStep const& b)
              {
                  if (int8 const order = CompareLaneClass(a, b))
                      return order < 0;
                  if (!a.patchClear)
                  {
                      if (a.patchScore != b.patchScore)
                          return a.patchScore > b.patchScore;
                      return a.fromSpike > b.fromSpike;
                  }
                  int32 const bandA = static_cast<int32>(a.catchS / ANUBARAK_KITE_CATCH_BAND_S);
                  int32 const bandB = static_cast<int32>(b.catchS / ANUBARAK_KITE_CATCH_BAND_S);
                  if (bandA != bandB)
                      return bandA > bandB;
                  if (a.restartClear != b.restartClear)
                      return a.restartClear;
                  if (a.leashed != b.leashed)
                      return a.leashed;
                  if (a.patchScore != b.patchScore)
                      return a.patchScore > b.patchScore;
                  if (a.gatherScore != b.gatherScore)
                      return a.gatherScore > b.gatherScore;
                  return a.fromSpike > b.fromSpike;
              });

    // Re-picking every tick flips between the two headings left at the rim, and the kiter stalls between
    // them
    uint32 const now = getMSTime();
    if (hold && hold->headingValid && !steps.empty())
    {
        Vec2 const held = {hold->heading.x, hold->heading.y};
        auto kept = steps.end();
        float nearest = ANUBARAK_KITE_LANE_KEEP_DEGREES * ANUBARAK_DEG_TO_RAD;
        for (auto it = steps.begin(); it != steps.end(); ++it)
        {
            float const angle = AngleBetween(held, it->dir);
            if (angle <= nearest)
            {
                nearest = angle;
                kept = it;
            }
        }

        LaneStep const& best = steps.front();
        bool const intact = kept != steps.end() && kept->patchPass >= ANUBARAK_SPIKE_PATCH_REACH &&
                            kept->catchS >= best.catchS - ANUBARAK_KITE_HEADING_CATCH_SLACK_S;
        bool const young = getMSTimeDiff(hold->headingMs, now) < ANUBARAK_KITE_HEADING_HOLD_MS;
        if (intact && kept != steps.begin() && (young || CompareLaneClass(best, *kept) == 0))
            std::rotate(steps.begin(), kept, kept + 1);
    }

    // Best first, so the floor raycasts stop at the first step that lands on the floor
    for (LaneStep const& step : steps)
    {
        Position valid;
        if (!OnFloor(kiter, step.raw, valid))
            continue;

        if (hold && (!hold->headingValid ||
                     AngleBetween({hold->heading.x, hold->heading.y}, step.dir) >
                         ANUBARAK_KITE_LANE_KEEP_DEGREES * ANUBARAK_DEG_TO_RAD))
        {
            hold->headingValid = true;
            hold->heading = {step.dir.x, step.dir.y};
            hold->headingMs = now;
        }

        return valid;
    }

    return RingStand(kiter, spike);
}

struct StandCandidate
{
    Creature* patch;
    Vec2 away;
    float rawDist;
    int8 rank;
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

// Patches the kite may end on, each with its stand past it off the spike, the nearest stand first
std::vector<StandCandidate> KiteCandidates(Player* kiter, Creature* spike, AnubarakState const& state,
                                           std::vector<Creature*> const& spheres)
{
    bool const heroic = AnubarakHeroic(kiter);
    bool const burrowers = AnyBurrower(kiter, state);
    std::vector<StandCandidate> candidates;
    for (Creature* sphere : spheres)
    {
        int8 const rank = KitePatchRank(kiter, state, sphere, heroic, burrowers);
        if (rank < 0)
            continue;

        Vec2 away = Delta(spike->GetPosition(), sphere->GetPosition());
        float const length = Length(away);
        // The spike is on it, so it's already being used up
        if (length < 0.5f)
            continue;

        away = {away.x / length, away.y / length};
        Position const raw = Offset(sphere->GetPosition(), away, ANUBARAK_KITE_STAND_OFFSET);
        candidates.push_back({sphere, away, kiter->GetExactDist2d(raw.GetPositionX(), raw.GetPositionY()), rank});
    }

    std::sort(candidates.begin(), candidates.end(),
              [](StandCandidate const& a, StandCandidate const& b) { return a.rawDist < b.rawDist; });
    return candidates;
}

// Rank first. Then heroic wants a route that spends no second tank patch on the way, and a walk clear of
// the gather goes before one that isn't; then the nearest stand. Under its own Hand of Protection the
// kiter takes a stand with the spike on it, since the spike can't hurt it.
bool PickKiteCandidate(Player* kiter, Creature* spike, AnubarakState const& state,
                       std::vector<StandCandidate> const& candidates, Creature*& chosenPatch, Position& chosenStand)
{
    Position const spikePos = spike->GetPosition();
    bool const heroic = AnubarakHeroic(kiter);
    bool const immune = UnderHandOfProtection(kiter);
    Position gather;
    bool const hasGather = GatherCentre(kiter, state, gather);

    std::vector<Position> tankSpots;
    for (uint8 side = 0; side < 2; ++side)
        if (Creature* patch = LatchedTankPatch(kiter, state, side))
            tankSpots.push_back(patch->GetPosition());

    chosenPatch = nullptr;
    // Floor checks are raycasts, so walk outward and stop once nothing further can beat the best.
    // routeClear: the spike's route there misses every other tank patch by the route clearance.
    auto const nearest = [&](int8 rank, bool routeClear, bool walkClear)
    {
        float bestDist = 0.0f;
        for (StandCandidate const& candidate : candidates)
        {
            if (candidate.rank != rank)
                continue;

            if (chosenPatch && candidate.rawDist > bestDist + ANUBARAK_KITE_ROTATION_SLACK)
                break;

            Position const raw = Offset(candidate.patch->GetPosition(), candidate.away, ANUBARAK_KITE_STAND_OFFSET);
            if (!immune && spike->GetExactDist2d(raw.GetPositionX(), raw.GetPositionY()) + ANUBARAK_KITE_ROTATION_SLACK <=
                               candidate.rawDist - ANUBARAK_KITE_ROTATION_SLACK)
                continue;

            Position stand;
            if (!FloorCheckedStand(kiter, candidate.patch->GetPosition(), candidate.away, stand) ||
                (!immune && !SpikeBehind(kiter, spike, stand)))
                continue;

            if (walkClear &&
                SegmentDistance2d(kiter->GetPosition(), stand, gather) < ANUBARAK_KITE_GATHER_WALK_CLEARANCE)
                continue;

            if (routeClear)
            {
                std::vector<Position> others;
                for (Position const& tank : tankSpots)
                    if (Dist2d(tank, candidate.patch->GetPosition()) > 0.5f)
                        others.push_back(tank);

                if (SpikeRouteNearPatch(spikePos, kiter->GetPosition(), stand, others, ANUBARAK_SPIKE_ROUTE_CLEARANCE))
                    continue;
            }

            float const dist = kiter->GetExactDist2d(stand.GetPositionX(), stand.GetPositionY());
            if (!chosenPatch || dist < bestDist)
            {
                chosenPatch = candidate.patch;
                chosenStand = stand;
                bestDist = dist;
            }
        }
    };

    // {route clear, walk clear}
    static bool const passes[][2] = {{true, true}, {true, false}, {false, true}, {false, false}};
    for (int8 rank = 0; rank < 2 && !chosenPatch; ++rank)
        for (auto const& pass : passes)
        {
            if (chosenPatch)
                break;

            if ((pass[0] && !heroic) || (pass[1] && !hasGather))
                continue;

            nearest(rank, pass[0], pass[1]);
        }

    return chosenPatch != nullptr;
}

bool ChooseKitePatch(Player* kiter, Creature* spike, AnubarakState const& state, std::vector<Creature*> const& spheres,
                     Creature*& patch, Position& stand)
{
    return PickKiteCandidate(kiter, spike, state, KiteCandidates(kiter, spike, state, spheres), patch, stand);
}

// seekPatch false runs the ring without looking for a patch: heroic until the kite stage says patch.
// lane is what heroic's run keeps the spike off, leash the patch stand its steps stay near, goal and
// hold the kite plan's.
void ComputeKiteStand(Player* kiter, Creature* spike, AnubarakState const& state, std::vector<Creature*> const& spheres,
                      std::vector<LaneObstacle> const& lane, AnubarakKiteStand const* previous, bool seekPatch,
                      Position const* leash, LaneGoal const& goal, KitePlan* hold, AnubarakKiteStand& out)
{
    bool const heroic = AnubarakHeroic(kiter);
    bool const immune = UnderHandOfProtection(kiter);
    std::vector<StandCandidate> const candidates =
        seekPatch ? KiteCandidates(kiter, spike, state, spheres) : std::vector<StandCandidate>();

    Creature* chosenPatch = nullptr;
    Position chosenStand;

    if (previous && !previous->patch.IsEmpty())
    {
        auto const held = std::find_if(candidates.begin(), candidates.end(), [previous](StandCandidate const& c)
                                       { return c.patch->GetGUID() == previous->patch; });
        Position fresh;
        if (held != candidates.end() && FloorCheckedStand(kiter, held->patch->GetPosition(), held->away, fresh) &&
            (immune || SpikeBehind(kiter, spike, fresh)))
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
                     (immune || SpikeBehind(kiter, spike, previous->stand)))
            {
                chosenPatch = held->patch;
                chosenStand = previous->stand;
            }
        }
    }

    if (!chosenPatch)
        PickKiteCandidate(kiter, spike, state, candidates, chosenPatch, chosenStand);

    if (!chosenPatch)
    {
        out.stand = heroic ? LaneStepStand(kiter, spike, lane, leash, goal, hold) : RingStand(kiter, spike);
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
char const* const KITE_LABEL = "kite";

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
// any burrower is up, the other side's DK side tank. Only one whose grip is back within the wait. A far
// grip walks the DK well off the side, so the one with no tank duty goes first and the side's own tank
// only without one.
Player* PickGripper(Player* bot, AnubarakState const& state, uint8 side, ObjectGuid const& busy, bool far = false)
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
    if (eligible(own) && !far)
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

    if (far)
        return eligible(own) ? own : nullptr;

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
    float const step = std::clamp(length - flat + ANUBARAK_GRIP_REACH_MARGIN, 0.0f, ANUBARAK_GRIP_STEP_OUT);
    return ValidateFloorPoint(bot, Offset(spot, toward, step));
}

// For a sphere hovering past the step-out: on the floor just inside Death Grip's reach of it, on its side
// nearest the spot, turned round it until on the floor and off the spike and its lane
bool FarGripStand(Player* dk, Creature* sphere, Position const& spot, Position& out)
{
    Vec2 toward = Delta(sphere->GetPosition(), spot);
    float const length = Length(toward);
    float const height = sphere->GetPositionZ() - spot.GetPositionZ();
    float const flat =
        std::sqrt(std::max(ANUBARAK_GRIP_RANGE * ANUBARAK_GRIP_RANGE - height * height, 0.0f)) - ANUBARAK_GRIP_REACH_MARGIN;
    if (length < 1.0f || flat < 1.0f)
        return false;

    toward = {toward.x / length, toward.y / length};
    Creature* spike = GetPursuingSpike(dk);
    Player* kiter = spike ? GetSpikeTarget(dk) : nullptr;
    Position const base(sphere->GetPositionX(), sphere->GetPositionY(), spot.GetPositionZ());
    static float const turns[] = {0.0f, 20.0f, -20.0f, 40.0f, -40.0f};
    for (float degrees : turns)
    {
        Position const raw = Offset(base, Rotate(toward, degrees * ANUBARAK_DEG_TO_RAD), flat);
        if (spike && (Dist2d(raw, spike->GetPosition()) < ANUBARAK_SPIKE_DANGER_RADIUS_FAST ||
                      (kiter && SegmentDistance2d(spike->GetPosition(), kiter->GetPosition(), raw) <
                                    ANUBARAK_SPIKE_LANE_CLEARANCE_FAST)))
            continue;

        Position valid;
        if (OnFloor(dk, raw, valid) && sphere->GetExactDist(valid.GetPositionX(), valid.GetPositionY(),
                                                            valid.GetPositionZ()) <= ANUBARAK_GRIP_RANGE)
        {
            out = valid;
            return true;
        }
    }

    return false;
}

// Pulled down by the grip: a flying sphere hovers about 13 yd up
bool GrippedDown(Creature* sphere, GripCast const& cast)
{
    return sphere->GetPositionZ() - cast.from.GetPositionZ() <= ANUBARAK_GRIP_LANDED_HEIGHT;
}

// True while a grip has a sphere for the side. The sphere it pulls is reserved, and the side's shooters
// join once it is down where the DK cast from, or anywhere it still makes the side's patch. One not
// landed 3 s on leaves the side to its shooters until a DK's Death Grip is back. With none in reach the
// DK keeps its duty and the side's shooters take any sphere they can. No grip takes a sphere already
// near the other side's spot, or one another side's DK has picked.
template <typename Taken>
bool ServeSideByGrip(Player* bot, AnubarakState& state, uint8 side, Position const& spot, Position const& otherSpot,
                     std::vector<Creature*> const& spheres, std::vector<Creature*> const& flying,
                     std::vector<GripDuty> const& previousGrips, Taken const& taken,
                     std::vector<std::pair<char const*, Creature*>>& needs, std::vector<Creature*>& reserved,
                     uint32 now)
{
    SideNeed& need = state.needs[side];
    GripCast& cast = state.grips[side];
    if (cast.valid)
    {
        Creature* sphere = FindSphere(spheres, cast.sphere);
        Player* dk = ObjectAccessor::GetPlayer(*bot, cast.dk);
        if (IsFrostSphereFlying(sphere) && dk && dk->IsAlive())
        {
            Creature* otherPatch = LatchedTankPatch(bot, state, 1 - side);
            Position const other = otherPatch ? otherPatch->GetPosition() : otherSpot;
            Position const at = sphere->GetPosition();
            if (GrippedDown(sphere, cast) &&
                (Dist2d(at, cast.from) <= ANUBARAK_GRIP_LANDED ||
                 (Dist2d(at, spot) <= ANUBARAK_SIDE_SHOOT_FAR && Dist2d(at, other) >= ANUBARAK_TANK_PATCH_GAP)))
                cast.landed = true;

            if (cast.landed || getMSTimeDiff(cast.castMs, now) < ANUBARAK_GRIP_JUDGE_MS)
            {
                state.gripDuties.push_back({cast.dk, sphere->GetGUID(), side, cast.from});
                if (cast.landed)
                {
                    state.grippedFor[sphere->GetGUID()] = side;
                    if (Dist2d(cast.from, spot) > ANUBARAK_GRIP_STEP_OUT + ANUBARAK_TANK_PATCH_ARRIVE)
                        state.farGripped.insert(sphere->GetGUID());

                    needs.emplace_back(SIDE_LABELS[side], sphere);
                }
                else
                    reserved.push_back(sphere);

                return true;
            }

            need.gripFailed = true;
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

    // Every sphere left hovers past the step-out, where a shot one lands too far off to serve the side:
    // the DK walks out to the nearest and its patch becomes the side's
    if (!sphere)
    {
        Creature* distant = GripSphere(previousGrips, flying, spot, side, ANUBARAK_ROOM_RADIUS, blocked);
        Player* farDk = distant ? PickGripper(bot, state, side, busy, true) : nullptr;
        Position farStand;
        if (farDk && FarGripStand(farDk, distant, spot, farStand))
        {
            dk = farDk;
            sphere = distant;
            stand = farStand;
        }
    }

    state.gripUsed = true;
    state.gripDuties.push_back({dk->GetGUID(), sphere ? sphere->GetGUID() : ObjectGuid::Empty, side, stand});
    if (!sphere)
        return false;

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

        Position const spots[2] = {SideSpot(state, spheres, tankPatches, 0),
                                   SideSpot(state, spheres, tankPatches, 1)};
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

    bool const gripWaits = state.needs[0].gripFailed || state.needs[1].gripFailed;
    state.grip = static_cast<uint32>(gripWaits        ? GripMode::Failed
                                     : state.gripUsed ? GripMode::Active
                                                      : GripMode::None);

    // Heroic makes no kite patch: its kite spends a tank patch, the side refilled after the emerge. A
    // sphere already falling near the kiter lands as a patch in 1.5 s, so don't spend another.
    Player* target = GetSpikeTarget(bot);
    Creature* spike = ResolveCreature(bot, state.spikeGuid);
    if (!heroic && target && spike && spike->IsAlive() && !FallingNear(spheres, target->GetPosition()))
    {
        AnubarakKiteStand stand;
        ComputeKiteStand(target, spike, state, spheres, LaneObstacles(target, state), nullptr, true, nullptr,
                         LaneGoal(), nullptr, stand);
        if (stand.branch == AnubarakKiteBranch::Ring)
        {
            std::vector<Creature*> open;
            for (Creature* sphere : flying)
                if (!taken(sphere))
                    open.push_back(sphere);

            if (Creature* sphere = KiteSphere(open, target, spike))
                needs.emplace_back(KITE_LABEL, sphere);
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

// Paladin bots alive and unmarked in the order a kiter's Hand of Protection comes from: neither tank nor
// healer, then a side tank with no burrower to hold or take, then a healer, then any other tank; the
// lowest guid first within each
std::vector<Player*> ProtectorOrder(Player* bot)
{
    std::vector<std::pair<uint8, Player*>> ranked;
    ForEachGroupMember(bot, [&](Player* member)
    {
        PlayerbotAI* ai = member->IsAlive() && member->getClass() == CLASS_PALADIN ? GET_PLAYERBOT_AI(member) : nullptr;
        if (!ai || member->HasAura(SPELL_MARK))
            return false;

        bool const tank = IsAnubarakTankPlayer(member);
        uint8 rank = 3;
        if (!tank)
            rank = PlayerbotAI::IsHeal(member) ? 2 : 0;
        else if (GetAnubarakBurrowerTankSide(member) >= 0 && !GetAnubarakBurrowerPick(ai))
            rank = 1;

        ranked.emplace_back(rank, member);
        return false;
    });

    std::sort(ranked.begin(), ranked.end(), [](auto const& a, auto const& b)
              { return a.first != b.first ? a.first < b.first : a.second->GetGUID() < b.second->GetGUID(); });

    std::vector<Player*> order;
    for (auto const& entry : ranked)
        order.push_back(entry.second);

    return order;
}

// Once per instance per ms. The claim stands for the mark window: one paladin covers the kiter, and
// passes it on only when its Hand of Protection hasn't landed 1.5 s in. Heroic follows the kite plan:
// its hand 1 s before the catch, the planned paladin first, and any paladin in reach then once the plan
// has failed, with no patch walk that beats the catch (or the spike within the immunity range anyway)
// and no Hand or immunity of the kiter's own planned. The kiter's own goes 0.6 s before the catch, its
// Hand 1 s before.
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
    claim.spikeFaster = false;
    claim.standReachable = false;
    claim.heroic = AnubarakHeroic(bot);
    claim.selfDue = false;

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
    bool const forbearance = kiter->HasAura(SPELL_FORBEARANCE);
    float impaleMs = 0.0f;
    bool const caught = ImpaleInMs(kiter, spike, impaleMs);
    bool const handDue = caught && impaleMs <= ANUBARAK_KITE_HAND_LEAD_S * IN_MILLISECONDS;
    claim.spikeInImmunityRange = spikeDist <= ANUBARAK_KITE_IMMUNITY_RANGE;
    claim.spikeFaster = caught;

    ObjectGuid planned;
    bool selfHand = false;
    bool ownPlanned = false;
    if (claim.heroic)
    {
        KitePlan const& plan = JudgeKitePlan(kiter, spike, *state);
        bool const handKite = HandKite(plan);
        selfHand = handKite && plan.hand == kiterGuid;
        ownPlanned = plan.terminal == AnubarakPursuit::KiteTerminal::Own;
        // A spike within the immunity range has beaten whatever walk the model gave the kiter
        claim.standReachable = plan.patch && plan.patchMarginS >= 0.0f && !claim.spikeInImmunityRange;
        claim.need = !immune && !forbearance && handDue && !selfHand &&
                     (handKite || (!claim.standReachable && !ownPlanned));
        if (handKite && !selfHand)
            planned = plan.hand;
    }
    else
    {
        Position stand;
        claim.standReachable = KiteWalkStand(kiter, spike, *state, stand) && (!caught || WalkMs(kiter, stand) <= impaleMs);
        claim.need = !immune && caught && !forbearance && spikeDist <= ANUBARAK_KITE_PROTECT_RANGE;
    }

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

    if (claim.need && claim.protector.IsEmpty())
    {
        // Range and sight asked here, the cast itself waits out the GCD
        auto const serves = [&](Player* paladin)
        {
            return paladin && paladin != kiter && paladin->IsAlive() && GET_PLAYERBOT_AI(paladin) &&
                   std::find(claim.passed.begin(), claim.passed.end(), paladin->GetGUID()) == claim.passed.end() &&
                   paladin->IsWithinDistInMap(kiter, ANUBARAK_HOP_RANGE) && paladin->IsWithinLOSInMap(kiter) &&
                   HandReady(paladin);
        };

        Player* pick = planned.IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(*bot, planned);
        if (!serves(pick))
        {
            pick = nullptr;
            for (Player* paladin : ProtectorOrder(bot))
                if (serves(paladin))
                {
                    pick = paladin;
                    break;
                }
        }

        if (pick)
        {
            claim.protector = pick->GetGUID();
            claim.claimMs = now;
        }
    }

    if (claim.heroic && !immune)
    {
        bool const ownDue = caught && impaleMs <= ANUBARAK_KITE_OWN_LEAD_S * IN_MILLISECONDS;
        claim.selfDue = (selfHand && handDue && !forbearance) ||
                        (ownDue && (ownPlanned || (claim.protector.IsEmpty() && !claim.standReachable)));
    }

    return &claim;
}

char const* ProtectRole(Player* bot, ProtectClaim const& claim)
{
    if (claim.need && claim.protector == bot->GetGUID())
        return "hop";

    if (claim.kiter != bot->GetGUID() || AnubarakKiterImmune(bot))
        return "none";

    if (claim.heroic)
        return claim.selfDue ? "self" : "none";

    return claim.spikeInImmunityRange && claim.spikeFaster && claim.protector.IsEmpty() && !claim.standReachable
               ? "self"
               : "none";
}

// Alive, unmarked, and not holding him while he's up, else null
Player* UsableSideTank(Player* bot, AnubarakState const& state, uint8 side)
{
    Player* tank = state.sideTanks[side].IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(*bot, state.sideTanks[side]);
    if (!tank || !tank->IsAlive() || tank->HasAura(SPELL_MARK))
        return nullptr;

    bool const holdsHim = state.phaseValue != AnubarakPhase::Submerged && tank->GetGUID() == PickupGuid(bot, state);
    return holdsHim ? nullptr : tank;
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

    bool usable[2];
    bool human[2];
    for (uint8 side = 0; side < 2; ++side)
    {
        Player* tank = UsableSideTank(bot, state, side);
        usable[side] = tank != nullptr;
        human[side] = tank && !GET_PLAYERBOT_AI(tank);
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

// Up and off Permafrost inside its free time: the ms of it left, else -1
int32 FreeMsLeft(AnubarakState const& state, Creature* burrower, uint32 now)
{
    auto const exit = state.exits.find(burrower->GetGUID());
    if (exit == state.exits.end() || exit->second.down || burrower->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) ||
        UnitOnPermafrost(burrower))
        return -1;

    uint32 const up = getMSTimeDiff(exit->second.upMs, now);
    return up < ANUBARAK_BURROWER_FREE_MS ? static_cast<int32>(ANUBARAK_BURROWER_FREE_MS - up) : -1;
}

// A tank has it, or nobody will: before that it is still walking in from its exit
bool BurrowerSettled(Player* bot, AnubarakState& state, Creature* burrower)
{
    Unit* victim = burrower->GetVictim();
    Player* holder = victim ? victim->ToPlayer() : nullptr;
    if (holder && IsAnubarakTankPlayer(holder) && burrower->GetExactDist2d(holder) <= ANUBARAK_FOCUS_SETTLE)
        return true;

    std::unordered_map<ObjectGuid, uint8> const& owners = BurrowerOwners(bot, state);
    auto const owner = owners.find(burrower->GetGUID());
    return owner != owners.end() && !UsableSideTank(bot, state, owner->second);
}

bool IsFocusableBurrower(Player* bot, AnubarakState& state, Creature* burrower, uint32 now)
{
    if (!burrower || !burrower->IsAlive() || burrower->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
        return false;

    if (burrower->GetHealthPct() < ANUBARAK_FOCUS_OFF_FROST_PCT)
        return true;

    return BurrowerSettled(bot, state, burrower) &&
           (UnitOnPermafrost(burrower) || FreeMsLeft(state, burrower, now) >= 0);
}

// The living DPS bots of each role by guid, side 0 and 1 in turn, split again at each wave so a death
// rebalances at the next one. -1 for anyone outside the split: a human, a bot that joined since.
int8 DpsSide(Player* bot, AnubarakState& state)
{
    uint32 const wave = state.wave.Get();
    if (!state.dpsSideValid || state.dpsSideWave != wave)
    {
        state.dpsSideValid = true;
        state.dpsSideWave = wave;

        std::vector<Player*> melee;
        std::vector<Player*> ranged;
        ForEachGroupMember(bot, [&](Player* member)
        {
            if (member->IsAlive() && GET_PLAYERBOT_AI(member) && IsToCDpsBot(member))
                (PlayerbotAI::IsMelee(member) ? melee : ranged).push_back(member);

            return false;
        });

        std::unordered_map<ObjectGuid, uint32> sides;
        for (std::vector<Player*>* role : {&melee, &ranged})
        {
            std::sort(role->begin(), role->end(), [](Player* a, Player* b) { return a->GetGUID() < b->GetGUID(); });
            for (size_t i = 0; i < role->size(); ++i)
                sides[(*role)[i]->GetGUID()] = static_cast<uint32>(i % 2);
        }

        for (auto it = state.dpsSide.begin(); it != state.dpsSide.end();)
            it = sides.count(it->first) ? std::next(it) : state.dpsSide.erase(it);

        for (auto const& [guid, side] : sides)
            state.dpsSide.Set(guid, side);
    }

    auto const it = state.dpsSide.find(bot->GetGUID());
    return it == state.dpsSide.end() ? -1 : static_cast<int8>(it->second);
}

// The side's burrower DPS focus, latched per side over the burrowers BurrowerOwners gives it: one off
// Permafrost in its free time first, the one whose time ends first, then the lowest health. The latched
// one stays while it qualifies and none of those outranks it.
Creature* SideFocus(Player* bot, AnubarakState& state, uint8 side)
{
    uint32 const now = getMSTime();
    std::unordered_map<ObjectGuid, uint8> const& owners = BurrowerOwners(bot, state);
    auto const onSide = [&](Creature* burrower)
    {
        auto const owner = burrower ? owners.find(burrower->GetGUID()) : owners.end();
        return owner != owners.end() && owner->second == side;
    };

    Creature* best = nullptr;
    int32 bestFree = -1;
    for (Creature* burrower : ResolveAlive(bot, state.burrowers))
    {
        if (!onSide(burrower) || !IsFocusableBurrower(bot, state, burrower, now))
            continue;

        // Guid breaks a tie, so the pick never hangs on the sweep's order
        int32 const free = FreeMsLeft(state, burrower, now);
        float const pct = burrower->GetHealthPct();
        bool better = !best;
        if (best && (free >= 0) != (bestFree >= 0))
            better = free >= 0;
        else if (best && free >= 0 && free != bestFree)
            better = free < bestFree;
        else if (best)
            better = pct < best->GetHealthPct() ||
                     (!(best->GetHealthPct() < pct) && burrower->GetGUID() < best->GetGUID());

        if (better)
        {
            best = burrower;
            bestFree = free;
        }
    }

    RaidObs::ObsValue<ObjectGuid>& latch = side == 0 ? state.focus0 : state.focus1;
    Creature* latched = ResolveCreature(bot, latch.Get());
    if (onSide(latched) && IsFocusableBurrower(bot, state, latched, now) &&
        (bestFree < 0 || FreeMsLeft(state, latched, now) >= 0))
        return latched;

    latch = best ? best->GetGUID() : ObjectGuid::Empty;
    return best;
}

// A burrower on someone other than a tank within this of a melee bot is its target, whatever its side
constexpr float ANUBARAK_MELEE_LOOSE_BURROWER = 10.0f;
// A burrower stand counts as in his melee reach this far inside it, since he turns and steps
constexpr float ANUBARAK_MELEE_BOSS_REACH_SLACK = 1.0f;
// A melee bot this far off its held burrower stand walks back to it
constexpr float ANUBARAK_MELEE_STAND_ARRIVE = 1.5f;
// A held stand is picked again once its burrower moved this far from where it stood at the pick
constexpr float ANUBARAK_MELEE_STAND_DRIFT = 1.5f;

Creature* LooseBurrowerNear(Player* bot, AnubarakState const& state)
{
    Creature* nearest = nullptr;
    float nearestDist = ANUBARAK_MELEE_LOOSE_BURROWER;
    for (Creature* burrower : ResolveAlive(bot, state.burrowers))
    {
        Unit* victim = burrower->GetVictim();
        Player* player = victim ? victim->ToPlayer() : nullptr;
        if (!player || IsAnubarakTankPlayer(player) || burrower->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            continue;

        float const dist = bot->GetExactDist2d(burrower);
        if (dist <= nearestDist)
        {
            nearest = burrower;
            nearestDist = dist;
        }
    }

    return nearest;
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

// A melee bot anywhere in reach of a burrower this far from every patch is out of the slow
bool BurrowerNearPatch(Creature* burrower, std::vector<Position> const& patches)
{
    return std::any_of(patches.begin(), patches.end(), [burrower](Position const& patch)
                       {
                           return Dist2d(burrower->GetPosition(), patch) <
                                  ANUBARAK_MELEE_PATCH_CLEARANCE + ANUBARAK_MELEE_BURROWER_REACH;
                       });
}

// Points ANUBARAK_MELEE_BURROWER_REACH round the burrower, straight out from its nearest patch and turned
// 20 deg at a time up to 100, clear of the patches. One in his melee reach behind him first, then one in
// his reach, the nearest the bot of those, so a switch to him needs no walk; else the one nearest him.
// floorChecked asks the mesh too.
bool MeleeBurrowerStand(Player* bot, Creature* burrower, std::vector<Position> const& patches, bool floorChecked,
                        Position& out)
{
    Creature* boss = GetAnubarak(bot);
    if (boss && boss->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
        boss = nullptr;

    Position const at = burrower->GetPosition();
    Position const* nearest = nullptr;
    for (Position const& patch : patches)
        if (!nearest || Dist2d(at, patch) < Dist2d(at, *nearest))
            nearest = &patch;

    if (!nearest)
        return false;

    Vec2 away = Delta(*nearest, at);
    float const length = Length(away);
    away = length > 0.1f ? Vec2{away.x / length, away.y / length} : Vec2{1.0f, 0.0f};

    auto const clear = [&](Position const& point)
    {
        return Dist2d(point, at) <= ANUBARAK_MELEE_BURROWER_REACH + 0.5f &&
               std::none_of(patches.begin(), patches.end(), [&point](Position const& patch)
                            { return Dist2d(point, patch) < ANUBARAK_MELEE_PATCH_CLEARANCE; });
    };

    float const bossReach = boss ? bot->GetMeleeRange(boss) - ANUBARAK_MELEE_BOSS_REACH_SLACK : 0.0f;
    struct Candidate
    {
        uint8 tier;
        float dist;
        Position pos;
    };

    std::vector<Candidate> candidates;
    static float const turns[] = {0.0f, 20.0f, -20.0f, 40.0f, -40.0f, 60.0f, -60.0f, 80.0f, -80.0f, 100.0f, -100.0f};
    for (float degrees : turns)
    {
        Position const raw = Offset(at, Rotate(away, degrees * ANUBARAK_DEG_TO_RAD), ANUBARAK_MELEE_BURROWER_REACH);
        if (!clear(raw))
            continue;

        uint8 tier = 2;
        float dist = boss ? Dist2d(boss->GetPosition(), raw) : Dist2d(bot->GetPosition(), raw);
        if (boss && dist <= bossReach)
        {
            tier = boss->HasInArc(static_cast<float>(M_PI), &raw) ? 1 : 0;
            dist = Dist2d(bot->GetPosition(), raw);
        }

        candidates.push_back({tier, dist, raw});
    }

    std::sort(candidates.begin(), candidates.end(), [](Candidate const& a, Candidate const& b)
              { return a.tier != b.tier ? a.tier < b.tier : a.dist < b.dist; });

    for (Candidate const& candidate : candidates)
    {
        Position valid = candidate.pos;
        if (floorChecked && (!OnFloor(bot, candidate.pos, valid) || !clear(valid)))
            continue;

        out = valid;
        return true;
    }

    return false;
}

bool MeleeReachesBurrower(Player* bot, AnubarakState const& state, Creature* burrower)
{
    std::vector<Position> const patches = PatchPositions(bot, state);
    Position stand;
    return !BurrowerNearPatch(burrower, patches) || MeleeBurrowerStand(bot, burrower, patches, false, stand);
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

bool AnubarakSpikeReachesFirst(Player* bot, Position const& spot)
{
    Creature* spike = bot ? GetPursuingSpike(bot) : nullptr;
    float const spikeSpeed = spike ? spike->GetSpeed(MOVE_RUN) : 0.0f;
    float const botSpeed = bot ? bot->GetSpeed(MOVE_RUN) : 0.0f;
    if (spikeSpeed <= 0.0f || botSpeed <= 0.0f)
        return false;

    float const walkS = bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) / botSpeed;
    return Dist2d(spike->GetPosition(), spot) / spikeSpeed <= walkS + ANUBARAK_SPIKE_WALK_MARGIN_S;
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
    return SideSpot(*state, ResolveSpheres(bot, *state), held, side);
}

Position GetAnubarakSideTankHold(Player* bot, uint8 side)
{
    Creature* patch = GetAnubarakTankPatch(bot, side);
    if (!patch)
        return GetAnubarakTankSpot(bot, side);

    Position const centre = patch->GetPosition();
    for (Creature* burrower : GetAnubarakBurrowers(bot))
        if (burrower->GetVictim() == bot && !burrower->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) &&
            !UnitOnPermafrost(burrower))
            return centre;

    // Not floor-checked: read every tick through the burrower pick, and it lies between the patch and the
    // anchor, both on the open floor
    Vec2 const toward = Delta(centre, GetAnubarakBossAnchor(bot));
    float const length = Length(toward);
    if (length < ANUBARAK_SIDE_TANK_RIM)
        return centre;

    return Offset(centre, {toward.x / length, toward.y / length}, ANUBARAK_SIDE_TANK_RIM);
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
    AnubarakState* state = boss ? ReadPhase(GET_PLAYERBOT_AI(bot)) : nullptr;
    if (!state || !GetAnubarakDragAnchor(bot, anchor) || !AnubarakSideSpots(bot, spot0, spot1))
        return points;

    points.push_back(anchor);

    // Patches that landed close together leave no gap on the line between them, so a leg refused at the
    // anchor can still end short of it on his side
    Vec2 along = Delta(spot0, spot1);
    float const length = Length(along);
    if (length >= 1.0f)
    {
        Vec2 normal = {-along.y / length, along.x / length};
        Vec2 const toBoss = Delta(anchor, boss->GetPosition());
        if (normal.x * toBoss.x + normal.y * toBoss.y < 0.0f)
            normal = {-normal.x, -normal.y};

        for (float offset : {4.0f, 7.5f})
            points.push_back(Offset(anchor, normal, offset));
    }

    // He comes up where the spike was, which can be past a patch on the line through both, and every leg
    // from there walks the tank through it
    std::vector<AnubarakPursuit::Point> patches;
    for (Position const& patch : PatchPositions(bot, *state))
        patches.push_back(ToPoint(patch));

    auto const add = [&](std::vector<AnubarakPursuit::Point> const& extra)
    {
        for (AnubarakPursuit::Point const& point : extra)
        {
            Position const pos(point.x, point.y, anchor.GetPositionZ());
            if (Dist2d(pos, ANUBARAK_ROOM_CENTER) <= ANUBARAK_SUBMERGE_SPOT_FLOOR)
                points.push_back(pos);
        }
    };

    AnubarakPursuit::Point const from = ToPoint(boss->GetPosition());
    add(AnubarakPursuit::DragDetours(from, ToPoint(anchor), patches, ANUBARAK_DRAG_DETOUR_TRIGGER,
                                     ANUBARAK_DRAG_DETOUR_STANDOFF, ANUBARAK_TANK_KEEPOUT));

    uint32 const now = getMSTime();
    if (!state->dragFreeValid || state->drag.valid || Dist2d(boss->GetPosition(), anchor) <= ANUBARAK_DRAG_STALL_RANGE)
    {
        state->dragFreeValid = true;
        state->dragFreeMs = now;
    }
    else if (getMSTimeDiff(state->dragFreeMs, now) >= ANUBARAK_DRAG_STALL_MS)
        add(AnubarakPursuit::RingPoints(ToPoint(anchor), ANUBARAK_DRAG_STALL_RANGE, from, patches,
                                        ANUBARAK_TANK_KEEPOUT));

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
        // navprobe-clean out to 45 yd, so he never ends up against a wall
        auto const onFloor = [](float x, float y)
        { return ANUBARAK_ROOM_CENTER.GetExactDist2d(x, y) <= ANUBARAK_SUBMERGE_SPOT_FLOOR; };

        // The spike is summoned on him, so it starts away from the raid and its run to the raid spends no
        // patch. Dropped when nothing passes.
        Position const raid = state->raidPoint;
        auto const offRaid = [&](float x, float y)
        {
            Position const spot(x, y, raid.GetPositionZ());
            return onFloor(x, y) && Dist2d(spot, raid) >= ANUBARAK_SUBMERGE_RAID_CLEARANCE &&
                   std::none_of(patches.begin(), patches.end(), [&](Position const& patch)
                                { return SegmentDistance2d(spot, raid, patch) < ANUBARAK_SPIKE_ROUTE_CLEARANCE; });
        };

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
        for (bool const awayFromRaid : {true, false})
        {
            if (state->submergeSpotValid || (awayFromRaid && !state->raidPointValid))
                continue;

            for (float clearance : ANUBARAK_SUBMERGE_CLEARANCES)
            {
                std::vector<HazardCircle> hazards;
                for (Position const& patch : patches)
                    hazards.emplace_back(patch, clearance);

                std::function<bool(float, float)> const accept =
                    awayFromRaid ? std::function<bool(float, float)>(offRaid) : std::function<bool(float, float)>(onFloor);
                Position const spot = FindNearestPositionClearOfHazards(bot, hazards, ANUBARAK_SUBMERGE_SPOT_SEARCH,
                                                                        2.0f, static_cast<float>(M_PI) / 8.0f, &prefer,
                                                                        accept, &cache);
                if (spot == Position())
                    continue;

                state->submergeSpotValid = true;
                state->submergeSpot = spot;
                state->submergeSpotClearance = clearance;
                break;
            }
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
           (bot->GetAuraCount(sSpellMgr->GetSpellIdForDifficulty(SPELL_EXPOSE_WEAKNESS, bot)) >=
                ANUBARAK_SIDE_DEFENSIVE_STACKS ||
            bot->GetHealthPct() < ANUBARAK_SIDE_DEFENSIVE_PCT);
}

Player* GetAnubarakSideTankToRes(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    AnubarakState const* state = bot ? ReadPhase(botAI) : nullptr;
    if (!state || state->deadSideTanks.empty() || !RebirthReady(bot) || bot->HasAura(SPELL_MARK))
        return nullptr;

    // One druid per corpse: the lowest guid with Rebirth ready, a tank only when no other druid can. Never
    // the one holding him: the cast takes it out of bear form.
    bool const surfaced = state->phaseValue == AnubarakPhase::Surface || state->phaseValue == AnubarakPhase::Swarm;
    ObjectGuid const pickup = surfaced ? PickupGuid(bot, *state) : ObjectGuid::Empty;
    Player* first = nullptr;
    bool firstTank = false;
    ForEachGroupMember(bot, [&](Player* member)
    {
        if (!member->IsAlive() || !GET_PLAYERBOT_AI(member) || member->HasAura(SPELL_MARK) || !RebirthReady(member) ||
            member->GetGUID() == pickup)
            return false;

        bool const tank = IsAnubarakTankPlayer(member);
        if (!first || (firstTank && !tank) || (firstTank == tank && member->GetGUID() < first->GetGUID()))
        {
            first = member;
            firstTank = tank;
        }

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
    pick.hold = GetAnubarakSideTankHold(bot, pick.side);

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
    AnubarakState* state = bot ? ReadPhase(GET_PLAYERBOT_AI(bot)) : nullptr;
    if (!state)
        return nullptr;

    Creature* const focus[2] = {SideFocus(bot, *state, 0), SideFocus(bot, *state, 1)};
    int8 const side = IsToCDpsBot(bot) ? DpsSide(bot, *state) : -1;
    if (side >= 0)
        return focus[side];

    return focus[0] ? focus[0] : focus[1];
}

Unit* GetAnubarakDpsTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!IsToCDpsBot(bot) || !AnubarakEngaged(botAI))
        return nullptr;

    // Read first: a new pull's first read clears the focus latches
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

    // Read even under a skull, so both focus latches stay current. A melee bot that can't hit its side's
    // focus from outside the patches stays on him.
    Creature* const focus[2] = {SideFocus(bot, *state, 0), SideFocus(bot, *state, 1)};
    int8 const side = DpsSide(bot, *state);
    bool const melee = PlayerbotAI::IsMelee(bot);
    Creature* own = side >= 0 ? focus[side] : (focus[0] ? focus[0] : focus[1]);
    Creature* other = side >= 0 ? focus[1 - side] : nullptr;
    if (own && melee && !MeleeReachesBurrower(bot, *state, own))
        own = nullptr;

    if (Unit* skull = GetToCHumanSkull(botAI, &IsAnubarakSkullTarget))
        return NoteDpsPick(bot, skull, "skull");

    // Gathered melee hold their place, so they take the scarabs that come to the raid and leave the
    // burrowers on the patches to the ranged
    if (state->phaseValue == AnubarakPhase::Submerged && melee && AnubarakGathers(botAI))
        if (Creature* scarab = NearestDpsScarab(bot, *state, ANUBARAK_GATHER_MELEE_SCARAB))
            return NoteDpsPick(bot, scarab, "scarab");

    if (melee)
        if (Creature* loose = LooseBurrowerNear(bot, *state))
            return NoteDpsPick(bot, loose, "loose");

    if (own)
        return NoteDpsPick(bot, own, "burrower");

    // A scarab left over from phase 2 goes before him: its Acid-Drenched Mandibles stacks last a minute
    // and every hit refreshes them. Melee take only one close by, so none walks off him across the room.
    bool const surfaced = state->phaseValue == AnubarakPhase::Surface || state->phaseValue == AnubarakPhase::Swarm;
    if (surfaced)
        if (Creature* scarab = melee ? NearestDpsScarab(bot, *state, ANUBARAK_GATHER_MELEE_SCARAB)
                                     : NearestDpsScarab(bot, *state))
            return NoteDpsPick(bot, scarab, "scarab");

    // Ranged reach the other side's from where they stand; melee never walk over to it
    if (!melee && other)
        return NoteDpsPick(bot, other, "burrower");

    Creature* boss = GetAnubarak(bot);
    if (boss && !boss->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
        return NoteDpsPick(bot, boss, "boss");

    if (Creature* scarab = state->phaseValue == AnubarakPhase::Submerged ? NearestDpsScarab(bot, *state) : nullptr)
        return NoteDpsPick(bot, scarab, "scarab");

    return NoteDpsPick(bot, nullptr, "none");
}

bool GetAnubarakMeleeBurrowerStand(Player* bot, Creature* burrower, Position& out)
{
    AnubarakState* state = bot && burrower ? ReadPhase(GET_PLAYERBOT_AI(bot)) : nullptr;
    if (!state)
        return false;

    std::vector<Position> const patches = PatchPositions(bot, *state);
    if (!BurrowerNearPatch(burrower, patches))
        return false;

    // Held, so the bot walks once per burrower and not after every turn he makes
    auto const held = state->meleeStands.find(bot->GetGUID());
    if (held != state->meleeStands.end() && held->second.burrower == burrower->GetGUID() &&
        Dist2d(burrower->GetPosition(), held->second.at) <= ANUBARAK_MELEE_STAND_DRIFT &&
        std::none_of(patches.begin(), patches.end(), [&held](Position const& patch)
                     { return Dist2d(held->second.stand, patch) < ANUBARAK_MELEE_PATCH_CLEARANCE; }))
    {
        out = held->second.stand;
        return true;
    }

    if (!MeleeBurrowerStand(bot, burrower, patches, true, out))
        return false;

    state->meleeStands[bot->GetGUID()] = MeleeStandLatch{burrower->GetGUID(), burrower->GetPosition(), out};
    return true;
}

bool AnubarakMeleeBurrowerStandApplies(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!IsToCDpsBot(bot) || !PlayerbotAI::IsMelee(bot) || bot->HasAura(SPELL_MARK) || !AnubarakEngaged(botAI))
        return false;

    AnubarakState const* state = ReadPhase(botAI);
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    Creature* burrower = target ? target->ToCreature() : nullptr;
    if (!state || !burrower || burrower->GetEntry() != static_cast<uint32>(ToCNpcs::NPC_NERUBIAN_BURROWER))
        return false;

    std::vector<Position> const patches = PatchPositions(bot, *state);
    Position stand;
    return BurrowerNearPatch(burrower, patches) && MeleeBurrowerStand(bot, burrower, patches, false, stand);
}

bool AnubarakMeleeNeedsBurrowerStand(PlayerbotAI* botAI)
{
    if (!AnubarakMeleeBurrowerStandApplies(botAI))
        return false;

    Player* bot = botAI->GetBot();
    Unit* burrower = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    AnubarakState const* state = ReadPhase(botAI);
    std::vector<Position> const patches = PatchPositions(bot, *state);
    bool const clear = std::none_of(patches.begin(), patches.end(), [bot](Position const& patch)
                                    { return Dist2d(bot->GetPosition(), patch) < ANUBARAK_MELEE_PATCH_CLEARANCE; });
    if (!clear || !bot->IsWithinMeleeRange(burrower))
        return true;

    auto const held = state->meleeStands.find(bot->GetGUID());
    return held != state->meleeStands.end() && held->second.burrower == burrower->GetGUID() &&
           Dist2d(bot->GetPosition(), held->second.stand) > ANUBARAK_MELEE_STAND_ARRIVE;
}

bool AnubarakNonTankOnPermafrost(Player* bot)
{
    if (!bot || !bot->IsAlive() || bot->HasAura(SPELL_MARK) || IsAnubarakTankPlayer(bot))
        return false;

    AnubarakState const* state = ReadPhase(GET_PLAYERBOT_AI(bot));
    if (!state)
        return false;

    // The gripped sphere comes down on the DK, which leaves once it is killing it
    AnubarakGripDuty grip;
    if (GetAnubarakGripDuty(bot, grip) && grip.step != AnubarakGripStep::Kill)
        return false;

    if (UnitOnPermafrost(bot))
        return true;

    std::vector<Position> const patches = PatchPositions(bot, *state);
    return std::any_of(patches.begin(), patches.end(), [bot](Position const& patch)
                       { return Dist2d(bot->GetPosition(), patch) < ANUBARAK_PERMAFROST_SLOW_REACH; });
}

bool GetAnubarakPermafrostStepOut(Player* bot, Position& out)
{
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    AnubarakState const* state = botAI && AnubarakNonTankOnPermafrost(bot) ? ReadPhase(botAI) : nullptr;
    if (!state)
        return false;

    std::vector<HazardCircle> hazards;
    for (Position const& patch : PatchPositions(bot, *state))
        hazards.emplace_back(patch, ANUBARAK_TANK_KEEPOUT);

    Creature* spike = GetPursuingSpike(bot);
    Player* kiter = spike ? GetSpikeTarget(bot) : nullptr;
    auto const offSpike = [&](float x, float y)
    {
        if (!spike)
            return true;

        SpikeDodgeReach const reach = DodgeReach(bot, spike);
        Position const at(x, y, bot->GetPositionZ());
        return Dist2d(at, spike->GetPosition()) >= reach.danger &&
               (!kiter || SegmentDistance2d(spike->GetPosition(), kiter->GetPosition(), at) >= reach.lane);
    };

    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    float const reach = !target                     ? 0.0f
                        : PlayerbotAI::IsMelee(bot) ? bot->GetMeleeRange(target) - 0.5f
                                                    : botAI->GetRange("spell") - ANUBARAK_SHOOT_MARGIN;
    auto const inReach = [&](float x, float y)
    { return !target || target->GetExactDist2d(x, y) <= reach; };

    Position const prefer = target ? target->GetPosition() : bot->GetPosition();
    HazardSweepCache cache;
    for (int pass = 0; pass < 3; ++pass)
    {
        std::function<bool(float, float)> accept;
        if (pass == 0)
            accept = [&](float x, float y) { return offSpike(x, y) && inReach(x, y); };
        else if (pass == 1)
            accept = offSpike;

        Position const spot = FindNearestPositionClearOfHazards(bot, hazards, ANUBARAK_TANK_STEP_SEARCH, 1.5f,
                                                                static_cast<float>(M_PI) / 8.0f, &prefer, accept,
                                                                &cache);
        if (spot == Position())
            continue;

        out = spot;
        return true;
    }

    return false;
}

Position GetAnubarakPatchSafeLeg(Player* bot, Position const& to)
{
    AnubarakState const* state = bot ? ReadPhase(GET_PLAYERBOT_AI(bot)) : nullptr;
    if (!state)
        return to;

    // The first patch the walk would cut, nearest the bot
    Position const from = bot->GetPosition();
    Position const* cut = nullptr;
    std::vector<Position> const patches = PatchPositions(bot, *state);
    for (Position const& patch : patches)
        if (PathClips(from, to, patch) && (!cut || Dist2d(from, patch) < Dist2d(from, *cut)))
            cut = &patch;

    return cut ? DetourWaypoint(bot, *cut, to) : to;
}

Creature* GetAnubarakScarabToPickUp(Player* bot)
{
    AnubarakState const* state = Sweep(bot);
    if (!state)
        return nullptr;

    bool const heroic = AnubarakHeroic(bot);
    bool const ownOnly = heroic && state->phaseValue == AnubarakPhase::Submerged && IsAnubarakPickupTank(bot);
    bool const healersOnly = heroic && GetAnubarakBurrowerTankSide(bot) >= 0;
    Creature* nearest = nullptr;
    float nearestDist = ANUBARAK_SCARAB_PICKUP_RANGE;
    for (Creature* scarab : ResolveAlive(bot, state->scarabs))
    {
        // The ten roaming at the pre-pull are the same entry, non-attackable and hitting nobody
        if (scarab->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) || scarab->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE))
            continue;

        Unit* victim = scarab->GetVictim();
        if (!victim || (victim == bot) != ownOnly)
            continue;

        Player* victimPlayer = victim->ToPlayer();
        if (!ownOnly && ((victimPlayer && IsAnubarakTankPlayer(victimPlayer)) ||
                         (healersOnly && !(victimPlayer && PlayerbotAI::IsHeal(victimPlayer)))))
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
    bool const ready = out.sphere && !bot->GetSpellCooldownDelay(SPELL_DEATH_GRIP);
    Position const sideSpot = GetAnubarakTankSpot(bot, duty->side);
    Position const otherSpot = GetAnubarakTankSpot(bot, 1 - duty->side);
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

// No duty re-read here: a recompute after the cast sees Death Grip on cooldown and drops the DK, and the
// landed sphere would go unrecorded
void NoteAnubarakGripCast(Player* bot, Creature* sphere, uint8 side)
{
    AnubarakState* state = bot && sphere && side < 2 ? ReadPhase(GET_PLAYERBOT_AI(bot)) : nullptr;
    if (!state)
        return;

    state->dutyMemoValid = false;
    GripCast& cast = state->grips[side];
    cast.valid = true;
    cast.dk = bot->GetGUID();
    cast.sphere = sphere->GetGUID();
    cast.castMs = getMSTime();
    cast.from = bot->GetPosition();
    cast.landed = false;
    state->needs[side].gripFailed = false;
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

    // The leave-Permafrost node would walk it straight back out
    for (Creature* patch : GetFrostSpheres(bot))
        if (IsPermafrostPatch(patch) && Dist2d(patch->GetPosition(), stand) < ANUBARAK_PERMAFROST_SLOW_REACH)
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

    AnubarakState* state = ReadPhase(GET_PLAYERBOT_AI(bot));
    Creature* spike = state ? GetPursuingSpike(bot) : nullptr;
    if (!spike)
    {
        RaidObs::NoteDerived(bot, "anub.kite", "none");
        return false;
    }

    bool const heroic = AnubarakHeroic(bot);
    bool const seekPatch = !heroic || GetAnubarakKiteStage(bot) == AnubarakKiteStage::Patch;
    Position leash;
    bool const leashed = heroic && spike->GetSpeed(MOVE_RUN) > KiterRunSpeed(bot) &&
                         KitePatchStand(bot, spike, *state, leash);

    LaneGoal goal;
    KitePlan* plan = heroic ? &JudgeKitePlan(bot, spike, *state) : nullptr;
    if (plan)
    {
        using AnubarakPursuit::KiteTerminal;
        if (plan->patch && (plan->terminal == KiteTerminal::Patch || plan->terminal == KiteTerminal::PatchHand))
        {
            goal.patch = true;
            goal.patchPos = plan->patchPos;
        }
        else if (Player* escort = HandKite(*plan) && plan->hand != bot->GetGUID()
                                      ? ObjectAccessor::GetPlayer(*bot, plan->hand)
                                      : nullptr)
        {
            goal.escort = true;
            goal.escortPos = escort->GetPosition();
        }

        goal.away = plan->terminal == KiteTerminal::Own && GatherCentre(bot, *state, goal.gather);
    }

    ComputeKiteStand(bot, spike, *state, ResolveSpheres(bot, *state), LaneObstacles(bot, *state),
                     seekPatch ? previous : nullptr, seekPatch, leashed ? &leash : nullptr, goal, plan, out);
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

// On the line from the room centre to the kiter this far short of it, the centre when the kiter is nearer
constexpr float ANUBARAK_ESCORT_SHORT = 25.0f;
constexpr float ANUBARAK_ESCORT_SPIKE_CLEARANCE = 15.0f;

// Turned round the room centre until clear of the spike, its lane and the patches, and on the floor
static bool EscortSpot(Player* paladin, Player* kiter, Creature* spike, AnubarakState const& state, Position& out)
{
    Vec2 const toCenter = Delta(kiter->GetPosition(), ANUBARAK_ROOM_CENTER);
    float const length = Length(toCenter);
    Position const base = length > ANUBARAK_ESCORT_SHORT
                              ? Offset(kiter->GetPosition(), {toCenter.x / length, toCenter.y / length}, ANUBARAK_ESCORT_SHORT)
                              : ANUBARAK_ROOM_CENTER;
    std::vector<Position> const patches = PatchPositions(kiter, state);
    SpikeDodgeReach const reach = DodgeReach(paladin, spike);

    static float const turns[] = {0.0f, 15.0f, -15.0f, 30.0f, -30.0f};
    for (float degrees : turns)
    {
        Vec2 const arm = Rotate(Delta(ANUBARAK_ROOM_CENTER, base), degrees * ANUBARAK_DEG_TO_RAD);
        Position const raw = Offset(ANUBARAK_ROOM_CENTER, arm, 1.0f);
        if (Dist2d(raw, spike->GetPosition()) < ANUBARAK_ESCORT_SPIKE_CLEARANCE ||
            SegmentDistance2d(spike->GetPosition(), kiter->GetPosition(), raw) < reach.laneClearance ||
            Dist2d(raw, kiter->GetPosition()) > ANUBARAK_HOP_RANGE - 1.0f ||
            std::any_of(patches.begin(), patches.end(), [&raw](Position const& patch)
                        { return Dist2d(raw, patch) < ANUBARAK_HEALER_PATCH_CLEARANCE; }))
            continue;

        Position valid;
        if (OnFloor(paladin, raw, valid))
        {
            out = valid;
            return true;
        }
    }

    return false;
}

// Heroic, once per instance per ms. The paladin bot whose Hand of Protection the kite plan ends or
// bridges the kite on, so the one walking there is the one DeriveProtection claims first. Latched by the
// plan, it ends with the hand, the mark, or the paladin unable to cast it.
static EscortRead const* DeriveEscort(Player* bot)
{
    AnubarakState* state = bot ? ReadPhase(GET_PLAYERBOT_AI(bot)) : nullptr;
    if (!state)
        return nullptr;

    EscortRead& escort = state->escort;
    uint32 const now = getMSTime();
    if (escort.memoValid && escort.memoMs == now)
        return &escort;

    escort = EscortRead();
    escort.memoValid = true;
    escort.memoMs = now;

    Creature* spike = AnubarakHeroic(bot) ? GetPursuingSpike(bot) : nullptr;
    Player* kiter = spike ? GetSpikeTarget(bot) : nullptr;
    if (!kiter || AnubarakKiterImmune(kiter))
        return &escort;

    KitePlan const& plan = JudgeKitePlan(kiter, spike, *state);
    if (!HandKite(plan) || plan.hand == kiter->GetGUID())
        return &escort;

    Player* paladin = ObjectAccessor::GetPlayer(*bot, plan.hand);
    if (!paladin || !paladin->IsAlive())
        return &escort;

    escort.paladin = plan.hand;
    escort.spotValid = EscortSpot(paladin, kiter, spike, *state, escort.spot);
    return &escort;
}

// Once per instance per ms. A kiter marked while it carries Permafrost gets Hand of Freedom from itself
// when it's a paladin bot, else from the first paladin bot in reach in the protector order. Never from a
// paladin with a Hand of its own on it or claimed for its Hand of Protection: one paladin's Hands on a
// target replace each other.
static FreedomRead const* DeriveFreedom(Player* bot)
{
    AnubarakState* state = bot ? ReadPhase(GET_PLAYERBOT_AI(bot)) : nullptr;
    if (!state)
        return nullptr;

    FreedomRead& freedom = state->freedom;
    uint32 const now = getMSTime();
    if (freedom.memoValid && freedom.memoMs == now)
        return &freedom;

    freedom = FreedomRead();
    freedom.memoValid = true;
    freedom.memoMs = now;

    Player* kiter = GetSpikeTarget(bot);
    if (!kiter || !UnitOnPermafrost(kiter) || kiter->HasAura(ai::paladin::SPELL_HAND_OF_FREEDOM))
        return &freedom;

    ProtectClaim const* claim = DeriveProtection(bot);
    ObjectGuid const protector = claim ? claim->protector : ObjectGuid::Empty;
    auto const frees = [&](Player* paladin)
    {
        PlayerbotAI* ai = paladin->getClass() == CLASS_PALADIN && paladin->IsAlive() ? GET_PLAYERBOT_AI(paladin) : nullptr;
        return ai && paladin->GetGUID() != protector && !ai::paladin::HasAnyPaladinHandFromCaster(kiter, paladin) &&
               (paladin == kiter ||
                (paladin->IsWithinDistInMap(kiter, ANUBARAK_HOP_RANGE) && paladin->IsWithinLOSInMap(kiter))) &&
               ai->CanCastSpell("hand of freedom", kiter);
    };

    freedom.kiter = kiter->GetGUID();
    if (frees(kiter))
    {
        freedom.paladin = kiter->GetGUID();
        return &freedom;
    }

    for (Player* paladin : ProtectorOrder(bot))
    {
        if (frees(paladin))
        {
            freedom.paladin = paladin->GetGUID();
            break;
        }
    }

    return &freedom;
}

bool GetAnubarakEscortSpot(Player* bot, Position& out)
{
    EscortRead const* escort = bot && bot->getClass() == CLASS_PALADIN ? DeriveEscort(bot) : nullptr;
    if (!escort || escort->paladin != bot->GetGUID() || !escort->spotValid)
        return false;

    out = escort->spot;
    return true;
}

bool IsAnubarakEscort(PlayerbotAI* botAI)
{
    Position spot;
    return botAI && GetAnubarakEscortSpot(botAI->GetBot(), spot);
}

Player* GetAnubarakKiterToFree(Player* bot)
{
    FreedomRead const* freedom = bot && bot->getClass() == CLASS_PALADIN ? DeriveFreedom(bot) : nullptr;
    bool const duty = freedom && freedom->paladin == bot->GetGUID();
    RaidObs::NoteDerived(bot, "anub.freedom", duty ? "cast" : "none");
    return duty ? ObjectAccessor::GetPlayer(*bot, freedom->kiter) : nullptr;
}

// A side tank with no burrower of its side to hold or take gathers too: standing on its patch, a mark
// on it would walk the spike straight into one. So does the main tank, off where the spike is summoned.
// The escort walks after the kiter instead.
static AnubarakState const* GatherState(Player* bot)
{
    PlayerbotAI* botAI = bot && AnubarakHeroic(bot) ? GET_PLAYERBOT_AI(bot) : nullptr;
    AnubarakState const* state = botAI ? ReadPhase(botAI) : nullptr;
    if (!state || state->phaseValue != AnubarakPhase::Submerged || !state->submergePosValid ||
        bot->HasAura(SPELL_MARK) || (GetAnubarakBurrowerTankSide(bot) >= 0 && GetAnubarakBurrowerPick(botAI)) ||
        IsAnubarakEscort(botAI))
        return nullptr;

    return state;
}

bool GetAnubarakGatherSpot(Player* bot, Position& out)
{
    AnubarakState const* state = GatherState(bot);
    Position gather;
    if (!state || !GatherCentre(bot, *state, gather))
        return false;

    // On the edge away from where he went down: the spike is summoned there 2.5 s in
    if (bot->GetGUID() == state->mainTank)
    {
        Vec2 const away = Delta(state->submergePos, gather);
        float const length = Length(away);
        out = length > 1.0f ? Offset(gather, {away.x / length, away.y / length}, ANUBARAK_GATHER_TANK_EDGE) : gather;
        return true;
    }

    // A slot per bot round the point, so twenty bots don't stand on one spot
    float const angle = static_cast<float>(bot->GetGUID().GetCounter() % 12) * 30.0f * ANUBARAK_DEG_TO_RAD;
    out = Offset(gather, {std::cos(angle), std::sin(angle)}, ANUBARAK_GATHER_SPREAD);
    return true;
}

bool AnubarakGathers(PlayerbotAI* botAI) { return botAI && GatherState(botAI->GetBot()); }

static AnubarakState const* HealerStandState(Player* bot)
{
    PlayerbotAI* botAI = bot && bot->IsAlive() && PlayerbotAI::IsHeal(bot) ? GET_PLAYERBOT_AI(bot) : nullptr;
    AnubarakState const* state = botAI ? ReadPhase(botAI) : nullptr;
    if (!state || !state->raidPointValid || bot->HasAura(SPELL_MARK) ||
        (state->phaseValue != AnubarakPhase::Surface && state->phaseValue != AnubarakPhase::Swarm))
        return nullptr;

    return state;
}

bool AnubarakHealerStandApplies(PlayerbotAI* botAI) { return botAI && HealerStandState(botAI->GetBot()); }

bool GetAnubarakHealerStand(Player* bot, Position& out)
{
    AnubarakState const* state = HealerStandState(bot);
    if (!state)
        return false;

    std::vector<ObjectGuid> healers;
    ForEachGroupMember(bot, [&](Player* member)
    {
        if (member->IsAlive() && GET_PLAYERBOT_AI(member) && PlayerbotAI::IsHeal(member))
            healers.push_back(member->GetGUID());

        return false;
    });

    std::sort(healers.begin(), healers.end());
    size_t index = static_cast<size_t>(
        std::distance(healers.begin(), std::find(healers.begin(), healers.end(), bot->GetGUID())));
    size_t count = std::max<size_t>(healers.size(), 1);
    std::vector<Position> const patches = PatchPositions(bot, *state);
    auto const clear = [&patches](Position const& slot)
    {
        return std::none_of(patches.begin(), patches.end(), [&slot](Position const& patch)
                            { return Dist2d(slot, patch) < ANUBARAK_HEALER_PATCH_CLEARANCE; });
    };

    // Patches this far apart put one side tank past a heal from the raid point, so the healers split in
    // two groups by guid, one a side, each off its side tank toward the raid point: there, not toward the
    // anchor, which keeps them off him
    Position centre = state->raidPoint;
    Position const spots[2] = {state->spot0.Get(), state->spot1.Get()};
    Player* mainTank = state->mainTank.IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(*bot, state->mainTank);
    if (Dist2d(spots[0], spots[1]) >= ANUBARAK_HEALER_SPLIT_GAP && index < healers.size())
    {
        Position const& spot = spots[index % 2];
        Vec2 const toward = Delta(spot, state->raidPoint);
        float const length = Length(toward);
        Position const own = length > 1.0f
                                 ? Offset(spot, {toward.x / length, toward.y / length},
                                          std::min(length, ANUBARAK_HEALER_SIDE_OFFSET))
                                 : state->raidPoint;
        if (clear(own) && (!mainTank || !mainTank->IsAlive() ||
                           Dist2d(own, mainTank->GetPosition()) <= ANUBARAK_HEALER_MAIN_TANK_REACH))
        {
            centre = own;
            count = (healers.size() - index % 2 + 1) / 2;
            index /= 2;
        }
    }

    float const base = 2.0f * static_cast<float>(M_PI) * static_cast<float>(index) / static_cast<float>(count);
    for (uint32 turn = 0; turn < ANUBARAK_HEALER_SLOT_TRIES; ++turn)
    {
        float const angle = base + 2.0f * static_cast<float>(M_PI) * static_cast<float>(turn) /
                                       static_cast<float>(ANUBARAK_HEALER_SLOT_TRIES);
        Position const slot = Offset(centre, {std::cos(angle), std::sin(angle)}, ANUBARAK_GATHER_SPREAD);
        if (clear(slot))
        {
            out = slot;
            return true;
        }
    }

    out = centre;
    return true;
}

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

    SpikeDodgeReach const reach = DodgeReach(bot, spike);
    if (bot->GetExactDist2d(spike->GetPositionX(), spike->GetPositionY()) < reach.danger)
        return true;

    Player* target = GetSpikeTarget(bot);
    return target && SegmentDistance2d(spike->GetPosition(), target->GetPosition(), bot->GetPosition()) < reach.lane;
}

bool GetAnubarakSpikeDodgeSpot(Player* bot, Position& out)
{
    Creature* spike = bot ? GetPursuingSpike(bot) : nullptr;
    if (!spike)
        return false;

    Position const spikePos = spike->GetPosition();
    Player* target = GetSpikeTarget(bot);
    Position const targetPos = target ? target->GetPosition() : spikePos;
    SpikeDodgeReach const reach = DodgeReach(bot, spike);

    std::vector<HazardCircle> const spikeOnly = {HazardCircle(spikePos, reach.clearance)};
    std::vector<HazardCircle> withPatches = spikeOnly;
    if (AnubarakHeroic(bot))
        for (Creature* sphere : GetFrostSpheres(bot))
            if (IsPermafrostPatch(sphere))
                withPatches.emplace_back(sphere->GetPosition(), ANUBARAK_PERMAFROST_SLOW_REACH);

    auto const clearOfLane = [&](float x, float y)
    { return !target || SegmentDistance2d(spikePos, targetPos, Position(x, y, 0.0f)) >= reach.laneClearance; };

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

    bool const nearSpike = bot->GetExactDist2d(spikePos.GetPositionX(), spikePos.GetPositionY()) < reach.danger;
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

    SpikeDodgeReach const reach = DodgeReach(bot, spike);
    float const spikeDist = Dist2d(spike->GetPosition(), hold.spot);
    if (spikeDist < reach.danger || spikeDist > std::max(ANUBARAK_DODGE_HOLD_RELEASE, hold.spikeDistAtSet))
        return false;

    if (kiter && SegmentDistance2d(spike->GetPosition(), kiter->GetPosition(), hold.spot) < reach.lane)
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

bool AnubarakKiterKeepsMark(Player* bot)
{
    AnubarakState* state = bot ? ReadPhase(GET_PLAYERBOT_AI(bot)) : nullptr;
    Creature* spike = state ? GetPursuingSpike(bot) : nullptr;
    if (!spike)
        return false;

    if (AnubarakHeroic(bot) && bot->HasAura(SPELL_MARK))
    {
        KitePlan const& plan = JudgeKitePlan(bot, spike, *state);
        if (plan.terminal == AnubarakPursuit::KiteTerminal::Own)
            return false;

        if (HandKite(plan) && plan.hand == bot->GetGUID())
            return true;
    }

    Position stand;
    if (KitePatchStand(bot, spike, *state, stand))
        return true;

    int32 const toEmerge = static_cast<int32>(state->submergedMs + ANUBARAK_SUBMERGED_MS - getMSTime());
    return state->phaseValue == AnubarakPhase::Submerged && toEmerge <= static_cast<int32>(ANUBARAK_KITE_HOP_EMERGE_MS);
}

bool GetAnubarakRedirect(Player* bot, AnubarakRedirect& out)
{
    out = AnubarakRedirect();
    uint8 const cls = bot ? bot->getClass() : 0;
    PlayerbotAI* botAI = cls == CLASS_HUNTER || cls == CLASS_ROGUE ? GET_PLAYERBOT_AI(bot) : nullptr;
    AnubarakState* state = botAI ? ReadPhase(botAI) : nullptr;
    if (!state || !bot->IsAlive() || bot->HasAura(SPELL_MARK))
        return false;

    Player* firstHunter = nullptr;
    Player* firstRogue = nullptr;
    std::vector<Player*> hunters;
    ForEachGroupMember(bot, [&](Player* member)
    {
        if (!member->IsAlive() || !GET_PLAYERBOT_AI(member))
            return false;

        if (member->getClass() == CLASS_HUNTER)
        {
            firstHunter = firstHunter ? firstHunter : member;
            hunters.push_back(member);
        }
        else if (member->getClass() == CLASS_ROGUE)
            firstRogue = firstRogue ? firstRogue : member;

        return false;
    });

    bool const pull = getMSTimeDiff(state->pullMs, getMSTime()) < ANUBARAK_REDIRECT_PULL_MS;
    if (pull && (bot == firstHunter || bot == firstRogue))
    {
        Player* tank = state->mainTank.IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(*bot, state->mainTank);
        if (!tank || !tank->IsAlive() || tank == bot)
            return false;

        out.tank = tank;
        out.dump = GetAnubarak(bot);
        return true;
    }

    // Selectable burrowers not on the bot side tank they belong to, by guid
    std::vector<std::pair<Creature*, Player*>> loose;
    std::unordered_map<ObjectGuid, uint8> const& owners = BurrowerOwners(bot, *state);
    for (Creature* burrower : ResolveAlive(bot, state->burrowers))
    {
        auto const owner = owners.find(burrower->GetGUID());
        if (burrower->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) || owner == owners.end())
            continue;

        Player* tank = UsableSideTank(bot, *state, owner->second);
        if (tank && GET_PLAYERBOT_AI(tank) && burrower->GetVictim() != tank)
            loose.emplace_back(burrower, tank);
    }

    std::sort(loose.begin(), loose.end(),
              [](auto const& a, auto const& b) { return a.first->GetGUID() < b.first->GetGUID(); });

    // Tricks only moves the threat of what the rogue hits, so it goes out on the burrower it's on
    if (cls == CLASS_ROGUE)
    {
        Unit* current = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
        for (auto const& [burrower, tank] : loose)
        {
            if (burrower != current || !bot->IsWithinDistInMap(tank, ANUBARAK_TRICKS_RANGE))
                continue;

            out.tank = tank;
            out.dump = burrower;
            return true;
        }

        return false;
    }

    // One hunter a burrower, in group order, the pull's Misdirection aside
    size_t index = 0;
    for (Player* hunter : hunters)
    {
        if (hunter == bot)
            break;

        if (!(pull && hunter == firstHunter))
            ++index;
    }

    if (index >= loose.size())
        return false;

    out.tank = loose[index].second;
    out.dump = loose[index].first;
    return true;
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
