/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERIGNIS_H
#define PLAYERBOTS_ULDENCOUNTERIGNIS_H

#include "Position.h"
#include "UldData.h"

class Creature;
class Player;
class PlayerbotAI;
class Unit;
class WorldObject;

enum UlduarIgnisIds
{
    NPC_IGNIS = 33118,
    NPC_IGNIS_IRON_CONSTRUCT = 33121,
    NPC_IGNIS_SCORCHED_GROUND = 33123,
    SPELL_IGNIS_CONSTRUCT_INACTIVE = 38757,
    SPELL_IGNIS_SCORCH = 62546,
    SPELL_IGNIS_SCORCH_25 = 63474,
    SPELL_IGNIS_FLAME_JETS = 62680,
    SPELL_IGNIS_FLAME_JETS_25 = 63472,
    SPELL_IGNIS_MOLTEN = 62373,
    SPELL_IGNIS_BRITTLE_10 = 62382,
    SPELL_IGNIS_BRITTLE_25 = 67114,
    SPELL_IGNIS_SLAG_POT_10 = 62717,
    SPELL_IGNIS_SLAG_POT_25 = 63477,
    SPELL_IGNIS_STRENGTH_OF_THE_CREATOR = 64473,
};

inline constexpr float ULDUAR_IGNIS_WATER_BRITTLE_RADIUS = 18.0f;

inline constexpr float ULDUAR_IGNIS_WATER_STANDOFF = 12.0f;

inline constexpr float ULDUAR_IGNIS_SCORCHED_GROUND_AVOID_RADIUS = 15.0f;
inline constexpr float ULDUAR_IGNIS_SCORCHED_GROUND_PARK_DISTANCE = 3.0f;

inline constexpr float ULDUAR_IGNIS_SCORCHED_GROUND_INERT_WATER_RADIUS = 25.0f;

inline constexpr float ULDUAR_IGNIS_MOLTEN_AVOID_RADIUS = 15.0f;

inline constexpr float ULDUAR_IGNIS_ROOM_SEARCH_RADIUS = 200.0f;

inline constexpr float ULDUAR_IGNIS_SCORCH_SPAWN_RANGE = 20.0f;

inline constexpr float ULDUAR_IGNIS_TANK_RADIUS = 9.5f;
inline constexpr float ULDUAR_IGNIS_TANK_SPOT_TOLERANCE = 2.0f;

inline constexpr float ULDUAR_IGNIS_TANK_DRAG_LEG = 5.0f;

inline constexpr uint8 ULDUAR_IGNIS_TANK_ARC_SLOTS = 3;
inline constexpr float ULDUAR_IGNIS_TANK_ARC_STEP = static_cast<float>(M_PI) / 3.0f;

inline constexpr float ULDUAR_IGNIS_TANK_BEARING = -static_cast<float>(M_PI) / 2.0f;

inline constexpr uint32 ULDUAR_IGNIS_BRITTLE_MELEE_FALLBACK_MS = 7000;

inline constexpr float ULDUAR_IGNIS_SLAG_POT_HEAL_HP_PCT = 85.0f;

extern const Position ULDUAR_IGNIS_BOSS_ANCHOR;
extern const Position ULDUAR_IGNIS_WATER_POOL_WEST;
extern const Position ULDUAR_IGNIS_WATER_POOL_EAST;

Unit* GetIgnis(PlayerbotAI* botAI);

Unit* GetIgnisIf(PlayerbotAI* botAI, bool (*wanted)(Creature const*));

Unit* GetEngagedIgnis(PlayerbotAI* botAI);
bool IsIgnisEngaged(PlayerbotAI* botAI);

bool IsIgnisConstructActivated(Unit const* construct);

bool IsIgnisConstructMolten(Unit const* construct);

bool IsIgnisConstructBrittle(Unit const* construct);

Unit* GetIgnisBrittleConstruct(PlayerbotAI* botAI);
Unit* GetIgnisNearestMoltenConstruct(PlayerbotAI* botAI, WorldObject const* from);

Unit* GetIgnisDrivenConstruct(PlayerbotAI* botAI, Player* tank);

Unit* GetIgnisHeldConstruct(PlayerbotAI* botAI, Player* tank);

Unit* GetIgnisNearestScorchedGround(PlayerbotAI* botAI, WorldObject const* from);
Position const& GetIgnisNearestWaterPool(WorldObject const* from);

Unit* GetIgnisAssignedScorchedGround(PlayerbotAI* botAI, WorldObject const* from, int8 tankIndex);

int8 GetIgnisConstructTankIndex(Player* bot);

Position const& GetIgnisAssignedWaterPool(int8 tankIndex);

Position GetIgnisWaterApproach(Player* tank, Position const& pool);

Position GetIgnisMainTankPosition(Player* bot, Unit* boss);

Position GetIgnisMainTankLeg(Player* bot, Unit* boss, Position const& spot);

bool IsIgnisScorchWindow(Unit const* boss);

bool IsIgnisFlameJetsCasting(Unit const* boss);

Player* GetIgnisSlagPotVictim(PlayerbotAI* botAI);
bool IsIgnisSlagPotVictim(Player* bot);

#endif
