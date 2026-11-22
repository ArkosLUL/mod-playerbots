#ifndef PLAYERBOTS_ULDTRIGGERS_MIMIRON_H
#define PLAYERBOTS_ULDTRIGGERS_MIMIRON_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "Trigger.h"

class MimironResetEncounterStateTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron reset encounter state trigger";

    MimironResetEncounterStateTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name, 5000) {}
    bool IsActive() override;
};

class MimironShockBlastTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron shock blast trigger";

    MimironShockBlastTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class MimironPhase1PositioningTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron phase 1 positioning trigger";

    MimironPhase1PositioningTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class MimironP3Wx2LaserBarrageTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron p3wx2 laser barrage trigger";

    MimironP3Wx2LaserBarrageTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class MimironArcSpreadTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron arc spread trigger";

    MimironArcSpreadTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class MimironAerialCommandUnitTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron aerial command unit trigger";

    MimironAerialCommandUnitTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class MimironRocketStrikeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron rocket strike trigger";

    MimironRocketStrikeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class MimironPhase4FocusTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron phase 4 focus trigger";

    MimironPhase4FocusTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class MimironMagneticCoreTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron magnetic core trigger";

    MimironMagneticCoreTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class MimironPlasmaBlastDefensiveTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron plasma blast defensive trigger";

    MimironPlasmaBlastDefensiveTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class MimironRedirectThreatTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron redirect threat trigger";

    MimironRedirectThreatTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class MimironSetDpsPriorityTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron set dps priority trigger";

    MimironSetDpsPriorityTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class MimironProximityMineTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron proximity mine trigger";

    MimironProximityMineTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class MimironBombBotTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron bomb bot trigger";

    MimironBombBotTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class MimironPetControlTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron pet control trigger";

    MimironPetControlTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class MimironFrostResistanceTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron frost resistance trigger";

    MimironFrostResistanceTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class MimironDodgeFlamesTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron dodge flames trigger";

    MimironDodgeFlamesTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class MimironApproachTargetTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron approach target trigger";

    MimironApproachTargetTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class MimironFrostBombTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron frost bomb trigger";

    MimironFrostBombTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class MimironFireBotTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron fire bot trigger";

    MimironFireBotTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class MimironSlowBombBotTrigger : public Trigger
{
public:
    static constexpr char const* Name = "mimiron slow bomb bot trigger";

    MimironSlowBombBotTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
