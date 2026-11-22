#ifndef PLAYERBOTS_RAID_TOCHELPERS_JORMUNGARS_H
#define PLAYERBOTS_RAID_TOCHELPERS_JORMUNGARS_H

#include <functional>
#include <vector>

#include "EncounterHelpers.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "Position.h"
#include "ToCData.h"

namespace TrialOfTheCrusaderHelpers
{

enum class ToCDisplayIds : uint32
{
    MODEL_ACIDMAW_STATIONARY    = 29815,
    MODEL_ACIDMAW_MOBILE        = 29816,
    MODEL_DREADSCALE_STATIONARY = 26935,
    MODEL_DREADSCALE_MOBILE     = 24564,
};

inline constexpr uint32 SPELL_PARALYTIC_TOXIN = 66823;
inline constexpr uint32 SPELL_BURNING_BILE = 66869;
inline constexpr uint32 SPELL_ACIDIC_SPEW = 66818;
inline constexpr uint32 SPELL_MOLTEN_SPEW = 66821;
inline constexpr uint32 SPELL_ACIDIC_SPEW_TICK = 66819;
inline constexpr uint32 SPELL_MOLTEN_SPEW_TICK = 66820;
inline constexpr uint32 SPELL_SLIME_POOL_AURA = 66882;
inline constexpr uint32 SPELL_PARALYTIC_SPRAY = 66901;
inline constexpr uint32 SPELL_BURNING_SPRAY = 66902;

inline constexpr float WORM_MOBILE_COMBAT_REACH = 12.0f;
inline constexpr float WORM_STATIONARY_COMBAT_REACH = 5.0f;
inline constexpr float WORM_MOBILE_MELEE_RANGE = PlayerMeleeRange(WORM_MOBILE_COMBAT_REACH);
inline constexpr float WORM_STATIONARY_MELEE_RANGE = PlayerMeleeRange(WORM_STATIONARY_COMBAT_REACH);
inline constexpr float WORM_MELEE_RING = 9.3f;
inline constexpr float WORM_SPEW_RANGE = 55.0f;
inline constexpr float WORM_SPEW_HIT_RANGE = WORM_SPEW_RANGE + DEFAULT_COMBAT_REACH;
inline constexpr float WORM_SPEW_REACH = WORM_SPEW_HIT_RANGE + 1.0f;
inline constexpr float WORM_FLOOR_RADIUS = 35.0f;
inline constexpr float WORM_SPOT_FLOOR_RADIUS = 44.0f;
inline constexpr float WORM_TAUNT_STAND = 20.0f;
inline constexpr float WORM_TANK_OFFSET = 10.0f;

inline constexpr float WORM_SWEEP_RADIUS = 15.0f;
inline constexpr float WORM_SWEEP_TRIGGER = 16.0f;
inline constexpr float WORM_SWEEP_CLEARANCE = 18.0f;

inline constexpr float WORM_SPLASH_RADIUS = 10.0f;
inline constexpr float WORM_BILE_TRIGGER = 10.5f;
inline constexpr float WORM_BILE_CLEARANCE = 12.0f;
inline constexpr float WORM_SPREAD_TRIGGER = 10.0f;
inline constexpr float WORM_SPREAD_CLEARANCE = 13.0f;
inline constexpr float WORM_SPRAY_TRIGGER = WORM_SPLASH_RADIUS + 2.0f;
inline constexpr float WORM_SPRAY_CLEARANCE = WORM_SPLASH_RADIUS + 3.0f;
inline constexpr float WORM_MELEE_STACK_TRIGGER = WORM_MELEE_RING + WORM_SPREAD_TRIGGER;
inline constexpr float WORM_MELEE_STACK_CLEARANCE = WORM_MELEE_RING + WORM_SPREAD_CLEARANCE;
inline constexpr float WORM_CASTER_CLUMP_CLEARANCE = WORM_MELEE_RING + 2.0f;

inline constexpr float WORM_MELEE_STAND = WORM_MELEE_RING - 1.0f;
inline constexpr float WORM_MELEE_GROUP_DEG = 54.0f;
inline constexpr float WORM_MELEE_REAR_ARC_DEG = 108.0f;

inline constexpr float WORM_CURE_REACH = 6.0f;
inline constexpr float WORM_CURE_TRIGGER = 8.0f;
inline constexpr float WORM_RUN_REACH = 8.5f;
inline constexpr float WORM_RUN_TRIGGER = 9.0f;
inline constexpr float WORM_CURE_NITRO_FROM = 15.0f;
inline constexpr int32 WORM_TOXIN_STUCK_SLOW = -70;
inline constexpr float WORM_TOXIN_HEAL_PCT = 80.0f;

inline constexpr float WORM_HOLD_ARRIVE = 1.0f;
inline constexpr float WORM_HOLD_TRIGGER = 2.5f;
inline constexpr float WORM_HOLD_MELEE_DRIFT = 2.5f;
inline constexpr float WORM_HOLD_MELEE_TURN_DEG = 20.0f;
inline constexpr float WORM_HOLD_DRIFT = 6.0f;
inline constexpr uint32 WORM_HOLD_FRESH_MS = 1000;
inline constexpr uint32 WORM_SEARCH_BACKOFF_MS = 1000;
inline constexpr uint32 WORM_REACH_HOLD_MS = 4000;

inline constexpr uint32 WORM_SETTLE_MS = 500;
inline constexpr float WORM_SETTLE_MOVE = 0.5f;
inline constexpr float WORM_SETTLE_TURN_DEG = 5.0f;

inline constexpr uint32 WORM_SPEW_FIRST_MS = 11000;
inline constexpr uint32 WORM_SPEW_REPEAT_MS = 22000;
inline constexpr uint32 WORM_SPEW_LEAD_MS = 4000;

inline constexpr float WORM_POOL_PATH_PAD = 1.0f;
inline constexpr float WORM_SWEEP_PATH_RADIUS = WORM_SWEEP_TRIGGER;

inline constexpr float WORM_SEARCH_RADIUS = 30.0f;
inline constexpr float WORM_PREFER_JITTER = 0.5f;
inline constexpr uint32 WORM_CURE_DIRECT_BEARINGS = 16;
inline constexpr float WORM_CURE_PARTIAL_STEP = 25.0f;
inline constexpr float WORM_CURE_DETOUR_STEP = 6.0f;

inline constexpr float WORM_SPEW_TRIGGER_PAD = 5.0f * static_cast<float>(M_PI) / 180.0f;
inline constexpr float WORM_SPEW_CLEAR_PAD = 12.0f * static_cast<float>(M_PI) / 180.0f;
inline constexpr float WORM_SPEW_DEFAULT_HALF_ARC = 12.0f * static_cast<float>(M_PI) / 180.0f;

inline constexpr float WORM_POOL_BASE_RADIUS = 2.0f;
inline constexpr float WORM_POOL_GROWTH_PER_TICK = 0.3f;
inline constexpr uint32 WORM_POOL_SOON_TICKS = 5;

inline constexpr float WORM_EDGE_MIN = 36.0f;
inline constexpr float WORM_EDGE_MAX = WORM_SPOT_FLOOR_RADIUS - 2.0f;
inline constexpr float WORM_EDGE_WORM_MIN = WORM_EDGE_MIN - WORM_TANK_OFFSET - 2.0f;
inline constexpr float WORM_POOL_TRIGGER_PAD = 1.0f;
inline constexpr float WORM_POOL_CLEARANCE_PAD = 2.0f;

inline constexpr uint32 WORM_WALK_LATCH_MS = 5000;
inline constexpr uint32 WORM_DRAG_CURE_WAIT_MS = 6000;

inline constexpr float WORM_THREAT_CAP = 0.9f;
inline constexpr float WORM_THREAT_RELEASE = 0.8f;
inline constexpr float WORM_AREA_THREAT_REACH = 30.0f;

static_assert(WORM_SWEEP_RADIUS < WORM_SWEEP_TRIGGER && WORM_SWEEP_TRIGGER < WORM_SWEEP_CLEARANCE, "sweep band");
static_assert(WORM_SPLASH_RADIUS < WORM_BILE_TRIGGER && WORM_BILE_TRIGGER < WORM_BILE_CLEARANCE, "bile band");
static_assert(WORM_SPREAD_TRIGGER < WORM_SPREAD_CLEARANCE, "spread band");
static_assert(WORM_SPRAY_TRIGGER < WORM_SPRAY_CLEARANCE, "spray band");
static_assert(WORM_SPEW_LEAD_MS < WORM_SPEW_FIRST_MS, "spew lead");
static_assert(WORM_EDGE_MIN < WORM_EDGE_MAX, "edge band");
static_assert(WORM_THREAT_RELEASE < WORM_THREAT_CAP && WORM_THREAT_CAP < 1.1f,
              "the brake holds short of the 110% that takes a worm off a tank in melee range");
static_assert(WORM_MELEE_STACK_TRIGGER < WORM_MELEE_STACK_CLEARANCE, "melee stack band");
static_assert(WORM_POOL_TRIGGER_PAD < WORM_POOL_CLEARANCE_PAD, "pool band");
static_assert(WORM_SPEW_TRIGGER_PAD < WORM_SPEW_CLEAR_PAD, "spew band");
static_assert(WORM_CURE_REACH < WORM_CURE_TRIGGER && WORM_CURE_TRIGGER < WORM_SPLASH_RADIUS,
              "a cure walk ends inside its trigger, and the trigger inside the pulse");
static_assert(WORM_CURE_REACH < WORM_RUN_REACH && WORM_RUN_REACH < WORM_RUN_TRIGGER &&
                  WORM_RUN_TRIGGER < WORM_SPLASH_RADIUS,
              "the far run band sits past the cure band and inside the pulse");
static_assert(WORM_CASTER_CLUMP_CLEARANCE < WORM_MELEE_STACK_TRIGGER,
              "the melee group circle sits inside the casters' spread circle on the worm");
static_assert(WORM_HOLD_ARRIVE < WORM_HOLD_TRIGGER, "a walk has to end inside its own trigger");
static_assert(WORM_MELEE_STAND <= WORM_MOBILE_MELEE_RANGE - 1.0f, "the melee stand is inside the role reach");
static_assert(180.0f - WORM_MELEE_GROUP_DEG >= WORM_MELEE_REAR_ARC_DEG, "the dealt stands sit in the rear arc");

bool IsWormSubmerged(Unit* worm);
bool IsWormMobile(Unit* worm);
float WormMeleeRange(Unit* worm);
float WormSpewHalfArc(PlayerbotAI* botAI);
bool IsWormSpewing(PlayerbotAI* botAI, Unit* worm);
bool IsWormSpewingAtOtherTank(PlayerbotAI* botAI, Unit* worm);

struct WormDragPlan
{
    std::vector<EncounterHelpers::HazardCircle> hazards;
    std::vector<EncounterHelpers::HazardCircle> poolsOnly;
    Position preferNear;
    bool edge = false;
};
bool GetMobileWormDrag(PlayerbotAI* botAI, Unit* worm, WormDragPlan& plan);
bool MobileWormDragWaits(PlayerbotAI* botAI);

Unit* GetWormRedirectTarget(PlayerbotAI* botAI, Player*& holder);

bool IsWormThreatBraked(PlayerbotAI* botAI, Unit* unit);
bool IsWormThreatBraking(PlayerbotAI* botAI);
bool IsWormThreatBrakedNear(PlayerbotAI* botAI, Position const& at, float range);

bool IsWormBileCarrierHeldOut(PlayerbotAI* botAI);

// order is the rule priority: FirstFailedRule compares these, the first rule a spot fails wins
enum class WormMoveReason : uint8
{
    None,
    Cure,
    Run,
    Pool,
    Spray,
    Bile,
    Sweep,
    Clump,
    Spew,
    Spread,
    Melee,
    Reach
};
char const* WormMoveReasonName(WormMoveReason reason);

enum class WormSpotTier : uint8
{
    Stand,
    Full,
    Pair,
    NoSpread,
    Partner,
    Direct,
    Detour,
    Partial,
    Escape,
    Loose,
    Cross
};
char const* WormSpotTierName(WormSpotTier tier);
bool WormTierParks(WormSpotTier tier);
bool WormHoldParks(WormMoveReason reason, WormSpotTier tier);
bool IsWormCureReason(WormMoveReason reason);

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
    bool urgent = false;
    bool held = false;
    WormHoldView hold;
    bool escapeFirst = false;
    bool hasStand = false;
    Position stand;
    Position preferNear;
    ObjectGuid partner;
    Position partnerPos;
    float partnerReach = WORM_CURE_REACH;
    std::vector<EncounterHelpers::HazardCircle> circles;
    std::vector<EncounterHelpers::HazardCircle> spreadCircles;
    std::vector<EncounterHelpers::HazardCircle> pairCircles;
    bool pairable = false;
    bool spreadStrict = false;
    std::vector<EncounterHelpers::HazardCircle> urgentCircles;
    std::vector<EncounterHelpers::HazardCircle> escapeCircles;
    std::vector<EncounterHelpers::HazardCircle> looseCircles;
    std::function<bool(float, float)> accept;
    std::function<bool(Position const& via, float, float)> acceptVia;
    std::function<bool(float, float)> escapeAccept;
    std::function<bool(float, float)> crossAccept;
    std::function<bool(float, float)> roleAccept;
};
bool GetWormMovePlan(PlayerbotAI* botAI, WormMovePlan& plan);
bool WormSpotStillSafe(WormMovePlan const& plan, Position const& spot);

void SetWormHold(PlayerbotAI* botAI, Position const& spot, WormMoveReason reason, WormSpotTier tier, bool parked);
bool IsWormSpotHeld(PlayerbotAI* botAI);
bool IsWormWalkInFlight(PlayerbotAI* botAI);
void BackOffWormSearch(PlayerbotAI* botAI);
bool WormReachCrossesPool(PlayerbotAI* botAI);
bool WormReachIntoSpew(PlayerbotAI* botAI);

Player* GetWormToxinHealTarget(PlayerbotAI* botAI);
char const* GetWormToxinSelfPurge(PlayerbotAI* botAI);

std::vector<EncounterHelpers::HazardCircle> GetSlimePoolCircles(PlayerbotAI* botAI, uint32 ticks, float pad);
Position PushOutOfSlimePools(PlayerbotAI* botAI, Position const& spot);
bool InSlimePoolWithoutWorms(PlayerbotAI* botAI);
bool FindSlimePoolExit(PlayerbotAI* botAI, Position& spot);
bool CorpseInSlimePool(Player* dead);

}

#endif
