#include "UldActions_FlameLeviathan.h"
#include "UldActions_Shared.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "AiObjectContext.h"
#include "CharmInfo.h"
#include "DBCEnums.h"
#include "GameObject.h"
#include "Group.h"
#include "LastMovementValue.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Position.h"
#include "EncounterHelpers.h"
#include "RaidObs.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "UldEncounter_FlameLeviathan.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "Unit.h"
#include "Vehicle.h"

namespace
{
// Bearing offsets tried off a blocked direction, nearest first.
constexpr float HAZARD_FAN[] = {0.0f, 0.6f, -0.6f, 1.2f, -1.2f, 1.8f, -1.8f, 2.4f, -2.4f, 3.0f};

// Whether driving straight from `from` to `to` stays `margin` outside every hazard's reach. A leg that
// starts inside one still counts as clear while it leads out of it. A point is a leg with no length.
bool LegClearOfHazards(Unit* vehicle, Position const& from, Position const& to, std::vector<Unit*> const& hazards,
                       float margin)
{
    float const dx = to.GetPositionX() - from.GetPositionX();
    float const dy = to.GetPositionY() - from.GetPositionY();
    float const lengthSq = dx * dx + dy * dy;

    for (Unit* hazard : hazards)
    {
        float const safe = FlameLeviathanHazardReach(hazard, vehicle) + margin;
        float const start = hazard->GetExactDist2d(from.GetPositionX(), from.GetPositionY());
        if (start < safe)
        {
            if (hazard->GetExactDist2d(to.GetPositionX(), to.GetPositionY()) <= start)
                return false;
            continue;
        }

        float along = 0.0f;
        if (lengthSq > 0.0f)
            along = std::clamp(((hazard->GetPositionX() - from.GetPositionX()) * dx +
                                (hazard->GetPositionY() - from.GetPositionY()) * dy) / lengthSq,
                               0.0f, 1.0f);

        if (hazard->GetExactDist2d(from.GetPositionX() + along * dx, from.GetPositionY() + along * dy) < safe)
            return false;
    }

    return true;
}

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

char const* HazardBranch(Unit* hazard)
{
    switch (hazard->GetEntry())
    {
        case NPC_FL_HODIRS_FURY_TARGET:
            return "hazard:fury";
        case NPC_FL_MIMIRONS_INFERNO:
        case NPC_FL_MIMIRONS_INFERNO_TARGET:
            return "hazard:inferno";
        default:
            return "hazard:hammer";
    }
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

// A barrel lobs for 1-3 s, so the stack read off the aura lags every cast. A landing shows on our own
// aura as a stack gained or the duration jumping back up, and the last one timed is the lead a refresh
// is thrown with.
Aura* FlameLeviathanVehicleAction::TrackBarrels(Unit* target)
{
    Aura* own = target->GetAura(SPELL_FL_BLUE_PYRITE_DOT, vehicleBase_->GetGUID());
    uint8 const stacks = own ? own->GetStackAmount() : 0;
    int32 const duration = own ? own->GetDuration() : 0;
    uint32 const now = getMSTime();

    if (target->GetGUID() != barrelTarget_)
    {
        barrelTarget_ = target->GetGUID();
        barrelsInFlight_.clear();
    }
    else if (own && !barrelsInFlight_.empty() &&
             (stacks > lastPyriteStacks_ || duration > lastPyriteDurationMs_ + 250))
    {
        barrelLeadMs_ = std::clamp<uint32>(getMSTimeDiff(barrelsInFlight_.front(), now), 500,
                                           ULDUAR_FL_PYRITE_FLIGHT_TIMEOUT_MS);
        size_t const landed = std::min<size_t>(std::max(int(stacks) - int(lastPyriteStacks_), 1), barrelsInFlight_.size());
        barrelsInFlight_.erase(barrelsInFlight_.begin(), barrelsInFlight_.begin() + landed);
    }

    barrelsInFlight_.erase(std::remove_if(barrelsInFlight_.begin(), barrelsInFlight_.end(),
                                          [now](uint32 cast)
                                          { return getMSTimeDiff(cast, now) >= ULDUAR_FL_PYRITE_FLIGHT_TIMEOUT_MS; }),
                           barrelsInFlight_.end());

    lastPyriteStacks_ = stacks;
    lastPyriteDurationMs_ = duration;
    return own;
}

bool FlameLeviathanVehicleAction::DemolisherAction(Unit* target)
{
    if (!target)
        return false;

    // Ahead of the thaw, which returns early: a landing nobody reads is timed out as a lost barrel.
    Aura* own = TrackBarrels(target);

    // Thawing outranks damage: a frozen vehicle is a full minute of nothing, and Hurl Boulder is free.
    // It is aimed at the ally on purpose - the boulder's blast is enemy-only, and the Flames it
    // triggers are what strip the stun. TARGET_FLAG_DEST_LOCATION means the unit only supplies a
    // destination, so passing a friendly one never trips a target check.
    if (Unit* frozen = FlameLeviathanFrozenVehicle(bot, vehicleBase_, ULDUAR_FL_HURL_BOULDER_MIN_RANGE,
                                                   ULDUAR_FL_HURL_BOULDER_MAX_RANGE))
        if (CastVehicle(SPELL_FL_HURL_BOULDER, frozen))
            return true;

    // Our own barrel stack, not the raid's: every demolisher carries its own Blue Pyrite aura, and
    // reading the pooled one would let one bot coast on another's refreshes. Barrels go on him only,
    // since the tank never refills itself; adds and trash get boulders.
    char const* barrel = "not boss";
    bool wantBarrel = false;
    if (target->GetEntry() == NPC_FLAME_LEVIATHAN)
    {
        uint32 const energy = vehicleBase_->GetPower(POWER_ENERGY);
        uint32 const stacks = std::min<uint32>((own ? own->GetStackAmount() : 0) + barrelsInFlight_.size(),
                                               ULDUAR_FL_PYRITE_MAX_STACKS);
        uint32 const burst = energy >= (ULDUAR_FL_PYRITE_MAX_STACKS - stacks) * ULDUAR_FL_PYRITE_BARREL_COST +
                                           ULDUAR_FL_PYRITE_REFRESH_RESERVE
                                 ? ULDUAR_FL_PYRITE_MAX_STACKS
                                 : ULDUAR_FL_PYRITE_BURST_STACKS;

        uint32 const lead = barrelLeadMs_ ? barrelLeadMs_ : ULDUAR_FL_PYRITE_FLIGHT_MS;

        if (energy < ULDUAR_FL_PYRITE_BARREL_COST)
            barrel = "dry";
        else if (stacks < burst)
        {
            barrel = "burst";
            wantBarrel = true;
        }
        else if (own && barrelsInFlight_.empty() &&
                 own->GetDuration() <= int32(lead + ULDUAR_FL_PYRITE_REFRESH_SLACK_MS))
        {
            barrel = "refresh";
            wantBarrel = true;
        }
        else
            barrel = "hold";
    }

    // Our own cooldown and the GCD a boulder shares are waits, not refusals, so only a cast the core
    // turned down (range, facing, LOS) reads as "fail".
    SpellInfo const* barrelInfo = sSpellMgr->GetSpellInfo(SPELL_FL_HURL_PYRITE_BARREL);
    CharmInfo* charm = vehicleBase_->GetCharmInfo();
    bool const ready = barrelInfo && !vehicleBase_->HasSpellCooldown(SPELL_FL_HURL_PYRITE_BARREL) &&
                       !(charm && charm->GetGlobalCooldownMgr().HasGlobalCooldown(barrelInfo));

    bool thrown = false;
    if (wantBarrel && ready)
    {
        thrown = CastVehicle(SPELL_FL_HURL_PYRITE_BARREL, target);
        if (thrown)
            barrelsInFlight_.push_back(getMSTime());
        else
            barrel = "fail";
    }

    // Stacks only, never the duration: the duration ticks every pass and would emit a note a tick.
    // Read off the aura rather than inferred from tick damage, which is what made the first pass at
    // this report stacks dropping one at a time - they cannot; the aura refreshes or it falls off
    // whole, and partially resisted ticks were rounding into the wrong bucket.
    if (RaidObs::Active())
    {
        RaidObs::NoteDerived(bot, "fl.pyrite", std::to_string(own ? own->GetStackAmount() : 0));
        RaidObs::NoteDerived(bot, "fl.barrel", barrel);
    }

    if (thrown)
        return true;

    // Pyrite is this seat's job, so adds only get what the barrel does not want. Ram is the
    // exception: it is free, it knocks back, and it covers the 15 yd an add has to be inside to be
    // chewing on the hull - which is also the band Hurl Boulder's 10 yd floor cannot fire into.
    if (Unit* add = FlameLeviathanBestAdd(botAI, vehicleBase_, 0.0f, ULDUAR_FL_RAM_CONE_RADIUS, 0.0f))
        if (FlameLeviathanFaceForCone(vehicleBase_, add, ULDUAR_FL_RAM_CONE_HALF_ANGLE,
                                      ULDUAR_FL_RAM_CONE_RADIUS))
            if (CastVehicle(SPELL_FL_DEMOLISHER_RAM, add, 4000))
                return true;

    if (!wantBarrel)
        if (Unit* add = FlameLeviathanBestAdd(botAI, vehicleBase_, ULDUAR_FL_HURL_BOULDER_MIN_RANGE,
                                              ULDUAR_FL_HURL_BOULDER_MAX_RANGE, ULDUAR_FL_CANNON_SPLASH))
            if (CastVehicle(SPELL_FL_HURL_BOULDER, add))
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

            // No claim: spell_vehicle_grab_pyrite credits every hit until the crate despawns, and the
            // credit goes to the grabbing gunner's own demolisher, so two gunners both gain from it.
            if (crate->GetDistance(bot) >= ULDUAR_FL_CRATE_GRAB_RANGE)
                continue;

            if (CastVehicle(SPELL_FL_GRAB_CRATE, crate))
                return true;
        }
    }

    // Mortar carries the same thaw as the driver's boulder, via Flames 65044, and has no minimum
    // range - so the gunner covers the close-in case the boulder's 10 yd floor cannot.
    if (Unit* frozen = FlameLeviathanFrozenVehicle(bot, vehicleBase_, 0.0f, ULDUAR_FL_MORTAR_MAX_RANGE))
        if (CastVehicle(SPELL_FL_MORTAR, frozen, 1000))
            return true;

    if (Unit* lift = FindMechanolift())
        if (CastVehicle(SPELL_FL_ANTI_AIR_ROCKET, lift, 250))
            return true;

    // Mortar is free and already the gunner's every-tick cast, so pointing it at an add is a target
    // swap rather than new spend. It also has no minimum range, which covers the adds that walk in
    // under Fire Cannon's 10 yd floor.
    if (Unit* add = FlameLeviathanBestAdd(botAI, vehicleBase_, 0.0f, ULDUAR_FL_MORTAR_MAX_RANGE,
                                          ULDUAR_FL_MORTAR_SPLASH))
        if (CastVehicle(SPELL_FL_MORTAR, add, 1000))
            return true;

    return CastVehicle(SPELL_FL_MORTAR, target, 1000);
}

bool FlameLeviathanVehicleAction::SiegeEngineAction(Unit* target)
{
    // No early return on a null target: a posted engine is 90 yd from him and shoots adds, so it has
    // work to do whether or not he is alive and reachable.

    // Earmark what this vehicle still owes: the interrupt duty travels with its fuel, and a pursued
    // driver or the vent reserve needs Steam Rush more than it needs a Ram.
    uint32 needed = ULDUAR_FL_RAM_COST;
    if (FlameLeviathanIsVentInterrupter(botAI, bot))
        needed += ULDUAR_FL_ELECTROSHOCK_COST;
    if (FlameLeviathanIsPursued(bot) || FlameLeviathanIsVentReserve(bot))
        needed += ULDUAR_FL_STEAM_RUSH_COST;

    if (vehicleBase_->GetPower(POWER_ENERGY) < needed)
        return false;

    // An add in the cone outranks him: 190k against his 230M, and unlike him they accumulate. Picked
    // once rather than tried and fallen through, because falling back to him mid-turn would just
    // turn the engine round again and it would spend the fight pointed at neither.
    Unit* shot = FlameLeviathanBestAdd(botAI, vehicleBase_, 0.0f, ULDUAR_FL_RAM_CONE_RADIUS, 0.0f);
    if (!shot)
        shot = target;

    if (!shot)
        return false;

    // Range alone is not the test - a siege engine pointed 55 degrees off him is in range, passes
    // CastVehicleSpell's 120 degree turn gate, and lands nothing. Turning costs the tick, which is
    // cheaper than the 40 energy.
    //
    // Two engines never turn: a posted one owes its facing to its corner, and the vent reserve owes
    // its facing to the boss. Both fire only at whatever is already in front of them. HoldStation
    // makes the same two exceptions, so the drive and the shot cannot disagree.
    if (FlameLeviathanCornerPost(botAI, bot) >= 0 || FlameLeviathanIsVentReserve(bot))
    {
        if (!FlameLeviathanInCone(vehicleBase_, shot, ULDUAR_FL_RAM_CONE_HALF_ANGLE,
                                  ULDUAR_FL_RAM_CONE_RADIUS))
            return false;
    }
    else if (!FlameLeviathanFaceForCone(vehicleBase_, shot, ULDUAR_FL_RAM_CONE_HALF_ANGLE,
                                        ULDUAR_FL_RAM_CONE_RADIUS))
        return false;

    return CastVehicle(SPELL_FL_RAM, shot);
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

    if (vehicleBase_->GetPower(POWER_ENERGY) < ULDUAR_FL_FIRE_CANNON_COST)
        return false;

    // A posted engine's gun serves its corner first, so its hull is the first thing a fresh add has
    // threat on and the add stays on the engine that can knock it back.
    if (Unit* hull = FlameLeviathanRiddenVehicle(bot))
    {
        int8 const post = FlameLeviathanHullCornerPost(botAI, bot, hull);
        if (post >= 0)
        {
            Position const point = FlameLeviathanCornerPostPoint(static_cast<uint8>(post));
            if (Unit* add = FlameLeviathanBestAdd(botAI, vehicleBase_, ULDUAR_FL_FIRE_CANNON_MIN_RANGE,
                                                  ULDUAR_FL_FIRE_CANNON_MAX_RANGE, ULDUAR_FL_CANNON_SPLASH, &point,
                                                  ULDUAR_FL_CORNER_HOLD_RADIUS))
                if (CastVehicle(SPELL_FL_FIRE_CANNON, add))
                    return true;
        }
    }

    // Fire Cannon is the heaviest gun the raid owns and the widest add coverage it has - 76k in a
    // 20 yd sphere against a 190k lasher, and it reaches four fifths of the arena from station. The
    // boss carries 230M, so the damage this costs him is noise next to adds that never despawn.
    if (Unit* add = FlameLeviathanBestAdd(botAI, vehicleBase_, ULDUAR_FL_FIRE_CANNON_MIN_RANGE,
                                          ULDUAR_FL_FIRE_CANNON_MAX_RANGE, ULDUAR_FL_CANNON_SPLASH))
        if (CastVehicle(SPELL_FL_FIRE_CANNON, add))
            return true;

    if (!target)
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

    if (vehicleBase_->GetPower(POWER_ENERGY) < ULDUAR_FL_SONIC_HORN_COST)
        return false;

    // Everything but the lead helps clear adds. The lead is excluded outright rather than trusted to
    // come back: it has to keep its back to him for the tar to land in his path, and one turn toward
    // an add costs a pool. Same single pick as the siege engine, for the same reason - alternating
    // between an add and him would leave the chopper facing neither.
    Unit* shot = FlameLeviathanIsTarLead(botAI, bot)
                     ? nullptr
                     : FlameLeviathanBestAdd(botAI, vehicleBase_, 0.0f,
                                             ULDUAR_FL_SONIC_HORN_CONE_RADIUS, 0.0f);
    if (!shot)
        shot = target;

    // The narrowest cone on the fight at 50 degrees, so it needs the facing more than the others do.
    if (!shot || !FlameLeviathanFaceForCone(vehicleBase_, shot, ULDUAR_FL_SONIC_HORN_CONE_HALF_ANGLE,
                                            ULDUAR_FL_SONIC_HORN_CONE_RADIUS))
        return false;

    return CastVehicle(SPELL_FL_SONIC_HORN, shot);
}

bool FlameLeviathanInterruptVentsAction::Execute(Event /*event*/)
{
    Unit* vehicleBase = bot->GetVehicleBase();
    if (!vehicleBase || vehicleBase->GetEntry() != NPC_SALVAGED_SIEGE_ENGINE)
        return false;

    Unit* boss = FlameLeviathanBoss(botAI);
    if (!boss || !FlameLeviathanIsVentChanneling(boss))
        return false;

    // This node outranks the urgent drive, so it never parks a hull that has a blast or a hazard to
    // get out of.
    uint32 const towerMask = FlameLeviathanActiveTowerMask(botAI);
    bool const dodging = FlameLeviathanShouldClearBatteringRam(botAI, bot) ||
                         (towerMask && GetFlameLeviathanNearestTowerHazard(botAI, vehicleBase, towerMask));

    // A moving hull's spline overrides SetFacingTo, so the turn only takes once stopped: on 2026-09-17
    // the reserve drove past him at 22 yd, still 33-87 degrees off, and never fired.
    if (!vehicleBase->HasInArc(ULDUAR_FL_ELECTROSHOCK_CONE_HALF_ANGLE * 2.0f, boss))
    {
        if (dodging)
            return false;

        vehicleBase->StopMoving();
    }

    // Turning is progress, so this owns the tick either way: Electroshock's cone is 60 degrees and
    // CastVehicleSpell only turns for something outside 120, so nothing else will ever point the
    // vehicle at him and the shot would go out into empty air.
    if (!FlameLeviathanFaceForCone(vehicleBase, boss, ULDUAR_FL_ELECTROSHOCK_CONE_HALF_ANGLE,
                                   ULDUAR_FL_ELECTROSHOCK_CONE_RADIUS))
        return true;

    if (!botAI->CanCastVehicleSpell(SPELL_FL_ELECTROSHOCK, boss))
    {
        // A Ram or Steam Rush GCD (category 133) is a wait, not a refusal. Hold the aim through it, or
        // the drive turns the hull away again.
        SpellInfo const* shockInfo = sSpellMgr->GetSpellInfo(SPELL_FL_ELECTROSHOCK);
        CharmInfo* charm = vehicleBase->GetCharmInfo();
        if (!dodging && shockInfo && charm && charm->GetGlobalCooldownMgr().HasGlobalCooldown(shockInfo))
        {
            vehicleBase->StopMoving();
            return true;
        }

        return false;
    }

    if (!botAI->CastVehicleSpell(SPELL_FL_ELECTROSHOCK, boss))
        return false;

    // Take the channel off the queue whichever way the shot goes, so the rest of the line does not
    // empty into it behind us.
    FlameLeviathanClaimVentChannel(bot);

    // CastVehicleSpell reports success even when CheckCast rejected, so the channel stopping is the
    // only honest confirmation. Electroshock is instant and resolves inline, so by now it has either
    // interrupted him or it has not. Charging the full cooldown for a miss is what left every siege
    // engine firing 10 s out of step with a 20 s vent cycle.
    //
    // Note, not NoteDerived: every shot has to show up, and a run of hits carries the same value.
    if (FlameLeviathanIsVentChanneling(boss))
    {
        RaidObs::Note(bot, "fl.vent", "miss");
        vehicleBase->AddSpellCooldown(SPELL_FL_ELECTROSHOCK, 0, ULDUAR_FL_ELECTROSHOCK_RETRY_MS);
        return true;
    }

    RaidObs::Note(bot, "fl.vent", "hit");
    vehicleBase->AddSpellCooldown(SPELL_FL_ELECTROSHOCK, 0, ULDUAR_FL_ELECTROSHOCK_COOLDOWN_MS);
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

    // The branch that owned the tick, which is the only place it is known: fl.corner and fl.station
    // name an assignment, not whether the vehicle is driving it.
    auto const branch = [this](char const* name)
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "fl.drive", name);
    };

    // He chases his threat victim exactly like a Pursued vehicle whenever nobody holds the aura.
    bool const pursued = FlameLeviathanIsPursued(bot);
    if (pursued || FlameLeviathanIsRamTarget(bot))
    {
        char const* how = pursued ? "kite" : "kite:victim";
        bool const kiting = Kite(boss, how);
        branch(how);
        return kiting;
    }

    ResetKite();

    Unit* hazard = nullptr;
    if (uint32 towerMask = FlameLeviathanActiveTowerMask(botAI))
        hazard = GetFlameLeviathanNearestTowerHazard(botAI, vehicleBase_, towerMask);

    // A hazard already cleared reports false rather than owning the tick, so fall through to the
    // station instead of failing the whole action and handing the tick to the on-foot rotation.
    if (hazard && ClearHazard(hazard))
    {
        branch(HazardBranch(hazard));
        return true;
    }

    if (ClearBatteringRam(boss))
    {
        branch("ram");
        return true;
    }

    // Below the two dodges on purpose: a posted engine still has to get out of a tower blast and out
    // of Battering Ram, and the corner will still be there afterwards.
    if (int8 const corner = FlameLeviathanCornerPost(botAI, bot); corner >= 0)
    {
        uint8 const slot = static_cast<uint8>(corner);
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "fl.corner", std::to_string(static_cast<uint32>(slot)));
        branch("corner");

        // At 7 yd/s the commute from the boss takes 15-27 s against a first wave at 34 s. Steam Rush
        // charges along the facing, so only with the post dead ahead and more than a charge away.
        Position const post = FlameLeviathanCornerPostPoint(slot);
        if (vehicleBase_->GetExactDist2d(post) > ULDUAR_FL_STEAM_RUSH_DIST + ULDUAR_FL_ARRIVE_TOLERANCE &&
            vehicleBase_->HasInArc(float(M_PI) / 4.0f, &post))
            CastVehicleSelfSpell(botAI, vehicleBase_, SPELL_FL_STEAM_RUSH, ULDUAR_FL_STEAM_RUSH_COST, 15000);

        // COMBAT, not FORCED like the kite: a Fury dodge is FORCED, and an equal-priority move waits
        // out this one for up to MaxWaitForMove (5 s) of a 6.5 s fuse.
        return DriveTo(post, ULDUAR_FL_ARENA_CORNERS[slot]);
    }

    if (vehicleBase_->GetEntry() == NPC_SALVAGED_DEMOLISHER &&
        vehicleBase_->GetPower(POWER_ENERGY) < ULDUAR_FL_CRATE_DETOUR_ENERGY)
        if (DetourToCrate(boss))
        {
            branch("crate");
            return true;
        }

    branch("station");
    return HoldStation(boss);
}

void FlameLeviathanDriveAction::ResetKite()
{
    if (kiteIdx_ < 0)
        return;

    kiteIdx_ = -1;
    kiteDir_ = 0;

    // The last kite leg went out FORCED, and IsWaitingForLastMove holds an equal priority until that
    // leg's travel time runs out. Pursued is over, so nothing should wait on it - a Fury dodge least.
    AI_VALUE(LastMovement&, "last movement").priority = MovementPriority::MOVEMENT_NORMAL;
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

bool FlameLeviathanDriveAction::DetourToCrate(Unit* boss)
{
    Unit* crate = NearestCrate(ULDUAR_FL_CRATE_DETOUR_RADIUS);
    if (!crate)
        return false;

    // Already inside the gunner's grab.
    float const grab = ULDUAR_FL_CRATE_GRAB_RANGE - ULDUAR_FL_ARRIVE_TOLERANCE;
    float const dist = vehicleBase_->GetExactDist2d(crate);
    if (dist <= grab)
        return false;

    // Only as far as the gunner can grab from, on the hull's side of the crate. And only while that
    // still keeps the barrel in range: a starved demolisher sat past 70 yd for 22-47% of a pull, and
    // every second out there drops Blue Pyrite stacks the crate was meant to keep up.
    float const angle = crate->GetAngle(vehicleBase_);
    Position const goal(crate->GetPositionX() + std::cos(angle) * grab, crate->GetPositionY() + std::sin(angle) * grab,
                        crate->GetPositionZ());

    float const leash = boss->GetCombatReach() + FlameLeviathanDemolisherStandDist(boss) + ULDUAR_FL_ARRIVE_TOLERANCE;
    if (boss->GetExactDist2d(goal) > leash)
        return false;

    DriveTo(goal, boss, false);
    return true;
}

bool FlameLeviathanDriveAction::ClearHazard(Unit* hazard)
{
    // Radial is the first thing tried, for all three reticles. A Hodir's Fury only gets here once it
    // has stopped and stunned itself, so it is a static mark like the other two.
    float const angle = hazard->GetAngle(vehicleBase_);

    float const reach = FlameLeviathanHazardReach(hazard, vehicleBase_);
    float const step = reach + 2.0f * ULDUAR_FL_ARRIVE_TOLERANCE - vehicleBase_->GetExactDist2d(hazard);

    // Guard, not a normal path: the scan margin is narrower than this clearance, so anything close
    // enough to be handed here still has ground to make up. Never step backwards if that changes.
    if (step <= 0.0f)
        return false;

    auto const pointFor = [this](float bearing, float reach)
    {
        return Position(vehicleBase_->GetPositionX() + std::cos(bearing) * reach,
                        vehicleBase_->GetPositionY() + std::sin(bearing) * reach,
                        vehicleBase_->GetPositionZ());
    };

    // Mimiron's Inferno is not one circle. Its head walks a waypoint path dropping a fresh 9 yd
    // patch every 2s, each burning 30s, so there are about fifteen of them lying in a line - and
    // straight out from the nearest lands in the next one as often as it escapes. Fan off the radial
    // until somewhere is clear of all of them.
    std::vector<Unit*> hazards;
    GetFlameLeviathanTowerHazards(botAI, vehicleBase_, FlameLeviathanActiveTowerMask(botAI),
                                  ULDUAR_FL_TOWER_HAZARD_CLEAR_SCAN, hazards);

    for (float travel : {step, step + reach, step + 2.0f * reach})
    {
        for (float offset : HAZARD_FAN)
        {
            Position const goal = pointFor(angle + offset, travel);

            // The arena bounds are the kite ring's, so a dodge that leaves them is a dodge into a
            // wall - the spline stops short and the vehicle stays in the fire.
            if (!FlameLeviathanInArena(goal))
                continue;

            if (!LegClearOfHazards(vehicleBase_, goal, goal, hazards, ULDUAR_FL_ARRIVE_TOLERANCE))
                continue;

            DriveTo(goal, nullptr, false, MovementPriority::MOVEMENT_FORCED);
            return true;
        }
    }

    // Boxed in by the trail. Straight out from the nearest patch still beats standing in it.
    DriveTo(pointFor(angle, step), nullptr, false, MovementPriority::MOVEMENT_FORCED);
    return true;
}

bool FlameLeviathanDriveAction::ClearBatteringRam(Unit* boss)
{
    if (!FlameLeviathanShouldClearBatteringRam(botAI, bot))
        return false;

    // Away from the rammed vehicle, which is where the blast is centred - running from the boss
    // instead is what the old test did, and it left the fleet standing in the sphere it was trying
    // to leave. Keep the guns on him while backing out; only the direction of travel changes.
    Unit* centre = FlameLeviathanRamCentre(botAI, bot);
    if (!centre)
        return false;

    float const safeDist = ULDUAR_FL_BATTERING_RAM_RADIUS + vehicleBase_->GetObjectSize();

    // Twice the arrival deadband of overshoot, because DriveTo parks anywhere within one of it and a
    // single deadband of margin lets the vehicle stop back on the edge of the blast.
    float const angle = centre->GetAngle(vehicleBase_);
    float const step = safeDist - vehicleBase_->GetExactDist2d(centre) + 2.0f * ULDUAR_FL_ARRIVE_TOLERANCE;
    Position const goal(vehicleBase_->GetPositionX() + std::cos(angle) * step,
                        vehicleBase_->GetPositionY() + std::sin(angle) * step,
                        vehicleBase_->GetPositionZ());

    DriveTo(goal, boss, false, MovementPriority::MOVEMENT_FORCED);
    return true;
}

bool FlameLeviathanDriveAction::HoldStation(Unit* boss)
{
    float standDist = ULDUAR_FL_SIEGE_STAND_DIST;
    char const* how = "siege";
    switch (vehicleBase_->GetEntry())
    {
        case NPC_VEHICLE_CHOPPER:
            // The lead chopper runs ahead of him instead, back turned, so its tar pool lands in his path.
            if (FlameLeviathanIsTarLead(botAI, bot))
            {
                float const lead = FlameLeviathanTarLeadDistance(boss, FlameLeviathanPursuedVehicle(botAI, bot),
                                                                 vehicleBase_->GetObjectSize());
                // Zero means the chase is too tight to lead without parking in the blast. Hold the
                // ordinary chopper station until there is room again rather than trading a vehicle
                // for one more tar pool.
                if (lead > 0.0f)
                {
                    if (RaidObs::Active())
                        RaidObs::NoteDerived(bot, "fl.station", "tar-lead");
                    return DriveTo(FlameLeviathanLeadPoint(boss, lead), boss, true);
                }
            }
            standDist = ULDUAR_FL_CHOPPER_STAND_DIST;
            how = "chopper";
            break;
        case NPC_SALVAGED_DEMOLISHER:
            // Demolishers hurl from 10-70 yd and never close, which also keeps them off Battering
            // Ram. Clamped, because FlameLeviathanOffsetPoint measures outward from his combat reach:
            // 15 plus the 50 band plus the deadband DriveTo parks in already sits past 70, and a
            // barrel that will not cast drops the Blue Pyrite stack the raid does its damage with.
            standDist = FlameLeviathanDemolisherStandDist(boss);
            how = "demolisher";
            break;
        default:
            break;
    }

    bool const ventReserve = FlameLeviathanIsVentReserve(bot);
    if (ventReserve)
        how = "vent";

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "fl.station", how);

    if (ventReserve && RushToVents(boss))
        return true;

    float const offset =
        FlameLeviathanStationBearingOffset(bot, vehicleBase_, boss->GetCombatReach() + standDist);
    Position const goal = FlameLeviathanRearPoint(boss, standDist, offset);

    // Ram and Sonic Horn are cones, so a driver that engages an add has to turn - and the park block
    // below re-faces him every tick. Point at whatever the cast node is about to shoot instead, or
    // the two spend the fight undoing each other and the vehicle ends up aimed at neither. The bands
    // mirror the cast node exactly, tar lead included: it never shoots adds, so it never turns.
    //
    // The vent reserve is the other exception. Electroshock's cone is 25 yd and 60 degrees, so an
    // engine turned onto an add has to spend a tick turning back every time the channel starts.
    float coneRadius = 0.0f;
    if (!ventReserve)
    {
        switch (vehicleBase_->GetEntry())
        {
            case NPC_SALVAGED_SIEGE_ENGINE:
            case NPC_SALVAGED_DEMOLISHER:
                coneRadius = ULDUAR_FL_RAM_CONE_RADIUS;
                break;
            case NPC_VEHICLE_CHOPPER:
                if (!FlameLeviathanIsTarLead(botAI, bot))
                    coneRadius = ULDUAR_FL_SONIC_HORN_CONE_RADIUS;
                break;
            default:
                break;
        }
    }

    if (coneRadius > 0.0f)
        if (Unit* add = FlameLeviathanBestAdd(botAI, vehicleBase_, 0.0f, coneRadius, 0.0f))
            return DriveTo(goal, add, false);

    return DriveTo(goal, boss, false);
}

bool FlameLeviathanDriveAction::RushToVents(Unit* boss)
{
    // Electroshock shares Steam Rush's 2 s GCD, so dash 2-5 s ahead of a channel, or at once into one
    // already running.
    uint32 const toVent = FlameLeviathanMsToNextVent(bot, boss);
    if (toVent && (toVent < ULDUAR_FL_STEAM_RUSH_GCD_MS || toVent > ULDUAR_FL_VENT_RUSH_LEAD_MS))
        return false;

    // Measured from his edge, like the cone. Inside a full charge the dash would carry the hull through
    // him, and driving closes the rest.
    if (vehicleBase_->GetExactDist2d(boss) - boss->GetObjectSize() <= ULDUAR_FL_STEAM_RUSH_DIST)
        return false;

    if (vehicleBase_->HasSpellCooldown(SPELL_FL_STEAM_RUSH) ||
        vehicleBase_->GetPower(POWER_ENERGY) < ULDUAR_FL_STEAM_RUSH_COST + ULDUAR_FL_ELECTROSHOCK_COST)
        return false;

    SpellInfo const* rushInfo = sSpellMgr->GetSpellInfo(SPELL_FL_STEAM_RUSH);
    CharmInfo* charm = vehicleBase_->GetCharmInfo();
    if (!rushInfo || (charm && charm->GetGlobalCooldownMgr().HasGlobalCooldown(rushInfo)))
        return false;

    // The charge runs along our facing, and must not end inside the blast round whoever he is chasing.
    float const bearing = vehicleBase_->GetAngle(boss);
    Position const landing(vehicleBase_->GetPositionX() + std::cos(bearing) * ULDUAR_FL_STEAM_RUSH_DIST,
                           vehicleBase_->GetPositionY() + std::sin(bearing) * ULDUAR_FL_STEAM_RUSH_DIST,
                           vehicleBase_->GetPositionZ());
    if (Unit* centre = FlameLeviathanRamCentre(botAI, bot))
        if (centre->GetExactDist2d(landing) <= ULDUAR_FL_BATTERING_RAM_RADIUS + vehicleBase_->GetObjectSize())
            return false;

    // Stopped first: a moving hull's spline overrides the facing.
    if (!vehicleBase_->HasInArc(float(M_PI) / 4.0f, boss))
    {
        vehicleBase_->StopMoving();
        vehicleBase_->SetFacingToObject(boss);
        return true;
    }

    // CastVehicleSpell reports success even when CheckCast rejected, so the energy leaving is the
    // confirmation. The spell is instant and resolves inline.
    uint32 const before = vehicleBase_->GetPower(POWER_ENERGY);
    if (!CastVehicleSelfSpell(botAI, vehicleBase_, SPELL_FL_STEAM_RUSH, ULDUAR_FL_STEAM_RUSH_COST, 15000))
        return false;

    if (vehicleBase_->GetPower(POWER_ENERGY) + ULDUAR_FL_STEAM_RUSH_COST > before)
        return false;

    if (RaidObs::Active())
        RaidObs::Note(bot, "fl.rush", "vent");

    return true;
}

std::optional<Position> FlameLeviathanDriveAction::KiteAroundFire(Unit* boss, Position const& node)
{
    std::vector<Unit*> fires;
    GetFlameLeviathanTowerHazards(botAI, vehicleBase_, FL_TOWER_FLAMES, ULDUAR_FL_TOWER_HAZARD_CLEAR_SCAN, fires);
    if (fires.empty())
        return std::nullopt;

    Position const here = vehicleBase_->GetPosition();
    float const bearing = vehicleBase_->GetAngle(&node);
    auto const along = [&here](float angle, float dist)
    {
        return Position(here.GetPositionX() + std::cos(angle) * dist, here.GetPositionY() + std::sin(angle) * dist,
                        here.GetPositionZ());
    };

    // Only the next stretch: the leg is re-planned every tick, and fire further along may be gone or
    // passed by then.
    float const lookahead = std::min(vehicleBase_->GetExactDist2d(node), ULDUAR_FL_KITE_FIRE_LOOKAHEAD);
    if (LegClearOfHazards(vehicleBase_, here, along(bearing, lookahead), fires, ULDUAR_FL_TOWER_HAZARD_MARGIN))
        return std::nullopt;

    // Never toward him: a detour that closes on the pursuer trades a burn for Battering Ram.
    float const bossDist = vehicleBase_->GetExactDist2d(boss);
    for (float offset : HAZARD_FAN)
    {
        Position const detour = along(bearing + offset, ULDUAR_FL_KITE_DETOUR_STEP);
        if (!FlameLeviathanInArena(detour) || boss->GetExactDist2d(detour) < bossDist)
            continue;

        if (LegClearOfHazards(vehicleBase_, here, detour, fires, ULDUAR_FL_TOWER_HAZARD_MARGIN))
            return detour;
    }

    return std::nullopt;
}

bool FlameLeviathanDriveAction::Kite(Unit* boss, char const*& branch)
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

    if (kiteIdx_ < 0)
    {
        // Picked per kite, away from him, then held until it ends: turning round mid-kite runs straight
        // back into the pursuer. A sense latched for the whole pull instead sent a demolisher 134 yd out
        // up the wall into him on 2026-09-17.
        int32 const here = nearestNode();
        float const ahead = ring[(here + 1) % count].GetExactDist2d(boss->GetPositionX(), boss->GetPositionY());
        float const behind = ring[(here - 1 + count) % count].GetExactDist2d(boss->GetPositionX(), boss->GetPositionY());
        kiteDir_ = ahead >= behind ? 1 : -1;

        kiteIdx_ = here;
        for (int32 i = 0; i < count && blocked(ring[kiteIdx_]); ++i)
            kiteIdx_ = (kiteIdx_ + kiteDir_ + count) % count;
    }
    else if (vehicleBase_->GetExactDist2d(ring[kiteIdx_]) <= ULDUAR_FL_KITE_ADVANCE_DIST)
    {
        // Advance on approach, never on arrival: waiting until the vehicle reaches the node drives
        // it into the node, and in a corner that is exactly where the boss cuts the diagonal.
        kiteIdx_ = (kiteIdx_ + kiteDir_ + count) % count;
        if (blocked(ring[kiteIdx_]))
            kiteIdx_ = (kiteIdx_ + kiteDir_ + count) % count;
    }

    // The kite outranks the hazard dodge, so it has to steer round the Inferno trail itself: every
    // Inferno hit at one 2026-09-17 pull landed on a hull kiting straight through it.
    Position goal = ring[kiteIdx_];
    if (FlameLeviathanActiveTowerMask(botAI) & FL_TOWER_FLAMES)
        if (std::optional<Position> detour = KiteAroundFire(boss, goal))
        {
            goal = *detour;
            branch = "kite:detour";
        }

    // FORCED, because IsWaitingForLastMove yields only to a strictly higher priority, and the station
    // walk already in flight is COMBAT: at equal priority the hull kept driving toward him for 3-7 s
    // after Pursued landed.
    DriveTo(goal, nullptr, false, MovementPriority::MOVEMENT_FORCED);

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
    std::optional<float> facing;
    if (faceTarget)
        facing = faceAway ? faceTarget->GetAngle(vehicleBase_) : vehicleBase_->GetAngle(faceTarget);

    return DriveToImpl(goal, facing, priority);
}

bool FlameLeviathanDriveAction::DriveTo(Position const& goal, Position const& facePoint, MovementPriority priority)
{
    return DriveToImpl(goal, vehicleBase_->GetAngle(facePoint.GetPositionX(), facePoint.GetPositionY()),
                       priority);
}

bool FlameLeviathanDriveAction::DriveToImpl(Position const& goal, std::optional<float> facing,
                                            MovementPriority priority)
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
        if (facing)
        {
            // NormalizeOrientation lands in [0, 2PI), so fold the far half back into [0, PI].
            float error = Position::NormalizeOrientation(*facing - vehicleBase_->GetOrientation());
            if (error > float(M_PI))
                error = 2.0f * float(M_PI) - error;

            if (error > ULDUAR_FL_FACING_TOLERANCE)
                vehicleBase_->SetFacingTo(*facing);
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

    // This action is the vehicle's only mover, so a leg in flight at this priority is our own stale one,
    // and IsWaitingForLastMove would hold the new goal for up to MaxWaitForMove. A higher leg still wins.
    LastMovement& lastMove = AI_VALUE(LastMovement&, "last movement");
    MovementPriority const held = lastMove.priority;
    if (held == priority && priority != MovementPriority::MOVEMENT_IDLE)
        lastMove.priority = static_cast<MovementPriority>(static_cast<int>(priority) - 1);

    if (!MoveTo(vehicleBase_->GetMapId(), goal.GetPositionX(), goal.GetPositionY(), goal.GetPositionZ(), false, false,
                false, false, priority))
    {
        lastMove.priority = held;
        return false;
    }

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
