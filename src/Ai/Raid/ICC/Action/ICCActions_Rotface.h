/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_ROTFACE_H
#define PLAYERBOTS_ICCACTIONS_ROTFACE_H

#include "Action.h"
#include "AttackAction.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ICCData.h"
#include "ICCHelpers_Rotface.h"
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

class IccRotfaceTankPositionAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc rotface tank position";

    IccRotfaceTankPositionAction(PlayerbotAI* botAI, std::string const name = Name)
        : AttackAction(botAI, name) {}
    bool Execute(Event event) override;

    bool MarkBossWithSkull(Unit* boss);
    bool PositionMainTankAndMelee(Unit* boss, Unit* smallOoze = nullptr);
    bool HandleAssistTankPositioning(Unit* boss);
    Unit* FindAssignedBigOoze(Unit* boss, std::vector<Unit*>& bigOozes);
    bool HandleBigOozeKiting(Unit* bigOoze);
};

class IccRotfaceGroupPositionAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc rotface group position";

    IccRotfaceGroupPositionAction(PlayerbotAI* botAI, std::string const name = Name)
        : AttackAction(botAI, name) {}
    bool Execute(Event event) override;

    //bool MoveAwayFromBigOoze(Unit* bigOoze);
    bool HandlePuddleAvoidance(Unit* boss);
    bool MoveAwayFromPuddle(Unit* boss, Unit* puddle, float puddleDistance);
    bool HandleOozeTargeting();
    bool HandleOozeMemberPositioning(Unit* mySmallOoze);
    bool PositionRangedAndHealers(Unit* boss,Unit* smallOoze);
    bool PositionHeroicGrid(Unit* boss);
};

class IccRotfaceMoveAwayFromExplosionAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc rotface move away from explosion";

    IccRotfaceMoveAwayFromExplosionAction(PlayerbotAI* botAI, std::string const name = Name)
        : MovementAction(botAI, name),
          _escapePosition(0.0f, 0.0f, 0.0f),
          _hasEscape(false),
          _holdUntil(0) {}
    bool Execute(Event event) override;

private:
    Position _escapePosition;
    bool _hasEscape;
    uint32 _holdUntil;
};

class IccRotfaceAvoidVileGasAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc rotface avoid vile gas";

    IccRotfaceAvoidVileGasAction(PlayerbotAI* botAI, std::string const name = Name)
        : MovementAction(botAI, name),
          _safeSpot(0.0f, 0.0f, 0.0f),
          _hasSafeSpot(false),
          _lastRunMs(0) {}
    bool Execute(Event event) override;

private:
    Position _safeSpot;
    bool _hasSafeSpot;
    uint32 _lastRunMs;
};

#endif
