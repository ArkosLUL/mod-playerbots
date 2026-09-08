#ifndef PLAYERBOTS_ULDTRIGGERS_THORIM_H
#define PLAYERBOTS_ULDTRIGGERS_THORIM_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "Trigger.h"

//
// Thorim
//
class ThorimUnbalancingStrikeTrigger : public Trigger
{
public:
    ThorimUnbalancingStrikeTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim unbalancing strike trigger") {}
    bool IsActive() override;
};

class ThorimUnbalancingStrikeSwapTrigger : public Trigger
{
public:
    ThorimUnbalancingStrikeSwapTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim unbalancing strike swap trigger") {}
    bool IsActive() override;
};

// Only ever true for one bot at a time. Two tanks both answering it is the ping-pong that walks the
// boss across the arena, so a tank already holding him hands the trade to the swap node instead.
class ThorimTankPickupTrigger : public Trigger
{
public:
    ThorimTankPickupTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim tank pickup trigger") {}
    bool IsActive() override;
};

class ThorimDpsPriorityTrigger : public Trigger
{
public:
    ThorimDpsPriorityTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim dps priority trigger") {}
    bool IsActive() override;
};

class ThorimGauntletPositioningTrigger : public Trigger
{
public:
    ThorimGauntletPositioningTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim gauntlet positioning trigger") {}
    bool IsActive() override;
};

// A corridor squad member up in the hallway with the Rune Giant dead. Nothing else covers this leg:
// the lane waypoints are all down at z 412, so up here the squad falls through to plain follow, and
// follow walks it straight over two Paralytic Field bunnies sitting on the centre line.
class ThorimBalconyAdvanceTrigger : public Trigger
{
public:
    ThorimBalconyAdvanceTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim balcony advance trigger") {}
    bool IsActive() override;
};

class ThorimArenaPositioningTrigger : public Trigger
{
public:
    ThorimArenaPositioningTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim arena positioning trigger") {}
    bool IsActive() override;
};

class ThorimFallFromFloorTrigger : public Trigger
{
public:
    ThorimFallFromFloorTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim fall from floor trigger") {}
    bool IsActive() override;
};

class ThorimPhase2PositioningTrigger : public Trigger
{
public:
    ThorimPhase2PositioningTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim phase 2 positioning trigger") {}
    bool IsActive() override;
};

// A Runic Smash telegraph is up and this bot is not in the lane it spares.
class ThorimRunicSmashTrigger : public Trigger
{
public:
    ThorimRunicSmashTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim runic smash trigger") {}
    bool IsActive() override;
};

class ThorimRunicBarrierBailTrigger : public Trigger
{
public:
    ThorimRunicBarrierBailTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim runic barrier bail trigger") {}
    bool IsActive() override;
};

class ThorimLightningChargeTrigger : public Trigger
{
public:
    ThorimLightningChargeTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim lightning charge trigger") {}
    bool IsActive() override;
};

// A bot standing under the Thunder Orb Thorim has charged. The orb pulses for 15s at ~3k a second and
// the field is far wider than it looks, so this is the largest avoidable damage source in phase 1.
class ThorimChargedOrbTrigger : public Trigger
{
public:
    ThorimChargedOrbTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim charged orb trigger") {}
    bool IsActive() override;
};

// An arena squad member whose pet has left the room. Pets are under no leash of their own and nothing
// recalls them, so one that wanders down the corridor opens packs the gauntlet squad has not reached.
class ThorimPetLeashTrigger : public Trigger
{
public:
    ThorimPetLeashTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim pet leash trigger") {}
    bool IsActive() override;
};

// An arena squad member that has left the box the boss script scans, or is on its way out of it.
class ThorimArenaLeashTrigger : public Trigger
{
public:
    ThorimArenaLeashTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim arena leash trigger") {}
    bool IsActive() override;
};

class ThorimResetEncounterStateTrigger : public Trigger
{
public:
    ThorimResetEncounterStateTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim reset encounter state trigger") {}
    bool IsActive() override;
};

// Hard mode: bot standing inside Sif's moving Blizzard ground AoE.
class ThorimSifBlizzardTrigger : public Trigger
{
public:
    ThorimSifBlizzardTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim sif blizzard trigger") {}
    bool IsActive() override;
};

// Hard mode: ranged/healer standing within Sif's point-blank Frost Nova range.
class ThorimSifFrostNovaTrigger : public Trigger
{
public:
    ThorimSifFrostNovaTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim sif frost nova trigger") {}
    bool IsActive() override;
};

#endif
