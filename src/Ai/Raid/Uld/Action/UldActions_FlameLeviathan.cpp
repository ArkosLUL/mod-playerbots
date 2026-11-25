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
#include "Spell.h"
#include "UldEncounter_FlameLeviathan.h"
#include "UldScripts.h"
#include "Unit.h"
#include "Vehicle.h"
#include "WorldPacket.h"
#include "WorldSession.h"

namespace
{
constexpr float HAZARD_FAN[] = {0.0f, 0.6f, -0.6f, 1.2f, -1.2f, 1.8f, -1.8f, 2.4f, -2.4f, 3.0f};

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

bool CastVehicleSelfSpell(PlayerbotAI* botAI, Unit* vehicleBase, uint32 spellId, uint32 cost, uint32 cooldownMs)
{
    if (!vehicleBase || vehicleBase->HasSpellCooldown(spellId))
        return false;

    uint32 const before = vehicleBase->GetPower(POWER_ENERGY);
    if (before < cost)
        return false;

    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    CharmInfo* charm = vehicleBase->GetCharmInfo();
    if (info && charm && charm->GetGlobalCooldownMgr().HasGlobalCooldown(info))
        return false;

    if (!botAI->CastVehicleSpell(spellId, vehicleBase))
        return false;

    if (cost && vehicleBase->GetPower(POWER_ENERGY) >= before)
        return false;

    vehicleBase->AddSpellCooldown(spellId, 0, cooldownMs);
    return true;
}

bool RequestVehicleExit(Player* bot)
{
    WorldPacket packet;
    bot->GetSession()->HandleRequestVehicleExit(packet);
    return !bot->GetVehicle();
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
}

bool FlameLeviathanVehicleAction::CastVehicle(uint32 spellId, Unit* target, uint32 cooldownMs)
{
    if (!target || !_vehicleBase)
        return false;

    if (!botAI->CanCastVehicleSpell(spellId, target) || !botAI->CastVehicleSpell(spellId, target))
        return false;

    _vehicleBase->AddSpellCooldown(spellId, 0, cooldownMs);
    return true;
}

Unit* FlameLeviathanVehicleAction::FindMechanolift()
{
    Position const centre = FlameLeviathanArenaCentre();
    GuidVector targets = AI_VALUE(GuidVector, "possible targets");
    for (auto const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || unit->GetEntry() != NPC_FL_MECHANOLIFT || !FlameLeviathanInArena(unit->GetPosition()))
            continue;

        if (unit->GetExactDist2d(&centre) > ULDUAR_FL_LIFT_DROP_RADIUS && unit->HasInArc(float(M_PI), &centre))
            continue;

        return unit;
    }

    return nullptr;
}

void FlameLeviathanVehicleAction::NoteLiftShot(Unit* lift)
{
    if (!RaidObs::Active() || lift->GetGUID() == _lastLiftNoted)
        return;

    _lastLiftNoted = lift->GetGUID();
    RaidObs::Note(bot, "fl.lift",
                  std::to_string(lift->GetGUID().GetCounter()) + " " +
                      std::to_string(static_cast<int32>(lift->GetPositionX())) + " " +
                      std::to_string(static_cast<int32>(lift->GetPositionY())));
}

Unit* FlameLeviathanVehicleAction::NearestFightingHostile()
{
    Unit* nearest = nullptr;
    for (auto const& guid : AI_VALUE(GuidVector, "possible targets"))
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || !unit->IsInCombat())
            continue;

        uint32 const entry = unit->GetEntry();
        if (entry == NPC_FLAME_LEVIATHAN || entry == NPC_FL_TURRET || entry == NPC_FL_DEFENSE_TURRET)
            continue;

        if (!nearest || _vehicleBase->GetExactDist(unit) < _vehicleBase->GetExactDist(nearest))
            nearest = unit;
    }

    return nearest;
}

bool FlameLeviathanVehicleAction::Execute(Event /*event*/)
{
    _vehicleBase = bot->GetVehicleBase();
    if (!_vehicleBase || !bot->GetVehicle())
        return false;

    if (FlameLeviathanSeatOf(bot) != FlameLeviathanSeat::Catapult)
        _catapultSinceMs = 0;

    Unit* boss = FlameLeviathanBoss(botAI);

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

    Unit* target = boss && boss->IsInCombat() ? boss : add;

    if (!target)
        target = NearestFightingHostile();

    switch (_vehicleBase->GetEntry())
    {
        case NPC_SALVAGED_DEMOLISHER:
            if (FlameLeviathanIsDriver(bot))
                return DemolisherAction(target);
            return FlameLeviathanSeatOf(bot) == FlameLeviathanSeat::Catapult && CatapultPassengerAction();
        case NPC_SALVAGED_DEMOLISHER_TURRET:
            return DemolisherTurretAction(target);
        case NPC_SALVAGED_SIEGE_ENGINE:
            return SiegeEngineAction(target);
        case NPC_SALVAGED_SIEGE_ENGINE_TURRET:
            return SiegeEngineTurretAction(target);
        case NPC_VEHICLE_CHOPPER:
            return FlameLeviathanIsDriver(bot) ? ChopperAction(target) : ChopperPassengerAction();
        default:
            break;
    }

    return false;
}

bool FlameLeviathanVehicleAction::ThrowPassenger(Unit* boss)
{
    if (!boss || !boss->IsInCombat() || FlameLeviathanShutdown(boss) || FlameLeviathanIsPursued(bot) ||
        FlameLeviathanIsRamTarget(bot))
        return false;

    if (_vehicleBase->IsNonMeleeSpellCast(false, true, true))
        return true;

    Player* passenger = FlameLeviathanCatapultPassenger(_vehicleBase);
    Position const aim = FlameLeviathanThrowAim(boss);
    float const dist = _vehicleBase->GetExactDist2d(aim);
    if (!passenger || dist > ULDUAR_FL_THROW_MAX_RANGE)
        return false;

    if (_vehicleBase->isMoving())
    {
        _vehicleBase->StopMoving();
        return true;
    }

    if (!_vehicleBase->HasInArc(ULDUAR_FL_THROW_FACING_ARC, &aim))
    {
        _vehicleBase->SetFacingTo(_vehicleBase->GetAngle(&aim));
        return true;
    }

    if (!botAI->CastVehicleSpell(SPELL_FL_THROW_PASSENGER, aim.GetPositionX(), aim.GetPositionY(),
                                 aim.GetPositionZ()))
        return false;

    Spell* throwing = _vehicleBase->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!throwing || throwing->m_spellInfo->Id != SPELL_FL_THROW_PASSENGER)
        return false;

    if (RaidObs::Active())
        RaidObs::Note(bot, "fl.throw",
                      RaidObs::DescribeAssignment(passenger->GetGUID()) + " " +
                          std::to_string(static_cast<int32>(dist)) + " " +
                          std::to_string(static_cast<int32>(boss->isMoving() ? boss->GetSpeed(MOVE_RUN) : 0.0f)));

    return true;
}

bool FlameLeviathanVehicleAction::LoadIntoCatapult()
{
    Unit* demolisher = FlameLeviathanRiddenVehicle(bot);
    if (!demolisher || !FlameLeviathanWantsCatapult(botAI, bot))
        return false;

    if (!botAI->CastVehicleSpell(SPELL_FL_LOAD_INTO_CATAPULT, _vehicleBase))
        return false;

    if (!demolisher->HasAura(SPELL_FL_LOAD_INTO_CATAPULT, bot->GetGUID()))
        return false;

    FlameLeviathanClaimSeat(bot, demolisher);
    if (RaidObs::Active())
        RaidObs::Note(bot, "fl.load", RaidObs::DescribeAssignment(demolisher->GetGUID()));

    return true;
}

bool FlameLeviathanVehicleAction::CatapultPassengerAction()
{
    uint32 const now = getMSTime();
    if (!_catapultSinceMs || FlameLeviathanIsPursued(bot))
        _catapultSinceMs = now;

    bool const engaged = FlameLeviathanEngaged(botAI);
    bool const stunned = FlameLeviathanShutdown(FlameLeviathanBoss(botAI));
    bool const waited = getMSTimeDiff(_catapultSinceMs, now) >= ULDUAR_FL_CATAPULT_WAIT_MS;
    if (engaged && !stunned && !waited)
        return false;

    _catapultSinceMs = 0;
    Unit* demolisher = _vehicleBase;
    if (!RequestVehicleExit(bot))
        return false;

    if (!engaged)
        return true;

    demolisher->HandleSpellClick(bot);
    if (RaidObs::Active())
        RaidObs::Note(bot, "fl.unload", stunned ? "shutdown" : "wait");

    return true;
}

bool FlameLeviathanVehicleAction::ChopperPassengerAction()
{
    if (!FlameLeviathanEngaged(botAI))
        return RequestVehicleExit(bot);

    Unit* demolisher = FlameLeviathanFerryTarget(botAI, bot, _vehicleBase);
    if (!demolisher || _vehicleBase->GetExactDist2d(demolisher) > ULDUAR_FL_FERRY_HOP_DIST)
        return false;

    if (!RequestVehicleExit(bot))
        return false;

    demolisher->HandleSpellClick(bot);

    Vehicle* kit = demolisher->GetVehicleKit();
    Unit* seat = kit ? kit->GetPassenger(ULDUAR_FL_DEMOLISHER_GUNNER_SEAT) : nullptr;
    if (RaidObs::Active() && seat && seat->HasAuraTypeWithCaster(SPELL_AURA_CONTROL_VEHICLE, bot->GetGUID()))
        RaidObs::Note(bot, "fl.regun", RaidObs::DescribeAssignment(demolisher->GetGUID()));

    return true;
}

bool FlameLeviathanVehicleAction::FirstAidKit()
{
    Player* passenger = FlameLeviathanChopperPassenger(_vehicleBase);
    if (!passenger || !passenger->IsAlive() || passenger->HasAura(SPELL_FL_FIRST_AID_KIT) ||
        passenger->GetHealthPct() >= ULDUAR_FL_FIRST_AID_KIT_HEALTH_PCT ||
        _vehicleBase->HasSpellCooldown(SPELL_FL_FIRST_AID_KIT))
        return false;

    if (!botAI->CastVehicleSpell(SPELL_FL_FIRST_AID_KIT, _vehicleBase) || !passenger->HasAura(SPELL_FL_FIRST_AID_KIT))
    {
        _vehicleBase->AddSpellCooldown(SPELL_FL_FIRST_AID_KIT, 0, ULDUAR_FL_FIRST_AID_KIT_RETRY_MS);
        return false;
    }

    _vehicleBase->AddSpellCooldown(SPELL_FL_FIRST_AID_KIT, 0, ULDUAR_FL_FIRST_AID_KIT_COOLDOWN_MS);
    if (RaidObs::Active())
        RaidObs::Note(bot, "fl.heal", RaidObs::DescribeAssignment(passenger->GetGUID()));

    return true;
}

Aura* FlameLeviathanVehicleAction::TrackBarrels(Unit* target)
{
    Aura* own = target->GetAura(SPELL_FL_BLUE_PYRITE_DOT, _vehicleBase->GetGUID());
    uint8 const stacks = own ? own->GetStackAmount() : 0;
    int32 const duration = own ? own->GetDuration() : 0;
    uint32 const now = getMSTime();

    if (target->GetGUID() != _barrelTarget)
    {
        _barrelTarget = target->GetGUID();
        _barrelsInFlight.clear();
    }
    else if (own && !_barrelsInFlight.empty() &&
             (stacks > _lastPyriteStacks || duration > _lastPyriteDurationMs + ULDUAR_FL_PYRITE_LANDING_JUMP_MS))
    {
        _barrelLeadMs = std::clamp<uint32>(getMSTimeDiff(_barrelsInFlight.front(), now), ULDUAR_FL_PYRITE_FLIGHT_MIN_MS,
                                           ULDUAR_FL_PYRITE_FLIGHT_TIMEOUT_MS);
        size_t const landed =
            std::min<size_t>(std::max(int(stacks) - int(_lastPyriteStacks), 1), _barrelsInFlight.size());
        _barrelsInFlight.erase(_barrelsInFlight.begin(), _barrelsInFlight.begin() + landed);
    }

    _barrelsInFlight.erase(std::remove_if(_barrelsInFlight.begin(), _barrelsInFlight.end(),
                                          [now](uint32 cast)
                                          { return getMSTimeDiff(cast, now) >= ULDUAR_FL_PYRITE_FLIGHT_TIMEOUT_MS; }),
                           _barrelsInFlight.end());

    _lastPyriteStacks = stacks;
    _lastPyriteDurationMs = duration;
    return own;
}

bool FlameLeviathanVehicleAction::DemolisherAction(Unit* target)
{
    Aura* own = target ? TrackBarrels(target) : nullptr;

    if (_vehicleBase->HasAura(SPELL_FL_PASSENGER_LOADED))
        return ThrowPassenger(FlameLeviathanBoss(botAI));

    if (!target)
        return false;

    if (Unit* frozen = FlameLeviathanFrozenVehicle(bot, _vehicleBase, ULDUAR_FL_HURL_BOULDER_MIN_RANGE,
                                                   ULDUAR_FL_HURL_BOULDER_MAX_RANGE))
        if (CastVehicle(SPELL_FL_HURL_BOULDER, frozen))
            return true;

    char const* barrel = "not boss";
    bool wantBarrel = false;
    if (target->GetEntry() == NPC_FLAME_LEVIATHAN)
    {
        uint32 const energy = _vehicleBase->GetPower(POWER_ENERGY);
        uint32 const stacks = std::min<uint32>((own ? own->GetStackAmount() : 0) + _barrelsInFlight.size(),
                                               ULDUAR_FL_PYRITE_MAX_STACKS);
        uint32 const burst = FlameLeviathanShutdown(target) ||
                                     energy >= (ULDUAR_FL_PYRITE_MAX_STACKS - stacks) * ULDUAR_FL_PYRITE_BARREL_COST +
                                                   ULDUAR_FL_PYRITE_REFRESH_RESERVE
                                 ? ULDUAR_FL_PYRITE_MAX_STACKS
                                 : ULDUAR_FL_PYRITE_BURST_STACKS;

        uint32 const lead = _barrelLeadMs ? _barrelLeadMs : ULDUAR_FL_PYRITE_FLIGHT_MS;

        if (energy < ULDUAR_FL_PYRITE_BARREL_COST)
            barrel = "dry";
        else if (stacks < burst)
        {
            barrel = "burst";
            wantBarrel = true;
        }
        else if (own && _barrelsInFlight.empty() &&
                 own->GetDuration() <= int32(lead + ULDUAR_FL_PYRITE_REFRESH_SLACK_MS))
        {
            barrel = "refresh";
            wantBarrel = true;
        }
        else
            barrel = "hold";
    }

    SpellInfo const* barrelInfo = sSpellMgr->GetSpellInfo(SPELL_FL_HURL_PYRITE_BARREL);
    CharmInfo* charm = _vehicleBase->GetCharmInfo();
    bool const ready = barrelInfo && !_vehicleBase->HasSpellCooldown(SPELL_FL_HURL_PYRITE_BARREL) &&
                       !(charm && charm->GetGlobalCooldownMgr().HasGlobalCooldown(barrelInfo));

    bool thrown = false;
    if (wantBarrel && ready)
    {
        thrown = CastVehicle(SPELL_FL_HURL_PYRITE_BARREL, target);
        if (thrown)
            _barrelsInFlight.push_back(getMSTime());
        else
            barrel = "fail";
    }

    if (RaidObs::Active())
    {
        RaidObs::NoteDerived(bot, "fl.pyrite", std::to_string(own ? own->GetStackAmount() : 0));
        RaidObs::NoteDerived(bot, "fl.barrel", barrel);
    }

    if (thrown)
        return true;

    if (Unit* add = FlameLeviathanBestAdd(botAI, _vehicleBase, 0.0f, ULDUAR_FL_RAM_CONE_RADIUS, 0.0f))
        if (FlameLeviathanFaceForCone(_vehicleBase, add, ULDUAR_FL_RAM_CONE_HALF_ANGLE,
                                      ULDUAR_FL_RAM_CONE_RADIUS))
            if (CastVehicle(SPELL_FL_DEMOLISHER_RAM, add, ULDUAR_FL_DEMOLISHER_RAM_COOLDOWN_MS))
                return true;

    if (!wantBarrel)
        if (Unit* add = FlameLeviathanBestAdd(botAI, _vehicleBase, ULDUAR_FL_HURL_BOULDER_MIN_RANGE,
                                              ULDUAR_FL_HURL_BOULDER_MAX_RANGE, ULDUAR_FL_CANNON_SPLASH))
            if (CastVehicle(SPELL_FL_HURL_BOULDER, add))
                return true;

    return CastVehicle(SPELL_FL_HURL_BOULDER, target);
}

bool FlameLeviathanVehicleAction::DemolisherTurretAction(Unit* target)
{
    Unit* demolisher = FlameLeviathanRiddenVehicle(bot);

    if ((FlameLeviathanIsPursued(bot) || FlameLeviathanIsRamTarget(bot)) && demolisher &&
        !demolisher->HasAura(SPELL_FL_INCREASED_SPEED))
    {
        if (CastVehicleSelfSpell(botAI, _vehicleBase, SPELL_FL_INCREASED_SPEED, ULDUAR_FL_INCREASED_SPEED_COST,
                                 ULDUAR_FL_SPEED_RECAST_MS))
        {
            if (demolisher->HasAura(SPELL_FL_INCREASED_SPEED))
            {
                if (RaidObs::Active())
                    RaidObs::Note(bot, "fl.speed", "demolisher");

                return true;
            }
        }
    }

    if (LoadIntoCatapult())
        return true;

    SpellInfo const* grabInfo = sSpellMgr->GetSpellInfo(SPELL_FL_GRAB_CRATE);
    CharmInfo* charm = _vehicleBase->GetCharmInfo();
    auto const grabOnCooldown = [grabInfo, charm]
    { return !grabInfo || !charm || charm->GetGlobalCooldownMgr().HasGlobalCooldown(grabInfo); };

    if (demolisher && demolisher->GetPower(POWER_ENERGY) <= ULDUAR_FL_CRATE_GRAB_CEILING && !grabOnCooldown())
    {
        GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
        for (auto const& guid : npcs)
        {
            Unit* crate = botAI->GetUnit(guid);
            if (!crate || crate->GetEntry() != NPC_FL_PYRITE_CONTAINER || FlameLeviathanCrateTaken(bot, crate))
                continue;

            if (crate->GetDistance(bot) >= ULDUAR_FL_CRATE_GRAB_RANGE)
                continue;

            if (CastVehicle(SPELL_FL_GRAB_CRATE, crate))
            {
                if (grabOnCooldown())
                    FlameLeviathanTakeCrate(bot, crate);
                return true;
            }
        }
    }

    if (Unit* frozen = FlameLeviathanFrozenVehicle(bot, _vehicleBase, 0.0f, ULDUAR_FL_MORTAR_MAX_RANGE))
        if (CastVehicle(SPELL_FL_MORTAR, frozen, ULDUAR_FL_MORTAR_COOLDOWN_MS))
            return true;

    if (Unit* lift = FindMechanolift())
        if (CastVehicle(SPELL_FL_ANTI_AIR_ROCKET, lift, ULDUAR_FL_ANTI_AIR_ROCKET_COOLDOWN_MS))
        {
            NoteLiftShot(lift);
            return true;
        }

    if (Unit* add = FlameLeviathanBestAdd(botAI, _vehicleBase, 0.0f, ULDUAR_FL_MORTAR_MAX_RANGE,
                                          ULDUAR_FL_MORTAR_SPLASH))
        if (CastVehicle(SPELL_FL_MORTAR, add, ULDUAR_FL_MORTAR_COOLDOWN_MS))
            return true;

    return CastVehicle(SPELL_FL_MORTAR, target, ULDUAR_FL_MORTAR_COOLDOWN_MS);
}

bool FlameLeviathanVehicleAction::SiegeEngineAction(Unit* target)
{
    uint32 needed = ULDUAR_FL_RAM_COST;
    if (!FlameLeviathanShutdown(FlameLeviathanBoss(botAI)))
    {
        if (FlameLeviathanIsVentInterrupter(botAI, bot))
            needed += ULDUAR_FL_ELECTROSHOCK_COST;
        if (FlameLeviathanIsPursued(bot) || FlameLeviathanIsVentReserve(bot))
            needed += ULDUAR_FL_STEAM_RUSH_COST;
    }

    if (_vehicleBase->GetPower(POWER_ENERGY) < needed)
        return false;

    Unit* add = FlameLeviathanBestAdd(botAI, _vehicleBase, 0.0f, ULDUAR_FL_RAM_CONE_RADIUS, 0.0f);
    Unit* shot = add ? add : target;

    if (!shot)
        return false;

    if (FlameLeviathanIsVentReserve(bot) || (!add && FlameLeviathanCornerPost(botAI, bot) >= 0))
    {
        if (!FlameLeviathanInCone(_vehicleBase, shot, ULDUAR_FL_RAM_CONE_HALF_ANGLE,
                                  ULDUAR_FL_RAM_CONE_RADIUS))
            return false;
    }
    else if (!FlameLeviathanFaceForCone(_vehicleBase, shot, ULDUAR_FL_RAM_CONE_HALF_ANGLE,
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
            if (CastVehicleSelfSpell(botAI, _vehicleBase, SPELL_FL_SHIELD_GENERATOR, 0,
                                     ULDUAR_FL_SHIELD_GENERATOR_COOLDOWN_MS))
                if (siege->HasAura(SPELL_FL_SHIELD_GENERATOR))
                    return true;
    }

    if (Unit* lift = FindMechanolift())
        if (CastVehicle(SPELL_FL_ANTI_AIR_ROCKET_SIEGE, lift, ULDUAR_FL_ANTI_AIR_ROCKET_COOLDOWN_MS))
        {
            NoteLiftShot(lift);
            return true;
        }

    if (_vehicleBase->GetPower(POWER_ENERGY) < ULDUAR_FL_FIRE_CANNON_COST)
        return false;

    if (Unit* hull = FlameLeviathanRiddenVehicle(bot))
    {
        int8 const post = FlameLeviathanHullCornerPost(botAI, bot, hull);
        if (post >= 0)
        {
            Position const point = FlameLeviathanCornerPostPoint(static_cast<uint8>(post));
            if (Unit* add = FlameLeviathanBestAdd(botAI, _vehicleBase, ULDUAR_FL_FIRE_CANNON_MIN_RANGE,
                                                  ULDUAR_FL_FIRE_CANNON_MAX_RANGE, ULDUAR_FL_CANNON_SPLASH, &point,
                                                  ULDUAR_FL_CORNER_HOLD_RADIUS))
                if (CastVehicle(SPELL_FL_FIRE_CANNON, add))
                    return true;
        }
    }

    if (Unit* add = FlameLeviathanBestAdd(botAI, _vehicleBase, ULDUAR_FL_FIRE_CANNON_MIN_RANGE,
                                          ULDUAR_FL_FIRE_CANNON_MAX_RANGE, ULDUAR_FL_CANNON_SPLASH))
        if (CastVehicle(SPELL_FL_FIRE_CANNON, add))
            return true;

    if (!target)
        return false;

    if (target->GetEntry() == NPC_FLAME_LEVIATHAN && FlameLeviathanShutdownSoon(bot) &&
        _vehicleBase->GetPower(POWER_ENERGY) < ULDUAR_FL_FIRE_CANNON_POOL)
        return false;

    return CastVehicle(SPELL_FL_FIRE_CANNON, target);
}

bool FlameLeviathanVehicleAction::ChopperAction(Unit* target)
{
    if (FirstAidKit())
        return true;

    if (target && !_vehicleBase->HasInArc(M_PI / 2.0f, target))
        if (CastVehicleSelfSpell(botAI, _vehicleBase, SPELL_FL_TAR, 0, ULDUAR_FL_TAR_COOLDOWN_MS))
            return true;

    if (_vehicleBase->GetPower(POWER_ENERGY) < ULDUAR_FL_SONIC_HORN_COST)
        return false;

    Unit* shot = FlameLeviathanIsTarLead(botAI, bot)
                     ? nullptr
                     : FlameLeviathanBestAdd(botAI, _vehicleBase, 0.0f,
                                             ULDUAR_FL_SONIC_HORN_CONE_RADIUS, 0.0f);
    if (!shot)
        shot = target;

    if (!shot || !FlameLeviathanFaceForCone(_vehicleBase, shot, ULDUAR_FL_SONIC_HORN_CONE_HALF_ANGLE,
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

    bool const dodging =
        FlameLeviathanShouldClearBatteringRam(botAI, bot) ||
        GetFlameLeviathanNearestTowerHazard(botAI, vehicleBase, FL_TOWER_ALL, ULDUAR_FL_TOWER_HAZARD_MARGIN,
                                            FlameLeviathanIsVentReserve(bot) ? FL_TOWER_STORM : 0);

    if (!vehicleBase->HasInArc(ULDUAR_FL_ELECTROSHOCK_CONE_HALF_ANGLE * 2.0f, boss))
    {
        if (dodging)
            return false;

        vehicleBase->StopMoving();
    }

    if (!FlameLeviathanFaceForCone(vehicleBase, boss, ULDUAR_FL_ELECTROSHOCK_CONE_HALF_ANGLE,
                                   ULDUAR_FL_ELECTROSHOCK_CONE_RADIUS))
        return true;

    if (!botAI->CanCastVehicleSpell(SPELL_FL_ELECTROSHOCK, boss))
    {
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

    FlameLeviathanClaimVentChannel(bot);

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
    _vehicleBase = bot->GetVehicleBase();
    if (!_vehicleBase || !FlameLeviathanIsDriver(bot))
        return false;

    if (!FlameLeviathanEngaged(botAI))
        return false;

    Unit* boss = FlameLeviathanBoss(botAI);
    if (!boss)
        return false;

    auto const branch = [this](char const* name)
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "fl.drive", name);
    };

    bool const stunned = FlameLeviathanShutdown(boss);
    bool const pursued = FlameLeviathanIsPursued(bot);
    if (!stunned && (pursued || FlameLeviathanIsRamTarget(bot)))
    {
        _dodgeGoal.reset();
        char const* how = pursued ? "kite" : "kite:victim";
        bool const kiting = Kite(boss, how);
        branch(how);
        return kiting;
    }

    ResetKite();

    bool const ventDuty = !stunned && FlameLeviathanIsVentReserve(bot) && FlameLeviathanVentWindowOpen(bot, boss);

    Unit* hazard = GetFlameLeviathanNearestTowerHazard(botAI, _vehicleBase, FL_TOWER_ALL,
                                                       ULDUAR_FL_TOWER_HAZARD_MARGIN, ventDuty ? FL_TOWER_STORM : 0);

    if (!hazard)
        _dodgeGoal.reset();

    if (hazard && ClearHazard(hazard, ventDuty ? boss : nullptr))
    {
        branch(HazardBranch(hazard));
        return true;
    }

    if (ClearBatteringRam(boss))
    {
        branch("ram");
        return true;
    }

    if (_vehicleBase->IsNonMeleeSpellCast(false, true, true) || Catapult(boss))
    {
        branch("catapult");
        return true;
    }

    if (Ferry())
    {
        branch("ferry");
        return true;
    }

    if (Pickup())
    {
        branch("pickup");
        return true;
    }

    if (int8 const corner = FlameLeviathanCornerPost(botAI, bot); corner >= 0)
    {
        uint8 const slot = static_cast<uint8>(corner);
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "fl.corner", std::to_string(static_cast<uint32>(slot)));
        branch("corner");

        Position const post = FlameLeviathanCornerPostPoint(slot);
        if (_vehicleBase->GetExactDist2d(post) > ULDUAR_FL_STEAM_RUSH_DIST + ULDUAR_FL_ARRIVE_TOLERANCE &&
            _vehicleBase->HasInArc(ULDUAR_FL_STEAM_RUSH_AIM_ARC, &post))
            CastVehicleSelfSpell(botAI, _vehicleBase, SPELL_FL_STEAM_RUSH, ULDUAR_FL_STEAM_RUSH_COST,
                                 ULDUAR_FL_STEAM_RUSH_COOLDOWN_MS);

        Unit* add = FlameLeviathanBestAdd(botAI, _vehicleBase, 0.0f, ULDUAR_FL_RAM_CONE_RADIUS, 0.0f);

        if (add)
            return DriveTo(post, add, false);

        return DriveTo(post, ULDUAR_FL_ARENA_CORNERS[slot]);
    }

    if (_vehicleBase->GetEntry() == NPC_SALVAGED_DEMOLISHER &&
        _vehicleBase->GetPower(POWER_ENERGY) < ULDUAR_FL_CRATE_DETOUR_ENERGY)
        if (DetourToCrate(boss))
        {
            branch("crate");
            return true;
        }

    char const* how = "station";
    bool const holding = HoldStation(boss, how);
    branch(how);
    return holding;
}

void FlameLeviathanDriveAction::ResetKite()
{
    if (_kiteIdx < 0)
        return;

    _kiteIdx = -1;
    _kiteDir = 0;

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

        if (_vehicleBase->GetExactDist2d(crate) > radius)
            continue;

        if (!nearest || _vehicleBase->GetExactDist2d(crate) < _vehicleBase->GetExactDist2d(nearest))
            nearest = crate;
    }

    return nearest;
}

bool FlameLeviathanDriveAction::DetourToCrate(Unit* boss)
{
    Unit* crate = NearestCrate(ULDUAR_FL_CRATE_DETOUR_RADIUS);
    if (!crate)
        return false;

    float const grab = ULDUAR_FL_CRATE_GRAB_RANGE - ULDUAR_FL_ARRIVE_TOLERANCE;
    float const dist = _vehicleBase->GetExactDist2d(crate);
    if (dist <= grab)
        return false;

    float const angle = crate->GetAngle(_vehicleBase);
    Position const goal(crate->GetPositionX() + std::cos(angle) * grab, crate->GetPositionY() + std::sin(angle) * grab,
                        crate->GetPositionZ());

    float const leash = boss->GetCombatReach() + FlameLeviathanDemolisherStandDist(boss) + ULDUAR_FL_ARRIVE_TOLERANCE;
    if (boss->GetExactDist2d(goal) > leash)
        return false;

    DriveTo(goal, boss, false);
    return true;
}

bool FlameLeviathanDriveAction::ClearHazard(Unit* hazard, Unit* keepInReachOf)
{
    float const angle = hazard->GetAngle(_vehicleBase);

    float const reach = FlameLeviathanHazardReach(hazard, _vehicleBase);
    float const step = reach + 2.0f * ULDUAR_FL_ARRIVE_TOLERANCE - _vehicleBase->GetExactDist2d(hazard);

    if (step <= 0.0f)
    {
        _dodgeGoal.reset();
        return false;
    }

    auto const pointFor = [this](float bearing, float reach)
    {
        return Position(_vehicleBase->GetPositionX() + std::cos(bearing) * reach,
                        _vehicleBase->GetPositionY() + std::sin(bearing) * reach,
                        _vehicleBase->GetPositionZ());
    };

    std::vector<Unit*> hazards;
    GetFlameLeviathanTowerHazards(botAI, _vehicleBase, FL_TOWER_ALL, ULDUAR_FL_TOWER_HAZARD_CLEAR_SCAN, hazards);

    Position const here = _vehicleBase->GetPosition();
    auto const usable = [&](Position const& goal)
    {
        return FlameLeviathanInArena(goal) &&
               LegClearOfHazards(_vehicleBase, goal, goal, hazards, ULDUAR_FL_ARRIVE_TOLERANCE) &&
               LegClearOfHazards(_vehicleBase, here, goal, hazards, 0.0f);
    };

    if (_dodgeGoal && !usable(*_dodgeGoal))
        _dodgeGoal.reset();

    if (!_dodgeGoal)
    {
        std::optional<Position> compromise;

        for (float travel : {step, step + reach, step + 2.0f * reach})
        {
            for (float offset : HAZARD_FAN)
            {
                Position const goal = pointFor(angle + offset, travel);
                if (!usable(goal))
                    continue;

                if (keepInReachOf &&
                    goal.GetExactDist2d(keepInReachOf) - keepInReachOf->GetObjectSize() >
                        ULDUAR_FL_ELECTROSHOCK_CONE_RADIUS)
                {
                    if (!compromise)
                        compromise = goal;
                    continue;
                }

                _dodgeGoal = goal;
                break;
            }

            if (_dodgeGoal)
                break;
        }

        if (!_dodgeGoal)
            _dodgeGoal = compromise;
    }

    Position const goal = _dodgeGoal ? *_dodgeGoal : pointFor(angle, step);
    if (!DriveTo(goal, keepInReachOf, false, MovementPriority::MOVEMENT_FORCED))
        _dodgeGoal.reset();

    return true;
}

bool FlameLeviathanDriveAction::ClearBatteringRam(Unit* boss)
{
    if (!FlameLeviathanShouldClearBatteringRam(botAI, bot))
        return false;

    Unit* centre = FlameLeviathanRamCentre(botAI, bot);
    if (!centre)
        return false;

    float const safeDist = ULDUAR_FL_BATTERING_RAM_RADIUS + _vehicleBase->GetObjectSize();

    float const angle = centre->GetAngle(_vehicleBase);
    float const step = safeDist - _vehicleBase->GetExactDist2d(centre) + 2.0f * ULDUAR_FL_ARRIVE_TOLERANCE;
    Position const goal(_vehicleBase->GetPositionX() + std::cos(angle) * step,
                        _vehicleBase->GetPositionY() + std::sin(angle) * step,
                        _vehicleBase->GetPositionZ());

    DriveTo(goal, boss, false, MovementPriority::MOVEMENT_FORCED);
    return true;
}

bool FlameLeviathanDriveAction::HoldStation(Unit* boss, char const*& branch)
{
    float standDist = ULDUAR_FL_SIEGE_STAND_DIST;
    float stationLead = 0.0f;
    bool centreSide = false;
    char const* how = "siege";
    switch (_vehicleBase->GetEntry())
    {
        case NPC_VEHICLE_CHOPPER:
            if (FlameLeviathanIsTarLead(botAI, bot))
            {
                float const lead = FlameLeviathanTarLeadDistance(boss, FlameLeviathanPursuedVehicle(botAI, bot),
                                                                 _vehicleBase->GetObjectSize());
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
            standDist = FlameLeviathanDemolisherStandDist(boss);
            stationLead = FlameLeviathanStationLead(boss);
            centreSide = true;
            how = "demolisher";
            break;
        default:
            break;
    }

    bool const stunned = FlameLeviathanShutdown(boss);
    bool const ventReserve = FlameLeviathanIsVentReserve(bot);
    if (ventReserve)
    {
        how = "vent";
        stationLead = FlameLeviathanStationLead(boss);
        centreSide = true;

        if (!stunned && FlameLeviathanMsToNextVent(bot, boss) > ULDUAR_FL_VENT_APPROACH_LEAD_MS)
            standDist = ULDUAR_FL_BATTERING_RAM_CAST_RANGE + _vehicleBase->GetCombatReach() +
                        ULDUAR_FL_ARRIVE_TOLERANCE;
    }

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "fl.station", how);

    if (ventReserve && !stunned && RushToVents(boss))
        return true;

    float fanHalf = 0.0f;
    float offset = FlameLeviathanStationBearingOffset(bot, _vehicleBase, boss->GetCombatReach() + standDist, &fanHalf);
    if (centreSide)
        offset += FlameLeviathanCentreTurn(boss, fanHalf);

    Position const station = FlameLeviathanRearPoint(boss, standDist, offset, stationLead);

    Position goal = station;
    if (std::optional<Position> detour = DetourAroundFire(boss, station, true))
    {
        goal = *detour;
        branch = "station:detour";
    }

    float coneRadius = 0.0f;
    if (!ventReserve)
    {
        switch (_vehicleBase->GetEntry())
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
        if (Unit* add = FlameLeviathanBestAdd(botAI, _vehicleBase, 0.0f, coneRadius, 0.0f))
            return DriveTo(goal, add, false);

    return DriveTo(goal, boss, false);
}

bool FlameLeviathanDriveAction::RushToVents(Unit* boss)
{
    uint32 const toVent = FlameLeviathanMsToNextVent(bot, boss);
    if (toVent && (toVent < ULDUAR_FL_STEAM_RUSH_GCD_MS || toVent > ULDUAR_FL_VENT_RUSH_LEAD_MS))
        return false;

    if (FlameLeviathanShouldClearBatteringRam(botAI, bot) ||
        GetFlameLeviathanNearestTowerHazard(botAI, _vehicleBase, FL_TOWER_ALL, ULDUAR_FL_TOWER_HAZARD_MARGIN,
                                            FL_TOWER_STORM))
        return false;

    if (_vehicleBase->GetExactDist2d(boss) - boss->GetObjectSize() <= ULDUAR_FL_STEAM_RUSH_DIST)
        return false;

    if (_vehicleBase->HasSpellCooldown(SPELL_FL_STEAM_RUSH) ||
        _vehicleBase->GetPower(POWER_ENERGY) < ULDUAR_FL_STEAM_RUSH_COST + ULDUAR_FL_ELECTROSHOCK_COST)
        return false;

    SpellInfo const* rushInfo = sSpellMgr->GetSpellInfo(SPELL_FL_STEAM_RUSH);
    CharmInfo* charm = _vehicleBase->GetCharmInfo();
    if (!rushInfo || (charm && charm->GetGlobalCooldownMgr().HasGlobalCooldown(rushInfo)))
        return false;

    float const bearing = _vehicleBase->GetAngle(boss);
    Position const landing(_vehicleBase->GetPositionX() + std::cos(bearing) * ULDUAR_FL_STEAM_RUSH_DIST,
                           _vehicleBase->GetPositionY() + std::sin(bearing) * ULDUAR_FL_STEAM_RUSH_DIST,
                           _vehicleBase->GetPositionZ());
    if (Unit* centre = FlameLeviathanRamCentre(botAI, bot))
        if (centre->GetExactDist2d(landing) <= ULDUAR_FL_BATTERING_RAM_RADIUS + _vehicleBase->GetObjectSize())
            return false;

    std::vector<Unit*> fires;
    GetFlameLeviathanTowerHazards(botAI, _vehicleBase, FL_TOWER_FLAMES, ULDUAR_FL_TOWER_HAZARD_CLEAR_SCAN, fires);
    if (!LegClearOfHazards(_vehicleBase, _vehicleBase->GetPosition(), landing, fires, 0.0f))
        return false;

    if (!_vehicleBase->HasInArc(ULDUAR_FL_STEAM_RUSH_AIM_ARC, boss))
    {
        _vehicleBase->StopMoving();
        _vehicleBase->SetFacingToObject(boss);
        return true;
    }

    if (!CastVehicleSelfSpell(botAI, _vehicleBase, SPELL_FL_STEAM_RUSH, ULDUAR_FL_STEAM_RUSH_COST,
                              ULDUAR_FL_STEAM_RUSH_COOLDOWN_MS))
        return false;

    if (RaidObs::Active())
        RaidObs::Note(bot, "fl.rush", "vent");

    return true;
}

std::optional<Position> FlameLeviathanDriveAction::DetourAroundFire(Unit* boss, Position const& goal, bool mayClose)
{
    std::vector<Unit*> fires;
    GetFlameLeviathanTowerHazards(botAI, _vehicleBase, FL_TOWER_FLAMES, ULDUAR_FL_TOWER_HAZARD_CLEAR_SCAN, fires);
    if (fires.empty())
        return std::nullopt;

    Position const here = _vehicleBase->GetPosition();
    float const bearing = _vehicleBase->GetAngle(&goal);
    auto const along = [&here](float angle, float dist)
    {
        return Position(here.GetPositionX() + std::cos(angle) * dist, here.GetPositionY() + std::sin(angle) * dist,
                        here.GetPositionZ());
    };

    float const lookahead = std::min(_vehicleBase->GetExactDist2d(goal), ULDUAR_FL_KITE_FIRE_LOOKAHEAD);
    if (LegClearOfHazards(_vehicleBase, here, along(bearing, lookahead), fires, ULDUAR_FL_TOWER_HAZARD_MARGIN))
        return std::nullopt;

    float const bossDist = _vehicleBase->GetExactDist2d(boss);
    for (float offset : HAZARD_FAN)
    {
        Position const detour = along(bearing + offset, ULDUAR_FL_KITE_DETOUR_STEP);
        if (!FlameLeviathanInArena(detour) || (!mayClose && boss->GetExactDist2d(detour) < bossDist))
            continue;

        if (LegClearOfHazards(_vehicleBase, here, detour, fires, ULDUAR_FL_TOWER_HAZARD_MARGIN))
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
            if (_vehicleBase->GetExactDist2d(ring[i]) < _vehicleBase->GetExactDist2d(ring[best]))
                best = i;
        return best;
    };

    if (_kiteIdx < 0)
    {
        int32 const here = nearestNode();
        float const ahead = ring[(here + 1) % count].GetExactDist2d(boss->GetPositionX(), boss->GetPositionY());
        float const behind =
            ring[(here - 1 + count) % count].GetExactDist2d(boss->GetPositionX(), boss->GetPositionY());
        _kiteDir = ahead >= behind ? 1 : -1;

        _kiteIdx = here;
        for (int32 i = 0; i < count && blocked(ring[_kiteIdx]); ++i)
            _kiteIdx = (_kiteIdx + _kiteDir + count) % count;
    }
    else if (_vehicleBase->GetExactDist2d(ring[_kiteIdx]) <= ULDUAR_FL_KITE_ADVANCE_DIST)
    {
        _kiteIdx = (_kiteIdx + _kiteDir + count) % count;
        if (blocked(ring[_kiteIdx]))
            _kiteIdx = (_kiteIdx + _kiteDir + count) % count;
    }

    bool const ramming = _vehicleBase->HasAura(SPELL_FL_BATTERING_RAM) ||
                         boss->IsWithinCombatRange(_vehicleBase, ULDUAR_FL_BATTERING_RAM_CAST_RANGE);

    Position goal = ring[_kiteIdx];
    if (!ramming)
        if (std::optional<Position> detour = DetourAroundFire(boss, goal, false))
        {
            goal = *detour;
            branch = "kite:detour";
        }

    DriveTo(goal, nullptr, false, MovementPriority::MOVEMENT_FORCED);

    if (!_vehicleBase->HasInArc(M_PI / 2.0f, boss))
    {
        switch (_vehicleBase->GetEntry())
        {
            case NPC_SALVAGED_SIEGE_ENGINE:
                CastVehicleSelfSpell(botAI, _vehicleBase, SPELL_FL_STEAM_RUSH, ULDUAR_FL_STEAM_RUSH_COST,
                                     ULDUAR_FL_STEAM_RUSH_COOLDOWN_MS);
                break;
            case NPC_VEHICLE_CHOPPER:
                if (!_vehicleBase->HasAura(SPELL_FL_SPEED_BOOST))
                    CastVehicleSelfSpell(botAI, _vehicleBase, SPELL_FL_SPEED_BOOST, ULDUAR_FL_SPEED_BOOST_COST,
                                         ULDUAR_FL_SPEED_RECAST_MS);
                break;
            default:
                break;
        }
    }

    return true;
}

bool FlameLeviathanDriveAction::Catapult(Unit* boss)
{
    if (_vehicleBase->GetEntry() != NPC_SALVAGED_DEMOLISHER || !_vehicleBase->HasAura(SPELL_FL_PASSENGER_LOADED))
        return false;

    Position const aim = FlameLeviathanThrowAim(boss);
    float const facing = _vehicleBase->GetAngle(&aim);
    if (_vehicleBase->GetExactDist2d(aim) <= ULDUAR_FL_THROW_MAX_RANGE)
    {
        DriveToImpl(_vehicleBase->GetPosition(), facing, MovementPriority::MOVEMENT_COMBAT);
        return true;
    }

    float const back = aim.GetAngle(_vehicleBase);
    Position const goal(aim.GetPositionX() + std::cos(back) * ULDUAR_FL_THROW_APPROACH,
                        aim.GetPositionY() + std::sin(back) * ULDUAR_FL_THROW_APPROACH, aim.GetPositionZ());
    DriveTo(goal, boss, false);
    return true;
}

bool FlameLeviathanDriveAction::Ferry()
{
    Player* passenger = FlameLeviathanChopperPassenger(_vehicleBase);
    Unit* demolisher = passenger ? FlameLeviathanFerryTarget(botAI, passenger, _vehicleBase) : nullptr;
    if (!demolisher)
        return false;

    float const angle = demolisher->GetAngle(_vehicleBase);
    float const stand = demolisher->GetObjectSize() + _vehicleBase->GetObjectSize() + 1.0f;
    Position const goal(demolisher->GetPositionX() + std::cos(angle) * stand,
                        demolisher->GetPositionY() + std::sin(angle) * stand, demolisher->GetPositionZ());
    DriveTo(goal, demolisher, false, MovementPriority::MOVEMENT_COMBAT, ULDUAR_FL_FERRY_ARRIVE);
    return true;
}

bool FlameLeviathanDriveAction::Pickup()
{
    Player* member = FlameLeviathanPickupFor(bot);
    if (!member)
        return false;

    float const angle = member->GetAngle(_vehicleBase);
    float const stand = _vehicleBase->GetObjectSize() + 1.0f;
    Position const goal(member->GetPositionX() + std::cos(angle) * stand,
                        member->GetPositionY() + std::sin(angle) * stand, member->GetPositionZ());
    DriveTo(goal, member, false, MovementPriority::MOVEMENT_COMBAT, ULDUAR_FL_PICKUP_ARRIVE);
    return true;
}

bool FlameLeviathanDriveAction::DriveTo(Position const& goal, Unit* faceTarget, bool faceAway,
                                        MovementPriority priority, float arrive)
{
    std::optional<float> facing;
    if (faceTarget)
        facing = faceAway ? faceTarget->GetAngle(_vehicleBase) : _vehicleBase->GetAngle(faceTarget);

    return DriveToImpl(goal, facing, priority, arrive);
}

bool FlameLeviathanDriveAction::DriveTo(Position const& goal, Position const& facePoint, MovementPriority priority)
{
    return DriveToImpl(goal, _vehicleBase->GetAngle(facePoint.GetPositionX(), facePoint.GetPositionY()),
                       priority);
}

bool FlameLeviathanDriveAction::DriveToImpl(Position const& goal, std::optional<float> facing,
                                            MovementPriority priority, float arrive)
{
    if (_vehicleBase->GetExactDist2d(goal) <= arrive)
    {
        if (!_parked)
        {
            _vehicleBase->StopMoving();
            _parked = true;
        }

        if (facing)
        {
            float error = Position::NormalizeOrientation(*facing - _vehicleBase->GetOrientation());
            if (error > float(M_PI))
                error = 2.0f * float(M_PI) - error;

            if (error > ULDUAR_FL_FACING_TOLERANCE)
                _vehicleBase->SetFacingTo(*facing);
        }

        return false;
    }

    if (_hasIssued && !_parked && _issued.GetExactDist2d(goal) < ULDUAR_FL_REPOSITION_EPSILON &&
        _vehicleBase->isMoving())
        return true;

    LastMovement& lastMove = AI_VALUE(LastMovement&, "last movement");
    MovementPriority const held = lastMove.priority;
    if (held == priority && priority != MovementPriority::MOVEMENT_IDLE)
        lastMove.priority = static_cast<MovementPriority>(static_cast<int>(priority) - 1);

    if (!MoveTo(_vehicleBase->GetMapId(), goal.GetPositionX(), goal.GetPositionY(), goal.GetPositionZ(), false, false,
                false, false, priority))
    {
        lastMove.priority = held;
        return false;
    }

    _issued = goal;
    _hasIssued = true;
    _parked = false;
    return true;
}

bool FlameLeviathanEnterVehicleAction::Execute(Event /*event*/)
{
    Unit* vehicleBase = FlameLeviathanVehicleToBoard(botAI);
    if (!vehicleBase)
        return false;

    if (bot->GetDistance(vehicleBase) > INTERACTION_DISTANCE)
        return MoveTo(vehicleBase, 0.0f,
                      FlameLeviathanEngaged(botAI) ? MovementPriority::MOVEMENT_COMBAT
                                                   : MovementPriority::MOVEMENT_NORMAL);

    botAI->RemoveShapeshift();
    vehicleBase->HandleSpellClick(bot);

    if (!bot->GetVehicle())
        return false;

    WorldPacket emptyPacket;
    bot->GetSession()->HandleCancelMountAuraOpcode(emptyPacket);
    return true;
}

bool FlameLeviathanSeatAction::Leave(char const* why)
{
    if (!RequestVehicleExit(bot))
        return false;

    if (RaidObs::Active())
        RaidObs::Note(bot, "fl.jump", why);

    return true;
}

bool FlameLeviathanSeatAction::Execute(Event /*event*/)
{
    Unit* seat = bot->GetVehicleBase();
    Unit* boss = seat && seat->GetEntry() == NPC_FL_SEAT ? seat->GetVehicleBase() : nullptr;
    if (!boss)
        return false;

    bool const stunned = FlameLeviathanShutdown(boss);
    if (stunned || !boss->IsAlive() || !boss->IsInCombat())
        return Leave(stunned ? "shutdown" : "fight over");

    if (Unit* turret = FlameLeviathanRiderTurret(bot))
        return Attack(turret);

    return FlameLeviathanRiderStranded(bot) && Leave("stranded");
}
