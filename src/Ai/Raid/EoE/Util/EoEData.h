/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_EOEDATA_H
#define PLAYERBOTS_EOEDATA_H

#include "Common.h"

#include <cmath>
#include <utility>

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

    SPELL_HASTE                         = 57060,

    SPELL_ARCANE_OVERLOAD_AURA          = 56432,
    SPELL_ARCANE_OVERLOAD_PROTECTION    = 56438,
    SPELL_SURGE_OF_POWER_P2             = 56505,

    SPELL_FLAME_SPIKE                   = 56091,
    SPELL_ENGULF_IN_FLAMES              = 56092,
    SPELL_REVIVIFY                      = 57090,
    SPELL_LIFE_BURST                    = 57143,
    SPELL_FLAME_SHIELD                  = 57108,
};

inline constexpr uint32 EOE_MAP_ID = 616;
inline constexpr uint8 EOE_RAID_SIZE_10MAN = 10;
inline constexpr uint8 EOE_RAID_SIZE_25MAN = 25;
inline constexpr uint32 EOE_DATA_MALYGOS = 0;
inline constexpr int32 EOE_DATA_FIRST_SURGE_TARGET_GUID = 14;
inline constexpr uint8 EOE_NUM_MAX_SURGE_TARGETS = 3;

inline constexpr float EOE_TWO_PI = 2.0f * static_cast<float>(M_PI);
inline constexpr float EOE_DIRECTION_EPSILON = 0.01f;

inline constexpr uint32 EOE_LATCH_STALE_MS = 5 * MINUTE * IN_MILLISECONDS;

inline constexpr std::pair<float, float> MALYGOS_CENTER_POSITION = {754.395f, 1301.27f};
inline constexpr float MALYGOS_PLATFORM_Z = 266.10f;
inline constexpr float MALYGOS_LANDING_ANGLES[] = {-2.3729f, 0.8117f, 2.3467f, -0.7783f};
inline constexpr uint8 MALYGOS_LANDING_ANGLE_COUNT = 4;

inline constexpr float MALYGOS_ANTIFALL_RADIUS = 30.0f;
inline constexpr float MALYGOS_ANTIFALL_INSET = 3.0f;

inline constexpr float MALYGOS_MAINTANK_OFFSET = 46.0f;
inline constexpr float MALYGOS_STACK_OFFSET = 12.0f;
inline constexpr float MALYGOS_HUNTER_OFFSET = -14.0f;
inline constexpr float MALYGOS_P1_POSITION_TOLERANCE = 5.0f;
inline constexpr float MALYGOS_MELEE_HOLD_DISTANCE = 15.0f;

inline constexpr float POWER_SPARK_BUFF_RADIUS = 12.0f;
inline constexpr float POWER_SPARK_GRIP_OFFSET = 4.5f;
inline constexpr float POWER_SPARK_GRIP_TOLERANCE = 2.0f;
inline constexpr float POWER_SPARK_GRIP_ENGAGE_RADIUS = 45.0f;
inline constexpr float POWER_SPARK_GRIP_SAFE_BOSS_DISTANCE = POWER_SPARK_BUFF_RADIUS + 6.0f;
inline constexpr float POWER_SPARK_MELEE_STICKY = 3.0f;
inline constexpr float POWER_SPARK_SNARE_RADIUS = 15.0f;

inline constexpr float BUBBLE_SEARCH_RADIUS = 60.0f;
inline constexpr float BUBBLE_SHRINK_PER_TICK = 0.02f;
inline constexpr float BUBBLE_MIN_USABLE_FACTOR = 0.35f;

inline constexpr float DISK_APPROACH_REACH = 3.0f;
inline constexpr float DISK_APPROACH_TOLERANCE = 2.0f;
inline constexpr float DISK_MAX_WALK_DISTANCE = 40.0f;
inline constexpr float DISK_DISMOUNT_MAX_HEIGHT = 5.0f;

inline constexpr float EOE_SURGE_SEARCH_RADIUS = 100.0f;
inline constexpr float SURGE_BEAM_CLEAR_DISTANCE = 12.0f;
inline constexpr float SURGE_BEAM_SIDESTEP = 6.0f;

inline constexpr float ARCANE_PULSE_RADIUS = 30.0f;
inline constexpr float DRAKE_STACK_RADIUS = ARCANE_PULSE_RADIUS + 15.0f;
inline constexpr float MALYGOS_P3_BOSS_Z = MALYGOS_PLATFORM_Z - 5.0f;
inline constexpr float DRAKE_STACK_ANGLE = -static_cast<float>(M_PI_2);
inline constexpr float DRAKE_STACK_TOLERANCE = 10.0f;
inline constexpr uint32 DRAKE_STACK_RECALC_MS = 300;
inline constexpr float DRAKE_APPROACH_ARC = static_cast<float>(M_PI) / 3.0f;
inline constexpr uint8 DRAKE_RING_HEADINGS = 24;
inline constexpr float DRAKE_DESTINATION_EPSILON = 2.0f;

inline constexpr float DRAKE_ATTACK_RANGE = 55.0f;
inline constexpr float DRAKE_FORMUP_RADIUS = 20.0f;
inline constexpr float DRAKE_FORMUP_SPREAD = 15.0f;

inline constexpr uint8 DRAKE_HEALERS_25MAN = 5;
inline constexpr uint8 DRAKE_HEALERS_10MAN = 2;
inline constexpr uint8 DRAKE_LIFE_BURST_COMBO = 5;
inline constexpr uint8 DRAKE_ENGULF_COMBO = 3;
inline constexpr uint8 DRAKE_ENGULF_SURGE_COMBO = 2;
inline constexpr uint32 DRAKE_LIFE_BURST_BUFF_MS = 25000;
inline constexpr uint32 DRAKE_LIFE_BURST_REFRESH_MS = 5000;
inline constexpr uint32 DRAKE_BURST_STAGGER_MS = 1500;
inline constexpr uint8 DRAKE_BURST_HEALTH_PCT = 90;
inline constexpr uint8 DRAKE_BURST_EMERGENCY_PCT = 30;
inline constexpr uint32 DRAKE_LIFE_BURST_HEAL = 5000;
inline constexpr uint32 DRAKE_HOLD_ENERGY_FLOOR = 75;

inline constexpr uint32 SURGE_BEAM_DELAY_MS = 3000;
inline constexpr uint32 SURGE_BEAM_END_MS = 6000;
inline constexpr uint32 SURGE_CYCLE_MS = 7000;
inline constexpr uint32 DRAKE_SHIELD_BASE_MS = 1000;
inline constexpr uint32 DRAKE_SHIELD_MS_PER_COMBO = 1000;
inline constexpr uint8 DRAKE_SHIELD_MAX_COMBO = 3;
inline constexpr uint8 DRAKE_SHIELD_RESERVE_COMBO = 1;
inline constexpr uint32 DRAKE_FIXATE_GAP_MS = 2000;

inline constexpr float STATIC_FIELD_SAFE_RADIUS = 32.0f;
inline constexpr float STATIC_FIELD_CLEARANCE = STATIC_FIELD_SAFE_RADIUS + DRAKE_STACK_TOLERANCE;

#endif
