/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_RAIDBOSSHELPERS_H
#define PLAYERBOTS_RAIDBOSSHELPERS_H

#include <utility>
#include <vector>

#include "AiObject.h"
#include "Position.h"
#include "Unit.h"

bool MarkTargetWithIcon(Player* bot, Unit* target, uint8 iconId);
bool MarkTargetWithSkull(Player* bot, Unit* target);
bool MarkTargetWithSquare(Player* bot, Unit* target);
bool MarkTargetWithStar(Player* bot, Unit* target);
bool MarkTargetWithCircle(Player* bot, Unit* target);
bool MarkTargetWithDiamond(Player* bot, Unit* target);
bool MarkTargetWithTriangle(Player* bot, Unit* target);
bool MarkTargetWithCross(Player* bot, Unit* target);
bool MarkTargetWithMoon(Player* bot, Unit* target);
bool ClearTargetIcon(Player* bot, uint8 iconId);
void SetRtiTarget(PlayerbotAI* botAI, const std::string& rtiName, Unit* target);
void SetRtiCcTarget(PlayerbotAI* botAI, const std::string& rtiName, Unit* target);
bool IsMechanicTrackerBot(PlayerbotAI* botAI, Player* bot, uint32 mapId, Player* exclude = nullptr);
Player* GetGroupMainTank(PlayerbotAI* botAI, Player* bot);
Player* GetGroupAssistTank(PlayerbotAI* botAI, Player* bot, uint8 index);
Unit* GetFirstAliveUnitByEntry(
    PlayerbotAI* botAI, uint32 entry);
// Feign death (Stalagg/Feugen) keeps the creature alive at 1 HP but unselectable and lying down,
// so IsAlive() on its own no longer means "still up".
bool IsDownOrFeigning(Unit const* unit);
Player* GetNearestPlayerInRadius(Player* bot, float radius);
bool IsBotInFrontalCone(Player* bot, Unit* source, float coneAngle, float range);
bool IsMechanicTrackerBot(Player* bot, uint32 mapId);
std::vector<Position> GetDynamicObjectPositions(Player* bot, float searchRadius, uint32 spellId);
// A hazard and the distance a bot has to keep from it.
using HazardCircle = std::pair<Position, float>;

// Nearest spot at least clearRadius from every hazard. Use instead of MovementAction::FleePosition
// for anything wider than a few yards: that one silently clamps its travel to
// AiPlayerbot.FleeDistance and cannot clear a large blast. Returns Position() when nothing inside
// maxRadius is clear.
Position FindNearestPositionClearOfHazards(Player* bot, std::vector<Position> const& hazards, float clearRadius,
                                           float maxRadius, float distanceStep = 2.0f,
                                           float angleStep = static_cast<float>(M_PI) / 8.0f);
// Same sweep with a clear radius per hazard, for an encounter that drops pools of two different
// sizes: clearing them all to the larger one buys safety with movement, and a moving bot cannot cast.
//
// preferNear breaks the tie inside whichever ring first has a clear spot. Without it the sweep takes
// the first angle that passes, which is a fixed compass direction and has nothing to do with where the
// bot wants to end up - on Hodir that walked melee a yard further out of melee range per hop.
Position FindNearestPositionClearOfHazards(Player* bot, std::vector<HazardCircle> const& hazards, float maxRadius,
                                           float distanceStep = 2.0f,
                                           float angleStep = static_cast<float>(M_PI) / 8.0f,
                                           Position const* preferNear = nullptr);
Position GetPositionOutsideFrontalCone(Player* bot, Unit* source, float coneAngle, float margin = M_PI / 12.0f);
void CommandPetAttack(PlayerbotAI* botAI, Unit* target);
void StopPet(PlayerbotAI* botAI);

#endif
