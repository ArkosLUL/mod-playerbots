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

class FlameLeviathanVehicleAction : public Action
{
public:
    static constexpr char const* Name = "flame leviathan vehicle";

    FlameLeviathanVehicleAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;

protected:
    bool DemolisherAction(Unit* target);
    bool DemolisherTurretAction(Unit* target);
    bool SiegeEngineAction(Unit* target);
    bool SiegeEngineTurretAction(Unit* target);
    bool ChopperAction(Unit* target);

    Aura* TrackBarrels(Unit* target);

    bool CastVehicle(uint32 spellId, Unit* target, uint32 cooldownMs = 1000);

    Unit* FindMechanolift();
    void NoteLiftShot(Unit* lift);

    Unit* NearestFightingHostile();

private:
    Unit* _vehicleBase = nullptr;

    std::vector<uint32> _barrelsInFlight;
    uint32 _barrelLeadMs = 0;
    ObjectGuid _barrelTarget;
    uint8 _lastPyriteStacks = 0;
    int32 _lastPyriteDurationMs = 0;

    ObjectGuid _lastLiftNoted;
};

class FlameLeviathanEnterVehicleAction : public MovementAction
{
public:
    static constexpr char const* Name = "flame leviathan enter vehicle";

    FlameLeviathanEnterVehicleAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
};

class FlameLeviathanInterruptVentsAction : public Action
{
public:
    static constexpr char const* Name = "flame leviathan interrupt vents";

    FlameLeviathanInterruptVentsAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
};

class FlameLeviathanDriveAction : public MovementAction
{
public:
    static constexpr char const* Name = "flame leviathan drive";

    FlameLeviathanDriveAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;

protected:
    bool Kite(Unit* boss, char const*& branch);
    std::optional<Position> DetourAroundFire(Unit* boss, Position const& goal, bool mayClose);
    bool ClearHazard(Unit* hazard, Unit* keepInReachOf = nullptr);
    bool ClearBatteringRam(Unit* boss);
    bool DetourToCrate(Unit* boss);
    bool HoldStation(Unit* boss, char const*& branch);

    bool RushToVents(Unit* boss);

    bool DriveTo(Position const& goal, Unit* faceTarget, bool faceAway,
                 MovementPriority priority = MovementPriority::MOVEMENT_COMBAT);

    bool DriveTo(Position const& goal, Position const& facePoint,
                 MovementPriority priority = MovementPriority::MOVEMENT_COMBAT);

    bool DriveToImpl(Position const& goal, std::optional<float> facing, MovementPriority priority);

    Unit* NearestCrate(float radius);
    void ResetKite();

private:
    Unit* _vehicleBase = nullptr;

    Position _issued;
    bool _hasIssued = false;
    bool _parked = false;

    int32 _kiteIdx = -1;
    int8 _kiteDir = 0;

    std::optional<Position> _dodgeGoal;
};

#endif
