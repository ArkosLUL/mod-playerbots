#ifndef PLAYERBOTS_RAID_TOCHELPERS_JORMUNGARS_H
#define PLAYERBOTS_RAID_TOCHELPERS_JORMUNGARS_H

#include <functional>
#include <vector>

#include "EncounterHelpers.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "Position.h"
#include "ToCData.h"

// Acidmaw and Dreadscale. Worm state, pools and the cure pairing are read once per instance per ms,
// only on map 649 while the Beasts encounter is live and a worm is engaged.
namespace TrialOfTheCrusaderHelpers
{

// Worm display ids per mobility form (boss_northrend_beasts.cpp Model)
enum class ToCDisplayIds : uint32
{
    MODEL_ACIDMAW_STATIONARY    = 29815,
    MODEL_ACIDMAW_MOBILE        = 29816,
    MODEL_DREADSCALE_STATIONARY = 26935,
    MODEL_DREADSCALE_MOBILE     = 24564,
};

// 10N ids; remap at the call where a row exists
constexpr uint32 SPELL_PARALYTIC_TOXIN = 66823;   // row 605
constexpr uint32 SPELL_BURNING_BILE = 66869;      // no row
constexpr uint32 SPELL_ACIDIC_SPEW = 66818;       // no row: the cast and the worm's 2.5 s aura
constexpr uint32 SPELL_MOLTEN_SPEW = 66821;       // no row
constexpr uint32 SPELL_ACIDIC_SPEW_TICK = 66819;  // row 602
constexpr uint32 SPELL_MOLTEN_SPEW_TICK = 66820;  // row 611
constexpr uint32 SPELL_SLIME_POOL_AURA = 66882;   // no row, one tick a second grows the pool
constexpr uint32 SPELL_PARALYTIC_SPRAY = 66901;   // row 604
constexpr uint32 SPELL_BURNING_SPRAY = 66902;     // row 608

constexpr float WORM_MELEE_RANGE = 9.3f;          // reach 6.5 + 1.5 + 4/3
constexpr float WORM_SPEW_RANGE = 55.0f;
constexpr float WORM_FLOOR_RADIUS = 35.0f;        // the worms surface up to 35 yd from ARENA_CENTER
// Bot spots only. 2 yd inside Icehowl's gate-side charge end, so the floor is known to reach it.
constexpr float WORM_SPOT_FLOOR_RADIUS = 44.0f;
constexpr float WORM_TAUNT_STAND = 20.0f;         // the mobile holder's reach under ground
constexpr float WORM_TANK_OFFSET = 8.5f;          // worm centre to its victim: chase stops at contact 0.5 + 6.5 + 1.5

// Sweep: a 15 yd circle round the stationary worm, centre to centre
constexpr float WORM_SWEEP_RADIUS = 15.0f;
constexpr float WORM_SWEEP_TRIGGER = 16.0f;
constexpr float WORM_SWEEP_CLEARANCE = 18.0f;

// Burning Bile's pulse and the Spray's splash, both 10 yd centre to centre
constexpr float WORM_SPLASH_RADIUS = 10.0f;
constexpr float WORM_BILE_TRIGGER = 10.5f;
constexpr float WORM_BILE_CLEARANCE = 12.0f;
constexpr float WORM_SPREAD_TRIGGER = 10.0f;
constexpr float WORM_SPREAD_CLEARANCE = 13.0f;
// Spray's splash lands where its target stands as the cast ends
constexpr float WORM_SPRAY_TRIGGER = WORM_SPLASH_RADIUS + 2.0f;
constexpr float WORM_SPRAY_CLEARANCE = WORM_SPLASH_RADIUS + 3.0f;
// Casters keep off the melee round the up mobile worm as one circle on it
constexpr float WORM_MELEE_STACK_TRIGGER = WORM_MELEE_RANGE + WORM_SPREAD_TRIGGER;
constexpr float WORM_MELEE_STACK_CLEARANCE = WORM_MELEE_RANGE + WORM_SPREAD_CLEARANCE;

// Dealt melee stands on the mobile worm
constexpr float WORM_MELEE_STAND = WORM_MELEE_RANGE - 1.0f;  // from its centre
constexpr float WORM_MELEE_GROUP_DEG = 54.0f;                // either side of its back: groups 13.4 yd apart
constexpr float WORM_MELEE_REAR_ARC_DEG = 108.0f;            // least angle off its facing for a melee spot

// A Toxin carrier stands this close to a Bile carrier, well inside the pulse
constexpr float WORM_CURE_REACH = 6.0f;
constexpr float WORM_CURE_TRIGGER = 8.0f;
// Toxin's snare amount (EFFECT_0) at which its carrier waits for a runner instead of walking
constexpr int32 WORM_TOXIN_STUCK_SLOW = -70;
// A carrier no Bile can cure gets its healer's heal under this health
constexpr float WORM_TOXIN_HEAL_PCT = 80.0f;

// Held spots
constexpr float WORM_HOLD_ARRIVE = 1.0f;
constexpr float WORM_HOLD_TRIGGER = 2.5f;           // a parked bot pushed past this walks back
constexpr float WORM_HOLD_MELEE_DRIFT = 2.5f;       // anchor worm moved, melee hold
constexpr float WORM_HOLD_MELEE_TURN_DEG = 20.0f;   // anchor worm turned, melee hold
constexpr float WORM_HOLD_DRIFT = 6.0f;             // anchor worm moved, any other hold
constexpr uint32 WORM_HOLD_FRESH_MS = 1000;         // a hold the plan hasn't confirmed this long guards nothing
constexpr uint32 WORM_SEARCH_BACKOFF_MS = 1000;     // after a non-urgent search found no spot

// A worm counts as settled once it hasn't walked or turned past these for WORM_SETTLE_MS
constexpr uint32 WORM_SETTLE_MS = 500;
constexpr float WORM_SETTLE_MOVE = 0.5f;
constexpr float WORM_SETTLE_TURN_DEG = 5.0f;

// The script's Spew: 11 s after the engage, at once on a mobile emerge, then 22 s after each cast start
constexpr uint32 WORM_SPEW_FIRST_MS = 11000;
constexpr uint32 WORM_SPEW_REPEAT_MS = 22000;
constexpr uint32 WORM_SPEW_LEAD_MS = 4000;

// Path test
constexpr float WORM_POOL_PATH_PAD = 1.0f;
constexpr float WORM_SWEEP_PATH_RADIUS = WORM_SWEEP_TRIGGER;

// Search
constexpr float WORM_SEARCH_RADIUS = 30.0f;
constexpr float WORM_PREFER_JITTER = 0.5f;
constexpr uint32 WORM_CURE_DIRECT_BEARINGS = 16;
constexpr float WORM_CURE_PARTIAL_STEP = 25.0f;

// Spew pads past the half-arc: stepping out triggers at the first, and the spot clears the second
constexpr float WORM_SPEW_TRIGGER_PAD = 5.0f * static_cast<float>(M_PI) / 180.0f;
constexpr float WORM_SPEW_CLEAR_PAD = 12.0f * static_cast<float>(M_PI) / 180.0f;
// TARGET_UNIT_CONE_ENEMY_24, for the ids spell_cone has no row for
constexpr float WORM_SPEW_DEFAULT_HALF_ARC = 12.0f * static_cast<float>(M_PI) / 180.0f;

// Slime Pool radius is 2 + 0.3 x tick, centre to centre, till the pool despawns: 30 s on normal (11 yd),
// 45 s on 10H, 60 s on 25H (20 yd)
constexpr float WORM_POOL_BASE_RADIUS = 2.0f;
constexpr float WORM_POOL_GROWTH_PER_TICK = 0.3f;
constexpr uint32 WORM_POOL_SOON_TICKS = 5;

// Heroic pools outlive the 12 s between them, so the mobile holder walks its worm round the edge and
// they line the wall: its drag looks on this band first, 2 yd inside the spot floor
constexpr float WORM_EDGE_MIN = 36.0f;
constexpr float WORM_EDGE_MAX = WORM_SPOT_FLOOR_RADIUS - 2.0f;
// Further in, a heroic worm gets walked out before its next pool. It stops 8.5 yd short of its tank, so
// one walked out from the middle settles past this.
constexpr float WORM_EDGE_WORM_MIN = WORM_EDGE_MIN - WORM_TANK_OFFSET - 2.0f;
constexpr float WORM_POOL_TRIGGER_PAD = 1.0f;    // on the radius now
constexpr float WORM_POOL_CLEARANCE_PAD = 2.0f;  // on the radius 5 s on

// A reposition walk holds the tick at most this long, so a bot rooted mid walk still gets a new spot
constexpr uint32 WORM_WALK_LATCH_MS = 5000;

static_assert(WORM_SWEEP_RADIUS < WORM_SWEEP_TRIGGER && WORM_SWEEP_TRIGGER < WORM_SWEEP_CLEARANCE, "sweep band");
static_assert(WORM_SPLASH_RADIUS < WORM_BILE_TRIGGER && WORM_BILE_TRIGGER < WORM_BILE_CLEARANCE, "bile band");
static_assert(WORM_SPREAD_TRIGGER < WORM_SPREAD_CLEARANCE, "spread band");
static_assert(WORM_SPRAY_TRIGGER < WORM_SPRAY_CLEARANCE, "spray band");
static_assert(WORM_SPEW_LEAD_MS < WORM_SPEW_FIRST_MS, "spew lead");
static_assert(WORM_EDGE_MIN < WORM_EDGE_MAX, "edge band");
static_assert(WORM_MELEE_STACK_TRIGGER < WORM_MELEE_STACK_CLEARANCE, "melee stack band");
static_assert(WORM_POOL_TRIGGER_PAD < WORM_POOL_CLEARANCE_PAD, "pool band");
static_assert(WORM_SPEW_TRIGGER_PAD < WORM_SPEW_CLEAR_PAD, "spew band");
static_assert(WORM_CURE_REACH < WORM_CURE_TRIGGER && WORM_CURE_TRIGGER < WORM_SPLASH_RADIUS,
              "a cure walk ends inside its trigger, and the trigger inside the pulse");
static_assert(WORM_HOLD_ARRIVE < WORM_HOLD_TRIGGER, "a walk has to end inside its own trigger");
static_assert(WORM_MELEE_STAND <= WORM_MELEE_RANGE - 1.0f, "the melee stand is inside the role reach");
static_assert(180.0f - WORM_MELEE_GROUP_DEG >= WORM_MELEE_REAR_ARC_DEG, "the dealt stands sit in the rear arc");

bool IsWormSubmerged(Unit* worm);                 // UNIT_FLAG_NOT_SELECTABLE, submerge start to emerge
// Display form. Under ground, the form it comes up in once the other worm is under too; while the
// other is still up, the opposite of that one's. A survivor flips at every emerge too.
bool IsWormMobile(Unit* worm);
float WormSpewHalfArc(PlayerbotAI* botAI);        // radians, half the cone the remapped tick uses

struct WormDragPlan
{
    std::vector<EncounterHelpers::HazardCircle> hazards;    // pools and the stationary worm
    std::vector<EncounterHelpers::HazardCircle> poolsOnly;  // fallback sweep
    Position preferNear;                                     // 10 yd on from the tank, away from the worm
    bool edge = false;                                       // heroic: the WORM_EDGE band first
};
// The mobile worm this bot holds must be walked out: true fills the plan for the tank's own spot
bool GetMobileWormDrag(PlayerbotAI* botAI, Unit* worm, WormDragPlan& plan);

// The worm this hunter or rogue redirects onto its duty holder right now, else nullptr
Unit* GetWormRedirectTarget(PlayerbotAI* botAI, Player*& holder);

// A Burning Bile carrier the reposition keeps off the raid: no Beasts tank duty, not sent as a runner
bool IsWormBileCarrierHeldOut(PlayerbotAI* botAI);

// In priority order: the first rule whose trigger the bot's spot fails names the move
enum class WormMoveReason : uint8
{
    None,
    Cure,
    Run,
    Pool,
    Spray,
    Bile,
    Sweep,
    Spew,
    Spread,
    Melee,
    Reach
};
char const* WormMoveReasonName(WormMoveReason reason);  // "cure", "run", "pool", ...

// The search that found a spot. Escape, loose, partial and cross spots are walked to, never parked on.
enum class WormSpotTier : uint8
{
    Stand,
    Full,
    Pair,  // spread from every caster but one with no other neighbour
    NoSpread,
    Partner,
    Direct,
    Partial,
    Escape,
    Loose,
    Cross  // urgent and nothing else found: the walk may cross a pool or Sweep
};
char const* WormSpotTierName(WormSpotTier tier);  // "stand", "full", "pair", "nospread", ...
bool WormTierParks(WormSpotTier tier);
// A Spray dodge lets go once it arrives, so melee walk back to their stand after the cast
bool WormHoldParks(WormMoveReason reason, WormSpotTier tier);
bool IsWormCureReason(WormMoveReason reason);     // cure or run

struct WormHoldView
{
    Position spot;
    WormMoveReason reason = WormMoveReason::None;
    WormSpotTier tier = WormSpotTier::Full;
    uint32 issuedMs = 0;
    bool parked = false;
};

struct WormMovePlan
{
    WormMoveReason reason = WormMoveReason::None;
    bool urgent = false;        // damage landing on the bot's spot now: forced move, break a pinning cast
    bool held = false;          // a valid hold: walk back to hold.spot, never search
    WormHoldView hold;
    bool escapeFirst = false;   // inside a pool: the escape tier goes first
    bool hasStand = false;      // Melee: the dealt stand
    Position stand;
    Position preferNear;
    ObjectGuid partner;         // Cure: the Bile carrier; Run: the Toxin carrier
    Position partnerPos;
    std::vector<EncounterHelpers::HazardCircle> circles;        // hard: pools, Spray, Bile, Sweep, stomp, own feet
    std::vector<EncounterHelpers::HazardCircle> spreadCircles;  // casters' spread and the melee stack
    // spreadCircles less the pair partner's; empty when no peer can take a partner
    std::vector<EncounterHelpers::HazardCircle> pairCircles;
    bool pairable = false;      // there is a pair partner, even when its circle was the only spread one
    bool spreadStrict = false;  // heroic with Dreadscale stationary: no nospread tier
    std::vector<EncounterHelpers::HazardCircle> urgentCircles;  // pools, Spray, Bile, Sweep while cast
    // urgentCircles, a run's other Bile carriers, a held-out carrier's clean bots
    std::vector<EncounterHelpers::HazardCircle> escapeCircles;
    std::vector<EncounterHelpers::HazardCircle> looseCircles;   // escapeCircles less those clean bots
    std::function<bool(float, float)> accept;        // floor, Spew cones, path, partner reach, melee rear arc
    std::function<bool(float, float)> escapeAccept;  // floor, Spew cones, path
    std::function<bool(float, float)> crossAccept;   // floor, Spew cones
    std::function<bool(float, float)> roleAccept;    // the role's reach, in combat-reach terms
};
// False when the bot has no worm plan at all; reason None when its spot is fine or held. Trigger and
// action both read it. Notes nb.cure and nb.wormhold; parks and releases holds.
bool GetWormMovePlan(PlayerbotAI* botAI, WormMovePlan& plan);
// urgentCircles, then escapeAccept while urgent, else accept
bool WormSpotStillSafe(WormMovePlan const& plan, Position const& spot);

// The spot the action just walked the bot to, parked when it already stands there. Notes `set`.
void SetWormHold(PlayerbotAI* botAI, Position const& spot, WormMoveReason reason, WormSpotTier tier, bool parked);
bool IsWormSpotHeld(PlayerbotAI* botAI);        // a hold the plan confirmed within WORM_HOLD_FRESH_MS
bool IsWormWalkInFlight(PlayerbotAI* botAI);    // unparked hold, under WORM_WALK_LATCH_MS, moving, booked on it
void BackOffWormSearch(PlayerbotAI* botAI);     // the plan stays quiet on non-urgent reasons for the backoff
// reach melee or reach spell would walk this bot through a pool, and the reposition can still find a
// way round: not while its search backs off, nor past the search radius
bool WormReachCrossesPool(PlayerbotAI* botAI);

// Toxin no Bile can cure: no worm is engaged, so the cure pairing is off, or nobody carries Bile. Only
// a Bile pulse removes it, and it ticks on to 60 s, so a lone survivor's Spray kills unhealed. Each such
// carrier gets one healer bot, nearest first, latched till its Toxin ends, while the Beasts gate is open.
Player* GetWormToxinHealTarget(PlayerbotAI* botAI);  // the carrier this healer bot keeps up, else nullptr
// This non-tank bot's own such Toxin has it stuck: "divine shield" or "ice block", whose school immunity
// purges it, else nullptr. Before Paralysis, which leaves the bot unable to cast anything.
char const* GetWormToxinSelfPurge(PlayerbotAI* botAI);

}

#endif
