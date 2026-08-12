/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_EOEACTIONS_H
#define PLAYERBOTS_EOEACTIONS_H

#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "VehicleActions.h"

#include <cmath>
#include <vector>

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
const float MALYGOS_MAINTANK_OFFSET = 42.0f;
const float MALYGOS_STACK_OFFSET = 12.0f;
const float MALYGOS_HUNTER_OFFSET = -14.0f;
const float MALYGOS_P1_POSITION_TOLERANCE = 5.0f;
const float MALYGOS_MELEE_HOLD_DISTANCE = 15.0f;

struct MalygosP1Layout
{
    std::pair<float, float> tank;
    std::pair<float, float> stack;
    std::pair<float, float> hunter;
    std::pair<float, float> grip;
};

// The offsets above rotated onto the landing bearing, latched per instance for the whole pull.
MalygosP1Layout const& GetMalygosP1Layout(Player* bot);

const float BUBBLE_SEARCH_RADIUS = 60.0f;
// Fraction of its protection radius an Arcane Overload bubble loses per tick, and the floor below
// which it is not worth crossing the platform for.
const float BUBBLE_SHRINK_PER_TICK = 0.02f;
const float BUBBLE_MIN_USABLE_FACTOR = 0.35f;

// Centre to centre, no bounding radii: the script's IsWithinDist3d takes the Position overload.
const float POWER_SPARK_BUFF_RADIUS = 12.0f;
// Where a DK parks to Death Grip a spark. Known-open gap: the corpse's ground buff (55849,
// triggered by 55852) reaches 8y, and this midpoint is 13y from both the stack and the hunters.
const float POWER_SPARK_GRIP_OFFSET = (MALYGOS_STACK_OFFSET + MALYGOS_HUNTER_OFFSET) / 2.0f;
// Tighter than the general P1 tolerance - the grip spot is only worth walking to if it is hit.
const float POWER_SPARK_GRIP_TOLERANCE = 2.0f;
const float POWER_SPARK_GRIP_ENGAGE_RADIUS = 45.0f;
const float POWER_SPARK_GRIP_SAFE_BOSS_DISTANCE = POWER_SPARK_BUFF_RADIUS + 6.0f;
// Slack on melee reach before a melee bot lets go of a spark it is already hitting.
const float POWER_SPARK_MELEE_STICKY = 3.0f;
// How close a spark has to be before a DK spends a rune snaring it. A gripped spark lands at his
// feet, so this keeps Chains of Ice on the one he pulled rather than one crossing to Malygos.
const float POWER_SPARK_SNARE_RADIUS = 15.0f;

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

const float DISK_APPROACH_REACH = 3.0f;
const float DISK_APPROACH_TOLERANCE = 2.0f;

const float SURGE_BEAM_CLEAR_DISTANCE = 12.0f;
const float SURGE_BEAM_SIDESTEP = 6.0f;

// Landed, selectable and free doubles as "its Nexus Lord is dead".
Unit* FindFreeHoverDisk(Player* bot);

// Disk duty is melee dps only. Everything that reasons about disks has to agree on this.
bool IsEligibleDiskRider(Player* bot);

bool AnyScionAlive(Player* bot);

float GetBubbleShrinkFactor(Unit* bubble);

bool IsSafelySheltered(Player* bot);

void GetStaticFields(Player* bot, std::vector<Unit*>& fields);

bool IsClearOfStaticFields(float x, float y, std::vector<Unit*> const& fields, float safeRadius);

// Where the P3 flight parks. The heading is latched per instance and only ever advances.
bool GetDrakeStackPoint(Player* bot, std::vector<Unit*> const& fields, float& x, float& y, float& z);

// The next waypoint towards a stack point: a hop around the boss rather than a chord across him,
// forward around the ring once the drake is on it.
void GetDrakeApproachPoint(Unit* drake, Unit* boss, float destX, float destY, float& x, float& y);

Unit* GetNearestPowerSpark(PlayerbotAI* botAI);

// The spark this bot should hit: of the ones it can reach standing still, the one nearest Malygos.
// currentTarget keeps it from swapping off a spark that has drifted just past reach.
Unit* GetPowerSparkToKill(PlayerbotAI* botAI, Unit* currentTarget);

// The spark a DK should chain: pulled in close and not snared yet.
Unit* GetPowerSparkToSnare(PlayerbotAI* botAI);

// Read by both the position action and the grip, so they cannot disagree about where the DK is.
bool IsOnPowerSparkGripDuty(PlayerbotAI* botAI);

// The drakes assigned to heal: guid-sorted, so every bot derives the same roster without talking.
void GetDrakeHealerGuids(PlayerbotAI* botAI, std::vector<ObjectGuid>& out);

bool IsDrakeHealer(PlayerbotAI* botAI, std::vector<ObjectGuid> const& healers);

// Power check for a drake self-cast: CanCastVehicleSpell reports BAD_TARGETS there, and
// CastVehicleSpell reports success even when the cast it prepared was rejected.
bool DrakeCanAfford(Unit* drake, uint32 spellId);

bool DrakeCanAffordWithShield(Unit* drake, uint32 spellId);

// Every Skytalon the raid is flying, walked from the group rather than the creature cache.
void GetDrakeFlight(Player* bot, std::vector<Unit*>& drakes);

// Reads the Life Burst buff as ground truth for who burst when, and how long ago.
uint32 DrakeAuraRemainingMs(Unit* drake, uint32 spellId);

// Healer drakes by energy descending, guid ascending. Every bot derives the same order.
uint8 GetDrakeHealerRank(PlayerbotAI* botAI, std::vector<ObjectGuid> const& healers);

bool IsDrakeSurgeTarget(PlayerbotAI* botAI);

class MalygosPositionAction : public MovementAction
{
public:
    MalygosPositionAction(PlayerbotAI* botAI, std::string const name = "malygos position") : MovementAction(botAI, name)
    {
    }

    bool Execute(Event event) override;
};

class MalygosTargetAction : public AttackAction
{
public:
    MalygosTargetAction(PlayerbotAI* botAI, std::string const name = "malygos target") : AttackAction(botAI, name) {}

    bool Execute(Event event) override;
};

class PullPowerSparkAction : public Action
{
public:
    PullPowerSparkAction(PlayerbotAI* botAI, std::string const name = "malygos pull power spark")
        : Action(botAI, name)
    {
    }

    bool Execute(Event event) override;
    bool isUseful() override;
};

class KillPowerSparkAction : public AttackAction
{
public:
    KillPowerSparkAction(PlayerbotAI* botAI, std::string const name = "malygos kill power spark")
        : AttackAction(botAI, name)
    {
    }

    bool Execute(Event event) override;
    bool isUseful() override;
};

// P2: strip the Nexus Lords' self-cast Haste (57060), without touching the mage's own target.
class MalygosSpellstealAction : public Action
{
public:
    MalygosSpellstealAction(PlayerbotAI* botAI, std::string const name = "malygos spellsteal") : Action(botAI, name) {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    Unit* GetHastedLord();
};

class MalygosSeekBubbleAction : public MovementAction
{
public:
    MalygosSeekBubbleAction(PlayerbotAI* botAI, std::string const name = "malygos seek bubble")
        : MovementAction(botAI, name)
    {
    }

    bool Execute(Event event) override;

private:
    // Latched, or a bot walks in circles between two shrinking bubbles.
    ObjectGuid assignedBubbleGuid;
};

class MalygosBoardDiskAction : public EnterVehicleAction
{
public:
    MalygosBoardDiskAction(PlayerbotAI* botAI) : EnterVehicleAction(botAI, "malygos board disk") {}

    bool Execute(Event event) override;
};

// P2: fly a boarded Hover Disk to the Scions, steering the vehicle rather than moving the bot.
class MalygosRideDiskAction : public AttackAction
{
public:
    MalygosRideDiskAction(PlayerbotAI* botAI) : AttackAction(botAI, "malygos ride disk") {}

    bool Execute(Event event) override;
    bool isPossible() override;

private:
    bool descending = false;
};

class AvoidSurgeOfPowerAction : public MovementAction
{
public:
    AvoidSurgeOfPowerAction(PlayerbotAI* botAI, std::string const name = "malygos avoid surge of power")
        : MovementAction(botAI, name)
    {
    }

    bool Execute(Event event) override;
};

// P3: sole owner of the drake's position. Nothing else may steer a Skytalon.
class EoEFlyDrakeAction : public MovementAction
{
public:
    EoEFlyDrakeAction(PlayerbotAI* ai) : MovementAction(ai, "eoe fly drake") {}

    bool Execute(Event event) override;
    bool isPossible() override;

private:
    uint32 stackCalcAtMs = 0;
    float stackX = 0.0f;
    float stackY = 0.0f;
    float stackZ = 0.0f;
    bool stackValid = false;

    // Last destination handed to the MotionMaster, so the same one is not restamped every tick.
    float issuedX = 0.0f;
    float issuedY = 0.0f;
    bool issued = false;
};

class EoEDrakeAttackAction : public Action
{
public:
    EoEDrakeAttackAction(PlayerbotAI* botAI) : Action(botAI, "eoe drake attack") {}

    bool Execute(Event event) override;
    bool isPossible() override;

protected:
    bool CastDrakeSpellAction(Unit* drake, Unit* target, uint32 spellId);
    bool DrakeDpsAction(Unit* drake, Unit* target);
    bool DrakeHealAction(Unit* drake, std::vector<ObjectGuid> const& healers);
};

class DrakeSurgeShieldAction : public Action
{
public:
    DrakeSurgeShieldAction(PlayerbotAI* botAI) : Action(botAI, "eoe drake surge shield") {}

    bool Execute(Event event) override;
    bool isPossible() override;

private:
    // A gap in lastSeenMs is how a fresh fixate is told from the one already being handled.
    uint32 fixateAtMs = 0;
    uint32 lastSeenMs = 0;
};

#endif
