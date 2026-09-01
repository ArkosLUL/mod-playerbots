#ifndef PLAYERBOTS_ULDACTIONS_FLAMELEVIATHAN_H
#define PLAYERBOTS_ULDACTIONS_FLAMELEVIATHAN_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Position.h"
#include "UldTriggers.h"
#include "Vehicle.h"

//
//  Flame Leviathan
//

// Every vehicle spell on this fight is instant, so casting never competes with driving. This action
// owns the casts only; all movement belongs to FlameLeviathanDriveAction.
class FlameLeviathanVehicleAction : public Action
{
public:
    FlameLeviathanVehicleAction(PlayerbotAI* botAI) : Action(botAI, "flame leviathan vehicle") {}
    bool Execute(Event event) override;

protected:
    bool DemolisherAction(Unit* target);
    bool DemolisherTurretAction(Unit* target);
    bool SiegeEngineAction(Unit* target);
    bool SiegeEngineTurretAction(Unit* target);
    bool ChopperAction(Unit* target);

    // Casts and records the cooldown the core will not apply itself: Spell::SendSpellCooldown
    // returns early for creature casters, so without this the action re-fires every tick.
    bool CastVehicle(uint32 spellId, Unit* target, uint32 cooldownMs = 1000);

    Unit* FindMechanolift();

    Unit* vehicleBase_ = nullptr;
};

class FlameLeviathanEnterVehicleAction : public MovementAction
{
public:
    FlameLeviathanEnterVehicleAction(PlayerbotAI* botAI) : MovementAction(botAI, "flame leviathan enter vehicle") {}
    bool Execute(Event event) override;

protected:
    bool EnterVehicle(Unit* vehicleBase, bool moveIfFar);
    bool ShouldEnter(Unit* vehicleBase);
    bool AllMainVehiclesOnUse();
};

// Electroshock. Flame Vents is a 10s channel and the boss script maps Electroshock to
// InterruptNonMeleeSpells, so this genuinely stops it. One siege engine fires per channel.
class FlameLeviathanInterruptVentsAction : public Action
{
public:
    FlameLeviathanInterruptVentsAction(PlayerbotAI* botAI) : Action(botAI, "flame leviathan interrupt vents") {}
    bool Execute(Event event) override;
};

// The only thing that steers a vehicle on this encounter. Two actions sharing one MotionMaster
// bounce rather than compromise, so kiting, hazard clearance, pyrite detours, the tar lead run and
// holding station are all branches of this one action rather than competing nodes.
class FlameLeviathanDriveAction : public MovementAction
{
public:
    FlameLeviathanDriveAction(PlayerbotAI* botAI) : MovementAction(botAI, "flame leviathan drive") {}
    bool Execute(Event event) override;

protected:
    bool Kite(Unit* boss);
    bool ClearHazard(Unit* hazard);
    bool ClearBatteringRam(Unit* boss);
    bool DetourToCrate(Unit* boss);
    bool HoldStation(Unit* boss);

    // Reach-then-hold. Returns true while genuinely travelling and false once parked and facing, so
    // lower-priority nodes still run and a silently failing move shows up as a stationary vehicle.
    bool DriveTo(Position const& goal, Unit* faceTarget, bool faceAway,
                 MovementPriority priority = MovementPriority::MOVEMENT_COMBAT);

    Unit* NearestCrate(float radius);
    void ResetKite();

    Unit* vehicleBase_ = nullptr;

    Position issued_;
    bool hasIssued_ = false;
    bool parked_ = false;

    int32 kiteIdx_ = -1;
};

#endif
