/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_BLOODPRINCES_H
#define PLAYERBOTS_ICCACTIONS_BLOODPRINCES_H

#include "Action.h"
#include "AttackAction.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ICCData.h"
#include "ICCHelpers_BloodPrinces.h"
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

class IccBpcKelesethTankAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc bpc keleseth tank";

    IccBpcKelesethTankAction(PlayerbotAI* botAI)
        : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccBpcMainTankAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc bpc main tank";

    IccBpcMainTankAction(PlayerbotAI* botAI)
        : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;

    bool MarkEmpoweredPrince();
};

class IccBpcEmpoweredVortexAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc bpc empowered vortex";

    IccBpcEmpoweredVortexAction(PlayerbotAI* botAI)
        : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;

    bool MaintainRangedSpacing();
    bool HandleEmpoweredVortexSpread();
};

class IccBpcKineticBombAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc bpc kinetic bomb";

    IccBpcKineticBombAction(PlayerbotAI* botAI)
        : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class IccBpcBallOfFlameAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc bpc ball of flame";

    IccBpcBallOfFlameAction(PlayerbotAI* botAI)
        : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
};

#endif
