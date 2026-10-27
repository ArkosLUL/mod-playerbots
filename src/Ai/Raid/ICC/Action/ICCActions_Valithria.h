/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_VALITHRIA_H
#define PLAYERBOTS_ICCACTIONS_VALITHRIA_H

#include "Action.h"
#include "AttackAction.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ICCData.h"
#include "ICCHelpers_Shared.h"
#include "ICCHelpers_Valithria.h"
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

class IccValithriaGroupAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc valithria group";

    IccValithriaGroupAction(PlayerbotAI* botAI)
        : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;

    bool Handle25ManGroupLogic();
    bool HandleMarkingLogic(bool inGroup1, bool inGroup2, bool singleMarkMode);
    bool Handle10ManGroupLogic();

private:
    bool ApplyCrowdControl(Unit* zombie);
};

class IccValithriaZombieKiteAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc valithria zombie kite";

    IccValithriaZombieKiteAction(PlayerbotAI* botAI)
        : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccValithriaPortalAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc valithria portal";

    IccValithriaPortalAction(PlayerbotAI* botAI)
        : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccValithriaHealAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc valithria heal";

    IccValithriaHealAction(PlayerbotAI* botAI)
        : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccValithriaDreamCloudAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc valithria dream cloud";

    IccValithriaDreamCloudAction(PlayerbotAI* botAI)
        : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;

private:
    std::vector<Creature*> CollectClouds(uint32 entry, Unit* reference);
};

#endif
