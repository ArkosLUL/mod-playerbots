/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef _PLAYERBOT_OSGEOMETRY_H
#define _PLAYERBOT_OSGEOMETRY_H

#include "OSData.h"
#include "ObjectGuid.h"
#include "Position.h"
#include <unordered_set>
#include <vector>

class Player;
class Unit;
class WorldObject;

namespace OsHelpers
{

TsunamiWave ClassifyTsunamiWave(Player* bot);
void NoteTsunamiHazards(Player* bot, std::unordered_set<ObjectGuid>& traced);
bool WaveClearsY(float y, TsunamiWave wave);
bool TwilightRealmWaveWait(Player* bot);
CorridorGroup CorridorGroupFor(Player* bot);
float SafeCorridorY(Player* bot);
float TankHoldX(Player* bot);
float CorridorToleranceFor(Player* bot);

bool InsideRoom(WorldObject const* object);

constexpr float OFF_PLATFORM_MARGIN = 1.0f;

bool OffThePlatform(Player* bot);

float PlatformMaxX(float y);
float PlatformMinX(float y);
void ClampToPlatform(float& x, float& y);
void ClampToRangeMarker(float& x, float& y);
void ClampDestination(float& x, float& y);
bool ResolveMoveDestination(Player* bot, float& x, float& y, float& z, bool rejectOnCollision);

Position const* LandingPositionFor(uint32 entry);
Position const* DrakeTankSpotFor(uint32 entry);
Unit* FindInboundDrake(Player* bot);
float SecondsUntilLanding(Unit* drake);
std::vector<Unit*> LandedDrakes(Player* bot);
Unit* FindUnitByEntries(Player* bot, std::vector<uint32> const& entries, float radius,
                        bool requireSelectable = true);
bool FissureBlocks(Player* bot, float x, float y);

float RaidLineX(Player* bot);
Unit* NewestLandedDrake(Player* bot);
Unit* OffTankChargeFor(Player* bot);
Position OffTankAnchor(Player* bot);

bool IsDrakeEntry(uint32 entry);
bool BehindDrake(Unit* drake, Position const& point);

bool InSartharionCone(Unit* boss, Position const& point);

}

#endif
