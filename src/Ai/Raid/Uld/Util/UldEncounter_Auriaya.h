/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERAURIAYA_H
#define PLAYERBOTS_ULDENCOUNTERAURIAYA_H

#include "Position.h"
#include "UldData.h"

#include <vector>

class Player;
class PlayerbotAI;
class Unit;
class WorldObject;

enum UlduarAuriayaIds
{
    NPC_AURIAYA_SANCTUM_SENTRY = 34014,
    NPC_AURIAYA_FERAL_DEFENDER = 34035,
    NPC_AURIAYA_SEEPING_FERAL_ESSENCE = 34098,
};

inline constexpr float ULDUAR_AURIAYA_AXIS_Z_PATHING_ISSUE_DETECT = 410.0f;

inline constexpr float ULDUAR_AURIAYA_SEEPING_ESSENCE_RADIUS = 7.0f;
inline constexpr float ULDUAR_AURIAYA_ESSENCE_CLEARANCE = 8.0f;

inline constexpr float ULDUAR_AURIAYA_SOAK_STAY_HALF_ANGLE = static_cast<float>(M_PI) / 4.0f;
inline constexpr float ULDUAR_AURIAYA_SOAK_STAY_MIN = 8.0f;
inline constexpr float ULDUAR_AURIAYA_SOAK_STAY_MAX = 28.0f;
inline constexpr float ULDUAR_AURIAYA_SOAK_TARGET_HALF_ANGLE = static_cast<float>(M_PI) / 6.0f;
inline constexpr float ULDUAR_AURIAYA_SOAK_TARGET_MIN = 12.0f;
inline constexpr float ULDUAR_AURIAYA_SOAK_TARGET_MAX = 24.0f;
inline constexpr float ULDUAR_AURIAYA_SOAK_SPOT_TOLERANCE = 1.0f;
inline constexpr float ULDUAR_AURIAYA_MAINTANK_SPOT_TOLERANCE = 3.0f;

inline constexpr int ULDUAR_AURIAYA_STATION_COUNT = 3;
inline constexpr float ULDUAR_AURIAYA_STATION_FOUL_RADIUS = 10.0f;
inline constexpr float ULDUAR_AURIAYA_ROOM_SEARCH_RADIUS = 100.0f;

inline constexpr float ULDUAR_AURIAYA_ESSENCE_LEASH = 12.0f;
inline constexpr float ULDUAR_AURIAYA_MELEE_DEFENDER_RANGE = 15.0f;

inline constexpr float ULDUAR_AURIAYA_SENTRY_TAUNT_REACH = 25.0f;
inline constexpr float ULDUAR_AURIAYA_SENTRY_TAUNT_WALK = 20.0f;
inline constexpr float ULDUAR_AURIAYA_MAINTANK_TAUNT_REACH = 30.0f;
inline constexpr int ULDUAR_AURIAYA_MAINTANK_MAX_SENTRIES = 2;
inline constexpr float ULDUAR_AURIAYA_PACK_RADIUS = 30.0f;

extern const Position ULDUAR_AURIAYA_MAINTANK_SPOTS[];

bool AuriayaFearWindowActive(PlayerbotAI* botAI);

Unit* GetAuriaya(PlayerbotAI* botAI);
bool AuriayaEncounterActive(PlayerbotAI* botAI);

bool IsAuriayaEngaged(PlayerbotAI* botAI);

bool AuriayaSentriesUnengaged(PlayerbotAI* botAI);

Unit* GetAuriayaFocusTarget(PlayerbotAI* botAI);

Unit* GetAuriayaLooseSentry(PlayerbotAI* botAI, Player* tank);

bool InAuriayaSentryTauntReach(Player* tank, Unit* sentry);

Unit* GetAuriayaOffTankSentry(Player* bot);

Unit* GetAuriayaOffTankHoldSentry(Player* tank);

bool AuriayaOffTankHoldsSentries(PlayerbotAI* botAI);

bool AuriayaMainTankAlone(PlayerbotAI* botAI);

Unit* GetAuriayaMainTankSentry(Player* tank);

std::vector<Unit*> CollectAuriayaEssencePools(WorldObject* from, float radius);

std::vector<Unit*> GetAuriayaRoomPools(PlayerbotAI* botAI);

float GetAuriayaPoolClearance(std::vector<Unit*> const& pools, float x, float y);

bool IsInAuriayaPool(Player* bot);

int GetAuriayaStationIndex(std::vector<Unit*> const& roomPools);

bool GetAuriayaSoakSpot(PlayerbotAI* botAI, Player* bot, Unit* boss, std::vector<Unit*> const& pools,
                        Position& out);

bool PickAuriayaSoakSpotFromBoss(Unit* boss, std::vector<Unit*> const& pools, Position& out);

bool GetAuriayaAnchor(PlayerbotAI* botAI, Player* bot, Unit* boss, std::vector<Unit*> const& roomPools,
                      Position& out, float& tolerance);

#endif
