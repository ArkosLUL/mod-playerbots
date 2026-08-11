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

#include <vector>

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

    // Nexus Lord self-cast, the one buff worth a spellsteal in P2.
    SPELL_HASTE                         = 57060,

    // Boss hazards (verified against core boss_malygos.cpp)
    SPELL_ARCANE_OVERLOAD_AURA          = 56432,    // ticks on the P2 bubble, re-granting the protection
    SPELL_ARCANE_OVERLOAD_PROTECTION    = 56438,    // -50% damage taken, granted inside the bubble
    SPELL_SURGE_OF_POWER_P2             = 56505,    // P2 beam

    // Drake Abilities:
    // DPS
    SPELL_FLAME_SPIKE                   = 56091,
    SPELL_ENGULF_IN_FLAMES              = 56092,
    // Healing
    SPELL_REVIVIFY                      = 57090,
    SPELL_LIFE_BURST                    = 57143,
    // Utility
    SPELL_FLAME_SHIELD                  = 57108,
};

const uint32 EOE_MAP_ID = 616;
// DATA_MALYGOS, mirrored from the core's eye_of_eternity.h - script headers are not on a
// module's include path. First entry of that file's Data enum.
const uint32 EOE_DATA_MALYGOS = 0;
// Guid slots boss_malygos.cpp fills with the P3 surge victims, 3s before the beam.
// Mirrored for the same reason as EOE_DATA_MALYGOS.
const int32 EOE_DATA_FIRST_SURGE_TARGET_GUID = 14;
const uint8 EOE_NUM_MAX_SURGE_TARGETS = 3;
// How far the P2 Surge of Power focus is looked for.
const float EOE_SURGE_SEARCH_RADIUS = 100.0f;

// One instance-wide cache in place of a grid sweep per bot per tick. Guids are cached, not
// pointers, so a creature that despawns inside the window cannot come back dangling.
void GetEoECreatures(Player* bot, uint32 entry, std::vector<Unit*>& out);
Unit* GetNearestEoECreature(Player* bot, uint32 entry, float maxDist = 1000.0f);
bool AnyEoECreature(Player* bot, uint32 entry);

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
    // Sparks walk in from the edge; checking every tick buys nothing.
    PowerSparkTrigger(PlayerbotAI* botAI) : Trigger(botAI, "malygos power spark", 200) {}
    bool IsActive() override;
};

// P2: this bot is out of shelter and an Arcane Overload bubble is reachable. The bubble grants
// SPELL_ARCANE_OVERLOAD_PROTECTION (-50% damage taken), which is what makes Surge of Power survivable.
class MalygosBubbleTrigger : public Trigger
{
public:
    // The seek is a walk across the platform, so 200ms of latency is invisible.
    MalygosBubbleTrigger(PlayerbotAI* botAI) : Trigger(botAI, "malygos bubble", 200) {}
    bool IsActive() override;
};

class MalygosFreeDiskTrigger : public Trigger
{
public:
    // A disk sits on the ground until someone takes it; there is nothing to race.
    MalygosFreeDiskTrigger(PlayerbotAI* botAI) : Trigger(botAI, "malygos free disk", 300) {}
    bool IsActive() override;
};

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
    // The peel is a MoveAway the MotionMaster carries on with, and the sweep behind this comes
    // back empty almost every time.
    SurgeOfPowerTrigger(PlayerbotAI* botAI) : Trigger(botAI, "malygos surge of power", 200) {}
    bool IsActive() override;
};

// P3: this bot is on a Wyrmrest Skytalon. EoE-owned because the Oculus "group flying"
// trigger also needs the raid leader mounted.
class MalygosDrakeFlightTrigger : public Trigger
{
public:
    MalygosDrakeFlightTrigger(PlayerbotAI* botAI) : Trigger(botAI, "malygos drake flight") {}
    bool IsActive() override;
};

class DrakeSurgeTrigger : public Trigger
{
public:
    DrakeSurgeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "eoe drake surge") {}
    bool IsActive() override;
};

#endif
