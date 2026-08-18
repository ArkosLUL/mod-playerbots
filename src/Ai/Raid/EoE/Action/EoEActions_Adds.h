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

#include <string>

// P2: strip the Nexus Lords' self-cast Haste (57060), without touching the mage's own target.
class MalygosSpellstealAction : public Action
{
public:
    MalygosSpellstealAction(PlayerbotAI* botAI, std::string const name = "malygos spellsteal") : Action(botAI, name) {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    Unit* GetHastedLord();
};

class MalygosSeekBubbleAction : public MovementAction
{
public:
    MalygosSeekBubbleAction(PlayerbotAI* botAI, std::string const name = "malygos seek bubble")
        : MovementAction(botAI, name)
    {
    }

    bool Execute(Event event) override;

private:
    // Latched, or a bot walks in circles between two shrinking bubbles.
    ObjectGuid assignedBubbleGuid;
};

class MalygosBoardDiskAction : public EnterVehicleAction
{
public:
    MalygosBoardDiskAction(PlayerbotAI* botAI) : EnterVehicleAction(botAI, "malygos board disk") {}

    bool Execute(Event event) override;
};

// P2: fly a boarded Hover Disk to the Scions, steering the vehicle rather than moving the bot.
class MalygosRideDiskAction : public AttackAction
{
public:
    MalygosRideDiskAction(PlayerbotAI* botAI) : AttackAction(botAI, "malygos ride disk") {}

    bool Execute(Event event) override;
    bool isPossible() override;

private:
    bool descending = false;
};

class AvoidSurgeOfPowerAction : public MovementAction
{
public:
    AvoidSurgeOfPowerAction(PlayerbotAI* botAI, std::string const name = "malygos avoid surge of power")
        : MovementAction(botAI, name)
    {
    }

    bool Execute(Event event) override;
};

#endif
