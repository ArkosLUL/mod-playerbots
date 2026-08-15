/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_OSHELPERS_H
#define PLAYERBOTS_OSHELPERS_H

#include "PlayerbotAI.h"
#include "Position.h"

#include <string>
#include <vector>

class Player;
class Unit;
class WorldObject;

namespace OsHelpers
{

constexpr uint32 OS_MAP_ID = 615;

// Difficulty variants come from creature_template.difficulty_entry_1; every entry that has one is
// paired here because the grid searches below match on entry, not on name.
namespace NpcId
{
constexpr uint32 Sartharion = 28860;
constexpr uint32 SartharionH = 31311;
constexpr uint32 Tenebron = 30452;
constexpr uint32 TenebronH = 31534;
constexpr uint32 Shadron = 30451;
constexpr uint32 ShadronH = 31520;
constexpr uint32 Vesperon = 30449;
constexpr uint32 VesperonH = 31535;
constexpr uint32 AcolyteOfShadron = 31218;
constexpr uint32 AcolyteOfShadronH = 31541;
constexpr uint32 AcolyteOfVesperon = 31219;
constexpr uint32 AcolyteOfVesperonH = 31543;
constexpr uint32 DiscipleOfShadron = 30688;
constexpr uint32 DiscipleOfShadronH = 31544;
constexpr uint32 DiscipleOfVesperon = 30858;
constexpr uint32 DiscipleOfVesperonH = 31546;
constexpr uint32 TwilightEgg = 30882;
constexpr uint32 TwilightEggH = 31539;
constexpr uint32 TwilightWhelp = 30890;
constexpr uint32 TwilightWhelpH = 31540;
constexpr uint32 LavaBlaze = 30643;
constexpr uint32 LavaBlazeH = 31317;
constexpr uint32 TwilightFissure = 30641;
constexpr uint32 TwilightFissureH = 31521;
constexpr uint32 FlameTsunami = 30616;
constexpr uint32 SafeAreaTrigger = 30494;
}

namespace SpellId
{
// Twilight Shift has two ids under the same name. The instance script removes 57620, but the portal
// path is not the only way in, so both are treated as "shifted".
constexpr uint32 TwilightShift = 57620;
constexpr uint32 TwilightShiftAlt = 57874;
// Full-school SPELL_AURA_DAMAGE_IMMUNITY (mask 127) plus +50% fire done. Granted by the Acolyte of
// Shadron and stripped only when it dies, so every point of raid damage lands on nothing while it
// is up.
constexpr uint32 GiftOfTwilightFire = 58766;
constexpr uint32 SartharionBerserk = 61632;
// DISPEL_ENRAGE, +200% health and +100% damage. Pulsed every 800ms in 6yd by the tsunami damage
// aura with no target conditions, so it lands on Lava Blazes, drakes and Sartharion alike.
constexpr uint32 MoltenFury = 60430;
// Applied 3.6s after a wave is summoned and stripped 7.4s later, in the same call that shrinks the
// wave to 0.1 scale. It is the whole of a tsunami's lethality.
constexpr uint32 FlameTsunamiDamageAura = 57492;
// Twilight Torment, the Acolyte of Vesperon's raid-wide debuff. Two ids for the same thing: the
// Sartharion one is what a drake called by him casts, the other is the standalone drake encounter.
constexpr uint32 TwilightTormentSartharion = 58835;
constexpr uint32 TwilightTormentVesperon = 57935;
// Power of Shadron: a permanent 50000yd area aura on every enemy, MOD_DAMAGE_PERCENT_TAKEN +100%
// against school mask 4. Each drake casts its own on itself the moment Sartharion engages, so the
// raid carries it from the pull until that drake dies. Vesperon's 61251 is the other half of what
// kills the tank - MOD_INCREASE_HEALTH_PERCENT -25% - and Tenebron's 61248 doubles shadow.
constexpr uint32 PowerOfShadron = 58105;
}

namespace GoId
{
constexpr uint32 TwilightPortal = 193988;
constexpr uint32 NormalPortal = 193989;
}

enum class TsunamiWave : uint8
{
    None,
    Left,
    Right
};

// The raid holds two corridors, not one: the gaps the two wave patterns leave never overlap, so the
// tank cannot stand where the raid stands. Between waves both groups are on their home hold and the
// main tank is 35.3yd from the raid line, inside the 38.5yd heal range, so healers reach him for the
// 14s of every 25s that nothing is in the air. A wave in either direction takes him back out of it,
// 46.7yd away on the left and 53.2yd on the right.
//
// Melee on the boss are a third profile rather than a copy of the tank's: they share his lane between
// waves and under left ones, and give the boss up entirely under right ones. See SafeCorridorY.
enum class CorridorGroup : uint8
{
    Tank,
    Melee,
    Raid
};

extern Position const SAFE_AREA_SOUTH;
extern Position const SAFE_AREA_NORTH;
extern Position const TENEBRON_LANDING;
extern Position const SHADRON_LANDING;
extern Position const VESPERON_LANDING;

// Where each drake is held. Measured in game, not derived, and none of them is the old shared pile:
// dragging a drake across the platform costs more than it buys, so each is tanked beside where it
// touches down, on the side that aims its 15yd frontal Shadow Breath at empty platform.
//
// Vesperon is held well north of its touchdown rather than level with it, which is the one place that
// rule and the platform disagree: it lands at X 3269.71, past the 3268 edge, so the only spot level
// with it is west - and west is straight down the raid line, whose east end is 9.5yd away. Held from
// the north it settles around (3267, 549) facing up the empty rim, which leaves the raid's home hold
// 17yd off, outside the 15yd breath, and its left-wave hold 71 degrees off the facing.
//
// Y is the spot's own value only while nothing is in the air. Against the 8.5yd kill half-width plus
// the 2yd off-tank tolerance each of them is inside one pattern or the other:
//
//   Tenebron 563.380   8.62 to the left line at 572,  0.62 to the right one at 564
//   Shadron  534.660   2.66 to the left line at 532, 13.34 to the right one at 548
//   Vesperon 556.000  16.00 to the left line at 572,  8.00 to the right one at 564
//
// so a wave that can reach the spot takes the raid corridor Y instead and the drake follows. Only
// Shadron's right waves and Vesperon's left ones clear it outright; see WaveClearsY. Tenebron is the
// long walk at 27.9yd, and the wave needs 6.2s from its summon to reach that X.
extern Position const TENEBRON_TANK_SPOT;
extern Position const SHADRON_TANK_SPOT;
extern Position const VESPERON_TANK_SPOT;

// Flame Tsunami stands in lines, and the two patterns never leave a gap in common, so each group
// below gets a home hold that is safe under one pattern and moves for the other one only. From the
// summon offsets in boss_sartharion.cpp:
//
//   left waves   Y 476 484 492 | 524 532 540 | 572 580 588   -> safe 500.5-515.5, 548.5-563.5
//   right waves  Y 500 508 516 | 548 556 564                 -> safe <=491.5, 524.5-539.5, >=572.5
//
// Safe means 8.5yd clear of a line: Flame Tsunami 57491 has a 7yd radius and the area search adds
// the target's 1.5yd combat reach, not the caster's. A bot "arrives" anywhere inside its tolerance,
// so what has to clear 8.5yd is the whole arrival disc, not the hold:
//
//   tank left  513.0     nearest line 524   11.00 clear   10.00 worst
//   tank right 490.0     nearest line 500   10.00 clear    9.00 worst
//   raid home  535.5     nearest line 548   12.50 clear   10.50 worst
//   raid left  551.0     nearest line 540   11.00 clear    9.00 worst
//
// The left hold is the one place melee stand with the tank, and they arrive on the raid's 2.0yd
// tolerance rather than his 1.0, so what caps it is their 9.00 and not his 10.00: 513.5 is the
// ceiling, where 514.5 is all his own tolerance would ask for.
//
// Each tank hold carries its own X - the two are 1.3yd apart, so a dodge that kept the X it started
// on would leave him beside the coordinate - and neither Y is the reading it came from. 490.581 left
// 0.92yd of room for a 1.0yd tolerance, and 490.0 buys the yard back while staying 3.5yd clear of the
// rim at 486.5. The left one was read at 511.089 and moved 1.9yd north, which is what brings him back
// inside heal range between waves.
//
// Both sit inside Sartharion's 20.83yd reach of where the pull drag parks him, (3228.5, 504.7), so he
// never walks again and the raid line stays behind him: Tail Lash reaches 30yd of the 31.2yd it would
// need and the frontal cone points away. The swap does turn him about 100 degrees, which is what the
// melee flank action is for.
constexpr float TANK_HOLD_LEFT_X = 3221.3743f;
constexpr float TANK_CORRIDOR_LEFT_Y = 513.0f;
constexpr float TANK_HOLD_RIGHT_X = 3220.0706f;
constexpr float TANK_CORRIDOR_RIGHT_Y = 490.0f;
// The right hold is the raid's home, and it cannot move further south. The west end of the band is
// 31.2yd from the boss while the tank is on his own right hold, against a 30yd cone the raid stands
// directly behind, and every yard south costs about a yard of that. The left one is capped by the
// wave instead, 11yd off the line at 540.
constexpr float RAID_CORRIDOR_LEFT_Y = 551.0f;   // gap 548.5-563.5
constexpr float RAID_CORRIDOR_RIGHT_Y = 535.5f;  // gap 524.5-539.5
constexpr float CORRIDOR_ARRIVAL_TOLERANCE = 2.0f;
// Tighter than the raid's. His right hold has the narrowest band on the platform: navmesh floor at
// Y 486.5, wave threshold at 491.5.
constexpr float TANK_ARRIVAL_TOLERANCE = 1.0f;

// Void Blast is 4.0yd radius plus the player's 1.5yd combat reach; the rest is margin.
constexpr float FISSURE_CLEAR_RADIUS = 8.0f;
// The dodge steps this much past the detection radius. Landing exactly on it leaves the bot on the
// trigger boundary, where the next tick fires the dodge again.
constexpr float FISSURE_STEP_MARGIN = 2.0f;

// Sartharion's combat reach is 18.0yd (creature_model_info, display 27035), so GetMeleeRange against
// a player is 18 + 1.5 + 4/3 = 20.83yd. He stops chasing the instant the tank is inside that, which
// is why the tank cannot simply walk to an anchor and expect the boss to follow him to it.
//
// So the tank drags him once, at the pull, into the platform's southern tip and waits there. He
// spawns at (3246.57, 551.26) and stops 20.83yd short along the ray to the tank, which from this
// corner is (3228.5, 504.7) - inside his reach from both tank corridor holds, so he never walks again
// and the raid line stays out of the cone that matters.
//
// The corner is a hand-measured in-game position, past where the navmesh floor starts at this X.
// Players path onto NAV_MAGMA (PathGenerator::CreateFilter), so it is reachable; if standing there
// costs the tank health, Y 487.0 is solid floor and moves the boss less than 1.5yd. It is 35.8yd from
// SAFE_AREA_SOUTH, inside the 40yd Range Marker aura, so no Pyrobuffet either. It sits 1.3yd off the
// left-wave line at 484, which never comes up: the first wave is summoned 20s into the fight and the
// whole drag is done inside 10.
constexpr float MAIN_TANK_DRAG_X = 3220.9905f;
constexpr float MAIN_TANK_DRAG_Y = 485.262f;
constexpr float MAIN_TANK_DRAG_TOLERANCE = 2.0f;
// Held for this long before the tank walks back, so the boss has time to close the last of the gap
// and settle facing him rather than mid-turn.
constexpr uint32 MAIN_TANK_DRAG_DWELL_MS = 5000;
// The drag needs ~10s and a wave costs it another ~11s, so anything past this means he is not
// following at all - a player is holding threat - and the tank should take his hold instead of
// waiting in a lane that is lethal under left waves.
constexpr uint32 MAIN_TANK_DRAG_TIMEOUT_MS = 45000;

// Flame Breath is an 82-degree frontal cone out to 60yd and Tail Lash an 82-degree cone off his tail
// out to 30yd (spell_cone 56908/56910, and 56910 carries SpellVisual 3879, which is what marks a cone
// as rear-facing). That leaves two 98-degree side wedges. Both are full arcs, matching HasInArc. The
// margin widens each cone by 8 degrees a side before anyone is called unsafe.
constexpr float SARTHARION_CONE_DEGREES = 82.0f;
constexpr float SARTHARION_CONE_MARGIN_DEGREES = 16.0f;
constexpr float FLAME_BREATH_RANGE = 60.0f;
constexpr float TAIL_LASH_RANGE = 30.0f;
// Melee stand this far inside melee range on the flank, so a small drift does not drop them out of it.
constexpr float FLANK_STAND_BACK = 2.0f;

// A drake carries a 15yd frontal Shadow Breath and nothing off its tail, so melee take the rear
// outright rather than the side wedge the shared "rear flank" aims for. The gate is 10 degrees wider
// than the draw: a destination drawn on the boundary re-fires the trigger the tick after it arrives.
constexpr float DRAKE_BREATH_RANGE = 15.0f;
constexpr float DRAKE_REAR_GATE_DEGREES = 140.0f;
constexpr float DRAKE_REAR_DRAW_DEGREES = 30.0f;

// Range Marker (56911) is a 40yd friendly area aura on the two Safe Area triggers, and it is the
// only exemption from Pyrobuffet. Destinations are pulled inside this radius of the nearer one.
constexpr float RANGE_MARKER_SAFE_RADIUS = 36.0f;

// Read off the Detour navmesh for map 615, not off the script's spawn extents: the floor narrows to
// a point in the south and its east edge moves with Y, so a single box either allows lava or gives
// away one end. Each band below was swept against the mesh and is 100% NAV_GROUND.
//
//   Y 487-492   X 3220-3231   the southern tip, the pull drag and the tank's right hold
//   Y 492-496   X 3220-3242
//   Y 496-504   X 3220-3250
//   Y 504-520   X 3227-3266   the tank's left hold, and again for Y 566-569
//   Y 520-566   X 3227-3268   the raid line, the drake pile and the off-tank
//
// The west edge runs out to 3219.75 from Y 490 north, so the southern bands take 3220 rather than the
// 3221 the Y-486 row alone would suggest - the tank's right hold needs that yard.
constexpr float PLATFORM_MIN_Y = 487.0f;
constexpr float PLATFORM_MAX_Y = 569.0f;
constexpr float PLATFORM_MIN_X_SOUTH = 3220.0f;
constexpr float PLATFORM_MIN_X_CORE = 3227.0f;
constexpr float PLATFORM_MAX_X_TIP = 3231.0f;
constexpr float PLATFORM_MAX_X_NECK = 3242.0f;
constexpr float PLATFORM_MAX_X_SOUTH = 3250.0f;
constexpr float PLATFORM_MAX_X_CORE = 3266.0f;
constexpr float PLATFORM_MAX_X_WIDE = 3268.0f;
constexpr float PLATFORM_TIP_MAX_Y = 492.0f;
constexpr float PLATFORM_NECK_MAX_Y = 496.0f;
constexpr float PLATFORM_SOUTH_MAX_Y = 504.0f;
constexpr float PLATFORM_WIDE_MIN_Y = 520.0f;
constexpr float PLATFORM_WIDE_MAX_Y = 566.0f;

// Deliberately looser than the destination box above: this one answers "is that unit in the fight",
// so a Lava Blaze standing on the rim still counts while the three drake perches - (3239, 657, 87),
// (3146, 521, 90) and (3363, 525, 98) - do not. The Z ceiling clears the 58.6-59.6 platform floor.
constexpr float ROOM_MIN_X = 3208.0f;
constexpr float ROOM_MAX_X = 3286.0f;
constexpr float ROOM_MIN_Y = 474.0f;
constexpr float ROOM_MAX_Y = 591.0f;
constexpr float ROOM_MAX_Z = 65.0f;

// Players arrive on map 615 at (3228.58, 385.86) and walk north onto the platform, so this is the X
// the raid enters on and the anchor every offset below is measured from. The boss ends up half a yard
// off it. The tank corridor holds have their own X, 7-8yd further west.
constexpr float RAID_ENTRY_ANCHOR_X = 3228.58f;

// The active corridor is 15yd wide, so the raid is effectively collinear along X and ordering is the
// only tool available: Sartharion and the main tank first, then ranged and healers. The offset is from
// the entry anchor, and it clears Shadron's touchdown (X 3230.50) by 18yd, outside its 15yd Shadow
// Breath cone. Nothing of the raid's stands further east - each drake is held beside where it lands,
// and Lava Blazes and whelps are taunted from wherever the off-tank happens to be.
constexpr float RAID_LINE_OFFSET_X = 20.0f;
// Keeps melee meeting a drake at its touchdown point east of it, so the frontal Shadow Breath cone
// points at empty platform rather than back down the raid line.
constexpr float OFFTANK_EAST_OFFSET = 4.0f;

// Ranged and healers hold a band this wide along X rather than one point, so a dozen bots do not pile
// onto the same coordinate and then fight the collision mover. Y stays on the tight corridor
// tolerance - that is the axis a tsunami kills on.
constexpr float RAID_LINE_TOLERANCE_X = 8.0f;

// Drakes are UNIT_FLAG_NOT_SELECTABLE while airborne, so "possible targets" never sees them and the
// sight-distance cap (100yd) is too short for the patrol paths anyway.
constexpr float ROOM_SEARCH_RADIUS = 200.0f;

// Threat redirect goes to the main tank for this long after the encounter starts, then follows the
// adds to the off-tank.
constexpr uint32 PULL_WINDOW_MS = 10000;

// Where the main tank's cooldowns are worth more than anywhere else in the fight. Power of Shadron
// doubles every point of fire he takes and Power of Vesperon costs him a quarter of his health pool,
// both from the pull, both permanent until their drake dies - so the class nodes, which hang these off
// a bare health trigger, spend a 5-minute Shield Wall on the first Flame Breath and have nothing left
// for the stretch that actually kills him. Held until Shadron is nearly down instead, then spent one
// at a time.
constexpr float MAIN_TANK_COOLDOWN_SHADRON_PCT = 50.0f;
// Not latched, unlike the Shadron gate: one dip this low buys one cooldown, not the rest of the fight.
constexpr float MAIN_TANK_COOLDOWN_PANIC_PCT = 25.0f;

constexpr float LAVA_BLAZE_TAUNT_RANGE = 30.0f;
constexpr float TRANQUILIZING_SHOT_RANGE = 35.0f;

// Sartharion resolution. Both go through phase-aware searches, so a bot inside the Twilight Realm
// (phase 16) will not find him - that is intended, in-realm behaviour must not depend on the boss.
Unit* GetSartharion(Player* bot);
bool SartharionEncounterActive(Player* bot);
bool SartharionDamageImmune(Player* bot);
uint32 EncounterElapsedMs(Player* bot);

bool HasTwilightShift(Unit const* unit);
// Tenebron's realm holds nothing but eggs, and they hatch into phase-1 whelps the off-tank picks up
// anyway, so the trip is not worth making. The other two are: Shadron's acolyte makes Sartharion
// immune to every school and Vesperon's torments the raid. Which realm the portal leads to cannot be
// read off the portal - all three drakes share one refcounted GameObject when Sartharion calls them -
// and the acolytes themselves are phase 16, which the grid searches filter out. So the reason to go
// in is read off what they cast on the platform instead.
bool TwilightRealmWorthEntering(Player* bot);
bool HasMoltenFury(Unit const* unit);

// The side of a wave that can still reach this bot, or None. Side is read off the tsunami's Y: the
// left and right spawn offsets are disjoint sets of whole numbers and Y never changes while a wave
// travels, so that half stays exact for the wave's whole life. "Can still reach" is the other half,
// and it is not the same as "a tsunami is on the map" - a wave that has swept past this X, or that
// has been spent, cannot come back, and waiting for its despawn instead costs the raid 5s a wave.
//
// Answers inside the Twilight Realm as well, where the search runs from a bot still on the platform: the
// side is a property of the wave, and the reach test is against the caller's own X, which phase 16 does
// not change.
TsunamiWave ClassifyTsunamiWave(Player* bot);
// True while a wave of this side cannot reach a bot holding this Y, margin included. The corridor
// holds are the fallback rather than the only safe ground: the off-tank's drake spots and melee behind
// a drake clear some of the lines outright, and a 28yd walk to ground that is no safer costs the trip
// twice - once out and once back.
bool WaveClearsY(float y, TsunamiWave wave);
// The window a shifted bot has to spend on ground it is safe to materialise on. Not a dodge - nothing in
// phase 16 can be touched by a wave. The three drakes share one portal refcount and each drake's entry is
// outstanding while its acolyte lives, so the shift is never stripped mid-kill; once they are dead it
// lands on a tick nobody chooses, and the bot reappears on whatever Y it was standing on.
bool TwilightRealmWaveWait(Player* bot);
// Healers and ranged are Raid always, the main tank is Tank always. Everyone left - melee and the
// off-tank - dodges in the corridor its own target is standing in.
CorridorGroup CorridorGroupFor(Player* bot);
// Home and away, not sticky. The tank homes on his left hold and leaves it for right waves; the raid
// homes on its right one and leaves it for left waves. With nothing in reach both are back home.
float SafeCorridorY(Player* bot);
// The main tank's two holds have their own X, unlike everyone else's, who keep whatever X they dodged
// from.
float TankHoldX(Player* bot);
// Tighter for the main tank, whose southern holds have no room for the raid-wide slop.
float CorridorToleranceFor(Player* bot);

// A drake sitting on its perch is alive and, for the first half second of the pull, selectable:
// Reset clears UNIT_FLAG_NOT_SELECTABLE and only EVENT_DRAGON_START_PATROL puts it back, 500ms after
// Sartharion engages. Every target search here is bounded by this so nobody runs off the arena at it.
bool InsideRoom(WorldObject const* object);

// The room box above reaches X 3286 so a Lava Blaze on the rim still counts as in the fight, which
// leaves 18yd of lava reading as "inside the room". This is the destination box instead, opened by a
// yard so the arrival tolerances cannot trip it: the off-tank idles up to 2yd off a drake spot.
constexpr float OFF_PLATFORM_MARGIN = 1.0f;

// True while the bot is standing outside the platform's own box, margin included. The strategy clamps
// every destination it issues and this is the other half - where the bot actually ended up.
bool OffThePlatform(Player* bot);

// The east and west edges both move with Y; north and south are constants. Banded rather than
// interpolated: each band is a rectangle that was swept against the navmesh, and an interpolated
// edge would not be.
float PlatformMaxX(float y);
float PlatformMinX(float y);
void ClampToPlatform(float& x, float& y);
void ClampToRangeMarker(float& x, float& y);
// Platform first, then Range Marker. Every MoveTo destination in this strategy goes through here.
void ClampDestination(float& x, float& y);

Position const* LandingPositionFor(uint32 entry);
Position const* DrakeTankSpotFor(uint32 entry);
Unit* FindInboundDrake(Player* bot);
float SecondsUntilLanding(Unit* drake);
std::vector<Unit*> LandedDrakes(Player* bot);
// requireSelectable is what tells a perched drake from a landed one, so it stays on by default. The
// Twilight Fissure is the one thing worth finding that is permanently UNIT_FLAG_NOT_SELECTABLE
// (creature_template.unit_flags 33554432), and its lookups pass false.
Unit* FindUnitByEntries(Player* bot, std::vector<uint32> const& entries, float radius,
                        bool requireSelectable = true);
// True while a live Twilight Fissure covers (x, y). The hold actions test their own destination with
// it, because the dodge steps further along X than the raid line tolerance allows: without this the
// hold walks the bot straight back into the blast and the two alternate every tick until it goes off.
bool FissureBlocks(Player* bot, float x, float y);

float RaidLineX(Player* bot);
// Newest drake on the ground, or nullptr.
Unit* NewestLandedDrake(Player* bot);
// What the off-tank is holding: the newest drake down, or the nearest add he can taunt from where he
// is standing.
Unit* OffTankChargeFor(Player* bot);
// Where he stands, which tracks drakes and nothing else: the newest one's spot, or Tenebron's until
// the first lands, so he is already on the spot when it touches down. Carries the wave override.
Position OffTankAnchor(Player* bot);

Player* GetOffTank(PlayerbotAI* botAI, Player* bot);
bool IsOffTank(Player* bot);
// Logs one warning per pull when the raid has no assist tank, then returns false so every off-tank
// behaviour stays inert. A half-working single-tank fallback is harder to diagnose than a bail.
bool RequireOffTank(PlayerbotAI* botAI, Player* bot);
Player* RedirectTarget(PlayerbotAI* botAI, Player* bot);
// Rogues redirect off their own target rather than off the raid-wide RedirectTarget above: Tricks moves
// everything for 6s, so it has to land on the tank of the thing the rogue is actually hitting - and
// never on a Lava Blaze, which dies to one Fan of Knives and takes the 30s cooldown with it. Sartharion
// needs no pull-window test of its own; his tank is the main tank for the whole fight. Hunters keep
// RedirectTarget, because Misdirection is three shots rather than a blanket window and the off-tank
// pulling a whelp off a healer is worth one.
Player* RedirectTankFor(PlayerbotAI* botAI, Player* bot);
// A landed drake that is not on the off-tank, nearest first. He attacks only the newest one, so without
// this nothing pulls back the drake he walks away from at a handover - and by the time it peels it is
// 35.5yd behind him at the far spot, outside the taunt range this searches with.
Unit* OffTankTauntTarget(Player* bot);
// The bot's own single-target taunt, or nullptr for a class that has none. The four names the
// multiplier already lists one by one, from the other side.
char const* TauntSpellFor(Player* bot);

// The first two healers by GUID.
constexpr size_t PORTAL_SQUAD_HEALERS = 2;

// Fixed at the pull by stable GUID sort and never reshuffled: every DPS, melee and ranged alike, the
// first two healers, and the second off-tank if the raid has one. The acolytes are worth the whole
// raid's damage - Shadron's makes Sartharion immune to every school - and the platform survives the
// trip, because the main tank keeps the boss and the first off-tank keeps the drakes.
bool PortalSquadMember(Player* bot);
bool TwilightAddsAlive(Player* bot);

bool IsDrakeEntry(uint32 entry);
// True while a point stands at least DRAKE_REAR_GATE_DEGREES off the drake's facing.
bool BehindDrake(Unit* drake, Position const& point);

// True while a point stands in Flame Breath's frontal cone or Tail Lash's rear one, margin included.
// Melee positioning is gated on this rather than run every tick, so a bot standing safely off his
// flank is left alone. Takes a point rather than a unit so a candidate destination can be tested
// before anyone walks to it.
bool InSartharionCone(Unit* boss, Position const& point);

// Shadron once it is on the ground, or nullptr while it is still circling: drakes carry
// UNIT_FLAG_NOT_SELECTABLE for their whole flight and clear it on POINT_LANDING.
Unit* LandedShadron(Player* bot);
// True once Shadron is down for good - dead, or never called at all. Power of Shadron is on the raid
// from the pull and permanent, so "no drake and no aura" is what separates gone from still in the air.
bool ShadronGone(Player* bot);

// One-way, latched per encounter: true once Shadron is on the ground. Burst is spent once, so the
// window does not close again when he dies. Sartharion is 60s into the fight by then and the Gift of
// Twilight interlock parks Bloodlust for as long as Shadron's acolyte lives, so it lands on the first
// moment the raid can actually damage the boss with a second drake up.
bool SartharionBurstWindowOpen(Player* bot);

// The window the main tank holds his cooldowns for, latched on Shadron and unlatched on his own health.
bool MainTankCooldownWindowOpen(Player* bot);
// The weakest cooldown he can cast right now, or nullptr - either because none is off cooldown or
// because one is already running. Ordered by cooldown length, which is what "weakest" means here.
char const* NextTankDefensive(PlayerbotAI* botAI, Player* bot);
// True for the cast names in that table, so the class nodes can be held off them.
bool IsHeldTankDefensive(std::string const& actionName);

// One-shot, latched per encounter: true once Sartharion has come south with the pull drag, or once
// the tank has given up waiting for him. EncounterState clears itself 15s after combat ends, so a
// wipe re-arms it.
bool MainTankDragDone(Unit* boss);
void SetMainTankDragged(Unit* boss);
// When the tank reached the drag corner, or 0 while he is not standing on it. Cleared rather than
// kept when he is pushed off, so a tsunami mid-wait costs the whole dwell instead of banking it.
uint32 MainTankDragArrivedMs(Unit* boss);
void SetMainTankDragArrivedMs(Unit* boss, uint32 arrivedMs);
// When the drag started, stamped on its first tick. The timeout runs off this rather than off the
// encounter clock, so it means what it says: the tank has been trying to drag for this long.
uint32 MainTankDragStartedMs(Unit* boss);
void SetMainTankDragStartedMs(Unit* boss, uint32 startedMs);

Unit* PriorityTarget(Player* bot);
Unit* TranquilizeTargetFor(Player* bot);

}

#endif
