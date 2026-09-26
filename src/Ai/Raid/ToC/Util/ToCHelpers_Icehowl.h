#ifndef PLAYERBOTS_RAID_TOCHELPERS_ICEHOWL_H
#define PLAYERBOTS_RAID_TOCHELPERS_ICEHOWL_H

#include "Define.h"
#include "ToCData.h"

class PlayerbotAI;
class Unit;

namespace TrialOfTheCrusaderHelpers
{

// boss_northrend_beasts.cpp IcehowlSpells
constexpr uint32 SPELL_STAGGERED_DAZE = 66758;  // no difficulty row
constexpr uint32 SPELL_FROTHING_RAGE = 66759;
constexpr uint32 SPELL_TRAMPLE = 66734;         // no difficulty row

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

}

#endif
