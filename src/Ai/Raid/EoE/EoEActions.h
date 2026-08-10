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

// Platform centre (CenterPos in the core script). Used as the P2 anti-fall anchor.
const std::pair<float, float> MALYGOS_CENTER_POSITION = {754.395f, 1301.27f};
// Platform floor (CenterPos.z). A disk rider has to come back down to about here before it is safe
// to dismount, otherwise it steps off 20-30y up.
const float MALYGOS_PLATFORM_Z = 266.10f;
// P1 tank spot: 42y due north of centre. The Exit Portal sits at 43.4y, so there is ground here.
// Dragging Malygos out this far leaves the whole platform behind him for the raid.
const std::pair<float, float> MALYGOS_MAINTANK_POSITION = {754.395f, 1343.27f};
// P1 raid stack, 12y north of centre. Malygos has a CombatReach of 20, so he stops roughly 21y
// short of the tank - around y=1322 - and everyone else needs to be south of *that*, not south of
// the tank. A stack any closer to the tank spot sits between Malygos and his victim, i.e. straight
// in the Arcane Breath cone. From here melee are still well inside his 20y reach.
const std::pair<float, float> MALYGOS_STACK_POSITION = {754.395f, 1313.27f};
// P1 ranged dps spot, 14y south of centre. Malygos' CombatReach of 20 pushes a hunter's 5y minimum
// range out to about 28y of centre-to-centre distance (Spell::CheckRange adds GetMeleeRange on top of
// the minimum for SPELL_RANGE_RANGED), and from the raid stack every shot came back TOO_CLOSE. It is
// also outside the ~23.5y at which "enemy too close for spell" starts firing escape actions that
// outrank the position hold. From here Malygos is ~34.5y away, so even a bot drifting the full
// MALYGOS_P1_POSITION_TOLERANCE toward him can still shoot.
const std::pair<float, float> MALYGOS_RANGED_POSITION = {754.395f, 1287.27f};
// How close a bot has to be to its assigned P1 spot before it stops correcting.
const float MALYGOS_P1_POSITION_TOLERANCE = 5.0f;
// How far the raid stack may sit from Malygos before it is pulled in towards him. Melee range
// against him is about 22.8y - his 20y CombatReach, the player's own reach and the 4/3 the core
// adds on top - and a bot may park MALYGOS_P1_POSITION_TOLERANCE off its spot, so anything up to
// ~17 is still swingable. 15 keeps a margin.
const float MALYGOS_MELEE_HOLD_DISTANCE = 15.0f;

// How close a bubble has to be before a bot will walk to it, and the radius the shelter checks
// treat as "the one covering me".
const float BUBBLE_SEARCH_RADIUS = 60.0f;
// Each tick of SPELL_ARCANE_OVERLOAD_AURA re-grants the protection over a radius shrunk by this
// much (SpellAuraEffects.cpp, aura 56432). Below BUBBLE_MIN_USABLE_FACTOR of the original radius a
// bubble is not worth crossing the platform for - it will despawn before it pays off.
const float BUBBLE_SHRINK_PER_TICK = 0.02f;
const float BUBBLE_MIN_USABLE_FACTOR = 0.35f;

// npc_power_spark hands Malygos its buff the moment it is within 12y of him (centre to centre, no
// bounding radii - the script's IsWithinDist3d resolves to the Position overload).
const float POWER_SPARK_BUFF_RADIUS = 12.0f;
// Where a DK parks to Death Grip a spark. Grip lands the target on the caster, and a killed spark
// leaves SPELL_POWER_SPARK_GROUND_BUFF (55852) on its corpse for a minute - so where the DK stands
// decides who gets the buff. This is the midpoint of MALYGOS_STACK_POSITION and
// MALYGOS_RANGED_POSITION, which is the best spot available without knowing 55852's radius: it
// maximises the smaller of the two distances. It also lands ~21y from where Malygos parks, so a
// spark dropped here still has to walk 9y before it could hand him anything, and it has ~12k hp.
const std::pair<float, float> POWER_SPARK_GRIP_POSITION = {754.395f, 1300.27f};
// Tighter than the general P1 tolerance - the grip spot is only worth walking to if it is hit.
const float POWER_SPARK_GRIP_TOLERANCE = 2.0f;
// How close a spark has to be before a DK gives up boss uptime to go and meet it.
const float POWER_SPARK_GRIP_ENGAGE_RADIUS = 45.0f;
// Called off if Malygos ends up near the grip spot: dropping a spark next to him hands over the buff.
const float POWER_SPARK_GRIP_SAFE_BOSS_DISTANCE = POWER_SPARK_BUFF_RADIUS + 6.0f;

// P3 drakes hold one stack point instead of trailing the raid leader. A drake that is still
// following is both moving and facing the wrong way, and CastVehicleSpell refuses to fire in either
// state - it turns the vehicle and bails out - so a following drake never lands an ability.
// Malygos is pacified and immobile for all of phase 3 (MI_POINT_PH_3_FIGHT_POSITION, then
// UNIT_FLAG_DISABLE_MOVE), so an offset from him is a fixed spot the whole flight can agree on.

// Arcane Pulse (57432) is a self-cast every 3s for ~28k arcane over this radius. It is the hard
// floor on how close the flight may ever get to him - two ticks kill a Skytalon.
const float ARCANE_PULSE_RADIUS = 30.0f;
// The pulse radius plus the drift the stack tolerates, plus a little. A drake nudged the full
// tolerance boss-ward has to still be outside the pulse, and the far side has to stay inside
// DRAKE_ATTACK_RANGE, which is what pins this between 40 and 45.
const float DRAKE_STACK_RADIUS = ARCANE_PULSE_RADIUS + 15.0f;
// Height of the stack. Malygos' live Z is no good for this: he opens phase 3 at CenterPos.z + 70
// and only sinks to his fight position at CenterPos.z - 5 a few seconds later, so a stack anchored
// on him flies up over the arena first and then rides him back down.
const float MALYGOS_P3_BOSS_Z = MALYGOS_PLATFORM_Z - 5.0f;
// Fixed world heading for that offset, due south of the boss. Any constant does, as long as every
// drake picks the same one.
const float DRAKE_STACK_ANGLE = -static_cast<float>(M_PI_2);
// How far off the stack point a drake drifts before it re-flies. Loose on purpose: drakes park
// anywhere inside this, which is what stops the flight piling onto one coordinate.
const float DRAKE_STACK_TOLERANCE = 10.0f;
// The stack point only moves when a Static Field lands on it, so it is worth holding between
// ticks - but not for long. A field lands *on* the flight, so every tick spent on a stale answer is
// another pulse taken. The creature lookup behind it is cached, so this is nearly free to redo.
const uint32 DRAKE_STACK_RECALC_MS = 300;
// Drake abilities reach 60y; leave headroom for drakes parked on the far side of the stack.
const float DRAKE_ATTACK_RANGE = 55.0f;
// Raid-wide drake healer counts. The rest of the flight is dps.
const uint8 DRAKE_HEALERS_25MAN = 5;
const uint8 DRAKE_HEALERS_10MAN = 2;
// Combo points needed before a healer dumps Life Burst instead of stacking another Revivify.
const uint8 DRAKE_LIFE_BURST_COMBO = 5;

// Static Field (57430) drops a stationary NPC_STATIC_FIELD that pulses for its whole 20s life, and
// the boss lands a fresh one every 12s.
const float STATIC_FIELD_SAFE_RADIUS = 32.0f;
// What the stack point has to clear, as opposed to what a drake has to clear. Drakes park anywhere
// within DRAKE_STACK_TOLERANCE of the point, so a point only STATIC_FIELD_SAFE_RADIUS from a field
// leaves whoever stops on the field side of it inside the pulse.
const float STATIC_FIELD_CLEARANCE = STATIC_FIELD_SAFE_RADIUS + DRAKE_STACK_TOLERANCE;

// A Hover Disk whose Nexus Lord has died: the core lands it, turns it friendly and clears
// UNIT_FLAG_NOT_SELECTABLE, so those checks double as "its rider is dead".
Unit* FindFreeHoverDisk(Player* bot);

// Disk duty is melee dps only. Everything that reasons about disks has to agree on this, or a bot
// waits out P2 for a ride it is never allowed to take instead of taking shelter.
bool IsEligibleDiskRider(Player* bot);

// A disk ride only pays off while there is a Scion left to shoot at; the core keeps the disks
// around until the Nexus Lords are down too, so "no Scions" is a real state, not a transition.
bool AnyScionAlive(Player* bot);

// How much of its original protection radius an Arcane Overload bubble still covers, 1.0 at spawn
// down towards 0. The bubble's model never shrinks (the core declares 56435 but never casts it),
// so its apparent size says nothing - the aura's tick count is the only honest source.
float GetBubbleShrinkFactor(Unit* bubble);

// True while the bot sits in a bubble with enough radius left to stay put in. Goes false before
// the bubble dies, which is what lets a sheltered bot relocate early instead of after it pops.
bool IsSafelySheltered(Player* bot);

// Every live Static Field in the instance. There are never more than two and the arena is small,
// so a distance filter would only ever drop fields some part of the stack ring still cares about.
void GetStaticFields(Player* bot, std::vector<Unit*>& fields);

// True when (x, y) is at least safeRadius from every one of them.
bool IsClearOfStaticFields(float x, float y, std::vector<Unit*> const& fields, float safeRadius);

// Where the P3 flight parks: a fixed offset from Malygos, slid clear of any Static Field that has
// landed on it. The slide stays on the DRAKE_STACK_RADIUS ring around him and always goes the same
// way around it, so the dodge can neither cost range on the boss nor double back into a field the
// flight has already run from. Depends only on the boss and the fields, so every drake resolves the
// same spot without coordinating. False when there is no drake or no boss to anchor to.
bool GetDrakeStackPoint(Player* bot, std::vector<Unit*> const& fields, float& x, float& y, float& z);

// Nearest live Power Spark the bot can see, or nullptr.
Unit* GetNearestPowerSpark(PlayerbotAI* botAI);

// True while this bot should be holding POWER_SPARK_GRIP_POSITION instead of its usual P1 spot.
// Both the position action and the grip itself read this, so they cannot disagree about it.
bool IsOnPowerSparkGripDuty(PlayerbotAI* botAI);

// True when this bot flies a healing drake in P3. The flight action needs it too - a healer must be
// free to turn onto its heal target, so only dps drakes are pinned facing Malygos.
bool IsDrakeHealer(PlayerbotAI* botAI);

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
    PullPowerSparkAction(PlayerbotAI* botAI, std::string const name = "pull power spark") : Action(botAI, name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class KillPowerSparkAction : public AttackAction
{
public:
    KillPowerSparkAction(PlayerbotAI* botAI, std::string const name = "kill power spark") : AttackAction(botAI, name) {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

// P2: strip the Nexus Lords' self-cast Haste (57060) instead of letting them swing at speed.
// Targeting stays where it is - the mage keeps shooting whatever MalygosTargetAction picked.
class MalygosSpellstealAction : public Action
{
public:
    MalygosSpellstealAction(PlayerbotAI* botAI, std::string const name = "malygos spellsteal") : Action(botAI, name) {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    Unit* GetHastedLord();
};

// P2: take shelter inside an Arcane Overload bubble for the -50% damage aura.
class MalygosSeekBubbleAction : public MovementAction
{
public:
    MalygosSeekBubbleAction(PlayerbotAI* botAI, std::string const name = "malygos seek bubble")
        : MovementAction(botAI, name)
    {
    }

    bool Execute(Event event) override;

private:
    // Bubbles shrink and despawn constantly, so latch one per bot for as long as it lives - picking
    // "nearest" every tick makes bots walk in circles between two shrinking bubbles.
    ObjectGuid assignedBubbleGuid;
};

// P2: melee dps boards a Hover Disk freed by a dead Nexus Lord.
class MalygosBoardDiskAction : public EnterVehicleAction
{
public:
    MalygosBoardDiskAction(PlayerbotAI* botAI) : EnterVehicleAction(botAI, "malygos board disk") {}

    bool Execute(Event event) override;
};

// P2: fly a boarded Hover Disk to the Scions, which hover out of reach of the platform.
// Steers the vehicle through its MotionMaster rather than moving the bot itself.
class MalygosRideDiskAction : public AttackAction
{
public:
    MalygosRideDiskAction(PlayerbotAI* botAI) : AttackAction(botAI, "malygos ride disk") {}

    bool Execute(Event event) override;
    bool isPossible() override;

private:
    // Set while the disk is flying itself back down to the platform to drop the bot off.
    bool descending = false;
};

// P2 ground avoidance: clear the Surge of Power beam.
class AvoidSurgeOfPowerAction : public MovementAction
{
public:
    AvoidSurgeOfPowerAction(PlayerbotAI* botAI, std::string const name = "avoid surge of power") : MovementAction(botAI, name)
    {
    }

    bool Execute(Event event) override;
};

// P3: sole owner of the drake's position. Parks the flight on one stack point, keeps that point
// clear of Static Fields and turns the drake onto the boss, then returns false so the rotation gets
// the tick. Nothing else may steer a Skytalon - two owners is what made the drake bounce.
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

    // Last destination handed to the MotionMaster. Re-issuing MovePoint restarts the spline, so a
    // drake that gets the same destination stamped every tick crawls and never arrives.
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
    Unit* vehicleBase;
    bool CastDrakeSpellAction(Unit* target, uint32 spellId, uint32 cooldown);
    bool DrakeDpsAction(Unit* target);
    bool DrakeHealAction();
};

// P3: Surge of Power cannot be outrun. The boss fires it as a triggered instant 3s after the
// fixate lands, so there is no beam to walk out of and no cast to beat - Flame Shield is the whole
// answer, and everything it does not cover is a heal check.
class DrakeSurgeShieldAction : public Action
{
public:
    DrakeSurgeShieldAction(PlayerbotAI* botAI) : Action(botAI, "drake surge shield") {}

    bool Execute(Event event) override;
    bool isPossible() override;
};

#endif
