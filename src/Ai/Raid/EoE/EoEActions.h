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

// Every add, bubble and hazard in this fight lives on or just above the platform, which is under
// 50y across. Grid searches visit a cell grid (33y a side) out to their radius, and getPhase alone
// runs them several times per bot per tick, so the radius is worth keeping tight.
const float EOE_ADD_SEARCH_RADIUS = 100.0f;

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

// P3 drakes hold a ring around Malygos instead of trailing the raid leader. A drake that is still
// following is both moving and facing the wrong way, and CastVehicleSpell refuses to fire in either
// state - it turns the vehicle and bails out - so a following drake never lands an ability.
const float DRAKE_RING_RADIUS = 40.0f;
// How far off its ring slot a drake drifts before it re-flies. Malygos moves, so a tight tolerance
// would restamp the destination every tick and the drake would never stop long enough to cast.
const float DRAKE_RING_TOLERANCE = 12.0f;
// Drake abilities reach 60y; leave headroom for the boss drifting between ticks.
const float DRAKE_ATTACK_RANGE = 55.0f;
// Raid-wide drake healer counts. The rest of the flight is dps.
const uint8 DRAKE_HEALERS_25MAN = 5;
const uint8 DRAKE_HEALERS_10MAN = 2;
// Combo points needed before a healer dumps Life Burst instead of stacking another Revivify.
const uint8 DRAKE_LIFE_BURST_COMBO = 5;
// Surge of Power repeats every 7s and the boss leaves its victim list standing until the next one,
// so the dodge has to time itself out or a victim would strafe non-stop and never attack again.
const uint32 DRAKE_SURGE_DODGE_COOLDOWN_MS = 5000;

// Static Field (57430) drops a stationary NPC_STATIC_FIELD that pulses for its whole 20s life, and
// the boss lands a fresh one every 12s. Bots break off at the danger radius and only stop running
// once they are outside the safe radius - a smaller gap has them re-triggering the instant they
// arrive, which reads as a stutter and leaves them in the damage.
const float STATIC_FIELD_DANGER_RADIUS = 20.0f;
const float STATIC_FIELD_SAFE_RADIUS = 32.0f;
const float STATIC_FIELD_SEARCH_RADIUS = 100.0f;

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

// Live Static Fields within STATIC_FIELD_SEARCH_RADIUS of the bot's drake.
void GetNearbyStaticFields(Unit* drake, std::vector<Unit*>& fields);

// True when (x, y) is at least safeRadius from every one of them.
bool IsClearOfStaticFields(float x, float y, std::vector<Unit*> const& fields, float safeRadius);

// True while a Static Field is close enough to the bot's drake to be worth breaking formation over.
bool IsStaticFieldNear(Player* bot);

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

class EoEFlyDrakeAction : public MovementAction
{
public:
    EoEFlyDrakeAction(PlayerbotAI* ai) : MovementAction(ai, "eoe fly drake") {}

    bool Execute(Event event) override;
    bool isPossible() override;
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

private:
    // Revivify's combo point lands on whoever was healed, and the drake only ever holds combo points
    // for one unit at a time - hopping to the next most-injured drake every tick reset the count to
    // one and Life Burst was never reachable. Hold a target until it is topped off or gone.
    ObjectGuid healTargetGuid;
};

// P3 drake avoidance: fly clear of a Static Field hazard. Not a MovementAction so the
// phase-3 movement suppression doesn't block it; it drives the vehicle directly.
class AvoidStaticFieldAction : public Action
{
public:
    AvoidStaticFieldAction(PlayerbotAI* botAI) : Action(botAI, "avoid static field") {}

    bool Execute(Event event) override;
    bool isPossible() override;

private:
    // Where this action sent the drake. Checking the MotionMaster alone can't tell our escape from
    // the dps range-close, so a drake mid-approach used to sit out the whole flee.
    bool fleeing = false;
    float fleeX = 0.0f;
    float fleeY = 0.0f;
};

// P3 drake avoidance: react to the Surge of Power fixate with Flame Shield + a hard peel.
class DrakeDodgeSurgeAction : public Action
{
public:
    DrakeDodgeSurgeAction(PlayerbotAI* botAI) : Action(botAI, "drake dodge surge") {}

    bool Execute(Event event) override;
    bool isPossible() override;

private:
    // When this bot last shielded and peeled. See DRAKE_SURGE_DODGE_COOLDOWN_MS.
    uint32 lastDodgeAtMs = 0;
};

#endif
