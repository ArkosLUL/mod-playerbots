/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERFLAMELEVIATHAN_H
#define PLAYERBOTS_ULDENCOUNTERFLAMELEVIATHAN_H

#include "Position.h"
#include "UldData.h"

#include <vector>

class Player;
class PlayerbotAI;
class Unit;

enum UlduarFlameLeviathanIds
{
    NPC_FL_THORIM_HAMMER_TARGET = 33364,
    NPC_FL_MIMIRONS_INFERNO_TARGET = 33369,
    NPC_FL_MIMIRONS_INFERNO = 33370,
    NPC_FL_HODIRS_FURY_TARGET = 33108,
    NPC_FL_HODIRS_FURY = 33212,

    SPELL_FL_PURSUED = 62374,
    SPELL_FL_GATHERING_SPEED = 62375,
    SPELL_FL_BATTERING_RAM = 62376,
    SPELL_FL_FLAME_VENTS = 62396,
    SPELL_FL_MISSILE_BARRAGE = 62400,
    NPC_FL_TURRET = 33139,
    NPC_FL_DEFENSE_TURRET = 33142,
    NPC_FL_POOL_OF_TAR = 33090,
    NPC_FL_PYRITE_CONTAINER = 33189,
    NPC_FL_MECHANOLIFT = 33214,

    NPC_FL_FREYA_WARD_TARGET = 33366,
    NPC_FL_FREYA_WARD = 33367,
    NPC_FL_WRITHING_LASHER = 33387,
    NPC_FL_WARD_OF_LIFE = 34275,
    SPELL_FL_TOWER_OF_LIFE = 64482,

    SPELL_FL_RAM = 62345,
    SPELL_FL_ELECTROSHOCK = 62522,
    SPELL_FL_STEAM_RUSH = 62346,

    SPELL_FL_FIRE_CANNON = 62358,
    SPELL_FL_ANTI_AIR_ROCKET_SIEGE = 62359,
    SPELL_FL_SHIELD_GENERATOR = 64677,

    SPELL_FL_SONIC_HORN = 62974,
    SPELL_FL_TAR = 62286,
    SPELL_FL_SPEED_BOOST = 62299,

    SPELL_FL_HODIRS_FURY_STUN = 62297,

    SPELL_FL_HURL_BOULDER = 62306,
    SPELL_FL_HURL_PYRITE_BARREL = 62490,
    SPELL_FL_DEMOLISHER_RAM = 62308,
    SPELL_FL_BLUE_PYRITE_DOT = 68605,

    SPELL_FL_MORTAR = 62634,
    SPELL_FL_ANTI_AIR_ROCKET = 64979,
    SPELL_FL_GRAB_CRATE = 62479,
    SPELL_FL_INCREASED_SPEED = 62471,
    SPELL_FL_LOAD_INTO_CATAPULT = 64414,

    SPELL_FL_THROW_PASSENGER = 62324,
    SPELL_FL_PASSENGER_LOADED = 62340,
    SPELL_FL_HOOKSHOT_AURA = 62336,

    NPC_FL_SEAT = 33114,
    SPELL_FL_SYSTEMS_SHUTDOWN = 62475,

    SPELL_FL_FIRST_AID_KIT = 64660,
};

enum class FlameLeviathanSeat : uint8
{
    None = 0,
    SiegeDriver = 1,
    SiegeGunner = 2,
    DemolisherDriver = 3,
    DemolisherGunner = 4,
    Catapult = 5,
    LeviathanSeat = 6,
    ChopperDriver = 7,
    ChopperPassenger = 8,
    OnFoot = 9,
};

inline constexpr int8 ULDUAR_FL_SEAT_RIDER = 0;
inline constexpr int8 ULDUAR_FL_SEAT_TURRET = 1;
inline constexpr int8 ULDUAR_FL_SEAT_DEVICE = 2;
inline constexpr int8 ULDUAR_FL_DEMOLISHER_GUNNER_SEAT = 1;
inline constexpr int8 ULDUAR_FL_DEMOLISHER_CATAPULT_SEAT = 3;
inline constexpr int8 ULDUAR_FL_CHOPPER_REAR_SEAT = 1;
inline constexpr uint8 ULDUAR_FL_LEVIATHAN_SEATS = 4;

enum FlameLeviathanTowerFlags
{
    FL_TOWER_STORM = 0x1,
    FL_TOWER_FLAMES = 0x2,
    FL_TOWER_FROST = 0x4,
    FL_TOWER_LIFE = 0x8,
    FL_TOWER_ALL = 0xF
};

inline constexpr float ULDUAR_FL_TOWER_HAZARD_MARGIN = 8.0f;

inline constexpr float ULDUAR_FL_TOWER_HAZARD_CLEAR_SCAN = 45.0f;

inline constexpr float ULDUAR_FL_FURY_RADIUS = 10.0f;
inline constexpr float ULDUAR_FL_HAMMER_RADIUS = 7.0f;
inline constexpr float ULDUAR_FL_INFERNO_RADIUS = 9.0f;

inline constexpr uint32 ULDUAR_FL_FURY_ARMED_MS = 6500;
inline constexpr uint32 ULDUAR_FL_FURY_STUN_MS = 10000;

extern std::vector<Position> const ULDUAR_FL_ARENA_CORNERS;

inline constexpr float ULDUAR_FL_SIEGE_STAND_DIST = 8.0f;

inline constexpr float ULDUAR_FL_CHOPPER_STAND_DIST = 20.0f;
inline constexpr float ULDUAR_FL_DEMOLISHER_BAND = 50.0f;

inline constexpr float ULDUAR_FL_STATION_LEAD_S = 1.5f;
inline constexpr float ULDUAR_FL_STATION_LEAD_MAX = 10.0f;

inline constexpr float ULDUAR_FL_CENTRE_TURN_DEADZONE = 15.0f;
inline constexpr float ULDUAR_FL_TAR_LEAD_DIST = 30.0f;

inline constexpr float ULDUAR_FL_FACING_TOLERANCE = 0.1f;

inline constexpr float ULDUAR_FL_ARRIVE_TOLERANCE = 8.0f;

inline constexpr float ULDUAR_FL_REPOSITION_EPSILON = 6.0f;

inline constexpr uint32 ULDUAR_FL_ELECTROSHOCK_COST = 20;
inline constexpr uint32 ULDUAR_FL_RAM_COST = 40;
inline constexpr uint32 ULDUAR_FL_STEAM_RUSH_COST = 40;
inline constexpr uint32 ULDUAR_FL_SONIC_HORN_COST = 20;
inline constexpr uint32 ULDUAR_FL_FIRE_CANNON_COST = 20;
inline constexpr uint32 ULDUAR_FL_SPEED_BOOST_COST = 50;
inline constexpr uint32 ULDUAR_FL_INCREASED_SPEED_COST = 25;
inline constexpr uint32 ULDUAR_FL_PYRITE_BARREL_COST = 5;

inline constexpr uint32 ULDUAR_FL_PYRITE_MAX_STACKS = 10;
inline constexpr uint32 ULDUAR_FL_PYRITE_BURST_STACKS = 6;
inline constexpr uint32 ULDUAR_FL_PYRITE_REFRESH_RESERVE = 20;

inline constexpr uint32 ULDUAR_FL_PYRITE_REFRESH_SLACK_MS = 2000;

inline constexpr uint32 ULDUAR_FL_PYRITE_FLIGHT_MS = 3000;
inline constexpr uint32 ULDUAR_FL_PYRITE_FLIGHT_TIMEOUT_MS = 5000;
inline constexpr uint32 ULDUAR_FL_PYRITE_FLIGHT_MIN_MS = 500;
inline constexpr int32 ULDUAR_FL_PYRITE_LANDING_JUMP_MS = 250;

inline constexpr float ULDUAR_FL_STEAM_RUSH_DIST = 35.0f;
inline constexpr float ULDUAR_FL_STEAM_RUSH_AIM_ARC = float(M_PI) / 4.0f;
inline constexpr uint32 ULDUAR_FL_STEAM_RUSH_COOLDOWN_MS = 15000;

inline constexpr uint32 ULDUAR_FL_STEAM_RUSH_GCD_MS = 2000;
inline constexpr uint32 ULDUAR_FL_VENT_RUSH_LEAD_MS = 5000;
inline constexpr uint32 ULDUAR_FL_VENT_INTERVAL_MS = 20000;

inline constexpr uint32 ULDUAR_FL_VENT_APPROACH_LEAD_MS = 2000;

inline constexpr float ULDUAR_FL_ELECTROSHOCK_CONE_RADIUS = 25.0f;
inline constexpr float ULDUAR_FL_RAM_CONE_RADIUS = 15.0f;
inline constexpr float ULDUAR_FL_SONIC_HORN_CONE_RADIUS = 35.0f;

inline constexpr float ULDUAR_FL_ELECTROSHOCK_CONE_HALF_ANGLE = 30.0f * float(M_PI) / 180.0f;
inline constexpr float ULDUAR_FL_RAM_CONE_HALF_ANGLE = 50.0f * float(M_PI) / 180.0f;
inline constexpr float ULDUAR_FL_SONIC_HORN_CONE_HALF_ANGLE = 25.0f * float(M_PI) / 180.0f;

inline constexpr uint32 ULDUAR_FL_ELECTROSHOCK_COOLDOWN_MS = 10000;
inline constexpr uint32 ULDUAR_FL_ELECTROSHOCK_RETRY_MS = 3000;

inline constexpr uint32 ULDUAR_FL_DEMOLISHER_RAM_COOLDOWN_MS = 4000;
inline constexpr uint32 ULDUAR_FL_MORTAR_COOLDOWN_MS = 1000;
inline constexpr uint32 ULDUAR_FL_SHIELD_GENERATOR_COOLDOWN_MS = 60000;
inline constexpr uint32 ULDUAR_FL_TAR_COOLDOWN_MS = 15000;
inline constexpr uint32 ULDUAR_FL_SPEED_RECAST_MS = 1000;
inline constexpr uint32 ULDUAR_FL_ANTI_AIR_ROCKET_COOLDOWN_MS = 250;

inline constexpr float ULDUAR_FL_BATTERING_RAM_RADIUS = 25.0f;

inline constexpr float ULDUAR_FL_BATTERING_RAM_CAST_RANGE = 15.0f;

inline constexpr float ULDUAR_FL_HURL_BOULDER_MIN_RANGE = 10.0f;
inline constexpr float ULDUAR_FL_HURL_BOULDER_MAX_RANGE = 70.0f;
inline constexpr float ULDUAR_FL_MORTAR_MAX_RANGE = 50.0f;

inline constexpr float ULDUAR_FL_FIRE_CANNON_MIN_RANGE = 10.0f;
inline constexpr float ULDUAR_FL_FIRE_CANNON_MAX_RANGE = 70.0f;

inline constexpr float ULDUAR_FL_CANNON_SPLASH = 20.0f;
inline constexpr float ULDUAR_FL_MORTAR_SPLASH = 11.0f;

inline constexpr float ULDUAR_FL_CORNER_STANDOFF = 12.0f;

inline constexpr float ULDUAR_FL_CORNER_HOLD_RADIUS = 30.0f;

inline constexpr float ULDUAR_FL_STATION_SPACING = 12.0f;

inline constexpr float ULDUAR_FL_STATION_MAX_ARC = 2.0f * float(M_PI) / 3.0f;

inline constexpr uint32 ULDUAR_FL_CRATE_GRAB_CEILING = 75;

inline constexpr float ULDUAR_FL_CRATE_GRAB_RANGE = 49.0f;

inline constexpr uint32 ULDUAR_FL_CRATE_DETOUR_ENERGY = 30;
inline constexpr float ULDUAR_FL_CRATE_DETOUR_RADIUS = 60.0f;

inline constexpr float ULDUAR_FL_LIFT_DROP_RADIUS = 20.0f;

inline constexpr float ULDUAR_FL_KITE_WALL_INSET = 15.0f;
inline constexpr float ULDUAR_FL_KITE_CORNER_CHAMFER = 35.0f;
inline constexpr float ULDUAR_FL_KITE_ADVANCE_DIST = 30.0f;
inline constexpr float ULDUAR_FL_KITE_BOSS_CLEARANCE = 50.0f;

inline constexpr float ULDUAR_FL_KITE_FIRE_LOOKAHEAD = 30.0f;
inline constexpr float ULDUAR_FL_KITE_DETOUR_STEP = 25.0f;

inline constexpr float ULDUAR_FL_THROW_MAX_RANGE = 70.0f;
inline constexpr float ULDUAR_FL_THROW_APPROACH = 55.0f;
inline constexpr float ULDUAR_FL_THROW_LEAD_S = 1.0f;
inline constexpr float ULDUAR_FL_THROW_FACING_ARC = float(M_PI) / 6.0f;
inline constexpr uint32 ULDUAR_FL_CATAPULT_WAIT_MS = 10000;
inline constexpr uint32 ULDUAR_FL_SEAT_CLAIM_MS = 15000;
inline constexpr uint32 ULDUAR_FL_SEAT_CLAIM_GRACE_MS = 2000;

inline constexpr float ULDUAR_FL_PICKUP_ARRIVE = 3.0f;
inline constexpr uint32 ULDUAR_FL_PICKUP_TIMEOUT_MS = 20000;
inline constexpr float ULDUAR_FL_REGUN_RADIUS = 15.0f;
inline constexpr float ULDUAR_FL_FERRY_HOP_DIST = 10.0f;
inline constexpr float ULDUAR_FL_FERRY_ARRIVE = 4.0f;
inline constexpr float ULDUAR_FL_ON_FOOT_ARENA_MARGIN = 20.0f;

inline constexpr uint32 ULDUAR_FL_FIRST_AID_KIT_COOLDOWN_MS = 60000;
inline constexpr uint32 ULDUAR_FL_FIRST_AID_KIT_RETRY_MS = 2000;
inline constexpr float ULDUAR_FL_FIRST_AID_KIT_HEALTH_PCT = 80.0f;

inline constexpr uint32 ULDUAR_FL_FIRE_CANNON_POOL = 100;

Unit* FlameLeviathanBoss(PlayerbotAI* botAI);

void FlameLeviathanTick(PlayerbotAI* botAI);

bool FlameLeviathanEngaged(PlayerbotAI* botAI);

Unit* FlameLeviathanRiddenVehicle(Player* bot);

bool FlameLeviathanIsDriver(Player* bot);

Unit* FlameLeviathanVehicleToBoard(PlayerbotAI* botAI);

bool FlameLeviathanIsPursued(Player* bot);

bool FlameLeviathanIsVentChanneling(Unit* boss);

bool FlameLeviathanIsVentInterrupter(PlayerbotAI* botAI, Player* bot);

void FlameLeviathanClaimVentChannel(Player* bot);

uint32 FlameLeviathanMsToNextVent(Player* bot, Unit* boss);

bool FlameLeviathanVentWindowOpen(Player* bot, Unit* boss);

bool FlameLeviathanFaceForCone(Unit* vehicleBase, Unit* target, float halfAngle, float radius);

bool FlameLeviathanInCone(Unit* vehicleBase, Unit* target, float halfAngle, float radius);

bool FlameLeviathanCrewUsable(Player* member);

Unit* FlameLeviathanFrozenVehicle(Player* bot, Unit* from, float minRange, float maxRange);

Unit* FlameLeviathanPursuedVehicle(PlayerbotAI* botAI, Player* bot);

Unit* FlameLeviathanRamCentre(PlayerbotAI* botAI, Player* bot);

bool FlameLeviathanIsRamTarget(Player* bot);

bool FlameLeviathanInBatteringRamBlast(Unit* vehicleBase, Unit* centre);

bool FlameLeviathanShouldClearBatteringRam(PlayerbotAI* botAI, Player* bot);

bool FlameLeviathanIsTarLead(PlayerbotAI* botAI, Player* bot);

Unit* FlameLeviathanBestAdd(PlayerbotAI* botAI, Unit* from, float minRange, float maxRange, float splash,
                            Position const* around = nullptr, float aroundRadius = 0.0f);

int8 FlameLeviathanCornerPost(PlayerbotAI* botAI, Player* bot);

int8 FlameLeviathanHullCornerPost(PlayerbotAI* botAI, Player* bot, Unit* hull);

bool FlameLeviathanIsVentReserve(Player* bot);

Position FlameLeviathanCornerPostPoint(uint8 index);

bool FlameLeviathanInArena(Position const& pos, float margin = 0.0f);

float FlameLeviathanDemolisherStandDist(Unit* boss);

float FlameLeviathanHazardReach(Unit* hazard, Unit* vehicle);

bool FlameLeviathanFuryArmed(Player* bot, Unit* reticle);

Unit* GetFlameLeviathanNearestTowerHazard(PlayerbotAI* botAI, Unit* from, uint32 towerMask,
                                          float margin = ULDUAR_FL_TOWER_HAZARD_MARGIN, uint32 strictMask = 0);

void GetFlameLeviathanTowerHazards(PlayerbotAI* botAI, Unit* from, uint32 towerMask, float radius,
                                   std::vector<Unit*>& out);

Position FlameLeviathanRearPoint(Unit* boss, float standDist, float bearingOffset = 0.0f, float lead = 0.0f);

float FlameLeviathanStationLead(Unit* boss);

float FlameLeviathanStationBearingOffset(Player* bot, Unit* vehicleBase, float radius, float* fanHalf = nullptr);

float FlameLeviathanCentreTurn(Unit* boss, float fanHalf);

Position FlameLeviathanArenaCentre();
Position FlameLeviathanLeadPoint(Unit* boss, float standDist);

float FlameLeviathanTarLeadDistance(Unit* boss, Unit* pursued, float size);

std::vector<Position> const& FlameLeviathanKiteRing();

FlameLeviathanSeat FlameLeviathanSeatOf(Player* member);

bool FlameLeviathanShutdown(Unit* boss);

bool FlameLeviathanShutdownSoon(Player* bot);

bool FlameLeviathanWantsCatapult(PlayerbotAI* botAI, Player* bot);

void FlameLeviathanClaimSeat(Player* bot, Unit* demolisher);

Player* FlameLeviathanCatapultPassenger(Unit* demolisher);

Position FlameLeviathanThrowAim(Unit* boss);

Unit* FlameLeviathanRiderTurret(Player* bot);

Unit* FlameLeviathanPickupChopper(PlayerbotAI* botAI, Player* member);

Player* FlameLeviathanPickupFor(Player* driver);

Player* FlameLeviathanChopperPassenger(Unit* chopper);

Unit* FlameLeviathanFerryTarget(PlayerbotAI* botAI, Player* passenger, Unit* from);

bool FlameLeviathanGunnerless(Unit* demolisher);

bool FlameLeviathanCrateTaken(Player* bot, Unit* crate);

void FlameLeviathanTakeCrate(Player* bot, Unit* crate);

#endif
