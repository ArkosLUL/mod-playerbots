/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ENCOUNTERHELPERS_H
#define PLAYERBOTS_ENCOUNTERHELPERS_H

#include "Common.h"
#include "Position.h"
#include <functional>
#include <string>
#include <utility>
#include <vector>

class Action;
class Player;
class PlayerbotAI;
class Unit;

namespace EncounterHelpers
{

// Cheap, rough proxies for how far along an encounter is. 95% HP means the boss and raid are
// positioned, the tank has threat, and the fight proper has started, so it's time to use cooldowns.
// 10% means the boss is almost dead, so ignore adds and finish off the boss.
inline constexpr float BOSS_ENGAGED_HEALTH_PCT = 95.0f;
inline constexpr float BOSS_BURN_HEALTH_PCT = 10.0f;

bool GetStepToPosition(
    Player* bot, Position const& position, float arrivalDist, Unit* facing, float& stepX,
    float& stepY, bool& backwards);
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
void SetRtiTarget(PlayerbotAI* botAI, std::string const& rtiName);
// Points "rti target" at an explicit unit as well as setting the mark, for an encounter that has to
// keep the focus on one creature rather than on whatever the bot is hitting.
void SetRtiTarget(PlayerbotAI* botAI, std::string const& rtiName, Unit* target);
// Drives the parallel "rti cc" value the per-class "cc" strategy reads, so a skull-marked kill target
// and a moon-marked CC target never compete for the same value.
void SetRtiCcTarget(PlayerbotAI* botAI, std::string const& rtiName, Unit* target);
bool IsMechanicTrackerBot(Player* bot, uint32 mapId);
Player* GetGroupMainTank(Player* bot);
Player* GetGroupAssistTank(Player* bot, uint8 index);
Unit* GetFirstAliveUnitByEntry(PlayerbotAI* botAI, uint32 entry); // DO NOT USE, WILL BE REMOVED
// Feign death (Stalagg/Feugen) keeps the creature alive at 1 HP but unselectable and lying down,
// so IsAlive() on its own no longer means "still up".
bool IsDownOrFeigning(Unit const* unit);
Player* GetNearestPlayerInRadius(Player* bot, float radius);
bool IsBotInFrontalCone(Player* bot, Unit* source, float coneAngle, float range);
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
//
// accept vetoes spots a circle can't describe, like a cone or a range cap. Asked after the collision
// check, so it judges the spot the bot would really stand on.
Position FindNearestPositionClearOfHazards(Player* bot, std::vector<HazardCircle> const& hazards, float maxRadius,
                                           float distanceStep = 2.0f,
                                           float angleStep = static_cast<float>(M_PI) / 8.0f,
                                           Position const* preferNear = nullptr,
                                           std::function<bool(float, float)> const& accept = {});
Position GetPositionOutsideFrontalCone(Player* bot, Unit* source, float coneAngle, float margin = M_PI / 12.0f);
void CommandPetAttack(PlayerbotAI* botAI, Unit* target);
void StopPet(PlayerbotAI* botAI);
bool IsDpsCooldownAction(Player* bot, Action* action);
bool IsTauntAction(Player* bot, Action* action);
bool IsAoeThreatAction(Player* bot, Action* action);

// Snaps a point onto walkable ground and clears the path to it. Raw formation geometry is exactly
// the shape that lands off the navmesh, and MoveTo would then fail without telling anyone.
Position ValidateFloorPoint(Player* bot, Position const& point);
// Class taunt. Non-tank classes return false.
bool CastClassTaunt(PlayerbotAI* botAI, Unit* target);

}

#endif
