#ifndef PLAYERBOTS_RAID_TOCHELPERS_ICEHOWL_H
#define PLAYERBOTS_RAID_TOCHELPERS_ICEHOWL_H

#include "Define.h"
#include "Position.h"
#include "ToCData.h"

class PlayerbotAI;
class Unit;

namespace TrialOfTheCrusaderHelpers
{

inline constexpr uint32 SPELL_STAGGERED_DAZE = 66758;
inline constexpr uint32 SPELL_FROTHING_RAGE = 66759;
inline constexpr uint32 SPELL_TRAMPLE = 66734;
inline constexpr uint32 SPELL_ARCTIC_BREATH = 66689;

inline constexpr float ICEHOWL_CHARGE_BACK = 35.0f;
inline constexpr float ICEHOWL_CHARGE_REACH = 50.0f;
inline constexpr float ICEHOWL_CHARGE_REACH_GATE = 46.0f;
inline constexpr float ICEHOWL_TRAMPLE_RADIUS = 12.0f;
inline constexpr float ICEHOWL_CHARGE_TRIGGER = 14.0f;
inline constexpr float ICEHOWL_CHARGE_CLEARANCE = 16.0f;
inline constexpr float ICEHOWL_CHARGE_TIGHT_CLEARANCE = 13.0f;
inline constexpr float ICEHOWL_CENTRE_TOLERANCE = 3.0f;

inline constexpr float ICEHOWL_BREATH_DEFAULT_CONE_DEG = 24.0f;
inline constexpr float ICEHOWL_SPREAD_MARGIN_DEG = 6.0f;
inline constexpr float ICEHOWL_SPREAD_TRIGGER_DEG = 3.0f;
inline constexpr float ICEHOWL_SPREAD_ARRIVE_DEG = 1.5f;
inline constexpr float ICEHOWL_SPREAD_HALF_ARC_DEG = 90.0f;
inline constexpr float ICEHOWL_SPREAD_HEALER_BEARING_DEG = 60.0f;
inline constexpr float ICEHOWL_COMBAT_REACH = 14.0f;
inline constexpr float ICEHOWL_MELEE_RANGE = PlayerMeleeRange(ICEHOWL_COMBAT_REACH);
inline constexpr float ICEHOWL_WHIRL_RADIUS = 15.0f;
inline constexpr float ICEHOWL_RANGED_MIN_RANGE = ICEHOWL_MELEE_RANGE + 5.0f;
inline constexpr float ICEHOWL_SPREAD_MELEE_MIN = 15.5f;
inline constexpr float ICEHOWL_SPREAD_MELEE_MAX = 16.2f;
inline constexpr float ICEHOWL_SPREAD_HEALER_MIN = 16.5f;
inline constexpr float ICEHOWL_SPREAD_HEALER_MAX = 18.5f;
inline constexpr float ICEHOWL_SPREAD_RANGED_MIN = 23.0f;
inline constexpr float ICEHOWL_SPREAD_RANGED_MAX = 26.5f;
inline constexpr float ICEHOWL_SPREAD_TRIGGER = 1.0f;
inline constexpr float ICEHOWL_SPREAD_ARRIVE = 0.5f;
inline constexpr float ICEHOWL_SPREAD_MELEE_TRIGGER = 0.3f;
inline constexpr float ICEHOWL_SPREAD_MELEE_ARRIVE = 0.15f;
inline constexpr float ICEHOWL_SPREAD_MIN_ROOM = 24.0f;
inline constexpr float ICEHOWL_SPREAD_MELEE_ROOM = 17.5f;
inline constexpr float ICEHOWL_SPREAD_RELATCH_MOVE = 8.0f;
inline constexpr float ICEHOWL_SPREAD_RELATCH_TURN_DEG = 30.0f;
inline constexpr uint32 ICEHOWL_SPREAD_VICTIM_GRACE_MS = 1000;
inline constexpr uint32 ICEHOWL_SPREAD_SETTLE_MS = 500;
inline constexpr float ICEHOWL_SPREAD_PROBE = ICEHOWL_SPREAD_RANGED_MAX + ICEHOWL_SPREAD_RELATCH_MOVE + 1.0f;
inline constexpr float ICEHOWL_SPREAD_MELEE_ARC_DEG =
    90.0f - ICEHOWL_SPREAD_RELATCH_TURN_DEG - ICEHOWL_SPREAD_TRIGGER_DEG;

static_assert(2.0f * ICEHOWL_SPREAD_TRIGGER_DEG <= ICEHOWL_SPREAD_MARGIN_DEG,
              "neighbours stay out of each other's breath");
static_assert(ICEHOWL_SPREAD_ARRIVE_DEG < ICEHOWL_SPREAD_TRIGGER_DEG && ICEHOWL_SPREAD_ARRIVE < ICEHOWL_SPREAD_TRIGGER &&
                  ICEHOWL_SPREAD_MELEE_ARRIVE < ICEHOWL_SPREAD_MELEE_TRIGGER,
              "a walk has to end inside its own trigger");
static_assert(ICEHOWL_SPREAD_MELEE_MAX + ICEHOWL_SPREAD_MELEE_TRIGGER < ICEHOWL_MELEE_RANGE, "melee stay in reach");
static_assert(ICEHOWL_SPREAD_MELEE_MIN - ICEHOWL_SPREAD_MELEE_TRIGGER > ICEHOWL_WHIRL_RADIUS, "melee stay out of Whirl");
static_assert(ICEHOWL_SPREAD_HEALER_MIN - ICEHOWL_SPREAD_TRIGGER > ICEHOWL_WHIRL_RADIUS, "healers stay out of Whirl");
static_assert(ICEHOWL_SPREAD_RANGED_MIN - ICEHOWL_SPREAD_TRIGGER > ICEHOWL_RANGED_MIN_RANGE,
              "hunters stay past their minimum range");
static_assert(ICEHOWL_SPREAD_MIN_ROOM - 1.0f >= ICEHOWL_SPREAD_RANGED_MIN, "a kept bearing fits the ranged band");
static_assert(ICEHOWL_SPREAD_MELEE_ROOM >= ICEHOWL_SPREAD_MELEE_MAX + 1.0f, "a melee-only bearing fits the melee band");

struct IcehowlBreathStand
{
    Position spot;
    Position icehowl;
    float bearing = 0.0f;
    float bandMin = 0.0f;
    float bandMax = 0.0f;
    float trigger = 0.0f;
    float arrive = 0.0f;
};

float IcehowlBreathStepDeg(Unit* anyUnitOnMap);

bool IcehowlChargeLatched(PlayerbotAI* botAI, Position& start, Position& end);

bool IcehowlChargeCycleActive(PlayerbotAI* botAI);

float DistanceToIcehowlCharge(PlayerbotAI* botAI, float x, float y);

float DistanceToIcehowlLane(Position const& start, Position const& end, float x, float y);

bool IsIcehowlStaggered(Unit* icehowl);

inline constexpr float ICEHOWL_DAZE_NITRO_FROM = 10.0f;
inline constexpr int32 ICEHOWL_DAZE_NITRO_MIN_LEFT_MS = 3000;

bool IcehowlDazeNitroDue(PlayerbotAI* botAI);

bool HasIcehowlFrothingRage(Unit* icehowl);

bool GetIcehowlBreathStand(PlayerbotAI* botAI, IcehowlBreathStand& stand);

bool IsOnIcehowlBreathStand(IcehowlBreathStand const& stand, float x, float y, float degrees, float yards);

bool HasIcehowlBreathSlot(PlayerbotAI* botAI);

}

#endif
