/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_EOEACTIONS_H
#define PLAYERBOTS_EOEACTIONS_H

#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

const std::pair<float, float> MALYGOS_MAINTANK_POSITION = {757.0f, 1337.0f};
const std::pair<float, float> MALYGOS_STACK_POSITION = {755.0f, 1301.0f};
// Platform centre (CenterPos in the core script). Used as the P2 anti-fall anchor.
const std::pair<float, float> MALYGOS_CENTER_POSITION = {754.395f, 1301.27f};

class MalygosPositionAction : public MovementAction
{
public:
    MalygosPositionAction(PlayerbotAI* botAI, std::string const name = "malygos position") : MovementAction(botAI, name)
    {
    }

    bool Execute(Event event) override;
};

class MalygosTargetAction : public AttackAction
{
public:
    MalygosTargetAction(PlayerbotAI* botAI, std::string const name = "malygos target") : AttackAction(botAI, name) {}

    bool Execute(Event event) override;
};

class PullPowerSparkAction : public Action
{
public:
    PullPowerSparkAction(PlayerbotAI* botAI, std::string const name = "pull power spark") : Action(botAI, name) {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    Unit* GetSpark();
};

class KillPowerSparkAction : public AttackAction
{
public:
    KillPowerSparkAction(PlayerbotAI* botAI, std::string const name = "kill power spark") : AttackAction(botAI, name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// P2 ground avoidance: step off Arcane Overload void zones (the core's "Deep Breath").
class DeepBreathDodgeAction : public MovementAction
{
public:
    DeepBreathDodgeAction(PlayerbotAI* botAI, std::string const name = "deep breath dodge") : MovementAction(botAI, name)
    {
    }

    bool Execute(Event event) override;
};

// P2 ground avoidance: clear the Surge of Power beam.
class AvoidSurgeOfPowerAction : public MovementAction
{
public:
    AvoidSurgeOfPowerAction(PlayerbotAI* botAI, std::string const name = "avoid surge of power") : MovementAction(botAI, name)
    {
    }

    bool Execute(Event event) override;
};

class EoEFlyDrakeAction : public MovementAction
{
public:
    EoEFlyDrakeAction(PlayerbotAI* ai) : MovementAction(ai, "eoe fly drake") {}

    bool Execute(Event event) override;
    bool isPossible() override;
};

class EoEDrakeAttackAction : public Action
{
public:
    EoEDrakeAttackAction(PlayerbotAI* botAI) : Action(botAI, "eoe drake attack") {}

    bool Execute(Event event) override;
    bool isPossible() override;

protected:
    Unit* vehicleBase;
    bool CastDrakeSpellAction(Unit* target, uint32 spellId, uint32 cooldown);
    bool DrakeDpsAction(Unit* target);
    bool DrakeHealAction();
    bool IsHealDrake();
};

// P3 drake avoidance: fly clear of a Static Field hazard. Not a MovementAction so the
// phase-3 movement suppression doesn't block it; it drives the vehicle directly.
class AvoidStaticFieldAction : public Action
{
public:
    AvoidStaticFieldAction(PlayerbotAI* botAI) : Action(botAI, "avoid static field") {}

    bool Execute(Event event) override;
    bool isPossible() override;
};

// P3 drake avoidance: react to the Surge of Power fixate with Flame Shield + a hard peel.
class DrakeDodgeSurgeAction : public Action
{
public:
    DrakeDodgeSurgeAction(PlayerbotAI* botAI) : Action(botAI, "drake dodge surge") {}

    bool Execute(Event event) override;
    bool isPossible() override;
};

#endif
