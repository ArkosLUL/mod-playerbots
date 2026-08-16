#ifndef PLAYERBOTS_ULDTRIGGERS_MIMIRON_H
#define PLAYERBOTS_ULDTRIGGERS_MIMIRON_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "UldBossHelper.h"
#include "Trigger.h"

//
// Mimiron
//
class MimironShockBlastTrigger : public Trigger
{
public:
    MimironShockBlastTrigger(PlayerbotAI* ai) : Trigger(ai, "mimiron shock blast trigger") {}
    bool IsActive() override;
};

class MimironPhase1PositioningTrigger : public Trigger
{
public:
    MimironPhase1PositioningTrigger(PlayerbotAI* ai) : Trigger(ai, "mimiron phase 1 positioning trigger") {}
    bool IsActive() override;
};

class MimironP3Wx2LaserBarrageTrigger : public Trigger
{
public:
    MimironP3Wx2LaserBarrageTrigger(PlayerbotAI* ai) : Trigger(ai, "mimiron p3wx2 laser barrage trigger") {}
    bool IsActive() override;
};

class MimironArcSpreadTrigger : public Trigger
{
public:
    MimironArcSpreadTrigger(PlayerbotAI* ai) : Trigger(ai, "mimiron arc spread trigger") {}
    bool IsActive() override;
};

class MimironAerialCommandUnitTrigger : public Trigger
{
public:
    MimironAerialCommandUnitTrigger(PlayerbotAI* ai) : Trigger(ai, "mimiron aerial command unit trigger") {}
    bool IsActive() override;
};

class MimironRocketStrikeTrigger : public Trigger
{
public:
    MimironRocketStrikeTrigger(PlayerbotAI* ai) : Trigger(ai, "mimiron rocket strike trigger") {}
    bool IsActive() override;
};

class MimironPhase4MarkDpsTrigger : public Trigger
{
public:
    MimironPhase4MarkDpsTrigger(PlayerbotAI* ai) : Trigger(ai, "mimiron phase 4 mark dps trigger") {}
    bool IsActive() override;
};

class MimironMagneticCoreTrigger : public Trigger
{
public:
    MimironMagneticCoreTrigger(PlayerbotAI* ai) : Trigger(ai, "mimiron magnetic core trigger") {}
    bool IsActive() override;
};

class MimironPlasmaBlastTrigger : public Trigger
{
public:
    MimironPlasmaBlastTrigger(PlayerbotAI* ai) : Trigger(ai, "mimiron plasma blast trigger") {}
    bool IsActive() override;
};

class MimironSetDpsPriorityTrigger : public Trigger
{
public:
    MimironSetDpsPriorityTrigger(PlayerbotAI* ai) : Trigger(ai, "mimiron set dps priority trigger") {}
    bool IsActive() override;
};

class MimironProximityMineTrigger : public Trigger
{
public:
    MimironProximityMineTrigger(PlayerbotAI* ai) : Trigger(ai, "mimiron proximity mine trigger") {}
    bool IsActive() override;
};

class MimironBombBotTrigger : public Trigger
{
public:
    MimironBombBotTrigger(PlayerbotAI* ai) : Trigger(ai, "mimiron bomb bot trigger") {}
    bool IsActive() override;
};

// Hard mode (Firefighter): bot is standing in a persistent ground-fire node and must step out.
class MimironDodgeFlamesTrigger : public Trigger
{
public:
    MimironDodgeFlamesTrigger(PlayerbotAI* ai) : Trigger(ai, "mimiron dodge flames trigger") {}
    bool IsActive() override;
};

// Hard mode (Firefighter): bot is inside VX-001's Frost Bomb radius and must clear it before it blows.
class MimironFrostBombTrigger : public Trigger
{
public:
    MimironFrostBombTrigger(PlayerbotAI* ai) : Trigger(ai, "mimiron frost bomb trigger") {}
    bool IsActive() override;
};

#endif
