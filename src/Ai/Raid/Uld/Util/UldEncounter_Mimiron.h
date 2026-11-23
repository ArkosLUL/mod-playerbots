/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERMIMIRON_H
#define PLAYERBOTS_ULDENCOUNTERMIMIRON_H

#include "ObjectGuid.h"
#include "Position.h"
#include "UldData.h"

#include <string>
#include <vector>

class Creature;
class Player;
class PlayerbotAI;
class Spell;
class Unit;

enum UlduarMimironIds
{
    NPC_LEVIATHAN_MKII = 33432,
    NPC_LEVIATHAN_MKII_CANNON = 34071,
    NPC_VX001 = 33651,
    NPC_AERIAL_COMMAND_UNIT = 33670,
    NPC_BOMB_BOT = 33836,
    NPC_ROCKET_STRIKE_N = 34047,
    NPC_ASSAULT_BOT = 34057,
    NPC_PROXIMITY_MINE = 34362,
    NPC_MIMIRON_DB_TARGET = 33576,
    NPC_JUNK_BOT = 33855,
    NPC_MAGNETIC_CORE = 34068,
    SPELL_P3WX2_LASER_BARRAGE_1 = 63293,
    SPELL_P3WX2_LASER_BARRAGE_2 = 63297,
    SPELL_SPINNING_UP = 63414,
    SPELL_SHOCK_BLAST = 63631,
    SPELL_P3WX2_LASER_BARRAGE_3 = 64042,
    SPELL_P3WX2_LASER_BARRAGE_AURA_1 = 63274,
    SPELL_P3WX2_LASER_BARRAGE_AURA_2 = 63300,
    SPELL_MIMIRON_PLASMA_BLAST = 62997,
    SPELL_MIMIRON_PLASMA_BLAST_25 = 64529,
    SPELL_MIMIRON_NAPALM_SHELL = 63666,
    SPELL_MIMIRON_MAGNETIC_FIELD = 64668,
    ITEM_MIMIRON_MAGNETIC_CORE = 46029,
    SPELL_MIMIRON_MAGNETIC_CORE_AURA = 64436,

    NPC_FLAMES_SPREAD = 34121,
    SPELL_MIMIRON_FLAMES_AURA = 64561,
    NPC_FROST_BOMB = 34149,
    NPC_EMERGENCY_FIRE_BOT = 34147,

    SPELL_MIMIRON_RAPID_BURST = 63382
};

inline constexpr float ULDUAR_MIMIRON_BARRAGE_HALF_ANGLE = 52.0f * static_cast<float>(M_PI) / 180.0f;
inline constexpr float ULDUAR_MIMIRON_BARRAGE_MARGIN = 15.0f * static_cast<float>(M_PI) / 180.0f;

inline constexpr float ULDUAR_MIMIRON_BARRAGE_BOUNDARY = 3.0f;

inline constexpr float ULDUAR_MIMIRON_DB_TARGET_SPEED = 20.8988f;

inline constexpr float ULDUAR_MIMIRON_DB_TARGET_SEARCH_RANGE = 250.0f;

inline constexpr float ULDUAR_MIMIRON_BARRAGE_FIRE_SECONDS = 10.0f;

inline constexpr float ULDUAR_MIMIRON_BARRAGE_STEP = 40.0f * static_cast<float>(M_PI) / 180.0f;

inline constexpr float ULDUAR_MIMIRON_BARRAGE_RING_MARGIN = 6.0f;

inline constexpr float ULDUAR_MIMIRON_BARRAGE_MELEE_RING_MARGIN = 1.0f;

inline constexpr float ULDUAR_MIMIRON_BARRAGE_MELEE_RING_MIN = 5.0f;

inline constexpr float ULDUAR_MIMIRON_BARRAGE_RADIUS_SHIFTS[] = {0.0f, 2.0f, -2.0f, 4.0f, -4.0f, 6.0f, -6.0f, 8.0f,
                                                                 -8.0f};

inline constexpr float ULDUAR_MIMIRON_BARRAGE_IGNITION_BUFFER = 0.5f;

inline constexpr float ULDUAR_MIMIRON_BARRAGE_REFUGE_PROBE = 5.0f * static_cast<float>(M_PI) / 180.0f;

inline constexpr float ULDUAR_MIMIRON_SPREAD_RADIUS_MAX = 24.0f;

inline constexpr float ULDUAR_MIMIRON_SPREAD_RADIUS = 22.0f;
inline constexpr float ULDUAR_MIMIRON_SPREAD_TOLERANCE = 5.0f;

inline constexpr uint32 ULDUAR_MIMIRON_WEDGE_REAIM_MS = 20000;

inline constexpr float ULDUAR_MIMIRON_SHIFT_SLACK = 4.0f;

inline constexpr float ULDUAR_MIMIRON_SPREAD_RANGE_MARGIN = 4.0f;

inline constexpr float ULDUAR_MIMIRON_ROOM_RADIUS = 40.0f;

inline constexpr float ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST = 18.0f;
inline constexpr float ULDUAR_MIMIRON_SHOCK_FLEE_MARGIN = 2.0f;

inline constexpr float ULDUAR_MIMIRON_HOLDER_RADIUS_SLACK = 7.0f;

inline constexpr float ULDUAR_MIMIRON_UNREACH_STEP_MARGIN = 4.0f;
inline constexpr float ULDUAR_MIMIRON_UNREACH_ARRIVED = 2.0f;

inline constexpr float ULDUAR_MIMIRON_PHASE3_SPACING = 6.0f;
inline constexpr float ULDUAR_MIMIRON_PHASE3_MIN_RADIUS = 18.0f;

inline constexpr float ULDUAR_MIMIRON_DISPERSE_DISTANCE = 5.5f;

inline constexpr float ULDUAR_MIMIRON_PHASE1_CAMP_FIRST_ROW = 21.0f;
inline constexpr float ULDUAR_MIMIRON_PHASE1_CAMP_SPACING = 6.0f;
inline constexpr uint32 ULDUAR_MIMIRON_PHASE1_CAMP_ROWS = 3;

inline constexpr float ULDUAR_MIMIRON_PHASE1_CAMP_HALF_ANGLE = 38.0f * static_cast<float>(M_PI) / 180.0f;

inline constexpr float ULDUAR_MIMIRON_CAMP_SIGHT_STEP = 4.0f * static_cast<float>(M_PI) / 180.0f;
inline constexpr float ULDUAR_MIMIRON_CAMP_SIGHT_MAX = 16.0f * static_cast<float>(M_PI) / 180.0f;
inline constexpr float ULDUAR_MIMIRON_CAMP_SIGHT_EYE = 2.0f;
inline constexpr uint32 ULDUAR_MIMIRON_CAMP_SIGHT_HOLD_MS = 15000;

inline constexpr float ULDUAR_MIMIRON_TANK_THREAT_LEAD = 1.3f;

inline constexpr uint32 ULDUAR_MIMIRON_TANK_HOLD_MAX_MS = 10000;

inline constexpr float ULDUAR_MIMIRON_STACK_FIRE_RADIUS = 10.0f;

inline constexpr uint32 ULDUAR_MIMIRON_STACK_FIRE_LIMIT = 2;
inline constexpr uint32 ULDUAR_MIMIRON_STACK_FIRE_MARGIN = 2;
inline constexpr uint32 ULDUAR_MIMIRON_STACK_HOLD_MS = 15000;

inline constexpr uint32 ULDUAR_MIMIRON_PLASMA_WINDOW_MS = 12000;

inline constexpr float ULDUAR_MIMIRON_PHASE3_WEDGE_HALF_ANGLE = 60.0f * static_cast<float>(M_PI) / 180.0f;

inline constexpr float ULDUAR_MIMIRON_CORE_LOOT_RANGE = 5.0f;
inline constexpr float ULDUAR_MIMIRON_CORE_USE_RANGE = 12.0f;

inline constexpr uint32 ULDUAR_MIMIRON_CORE_BANK = 2;

inline constexpr uint32 ULDUAR_MIMIRON_CORE_HOLD_EXPIRY_MS = 10000;
inline constexpr float ULDUAR_MIMIRON_CORE_HOLD_RELEASE_PCT = 25.0f;

inline constexpr float ULDUAR_MIMIRON_CORE_SEARCH_RANGE = 90.0f;

inline constexpr float ULDUAR_MIMIRON_CORE_PENDING_RANGE = 100.0f;

inline constexpr uint32 ULDUAR_MIMIRON_CORE_ARM_MS = 6000;

inline constexpr float ULDUAR_MIMIRON_PHASE4_HOLD_PCT = 10.0f;

inline constexpr float ULDUAR_MIMIRON_PHASE4_FOCUS_BAND_PCT = 2.0f;

inline constexpr float ULDUAR_MIMIRON_PHASE4_CONVERGE_PCT = 4.0f;

inline constexpr float ULDUAR_MIMIRON_PHASE4_TANK_RINGS[] = {8.0f, 16.0f, 24.0f};
inline constexpr uint32 ULDUAR_MIMIRON_PHASE4_TANK_BEARINGS = 12;

inline constexpr float ULDUAR_MIMIRON_PHASE4_TANK_CLEAR_RADIUS = 14.0f;

inline constexpr uint32 ULDUAR_MIMIRON_PHASE4_TANK_FIRE_LIMIT = 3;
inline constexpr uint32 ULDUAR_MIMIRON_PHASE4_TANK_FIRE_MARGIN = 2;
inline constexpr uint32 ULDUAR_MIMIRON_PHASE4_TANK_HOLD_MS = 30000;

inline constexpr uint32 ULDUAR_MIMIRON_PHASE4_LUST_DELAY_MS = 15000;

inline constexpr float ULDUAR_MIMIRON_STAGING_SEARCH_RANGE = 200.0f;

inline constexpr float ULDUAR_MIMIRON_SPENT_HEALTH_PCT = 10.0f;

enum class MimironHandoverStep : uint8
{
    None = 0,
    Hold = 1,
    Late = 2,
    WalkIn = 3,
};

inline constexpr float ULDUAR_MIMIRON_HOLD_OUTER_RADIUS = 44.0f;
inline constexpr uint32 ULDUAR_MIMIRON_HOLD_ROWS = 3;
inline constexpr float ULDUAR_MIMIRON_HOLD_ROW_STEP = 4.0f;
inline constexpr float ULDUAR_MIMIRON_HOLD_SPACING = 4.0f;

inline constexpr uint32 ULDUAR_MIMIRON_HOLD_BEARINGS = 24;
inline constexpr float ULDUAR_MIMIRON_HOLD_CLEAR_RADIUS = 20.0f;

inline constexpr uint32 ULDUAR_MIMIRON_HANDOVER_STALE_MS = 1000;

inline constexpr float ULDUAR_MIMIRON_MINE_TRIGGER_RADIUS = 3.0f;
inline constexpr float ULDUAR_MIMIRON_MINE_CLEARANCE = 3.5f;
inline constexpr float ULDUAR_MIMIRON_MINE_MIN_STEP = 2.0f;
inline constexpr float ULDUAR_MIMIRON_MINE_MAX_STEP = 5.0f;
inline constexpr float ULDUAR_MIMIRON_MINE_LEG_CLEARANCE = 2.5f;
inline constexpr float ULDUAR_MIMIRON_MINE_CROSS_HEALTH_PCT = 70.0f;
inline constexpr float ULDUAR_MIMIRON_PATH_BEND_PAD = 5.0f;

inline constexpr float ULDUAR_MIMIRON_ROCKET_CLEARANCE = 8.0f;
inline constexpr float ULDUAR_MIMIRON_ROCKET_FLEE_MARGIN = 0.5f;

inline constexpr float ULDUAR_MIMIRON_ROCKET_RUN_RADIUS = 5.0f;

inline constexpr float ULDUAR_MIMIRON_HOLD_SEARCH_PAD = 5.0f;

inline constexpr float ULDUAR_MIMIRON_ROCKET_HOLD_MARGIN = 4.0f;

inline constexpr float ULDUAR_MIMIRON_NAPALM_RADIUS = 6.0f;
inline constexpr float ULDUAR_MIMIRON_BOMB_BOT_RADIUS = 8.0f;

inline constexpr float ULDUAR_MIMIRON_BOMB_BOT_SNARE_MIN_APPROACH = 15.0f;

inline constexpr float ULDUAR_MIMIRON_ROOM_SEARCH_RANGE = 100.0f;

inline constexpr float ULDUAR_MIMIRON_PHASE4_DISPERSE_DISTANCE = 4.0f;

inline constexpr uint32 ULDUAR_MIMIRON_OBS_SCAN_INTERVAL_MS = 250;

bool IsMimironSpotMineSafe(Player* bot, Position const& dest,
                           float clearance = ULDUAR_MIMIRON_MINE_CLEARANCE);

struct MimironFirefighterHazards
{
    std::vector<Position> flames;
    std::vector<Position> bombs;
    std::vector<Position> fireBots;
    std::vector<Position> sprayLanes;
};

MimironFirefighterHazards GetMimironFirefighterHazards(PlayerbotAI* botAI);

bool IsMimironSpotFireSafe(MimironFirefighterHazards const& hazards, Position const& dest);
bool IsMimironSpotBombSafe(MimironFirefighterHazards const& hazards, Position const& dest);

bool IsMimironSpotBurning(MimironFirefighterHazards const& hazards, Position const& pos);

bool IsMimironSpotFireBotSafe(Player* bot, MimironFirefighterHazards const& hazards, Position const& dest,
                              float sirenRadius, float sprayHalfWidth);
bool IsMimironSpotInFireBotSpray(MimironFirefighterHazards const& hazards, Position const& dest,
                                 float halfWidth);

std::vector<ObjectGuid> GetMimironKeptFireBots(PlayerbotAI* botAI, Player* bot);
bool IsMimironFireBotProtected(PlayerbotAI* botAI, Player* bot, Unit* fireBot);

bool IsMimironSpotShockSafe(PlayerbotAI* botAI, Position const& dest);

float GetMimironHolderRadius(PlayerbotAI* botAI, Player* bot);
bool IsMimironSpotHolderSafe(float holderRadius, Position const& dest);

Unit* GetMimironUnreachableMech(PlayerbotAI* botAI, Player* bot);

bool IsMimironSpotRocketSafe(PlayerbotAI* botAI, Position const& dest);

bool IsMimironSpotSafe(Player* bot, Position const& dest);

bool IsMimironWalkFireSafe(Player* bot, MimironFirefighterHazards const& hazards, Position const& dest);

std::vector<Position> GetMimironMines(PlayerbotAI* botAI);

bool IsMimironLegMineSafe(Position const& from, std::vector<Position> const& mines, Position const& dest);

bool IsMimironWalkMineSafe(Player* bot, std::vector<Position> const& mines, Position const& dest);

bool IsMimironPathMineSafe(Player* bot, std::vector<Position> const& mines, Position const& dest);

struct MimironApproach
{
    Position dest;
    char const* how;
    float turn;
    float burn = 0.0f;
    bool overMines = false;
};

std::vector<MimironApproach> GetMimironSlotApproaches(PlayerbotAI* botAI, Player* bot, Position const& slot,
                                                      MimironFirefighterHazards const& hazards);

bool IsMimironTankAnchorSlot(PlayerbotAI* botAI, Player* bot);

bool ClaimMimironPlasmaWindow(Player* bot);

bool IsMimironTankDragReady(PlayerbotAI* botAI, Player* bot);

Position const& GetMimironPhase1StackAnchor(PlayerbotAI* botAI, Player* bot);

void ResetMimironFightState(Player* bot);

void NoteMimironFireDodge(Player* bot);
bool IsMimironFireHoldActive(Player* bot);

bool IsMimironNearFrostBomb(PlayerbotAI* botAI);
bool IsMimironNearRocket(PlayerbotAI* botAI);
bool IsMimironHeldByFire(PlayerbotAI* botAI);

bool GetMimironTargetApproach(PlayerbotAI* botAI, Player* bot, Unit* target, float range,
                              Position& out);

bool GetMimironSightApproach(PlayerbotAI* botAI, Player* bot, Unit* target, float range,
                             Position& out);

float GetMimironApproachRange(PlayerbotAI* botAI, Player* bot);

float GetMimironPhase1DisperseDistance(PlayerbotAI* botAI);

bool IsMimironAcuGrounded(PlayerbotAI* botAI);

bool IsMimironAcuAirborne(PlayerbotAI* botAI, Player* bot);

Creature* GetMimironCoreCorpse(Player* bot);

bool IsMimironCoreFetchDue(PlayerbotAI* botAI, Player* bot);

bool TakeMimironCore(Player* bot, Creature* corpse);

bool IsMimironCoreUseReady(PlayerbotAI* botAI, Player* bot);

void NoteMimironCoreSpent(Player* bot);

std::string GetMimironBombBotSnare(Player* bot);

float GetMimironBombBotApproach(Player* bot, Unit* bombBot);

Unit* GetMimironBombBotChasing(PlayerbotAI* botAI, Player* bot);

Unit* GetMimironRingFocus(PlayerbotAI* botAI);

Unit* GetMimironStagingFocus(Player* bot);

bool IsNearMimironRoom(Player* bot);

MimironHandoverStep GetMimironHandoverStep(PlayerbotAI* botAI, Player* bot);

void MimironTick(PlayerbotAI* botAI);

bool IsMimironEngaged(PlayerbotAI* botAI);

bool IsMimironPhase4(Player* bot);

bool MimironPhase1Active(PlayerbotAI* botAI);

bool IsMimironPhase2(PlayerbotAI* botAI);

inline constexpr uint32 ULDUAR_MIMIRON_STORM_COOLDOWN_LOW_COUNT = 6;

bool IsMimironPhase3(PlayerbotAI* botAI);

Player* GetMimironFrostResistancePaladin(PlayerbotAI* botAI, Player* bot);

Unit* GetMimironPhase4Focus(PlayerbotAI* botAI, Player* bot, bool melee);

Unit* GetMimironPhase4TankFocus(PlayerbotAI* botAI, Player* bot);

Player* GetMimironCoreCarrier(PlayerbotAI* botAI);

float GetMimironSpinningUpSeconds(Unit* vx001);

struct MimironBarrageWindow
{
    bool valid = false;
    float lead = 0.0f;
    float sweep = 0.0f;
    float rate = 0.0f;
    float untilLive = 0.0f;
};

MimironBarrageWindow GetMimironBarrageWindow(Player* bot, Unit* vx001);

bool IsMimironSpotBarrageSafe(Unit* vx001, MimironBarrageWindow const& window, Position const& dest,
                              float travelSeconds);

bool GetMimironSpreadSlot(PlayerbotAI* botAI, Player* bot, Position& out);

Spell* GetMimironPlasmaBlastCast(Unit* cannon);

struct MimironRapidBurstWindow
{
    bool valid = false;
    float centreline = 0.0f;
};

MimironRapidBurstWindow GetMimironRapidBurstWindow(PlayerbotAI* botAI, Player* bot, Unit* vx001);

bool IsMimironSpotRapidBurstSafe(Unit* vx001, MimironRapidBurstWindow const& window,
                                 Position const& dest);

struct MimironBurstSector
{
    bool valid = false;
    float centre = 0.0f;
    float halfWidth = 0.0f;
};

MimironBurstSector GetMimironOtherGroupSector(PlayerbotAI* botAI, Player* bot, Unit* vx001);

bool IsMimironSpotInSector(Unit* vx001, MimironBurstSector const& sector, Position const& dest);

inline constexpr float ULDUAR_MIMIRON_FLAMES_RADIUS = 5.0f;

inline constexpr float ULDUAR_MIMIRON_FLAMES_BURN_RADIUS = 3.5f;

inline constexpr float ULDUAR_MIMIRON_FLAMES_CROSS_MAX = 4.0f;

inline constexpr float ULDUAR_MIMIRON_FLAMES_CROSS_HEALTH_PCT = 75.0f;

inline constexpr uint32 ULDUAR_MIMIRON_STAND_IN_BEARINGS = 12;
inline constexpr float ULDUAR_MIMIRON_STAND_IN_BEARING_STEP = static_cast<float>(M_PI) / 6.0f;
inline constexpr float ULDUAR_MIMIRON_STAND_IN_RING_STEP = 2.0f;

inline constexpr float ULDUAR_MIMIRON_SLOT_SUBSTITUTE_RADIUS = 6.0f;

inline constexpr float ULDUAR_MIMIRON_SLOT_SUBSTITUTE_RADIUS_WIDE = 12.0f;

inline constexpr float ULDUAR_MIMIRON_DETOUR_TURNS_DEG[] = {20.0f, 35.0f, 50.0f, 65.0f};

inline constexpr float ULDUAR_MIMIRON_DRINK_FIRE_CLEARANCE = 15.0f;

inline constexpr float ULDUAR_MIMIRON_FROST_BOMB_RADIUS = 30.0f;
inline constexpr float ULDUAR_MIMIRON_FROST_BOMB_STAND_RADIUS = 32.0f;
inline constexpr float ULDUAR_MIMIRON_FROST_BOMB_CLEARANCE = 34.0f;
inline constexpr float ULDUAR_MIMIRON_FROST_BOMB_HOLD_MARGIN = 4.0f;

inline constexpr float ULDUAR_MIMIRON_FIREBOT_CLEANUP_PCT = 15.0f;
inline constexpr uint32 ULDUAR_MIMIRON_JUNK_BOT_PILE = 4;

inline constexpr float ULDUAR_MIMIRON_FIREBOT_SPRAY_LENGTH = 18.0f;
inline constexpr float ULDUAR_MIMIRON_FIREBOT_SPRAY_HALF_WIDTH = 3.5f;
inline constexpr float ULDUAR_MIMIRON_FIREBOT_SPRAY_STAND_HALF_WIDTH = 4.5f;
inline constexpr float ULDUAR_MIMIRON_FIREBOT_SPRAY_SIDESTEP = 1.5f;
inline constexpr float ULDUAR_MIMIRON_FIREBOT_SPRAY_REACH = 5.0f;
inline constexpr float ULDUAR_MIMIRON_FIREBOT_SIREN_CLEARANCE = 13.0f;
inline constexpr float ULDUAR_MIMIRON_FIREBOT_SIREN_STAND = 17.0f;
inline constexpr float ULDUAR_MIMIRON_FIREBOT_SIREN_FLEE = 19.0f;
inline constexpr float ULDUAR_MIMIRON_FIREBOT_AOE_CLEARANCE = 12.0f;

inline constexpr float ULDUAR_MIMIRON_FLAMES_DODGE_HEALTH_PCT = 60.0f;
inline constexpr float ULDUAR_MIMIRON_FLAMES_PARKED_DODGE_HEALTH_PCT = 80.0f;

inline constexpr uint32 ULDUAR_MIMIRON_FLAMES_DODGE_NODE_OVERRIDE = 2;

inline constexpr uint32 ULDUAR_MIMIRON_FLAMES_HOLD_MS = 2500;

inline constexpr float ULDUAR_MIMIRON_APPROACH_INSET = 2.0f;

inline constexpr float ULDUAR_MIMIRON_FLAMES_STEP_LADDER[] = {3.0f, 5.0f, 7.0f, 10.0f};

inline constexpr float ULDUAR_MIMIRON_FLAMES_MAX_HOP = 12.0f;

inline constexpr float ULDUAR_MIMIRON_FLEE_FAN_STEP = static_cast<float>(M_PI) / 8.0f;
inline constexpr float ULDUAR_MIMIRON_FLEE_FAN_MAX = 5.0f * ULDUAR_MIMIRON_FLEE_FAN_STEP;

inline constexpr float ULDUAR_MIMIRON_FLEE_MIN_PROGRESS_PCT = 0.5f;

inline constexpr float ULDUAR_MIMIRON_RAPID_BURST_HALF_ANGLE = 30.0f * static_cast<float>(M_PI) / 180.0f;

inline constexpr float ULDUAR_MIMIRON_PHASE2_MELEE_HALF_ANGLE = 45.0f * static_cast<float>(M_PI) / 180.0f;

inline constexpr float ULDUAR_MIMIRON_PHASE2_MELEE_REACH_MARGIN = 2.0f;

extern const Position ULDUAR_MIMIRON_ROOM_CENTER;
extern const Position ULDUAR_MIMIRON_PHASE3_STAGE;
extern const Position ULDUAR_MIMIRON_PHASE4_TANK_SPOT;

extern const Position ULDUAR_MIMIRON_PHASE1_TANK_SPOT;

inline constexpr uint8 ULDUAR_MIMIRON_PHASE1_STACK_COUNT = 2;
extern const Position ULDUAR_MIMIRON_PHASE1_STACK_SPOTS[ULDUAR_MIMIRON_PHASE1_STACK_COUNT];

#endif
