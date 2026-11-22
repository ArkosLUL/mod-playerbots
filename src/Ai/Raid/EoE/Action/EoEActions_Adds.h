/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_EOEACTIONS_ADDS_H
#define PLAYERBOTS_EOEACTIONS_ADDS_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "VehicleActions.h"

class MalygosSpellstealAction : public Action
{
public:
    static constexpr char const* Name = "malygos spellsteal";

    MalygosSpellstealAction(PlayerbotAI* botAI) : Action(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    Unit* GetHastedLord();

    ObjectGuid _lordGuid;
};

class MalygosSeekBubbleAction : public MovementAction
{
public:
    static constexpr char const* Name = "malygos seek bubble";

    MalygosSeekBubbleAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    ObjectGuid _assignedBubbleGuid;
};

class MalygosBoardDiskAction : public EnterVehicleAction
{
public:
    static constexpr char const* Name = "malygos board disk";

    MalygosBoardDiskAction(PlayerbotAI* botAI) : EnterVehicleAction(botAI, Name) {}

    bool Execute(Event event) override;
};

class MalygosRideDiskAction : public AttackAction
{
public:
    static constexpr char const* Name = "malygos ride disk";

    MalygosRideDiskAction(PlayerbotAI* botAI) : AttackAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isPossible() override;

private:
    bool _descending = false;
};

class AvoidSurgeOfPowerAction : public MovementAction
{
public:
    static constexpr char const* Name = "malygos avoid surge of power";

    AvoidSurgeOfPowerAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    ObjectGuid _surgeGuid;
};

#endif
