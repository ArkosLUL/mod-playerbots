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

#include <optional>
#include <vector>

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

    // Reads our own Blue Pyrite on the target and matches landings to the barrels in flight.
    Aura* TrackBarrels(Unit* target);

    // Casts and records the cooldown the core will not apply itself: Spell::SendSpellCooldown
    // returns early for creature casters, so without this the action re-fires every tick.
    bool CastVehicle(uint32 spellId, Unit* target, uint32 cooldownMs = 1000);

    Unit* FindMechanolift();

    Unit* vehicleBase_ = nullptr;

    // Cast times of barrels not yet seen landing, and the last flight timed.
    std::vector<uint32> barrelsInFlight_;
    uint32 barrelLeadMs_ = 0;  // ULDUAR_FL_PYRITE_FLIGHT_MS until a landing has been timed
    ObjectGuid barrelTarget_;
    uint8 lastPyriteStacks_ = 0;
    int32 lastPyriteDurationMs_ = 0;
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
    // Names the branch it took, kite:detour when it steered round the Inferno trail.
    bool Kite(Unit* boss, char const*& branch);
    std::optional<Position> KiteAroundFire(Unit* boss, Position const& node);
    bool ClearHazard(Unit* hazard);
    bool ClearBatteringRam(Unit* boss);
    bool DetourToCrate(Unit* boss);
    bool HoldStation(Unit* boss);

    // The vent reserve's Steam Rush into Electroshock reach. True while it owns the tick, turning or
    // dashing.
    bool RushToVents(Unit* boss);

    // Reach-then-hold. Returns true while genuinely travelling and false once parked and facing, so
    // lower-priority nodes still run and a silently failing move shows up as a stationary vehicle.
    bool DriveTo(Position const& goal, Unit* faceTarget, bool faceAway,
                 MovementPriority priority = MovementPriority::MOVEMENT_COMBAT);

    // Same, facing a fixed point. A posted siege engine faces its corner, which is a place and not
    // a unit, and there is nothing standing there to aim at.
    bool DriveTo(Position const& goal, Position const& facePoint,
                 MovementPriority priority = MovementPriority::MOVEMENT_COMBAT);

    // The orientation, if any, is applied only once parked. Empty means "hold what you are on".
    bool DriveToImpl(Position const& goal, std::optional<float> facing, MovementPriority priority);

    Unit* NearestCrate(float radius);
    void ResetKite();

    Unit* vehicleBase_ = nullptr;

    Position issued_;
    bool hasIssued_ = false;
    bool parked_ = false;

    int32 kiteIdx_ = -1;
    int8 kiteDir_ = 0;  // picked at the start of each kite, then held
};

#endif
