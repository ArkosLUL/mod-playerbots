/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_SVALNA_H
#define PLAYERBOTS_ICCACTIONS_SVALNA_H

#include "Action.h"
#include "AttackAction.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ICCData.h"
#include "ICCHelpers_Shared.h"
#include "ICCHelpers_Svalna.h"
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

class IccValkyreSpearAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc valkyre spear";

    IccValkyreSpearAction(PlayerbotAI* botAI)
            : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccSisterSvalnaAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc sister svalna";

    IccSisterSvalnaAction(PlayerbotAI* botAI)
            : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

#endif
