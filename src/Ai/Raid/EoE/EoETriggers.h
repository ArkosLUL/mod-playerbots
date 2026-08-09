/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

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
    NPC_HOVER_DISK                      = 30248,

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
    SPELL_ARCANE_OVERLOAD               = 56430,    // P2 shelter bubble (summons NPC_ARCANE_OVERLOAD)
    SPELL_ARCANE_OVERLOAD_AURA          = 56432,    // ticks on the bubble, re-granting the protection
    SPELL_ARCANE_OVERLOAD_PROTECTION    = 56438,    // -50% damage taken, granted inside the bubble
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
// DATA_MALYGOS from the core's eye_of_eternity.h. Script headers are not on a module's include path,
// so the value is mirrored here; it is the first entry of that file's Data enum.
const uint32 EOE_DATA_MALYGOS = 0;
// boss_malygos.cpp stores the P3 Surge of Power victims in its AI's guid slots, one per target, and
// fills them 3s before the beam goes off. Mirrored here for the same reason as EOE_DATA_MALYGOS.
const int32 EOE_DATA_FIRST_SURGE_TARGET_GUID = 14;
const uint8 EOE_NUM_MAX_SURGE_TARGETS = 3;

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

// P2: this bot is out of shelter and an Arcane Overload bubble is reachable. The bubble grants
// SPELL_ARCANE_OVERLOAD_PROTECTION (-50% damage taken), which is what makes Surge of Power survivable.
class MalygosBubbleTrigger : public Trigger
{
public:
    MalygosBubbleTrigger(PlayerbotAI* botAI) : Trigger(botAI, "malygos bubble") {}
    bool IsActive() override;
};

// P2: a Hover Disk has landed and lost its Nexus Lord, so melee dps can board it.
class MalygosFreeDiskTrigger : public Trigger
{
public:
    MalygosFreeDiskTrigger(PlayerbotAI* botAI) : Trigger(botAI, "malygos free disk") {}
    bool IsActive() override;
};

// P2: this bot is riding a Hover Disk.
class MalygosOnDiskTrigger : public Trigger
{
public:
    MalygosOnDiskTrigger(PlayerbotAI* botAI) : Trigger(botAI, "malygos on disk") {}
    bool IsActive() override;
};

// P2: Surge of Power beam active (surge NPC summoned or boss casting the P2 surge).
class SurgeOfPowerTrigger : public Trigger
{
public:
    SurgeOfPowerTrigger(PlayerbotAI* botAI) : Trigger(botAI, "surge of power") {}
    bool IsActive() override;
};

// P3: this bot is on a Wyrmrest Skytalon. EoE-owned on purpose - the Oculus "group flying" trigger
// this used to borrow also needs the raid leader to be mounted, and a human who has not taken a
// drake yet would leave the whole flight unable to position or turn.
class MalygosDrakeFlightTrigger : public Trigger
{
public:
    MalygosDrakeFlightTrigger(PlayerbotAI* botAI) : Trigger(botAI, "malygos drake flight") {}
    bool IsActive() override;
};

// P3: a Static Field hazard is near this bot's drake.
class StaticFieldTrigger : public Trigger
{
public:
    StaticFieldTrigger(PlayerbotAI* botAI) : Trigger(botAI, "static field") {}
    bool IsActive() override;
};

// P3: Malygos has picked this bot's drake as a Surge of Power victim (10/25-man).
class DrakeSurgeTrigger : public Trigger
{
public:
    DrakeSurgeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "drake surge") {}
    bool IsActive() override;
};

#endif
