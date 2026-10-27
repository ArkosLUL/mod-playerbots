/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_BLOODQUEEN_H
#define PLAYERBOTS_ICCACTIONS_BLOODQUEEN_H

#include "Action.h"
#include "AttackAction.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ICCData.h"
#include "ICCHelpers_BloodQueen.h"
#include "ICCHelpers_Shared.h"
#include "ICCScripts.h"
#include "LastMovementValue.h"
#include "MovementActions.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "Vehicle.h"
#include <set>

class IccBqlGroupPositionAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc bql group position";

    IccBqlGroupPositionAction(PlayerbotAI* botAI)
        : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;

    bool HandleTankPosition(Unit* boss, Aura* frenzyAura, Aura* shadowAura);
    bool HandleShadowsMovement();
    Position AdjustControlPoint(Position const& wall, Position const& center, float factor);
    Position CalculateBezierPoint(float t, const Position path[4]);
    bool HandleGroupPosition(Unit* boss, Aura* frenzyAura, Aura* shadowAura);

private:
    // Evaluate curves
    struct CurveInfo
    {
        Position moveTarget;
        int curveIdx = 0;
        bool foundSafe = false;
        float minDist = 0.0f;
        float score = 0.0f;
        Position closestPoint;
        float t_closest = 0.0f;
    };
};

class IccBqlPactOfDarkfallenAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc bql pact of darkfallen";

    IccBqlPactOfDarkfallenAction(PlayerbotAI* botAI)
        : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;

    bool CalculateCenterPosition(Position& targetPos, std::vector<Player*> const& playersWithAura);
    bool MoveToTargetPosition(Position const& targetPos, int auraCount);
};

class IccBqlVampiricBiteAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc bql vampiric bite";

    IccBqlVampiricBiteAction(PlayerbotAI* botAI)
            : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;

    Player* FindBestBiteTarget(Group* group);
    bool IsInvalidTarget(Player* player);
    bool MoveTowardsTarget(Player* target);
    bool CastVampiricBite(Player* target);
};

#endif
