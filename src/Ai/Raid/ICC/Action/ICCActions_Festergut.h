/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONS_FESTERGUT_H
#define PLAYERBOTS_ICCACTIONS_FESTERGUT_H

#include "Action.h"
#include "AttackAction.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ICCData.h"
#include "ICCHelpers_Festergut.h"
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

class IccFestergutGroupPositionAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc festergut group position";

    IccFestergutGroupPositionAction(PlayerbotAI* botAI, std::string const name = Name)
        : AttackAction(botAI, name) {}
    bool Execute(Event event) override;

    bool HasSporesInGroup();
    bool PositionNonTankMembers();
    int CalculatePositionIndex(Group* group);
};

class IccFestergutSporeAction : public AttackAction
{
public:
    static constexpr char const* Name = "icc festergut spore";

    IccFestergutSporeAction(PlayerbotAI* botAI, std::string const name = Name)
        : AttackAction(botAI, name) {}
    bool Execute(Event event) override;

    Position CalculateSpreadPosition();
    struct SporeInfo
    {
        std::vector<Unit*> sporedPlayers;
        ObjectGuid lowestGuid;
        bool hasLowestGuid = false;
    };
    SporeInfo FindSporedPlayers();
    Position DetermineTargetPosition(bool hasSpore, SporeInfo const& sporeInfo, Position const& spreadRangedPos);
    bool CheckMainTankSpore();
    bool GooNear(Position const& pos);
};

class IccFestergutAvoidMalleableGooAction : public MovementAction
{
public:
    static constexpr char const* Name = "icc festergut avoid malleable goo";

    IccFestergutAvoidMalleableGooAction(PlayerbotAI* botAI,
                                        std::string const name = Name)
        : MovementAction(botAI, name) {}
    bool Execute(Event event) override;
};

#endif
