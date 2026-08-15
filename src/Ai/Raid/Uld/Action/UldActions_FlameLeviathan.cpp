#include "UldActions_FlameLeviathan.h"
#include "UldActions_Shared.h"

#include <cmath>
#include <unordered_map>
#include <vector>

#include "AiObjectContext.h"
#include "DBCEnums.h"
#include "GameObject.h"
#include "Group.h"
#include "LastMovementValue.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Position.h"
#include "RaidBossHelpers.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "UldBossHelper.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "Unit.h"
#include "Vehicle.h"

namespace
{
// Kite sense per instance, so every vehicle that takes Pursued runs the ring the same way round.
// Latched on first use and never reversed: turning around runs straight back into the pursuer, and
// a direction that flips on a distance test is itself the oscillation.
thread_local std::unordered_map<uint32 /*instanceId*/, int8> flKiteDirection;

// Spells aimed at the vehicle itself (Tar, Steam Rush, the speed buffs, Shield Generator) cannot go
// through CanCastVehicleSpell: a self-cast comes back SPELL_FAILED_BAD_TARGETS, which that helper
// does not tolerate. Gate on cooldown and power instead, and never pass another unit as the target
// or CastVehicleSpell turns the vehicle to face it first - which would aim Steam Rush at the boss
// and drop the tar pool on the wrong side of the chopper.
bool CastVehicleSelfSpell(PlayerbotAI* botAI, Unit* vehicleBase, uint32 spellId, uint32 cost, uint32 cooldownMs)
{
    if (!vehicleBase || vehicleBase->HasSpellCooldown(spellId))
        return false;

    if (vehicleBase->GetPower(POWER_ENERGY) < cost)
        return false;

    if (!botAI->CastVehicleSpell(spellId, vehicleBase))
        return false;

    vehicleBase->AddSpellCooldown(spellId, 0, cooldownMs);
    return true;
}
}  // namespace

bool FlameLeviathanVehicleAction::CastVehicle(uint32 spellId, Unit* target, uint32 cooldownMs)
{
    if (!target || !vehicleBase_)
        return false;

    if (!botAI->CanCastVehicleSpell(spellId, target) || !botAI->CastVehicleSpell(spellId, target))
        return false;

    vehicleBase_->AddSpellCooldown(spellId, 0, cooldownMs);
    return true;
}

// Only lifts near the arena are worth a rocket: the crate drops where the lift died, and one shot
// down across the zone leaves pyrite nobody will ever drive to.
Unit* FlameLeviathanVehicleAction::FindMechanolift()
{
    GuidVector targets = AI_VALUE(GuidVector, "possible targets");
    for (auto const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || unit->GetEntry() != NPC_FL_MECHANOLIFT)
            continue;

        if (!FlameLeviathanInArena(unit->GetPosition(), ULDUAR_FL_CRATE_DETOUR_RADIUS))
            continue;

        return unit;
    }

    return nullptr;
}

bool FlameLeviathanVehicleAction::Execute(Event /*event*/)
{
    vehicleBase_ = bot->GetVehicleBase();
    if (!vehicleBase_ || !bot->GetVehicle())
        return false;

    Unit* boss = FlameLeviathanBoss(botAI);

    // Threat on this fight belongs to the vehicle creature rather than the bot, so "attackers" is
    // only good for adds - the boss is resolved by entry instead.
    Unit* add = nullptr;
    GuidVector attackers = context->GetValue<GuidVector>("attackers")->Get();
    for (auto const& guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit)
            continue;

        uint32 const entry = unit->GetEntry();
        if (entry == NPC_FL_TURRET || entry == NPC_FL_DEFENSE_TURRET || entry == NPC_FLAME_LEVIATHAN)
            continue;

        if (!add || bot->GetExactDist(add) > bot->GetExactDist(unit))
            add = unit;
    }

    Unit* target = boss ? boss : add;

    switch (vehicleBase_->GetEntry())
    {
        case NPC_SALVAGED_DEMOLISHER:
            return DemolisherAction(target);
        case NPC_SALVAGED_DEMOLISHER_TURRET:
            return DemolisherTurretAction(target);
        case NPC_SALVAGED_SIEGE_ENGINE:
            return SiegeEngineAction(target);
        case NPC_SALVAGED_SIEGE_ENGINE_TURRET:
            return SiegeEngineTurretAction(target);
        case NPC_VEHICLE_CHOPPER:
            return ChopperAction(target);
        default:
            break;
    }

    return false;
}

bool FlameLeviathanVehicleAction::DemolisherAction(Unit* target)
{
    if (!target)
        return false;

    // Our own barrel stack, not the raid's: every demolisher carries its own Blue Pyrite aura, and
    // reading the pooled one would let one bot coast on another's refreshes.
    Aura* own = target->GetAura(SPELL_FL_BLUE_PYRITE_DOT, vehicleBase_->GetGUID());
    bool const needBarrel = !own || own->GetDuration() <= 5000 || own->GetStackAmount() < 10;

    // The demolisher does not regenerate, so a full tank is 20 barrels. Below the reserve it drops
    // to free boulders and keeps enough pyrite for the gunner's Increased Speed when Pursued lands.
    if (needBarrel && vehicleBase_->GetPower(POWER_ENERGY) >= ULDUAR_FL_PYRITE_RESERVE)
        if (CastVehicle(SPELL_FL_HURL_PYRITE_BARREL, target))
            return true;

    return CastVehicle(SPELL_FL_HURL_BOULDER, target);
}

bool FlameLeviathanVehicleAction::DemolisherTurretAction(Unit* target)
{
    Unit* demolisher = FlameLeviathanRiddenVehicle(bot);

    if (FlameLeviathanIsPursued(bot) && demolisher && !demolisher->HasAura(SPELL_FL_INCREASED_SPEED))
    {
        if (CastVehicleSelfSpell(botAI, vehicleBase_, SPELL_FL_INCREASED_SPEED, ULDUAR_FL_INCREASED_SPEED_COST, 1000))
        {
            // CastVehicleSpell reports success even when CheckCast rejected, so the aura is the
            // only honest confirmation. Instants resolve inline, so it is already there or it failed.
            if (demolisher->HasAura(SPELL_FL_INCREASED_SPEED))
                return true;
        }
    }

    // Crates are the demolisher's only refill. Above the ceiling the +25 would be partly thrown away.
    if (demolisher && demolisher->GetPower(POWER_ENERGY) <= ULDUAR_FL_CRATE_GRAB_CEILING)
    {
        GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
        for (auto const& guid : npcs)
        {
            Unit* crate = botAI->GetUnit(guid);
            if (!crate || crate->GetEntry() != NPC_FL_PYRITE_CONTAINER)
                continue;

            if (crate->GetDistance(bot) >= 49.0f)
                continue;

            if (CastVehicle(SPELL_FL_GRAB_CRATE, crate))
                return true;
        }
    }

    if (Unit* lift = FindMechanolift())
        if (CastVehicle(SPELL_FL_ANTI_AIR_ROCKET, lift, 250))
            return true;

    return CastVehicle(SPELL_FL_MORTAR, target, 1000);
}

bool FlameLeviathanVehicleAction::SiegeEngineAction(Unit* target)
{
    if (!target)
        return false;

    // Ram is an 18 yd frontal cone, so standing off means the energy buys nothing.
    if (!vehicleBase_->IsWithinCombatRange(target, ULDUAR_FL_RAM_CONE_RADIUS))
        return false;

    // Earmark what this vehicle still owes: the interrupt duty travels with its fuel, and a pursued
    // driver needs Steam Rush more than it needs a Ram.
    uint32 needed = ULDUAR_FL_RAM_COST;
    if (FlameLeviathanIsVentInterrupter(botAI, bot))
        needed += ULDUAR_FL_ELECTROSHOCK_COST;
    if (FlameLeviathanIsPursued(bot))
        needed += ULDUAR_FL_STEAM_RUSH_COST;

    if (vehicleBase_->GetPower(POWER_ENERGY) < needed)
        return false;

    return CastVehicle(SPELL_FL_RAM, target);
}

bool FlameLeviathanVehicleAction::SiegeEngineTurretAction(Unit* target)
{
    if (FlameLeviathanIsPursued(bot))
    {
        Unit* siege = FlameLeviathanRiddenVehicle(bot);
        if (siege && !siege->HasAura(SPELL_FL_SHIELD_GENERATOR))
            if (CastVehicleSelfSpell(botAI, vehicleBase_, SPELL_FL_SHIELD_GENERATOR, 0, 60000))
                if (siege->HasAura(SPELL_FL_SHIELD_GENERATOR))
                    return true;
    }

    if (Unit* lift = FindMechanolift())
        if (CastVehicle(SPELL_FL_ANTI_AIR_ROCKET_SIEGE, lift, 250))
            return true;

    if (!target || vehicleBase_->GetPower(POWER_ENERGY) < ULDUAR_FL_FIRE_CANNON_COST)
        return false;

    return CastVehicle(SPELL_FL_FIRE_CANNON, target);
}

bool FlameLeviathanVehicleAction::ChopperAction(Unit* target)
{
    // Tar spawns its pool 9 yd behind the chopper, so it only lands in his path while he is behind
    // us - true whenever this chopper is pursued or is running the lead.
    if (target && !vehicleBase_->HasInArc(M_PI / 2.0f, target))
        if (CastVehicleSelfSpell(botAI, vehicleBase_, SPELL_FL_TAR, 0, 15000))
            return true;

    if (!target || vehicleBase_->GetPower(POWER_ENERGY) < ULDUAR_FL_SONIC_HORN_COST)
        return false;

    return CastVehicle(SPELL_FL_SONIC_HORN, target);
}

bool FlameLeviathanInterruptVentsAction::Execute(Event /*event*/)
{
    Unit* vehicleBase = bot->GetVehicleBase();
    if (!vehicleBase || vehicleBase->GetEntry() != NPC_SALVAGED_SIEGE_ENGINE)
        return false;

    Unit* boss = FlameLeviathanBoss(botAI);
    if (!boss || !FlameLeviathanIsVentChanneling(boss))
        return false;

    if (!botAI->CanCastVehicleSpell(SPELL_FL_ELECTROSHOCK, boss))
        return false;

    if (!botAI->CastVehicleSpell(SPELL_FL_ELECTROSHOCK, boss))
        return false;

    vehicleBase->AddSpellCooldown(SPELL_FL_ELECTROSHOCK, 0, 10000);
    return true;
}

bool FlameLeviathanDriveAction::Execute(Event /*event*/)
{
    vehicleBase_ = bot->GetVehicleBase();
    if (!vehicleBase_ || !FlameLeviathanIsDriver(bot))
        return false;

    if (!FlameLeviathanEngaged(botAI))
        return false;

    Unit* boss = FlameLeviathanBoss(botAI);
    if (!boss)
        return false;

    if (FlameLeviathanIsPursued(bot))
        return Kite(boss);

    ResetKite();

    if (uint32 towerMask = FlameLeviathanActiveTowerMask(botAI))
        if (Unit* hazard =
                GetFlameLeviathanNearestTowerHazard(botAI, vehicleBase_, towerMask, ULDUAR_FL_TOWER_HAZARD_RADIUS))
            return ClearHazard(hazard);

    if (vehicleBase_->GetEntry() == NPC_SALVAGED_DEMOLISHER &&
        vehicleBase_->GetPower(POWER_ENERGY) < ULDUAR_FL_PYRITE_RESERVE)
        if (DetourToCrate(boss))
            return true;

    return HoldStation(boss);
}

void FlameLeviathanDriveAction::ResetKite()
{
    // Only the per-bot node index is cleared. The instance direction stays latched for the pull, so
    // a bot leaving the kite cannot flip the sense out from under one still running it.
    kiteIdx_ = -1;
}

Unit* FlameLeviathanDriveAction::NearestCrate(float radius)
{
    Unit* nearest = nullptr;
    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
    for (auto const& guid : npcs)
    {
        Unit* crate = botAI->GetUnit(guid);
        if (!crate || crate->GetEntry() != NPC_FL_PYRITE_CONTAINER)
            continue;

        if (vehicleBase_->GetExactDist2d(crate) > radius)
            continue;

        if (!nearest || vehicleBase_->GetExactDist2d(crate) < vehicleBase_->GetExactDist2d(nearest))
            nearest = crate;
    }

    return nearest;
}

bool FlameLeviathanDriveAction::DetourToCrate(Unit* /*boss*/)
{
    Unit* crate = NearestCrate(ULDUAR_FL_CRATE_DETOUR_RADIUS);
    if (!crate)
        return false;

    DriveTo(crate->GetPosition(), crate, false);
    return true;
}

bool FlameLeviathanDriveAction::ClearHazard(Unit* hazard)
{
    float const angle = hazard->GetAngle(vehicleBase_);
    float const fleeDist = ULDUAR_FL_TOWER_HAZARD_RADIUS - vehicleBase_->GetExactDist2d(hazard) + 5.0f;
    Position const goal(vehicleBase_->GetPositionX() + std::cos(angle) * fleeDist,
                        vehicleBase_->GetPositionY() + std::sin(angle) * fleeDist, vehicleBase_->GetPositionZ());

    DriveTo(goal, nullptr, false, MovementPriority::MOVEMENT_FORCED);
    return true;
}

bool FlameLeviathanDriveAction::HoldStation(Unit* boss)
{
    float standDist = ULDUAR_FL_SIEGE_STAND_DIST;
    switch (vehicleBase_->GetEntry())
    {
        case NPC_VEHICLE_CHOPPER:
            // The lead chopper runs ahead of him instead, back turned, so its tar pool lands in his path.
            if (FlameLeviathanIsTarLead(botAI, bot))
                return DriveTo(FlameLeviathanLeadPoint(boss), boss, true);
            standDist = ULDUAR_FL_CHOPPER_STAND_DIST;
            break;
        case NPC_SALVAGED_DEMOLISHER:
            // Demolishers hurl from 10-70 yd and never close, which also keeps them off Battering Ram.
            standDist = ULDUAR_FL_DEMOLISHER_BAND;
            break;
        default:
            break;
    }

    return DriveTo(FlameLeviathanRearPoint(boss, standDist), boss, false);
}

bool FlameLeviathanDriveAction::Kite(Unit* boss)
{
    std::vector<Position> const& ring = FlameLeviathanKiteRing();
    int32 const count = static_cast<int32>(ring.size());
    if (count < 2)
        return false;

    auto blocked = [boss](Position const& node)
    { return node.GetExactDist2d(boss->GetPositionX(), boss->GetPositionY()) < ULDUAR_FL_KITE_BOSS_CLEARANCE; };

    auto nearestNode = [this, &ring, count]
    {
        int32 best = 0;
        for (int32 i = 1; i < count; ++i)
            if (vehicleBase_->GetExactDist2d(ring[i]) < vehicleBase_->GetExactDist2d(ring[best]))
                best = i;
        return best;
    };

    uint32 const instanceId = bot->GetInstanceId();
    auto dirIt = flKiteDirection.find(instanceId);
    if (dirIt == flKiteDirection.end())
    {
        int32 const here = nearestNode();
        float const ahead = ring[(here + 1) % count].GetExactDist2d(boss->GetPositionX(), boss->GetPositionY());
        float const behind =
            ring[(here - 1 + count) % count].GetExactDist2d(boss->GetPositionX(), boss->GetPositionY());
        dirIt = flKiteDirection.emplace(instanceId, ahead >= behind ? int8(1) : int8(-1)).first;
    }
    int32 const dir = dirIt->second;

    if (kiteIdx_ < 0)
    {
        kiteIdx_ = nearestNode();
        for (int32 i = 0; i < count && blocked(ring[kiteIdx_]); ++i)
            kiteIdx_ = (kiteIdx_ + dir + count) % count;
    }
    else if (vehicleBase_->GetExactDist2d(ring[kiteIdx_]) <= ULDUAR_FL_KITE_ADVANCE_DIST)
    {
        // Advance on approach, never on arrival: waiting until the vehicle reaches the node drives
        // it into the node, and in a corner that is exactly where the boss cuts the diagonal.
        kiteIdx_ = (kiteIdx_ + dir + count) % count;
        if (blocked(ring[kiteIdx_]))
            kiteIdx_ = (kiteIdx_ + dir + count) % count;
    }

    DriveTo(ring[kiteIdx_], nullptr, false);

    // Escape buttons, spent only once already running away from him.
    if (!vehicleBase_->HasInArc(M_PI / 2.0f, boss))
    {
        switch (vehicleBase_->GetEntry())
        {
            case NPC_SALVAGED_SIEGE_ENGINE:
                // Steam Rush charges along our own facing, so with him in front it would dash into him.
                CastVehicleSelfSpell(botAI, vehicleBase_, SPELL_FL_STEAM_RUSH, ULDUAR_FL_STEAM_RUSH_COST, 15000);
                break;
            case NPC_VEHICLE_CHOPPER:
                if (!vehicleBase_->HasAura(SPELL_FL_SPEED_BOOST))
                    CastVehicleSelfSpell(botAI, vehicleBase_, SPELL_FL_SPEED_BOOST, ULDUAR_FL_SPEED_BOOST_COST, 1000);
                break;
            default:
                break;
        }
    }

    return true;
}

bool FlameLeviathanDriveAction::DriveTo(Position const& goal, Unit* faceTarget, bool faceAway, MovementPriority priority)
{
    if (vehicleBase_->GetExactDist2d(goal) <= ULDUAR_FL_ARRIVE_TOLERANCE)
    {
        if (!parked_)
        {
            vehicleBase_->StopMoving();
            parked_ = true;
        }

        // A moving vehicle can hold neither a facing nor a cast, and CastVehicleSpell otherwise
        // burns a tick turning before every shot. Park facing him and the casts go out immediately.
        // Only correct a facing that has actually drifted: SetFacingTo launches a spline, and one
        // re-issued every tick makes the vehicle jitter and read as still moving.
        if (faceTarget)
        {
            float const wanted =
                faceAway ? faceTarget->GetAngle(vehicleBase_) : vehicleBase_->GetAngle(faceTarget);
            // NormalizeOrientation lands in [0, 2PI), so fold the far half back into [0, PI].
            float error = Position::NormalizeOrientation(wanted - vehicleBase_->GetOrientation());
            if (error > float(M_PI))
                error = 2.0f * float(M_PI) - error;

            if (error > ULDUAR_FL_FACING_TOLERANCE)
                vehicleBase_->SetFacingTo(wanted);
        }

        // Yield once parked, so the rotation runs and a move that silently failed shows up as a
        // vehicle standing still rather than as this action quietly owning every tick.
        return false;
    }

    // Re-stamping the same destination restarts the spline, so the vehicle would crawl and never
    // arrive. Only re-issue on real drift, or when the previous move has stopped.
    if (hasIssued_ && !parked_ && issued_.GetExactDist2d(goal) < ULDUAR_FL_REPOSITION_EPSILON &&
        vehicleBase_->isMoving())
        return true;

    if (!MoveTo(vehicleBase_->GetMapId(), goal.GetPositionX(), goal.GetPositionY(), goal.GetPositionZ(), false, false,
                false, false, priority))
        return false;

    issued_ = goal;
    hasIssued_ = true;
    parked_ = false;
    return true;
}

bool FlameLeviathanEnterVehicleAction::Execute(Event /*event*/)
{
    // do not switch vehicles yet
    if (bot->GetVehicle())
        return false;
    Unit* vehicleToEnter = nullptr;
    GuidVector npcs = AI_VALUE(GuidVector, "nearest vehicles far");
    for (GuidVector::iterator i = npcs.begin(); i != npcs.end(); i++)
    {
        Unit* vehicleBase = botAI->GetUnit(*i);
        if (!vehicleBase)
            continue;

        if (vehicleBase->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            continue;

        if (!ShouldEnter(vehicleBase))
            continue;

        if (!vehicleToEnter || bot->GetExactDist(vehicleToEnter) > bot->GetExactDist(vehicleBase))
            vehicleToEnter = vehicleBase;
    }

    if (!vehicleToEnter)
        return false;

    if (EnterVehicle(vehicleToEnter, true))
        return true;

    return false;
}

bool FlameLeviathanEnterVehicleAction::EnterVehicle(Unit* vehicleBase, bool moveIfFar)
{
    float dist = bot->GetDistance(vehicleBase);

    if (dist > INTERACTION_DISTANCE && !moveIfFar)
        return false;

    if (dist > INTERACTION_DISTANCE)
        return MoveTo(vehicleBase);

    botAI->RemoveShapeshift();
    // Use HandleSpellClick instead of Unit::EnterVehicle to handle special vehicle script (ulduar)
    vehicleBase->HandleSpellClick(bot);

    if (!bot->IsOnVehicle(vehicleBase))
        return false;

    // dismount because bots can enter vehicle on mount
    WorldPacket emptyPacket;
    bot->GetSession()->HandleCancelMountAuraOpcode(emptyPacket);
    return true;
}

bool FlameLeviathanEnterVehicleAction::ShouldEnter(Unit* target)
{
    Vehicle* vehicleKit = target->GetVehicleKit();
    if (!vehicleKit)
        return false;

    bool isMelee = botAI->IsMelee(bot);
    bool allMain = AllMainVehiclesOnUse();
    bool inUse = vehicleKit->IsVehicleInUse();
    int32 entry = target->GetEntry();
    if (entry != NPC_SALVAGED_DEMOLISHER && entry != NPC_SALVAGED_SIEGE_ENGINE && entry != NPC_VEHICLE_CHOPPER)
        return false;
    // two phase enter (make all main vehicles in use -> next player enter)
    if (!allMain)
    {
        if (inUse)
            return false;
        if (entry != NPC_SALVAGED_DEMOLISHER && entry != NPC_SALVAGED_SIEGE_ENGINE)
            return false;
        if (entry == NPC_SALVAGED_DEMOLISHER && isMelee)
            return false;
        if (entry == NPC_SALVAGED_SIEGE_ENGINE && !isMelee)
            return false;
        return true;
    }

    if (!vehicleKit->GetAvailableSeatCount())
        return false;

    // do not enter useless seat
    if (entry == NPC_SALVAGED_SIEGE_ENGINE)
    {
        Unit* turret = vehicleKit->GetPassenger(7);
        if (!turret)
            return false;
        Vehicle* turretVehicle = turret->GetVehicleKit();
        if (!turretVehicle)
            return false;
        if (turretVehicle->IsVehicleInUse())
            return false;
        return true;
    }

    if (entry == NPC_SALVAGED_DEMOLISHER)
    {
        if (vehicleKit->GetPassenger(0))
        {
            Unit* target2 = vehicleKit->GetPassenger(1);
            if (!target2)
                return false;
            Vehicle* vehicle2 = target2->GetVehicleKit();
            if (!vehicle2)
                return false;
            if (vehicle2->GetPassenger(0))
                return false;
        }
        return true;
    }

    if (entry == NPC_VEHICLE_CHOPPER && vehicleKit->GetAvailableSeatCount() <= 1)
        return false;

    return true;
}

bool FlameLeviathanEnterVehicleAction::AllMainVehiclesOnUse()
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;
    int demolisher = 0;
    int siege = 0;
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* player = gref->GetSource();
        if (!player)
            continue;
        Unit* vehicleBase = player->GetVehicleBase();
        if (!vehicleBase)
            continue;
        if (vehicleBase->GetEntry() == NPC_SALVAGED_DEMOLISHER)
            ++demolisher;
        else if (vehicleBase->GetEntry() == NPC_SALVAGED_SIEGE_ENGINE)
            ++siege;
    }
    Difficulty diff = bot->GetRaidDifficulty();
    int maxC = (diff == RAID_DIFFICULTY_10MAN_NORMAL || diff == RAID_DIFFICULTY_10MAN_HEROIC) ? 2 : 5;
    return demolisher >= maxC && siege >= maxC;
}
