/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_EOEACTIONS_DRAKES_H
#define PLAYERBOTS_EOEACTIONS_DRAKES_H

#include "Action.h"
#include "MovementActions.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"

#include <vector>

// P3: sole owner of the drake's position. Nothing else may steer a Skytalon.
class EoEFlyDrakeAction : public MovementAction
{
public:
    EoEFlyDrakeAction(PlayerbotAI* ai) : MovementAction(ai, "eoe fly drake") {}

    bool Execute(Event event) override;
    bool isPossible() override;

private:
    uint32 stackCalcAtMs = 0;
    float stackX = 0.0f;
    float stackY = 0.0f;
    float stackZ = 0.0f;
    bool stackValid = false;

    // Last destination handed to the MotionMaster, so the same one is not restamped every tick.
    float issuedX = 0.0f;
    float issuedY = 0.0f;
    bool issued = false;
};

class EoEDrakeAttackAction : public Action
{
public:
    EoEDrakeAttackAction(PlayerbotAI* botAI) : Action(botAI, "eoe drake attack") {}

    bool Execute(Event event) override;
    bool isPossible() override;

protected:
    bool CastDrakeSpellAction(Unit* target, uint32 spellId);
    bool DrakeDpsAction(Unit* drake, Unit* target);
    bool DrakeHealAction(Unit* drake, std::vector<ObjectGuid> const& healers);
};

class DrakeSurgeShieldAction : public Action
{
public:
    DrakeSurgeShieldAction(PlayerbotAI* botAI) : Action(botAI, "eoe drake surge shield") {}

    bool Execute(Event event) override;
    bool isPossible() override;
};

#endif
