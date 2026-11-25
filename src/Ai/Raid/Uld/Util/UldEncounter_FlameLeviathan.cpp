/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_FlameLeviathan.h"

#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ObjectDefines.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidDifficulty.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "Timer.h"
#include "UldEncounterGate.h"
#include "UldScripts.h"
#include "UldTargets.h"
#include "Unit.h"
#include "Vehicle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <unordered_map>
#include <vector>

using namespace EncounterHelpers;

std::vector<Position> const ULDUAR_FL_ARENA_CORNERS = {
    Position(159.4f, 64.1f, 409.8f),
    Position(382.9f, 74.0f, 411.6f),
    Position(374.0f, -141.0f, 411.0f),
    Position(157.7f, -140.3f, 409.8f)
};

Unit* FlameLeviathanBoss(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    InstanceScript* instance = bot ? bot->GetInstanceScript() : nullptr;
    if (!instance)
        return botAI ? GetUldUnitByEntry(botAI, NPC_FLAME_LEVIATHAN) : nullptr;

    // Check the entry too, index 0 is some other boss in any instance that isn't Ulduar.
    Creature* boss = instance->GetCreature(ULD_BOSS_LEVIATHAN);
    return boss && boss->GetEntry() == NPC_FLAME_LEVIATHAN && boss->IsInWorld() && boss->IsAlive() ? boss : nullptr;
}

namespace
{
struct FlameLeviathanState
{
    bool pullTraced = false;

    RaidObs::ObsValue<ObjectGuid> ventClaimedBy{"fl.interrupter"};

    RaidObs::ObsValue<ObjectGuid> pursuedVehicle{"fl.pursued"};

    RaidObs::ObsValue<ObjectGuid> ramTarget{"fl.ramtarget"};

    uint32 ventStartMs = 0;
    bool ventChanneling = false;

    RaidObs::ObsGuidMap<bool> frozen{"fl.frozen"};

    RaidObs::ObsValue<ObjectGuid> ventReserve{"fl.reserve"};

    std::vector<ObjectGuid> siegeOrder;

    std::vector<ObjectGuid> furyReticles;
    std::unordered_map<ObjectGuid, uint32> furyStunnedMs;
    RaidObs::ObsGuidSet furyArmed{"fl.fury"};

    RaidObs::ObsGuidMap<uint32> seats{"fl.seat"};
    RaidObs::ObsGuidMap<ObjectGuid> origin{"fl.origin"};
    RaidObs::ObsGuidSet seatClaims{"fl.claim"};
    std::unordered_map<ObjectGuid, uint32> claimMs;
    RaidObs::ObsGuidMap<ObjectGuid> pickup{"fl.pickup"};
    std::unordered_map<ObjectGuid, uint32> pickupMs;
    RaidObs::ObsGuidSet cratesTaken{"fl.crate"};
    RaidObs::ObsValue<uint32> turrets{"fl.turrets"};
    RaidObs::ObsValue<bool> shutdown{"fl.shutdown"};
    bool shutdownSoon = false;

    std::unordered_map<ObjectGuid, FlameLeviathanSeat> plan;
    uint32 planMs = 0;

    uint32 scanMs = 0;
};

RaidInstanceState<FlameLeviathanState> flStates;

constexpr uint32 ULDUAR_FL_SCAN_INTERVAL_MS = 200;

FlameLeviathanState& FlameLeviathanStateFor(Player* bot) { return flStates.For(bot->GetInstanceId()); }

void TickHodirsFury(FlameLeviathanState& state, PlayerbotAI* botAI, Player* bot)
{
    if (!botAI)
        return;

    if (state.furyReticles.size() < 2)
    {
        for (auto const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
        {
            Unit* unit = botAI->GetUnit(guid);
            if (unit && unit->GetEntry() == NPC_FL_HODIRS_FURY_TARGET &&
                std::find(state.furyReticles.begin(), state.furyReticles.end(), guid) == state.furyReticles.end())
                state.furyReticles.push_back(guid);
        }
    }

    Map* map = bot->GetMap();
    uint32 const now = getMSTime();
    for (ObjectGuid const& guid : state.furyReticles)
    {
        Creature* reticle = map ? map->GetCreature(guid) : nullptr;
        if (!reticle || !reticle->IsAlive() || !reticle->HasUnitState(UNIT_STATE_STUNNED))
        {
            state.furyStunnedMs.erase(guid);
            state.furyArmed.erase(guid);
            continue;
        }

        auto const [it, fresh] = state.furyStunnedMs.try_emplace(guid, now);
        if (!fresh && getMSTimeDiff(it->second, now) >= ULDUAR_FL_FURY_STUN_MS)
            it->second = now;

        if (getMSTimeDiff(it->second, now) >= ULDUAR_FL_FURY_ARMED_MS)
        {
            state.furyArmed.erase(guid);
            continue;
        }

        if (state.furyArmed.count(guid))
            continue;

        state.furyArmed.insert(guid);
        if (RaidObs::Active())
            RaidObs::NoteHazardCircle(map, SPELL_FL_HODIRS_FURY_STUN, reticle->GetPosition(), ULDUAR_FL_FURY_RADIUS,
                                      ULDUAR_FL_FURY_ARMED_MS);
    }
}

Unit* FlameLeviathanSeatTurret(Unit* seat)
{
    Vehicle* kit = seat && seat->GetEntry() == NPC_FL_SEAT ? seat->GetVehicleKit() : nullptr;
    Unit* turret = kit ? kit->GetPassenger(ULDUAR_FL_SEAT_TURRET) : nullptr;
    return turret && turret->IsAlive() ? turret : nullptr;
}

uint32 FlameLeviathanSeatsNeedingRider(Unit* boss)
{
    Vehicle* kit = boss ? boss->GetVehicleKit() : nullptr;
    if (!kit)
        return 0;

    uint32 needed = 0;
    for (uint8 i = 0; i < ULDUAR_FL_LEVIATHAN_SEATS; ++i)
    {
        Unit* seat = kit->GetPassenger(i);
        Vehicle* seatKit = seat && seat->GetEntry() == NPC_FL_SEAT ? seat->GetVehicleKit() : nullptr;
        if (!seatKit || seatKit->GetPassenger(ULDUAR_FL_SEAT_RIDER) || !seatKit->GetPassenger(ULDUAR_FL_SEAT_DEVICE))
            continue;

        if (FlameLeviathanSeatTurret(seat))
            ++needed;
    }

    return needed;
}

void TickTurrets(FlameLeviathanState& state, Unit* boss)
{
    uint32 mask = 0;
    uint32 riders = 0;
    if (Vehicle* kit = boss->GetVehicleKit())
    {
        for (uint8 i = 0; i < ULDUAR_FL_LEVIATHAN_SEATS; ++i)
        {
            Unit* seat = kit->GetPassenger(i);
            Vehicle* seatKit = seat && seat->GetEntry() == NPC_FL_SEAT ? seat->GetVehicleKit() : nullptr;
            if (!seatKit)
                continue;

            if (seatKit->GetPassenger(ULDUAR_FL_SEAT_RIDER))
                ++riders;

            if (FlameLeviathanSeatTurret(seat))
                mask |= 1u << i;
        }
    }

    bool const down = FlameLeviathanShutdown(boss);
    state.turrets = mask;
    state.shutdown = down;

    uint32 live = 0;
    for (uint32 bits = mask; bits; bits &= bits - 1)
        ++live;

    state.shutdownSoon = !down && riders > 0 && live <= 1;
}

bool FlameLeviathanOnFootInArena(Player* member)
{
    return FlameLeviathanSeatOf(member) == FlameLeviathanSeat::OnFoot && !member->HasAura(SPELL_FL_HOOKSHOT_AURA) &&
           FlameLeviathanInArena(member->GetPosition(), ULDUAR_FL_ON_FOOT_ARENA_MARGIN);
}

Player* FlameLeviathanSeatedPlayer(Unit* hull, int8 seat)
{
    Vehicle* kit = hull ? hull->GetVehicleKit() : nullptr;
    Unit* passenger = kit ? kit->GetPassenger(seat) : nullptr;
    return passenger ? passenger->ToPlayer() : nullptr;
}

bool FlameLeviathanPickupValid(Player* bot, ObjectGuid chopperGuid, Creature*& hull)
{
    hull = bot->GetMap() ? bot->GetMap()->GetCreature(chopperGuid) : nullptr;
    if (!hull || !hull->IsAlive() || hull->GetEntry() != NPC_VEHICLE_CHOPPER)
        return false;

    Player* driver = FlameLeviathanSeatedPlayer(hull, 0);
    Vehicle* kit = hull->GetVehicleKit();
    return driver && kit && FlameLeviathanCrewUsable(driver) && !kit->GetPassenger(ULDUAR_FL_CHOPPER_REAR_SEAT);
}

void TickSeats(FlameLeviathanState& state, Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return;

    uint32 const now = getMSTime();
    std::vector<Player*> onFoot;
    std::vector<Unit*> choppers;
    std::vector<Unit*> gunnerless;

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member)
            continue;

        ObjectGuid const guid = member->GetGUID();
        FlameLeviathanSeat const kind = FlameLeviathanSeatOf(member);
        if (kind == FlameLeviathanSeat::None)
            state.seats.erase(guid);
        else
            state.seats.Set(guid, static_cast<uint32>(kind));

        if (kind == FlameLeviathanSeat::Catapult)
            if (Unit* demolisher = member->GetVehicleBase())
                state.origin.Set(guid, demolisher->GetGUID());

        if (kind == FlameLeviathanSeat::ChopperDriver && FlameLeviathanCrewUsable(member))
            choppers.push_back(member->GetVehicleBase());

        if (kind == FlameLeviathanSeat::DemolisherDriver && FlameLeviathanGunnerless(member->GetVehicleBase()))
            gunnerless.push_back(member->GetVehicleBase());

        if (state.seatClaims.count(guid))
        {
            uint32 const age = getMSTimeDiff(state.claimMs[guid], now);
            bool const flying = kind == FlameLeviathanSeat::OnFoot && member->HasAura(SPELL_FL_HOOKSHOT_AURA);
            bool const keep =
                kind == FlameLeviathanSeat::Catapult || flying || age < ULDUAR_FL_SEAT_CLAIM_GRACE_MS;
            if (!keep || age >= ULDUAR_FL_SEAT_CLAIM_MS)
            {
                state.seatClaims.erase(guid);
                state.claimMs.erase(guid);
            }
        }

        if (FlameLeviathanOnFootInArena(member) && !state.seatClaims.count(guid))
            onFoot.push_back(member);
        else if (state.pickup.count(guid))
        {
            state.pickup.erase(guid);
            state.pickupMs.erase(guid);
        }
    }

    std::sort(onFoot.begin(), onFoot.end(),
              [](Player* a, Player* b) { return a->GetGUID() < b->GetGUID(); });

    std::vector<ObjectGuid> taken;
    std::vector<Player*> unassigned;
    for (Player* member : onFoot)
    {
        ObjectGuid const guid = member->GetGUID();
        bool const regun = std::any_of(gunnerless.begin(), gunnerless.end(), [member](Unit* demolisher)
                                       { return member->GetExactDist2d(demolisher) <= ULDUAR_FL_REGUN_RADIUS; });

        auto const it = state.pickup.find(guid);
        Creature* hull = nullptr;
        bool const keep = !regun && it != state.pickup.end() && FlameLeviathanPickupValid(member, it->second, hull) &&
                          std::find(taken.begin(), taken.end(), it->second) == taken.end() &&
                          getMSTimeDiff(state.pickupMs[guid], now) < ULDUAR_FL_PICKUP_TIMEOUT_MS;
        if (keep)
        {
            taken.push_back(it->second);
            continue;
        }

        if (it != state.pickup.end())
        {
            state.pickup.erase(guid);
            state.pickupMs.erase(guid);
        }

        if (!regun)
            unassigned.push_back(member);
    }

    for (Player* member : unassigned)
    {
        Unit* best = nullptr;
        for (Unit* chopper : choppers)
        {
            if (!chopper || std::find(taken.begin(), taken.end(), chopper->GetGUID()) != taken.end())
                continue;

            Creature* hull = nullptr;
            if (!FlameLeviathanPickupValid(member, chopper->GetGUID(), hull))
                continue;

            if (!best || member->GetExactDist2d(chopper) < member->GetExactDist2d(best))
                best = chopper;
        }

        if (!best)
            continue;

        taken.push_back(best->GetGUID());
        state.pickup.Set(member->GetGUID(), best->GetGUID());
        state.pickupMs[member->GetGUID()] = now;
    }
}

Unit* FlameLeviathanHullOf(Unit* unit)
{
    for (Unit* each = unit; each; each = each->GetVehicleBase())
    {
        switch (each->GetEntry())
        {
            case NPC_SALVAGED_SIEGE_ENGINE:
            case NPC_SALVAGED_DEMOLISHER:
            case NPC_VEHICLE_CHOPPER:
                return each;
            default:
                break;
        }
    }

    return nullptr;
}

void ElectVentReserve(FlameLeviathanState& state, Player* bot, Unit* boss)
{
    Group* group = bot->GetGroup();
    if (!group || !boss->HasAura(SPELL_FL_TOWER_OF_LIFE))
    {
        state.ventReserve = ObjectGuid::Empty;
        return;
    }

    Unit* best = nullptr;
    float bestDist = 0.0f;
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member || !FlameLeviathanIsDriver(member))
            continue;

        Unit* hull = member->GetVehicleBase();
        if (!hull || hull->GetEntry() != NPC_SALVAGED_SIEGE_ENGINE)
            continue;

        if (!FlameLeviathanCrewUsable(member) || FlameLeviathanIsPursued(member))
            continue;

        if (hull->GetGUID() == state.ventReserve.Get())
            return;

        float const dist = hull->GetExactDist2d(boss);
        if (!best || dist < bestDist || (dist == bestDist && hull->GetGUID() < best->GetGUID()))
        {
            best = hull;
            bestDist = dist;
        }
    }

    state.ventReserve = best ? best->GetGUID() : ObjectGuid::Empty;
}

void TickFlameLeviathan(PlayerbotAI* botAI, Player* bot)
{
    FlameLeviathanState& state = FlameLeviathanStateFor(bot);
    if (state.scanMs && GetMSTimeDiffToNow(state.scanMs) < ULDUAR_FL_SCAN_INTERVAL_MS)
        return;

    state.scanMs = getMSTime();

    Unit* boss = FlameLeviathanBoss(botAI);

    if (!boss || !boss->IsInCombat())
    {
        state.pullTraced = false;
        state.ventClaimedBy = ObjectGuid::Empty;
        state.pursuedVehicle = ObjectGuid::Empty;
        state.ramTarget = ObjectGuid::Empty;
        state.ventReserve = ObjectGuid::Empty;
        state.ventStartMs = 0;
        state.ventChanneling = false;
        state.frozen.clear();
        state.siegeOrder.clear();
        state.furyReticles.clear();
        state.furyStunnedMs.clear();
        state.furyArmed.clear();
        state.seats.clear();
        state.origin.clear();
        state.seatClaims.clear();
        state.claimMs.clear();
        state.pickup.clear();
        state.pickupMs.clear();
        state.cratesTaken.clear();
        state.shutdown = false;
        state.shutdownSoon = false;
        return;
    }

    if (!state.pullTraced)
    {
        state.pullTraced = true;
        RaidObs::MarkPull(bot->GetMap(), boss);
    }

    bool const channeling = FlameLeviathanIsVentChanneling(boss);
    if (!channeling)
        state.ventClaimedBy = ObjectGuid::Empty;
    else if (!state.ventChanneling)
        state.ventStartMs = getMSTime();
    state.ventChanneling = channeling;

    TickHodirsFury(state, botAI, bot);

    ObjectGuid pursued;
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
        {
            Player* member = gref->GetSource();
            if (!member || !member->IsAlive())
                continue;

            Unit* base = FlameLeviathanRiddenVehicle(member);
            if (!base)
                continue;

            if (!pursued && base->HasAura(SPELL_FL_PURSUED))
                pursued = base->GetGUID();

            state.frozen.Set(base->GetGUID(), base->HasUnitState(UNIT_STATE_NOT_MOVE));

            if (state.ventClaimedBy.Get() == member->GetGUID() && !FlameLeviathanCrewUsable(member))
                state.ventClaimedBy = ObjectGuid::Empty;
        }
    }

    state.pursuedVehicle = pursued;

    ObjectGuid rammed;
    if (!pursued)
        if (Unit* hull = FlameLeviathanHullOf(boss->GetVictim()))
            rammed = hull->GetGUID();
    state.ramTarget = rammed;

    ElectVentReserve(state, bot, boss);

    TickTurrets(state, boss);
    TickSeats(state, bot);
}
}

void FlameLeviathanTick(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot && bot->GetMapId() == ULDUAR_MAP_ID)
        TickFlameLeviathan(botAI, bot);
}

bool FlameLeviathanEngaged(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot || bot->GetMapId() != ULDUAR_MAP_ID)
        return false;

    Unit* boss = FlameLeviathanBoss(botAI);
    if (!boss || boss->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE))
        return false;

    if (boss->IsInCombat())
        return true;

    return bot->IsInCombat();
}

Unit* FlameLeviathanRiddenVehicle(Player* bot)
{
    Unit* base = bot ? bot->GetVehicleBase() : nullptr;
    if (!base || base->GetEntry() == NPC_FL_SEAT)
        return nullptr;

    if (Unit* parent = base->GetVehicleBase())
        return parent;

    return base;
}

bool FlameLeviathanIsDriver(Player* bot)
{
    Vehicle* vehicle = bot ? bot->GetVehicle() : nullptr;
    Unit* base = vehicle ? vehicle->GetBase() : nullptr;
    if (!base)
        return false;

    uint32 const entry = base->GetEntry();
    if (entry != NPC_SALVAGED_SIEGE_ENGINE && entry != NPC_VEHICLE_CHOPPER && entry != NPC_SALVAGED_DEMOLISHER)
        return false;

    VehicleSeatEntry const* seat = vehicle->GetSeatForPassenger(bot);
    return seat && seat->CanControl();
}

FlameLeviathanSeat FlameLeviathanSeatOf(Player* member)
{
    if (!member || !member->IsAlive())
        return FlameLeviathanSeat::None;

    Vehicle* vehicle = member->GetVehicle();
    if (!vehicle)
        return FlameLeviathanSeat::OnFoot;

    Unit* base = vehicle->GetBase();
    if (!base)
        return FlameLeviathanSeat::None;

    int8 const seat = member->GetTransSeat();
    switch (base->GetEntry())
    {
        case NPC_SALVAGED_SIEGE_ENGINE:
            return seat == 0 ? FlameLeviathanSeat::SiegeDriver : FlameLeviathanSeat::None;
        case NPC_SALVAGED_SIEGE_ENGINE_TURRET:
            return FlameLeviathanSeat::SiegeGunner;
        case NPC_SALVAGED_DEMOLISHER:
            if (seat == 0)
                return FlameLeviathanSeat::DemolisherDriver;
            return seat == ULDUAR_FL_DEMOLISHER_CATAPULT_SEAT ? FlameLeviathanSeat::Catapult : FlameLeviathanSeat::None;
        case NPC_SALVAGED_DEMOLISHER_TURRET:
            return FlameLeviathanSeat::DemolisherGunner;
        case NPC_VEHICLE_CHOPPER:
            if (seat == 0)
                return FlameLeviathanSeat::ChopperDriver;
            return seat == ULDUAR_FL_CHOPPER_REAR_SEAT ? FlameLeviathanSeat::ChopperPassenger : FlameLeviathanSeat::None;
        case NPC_FL_SEAT:
            return FlameLeviathanSeat::LeviathanSeat;
        default:
            return FlameLeviathanSeat::None;
    }
}

namespace
{
constexpr std::array<FlameLeviathanSeat, 5> BOARDING_SEATS = {
    FlameLeviathanSeat::SiegeDriver, FlameLeviathanSeat::DemolisherDriver, FlameLeviathanSeat::ChopperDriver,
    FlameLeviathanSeat::DemolisherGunner, FlameLeviathanSeat::SiegeGunner};

enum class SeatRole : uint8
{
    Tank,
    Heal,
    RangedDps,
    MeleeDps
};

SeatRole FlameLeviathanSeatRole(Player* member)
{
    if (PlayerbotAI::IsTank(member, true))
        return SeatRole::Tank;

    if (PlayerbotAI::IsHeal(member, true))
        return SeatRole::Heal;

    if (PlayerbotAI::IsRangedDps(member, true))
        return SeatRole::RangedDps;

    return SeatRole::MeleeDps;
}

bool IsBoardingSeat(FlameLeviathanSeat kind)
{
    return std::find(BOARDING_SEATS.begin(), BOARDING_SEATS.end(), kind) != BOARDING_SEATS.end();
}

constexpr uint32 ULDUAR_FL_PLAN_CACHE_MS = 1000;

void FlameLeviathanDealSeats(Player* bot, std::unordered_map<ObjectGuid, FlameLeviathanSeat>& dealt)
{
    dealt.clear();
    Group* group = bot->GetGroup();
    if (!group)
        return;

    int32 const perKind = IsRaid25Man(bot) ? 5 : 2;
    std::array<int32, 10> capacity{};
    for (FlameLeviathanSeat kind : BOARDING_SEATS)
        capacity[static_cast<uint8>(kind)] = perKind;

    std::vector<std::pair<Player*, SeatRole>> open;
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        FlameLeviathanSeat const kind = FlameLeviathanSeatOf(member);
        if (IsBoardingSeat(kind))
        {
            --capacity[static_cast<uint8>(kind)];
            continue;
        }

        if (kind == FlameLeviathanSeat::OnFoot && GET_PLAYERBOT_AI(member))
            open.emplace_back(member, FlameLeviathanSeatRole(member));
    }

    std::sort(open.begin(), open.end(), [](auto const& a, auto const& b) { return a.first->GetGUID() < b.first->GetGUID(); });

    std::vector<FlameLeviathanSeat> plan(open.size(), FlameLeviathanSeat::None);
    auto const take = [&](FlameLeviathanSeat kind, std::optional<SeatRole> role)
    {
        int32& left = capacity[static_cast<uint8>(kind)];
        for (size_t i = 0; i < open.size() && left > 0; ++i)
        {
            if (plan[i] != FlameLeviathanSeat::None || (role && open[i].second != *role))
                continue;

            plan[i] = kind;
            --left;
        }
    };

    take(FlameLeviathanSeat::DemolisherGunner, SeatRole::RangedDps);
    take(FlameLeviathanSeat::SiegeDriver, SeatRole::Tank);
    take(FlameLeviathanSeat::SiegeDriver, SeatRole::MeleeDps);
    take(FlameLeviathanSeat::DemolisherDriver, SeatRole::Heal);
    take(FlameLeviathanSeat::ChopperDriver, SeatRole::Heal);
    take(FlameLeviathanSeat::SiegeGunner, SeatRole::MeleeDps);
    for (FlameLeviathanSeat kind : BOARDING_SEATS)
        take(kind, std::nullopt);

    for (size_t i = 0; i < open.size(); ++i)
        dealt[open[i].first->GetGUID()] = plan[i];
}

FlameLeviathanSeat FlameLeviathanPlannedSeat(Player* bot)
{
    FlameLeviathanState& state = FlameLeviathanStateFor(bot);
    uint32 const now = getMSTime();
    if (!state.planMs || getMSTimeDiff(state.planMs, now) >= ULDUAR_FL_PLAN_CACHE_MS ||
        !state.plan.count(bot->GetGUID()))
    {
        FlameLeviathanDealSeats(bot, state.plan);
        state.planMs = now;
    }

    auto const it = state.plan.find(bot->GetGUID());
    return it != state.plan.end() ? it->second : FlameLeviathanSeat::None;
}

bool FlameLeviathanHullOpenFor(Unit* hull, FlameLeviathanSeat kind, bool& waiting)
{
    Vehicle* kit = hull->GetVehicleKit();
    if (!kit)
        return false;

    if (!kit->IsVehicleInUse() && !hull->HasNpcFlag(UNIT_NPC_FLAG_SPELLCLICK))
        return false;

    uint32 const entry = hull->GetEntry();
    bool const driven = kit->GetPassenger(0) != nullptr;
    switch (kind)
    {
        case FlameLeviathanSeat::SiegeDriver:
            return entry == NPC_SALVAGED_SIEGE_ENGINE && !driven;
        case FlameLeviathanSeat::DemolisherDriver:
            return entry == NPC_SALVAGED_DEMOLISHER && !driven;
        case FlameLeviathanSeat::ChopperDriver:
            return entry == NPC_VEHICLE_CHOPPER && !driven;
        case FlameLeviathanSeat::ChopperPassenger:
            return entry == NPC_VEHICLE_CHOPPER && driven && !kit->GetPassenger(ULDUAR_FL_CHOPPER_REAR_SEAT);
        case FlameLeviathanSeat::SiegeGunner:
        {
            if (entry != NPC_SALVAGED_SIEGE_ENGINE)
                return false;

            Unit* turret = kit->GetPassenger(7);
            Vehicle* turretKit = turret ? turret->GetVehicleKit() : nullptr;
            if (!turretKit || turretKit->IsVehicleInUse())
                return false;

            waiting = waiting || !driven;
            return driven;
        }
        case FlameLeviathanSeat::DemolisherGunner:
        {
            if (entry != NPC_SALVAGED_DEMOLISHER)
                return false;

            Unit* seat = kit->GetPassenger(ULDUAR_FL_DEMOLISHER_GUNNER_SEAT);
            Vehicle* seatKit = seat ? seat->GetVehicleKit() : nullptr;
            if (!seatKit || seatKit->GetPassenger(0) || kit->GetPassenger(ULDUAR_FL_DEMOLISHER_CATAPULT_SEAT))
                return false;

            waiting = waiting || !driven;
            return driven;
        }
        default:
            return false;
    }
}
}

Unit* FlameLeviathanVehicleToBoard(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || bot->GetMapId() != ULDUAR_MAP_ID || !bot->IsAlive() || bot->GetVehicle() ||
        bot->HasAura(SPELL_FL_HOOKSHOT_AURA) ||
        bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == EFFECT_MOTION_TYPE)
        return nullptr;

    std::vector<Unit*> hulls;
    for (ObjectGuid const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest vehicles far")->Get())
    {
        Unit* hull = botAI->GetUnit(guid);
        if (!hull || !hull->IsAlive() || hull->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            continue;

        uint32 const entry = hull->GetEntry();
        if (entry == NPC_SALVAGED_DEMOLISHER || entry == NPC_SALVAGED_SIEGE_ENGINE || entry == NPC_VEHICLE_CHOPPER)
            hulls.push_back(hull);
    }

    auto const nearest = [bot, &hulls](FlameLeviathanSeat kind, float maxDist, bool& waiting) -> Unit*
    {
        Unit* best = nullptr;
        for (Unit* hull : hulls)
        {
            if (bot->GetExactDist(hull) > maxDist || !FlameLeviathanHullOpenFor(hull, kind, waiting))
                continue;

            if (!best || bot->GetExactDist(hull) < bot->GetExactDist(best))
                best = hull;
        }

        return best;
    };

    if (hulls.empty() && !FlameLeviathanPickupChopper(botAI, bot))
        return nullptr;

    float const anywhere = std::numeric_limits<float>::max();
    bool waiting = false;
    if (FlameLeviathanEngaged(botAI))
    {
        if (Unit* hull = nearest(FlameLeviathanSeat::DemolisherGunner, ULDUAR_FL_REGUN_RADIUS, waiting))
            return hull;

        if (Unit* chopper = FlameLeviathanPickupChopper(botAI, bot))
            return chopper;
    }
    else if (FlameLeviathanSeat const planned = FlameLeviathanPlannedSeat(bot); planned != FlameLeviathanSeat::None)
    {
        if (Unit* hull = nearest(planned, anywhere, waiting))
            return hull;

        if (waiting)
            return nullptr;
    }

    for (FlameLeviathanSeat kind : BOARDING_SEATS)
    {
        bool unused = false;
        if (Unit* hull = nearest(kind, anywhere, unused))
            return hull;
    }

    return nullptr;
}

bool FlameLeviathanIsPursued(Player* bot)
{
    Unit* base = bot ? bot->GetVehicleBase() : nullptr;
    if (!base)
        return false;

    if (base->HasAura(SPELL_FL_PURSUED))
        return true;

    Unit* parent = base->GetVehicleBase();
    return parent && parent->HasAura(SPELL_FL_PURSUED);
}

bool FlameLeviathanIsVentChanneling(Unit* boss)
{
    if (!boss)
        return false;

    Spell* channel = boss->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
    return channel && channel->m_spellInfo && channel->m_spellInfo->Id == SPELL_FL_FLAME_VENTS;
}

static bool FlameLeviathanInConeRange(Unit* caster, Unit* target, float radius)
{
    return caster && target && caster->GetExactDist(target) <= radius + target->GetObjectSize();
}

static bool FlameLeviathanCanElectroshock(Unit* siegeEngine, Unit* boss)
{
    return siegeEngine && boss && !siegeEngine->HasSpellCooldown(SPELL_FL_ELECTROSHOCK) &&
           siegeEngine->GetPower(POWER_ENERGY) >= ULDUAR_FL_ELECTROSHOCK_COST &&
           FlameLeviathanInConeRange(siegeEngine, boss, ULDUAR_FL_ELECTROSHOCK_CONE_RADIUS);
}

bool FlameLeviathanInCone(Unit* vehicleBase, Unit* target, float halfAngle, float radius)
{
    return vehicleBase && target && FlameLeviathanInConeRange(vehicleBase, target, radius) &&
           vehicleBase->HasInArc(halfAngle * 2.0f, target);
}

bool FlameLeviathanFaceForCone(Unit* vehicleBase, Unit* target, float halfAngle, float radius)
{
    if (!vehicleBase || !target)
        return false;

    if (!FlameLeviathanInConeRange(vehicleBase, target, radius))
        return false;

    if (vehicleBase->HasInArc(halfAngle * 2.0f, target))
        return true;

    vehicleBase->SetFacingToObject(target);
    return false;
}

bool FlameLeviathanIsVentInterrupter(PlayerbotAI* botAI, Player* bot)
{
    Unit* base = bot ? bot->GetVehicleBase() : nullptr;
    if (!base || base->GetEntry() != NPC_SALVAGED_SIEGE_ENGINE)
        return false;

    Unit* boss = FlameLeviathanBoss(botAI);
    if (!FlameLeviathanCanElectroshock(base, boss))
        return false;

    if (!FlameLeviathanCrewUsable(bot) || FlameLeviathanIsPursued(bot))
        return false;

    ObjectGuid const claimed = FlameLeviathanStateFor(bot).ventClaimedBy.Get();
    if (claimed)
        return claimed == bot->GetGUID();

    Group* group = bot->GetGroup();
    if (!group)
        return true;

    uint32 const myEnergy = base->GetPower(POWER_ENERGY);
    ObjectGuid const myGuid = bot->GetGUID();

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member || member == bot || !FlameLeviathanCrewUsable(member) || FlameLeviathanIsPursued(member))
            continue;

        Unit* memberBase = member->GetVehicleBase();
        if (!memberBase || memberBase->GetEntry() != NPC_SALVAGED_SIEGE_ENGINE)
            continue;

        if (!FlameLeviathanCanElectroshock(memberBase, boss))
            continue;

        uint32 const energy = memberBase->GetPower(POWER_ENERGY);

        if (energy > myEnergy || (energy == myEnergy && member->GetGUID() < myGuid))
            return false;
    }

    return true;
}

void FlameLeviathanClaimVentChannel(Player* bot)
{
    if (bot)
        FlameLeviathanStateFor(bot).ventClaimedBy = bot->GetGUID();
}

uint32 FlameLeviathanMsToNextVent(Player* bot, Unit* boss)
{
    if (FlameLeviathanIsVentChanneling(boss))
        return 0;

    uint32 const start = bot ? FlameLeviathanStateFor(bot).ventStartMs : 0;
    if (!start)
        return std::numeric_limits<uint32>::max();

    return ULDUAR_FL_VENT_INTERVAL_MS - getMSTimeDiff(start, getMSTime()) % ULDUAR_FL_VENT_INTERVAL_MS;
}

bool FlameLeviathanVentWindowOpen(Player* bot, Unit* boss)
{
    uint32 const toVent = FlameLeviathanMsToNextVent(bot, boss);
    return toVent == 0 || toVent <= ULDUAR_FL_VENT_RUSH_LEAD_MS;
}

bool FlameLeviathanCrewUsable(Player* member)
{
    if (!member || !member->IsAlive())
        return false;

    Unit* base = FlameLeviathanRiddenVehicle(member);
    return base && !base->HasUnitState(UNIT_STATE_NOT_MOVE) && !member->HasUnitState(UNIT_STATE_STUNNED);
}

Unit* FlameLeviathanFrozenVehicle(Player* bot, Unit* from, float minRange, float maxRange)
{
    Group* group = bot ? bot->GetGroup() : nullptr;
    if (!group || !from)
        return nullptr;

    Unit* best = nullptr;
    float bestDist = maxRange;

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member || !member->IsAlive())
            continue;

        Unit* base = FlameLeviathanRiddenVehicle(member);
        if (!base || base == from || !base->HasAura(SPELL_FL_HODIRS_FURY_STUN))
            continue;

        float const dist = from->GetExactDist2d(base);
        if (dist < minRange || dist > bestDist)
            continue;

        best = base;
        bestDist = dist;
    }

    return best;
}

static bool FlameLeviathanHeldByAnotherPost(Unit* add, Unit* from)
{
    Unit* victim = add->GetVictim();
    if (!victim || victim->GetEntry() != NPC_SALVAGED_SIEGE_ENGINE)
        return false;

    if (from == victim || from->GetVehicleBase() == victim)
        return false;

    bool posted = false;
    for (uint8 i = 0; i < ULDUAR_FL_ARENA_CORNERS.size(); ++i)
        if (victim->GetExactDist2d(FlameLeviathanCornerPostPoint(i)) <= ULDUAR_FL_CORNER_HOLD_RADIUS)
        {
            posted = true;
            break;
        }

    if (!posted)
        return false;

    if (add->GetExactDist2d(victim) >= ULDUAR_FL_FIRE_CANNON_MIN_RANGE)
        return true;

    return FlameLeviathanInCone(victim, add, ULDUAR_FL_RAM_CONE_HALF_ANGLE, ULDUAR_FL_RAM_CONE_RADIUS);
}

Unit* FlameLeviathanBestAdd(PlayerbotAI* botAI, Unit* from, float minRange, float maxRange, float splash,
                            Position const* around, float aroundRadius)
{
    if (!botAI || !from)
        return nullptr;

    std::vector<Unit*> adds;
    for (auto const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        uint32 const entry = unit->GetEntry();
        if (entry == NPC_FL_WRITHING_LASHER || entry == NPC_FL_WARD_OF_LIFE)
            adds.push_back(unit);
    }

    Unit* best = nullptr;
    uint32 bestCluster = 0;
    float bestDist = 0.0f;

    for (Unit* add : adds)
    {
        float const dist = from->GetExactDist2d(add);
        if (dist < minRange || dist > maxRange)
            continue;

        if (around && add->GetExactDist2d(*around) > aroundRadius)
            continue;

        if (FlameLeviathanHeldByAnotherPost(add, from))
            continue;

        uint32 cluster = 0;
        if (splash > 0.0f)
            for (Unit* other : adds)
                if (add->GetExactDist2d(other) <= splash)
                    ++cluster;

        if (!best || cluster > bestCluster || (cluster == bestCluster && dist < bestDist))
        {
            best = add;
            bestCluster = cluster;
            bestDist = dist;
        }
    }

    return best;
}

Unit* FlameLeviathanPursuedVehicle(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot)
        return nullptr;

    ObjectGuid const guid = FlameLeviathanStateFor(bot).pursuedVehicle.Get();
    return guid ? botAI->GetUnit(guid) : nullptr;
}

Unit* FlameLeviathanRamCentre(PlayerbotAI* botAI, Player* bot)
{
    if (Unit* pursued = FlameLeviathanPursuedVehicle(botAI, bot))
        return pursued;

    ObjectGuid const guid = botAI && bot ? FlameLeviathanStateFor(bot).ramTarget.Get() : ObjectGuid::Empty;
    return guid ? botAI->GetUnit(guid) : nullptr;
}

bool FlameLeviathanIsRamTarget(Player* bot)
{
    Unit* hull = FlameLeviathanRiddenVehicle(bot);
    ObjectGuid const target = hull ? FlameLeviathanStateFor(bot).ramTarget.Get() : ObjectGuid::Empty;
    return target && hull->GetGUID() == target;
}

bool FlameLeviathanInBatteringRamBlast(Unit* vehicleBase, Unit* centre)
{
    if (!vehicleBase || !centre || vehicleBase == centre)
        return false;

    return vehicleBase->GetExactDist2d(centre) <= ULDUAR_FL_BATTERING_RAM_RADIUS + vehicleBase->GetObjectSize();
}

bool FlameLeviathanShouldClearBatteringRam(PlayerbotAI* botAI, Player* bot)
{
    if (!bot || FlameLeviathanIsPursued(bot) || FlameLeviathanIsRamTarget(bot))
        return false;

    Unit* vehicleBase = FlameLeviathanRiddenVehicle(bot);
    Unit* centre = FlameLeviathanRamCentre(botAI, bot);
    Unit* boss = FlameLeviathanBoss(botAI);
    if (!vehicleBase || !centre || !boss || FlameLeviathanShutdown(boss))
        return false;

    if (!boss->IsWithinCombatRange(centre, ULDUAR_FL_BATTERING_RAM_CAST_RANGE))
        return false;

    return FlameLeviathanInBatteringRamBlast(vehicleBase, centre);
}

namespace
{
bool FlameLeviathanChopperOnDuty(Player* driver)
{
    return FlameLeviathanChopperPassenger(driver->GetVehicleBase()) || FlameLeviathanPickupFor(driver);
}
}

bool FlameLeviathanIsTarLead(PlayerbotAI* /*botAI*/, Player* bot)
{
    Unit* base = bot ? bot->GetVehicleBase() : nullptr;
    if (!base || base->GetEntry() != NPC_VEHICLE_CHOPPER || !FlameLeviathanIsDriver(bot))
        return false;

    if (FlameLeviathanIsPursued(bot))
        return false;

    if (!FlameLeviathanCrewUsable(bot) || FlameLeviathanChopperOnDuty(bot))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return true;

    ObjectGuid const myGuid = bot->GetGUID();
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member || member == bot || !FlameLeviathanCrewUsable(member) || !FlameLeviathanIsDriver(member))
            continue;

        Unit* memberBase = member->GetVehicleBase();
        if (!memberBase || memberBase->GetEntry() != NPC_VEHICLE_CHOPPER)
            continue;

        if (FlameLeviathanIsPursued(member) || FlameLeviathanChopperOnDuty(member))
            continue;

        if (member->GetGUID() < myGuid)
            return false;
    }

    return true;
}

static int32 FlameLeviathanSiegeRank(Player* bot)
{
    if (!bot || !FlameLeviathanIsDriver(bot))
        return -1;

    Unit* base = bot->GetVehicleBase();
    if (!base || base->GetEntry() != NPC_SALVAGED_SIEGE_ENGINE)
        return -1;

    FlameLeviathanState& state = FlameLeviathanStateFor(bot);
    ObjectGuid const myGuid = base->GetGUID();

    if (state.siegeOrder.empty())
    {
        if (Group* group = bot->GetGroup())
        {
            for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
            {
                Player* member = gref->GetSource();
                if (!member || !FlameLeviathanIsDriver(member))
                    continue;

                Unit* memberBase = member->GetVehicleBase();
                if (!memberBase || memberBase->GetEntry() != NPC_SALVAGED_SIEGE_ENGINE)
                    continue;

                state.siegeOrder.push_back(memberBase->GetGUID());
            }
        }

        std::sort(state.siegeOrder.begin(), state.siegeOrder.end());
    }

    auto const it = std::find(state.siegeOrder.begin(), state.siegeOrder.end(), myGuid);
    if (it == state.siegeOrder.end())
    {
        state.siegeOrder.push_back(myGuid);
        return static_cast<int32>(state.siegeOrder.size()) - 1;
    }

    return static_cast<int32>(std::distance(state.siegeOrder.begin(), it));
}

int8 FlameLeviathanCornerPost(PlayerbotAI* botAI, Player* bot)
{
    Unit* boss = FlameLeviathanBoss(botAI);
    if (!bot || !boss || !boss->HasAura(SPELL_FL_TOWER_OF_LIFE))
        return -1;

    if (FlameLeviathanIsPursued(bot) || !FlameLeviathanCrewUsable(bot))
        return -1;

    if (FlameLeviathanIsVentReserve(bot))
        return -1;

    int32 rank = FlameLeviathanSiegeRank(bot);
    if (rank < 0)
        return -1;

    if (rank == 0)
    {
        FlameLeviathanState const& state = FlameLeviathanStateFor(bot);
        auto const it = std::find(state.siegeOrder.begin(), state.siegeOrder.end(), state.ventReserve.Get());
        if (it == state.siegeOrder.end())
            return -1;

        rank = static_cast<int32>(std::distance(state.siegeOrder.begin(), it));
    }

    if (rank <= 0 || rank > static_cast<int32>(ULDUAR_FL_ARENA_CORNERS.size()))
        return -1;

    return static_cast<int8>(rank - 1);
}

int8 FlameLeviathanHullCornerPost(PlayerbotAI* botAI, Player* bot, Unit* hull)
{
    Group* group = bot ? bot->GetGroup() : nullptr;
    if (!group || !hull)
        return -1;

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (member && member->GetVehicleBase() == hull && FlameLeviathanIsDriver(member))
            return FlameLeviathanCornerPost(botAI, member);
    }

    return -1;
}

bool FlameLeviathanIsVentReserve(Player* bot)
{
    Unit* hull = bot && FlameLeviathanIsDriver(bot) ? bot->GetVehicleBase() : nullptr;
    ObjectGuid const reserve = bot ? FlameLeviathanStateFor(bot).ventReserve.Get() : ObjectGuid::Empty;
    return hull && reserve && hull->GetGUID() == reserve;
}

Position FlameLeviathanArenaCentre()
{
    float centreX = 0.0f;
    float centreY = 0.0f;
    for (Position const& each : ULDUAR_FL_ARENA_CORNERS)
    {
        centreX += each.GetPositionX();
        centreY += each.GetPositionY();
    }

    float const count = static_cast<float>(ULDUAR_FL_ARENA_CORNERS.size());
    return Position(centreX / count, centreY / count, ULDUAR_FL_ARENA_CORNERS.front().GetPositionZ());
}

Position FlameLeviathanCornerPostPoint(uint8 index)
{
    Position const& corner = ULDUAR_FL_ARENA_CORNERS[index % ULDUAR_FL_ARENA_CORNERS.size()];
    Position const centre = FlameLeviathanArenaCentre();

    float dx = centre.GetPositionX() - corner.GetPositionX();
    float dy = centre.GetPositionY() - corner.GetPositionY();
    float const len = std::sqrt(dx * dx + dy * dy);
    if (len > 0.0f)
    {
        dx /= len;
        dy /= len;
    }

    return Position(corner.GetPositionX() + dx * ULDUAR_FL_CORNER_STANDOFF,
                    corner.GetPositionY() + dy * ULDUAR_FL_CORNER_STANDOFF, corner.GetPositionZ());
}

float FlameLeviathanDemolisherStandDist(Unit* boss)
{
    return std::min(ULDUAR_FL_DEMOLISHER_BAND,
                    ULDUAR_FL_HURL_BOULDER_MAX_RANGE - boss->GetCombatReach() - 2.0f * ULDUAR_FL_ARRIVE_TOLERANCE);
}

namespace
{
uint32 FlameLeviathanHazardTower(uint32 entry)
{
    switch (entry)
    {
        case NPC_FL_THORIM_HAMMER_TARGET:
            return FL_TOWER_STORM;
        case NPC_FL_MIMIRONS_INFERNO_TARGET:
        case NPC_FL_MIMIRONS_INFERNO:
            return FL_TOWER_FLAMES;
        case NPC_FL_HODIRS_FURY_TARGET:
            return FL_TOWER_FROST;
        default:
            return 0;
    }
}

bool IsFlameLeviathanTowerHazard(uint32 entry, uint32 towerMask)
{
    return ((towerMask & FL_TOWER_STORM) && entry == NPC_FL_THORIM_HAMMER_TARGET) ||
           ((towerMask & FL_TOWER_FLAMES) &&
            (entry == NPC_FL_MIMIRONS_INFERNO_TARGET || entry == NPC_FL_MIMIRONS_INFERNO)) ||
           ((towerMask & FL_TOWER_FROST) && entry == NPC_FL_HODIRS_FURY_TARGET);
}

template <typename Visit>
void ForEachTowerHazard(PlayerbotAI* botAI, uint32 towerMask, Visit visit)
{
    Player* bot = botAI->GetBot();
    for (auto const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || !IsFlameLeviathanTowerHazard(unit->GetEntry(), towerMask))
            continue;

        if (unit->GetEntry() == NPC_FL_HODIRS_FURY_TARGET && !FlameLeviathanFuryArmed(bot, unit))
            continue;

        visit(unit);
    }
}
}

float FlameLeviathanHazardReach(Unit* hazard, Unit* vehicle)
{
    switch (hazard->GetEntry())
    {
        case NPC_FL_MIMIRONS_INFERNO:
        case NPC_FL_MIMIRONS_INFERNO_TARGET:
            return ULDUAR_FL_INFERNO_RADIUS + DEFAULT_WORLD_OBJECT_SIZE + (vehicle ? vehicle->GetObjectSize() : 0.0f);
        case NPC_FL_HODIRS_FURY_TARGET:
            return ULDUAR_FL_FURY_RADIUS;
        default:
            return ULDUAR_FL_HAMMER_RADIUS;
    }
}

bool FlameLeviathanFuryArmed(Player* bot, Unit* reticle)
{
    return bot && reticle && FlameLeviathanStateFor(bot).furyArmed.count(reticle->GetGUID());
}

Unit* GetFlameLeviathanNearestTowerHazard(PlayerbotAI* botAI, Unit* from, uint32 towerMask, float margin,
                                          uint32 strictMask)
{
    if (!botAI || !from)
        return nullptr;

    Unit* nearest = nullptr;
    float best = 0.0f;

    ForEachTowerHazard(botAI, towerMask,
                       [&](Unit* hazard)
                       {
                           float const gap = from->GetExactDist2d(hazard) - FlameLeviathanHazardReach(hazard, from);
                           float const pad =
                               (strictMask & FlameLeviathanHazardTower(hazard->GetEntry())) ? 0.0f : margin;
                           if (gap < pad && (!nearest || gap < best))
                           {
                               best = gap;
                               nearest = hazard;
                           }
                       });

    return nearest;
}

void GetFlameLeviathanTowerHazards(PlayerbotAI* botAI, Unit* from, uint32 towerMask, float radius,
                                   std::vector<Unit*>& out)
{
    out.clear();
    if (!botAI || !from)
        return;

    ForEachTowerHazard(botAI, towerMask,
                       [&](Unit* hazard)
                       {
                           if (from->GetExactDist2d(hazard) <= radius)
                               out.push_back(hazard);
                       });
}

bool FlameLeviathanInArena(Position const& pos, float margin)
{
    float minX = ULDUAR_FL_ARENA_CORNERS[0].GetPositionX();
    float maxX = minX;
    float minY = ULDUAR_FL_ARENA_CORNERS[0].GetPositionY();
    float maxY = minY;

    for (Position const& corner : ULDUAR_FL_ARENA_CORNERS)
    {
        minX = std::min(minX, corner.GetPositionX());
        maxX = std::max(maxX, corner.GetPositionX());
        minY = std::min(minY, corner.GetPositionY());
        maxY = std::max(maxY, corner.GetPositionY());
    }

    return pos.GetPositionX() >= minX - margin && pos.GetPositionX() <= maxX + margin &&
           pos.GetPositionY() >= minY - margin && pos.GetPositionY() <= maxY + margin;
}

static Position FlameLeviathanOffsetPoint(Unit* boss, float bearing, float standDist)
{
    float const dist = boss->GetCombatReach() + standDist;
    return Position(boss->GetPositionX() + std::cos(bearing) * dist,
                    boss->GetPositionY() + std::sin(bearing) * dist, boss->GetPositionZ());
}

float FlameLeviathanStationBearingOffset(Player* bot, Unit* vehicleBase, float radius, float* fanHalf)
{
    if (fanHalf)
        *fanHalf = 0.0f;

    Group* group = bot ? bot->GetGroup() : nullptr;
    if (!group || !vehicleBase || radius <= 0.0f)
        return 0.0f;

    uint32 const entry = vehicleBase->GetEntry();
    ObjectGuid const myGuid = vehicleBase->GetGUID();

    std::vector<ObjectGuid> seen;
    int32 index = 0;
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member || !member->IsAlive())
            continue;

        Unit* base = FlameLeviathanRiddenVehicle(member);
        if (!base || base->GetEntry() != entry)
            continue;

        ObjectGuid const guid = base->GetGUID();
        if (std::find(seen.begin(), seen.end(), guid) != seen.end())
            continue;

        seen.push_back(guid);
        if (guid < myGuid)
            ++index;
    }

    int32 const count = static_cast<int32>(seen.size());
    if (count < 2)
        return 0.0f;

    float spread = 2.0f * std::asin(std::min(1.0f, ULDUAR_FL_STATION_SPACING / (2.0f * radius)));

    spread = std::min(spread, ULDUAR_FL_STATION_MAX_ARC / static_cast<float>(count - 1));

    if (fanHalf)
        *fanHalf = static_cast<float>(count - 1) * 0.5f * spread;

    return (static_cast<float>(index) - static_cast<float>(count - 1) * 0.5f) * spread;
}

float FlameLeviathanCentreTurn(Unit* boss, float fanHalf)
{
    Position const centre = FlameLeviathanArenaCentre();
    if (!boss || boss->GetExactDist2d(centre) <= ULDUAR_FL_CENTRE_TURN_DEADZONE)
        return 0.0f;

    float turn = Position::NormalizeOrientation(boss->GetAngle(centre.GetPositionX(), centre.GetPositionY()) -
                                                (boss->GetOrientation() + float(M_PI)));
    if (turn > float(M_PI))
        turn -= 2.0f * float(M_PI);

    float const cap = std::max(0.0f, float(M_PI) / 2.0f - fanHalf);
    return std::clamp(turn, -cap, cap);
}

float FlameLeviathanStationLead(Unit* boss)
{
    if (!boss || !boss->isMoving())
        return 0.0f;

    return std::min(boss->GetSpeed(MOVE_RUN) * ULDUAR_FL_STATION_LEAD_S, ULDUAR_FL_STATION_LEAD_MAX);
}

Position FlameLeviathanRearPoint(Unit* boss, float standDist, float bearingOffset, float lead)
{
    Position point = FlameLeviathanOffsetPoint(boss, boss->GetOrientation() + M_PI + bearingOffset, standDist);
    if (lead <= 0.0f)
        return point;

    point.Relocate(point.GetPositionX() + std::cos(boss->GetOrientation()) * lead,
                   point.GetPositionY() + std::sin(boss->GetOrientation()) * lead, point.GetPositionZ());
    return point;
}

Position FlameLeviathanLeadPoint(Unit* boss, float standDist)
{
    return FlameLeviathanOffsetPoint(boss, boss->GetOrientation(), standDist);
}

float FlameLeviathanTarLeadDistance(Unit* boss, Unit* pursued, float size)
{
    if (!boss)
        return 0.0f;

    if (!pursued)
        return ULDUAR_FL_TAR_LEAD_DIST;

    float const room = boss->GetExactDist2d(pursued) - boss->GetCombatReach() -
                       ULDUAR_FL_BATTERING_RAM_RADIUS - size;

    return room <= 0.0f ? 0.0f : std::min(ULDUAR_FL_TAR_LEAD_DIST, room);
}

std::vector<Position> const& FlameLeviathanKiteRing()
{
    static std::vector<Position> const ring = []
    {
        std::vector<Position> nodes;
        size_t const count = ULDUAR_FL_ARENA_CORNERS.size();

        float centreX = 0.0f;
        float centreY = 0.0f;
        for (Position const& corner : ULDUAR_FL_ARENA_CORNERS)
        {
            centreX += corner.GetPositionX();
            centreY += corner.GetPositionY();
        }
        centreX /= static_cast<float>(count);
        centreY /= static_cast<float>(count);

        std::vector<Position> inset;
        inset.reserve(count);
        for (Position const& corner : ULDUAR_FL_ARENA_CORNERS)
        {
            float dx = centreX - corner.GetPositionX();
            float dy = centreY - corner.GetPositionY();
            float const len = std::sqrt(dx * dx + dy * dy);
            if (len > 0.0f)
            {
                dx /= len;
                dy /= len;
            }
            inset.emplace_back(corner.GetPositionX() + dx * ULDUAR_FL_KITE_WALL_INSET,
                               corner.GetPositionY() + dy * ULDUAR_FL_KITE_WALL_INSET,
                               corner.GetPositionZ());
        }

        auto towards = [](Position const& from, Position const& to)
        {
            float dx = to.GetPositionX() - from.GetPositionX();
            float dy = to.GetPositionY() - from.GetPositionY();
            float const len = std::sqrt(dx * dx + dy * dy);
            if (len > 0.0f)
            {
                dx /= len;
                dy /= len;
            }
            return Position(from.GetPositionX() + dx * ULDUAR_FL_KITE_CORNER_CHAMFER,
                            from.GetPositionY() + dy * ULDUAR_FL_KITE_CORNER_CHAMFER,
                            from.GetPositionZ());
        };

        for (size_t i = 0; i < count; ++i)
        {
            Position const& prev = inset[(i + count - 1) % count];
            Position const& next = inset[(i + 1) % count];
            nodes.push_back(towards(inset[i], prev));
            nodes.push_back(towards(inset[i], next));
        }

        return nodes;
    }();

    return ring;
}

bool FlameLeviathanShutdown(Unit* boss) { return boss && boss->HasAura(SPELL_FL_SYSTEMS_SHUTDOWN); }

bool FlameLeviathanShutdownSoon(Player* bot) { return bot && FlameLeviathanStateFor(bot).shutdownSoon; }

Player* FlameLeviathanCatapultPassenger(Unit* demolisher)
{
    return demolisher && demolisher->GetEntry() == NPC_SALVAGED_DEMOLISHER
               ? FlameLeviathanSeatedPlayer(demolisher, ULDUAR_FL_DEMOLISHER_CATAPULT_SEAT)
               : nullptr;
}

Player* FlameLeviathanChopperPassenger(Unit* chopper)
{
    return chopper && chopper->GetEntry() == NPC_VEHICLE_CHOPPER
               ? FlameLeviathanSeatedPlayer(chopper, ULDUAR_FL_CHOPPER_REAR_SEAT)
               : nullptr;
}

bool FlameLeviathanGunnerless(Unit* demolisher)
{
    if (!demolisher || !demolisher->IsAlive() || demolisher->GetEntry() != NPC_SALVAGED_DEMOLISHER)
        return false;

    Vehicle* kit = demolisher->GetVehicleKit();
    if (!kit || !kit->GetPassenger(0) || kit->GetPassenger(ULDUAR_FL_DEMOLISHER_CATAPULT_SEAT))
        return false;

    Unit* seat = kit->GetPassenger(ULDUAR_FL_DEMOLISHER_GUNNER_SEAT);
    Vehicle* seatKit = seat ? seat->GetVehicleKit() : nullptr;
    return seatKit && !seatKit->GetPassenger(0);
}

bool FlameLeviathanWantsCatapult(PlayerbotAI* botAI, Player* bot)
{
    if (FlameLeviathanSeatOf(bot) != FlameLeviathanSeat::DemolisherGunner || PlayerbotAI::IsHeal(bot, true))
        return false;

    if (bot->HasSpellCooldown(SPELL_FL_LOAD_INTO_CATAPULT))
        return false;

    Unit* boss = FlameLeviathanBoss(botAI);
    if (!boss || !boss->IsInCombat() || FlameLeviathanShutdown(boss))
        return false;

    Unit* demolisher = FlameLeviathanRiddenVehicle(bot);
    if (!demolisher || demolisher->GetEntry() != NPC_SALVAGED_DEMOLISHER || FlameLeviathanCatapultPassenger(demolisher))
        return false;

    if (!FlameLeviathanCrewUsable(bot) || FlameLeviathanIsPursued(bot) || FlameLeviathanIsRamTarget(bot))
        return false;

    Player* driver = FlameLeviathanSeatedPlayer(demolisher, 0);
    if (!driver || !FlameLeviathanCrewUsable(driver))
        return false;

    if (demolisher->GetExactDist2d(boss) > ULDUAR_FL_THROW_MAX_RANGE + ULDUAR_FL_ARRIVE_TOLERANCE)
        return false;

    FlameLeviathanState const& state = FlameLeviathanStateFor(bot);
    if (state.seatClaims.count(bot->GetGUID()))
        return true;

    return FlameLeviathanSeatsNeedingRider(boss) > state.seatClaims.size();
}

void FlameLeviathanClaimSeat(Player* bot, Unit* demolisher)
{
    if (!bot || !demolisher)
        return;

    FlameLeviathanState& state = FlameLeviathanStateFor(bot);
    state.seatClaims.insert(bot->GetGUID());
    state.claimMs[bot->GetGUID()] = getMSTime();
    state.origin.Set(bot->GetGUID(), demolisher->GetGUID());
}

Position FlameLeviathanThrowAim(Unit* boss)
{
    if (!boss->isMoving() || boss->HasUnitState(UNIT_STATE_STUNNED))
        return boss->GetPosition();

    float const lead = boss->GetSpeed(MOVE_RUN) * ULDUAR_FL_THROW_LEAD_S;
    return Position(boss->GetPositionX() + std::cos(boss->GetOrientation()) * lead,
                    boss->GetPositionY() + std::sin(boss->GetOrientation()) * lead, boss->GetPositionZ());
}

Unit* FlameLeviathanRiderTurret(Player* bot)
{
    Unit* seat = bot ? bot->GetVehicleBase() : nullptr;
    if (!seat || seat->GetEntry() != NPC_FL_SEAT)
        return nullptr;

    auto const attackable = [bot](Unit* seatUnit) -> Unit*
    {
        Unit* turret = FlameLeviathanSeatTurret(seatUnit);
        if (!turret || turret->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) || !bot->IsValidAttackTarget(turret))
            return nullptr;
        return turret;
    };

    if (Unit* own = attackable(seat))
        return own;

    Unit* boss = seat->GetVehicleBase();
    Vehicle* kit = boss ? boss->GetVehicleKit() : nullptr;
    if (!kit)
        return nullptr;

    int8 const partner = seat->GetTransSeat() ^ (boss->GetMap()->Is25ManRaid() ? 2 : 1);
    if (Unit* other = attackable(kit->GetPassenger(partner)))
        return other;

    for (uint8 i = 0; i < ULDUAR_FL_LEVIATHAN_SEATS; ++i)
        if (Unit* any = attackable(kit->GetPassenger(i)))
            return any;

    return nullptr;
}

Unit* FlameLeviathanPickupChopper(PlayerbotAI* /*botAI*/, Player* member)
{
    if (!member)
        return nullptr;

    FlameLeviathanState const& state = FlameLeviathanStateFor(member);
    auto const it = state.pickup.find(member->GetGUID());
    if (it == state.pickup.end())
        return nullptr;

    Creature* hull = nullptr;
    return FlameLeviathanPickupValid(member, it->second, hull) ? hull : nullptr;
}

Player* FlameLeviathanPickupFor(Player* driver)
{
    Unit* chopper = driver && FlameLeviathanIsDriver(driver) ? driver->GetVehicleBase() : nullptr;
    if (!chopper || chopper->GetEntry() != NPC_VEHICLE_CHOPPER)
        return nullptr;

    for (auto const& [member, hull] : FlameLeviathanStateFor(driver).pickup)
    {
        if (hull != chopper->GetGUID())
            continue;

        Player* player = ObjectAccessor::GetPlayer(*driver, member);
        if (player && FlameLeviathanSeatOf(player) == FlameLeviathanSeat::OnFoot)
            return player;
    }

    return nullptr;
}

Unit* FlameLeviathanFerryTarget(PlayerbotAI* /*botAI*/, Player* passenger, Unit* from)
{
    if (!passenger || !from)
        return nullptr;

    FlameLeviathanState const& state = FlameLeviathanStateFor(passenger);
    if (auto const it = state.origin.find(passenger->GetGUID()); it != state.origin.end())
        if (Creature* own = passenger->GetMap()->GetCreature(it->second); own && FlameLeviathanGunnerless(own))
            return own;

    Group* group = passenger->GetGroup();
    if (!group)
        return nullptr;

    Unit* best = nullptr;
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member || FlameLeviathanSeatOf(member) != FlameLeviathanSeat::DemolisherDriver)
            continue;

        Unit* demolisher = member->GetVehicleBase();
        if (!FlameLeviathanGunnerless(demolisher))
            continue;

        if (!best || from->GetExactDist2d(demolisher) < from->GetExactDist2d(best))
            best = demolisher;
    }

    return best;
}

bool FlameLeviathanCrateTaken(Player* bot, Unit* crate)
{
    return crate->GetVehicle() || FlameLeviathanStateFor(bot).cratesTaken.count(crate->GetGUID());
}

void FlameLeviathanTakeCrate(Player* bot, Unit* crate) { FlameLeviathanStateFor(bot).cratesTaken.insert(crate->GetGUID()); }
