#ifndef PLAYERBOTS_ULDTRIGGERS_THORIM_H
#define PLAYERBOTS_ULDTRIGGERS_THORIM_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "Trigger.h"

class ThorimUnbalancingStrikeSwapTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim unbalancing strike swap trigger";

    ThorimUnbalancingStrikeSwapTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class ThorimTankPickupTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim tank pickup trigger";

    ThorimTankPickupTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class ThorimDpsPriorityTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim dps priority trigger";

    ThorimDpsPriorityTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class ThorimGauntletPositioningTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim gauntlet positioning trigger";

    ThorimGauntletPositioningTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class ThorimGauntletLeverTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim gauntlet lever trigger";

    ThorimGauntletLeverTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class ThorimGauntletLeadTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim gauntlet lead trigger";

    ThorimGauntletLeadTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class ThorimBalconyAdvanceTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim balcony advance trigger";

    ThorimBalconyAdvanceTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class ThorimArenaPositioningTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim arena positioning trigger";

    ThorimArenaPositioningTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class ThorimFallFromFloorTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim fall from floor trigger";

    ThorimFallFromFloorTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class ThorimPhase2PositioningTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim phase 2 positioning trigger";

    ThorimPhase2PositioningTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class ThorimUnreachableTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim unreachable trigger";

    ThorimUnreachableTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class ThorimRunicSmashTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim runic smash trigger";

    ThorimRunicSmashTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class ThorimRunicBarrierBailTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim runic barrier bail trigger";

    ThorimRunicBarrierBailTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class ThorimChargedOrbTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim charged orb trigger";

    ThorimChargedOrbTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class ThorimFrostAuraTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim frost aura trigger";

    ThorimFrostAuraTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class ThorimPetLeashTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim pet leash trigger";

    ThorimPetLeashTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class ThorimArenaLeashTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim arena leash trigger";

    ThorimArenaLeashTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class ThorimResetEncounterStateTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim reset encounter state trigger";

    ThorimResetEncounterStateTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class ThorimPersonalDefensiveTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim personal defensive trigger";

    ThorimPersonalDefensiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class ThorimSifBlizzardTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim sif blizzard trigger";

    ThorimSifBlizzardTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
