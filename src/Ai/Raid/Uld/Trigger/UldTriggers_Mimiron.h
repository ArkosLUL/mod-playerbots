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
    static constexpr char const* Name = "mimiron reset encounter state trigger";

    MimironResetEncounterStateTrigger(PlayerbotAI* ai) : Trigger(ai, Name, 5) {}
    bool IsActive() override;
};

class MimironShockBlastTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron shock blast trigger";

    MimironShockBlastTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class MimironPhase1PositioningTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron phase 1 positioning trigger";

    MimironPhase1PositioningTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class MimironP3Wx2LaserBarrageTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron p3wx2 laser barrage trigger";

    MimironP3Wx2LaserBarrageTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class MimironArcSpreadTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron arc spread trigger";

    MimironArcSpreadTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class MimironAerialCommandUnitTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron aerial command unit trigger";

    MimironAerialCommandUnitTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class MimironRocketStrikeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron rocket strike trigger";

    MimironRocketStrikeTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class MimironPhase4FocusTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron phase 4 focus trigger";

    MimironPhase4FocusTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class MimironMagneticCoreTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron magnetic core trigger";

    MimironMagneticCoreTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// Live for the ~5 s the cannon is actually casting Plasma Blast on the tank. Six ticks totalling
// 54k to 119k land on a 43k pool, so the button has to be down before the first one, not after a
// health threshold notices the third.
class MimironPlasmaBlastDefensiveTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron plasma blast defensive trigger";

    MimironPlasmaBlastDefensiveTrigger(PlayerbotAI* ai)
        : Trigger(ai, Name) {}
    bool IsActive() override;
};

// Phase 1 only: feed the main tank so the MK II stays on him through the 53 yd walk west. Later
// phases split two mechs across two tanks, which is why UldThreatRedirectMultiplier holds the
// class-generic nodes for the whole encounter.
class MimironRedirectThreatTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron redirect threat trigger";

    MimironRedirectThreatTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class MimironSetDpsPriorityTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron set dps priority trigger";

    MimironSetDpsPriorityTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class MimironProximityMineTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron proximity mine trigger";

    MimironProximityMineTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class MimironBombBotTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron bomb bot trigger";

    MimironBombBotTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// This bot has a pet and the encounter is in a phase where it needs telling where to go.
class MimironPetControlTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron pet control trigger";

    MimironPetControlTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// This bot is the phase 3 Frost Resistance Aura paladin and is not running it yet.
class MimironFrostResistanceTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron frost resistance trigger";

    MimironFrostResistanceTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// Hard mode (Firefighter): bot is standing in a persistent ground-fire node and must step out.
class MimironDodgeFlamesTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron dodge flames trigger";

    MimironDodgeFlamesTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// Hard mode (Firefighter): the bot has to close on its target and the spot the generic reach nodes
// would stop on is burning, but another bearing round the target is not.
class MimironApproachTargetTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron approach target trigger";

    MimironApproachTargetTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// Hard mode (Firefighter): bot is inside VX-001's Frost Bomb radius and must clear it before it blows.
class MimironFrostBombTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron frost bomb trigger";

    MimironFrostBombTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// Hard mode (Firefighter): bot is standing in a fire bot's spray line or, as a caster or healer in
// 25-man, inside its silence aura.
class MimironFireBotTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron fire bot trigger";

    MimironFireBotTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// This bot carries a ranged snare and is already shooting a Bomb Bot that still has ground to cover.
class MimironSlowBombBotTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron slow bomb bot trigger";

    MimironSlowBombBotTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

#endif
