/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERKOLOGARN_H
#define PLAYERBOTS_ULDENCOUNTERKOLOGARN_H

#include "Position.h"
#include "UldData.h"

class Player;
class PlayerbotAI;
class Unit;
class WorldObject;

enum UlduarKologarnIds
{
    NPC_LEFT_ARM = 32933,
    NPC_RIGHT_ARM = 32934,
    NPC_RUBBLE = 33768,
    NPC_KOLOGARN_EYEBEAM_LEFT = 33632,
    NPC_KOLOGARN_EYEBEAM_RIGHT = 33802,

    SPELL_CRUNCH_ARMOR = 63355,
    SPELL_CRUNCH_ARMOR_ALT = 64002,

    SPELL_STONE_GRIP_10 = 62166,
    SPELL_STONE_GRIP_25 = 63981,

    SPELL_FOCUSED_EYEBEAM_10_2 = 63346,
    SPELL_FOCUSED_EYEBEAM_10 = 63347,
    SPELL_FOCUSED_EYEBEAM_25_2 = 63976,
    SPELL_FOCUSED_EYEBEAM_25 = 63977,
};

inline constexpr float ULDUAR_KOLOGARN_AXIS_Z_PATHING_ISSUE_DETECT = 420.0f;
inline constexpr float ULDUAR_KOLOGARN_EYEBEAM_RADIUS = 3.0f;

inline constexpr float ULDUAR_KOLOGARN_ROOM_SEARCH_RADIUS = 100.0f;

inline constexpr float ULDUAR_KOLOGARN_WALKWAY_X_MIN = 1745.0f;
inline constexpr float ULDUAR_KOLOGARN_WALKWAY_X_MAX = 1780.0f;
inline constexpr float ULDUAR_KOLOGARN_WALKWAY_Y_MIN = -48.0f;
inline constexpr float ULDUAR_KOLOGARN_WALKWAY_Y_MAX = -2.0f;
inline constexpr float ULDUAR_KOLOGARN_WALKWAY_Z = 448.0f;

inline constexpr float ULDUAR_KOLOGARN_EYEBEAM_REACT_RADIUS = 8.0f;
inline constexpr float ULDUAR_KOLOGARN_EYEBEAM_SAFE_DISTANCE = 12.0f;
inline constexpr float ULDUAR_KOLOGARN_EYEBEAM_RUN_STEP = 10.0f;

inline constexpr float ULDUAR_KOLOGARN_RUBBLE_HOLD_OFFSET = 18.0f;

inline constexpr uint8 ULDUAR_KOLOGARN_CRUNCH_ARMOR_SWAP_STACKS = 2;

inline constexpr float ULDUAR_KOLOGARN_TAUNT_RANGE = 30.0f;

Unit* GetKologarn(PlayerbotAI* botAI);
Unit* GetKologarnRightArm(PlayerbotAI* botAI);

bool KologarnEncounterActive(PlayerbotAI* botAI);
Unit* GetKologarnNearestRubble(PlayerbotAI* botAI, WorldObject const* from);
bool KologarnHasRubble(PlayerbotAI* botAI);

Unit* GetKologarnLooseRubble(PlayerbotAI* botAI, Player* bot);

uint8 GetKologarnCrunchArmorStacks(Unit const* unit);

bool IsKologarnStoneGripped(Unit const* unit);

Player* GetKologarnBodyTank(PlayerbotAI* botAI);
bool IsKologarnBodyTank(PlayerbotAI* botAI, Player* bot);

bool IsKologarnOffTank(PlayerbotAI* botAI, Player* bot);

Unit* GetKologarnDpsTarget(PlayerbotAI* botAI, Player* bot);

Unit* GetKologarnOffTankTarget(PlayerbotAI* botAI, Player* bot);

Unit* GetKologarnEyebeamChasing(Player* bot);
Unit* GetKologarnNearestEyebeam(Player* bot, float radius);

Position GetKologarnRubbleHoldSpot(PlayerbotAI* botAI, Unit* rubble);

Position GetKologarnEyebeamEscapeStep(Player* bot, Unit* eye);

#endif
