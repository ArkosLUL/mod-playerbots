/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_EOEACTIONS_SPARKS_H
#define PLAYERBOTS_EOEACTIONS_SPARKS_H

#include "Action.h"
#include "AttackAction.h"
#include "PlayerbotAI.h"

#include <string>

// P1: the DK half of the Power Spark answer - Death Grip, then Chains of Ice on what it pulled.
class PullPowerSparkAction : public Action
{
public:
    PullPowerSparkAction(PlayerbotAI* botAI, std::string const name = "malygos pull power spark")
        : Action(botAI, name)
    {
    }

    bool Execute(Event event) override;
    bool isUseful() override;
};

class KillPowerSparkAction : public AttackAction
{
public:
    KillPowerSparkAction(PlayerbotAI* botAI, std::string const name = "malygos kill power spark")
        : AttackAction(botAI, name)
    {
    }

    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
