/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */


#ifndef _PLAYERBOT_OSGEOMETRY_H
#define _PLAYERBOT_OSGEOMETRY_H

#include "OSData.h"
#include "Position.h"
#include <vector>

class Player;
class Unit;
class WorldObject;

// Where things are: platform bounds and clamping, Flame Tsunami lines and corridors, cone tests,
// and drake landing/tank geometry. Nothing here reads encounter state beyond what it is handed.
namespace OsHelpers
{

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
// Resolves the destination's own ground Z instead of reusing the bot's - the platform floor runs
// 58.6-59.6, so a destination 20yd away can sit a yard off the ground the bot is standing on - and
// validates the path. rejectOnCollision is true for the dodges, where a blocked path means try
// somewhere else, and false for routine holds, which walk to whatever the clamp gives back.
bool ResolveMoveDestination(Player* bot, float& x, float& y, float& z, bool rejectOnCollision);

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

bool IsDrakeEntry(uint32 entry);
// True while a point stands at least DRAKE_REAR_GATE_DEGREES off the drake's facing.
bool BehindDrake(Unit* drake, Position const& point);

// True while a point stands in Flame Breath's frontal cone or Tail Lash's rear one, margin included.
// Melee positioning is gated on this rather than run every tick, so a bot standing safely off his
// flank is left alone. Takes a point rather than a unit so a candidate destination can be tested
// before anyone walks to it.
bool InSartharionCone(Unit* boss, Position const& point);

}

#endif
