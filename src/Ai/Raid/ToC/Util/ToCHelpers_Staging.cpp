#include "ToCHelpers_Staging.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "SharedDefines.h"
#include "Timer.h"
#include "ToCData.h"
#include "ToCHelpers_Anubarak.h"
#include "ToCHelpers_FactionChampionsDefence.h"
#include "ToCHelpers_Icehowl.h"
#include "ToCHelpers_Jaraxxus.h"
#include "ToCHelpers_Jormungars.h"
#include "ToCHelpers_NorthrendBeasts.h"
#include "ToCHelpers_TwinValkyr.h"

namespace TrialOfTheCrusaderHelpers
{
namespace
{
// one Barrett per stage: Beasts, Beasts heroic, Jaraxxus, champions, twins, Anub'arak
std::vector<uint32> const BARRETT_ENTRIES = {34816, 35909, 35035, 35766, 35770, 35771};
// He stands about 50 yd from ARENA_CENTER, so this reaches him from anywhere in the arena
constexpr float BARRETT_SEARCH_RADIUS = 250.0f;
// Each stage summons a new Barrett whose gossip can be picked as soon as he spawns, and the window only
// opens once a read saw him with it
constexpr uint32 BARRETT_SEARCH_MS = 500;

// The reader takes a stand noted up to 2 s past the release
constexpr uint32 STAGING_NOTE_TAIL_MS = 2000;

constexpr float ARENA_ROOM_RADIUS = 80.0f;
constexpr float ARENA_ROOM_MIN_Z = 380.0f;
// Past the Web Door, on the floor below the arena
constexpr float ANUBARAK_ROOM_MIN_X = 661.6f;
constexpr float ANUBARAK_ROOM_MAX_Z = 200.0f;

constexpr float ARENA_STAND_Z = 394.5f;
constexpr float ANUBARAK_STAND_Z = 142.2f;

struct Point2
{
    float x;
    float y;
};

struct Vec2
{
    float x;
    float y;
};

// trial_of_the_crusader.h Locs[], under its LocNames names. The script header isn't on our include path.
constexpr Point2 LOC_GATE_FRONT{563.93f, 178.37f};          // 1, Gormok and Icehowl
constexpr Point2 LOC_DREADSCALE{575.74f, 171.5f};           // 2
constexpr Point2 LOC_ACIDMAW{549.93f, 171.5f};              // 3
constexpr Point2 LOC_CENTER{563.672974f, 139.571f};         // 4
constexpr Point2 LOC_VALKYR_DEST_2_RIGHT{585.5f, 170.0f};   // 9, Fjola
constexpr Point2 LOC_VALKYR_DEST_2_LEFT{545.5f, 170.0f};    // 10, Eydis
constexpr Point2 LOC_DARKESS_1{586.060242f, 117.514809f};   // 13
constexpr Point2 LOC_LIGHTESS_1{541.021118f, 117.262932f};  // 15
// The intro walks him from LOC_CENTER to 10 yd south of it
constexpr Point2 JARAXXUS_INTRO_SPOT{563.672974f, 129.571f};

// FactionChampionLoc[10..19]: an Alliance raid's landing slots, mirrored about LOC_CENTER's x for Horde
constexpr Point2 FACTION_CHAMPION_LANDING[] = {
    {535.469f, 113.012f}, {526.417f, 137.465f}, {528.108f, 111.057f}, {519.92f, 134.285f},  {533.648f, 119.148f},
    {531.399f, 125.63f},  {528.958f, 131.47f},  {526.309f, 116.667f}, {524.238f, 122.411f}, {521.901f, 128.488f},
};

constexpr float NON_DUTY_MIN = STAGING_DUTY_STAND + STAGING_RELEASE_MARGIN;

struct RingSlot
{
    float radius;
    float degrees;
};

// Gormok: arcs to his sides 25+ yd out (Stomp reaches 20, clearance 24), off his drag line to the centre
constexpr float GORMOK_SWAP_STAND = 9.0f;
constexpr float GORMOK_MELEE_STAND = 9.0f;
constexpr float GORMOK_CASTER_INNER = 25.0f;
constexpr RingSlot GORMOK_CASTER_SLOTS[] = {
    {GORMOK_CASTER_INNER, 44.0f}, {GORMOK_CASTER_INNER, 62.0f}, {GORMOK_CASTER_INNER, 80.0f},
    {33.0f, 35.0f},               {33.0f, 49.0f},               {33.0f, 63.0f},
    {40.0f, 25.0f},               {40.0f, 37.0f},
};

// Worms: casters off Sweep round Acidmaw, past the melee circle on Dreadscale and inside spell reach
// of him, out of his Spew, which faces his tank
constexpr Point2 WORM_LATTICE_ORIGIN{566.27f, 157.5f};
constexpr float WORM_LATTICE_STEP = 10.5f;
constexpr float WORM_CASTER_ACIDMAW_MIN = WORM_SWEEP_CLEARANCE + 1.0f;
constexpr float WORM_CASTER_DREADSCALE_MIN = WORM_MELEE_STACK_CLEARANCE;
constexpr float WORM_CASTER_DREADSCALE_MAX = 36.0f;

// Icehowl: one radius per role inside the breath spread's bands
constexpr float ICEHOWL_MELEE_STAND = 11.0f;
constexpr float ICEHOWL_HEALER_STAND = 17.5f;
constexpr float ICEHOWL_RANGED_STAND = 23.0f;
constexpr float ICEHOWL_RANGED_ARC_DEG = 72.0f;
constexpr float ICEHOWL_BEARING_EPSILON_DEG = 0.01f;

// Jaraxxus: no spread, so the casters stack in front of him and melee stand a few steps back
constexpr float JARAXXUS_ADD_TANK_STAND = 8.0f;
constexpr float JARAXXUS_MELEE_STAND = 9.0f;
constexpr float JARAXXUS_MELEE_DEGREES[] = {0.0f, 25.0f, -25.0f, 50.0f, -50.0f};
constexpr float JARAXXUS_STACK_AHEAD = 20.0f;
constexpr float JARAXXUS_STACK_RADIUS = 6.0f;
constexpr float JARAXXUS_STACK_STEP = 3.0f;

// Champions: detection_range 20 plus the level gap, centre to centre with no combat reach
constexpr float CHAMPION_AGGRO = 23.0f;
constexpr float CHAMPION_CLEARANCE = CHAMPION_AGGRO + STAGING_RELEASE_MARGIN;
// a parked tank drifts up to STAGING_UNPARK before it walks back
constexpr float CHAMPION_TANK_CLEARANCE = CHAMPION_AGGRO + STAGING_UNPARK;
constexpr float CHAMPION_TANK_SEARCH_STEP = 0.5f;
constexpr float CHAMPION_TANK_SEARCH_MAX = 60.0f;
constexpr float CHAMPION_SIDE_STEP = 3.0f;
// the first engage sends every idle champion at its nearest player, so everyone else stands this much
// farther from each slot than a tank
constexpr float CHAMPION_TANK_LEAD = 3.0f;
// about 24 yd past the tank line, where the DK tank's grip drops a healer, so the bot DK grips without a walk.
// The player stands 24 yd further out, still on the floor.
constexpr float CHAMPION_GRIP_POST = 54.0f;
constexpr float CHAMPION_PLAYER_POST = CHAMPION_GRIP_POST + 24.0f;
// the gripped healer is carried down this lane to the player, so healers and ranged wait off it
constexpr float CHAMPION_GRIP_LANE = 10.0f;
// healers wait just behind the melee, so the tanks and melee are in heal range from the first second
constexpr float CHAMPION_HEAL_BEHIND = 12.0f;
constexpr uint32 SPELL_DEATH_GRIP = 49576;
constexpr float CHAMPION_LATTICE_STEP = 10.0f;

// Twins: tanks on the line from the south portal of their colour to their twin, the raid by Dark 1.
// Every stand ends up 30+ yd from both twins and nearer its south portal than the north one.
constexpr float TWIN_TANK_STAND = 22.5f;
constexpr float TWIN_STACK_AHEAD = 16.0f;
constexpr float TWIN_STACK_RADIUS = 8.0f;
constexpr float TWIN_STACK_STEP = 4.0f;

// Anub'arak: the raid west of the room centre, 40+ yd from Locs[17], so his run ends on the anchor
constexpr float ANUBARAK_MELEE_STAND = 8.0f;
constexpr float ANUBARAK_MELEE_DEGREES[] = {0.0f, 40.0f, -40.0f};
constexpr RingSlot ANUBARAK_CASTER_SLOTS[] = {
    {22.0f, 180.0f}, {22.0f, 160.0f}, {22.0f, 200.0f}, {22.0f, 140.0f}, {22.0f, 220.0f}, {22.0f, 120.0f},
    {22.0f, 240.0f}, {30.0f, 170.0f}, {30.0f, 190.0f}, {30.0f, 150.0f}, {30.0f, 210.0f}, {30.0f, 130.0f},
    {30.0f, 230.0f},
};

// Lattice points sit exactly on the radius
constexpr float LATTICE_EPSILON = 0.01f;

static_assert(GORMOK_SWAP_STAND > STAGING_DUTY_STAND, "his tank stands nearest");
static_assert(GORMOK_MELEE_STAND >= NON_DUTY_MIN && GORMOK_CASTER_INNER >= NON_DUTY_MIN, "gormok release margin");
static_assert(WORM_MELEE_STAND >= NON_DUTY_MIN, "worm melee clear of Dreadscale's release");
static_assert(LOC_DREADSCALE.x - LOC_ACIDMAW.x - WORM_MELEE_STAND >= NON_DUTY_MIN,
              "worm melee clear of Acidmaw's release");
static_assert(WORM_CASTER_ACIDMAW_MIN >= NON_DUTY_MIN && WORM_CASTER_DREADSCALE_MIN >= NON_DUTY_MIN,
              "worm casters clear of both releases");
static_assert(LOC_DREADSCALE.x - LOC_ACIDMAW.x > 2.0f * STAGING_DUTY_STAND, "each worm's holder stands nearest it");
static_assert(ICEHOWL_MELEE_STAND >= NON_DUTY_MIN, "icehowl release margin");
static_assert(ICEHOWL_SPREAD_MELEE_MIN <= ICEHOWL_MELEE_STAND && ICEHOWL_MELEE_STAND <= ICEHOWL_SPREAD_MELEE_MAX,
              "icehowl melee sit in the breath spread's band");
static_assert(ICEHOWL_SPREAD_HEALER_MIN <= ICEHOWL_HEALER_STAND && ICEHOWL_HEALER_STAND <= ICEHOWL_SPREAD_HEALER_MAX,
              "icehowl healers sit in the breath spread's band");
static_assert(ICEHOWL_SPREAD_RANGED_MIN <= ICEHOWL_RANGED_STAND && ICEHOWL_RANGED_STAND <= ICEHOWL_SPREAD_RANGED_MAX,
              "icehowl ranged sit in the breath spread's band");
static_assert(JARAXXUS_ADD_TANK_STAND > STAGING_DUTY_STAND, "his main tank stands nearest");
static_assert(JARAXXUS_MELEE_STAND >= NON_DUTY_MIN && JARAXXUS_STACK_AHEAD - JARAXXUS_STACK_RADIUS >= NON_DUTY_MIN,
              "jaraxxus release margin");

enum class StandRole : uint8
{
    Healer,
    Ranged,
    Melee
};

constexpr size_t STAND_ROLE_COUNT = 3;

struct DealtStand
{
    Position stand;
    char const* role = "none";
    // A forced attack's tank stand: the boss walks onto it from release
    bool duty = false;
    Position release;
};

struct StagingHold
{
    Position raw;
    Position spot;
    bool hasSpot = false;
    bool parked = false;
};

struct StageNote
{
    ToCStagingBoss arrival = ToCStagingBoss::None;
    std::string label;
};

struct StagingState
{
    uint32 memoMs = 0;
    bool memoValid = false;  // 0 is a real getMSTime value
    ToCStagingBoss arrival = ToCStagingBoss::None;

    // A ToC trace opens at the engage, which is also when the arrival closes, so every bot's last label
    // is noted again for a while after it: a note written before the open never reaches a file.
    std::unordered_map<ObjectGuid, StageNote> stageNotes;
    ToCStagingBoss closedArrival = ToCStagingBoss::None;
    uint32 closedMs = 0;

    ObjectGuid barrett;
    uint32 barrettSearchMs = 0;
    bool barrettSearched = false;
    // The encounter of the stage Barrett last showed his gossip at
    ToCEncounter gossipSeenFor = ToCEncounter::None;

    ToCStagingBoss dealArrival = ToCStagingBoss::None;
    uint32 dealMs = 0;
    bool dealValid = false;
    std::unordered_map<ObjectGuid, DealtStand> stands;

    std::unordered_map<ObjectGuid, StagingHold> holds;
};

RaidInstanceState<StagingState> stagingStates;

struct DutyStand
{
    ObjectGuid guid;
    char const* role;
    Position stand;
    bool forced;
    Position release;
};

// Slots shared by the roles: each role's order lists the slot indices it takes, in turn
struct Layout
{
    std::vector<Position> slots;
    std::vector<size_t> order[STAND_ROLE_COUNT];
};

float DegToRad(float degrees) { return degrees * static_cast<float>(M_PI) / 180.0f; }

float Dist(Point2 a, Point2 b) { return std::hypot(a.x - b.x, a.y - b.y); }

Vec2 Direction(Point2 from, Point2 to)
{
    float const length = Dist(from, to);
    if (length <= 0.0f)
        return {0.0f, 1.0f};

    return {(to.x - from.x) / length, (to.y - from.y) / length};
}

Vec2 Negate(Vec2 v) { return {-v.x, -v.y}; }

// Counter-clockwise
Vec2 Rotate(Vec2 v, float degrees)
{
    float const c = std::cos(DegToRad(degrees));
    float const s = std::sin(DegToRad(degrees));
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}

Point2 Along(Point2 from, Vec2 dir, float distance) { return {from.x + dir.x * distance, from.y + dir.y * distance}; }

Point2 PointOf(Position const& position) { return {position.GetPositionX(), position.GetPositionY()}; }

Position At(Point2 point, float z) { return Position(point.x, point.y, z); }

// Rows run along x
std::vector<Point2> HexLattice(Point2 origin, float step, Point2 centre, float radius)
{
    std::vector<Point2> points;
    int32 const span = static_cast<int32>(std::ceil((Dist(origin, centre) + radius) / step)) * 2 + 1;
    float const rowHeight = step * std::sqrt(3.0f) / 2.0f;
    for (int32 j = -span; j <= span; ++j)
    {
        for (int32 i = -span; i <= span; ++i)
        {
            Point2 const point{origin.x + step * (static_cast<float>(i) + static_cast<float>(j) / 2.0f),
                               origin.y + rowHeight * static_cast<float>(j)};
            if (Dist(point, centre) <= radius + LATTICE_EPSILON)
                points.push_back(point);
        }
    }

    return points;
}

bool OnArenaFloor(Point2 point) { return Dist(point, PointOf(ARENA_CENTER)) <= WORM_SPOT_FLOOR_RADIUS; }

// Arena slots past the floor radius are dropped
void AddArenaSlot(Layout& layout, Point2 point, std::initializer_list<StandRole> roles)
{
    if (!OnArenaFloor(point))
        return;

    for (StandRole role : roles)
        layout.order[static_cast<size_t>(role)].push_back(layout.slots.size());

    layout.slots.push_back(At(point, ARENA_STAND_Z));
}

void SortOrder(Layout& layout, StandRole role, std::function<float(Point2)> const& key)
{
    std::vector<size_t>& order = layout.order[static_cast<size_t>(role)];
    std::stable_sort(order.begin(), order.end(),
                     [&](size_t lhs, size_t rhs)
                     { return key(PointOf(layout.slots[lhs])) < key(PointOf(layout.slots[rhs])); });
}

void AddForcedDuty(std::vector<DutyStand>& duties, Player* tank, char const* role, Point2 stand, Point2 release)
{
    if (tank)
        duties.push_back({tank->GetGUID(), role, At(stand, ARENA_STAND_Z), true, At(release, ARENA_STAND_Z)});
}

void LayoutGormok(Player* bot, std::vector<DutyStand>& duties, Layout& layout)
{
    Point2 const release = LOC_GATE_FRONT;
    Vec2 const u = Direction(release, LOC_CENTER);

    std::vector<Player*> const roster = GetBeastsTankRoster(bot);
    AddForcedDuty(duties, roster.size() > 0 ? roster[0] : nullptr, "tank", Along(release, u, STAGING_DUTY_STAND),
                  release);
    AddForcedDuty(duties, roster.size() > 1 ? roster[1] : nullptr, "swap", Along(release, u, GORMOK_SWAP_STAND),
                  release);

    for (float side : {90.0f, -90.0f})
        AddArenaSlot(layout, Along(release, Rotate(u, side), GORMOK_MELEE_STAND), {StandRole::Melee});

    for (RingSlot const& slot : GORMOK_CASTER_SLOTS)
        for (float sign : {1.0f, -1.0f})
            AddArenaSlot(layout, Along(release, Rotate(u, sign * slot.degrees), slot.radius),
                         {StandRole::Healer, StandRole::Ranged});
}

void LayoutWorms(PlayerbotAI* botAI, std::vector<DutyStand>& duties, Layout& layout)
{
    Point2 const dreadscale = LOC_DREADSCALE;
    Point2 const acidmaw = LOC_ACIDMAW;
    Vec2 const w = Direction(acidmaw, dreadscale);
    Point2 const mobileStand = Along(dreadscale, w, STAGING_DUTY_STAND);
    Point2 const stationaryStand = Along(acidmaw, Negate(w), STAGING_DUTY_STAND);

    std::vector<Player*> const roster = GetBeastsTankRoster(botAI->GetBot());
    AddForcedDuty(duties, roster.size() > 0 ? roster[0] : nullptr, "mobile", mobileStand, dreadscale);
    AddForcedDuty(duties, roster.size() > 1 ? roster[1] : nullptr, "stationary", stationaryStand, acidmaw);

    for (float side : {WORM_MELEE_GROUP_DEG, -WORM_MELEE_GROUP_DEG})
        AddArenaSlot(layout, Along(dreadscale, Rotate(Negate(w), side), WORM_MELEE_STAND), {StandRole::Melee});

    float const spewClear = WormSpewHalfArc(botAI) + WORM_SPEW_CLEAR_PAD;
    std::vector<Point2> const lattice =
        HexLattice(WORM_LATTICE_ORIGIN, WORM_LATTICE_STEP, dreadscale, WORM_CASTER_DREADSCALE_MAX);
    for (Point2 const& point : lattice)
    {
        float const fromDreadscale = Dist(point, dreadscale);
        if (Dist(point, acidmaw) < WORM_CASTER_ACIDMAW_MIN || fromDreadscale < WORM_CASTER_DREADSCALE_MIN ||
            fromDreadscale <= 0.0f)
            continue;

        float const along = ((point.x - dreadscale.x) * w.x + (point.y - dreadscale.y) * w.y) / fromDreadscale;
        if (std::acos(std::clamp(along, -1.0f, 1.0f)) <= spewClear)
            continue;

        AddArenaSlot(layout, point, {StandRole::Healer, StandRole::Ranged});
    }

    SortOrder(layout, StandRole::Healer,
              [&](Point2 slot) { return std::max(Dist(slot, mobileStand), Dist(slot, stationaryStand)); });
    SortOrder(layout, StandRole::Ranged, [&](Point2 slot) { return Dist(slot, dreadscale); });
}

void LayoutIcehowl(Player* bot, std::vector<DutyStand>& duties, Layout& layout)
{
    Point2 const release = LOC_GATE_FRONT;
    Vec2 const n = Direction(LOC_CENTER, release);
    Vec2 const back = Negate(n);
    float const step = IcehowlBreathStepDeg(bot);

    std::vector<Player*> const roster = GetBeastsTankRoster(bot);
    AddForcedDuty(duties, roster.size() > 0 ? roster[0] : nullptr, "tank", Along(release, n, STAGING_DUTY_STAND),
                  release);

    // k = 0, 1, -1, 2, -2, ...
    std::vector<float> bearings;
    if (step > 0.0f)
    {
        bearings.push_back(0.0f);
        float const widest = ICEHOWL_SPREAD_HALF_ARC_DEG + ICEHOWL_BEARING_EPSILON_DEG;
        for (int32 k = 1; static_cast<float>(k) * step <= widest; ++k)
        {
            bearings.push_back(static_cast<float>(k) * step);
            bearings.push_back(-static_cast<float>(k) * step);
        }
    }

    for (float bearing : bearings)
        if (std::fabs(bearing) <= ICEHOWL_SPREAD_MELEE_ARC_DEG + ICEHOWL_BEARING_EPSILON_DEG)
            AddArenaSlot(layout, Along(release, Rotate(back, bearing), ICEHOWL_MELEE_STAND), {StandRole::Melee});

    // Healers nearest the 60° bearings first
    std::vector<float> healerBearings = bearings;
    std::stable_sort(healerBearings.begin(), healerBearings.end(),
                     [](float lhs, float rhs)
                     {
                         return std::fabs(std::fabs(lhs) - ICEHOWL_SPREAD_HEALER_BEARING_DEG) <
                                std::fabs(std::fabs(rhs) - ICEHOWL_SPREAD_HEALER_BEARING_DEG);
                     });
    for (float bearing : healerBearings)
        AddArenaSlot(layout, Along(release, Rotate(back, bearing), ICEHOWL_HEALER_STAND), {StandRole::Healer});

    for (float bearing : bearings)
        if (std::fabs(bearing) <= ICEHOWL_RANGED_ARC_DEG + ICEHOWL_BEARING_EPSILON_DEG)
            AddArenaSlot(layout, Along(release, Rotate(back, bearing), ICEHOWL_RANGED_STAND), {StandRole::Ranged});
}

Point2 JaraxxusAnchor(PlayerbotAI* botAI, uint32 stage)
{
    if (stage == TOC_PROGRESS_BEASTS_DEAD)
        return JARAXXUS_INTRO_SPOT;

    Unit* jaraxxus = GetJaraxxus(botAI);
    return jaraxxus ? PointOf(jaraxxus->GetPosition()) : LOC_CENTER;
}

void LayoutJaraxxus(PlayerbotAI* botAI, uint32 stage, std::vector<Player*> const& bots,
                    std::vector<DutyStand>& duties, Layout& layout)
{
    Point2 const anchor = JaraxxusAnchor(botAI, stage);
    Vec2 const front{0.0f, 1.0f};

    for (Player* member : bots)
    {
        if (PlayerbotAI::IsMainTank(member))
            AddForcedDuty(duties, member, "tank", Along(anchor, front, STAGING_DUTY_STAND), anchor);
        else if (PlayerbotAI::IsAssistTankOfIndex(member, 0, true))
            AddForcedDuty(duties, member, "addtank0", Along(anchor, Rotate(front, 90.0f), JARAXXUS_ADD_TANK_STAND),
                          anchor);
        else if (PlayerbotAI::IsAssistTankOfIndex(member, 1, true))
            AddForcedDuty(duties, member, "addtank1", Along(anchor, Rotate(front, -90.0f), JARAXXUS_ADD_TANK_STAND),
                          anchor);
    }

    for (float degrees : JARAXXUS_MELEE_DEGREES)
        AddArenaSlot(layout, Along(anchor, Rotate(Negate(front), degrees), JARAXXUS_MELEE_STAND), {StandRole::Melee});

    Point2 const stack = Along(anchor, front, JARAXXUS_STACK_AHEAD);
    for (Point2 const& point : HexLattice(stack, JARAXXUS_STACK_STEP, stack, JARAXXUS_STACK_RADIUS))
        AddArenaSlot(layout, point, {StandRole::Healer, StandRole::Ranged});

    auto const inner = [&](Point2 slot) { return Dist(slot, stack); };
    SortOrder(layout, StandRole::Healer, inner);
    SortOrder(layout, StandRole::Ranged, inner);
}

std::vector<Point2> ChampionLanding(Player* bot)
{
    InstanceScript* instance = bot->GetInstanceScript();
    bool const alliance = instance && instance->GetTeamIdInInstance() == TEAM_ALLIANCE;

    std::vector<Point2> landing;
    for (Point2 const& slot : FACTION_CHAMPION_LANDING)
        landing.push_back({alliance ? slot.x : 2.0f * LOC_CENTER.x - slot.x, slot.y});

    return landing;
}

Point2 ChampionLandingCentroid(std::vector<Point2> const& landing)
{
    Point2 centroid{0.0f, 0.0f};
    for (Point2 const& point : landing)
    {
        centroid.x += point.x;
        centroid.y += point.y;
    }
    centroid.x /= static_cast<float>(landing.size());
    centroid.y /= static_cast<float>(landing.size());
    return centroid;
}

// 0, +3, -3, +6, -6 yd across the ray
float ChampionSideOffset(size_t index)
{
    float const steps = static_cast<float>((index + 1) / 2);
    return (index % 2 ? steps : -steps) * CHAMPION_SIDE_STEP;
}

// First step out along the ray that clears every slot past the tank clearance
Point2 ChampionTankStand(std::vector<Point2> const& landing, Point2 centroid, Vec2 inward, float side)
{
    Point2 const base = Along(centroid, Rotate(inward, 90.0f), side);
    int32 const steps = static_cast<int32>(CHAMPION_TANK_SEARCH_MAX / CHAMPION_TANK_SEARCH_STEP);
    for (int32 step = 0; step < steps; ++step)
    {
        Point2 const stand = Along(base, inward, static_cast<float>(step) * CHAMPION_TANK_SEARCH_STEP);
        if (std::all_of(landing.begin(), landing.end(),
                        [&](Point2 slot) { return Dist(stand, slot) >= CHAMPION_TANK_CLEARANCE; }))
            return stand;
    }

    return Along(base, inward, CHAMPION_TANK_SEARCH_MAX);
}

// Nobody stands in aggro range, so the champions idle until a player engages one. The bot tanks wait
// nearest them on the way to the arena centre and the grip DKs past them.
void LayoutChampions(Player* bot, std::vector<Player*> const& bots, std::vector<DutyStand>& duties, Layout& layout)
{
    std::vector<Point2> const landing = ChampionLanding(bot);
    Point2 const centroid = ChampionLandingCentroid(landing);

    Point2 const centre = PointOf(ARENA_CENTER);
    Vec2 const inward = Direction(centroid, centre);
    std::vector<Point2> tankStands;
    size_t gripPosts = 0;
    for (Player* member : bots)
    {
        if (PlayerbotAI::IsTank(member))
        {
            Point2 const stand = ChampionTankStand(landing, centroid, inward, ChampionSideOffset(tankStands.size()));
            AddForcedDuty(duties, member, "tank", stand, centroid);
            tankStands.push_back(stand);
        }
        else if (member->getClass() == CLASS_DEATH_KNIGHT && member->HasSpell(SPELL_DEATH_GRIP))
        {
            Point2 const post = Along(Along(centroid, inward, CHAMPION_GRIP_POST), Rotate(inward, 90.0f),
                                      ChampionSideOffset(gripPosts++));
            duties.push_back({member->GetGUID(), "grip", At(post, ARENA_STAND_Z), false, Position()});
        }
    }

    auto const behindTanks = [&](Point2 point, Point2 slot)
    {
        if (tankStands.empty())
            return true;

        float nearestTank = Dist(tankStands.front(), slot);
        for (Point2 const& stand : tankStands)
            nearestTank = std::min(nearestTank, Dist(stand, slot));

        return Dist(point, slot) >= nearestTank + CHAMPION_TANK_LEAD;
    };

    for (Point2 const& point : HexLattice(centre, CHAMPION_LATTICE_STEP, centre, WORM_SPOT_FLOOR_RADIUS))
    {
        bool const clear = std::all_of(landing.begin(), landing.end(), [&](Point2 slot)
                                       { return Dist(point, slot) >= CHAMPION_CLEARANCE && behindTanks(point, slot); });
        if (clear)
            AddArenaSlot(layout, point, {StandRole::Healer, StandRole::Ranged, StandRole::Melee});
    }

    Point2 const front = tankStands.empty() ? ChampionTankStand(landing, centroid, inward, 0.0f) : tankStands.front();
    Point2 const healAnchor = Along(front, inward, CHAMPION_HEAL_BEHIND);
    Point2 const playerPost = Along(centroid, inward, CHAMPION_PLAYER_POST);
    auto const inLane = [&](Point2 slot)
    {
        float const along = (slot.x - centroid.x) * inward.x + (slot.y - centroid.y) * inward.y;
        if (along < 0.0f)
            return Dist(slot, centroid) < CHAMPION_GRIP_LANE;
        if (along > CHAMPION_PLAYER_POST)
            return Dist(slot, playerPost) < CHAMPION_GRIP_LANE;

        return std::fabs((slot.x - centroid.x) * inward.y - (slot.y - centroid.y) * inward.x) < CHAMPION_GRIP_LANE;
    };

    // lane slots last
    constexpr float LANE_PENALTY = 1000.0f;
    SortOrder(layout, StandRole::Healer,
              [&](Point2 slot) { return Dist(slot, healAnchor) + (inLane(slot) ? LANE_PENALTY : 0.0f); });
    SortOrder(layout, StandRole::Ranged,
              [&](Point2 slot) { return -Dist(slot, centroid) + (inLane(slot) ? LANE_PENALTY : 0.0f); });
    SortOrder(layout, StandRole::Melee, [&](Point2 slot) { return Dist(slot, centroid); });
}

void LayoutTwins(std::vector<Player*> const& bots, std::vector<DutyStand>& duties, Layout& layout)
{
    Point2 const centre = PointOf(ARENA_CENTER);
    Point2 const fjola = Along(LOC_DARKESS_1, Direction(LOC_DARKESS_1, LOC_VALKYR_DEST_2_RIGHT), TWIN_TANK_STAND);
    Point2 const eydis = Along(LOC_LIGHTESS_1, Direction(LOC_LIGHTESS_1, LOC_VALKYR_DEST_2_LEFT), TWIN_TANK_STAND);
    Point2 const both = Along(LOC_DARKESS_1, Direction(LOC_DARKESS_1, centre), TWIN_TANK_STAND);

    for (Player* member : bots)
    {
        switch (GetTwinTankRole(member))
        {
            case TwinTankRole::Fjola:
                duties.push_back({member->GetGUID(), "fjola", At(fjola, ARENA_STAND_Z), false, Position()});
                break;
            case TwinTankRole::Eydis:
                duties.push_back({member->GetGUID(), "eydis", At(eydis, ARENA_STAND_Z), false, Position()});
                break;
            case TwinTankRole::Both:
                duties.push_back({member->GetGUID(), "both", At(both, ARENA_STAND_Z), false, Position()});
                break;
            default:
                break;
        }
    }

    Point2 const stack = Along(LOC_DARKESS_1, Direction(LOC_DARKESS_1, centre), TWIN_STACK_AHEAD);
    for (Point2 const& point : HexLattice(stack, TWIN_STACK_STEP, stack, TWIN_STACK_RADIUS))
        AddArenaSlot(layout, point, {StandRole::Healer, StandRole::Ranged, StandRole::Melee});

    auto const outer = [&](Point2 slot) { return -Dist(slot, centre); };
    SortOrder(layout, StandRole::Healer, outer);
    SortOrder(layout, StandRole::Ranged, outer);
    SortOrder(layout, StandRole::Melee, [&](Point2 slot) { return Dist(slot, centre); });
}

void LayoutAnubarak(Player* bot, std::vector<DutyStand>& duties, Layout& layout)
{
    Point2 const centre = PointOf(ANUBARAK_ROOM_CENTER);

    AnubarakPrepullTanks const tanks = GetAnubarakPrepullTanks(bot);
    Point2 const pickup = PointOf(GetAnubarakPickupStand(bot));
    if (!tanks.pickup.IsEmpty())
        duties.push_back({tanks.pickup, "pickup", At(pickup, ANUBARAK_STAND_Z), false, Position()});

    char const* const sideNames[2] = {"side0", "side1"};
    for (uint8 side = 0; side < 2; ++side)
        if (!tanks.side[side].IsEmpty())
            duties.push_back({tanks.side[side], sideNames[side],
                              At(PointOf(GetAnubarakTankSidePoint(side)), ANUBARAK_STAND_Z), false, Position()});

    auto const addSlot = [&](Point2 point, std::initializer_list<StandRole> roles)
    {
        for (StandRole role : roles)
            layout.order[static_cast<size_t>(role)].push_back(layout.slots.size());

        layout.slots.push_back(At(point, ANUBARAK_STAND_Z));
    };

    Vec2 const west{-1.0f, 0.0f};
    for (float degrees : ANUBARAK_MELEE_DEGREES)
        addSlot(Along(centre, Rotate(west, degrees), ANUBARAK_MELEE_STAND), {StandRole::Melee});

    Vec2 const east{1.0f, 0.0f};
    for (RingSlot const& slot : ANUBARAK_CASTER_SLOTS)
        addSlot(Along(centre, Rotate(east, slot.degrees), slot.radius), {StandRole::Healer, StandRole::Ranged});
}

char const* ArrivalName(ToCStagingBoss arrival)
{
    switch (arrival)
    {
        case ToCStagingBoss::Gormok:
            return "gormok";
        case ToCStagingBoss::Worms:
            return "worms";
        case ToCStagingBoss::Icehowl:
            return "icehowl";
        case ToCStagingBoss::Jaraxxus:
            return "jaraxxus";
        case ToCStagingBoss::Champions:
            return "champions";
        case ToCStagingBoss::Twins:
            return "twins";
        case ToCStagingBoss::Anubarak:
            return "anubarak";
        default:
            return "none";
    }
}

char const* RoleName(StandRole role)
{
    switch (role)
    {
        case StandRole::Healer:
            return "healer";
        case StandRole::Ranged:
            return "ranged";
        default:
            return "melee";
    }
}

StandRole RoleOf(Player* member)
{
    if (PlayerbotAI::IsHeal(member))
        return StandRole::Healer;

    return PlayerbotAI::IsRangedDps(member) ? StandRole::Ranged : StandRole::Melee;
}

bool IsStagingBot(Player* member, uint32 instanceId)
{
    return member && member->IsInWorld() && member->IsAlive() && member->GetMapId() == TRIAL_OF_THE_CRUSADER_MAP_ID &&
           member->GetInstanceId() == instanceId && GET_PLAYERBOT_AI(member);
}

// Humans can't be moved, so they get no stand. By guid, so every bot deals the same.
std::vector<Player*> StagingBots(Player* bot)
{
    std::vector<Player*> bots;
    uint32 const instanceId = bot->GetInstanceId();
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (IsStagingBot(ref->GetSource(), instanceId))
                bots.push_back(ref->GetSource());
    }
    else if (IsStagingBot(bot, instanceId))
        bots.push_back(bot);

    std::sort(bots.begin(), bots.end(), [](Player* lhs, Player* rhs) { return lhs->GetGUID() < rhs->GetGUID(); });
    return bots;
}

// First in the order among the least taken, so past the end the list starts over
size_t const* PickSlot(std::vector<size_t> const& order, std::vector<uint32> const& taken)
{
    size_t const* best = nullptr;
    for (size_t const& index : order)
        if (!best || taken[index] < taken[*best])
            best = &index;

    return best;
}

void Deal(PlayerbotAI* botAI, StagingState& state, ToCStagingBoss arrival, uint32 now)
{
    state.dealArrival = arrival;
    state.dealMs = now;
    state.dealValid = true;
    state.stands.clear();

    Player* bot = botAI->GetBot();
    InstanceScript* instance = bot->GetInstanceScript();
    uint32 const stage = instance ? instance->GetData(TOC_DATA_INSTANCE_PROGRESS) : 0;
    std::vector<Player*> const bots = StagingBots(bot);

    std::vector<DutyStand> duties;
    Layout layout;
    switch (arrival)
    {
        case ToCStagingBoss::Gormok:
            LayoutGormok(bot, duties, layout);
            break;
        case ToCStagingBoss::Worms:
            LayoutWorms(botAI, duties, layout);
            break;
        case ToCStagingBoss::Icehowl:
            LayoutIcehowl(bot, duties, layout);
            break;
        case ToCStagingBoss::Jaraxxus:
            LayoutJaraxxus(botAI, stage, bots, duties, layout);
            break;
        case ToCStagingBoss::Champions:
            LayoutChampions(bot, bots, duties, layout);
            break;
        case ToCStagingBoss::Twins:
            LayoutTwins(bots, duties, layout);
            break;
        case ToCStagingBoss::Anubarak:
            LayoutAnubarak(bot, duties, layout);
            break;
        default:
            return;
    }

    auto const isBot = [&bots](ObjectGuid const& guid)
    { return std::any_of(bots.begin(), bots.end(), [&guid](Player* member) { return member->GetGUID() == guid; }); };

    for (DutyStand const& duty : duties)
    {
        if (!isBot(duty.guid) || state.stands.count(duty.guid))
            continue;

        DealtStand& dealt = state.stands[duty.guid];
        dealt.stand = duty.stand;
        dealt.role = duty.role;
        dealt.duty = duty.forced;
        dealt.release = duty.release;
    }

    std::vector<uint32> taken(layout.slots.size(), 0);
    for (StandRole role : {StandRole::Healer, StandRole::Ranged, StandRole::Melee})
    {
        for (Player* member : bots)
        {
            if (state.stands.count(member->GetGUID()) || RoleOf(member) != role)
                continue;

            size_t const* pick = PickSlot(layout.order[static_cast<size_t>(role)], taken);
            if (!pick)
                continue;

            ++taken[*pick];
            DealtStand& dealt = state.stands[member->GetGUID()];
            dealt.stand = layout.slots[*pick];
            dealt.role = RoleName(role);
        }
    }
}

StagingState* StateFor(Player* bot)
{
    if (!bot || !bot->IsInWorld() || bot->GetMapId() != TRIAL_OF_THE_CRUSADER_MAP_ID || !bot->GetInstanceId())
        return nullptr;

    return &stagingStates.For(bot->GetInstanceId());
}

Creature* FindBarrett(Player* bot, StagingState& state, uint32 now)
{
    Map* map = bot->GetMap();
    if (!map)
        return nullptr;

    bool const lost = !state.barrett.IsEmpty();
    if (lost)
        if (Creature* barrett = map->GetCreature(state.barrett))
            return barrett;

    if (!lost && state.barrettSearched && getMSTimeDiff(state.barrettSearchMs, now) < BARRETT_SEARCH_MS)
        return nullptr;

    state.barrettSearched = true;
    state.barrettSearchMs = now;
    state.barrett.Clear();

    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, BARRETT_ENTRIES, BARRETT_SEARCH_RADIUS);
    for (Creature* creature : found)
    {
        if (creature->IsAlive())
        {
            state.barrett = creature->GetGUID();
            return creature;
        }
    }

    return nullptr;
}

// A dead or despawned beast's slot keeps its guid until a wipe's cleanup clears it
ToCStagingBoss NextBeast(Map* map, InstanceScript* instance)
{
    auto const upNext = [&](uint32 type)
    {
        ObjectGuid const guid = instance->GetGuidData(type);
        if (guid.IsEmpty())
            return true;

        Creature* beast = map->GetCreature(guid);
        return beast && beast->IsAlive();
    };

    if (upNext(TOC_DATA_GORMOK))
        return ToCStagingBoss::Gormok;

    if (upNext(TOC_DATA_DREADSCALE) || upNext(TOC_DATA_ACIDMAW))
        return ToCStagingBoss::Worms;

    return ToCStagingBoss::Icehowl;
}

ToCStagingBoss ReadArrival(PlayerbotAI* botAI, Player* bot, StagingState& state, uint32 now)
{
    InstanceScript* instance = bot->GetInstanceScript();
    Map* map = bot->GetMap();
    if (!instance || !map)
        return ToCStagingBoss::None;

    uint32 const stage = instance->GetData(TOC_DATA_INSTANCE_PROGRESS);
    bool const nothingLive = ToCLiveEncounter(botAI) == ToCEncounter::None;
    // no Barrett from here on
    if (stage == TOC_PROGRESS_ANUB_ARAK)
        return nothingLive ? ToCStagingBoss::Anubarak : ToCStagingBoss::None;

    ToCEncounter const encounter = ToCEncounterOfStage(stage);
    Creature* barrett = FindBarrett(bot, state, now);
    bool const gossip = barrett && barrett->IsAlive() && barrett->HasNpcFlag(UNIT_NPC_FLAG_GOSSIP);
    if (gossip)
        state.gossipSeenFor = encounter;

    if (encounter == ToCEncounter::None || !nothingLive)
        return ToCStagingBoss::None;

    // stage 3 with nothing live is a Jaraxxus retry: he waits chained for Barrett's gossip, which frees
    // him at once, so stand during it
    bool const retry = stage == TOC_PROGRESS_JARAXXUS_INTRO_DONE;
    // the gossip walks Barrett out and despawns him. After a kill the latch still holds the last
    // encounter till the next stage's Barrett shows up
    if (!retry && (gossip || state.gossipSeenFor != encounter))
        return ToCStagingBoss::None;

    switch (encounter)
    {
        case ToCEncounter::NorthrendBeasts:
            return NextBeast(map, instance);
        case ToCEncounter::Jaraxxus:
            return ToCStagingBoss::Jaraxxus;
        case ToCEncounter::FactionChampions:
            return ToCStagingBoss::Champions;
        case ToCEncounter::TwinValkyr:
            return ToCStagingBoss::Twins;
        default:
            return ToCStagingBoss::None;
    }
}

bool InArrivalRoom(Player* player, ToCStagingBoss arrival)
{
    if (arrival == ToCStagingBoss::Anubarak)
        return player->GetPositionX() > ANUBARAK_ROOM_MIN_X && player->GetPositionZ() < ANUBARAK_ROOM_MAX_Z;

    return player->GetExactDist2d(ARENA_CENTER) <= ARENA_ROOM_RADIUS && player->GetPositionZ() > ARENA_ROOM_MIN_Z;
}

// The master, else the group leader, on this instance
Player* StagingLeader(PlayerbotAI* botAI, Player* bot)
{
    Player* leader = botAI->GetMaster();
    if (!leader)
        if (Group* group = bot->GetGroup())
            leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID());

    if (!leader || !leader->IsInWorld() || leader->GetMapId() != bot->GetMapId() ||
        leader->GetInstanceId() != bot->GetInstanceId())
        return nullptr;

    return leader;
}

DealtStand const* DealtFor(PlayerbotAI* botAI, StagingState& state, ToCStagingBoss arrival)
{
    uint32 const now = getMSTime();
    if (!state.dealValid || state.dealArrival != arrival || getMSTimeDiff(state.dealMs, now) >= STAGING_DEAL_MS)
        Deal(botAI, state, arrival, now);

    auto const itr = state.stands.find(botAI->GetBot()->GetGUID());
    return itr == state.stands.end() ? nullptr : &itr->second;
}

// The essence walk goes first while its portal is up
bool TwinsEssenceFirst(PlayerbotAI* botAI, ToCStagingBoss arrival)
{
    if (arrival != ToCStagingBoss::Twins || !TwinValkyrMustSwapEssence(botAI, false))
        return false;

    return GetEssencePortal(botAI->GetBot(), GetWantedEssence(botAI).colour) != nullptr;
}

// Forced attacks pick the nearest player, so a human between the stand and the release point gets it
bool HumanNearerRelease(Player* bot, DealtStand const& dealt, Position const& spot)
{
    Group* group = bot->GetGroup();
    if (!dealt.duty || !group)
        return false;

    float const standDistance = spot.GetExactDist2d(dealt.release);
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && IsRealPlayer(member) && member->IsAlive() && member->IsInMap(bot) &&
            member->GetExactDist2d(dealt.release) < standDistance)
            return true;
    }

    return false;
}

void NoteStage(Player* bot, StagingState& state, ToCStagingBoss arrival, DealtStand const* dealt,
               Position const& spot)
{
    std::string label = "none";
    if (dealt)
    {
        label = std::string(ArrivalName(arrival)) + "." + dealt->role + " " + RaidObs::DescribeDerived(spot);
        if (HumanNearerRelease(bot, *dealt, spot))
            label += " human";
    }

    StageNote& stageNote = state.stageNotes[bot->GetGUID()];
    stageNote.arrival = arrival;
    stageNote.label = label;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "toc.stage", label);
}

// Any bot's tick writes them all: a bot mid-cast or drinking at the engage runs no triggers
void NoteClosedArrival(Player* bot, StagingState& state)
{
    if (state.closedArrival == ToCStagingBoss::None)
        return;

    if (getMSTimeDiff(state.closedMs, getMSTime()) > STAGING_NOTE_TAIL_MS)
    {
        for (auto itr = state.stageNotes.begin(); itr != state.stageNotes.end();)
            itr = itr->second.arrival == state.closedArrival ? state.stageNotes.erase(itr) : std::next(itr);

        state.closedArrival = ToCStagingBoss::None;
        return;
    }

    if (!RaidObs::Active())
        return;

    for (auto const& [guid, stageNote] : state.stageNotes)
        if (stageNote.arrival == state.closedArrival)
            if (Player* member = ObjectAccessor::GetPlayer(*bot, guid))
                RaidObs::NoteDerived(member, "toc.stage", stageNote.label);
}

ToCStagingRead ReadStand(PlayerbotAI* botAI, bool note)
{
    ToCStagingRead read;
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    StagingState* state = StateFor(bot);
    if (!state)
        return read;

    ToCStagingBoss const arrival = GetToCStagingBoss(botAI);
    if (note)
        NoteClosedArrival(bot, *state);

    if (arrival == ToCStagingBoss::None)
    {
        state->holds.erase(bot->GetGUID());
        return read;
    }

    DealtStand const* dealt = DealtFor(botAI, *state, arrival);
    // the DK tank walks into Death Grip reach of the champion the player called
    bool const chainGrip = arrival == ToCStagingBoss::Champions && FactionChampionsOpenerGripDue(botAI);
    if (!dealt || TwinsEssenceFirst(botAI, arrival) || chainGrip)
    {
        state->holds.erase(bot->GetGUID());
        if (note)
            NoteStage(bot, *state, arrival, nullptr, Position());

        return read;
    }

    StagingHold& hold = state->holds[bot->GetGUID()];
    if (!hold.hasSpot || hold.raw.GetExactDist2d(dealt->stand) > STAGING_STAND_MOVED)
    {
        Position const floor = EncounterHelpers::ValidateFloorPoint(bot, dealt->stand);
        hold.raw = dealt->stand;
        hold.spot = floor.GetExactDist(dealt->stand) <= STAGING_FLOOR_TOLERANCE ? floor : dealt->stand;
        hold.hasSpot = true;
        hold.parked = false;
    }

    float const distance = bot->GetExactDist2d(hold.spot);
    bool const parked = hold.parked ? distance <= STAGING_UNPARK : distance <= STAGING_PARK;

    read.held = true;
    read.walk = !parked;
    read.duty = dealt->duty;
    read.spot = hold.spot;

    if (note)
        NoteStage(bot, *state, arrival, dealt, hold.spot);

    return read;
}
}  // namespace

Position GetFactionChampionsLandingCentre(Player* bot)
{
    Point2 const centroid = ChampionLandingCentroid(ChampionLanding(bot));
    return At(centroid, ARENA_STAND_Z);
}

Position GetFactionChampionsGripPost(Player* bot)
{
    Point2 const centroid = ChampionLandingCentroid(ChampionLanding(bot));
    return At(Along(centroid, Direction(centroid, PointOf(ARENA_CENTER)), CHAMPION_GRIP_POST), ARENA_STAND_Z);
}

ToCStagingBoss GetToCStagedArrival(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    StagingState* state = StateFor(bot);
    if (!state)
        return ToCStagingBoss::None;

    uint32 const now = getMSTime();
    if (state->memoValid && state->memoMs == now)
        return state->arrival;

    ToCStagingBoss const previous = state->arrival;
    state->memoValid = true;
    state->memoMs = now;
    state->arrival = ReadArrival(botAI, bot, *state, now);
    if (previous != ToCStagingBoss::None && state->arrival != previous)
    {
        state->closedArrival = previous;
        state->closedMs = now;
    }

    return state->arrival;
}

ToCStagingBoss GetToCStagingBoss(PlayerbotAI* botAI)
{
    ToCStagingBoss const arrival = GetToCStagedArrival(botAI);
    if (arrival == ToCStagingBoss::None)
        return arrival;

    Player* bot = botAI->GetBot();
    if (!bot->IsAlive() || bot->IsInCombat() || !InArrivalRoom(bot, arrival))
        return ToCStagingBoss::None;

    // A leader who walks out releases the hold
    Player* leader = StagingLeader(botAI, bot);
    return leader && InArrivalRoom(leader, arrival) ? arrival : ToCStagingBoss::None;
}

ToCEncounter ToCStagingEncounter(PlayerbotAI* botAI)
{
    switch (GetToCStagingBoss(botAI))
    {
        case ToCStagingBoss::Gormok:
        case ToCStagingBoss::Worms:
        case ToCStagingBoss::Icehowl:
            return ToCEncounter::NorthrendBeasts;
        case ToCStagingBoss::Jaraxxus:
            return ToCEncounter::Jaraxxus;
        case ToCStagingBoss::Champions:
            return ToCEncounter::FactionChampions;
        case ToCStagingBoss::Twins:
            return ToCEncounter::TwinValkyr;
        case ToCStagingBoss::Anubarak:
            return ToCEncounter::Anubarak;
        default:
            return ToCEncounter::None;
    }
}

ToCStagingRead GetToCStagingRead(PlayerbotAI* botAI) { return ReadStand(botAI, true); }

void SetToCStagingParked(PlayerbotAI* botAI, bool parked)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    StagingState* state = StateFor(bot);
    if (!state)
        return;

    auto const itr = state->holds.find(bot->GetGUID());
    if (itr != state->holds.end())
        itr->second.parked = parked;
}

bool IsToCStagingHeld(PlayerbotAI* botAI) { return ReadStand(botAI, false).held; }

bool IsToCStagingRestHeld(PlayerbotAI* botAI)
{
    ToCStagingRead const read = ReadStand(botAI, false);
    return read.held && (read.walk || read.duty);
}

}
