/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ONYGEOMETRY_H
#define PLAYERBOTS_ONYGEOMETRY_H

#include <vector>

#include "EncounterHelpers.h"
#include "OnyData.h"

class Player;
class Unit;

// Where things are: breath lanes, fissures, the ground slots, her flank and the phase 2 cells.
namespace OnyHelpers
{
float DistanceToSegment2d(float x, float y, Position const& from, Position const& to);

BreathLane const* BreathLaneForSpell(uint32 spellId);
// Waypoints run 1 to 8.
BreathLane const& BreathLaneForWaypoint(uint8 waypoint);
uint8 NearestWaypoint(float x, float y);

// The lane as overlapping circles, close enough together that the gaps between them stay inside
// the clearance.
void AppendLaneHazards(BreathLane const& lane, float clearance, std::vector<EncounterHelpers::HazardCircle>& out);

float NearestFissureDistance(float x, float y);

bool IsFireballSpell(uint32 spellId);
bool IsFlameBreathSpell(uint32 spellId);

// Signed angle of a point off her facing, in degrees: positive is her left (west when she faces
// north), 0 dead ahead, +-180 dead behind.
float BearingOffFacingDeg(Position const& origin, float facing, float x, float y);

// Ring radius and signed bearing of a ground slot. Slot = ring * 6 + k: k even on her left, odd
// on her right, bearing growing every second k.
float GroundSlotRadius(uint8 slot);
float GroundSlotBearingDeg(uint8 slot);
Position GroundSlotPoint(Position const& anchor, float facing, uint8 slot, float radiusDelta = 0.0f,
                         float bearingDeltaDeg = 0.0f);

// The phase 2 cells for one waypoint, best first: clear of its breath lane before inside it, then
// nearest her hover point, and never two closer than the Fireball splash.
std::vector<Position> BuildAirCells(Player* bot, BreathLane const& lane, std::vector<Position> const& eggs);
}  // namespace OnyHelpers

#endif
