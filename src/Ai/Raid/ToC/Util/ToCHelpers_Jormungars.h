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
constexpr uint32 SPELL_WORM_ENRAGE = 68335;       // no row, the survivor's once the other worm dies

constexpr float WORM_MELEE_RANGE = 9.3f;          // reach 6.5 + 1.5 + 4/3
constexpr float WORM_SPEW_RANGE = 55.0f;
constexpr float WORM_FLOOR_RADIUS = 35.0f;        // the worms surface up to 35 yd from ARENA_CENTER
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
constexpr float WORM_SPREAD_TRIGGER = 9.0f;
constexpr float WORM_SPREAD_CLEARANCE = 11.0f;

// A Toxin carrier stands this close to a Bile carrier, well inside the pulse
constexpr float WORM_CURE_REACH = 6.0f;
constexpr float WORM_CURE_TRIGGER = 8.0f;
// Toxin's snare amount (EFFECT_0) at which its carrier waits for a runner instead of walking
constexpr int32 WORM_TOXIN_STUCK_SLOW = -70;

// Spew pads past the half-arc: stepping out triggers at the first, and the spot clears the second
constexpr float WORM_SPEW_TRIGGER_PAD = 5.0f * static_cast<float>(M_PI) / 180.0f;
constexpr float WORM_SPEW_CLEAR_PAD = 12.0f * static_cast<float>(M_PI) / 180.0f;
// TARGET_UNIT_CONE_ENEMY_24, for the ids spell_cone has no row for
constexpr float WORM_SPEW_DEFAULT_HALF_ARC = 12.0f * static_cast<float>(M_PI) / 180.0f;

// Slime Pool radius is 2 + 0.3 x tick, 11 at the end, centre to centre
constexpr float WORM_POOL_BASE_RADIUS = 2.0f;
constexpr float WORM_POOL_GROWTH_PER_TICK = 0.3f;
constexpr float WORM_POOL_MAX_RADIUS = 11.0f;
constexpr uint32 WORM_POOL_SOON_TICKS = 5;
constexpr float WORM_POOL_TRIGGER_PAD = 1.0f;    // on the radius now
constexpr float WORM_POOL_CLEARANCE_PAD = 2.0f;  // on the radius 5 s on

// A reposition walk holds the tick at most this long, so a bot rooted mid walk still gets a new spot
constexpr uint32 WORM_WALK_LATCH_MS = 5000;

bool IsWormSubmerged(Unit* worm);                 // UNIT_FLAG_NOT_SELECTABLE, submerge start to emerge
// Display form. Under ground, the form it comes up in once the other worm is under too; while the
// other is still up, the opposite of that one's. An enraged survivor comes up mobile.
bool IsWormMobile(Unit* worm);
float WormSpewHalfArc(PlayerbotAI* botAI);        // radians, half the cone the remapped tick uses

struct WormDragPlan
{
    std::vector<EncounterHelpers::HazardCircle> hazards;    // pools and the stationary worm
    std::vector<EncounterHelpers::HazardCircle> poolsOnly;  // fallback sweep
    Position preferNear;                                     // 10 yd on from the tank, away from the worm
};
// The mobile worm this bot holds must be walked out: true fills the plan for the tank's own spot
bool GetMobileWormDrag(PlayerbotAI* botAI, Unit* worm, WormDragPlan& plan);

// The worm this hunter or rogue redirects onto its duty holder right now, else nullptr
Unit* GetWormRedirectTarget(PlayerbotAI* botAI, Player*& holder);

// A Burning Bile carrier the reposition keeps off the raid: no Beasts tank duty, not sent as a runner
bool IsWormBileCarrierHeldOut(PlayerbotAI* botAI);

enum class WormMoveReason : uint8
{
    None,
    Cure,
    Run,
    Pool,
    Bile,
    Sweep,
    Spew,
    Spread
};
char const* WormMoveReasonName(WormMoveReason reason);  // "cure", "run", "pool", ...
struct WormMovePlan
{
    WormMoveReason reason = WormMoveReason::None;
    bool urgent = false;  // damage landing on the bot's spot now: forced move, break a pinning cast
    Position preferNear;
    ObjectGuid partner;   // Cure: the Bile carrier; Run: the Toxin carrier
    std::vector<EncounterHelpers::HazardCircle> circles;        // every clearance that applies
    std::vector<EncounterHelpers::HazardCircle> urgentCircles;  // pools, Bile, Sweep while cast
    // The urgent escape's: urgentCircles, plus on a run the other Bile carriers the walk ignores
    std::vector<EncounterHelpers::HazardCircle> escapeCircles;
    std::function<bool(float, float)> accept;        // floor, Spew cones, partner reach
    std::function<bool(float, float)> escapeAccept;  // accept without the partner reach
    std::function<bool(float, float)> roleAccept;    // reach of the bot's target, or of both holders
};
// Reason None when the bot's spot is fine. Trigger and action both read this. Notes nb.cure.
bool GetWormMovePlan(PlayerbotAI* botAI, WormMovePlan& plan);
// urgentCircles, and accept (escapeAccept while urgent)
bool WormSpotStillSafe(WormMovePlan const& plan, Position const& spot);

// The reposition walk, shared by the action that issues it and the guard that protects it
void SetWormWalk(Player* bot, Position const& spot);
void ClearWormWalk(Player* bot);
bool GetWormWalk(Player* bot, Position& spot, uint32& issuedMs);
bool IsWormWalkInFlight(PlayerbotAI* botAI);  // latched, under 5 s old, moving, `last movement` on it

}

#endif
