#ifndef PLAYERBOTS_ULDACTIONS_MIMIRON_H
#define PLAYERBOTS_ULDACTIONS_MIMIRON_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericActions.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldBossHelper.h"
#include "UldTriggers.h"
#include "Vehicle.h"

class MimironShockBlastAction : public MovementAction
{
public:
    MimironShockBlastAction(PlayerbotAI* ai) : MovementAction(ai, "mimiron shock blast action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MimironPhase1PositioningAction : public MovementAction
{
public:
    MimironPhase1PositioningAction(PlayerbotAI* ai) : MovementAction(ai, "mimiron phase 1 positioning action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MimironP3Wx2LaserBarrageAction : public MovementAction
{
public:
    MimironP3Wx2LaserBarrageAction(PlayerbotAI* ai, float distance = 24.0f, float delta_angle = M_PI / 8)
        : MovementAction(ai, "mimiron p3wx2 laser barrage action")
    {
        this->distance = distance;
        this->delta_angle = delta_angle;
    }
    virtual bool Execute(Event event);

protected:
    float distance, delta_angle;
};

class MimironRapidBurstAction : public MovementAction
{
public:
    MimironRapidBurstAction(PlayerbotAI* ai) : MovementAction(ai, "mimiron rapid burst action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MimironAerialCommandUnitAction : public Action
{
public:
    MimironAerialCommandUnitAction(PlayerbotAI* ai) : Action(ai, "mimiron aerial command unit action") {}

    bool Execute(Event event) override;
};

class MimironRocketStrikeAction : public MovementAction
{
public:
    MimironRocketStrikeAction(PlayerbotAI* ai) : MovementAction(ai, "mimiron rocket strike action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class MimironPhase4MarkDpsAction : public Action
{
public:
    MimironPhase4MarkDpsAction(PlayerbotAI* ai) : Action(ai, "mimiron phase 4 mark dps action") {}

    bool Execute(Event event) override;
};

class MimironCheatAction : public Action
{
public:
    MimironCheatAction(PlayerbotAI* ai) : Action(ai, "mimiron cheat action") {}

    bool Execute(Event event) override;
};

class MimironProximityMineAction : public MoveAwayFromCreatureAction
{
public:
    MimironProximityMineAction(PlayerbotAI* ai)
        : MoveAwayFromCreatureAction(ai, "mimiron proximity mine action", NPC_PROXIMITY_MINE, 6.0f) {}
};

class MimironBombBotAction : public MoveAwayFromCreatureAction
{
public:
    MimironBombBotAction(PlayerbotAI* ai)
        : MoveAwayFromCreatureAction(ai, "mimiron bomb bot action", NPC_BOMB_BOT, 6.0f) {}
};

// Hard mode (Firefighter): step out of the persistent ground fire before it burns the bot down.
class MimironDodgeFlamesAction : public MovementAction
{
public:
    MimironDodgeFlamesAction(PlayerbotAI* ai) : MovementAction(ai, "mimiron dodge flames action") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// Hard mode (Firefighter): clear VX-001's Frost Bomb radius before it detonates.
class MimironFrostBombAction : public MoveAwayFromCreatureAction
{
public:
    MimironFrostBombAction(PlayerbotAI* ai)
        : MoveAwayFromCreatureAction(ai, "mimiron frost bomb action", NPC_FROST_BOMB,
                                     ULDUAR_MIMIRON_FROST_BOMB_RADIUS) {}

    bool isUseful() override;
};

#endif
