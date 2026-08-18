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

// ---------------------------------------------------------------------------------------------
// Ids
// ---------------------------------------------------------------------------------------------

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

const float EOE_TWO_PI = 2.0f * static_cast<float>(M_PI);

// Instance ids are recycled, so a latch with no window eventually hands a fresh pull the previous
// tenant's state. Far longer than any encounter, so it can never expire mid-fight.
const uint32 EOE_LATCH_STALE_MS = 5 * MINUTE * IN_MILLISECONDS;

// ---------------------------------------------------------------------------------------------
// Arena geometry
// ---------------------------------------------------------------------------------------------

// Platform centre and floor (CenterPos in the core script).
const std::pair<float, float> MALYGOS_CENTER_POSITION = {754.395f, 1301.27f};
const float MALYGOS_PLATFORM_Z = 266.10f;
// The four bearings from centre Malygos can land on, mirrored from the core's FourSidesPos.
const float MALYGOS_LANDING_ANGLES[] = {-2.3729f, 0.8117f, 2.3467f, -0.7783f};
const uint8 MALYGOS_LANDING_ANGLE_COUNT = 4;

// P2/P4 anti-fall ring around the platform centre, and how far inside it a drifting bot is put back.
const float MALYGOS_ANTIFALL_RADIUS = 30.0f;
const float MALYGOS_ANTIFALL_INSET = 3.0f;

// P1 hold spots, as signed distances from centre along the landing bearing: positive towards Malygos.
// The tank spot cannot pass 47.5: the platform floor steps up a yard there. It also cannot pass 48.5,
// where the melee stack starts being clamped inward - Malygos parks ~21.5 yd short of his victim.
const float MALYGOS_MAINTANK_OFFSET = 46.0f;
const float MALYGOS_STACK_OFFSET = 12.0f;
const float MALYGOS_HUNTER_OFFSET = -14.0f;
const float MALYGOS_P1_POSITION_TOLERANCE = 5.0f;
const float MALYGOS_MELEE_HOLD_DISTANCE = 15.0f;

// ---------------------------------------------------------------------------------------------
// Power Sparks (P1)
// ---------------------------------------------------------------------------------------------

// Centre to centre, no bounding radii: the script's IsWithinDist3d takes the Position overload.
const float POWER_SPARK_BUFF_RADIUS = 12.0f;
// Where a DK parks to Death Grip a spark. Death Grip reaches 30 yd plus both combat reaches, a spark
// hands its buff over 12 yd from Malygos, and he parks about 23 yd out - so anything inside +3.7 can
// never catch a spark walking in on his own bearing. The ceiling is grip duty itself, which needs
// POWER_SPARK_GRIP_SAFE_BOSS_DISTANCE between him and this spot and runs out at +5.2. Close enough
// to the melee stack that the corpse ground buff (55849, 8y, +50% damage) reaches them.
const float POWER_SPARK_GRIP_OFFSET = 4.5f;
// Tighter than the general P1 tolerance - the grip spot is only worth walking to if it is hit.
const float POWER_SPARK_GRIP_TOLERANCE = 2.0f;
const float POWER_SPARK_GRIP_ENGAGE_RADIUS = 45.0f;
const float POWER_SPARK_GRIP_SAFE_BOSS_DISTANCE = POWER_SPARK_BUFF_RADIUS + 6.0f;
// Slack on melee reach before a melee bot lets go of a spark it is already hitting.
const float POWER_SPARK_MELEE_STICKY = 3.0f;
// How close a spark has to be before a DK spends a rune snaring it. A gripped spark lands at his
// feet, so this keeps Chains of Ice on the one he pulled rather than one crossing to Malygos.
const float POWER_SPARK_SNARE_RADIUS = 15.0f;

// ---------------------------------------------------------------------------------------------
// Arcane Overload bubbles, hover disks and Surge of Power (P2)
// ---------------------------------------------------------------------------------------------

const float BUBBLE_SEARCH_RADIUS = 60.0f;
// Fraction of its protection radius an Arcane Overload bubble loses per tick, and the floor below
// which it is not worth crossing the platform for.
const float BUBBLE_SHRINK_PER_TICK = 0.02f;
const float BUBBLE_MIN_USABLE_FACTOR = 0.35f;

const float DISK_APPROACH_REACH = 3.0f;
const float DISK_APPROACH_TOLERANCE = 2.0f;

// How far the P2 Surge of Power focus is looked for.
const float EOE_SURGE_SEARCH_RADIUS = 100.0f;
const float SURGE_BEAM_CLEAR_DISTANCE = 12.0f;
const float SURGE_BEAM_SIDESTEP = 6.0f;

// ---------------------------------------------------------------------------------------------
// Drake flight (P3)
// ---------------------------------------------------------------------------------------------

// Arcane Pulse (57432) radius - the hard floor on the P3 flight's distance to the boss.
const float ARCANE_PULSE_RADIUS = 30.0f;
// Radius of the P3 stack ring, and its fixed height and heading. Malygos' live Z is no good: he
// opens the phase 70y up and sinks to his fight position seconds later.
const float DRAKE_STACK_RADIUS = ARCANE_PULSE_RADIUS + 15.0f;
const float MALYGOS_P3_BOSS_Z = MALYGOS_PLATFORM_Z - 5.0f;
const float DRAKE_STACK_ANGLE = -static_cast<float>(M_PI_2);
// How far off the stack point a drake drifts before it re-flies. Loose on purpose: it is what stops
// the flight piling onto one coordinate.
const float DRAKE_STACK_TOLERANCE = 10.0f;
const uint32 DRAKE_STACK_RECALC_MS = 300;
// How far around the ring a drake covers in one hop, kept short so the chord clears Arcane Pulse.
const float DRAKE_APPROACH_ARC = static_cast<float>(M_PI) / 3.0f;
const uint8 DRAKE_RING_HEADINGS = 24;
const float DRAKE_DESTINATION_EPSILON = 2.0f;

// Drake abilities reach 60y; leave headroom for drakes parked on the far side of the stack.
const float DRAKE_ATTACK_RANGE = 55.0f;
const float DRAKE_FORMUP_RADIUS = 20.0f;
const float DRAKE_FORMUP_SPREAD = 15.0f;

const uint8 DRAKE_HEALERS_25MAN = 5;
const uint8 DRAKE_HEALERS_10MAN = 2;
// Combo points a healer banks before Life Burst - for the +50% healing buff, not the heal.
const uint8 DRAKE_LIFE_BURST_COMBO = 5;
const uint8 DRAKE_ENGULF_COMBO = 3;
// What a fixated drake needs before it dumps the bank into Engulf instead of saving it for the
// shield. One point refreshes the stack for only 6 s, less than the cycle that rebuilds it, so the
// stack drops; two carries 10 s.
const uint8 DRAKE_ENGULF_SURGE_COMBO = 2;
// Life Burst's self buff at full combo, and how little may be left before a healer renews it.
const uint32 DRAKE_LIFE_BURST_BUFF_MS = 25000;
const uint32 DRAKE_LIFE_BURST_REFRESH_MS = 5000;
const uint32 DRAKE_BURST_STAGGER_MS = 1500;
// Drake health at which a burst is wanted, and at which the whole corps goes and the stagger is off.
const uint8 DRAKE_BURST_HEALTH_PCT = 90;
const uint8 DRAKE_BURST_EMERGENCY_PCT = 30;
const uint32 DRAKE_LIFE_BURST_HEAL = 5000;
// A Life Burst plus a Flame Shield. Below this a capped healer stops casting and lets the bar fill.
const uint32 DRAKE_HOLD_ENERGY_FLOOR = 75;

// The beam has to be covered across [DELAY, END]; nothing before it counts.
const uint32 SURGE_BEAM_DELAY_MS = 3000;
const uint32 SURGE_BEAM_END_MS = 6000;
// How long the boss holds a drake in its surge slots - one full repeat of EVENT_SPELL_PH3_SURGE_OF_POWER.
// A drake still flagged this long after its fixate began has been picked again, which is the only
// signal there is: the boss clears and refills the slots inside one UpdateAI, so a back-to-back pick
// leaves no gap to spot. Runs a few hundred ms early when a competing event wins the tick, which is
// far inside the shield's one-second granularity.
const uint32 SURGE_CYCLE_MS = 7000;
const uint32 DRAKE_SHIELD_BASE_MS = 1000;
const uint32 DRAKE_SHIELD_MS_PER_COMBO = 1000;
// Above this the bank is worth more as a finisher, so the shield waits for the rotation to spend it.
const uint8 DRAKE_SHIELD_MAX_COMBO = 3;
// What a fixated drake rebuilds to and stops at. Two points would cover the whole beam instead of
// its first two seconds, but the bar cannot fund them: the rotation spends faster than the drake
// regenerates, and a shield that never goes up costs the full 72,000.
const uint8 DRAKE_SHIELD_RESERVE_COMBO = 1;
const uint32 DRAKE_FIXATE_GAP_MS = 2000;

// Radius a drake has to clear of a Static Field, and the wider radius the stack point has to clear
// because drakes park anywhere within DRAKE_STACK_TOLERANCE of it.
const float STATIC_FIELD_SAFE_RADIUS = 32.0f;
const float STATIC_FIELD_CLEARANCE = STATIC_FIELD_SAFE_RADIUS + DRAKE_STACK_TOLERANCE;

#endif
