/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_PUTRICIDE_H
#define PLAYERBOTS_ICCACTIONS_PUTRICIDE_H

#include "Action.h"
#include "AttackAction.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ICCData.h"
#include "ICCHelpers_Putricide.h"
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

class IccPutricideMutatedPlagueAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc putricide mutated plague";

    IccPutricideMutatedPlagueAction(PlayerbotAI* botAI, std::string const name = Name)
        : AttackAction(botAI, name) {}
    bool Execute(Event event) override;
};

class IccPutricideGrowingOozePuddleAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc putricide growing ooze puddle";

    IccPutricideGrowingOozePuddleAction(PlayerbotAI* botAI, std::string const name = Name)
        : AttackAction(botAI, name) {}
    bool Execute(Event event) override;

    Unit* FindClosestThreateningPuddle();
    Position CalculateSafeMovePosition(Unit* closestPuddle);
    bool IsPositionTooCloseToOtherPuddles(float x, float y, Unit* ignorePuddle);
    bool PathCrossesAnyPuddle(float fromX, float fromY, float toX, float toY, Unit* ignorePuddle);
};

class IccPutricideVolatileOozeAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc putricide volatile ooze";

    IccPutricideVolatileOozeAction(PlayerbotAI* botAI, std::string const name = Name)
        : AttackAction(botAI, name) {}
    bool Execute(Event event) override;

    bool MarkOozeWithSkull(Unit* ooze);
    Unit* FindAuraTarget();
};

class IccPutricideGasCloudAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc putricide gas cloud";

    IccPutricideGasCloudAction(PlayerbotAI* botAI, std::string const name = Name)
        : AttackAction(botAI, name) {}
    bool Execute(Event event) override;

    bool HandleGaseousBloatMovement(Unit* gasCloud);
    bool HandleGroupAuraSituation(Unit* gasCloud);
    bool GroupHasGaseousBloat(Group* group);
};

class IccPutricideAvoidMalleableGooAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc putricide avoid malleable goo";

    IccPutricideAvoidMalleableGooAction(PlayerbotAI* botAI, std::string const name = Name)
        : MovementAction(botAI, name) {}
    bool Execute(Event event) override;

    bool HandleTankPositioning(Unit* boss);
    bool HandleUnboundPlague(Unit* boss);
    bool HandleBossPositioning(Unit* boss);
    bool HasObstacleBetween(Position const& from, Position const& to);
    bool IsOnPath(Position const& from, Position const& to, Position const& point, float threshold);
};

class IccPutricideAbominationAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc putricide abomination";

    IccPutricideAbominationAction(PlayerbotAI* botAI, std::string const name = Name)
        : AttackAction(botAI, name) {}
    bool Execute(Event event) override;

private:
    bool BecomeAbomination();
    bool IsSomeoneAlreadyPiloting();
    Unit* FindClosestPuddle(float maxRange);
    Unit* PickSlashTarget(Unit* boss);
    bool TryRegurgitate(Unit* abo, Unit* target);
    bool TryEatOoze(Unit* abo, Unit* puddle);
};

#endif
