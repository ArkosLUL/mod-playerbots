#ifndef PLAYERBOTS_RAID_TOCHELPERS_ICEHOWL_H
#define PLAYERBOTS_RAID_TOCHELPERS_ICEHOWL_H

#include "Define.h"
#include "Position.h"
#include "ToCData.h"

class PlayerbotAI;
class Unit;

namespace TrialOfTheCrusaderHelpers
{

// boss_northrend_beasts.cpp IcehowlSpells
constexpr uint32 SPELL_STAGGERED_DAZE = 66758;  // no difficulty row
constexpr uint32 SPELL_FROTHING_RAGE = 66759;
constexpr uint32 SPELL_TRAMPLE = 66734;         // no difficulty row
constexpr uint32 SPELL_ARCTIC_BREATH = 66689;

// Charge geometry from EVENT_JUMP_BACK: he jumps 35 yd back from ARENA_CENTER, then charges through
// it to 50 yd out, 46 when the line's angle falls in (1, 2) rad (the main gate).
constexpr float ICEHOWL_CHARGE_BACK = 35.0f;
constexpr float ICEHOWL_CHARGE_REACH = 50.0f;
constexpr float ICEHOWL_CHARGE_REACH_GATE = 46.0f;
// DoTrampleIfValid: any living player within 12 yd of him, 3D centre to centre
constexpr float ICEHOWL_TRAMPLE_RADIUS = 12.0f;
constexpr float ICEHOWL_CHARGE_TRIGGER = 14.0f;
constexpr float ICEHOWL_CHARGE_CLEARANCE = 16.0f;
constexpr float ICEHOWL_CHARGE_TIGHT_CLEARANCE = 13.0f;
// He lands on ARENA_CENTER exactly, this only absorbs float noise
constexpr float ICEHOWL_CENTRE_TOLERANCE = 3.0f;

// Arctic Breath's bisector runs through its target, so a neighbour is hit only within half the cone
// of that bearing, at any range. Bots spread by bearing, one step being half the cone plus the margin.
constexpr float ICEHOWL_BREATH_DEFAULT_CONE_DEG = 24.0f;  // TARGET_UNIT_CONE_ENEMY_24, no spell_cone row
constexpr float ICEHOWL_SPREAD_MARGIN_DEG = 6.0f;
// A bot this far off its bearing walks back, so two neighbours off toward each other still sit
// outside a breath on either
constexpr float ICEHOWL_SPREAD_TRIGGER_DEG = 3.0f;
constexpr float ICEHOWL_SPREAD_ARRIVE_DEG = 1.5f;
constexpr float ICEHOWL_SPREAD_HALF_ARC_DEG = 90.0f;      // bearings kept either side of his back
// A healer here at 17 yd is ~27 yd from his tank, ~30 straight behind
constexpr float ICEHOWL_SPREAD_HEALER_BEARING_DEG = 60.0f;
// Whirl, and a hunter's 5 yd minimum range past his melee range (reaches 12 and 1.5 plus 4/3), all
// centre to centre
constexpr float ICEHOWL_MELEE_RANGE = 12.0f + 1.5f + 4.0f / 3.0f;
constexpr float ICEHOWL_WHIRL_RADIUS = 15.0f;
constexpr float ICEHOWL_RANGED_MIN_RANGE = ICEHOWL_MELEE_RANGE + 5.0f;
// Radius bands from his centre
constexpr float ICEHOWL_SPREAD_MELEE_MIN = 8.0f;
constexpr float ICEHOWL_SPREAD_MELEE_MAX = 13.0f;
constexpr float ICEHOWL_SPREAD_HEALER_MIN = 16.5f;
constexpr float ICEHOWL_SPREAD_HEALER_MAX = 18.5f;
constexpr float ICEHOWL_SPREAD_RANGED_MIN = 21.5f;
constexpr float ICEHOWL_SPREAD_RANGED_MAX = 25.0f;
// How far outside its band a bot may stand before it walks back
constexpr float ICEHOWL_SPREAD_TRIGGER = 1.0f;
constexpr float ICEHOWL_SPREAD_ARRIVE = 0.5f;
// A bearing walled closer than this is dropped
constexpr float ICEHOWL_SPREAD_MIN_ROOM = 22.5f;
// Whirl throws his tank ~31 yd and he follows, so the layout re-latches when he moves or turns this far
constexpr float ICEHOWL_SPREAD_RELATCH_MOVE = 8.0f;
constexpr float ICEHOWL_SPREAD_RELATCH_TURN_DEG = 30.0f;
// Past the ranged band by his drift before a re-latch, so an open bearing never reads as walled
constexpr float ICEHOWL_SPREAD_PROBE = ICEHOWL_SPREAD_RANGED_MAX + ICEHOWL_SPREAD_RELATCH_MOVE + 1.0f;
// Melee bearings stay this far round from his back. Past 90° off it he parries them and hastes his
// next swing, and a stand error or his turn before a re-latch must not take them there.
constexpr float ICEHOWL_SPREAD_MELEE_ARC_DEG = 90.0f - ICEHOWL_SPREAD_RELATCH_TURN_DEG - ICEHOWL_SPREAD_TRIGGER_DEG;

static_assert(2.0f * ICEHOWL_SPREAD_TRIGGER_DEG <= ICEHOWL_SPREAD_MARGIN_DEG,
              "neighbours stay out of each other's breath");
static_assert(ICEHOWL_SPREAD_ARRIVE_DEG < ICEHOWL_SPREAD_TRIGGER_DEG && ICEHOWL_SPREAD_ARRIVE < ICEHOWL_SPREAD_TRIGGER,
              "a walk has to end inside its own trigger");
static_assert(ICEHOWL_SPREAD_MELEE_MAX + ICEHOWL_SPREAD_TRIGGER < ICEHOWL_MELEE_RANGE, "melee stay in reach");
static_assert(ICEHOWL_SPREAD_HEALER_MIN - ICEHOWL_SPREAD_TRIGGER > ICEHOWL_WHIRL_RADIUS, "healers stay out of Whirl");
static_assert(ICEHOWL_SPREAD_RANGED_MIN - ICEHOWL_SPREAD_TRIGGER > ICEHOWL_RANGED_MIN_RANGE,
              "hunters stay past their minimum range");
static_assert(ICEHOWL_SPREAD_MIN_ROOM - 1.0f >= ICEHOWL_SPREAD_RANGED_MIN, "a kept bearing fits the ranged band");

// A bot's Arctic Breath stand in Icehowl's frame: a breath aims by bearing, so a bot holds its stand
// while it stays near the bearing from him and inside the radius band.
struct IcehowlBreathStand
{
    Position spot;
    Position icehowl;
    float bearing = 0.0f;  // radians, from him
    float bandMin = 0.0f;
    float bandMax = 0.0f;  // capped by the room along the bearing
};

// The charge line, from where the jump back lands to where the charge stops. Latched from the gaze
// to the end of the charge, read once per instance per ms. False off map 649 and outside the charge.
bool IcehowlChargeLatched(PlayerbotAI* botAI, Position& start, Position& end);

// 2D distance from (x, y) to the latched line, FLT_MAX when nothing is latched.
float DistanceToIcehowlCharge(PlayerbotAI* botAI, float x, float y);

// Same, for a caller already holding the line.
float DistanceToIcehowlLane(Position const& start, Position const& end, float x, float y);

// The wall crash: 15 s stunned, +100% damage taken
bool IsIcehowlStaggered(Unit* icehowl);

bool HasIcehowlFrothingRage(Unit* icehowl);

// The bot's Arctic Breath stand: Icehowl's spot plus its own distance to him, clamped into its role's
// band, along its bearing. False while the spread is off (another beast up, a charge, his victim out
// of his melee range) and for a bot with no bearing: tanks, humans, one the deal hasn't reached yet,
// melee with every bearing near his back walled.
bool GetIcehowlBreathStand(PlayerbotAI* botAI, IcehowlBreathStand& stand);

// (x, y) within `degrees` of the stand's bearing and `yards` of its band
bool IsOnIcehowlBreathStand(IcehowlBreathStand const& stand, float x, float y, float degrees, float yards);

bool HasIcehowlBreathSlot(PlayerbotAI* botAI);

}

#endif
