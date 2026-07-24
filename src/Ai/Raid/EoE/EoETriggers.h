#ifndef PLAYERBOTS_EOETRIGGERS_H
#define PLAYERBOTS_EOETRIGGERS_H

#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Trigger.h"

enum EyeOfEternityIDs
{
    NPC_MALYGOS                         = 28859,
    NPC_POWER_SPARK                     = 30084,
    NPC_NEXUS_LORD                      = 30245,
    NPC_SCION_OF_ETERNITY               = 30249,
    NPC_WYRMREST_SKYTALON               = 30161,
    NPC_ARCANE_OVERLOAD                 = 30282,
    NPC_SURGE_OF_POWER                  = 30334,
    NPC_STATIC_FIELD                    = 30592,

    SPELL_POWER_SPARK_VISUAL            = 55845,
    SPELL_POWER_SPARK_GROUND_BUFF       = 55852,
    SPELL_POWER_SPARK_MALYGOS_BUFF      = 56152,

    SPELL_TELEPORT_VISUAL               = 52096,

    SPELL_SCION_ARCANE_BARRAGE          = 56397,
    SPELL_ARCANE_SHOCK_N                = 57058,
    SPELL_ARCANE_SHOCK_H                = 60073,
    SPELL_HASTE                         = 57060,

    SPELL_ALEXSTRASZA_GIFT              = 61028,

    // Boss hazards (verified against core boss_malygos.cpp)
    SPELL_ARCANE_BREATH                 = 56272,    // P1 frontal cone on tank
    SPELL_ARCANE_STORM                  = 61693,    // P1/P2/P3 random-target AoE, unavoidable
    SPELL_ARCANE_OVERLOAD               = 56430,    // P2 ground void zone (summons NPC_ARCANE_OVERLOAD)
    SPELL_SURGE_OF_POWER_P2             = 56505,    // P2 beam
    SPELL_SURGE_OF_POWER_P3             = 57407,    // P3 fixate beam, 10-man
    SPELL_SURGE_OF_POWER_P3_25          = 60936,    // P3 fixate beam, 25-man
    SPELL_ARCANE_PULSE                  = 57432,    // P3 raid pulse, unavoidable
    SPELL_STATIC_FIELD                  = 57430,    // P3 (summons NPC_STATIC_FIELD hazard)

    // Drake Abilities:
    // DPS
    SPELL_FLAME_SPIKE                   = 56091,
    SPELL_ENGULF_IN_FLAMES              = 56092,
    // Healing
    SPELL_REVIVIFY                      = 57090,
    SPELL_LIFE_BURST                    = 57143,
    // Utility
    SPELL_FLAME_SHIELD                  = 57108,
    SPELL_BLAZING_SPEED                 = 57092,
};

const uint32 EOE_MAP_ID = 616;

class MalygosTrigger : public Trigger
{
public:
    MalygosTrigger(PlayerbotAI* botAI) : Trigger(botAI, "malygos") {}
    bool IsActive() override;
    // Finds Malygos even while he is flagged non-attackable (P2 flight / phase transitions).
    static Unit* getMalygos(Player* bot);
    // 0 = inactive/pre-pull, 1 = P1, 2 = P2 (adds up), 3 = drake phase, 4 = phase transition hold.
    static uint8 getPhase(Player* bot);
};

class PowerSparkTrigger : public Trigger
{
public:
    PowerSparkTrigger(PlayerbotAI* botAI) : Trigger(botAI, "power spark") {}
    bool IsActive() override;
};

// P2: Malygos casting Arcane Overload. In this core there is no separate "Deep Breath" spell;
// the retail Deep Breath telegraph is realized here as the Arcane Overload void zone (56430).
class DeepBreathTrigger : public Trigger
{
public:
    DeepBreathTrigger(PlayerbotAI* botAI) : Trigger(botAI, "deep breath") {}
    bool IsActive() override;
};

// P2: Surge of Power beam active (surge NPC summoned or boss casting the P2 surge).
class SurgeOfPowerTrigger : public Trigger
{
public:
    SurgeOfPowerTrigger(PlayerbotAI* botAI) : Trigger(botAI, "surge of power") {}
    bool IsActive() override;
};

// P3: a Static Field hazard is near this bot's drake.
class StaticFieldTrigger : public Trigger
{
public:
    StaticFieldTrigger(PlayerbotAI* botAI) : Trigger(botAI, "static field") {}
    bool IsActive() override;
};

// P3: Malygos casting the fixate Surge of Power (10/25-man).
class DrakeSurgeTrigger : public Trigger
{
public:
    DrakeSurgeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "drake surge") {}
    bool IsActive() override;
};

#endif
