/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_SAURFANG_H
#define PLAYERBOTS_ICCACTIONS_SAURFANG_H

#include "Action.h"
#include "AttackAction.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ICCData.h"
#include "ICCHelpers_Saurfang.h"
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

class IccDbsTankPositionAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc dbs tank position";

    IccDbsTankPositionAction(PlayerbotAI* botAI, std::string const name = Name)
        : AttackAction(botAI, name) {}
    bool Execute(Event event) override;

    bool CrowdControlBloodBeasts();
    bool EvadeBloodBeasts();
    bool PositionInRangedFormation();
};

class IccAddsDbsAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc adds dbs";

    IccAddsDbsAction(PlayerbotAI* botAI, std::string const name = Name)
        : AttackAction(botAI, name) {}
    bool Execute(Event event) override;

    Unit* FindPriorityTarget(Unit* boss);
    bool UpdateSkullMarker(Unit* priorityTarget);
};

#endif
