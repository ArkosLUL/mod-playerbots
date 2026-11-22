/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_EOEACTIONS_SHARED_H
#define PLAYERBOTS_EOEACTIONS_SHARED_H

#include "AttackAction.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"

class MalygosPositionAction : public MovementAction
{
public:
    static constexpr char const* Name = "malygos position";

    MalygosPositionAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MalygosTargetAction : public AttackAction
{
public:
    static constexpr char const* Name = "malygos target";

    MalygosTargetAction(PlayerbotAI* botAI) : AttackAction(botAI, Name) {}

    bool Execute(Event event) override;
};

#endif
