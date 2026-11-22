/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERXT002_H
#define PLAYERBOTS_ULDENCOUNTERXT002_H

#include <string>
#include <vector>

#include "Position.h"
#include "UldData.h"

class Player;
class PlayerbotAI;
class Unit;

enum UlduarXT002Ids
{
    PB_NPC_XT002_PUMMELLER = 33344,
    PB_NPC_XT002_BOOMBOT = 33346,
    PB_NPC_XT002_LIFE_SPARK = 34004,
    PB_NPC_XT002_VOID_ZONE = 34001,
    SPELL_XT002_SEARING_LIGHT = 63018,
    SPELL_XT002_GRAVITY_BOMB = 63024,
    SPELL_XT002_HEARTBREAK = 65737,
    SPELL_XT002_EXPOSED_HEART = 63849,
    SPELL_XT002_SUBMERGE = 37751,
};

inline constexpr float ULDUAR_XT002_DEBUFF_CLEAR_RADIUS = 25.0f;

inline constexpr float ULDUAR_XT002_GRAVITY_BOMB_PULL_RADIUS = 20.0f;

inline constexpr float ULDUAR_XT002_BOOMBOT_AVOID_RADIUS = 12.0f;

inline constexpr float ULDUAR_XT002_VOID_ZONE_RADIUS = 6.0f;

inline constexpr float ULDUAR_XT002_MAINTANK_SPOT_TOLERANCE = 3.0f;
inline constexpr float ULDUAR_XT002_RANGED_SPOT_TOLERANCE = 5.0f;

inline constexpr float ULDUAR_XT002_RANGED_RING_OFFSET_Y = 6.0f;
inline constexpr float ULDUAR_XT002_RANGED_RING_INNER_X = 6.0f;
inline constexpr float ULDUAR_XT002_RANGED_RING_INNER_Y = 8.0f;
inline constexpr float ULDUAR_XT002_RANGED_RING_OUTER_X = 9.0f;
inline constexpr float ULDUAR_XT002_RANGED_RING_OUTER_Y = 12.0f;
inline constexpr size_t ULDUAR_XT002_RANGED_RING_INNER_SLOTS = 6;

inline constexpr float ULDUAR_XT002_HUNTER_ARC_RADIUS = 37.0f;

inline constexpr float ULDUAR_XT002_HUNTER_ARC_FIRST_BEARING_DEG = 175.0f;
inline constexpr float ULDUAR_XT002_HUNTER_ARC_BEARING_STEP_DEG = 10.0f;
inline constexpr size_t ULDUAR_XT002_HUNTER_ARC_SLOTS = 3;
inline constexpr float ULDUAR_XT002_HUNTER_ARC_ROW_STEP = 6.0f;

inline constexpr float ULDUAR_XT002_BOMB_GRID_STEP = 6.0f;
inline constexpr int ULDUAR_XT002_BOMB_GRID_X_CELLS = 5;
inline constexpr int ULDUAR_XT002_BOMB_GRID_Y_CELLS = 4;

inline constexpr int ULDUAR_XT002_BOMB_GRID_X_CELLS_MELEE = 7;

inline constexpr float ULDUAR_XT002_BOMB_CELL_ARRIVED = 2.0f;
inline constexpr float ULDUAR_XT002_BOMB_CELL_REENGAGE = 5.0f;

inline constexpr uint32 ULDUAR_XT002_BOMB_TRAVEL_MARGIN_MS = 1500;

inline constexpr float ULDUAR_XT002_BOMB_CELL_PREFERRED_CLEARANCE =
    ULDUAR_XT002_VOID_ZONE_RADIUS + ULDUAR_XT002_BOMB_CELL_ARRIVED;

inline constexpr float ULDUAR_XT002_BOMB_APPROACH_CLEARANCE = 7.5f;

inline constexpr float ULDUAR_XT002_ADD_LEASH_RADIUS = 60.0f;

inline constexpr float ULDUAR_XT002_RANGED_ENGAGE_RANGE = 35.0f;
inline constexpr float ULDUAR_XT002_MELEE_ENGAGE_RANGE = 15.0f;

inline constexpr float ULDUAR_XT002_TAUNT_RANGE = 30.0f;

inline constexpr float ULDUAR_XT002_VOID_ZONE_SEARCH_RADIUS = 110.0f;

inline constexpr size_t ULDUAR_XT002_MOVE_CANDIDATE_ATTEMPTS = 5;

inline constexpr float ULDUAR_XT002_SEARING_LIGHT_DETOUR = 10.0f;

inline constexpr float ULDUAR_XT002_SEARING_LIGHT_SLOT_CLEARANCE = 12.0f;

inline constexpr float ULDUAR_XT002_HEART_SAFE_HP_PCT = 15.0f;

inline constexpr float ULDUAR_XT002_FINAL_PUSH_HP_PCT = 25.0f;

inline constexpr uint32 ULDUAR_XT002_BUFF_TANK_SETTLE_MS = 5000;

inline constexpr uint32 ULDUAR_XT002_BUFF_TANK_STALE_MS = 10000;

inline constexpr float ULDUAR_XT002_THREAT_CAP = 0.9f;
inline constexpr float ULDUAR_XT002_THREAT_RELEASE = 0.8f;
inline constexpr float ULDUAR_XT002_AREA_THREAT_REACH = 25.0f;

static_assert(ULDUAR_XT002_THREAT_RELEASE < ULDUAR_XT002_THREAT_CAP && ULDUAR_XT002_THREAT_CAP < 1.1f,
              "the brake holds short of the 110% that takes XT off his holder");

inline constexpr float ULDUAR_XT002_BOMB_HEAL_MARGIN = 5.0f;

inline constexpr float ULDUAR_XT002_BOMB_HEAL_BEARING_STEP_DEG = 15.0f;
inline constexpr int ULDUAR_XT002_BOMB_HEAL_BEARING_STEPS = 3;

inline constexpr float ULDUAR_XT002_BOMB_HEAL_TOP_PCT = 90.0f;

extern const Position ULDUAR_XT002_SPAWN;
extern const Position ULDUAR_XT002_MAINTANK_SPOT;
extern const Position ULDUAR_XT002_RANGED_SPOT;
extern const Position ULDUAR_XT002_SEARING_LIGHT_SPOT;
extern const Position ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_MELEE;
extern const Position ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_RANGED_SOUTH;
extern const Position ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_RANGED_NORTH;

struct XT002BombLot
{
    Position origin;
    float yDirection;
    int xCells = ULDUAR_XT002_BOMB_GRID_X_CELLS;
    int yCells = ULDUAR_XT002_BOMB_GRID_Y_CELLS;
};

Unit* GetXT002(PlayerbotAI* botAI);

Unit* GetXT002ExposedHeart(PlayerbotAI* botAI);

bool IsXT002Submerged(Unit* xt002);

uint32 GetXT002SearingLightSpellId(Player* bot);
uint32 GetXT002GravityBombSpellId(Player* bot);
uint32 GetXT002HeartbreakSpellId(Player* bot);

struct XT002TankRoles
{
    Player* holder = nullptr;
    Player* pummeller = nullptr;
};

XT002TankRoles GetXT002TankRoles(Player* bot, Unit* xt002);

bool IsXT002PummellerTank(Player* bot, Unit* xt002);

Player* GetXT002BuffTank(Player* bot, Unit* xt002);

Player* GetXT002BuffTarget(PlayerbotAI* botAI, std::string const& spell);

bool IsXT002ThreatBraked(Player* bot, Unit* xt002);

struct XT002BombHealLatch
{
    Player* carrier = nullptr;
    bool sent = false;
};

XT002BombHealLatch GetXT002BombHealLatch(Player* bot);

Player* GetXT002BombHealTarget(Player* bot);

std::vector<Player*> GetXT002BombCarriers(Player* bot);

bool IsXT002AddEngageable(Unit* xt002, Unit* unit);

Unit* GetXT002EngageableAdd(PlayerbotAI* botAI, Player* bot, uint32 entry, float botReach);

bool IsXT002AddEvading(Unit* unit);

bool GetXT002RangedSlot(Player* bot, Unit* xt002, Position& out);

std::vector<Position> GetXT002OtherFormationSlots(Player* bot);

std::vector<XT002BombLot> GetXT002BombLots(Player* bot);

#endif
