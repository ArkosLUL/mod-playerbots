#ifndef PLAYERBOTS_ULDTRIGGERS_THORIM_H
#define PLAYERBOTS_ULDTRIGGERS_THORIM_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "Trigger.h"

//
// Thorim
//
class ThorimUnbalancingStrikeSwapTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim unbalancing strike swap trigger";

    ThorimUnbalancingStrikeSwapTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// Only ever true for one bot at a time. Two tanks both answering it is the ping-pong that walks the
// boss across the arena, so a tank already holding him hands the trade to the swap node instead.
class ThorimTankPickupTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim tank pickup trigger";

    ThorimTankPickupTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class ThorimDpsPriorityTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim dps priority trigger";

    ThorimDpsPriorityTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class ThorimGauntletPositioningTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim gauntlet positioning trigger";

    ThorimGauntletPositioningTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// A corridor squad member up in the hallway with the Rune Giant dead. Nothing else covers this leg:
// the lane waypoints are all down at z 412, so up here the squad falls through to plain follow, and
// follow walks it straight over two Paralytic Field bunnies sitting on the centre line.
class ThorimBalconyAdvanceTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim balcony advance trigger";

    ThorimBalconyAdvanceTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class ThorimArenaPositioningTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim arena positioning trigger";

    ThorimArenaPositioningTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class ThorimFallFromFloorTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim fall from floor trigger";

    ThorimFallFromFloorTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class ThorimPhase2PositioningTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim phase 2 positioning trigger";

    ThorimPhase2PositioningTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// A Runic Smash telegraph is up and this bot is not in the lane it spares.
class ThorimRunicSmashTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim runic smash trigger";

    ThorimRunicSmashTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class ThorimRunicBarrierBailTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim runic barrier bail trigger";

    ThorimRunicBarrierBailTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// A bot standing under the Thunder Orb Thorim has charged. The orb pulses for 15s at ~3k a second and
// the field is far wider than it looks, so this is the largest avoidable damage source in phase 1.
class ThorimChargedOrbTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim charged orb trigger";

    ThorimChargedOrbTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// An arena squad member whose pet has left the room. Pets are under no leash of their own and nothing
// recalls them, so one that wanders down the corridor opens packs the gauntlet squad has not reached.
class ThorimPetLeashTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim pet leash trigger";

    ThorimPetLeashTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// An arena squad member that has left the box the boss script scans, or is on its way out of it.
class ThorimArenaLeashTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim arena leash trigger";

    ThorimArenaLeashTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class ThorimResetEncounterStateTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim reset encounter state trigger";

    ThorimResetEncounterStateTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

// Hard mode: bot standing inside Sif's moving Blizzard ground AoE.
class ThorimSifBlizzardTrigger : public Trigger
{
public:
    static constexpr char const* Name = "thorim sif blizzard trigger";

    ThorimSifBlizzardTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

#endif
