#ifndef PLAYERBOTS_ULDTRIGGERS_MIMIRON_H
#define PLAYERBOTS_ULDTRIGGERS_MIMIRON_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "Trigger.h"

//
// Mimiron
//

// "disperse distance" is bot state, not encounter state, so without this Mimiron's 6.0 outlives the
// pull and the generic spread mover keeps walking bots around for the rest of the raid night. It
// also clears a "disperse enable" typed inside Ulduar, which is the price of not needing to know who
// set it.
class MimironResetEncounterStateTrigger : public Trigger
{
public:
    MimironResetEncounterStateTrigger(PlayerbotAI* ai) : Trigger(ai, "mimiron reset encounter state trigger", 5) {}
    bool IsActive() override;
};

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

// Live whenever this bot is inside VX-001's Rapid Burst cone and can walk out of it in time. Not
// gated on hard mode: Rapid Burst is scheduled unconditionally when phase 2 starts, so a normal
// clear eats the same cone.
class MimironRapidBurstTrigger : public Trigger
{
public:
    MimironRapidBurstTrigger(PlayerbotAI* ai) : Trigger(ai, "mimiron rapid burst trigger") {}
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

class MimironPhase4FocusTrigger : public Trigger
{
public:
    MimironPhase4FocusTrigger(PlayerbotAI* ai) : Trigger(ai, "mimiron phase 4 focus trigger") {}
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

// This bot has a pet and the encounter is in a phase where it needs telling where to go.
class MimironPetControlTrigger : public Trigger
{
public:
    MimironPetControlTrigger(PlayerbotAI* ai) : Trigger(ai, "mimiron pet control trigger") {}
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

// This bot carries a ranged snare and is already shooting a Bomb Bot that still has ground to cover.
class MimironSlowBombBotTrigger : public Trigger
{
public:
    MimironSlowBombBotTrigger(PlayerbotAI* ai) : Trigger(ai, "mimiron slow bomb bot trigger") {}
    bool IsActive() override;
};

#endif
