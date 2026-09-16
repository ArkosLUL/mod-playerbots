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
#include "ObjectDefines.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "Timer.h"
#include "UldEncounterGate.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "Unit.h"
#include "Vehicle.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <unordered_map>
#include <vector>

using namespace EncounterHelpers;

// The four NPC_FREYA_WARD_TARGET spawn points from boss_flame_leviathan.cpp, in ring order.
std::vector<Position> const ULDUAR_FL_ARENA_CORNERS = {
    Position(159.4f, 64.1f, 409.8f),
    Position(382.9f, 74.0f, 411.6f),
    Position(374.0f, -141.0f, 411.0f),
    Position(157.7f, -140.3f, 409.8f)
};

// The instance script tracks him from create to remove. The target scan it replaced stops at
// SightDistance (100 yd), and a bot beyond it read "no boss" and wiped the shared state below for the
// whole raid: fl.pursued flapped 26 times in one 2026-09-16 pull, each time with a bot 117-147 yd out.
Unit* FlameLeviathanBoss(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    InstanceScript* instance = bot ? bot->GetInstanceScript() : nullptr;
    if (!instance)
        return botAI ? GetFirstAliveUnitByEntry(botAI, NPC_FLAME_LEVIATHAN) : nullptr;

    // The entry as well: index 0 names some other boss in any instance that is not Ulduar.
    Creature* boss = instance->GetCreature(ULD_BOSS_LEVIATHAN);
    return boss && boss->GetEntry() == NPC_FLAME_LEVIATHAN && boss->IsInWorld() && boss->IsAlive() ? boss : nullptr;
}

namespace
{
// Raid-wide answers, folded once per instance per tick rather than once per bot - and sharing the
// result is what stops two vehicles disagreeing about whose channel it is.
struct FlameLeviathanState
{
    // boss_flame_leviathan never sets IN_PROGRESS (only SPECIAL / NOT_STARTED / DONE) and the unit
    // it engages is a vehicle rather than a roster player, so neither RaidObs opener fires and this
    // fight has never left a trace.
    bool pullTraced = false;

    // Who has already fired into the channel now running. Cleared the moment he stops channelling,
    // so the next channel starts unclaimed - the 10 s gap between channels guarantees we see one.
    RaidObs::ObsValue<ObjectGuid> ventClaimedBy{"fl.interrupter"};

    // The vehicle currently wearing Pursued. Battering Ram is a 25 yd sphere centred on it, so this
    // is the thing every other vehicle measures itself against.
    RaidObs::ObsValue<ObjectGuid> pursuedVehicle{"fl.pursued"};

    // Vehicles that cannot move. Hodir's Fury carries an undispellable 60 s stun, and a frozen
    // vehicle has to be counted out rather than waited on - it still holds roles otherwise.
    RaidObs::ObsGuidMap<bool> frozen{"fl.frozen"};

    // The siege hull kept on the boss for Flame Vents while the others post. Re-electing by distance
    // every scan would hand the role to a posted engine whenever the reserve froze, and back when it
    // thawed, dragging one corner's engine across the arena twice for one Hodir's Fury.
    RaidObs::ObsValue<ObjectGuid> ventReserve{"fl.reserve"};

    // Siege hulls in guid order, latched the first time anyone asks. Ranking the live ones instead
    // renumbered every engine below a loss, so one hull dying swapped all four corners at once.
    std::vector<ObjectGuid> siegeOrder;

    // Hodir's Fury reticles, when each was first seen stunned, and the ones whose strike is still to
    // land. Read off the reticle's stun because nothing else marks the commit.
    std::vector<ObjectGuid> furyReticles;
    std::unordered_map<ObjectGuid, uint32> furyStunnedMs;
    RaidObs::ObsGuidSet furyArmed{"fl.fury"};

    uint32 scanMs = 0;
};

// Not thread_local. A map is updated by one thread at a time but is never pinned to one, and
// MapUpdate.Threads is 6 here, so per-thread copies hand the same instance a fresh state whenever the
// pool reassigns it - which silently resets the vent claim and made fl.pursued flap 199 times in a
// pull that switched target twelve times. References into an unordered_map survive rehashing, so the
// lock only has to cover the lookup.
std::mutex flStatesMutex;
std::unordered_map<uint32 /*instanceId*/, FlameLeviathanState> flStates;

// Long enough that the scan is cheap, short enough that a 31s Pursued cycle is never missed.
constexpr uint32 ULDUAR_FL_SCAN_INTERVAL_MS = 200;

FlameLeviathanState& FlameLeviathanStateFor(Player* bot)
{
    std::lock_guard<std::mutex> guard(flStatesMutex);
    return flStates[bot->GetInstanceId()];
}

// Arms a reticle from the tick it is first seen stunned, for the fuse and the strike's flight.
void TickHodirsFury(FlameLeviathanState& state, PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !(FlameLeviathanActiveTowerMask(botAI) & FL_TOWER_FROST))
        return;

    // SummonTowerHelpers places two, and they live for the whole pull.
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

        // First seen already stunned counts as a fresh commit: dodging a spent one costs a short drive,
        // standing under a live one costs the hull for a minute.
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

void ElectVentReserve(FlameLeviathanState& state, Player* bot, Unit* boss)
{
    // With the Life tower down nobody posts, and every engine holds station as it always did.
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

// Everything that has to be true once per instance per tick rather than once per bot: open the
// trace, expire a vent claim, notice a Pursued switch, and record which vehicles are frozen.
void TickFlameLeviathan(PlayerbotAI* botAI, Player* bot, Unit* boss)
{
    FlameLeviathanState& state = FlameLeviathanStateFor(bot);
    if (state.scanMs && GetMSTimeDiffToNow(state.scanMs) < ULDUAR_FL_SCAN_INTERVAL_MS)
        return;

    state.scanMs = getMSTime();

    // Off the boss, never off the calling bot: one bot dropping combat is not the pull ending, and
    // without this reset a wipe would leave the latch set and the re-pull would open no trace.
    if (!boss || !boss->IsInCombat())
    {
        state.pullTraced = false;
        state.ventClaimedBy = ObjectGuid::Empty;
        state.pursuedVehicle = ObjectGuid::Empty;
        state.ventReserve = ObjectGuid::Empty;
        state.frozen.clear();
        state.siegeOrder.clear();
        state.furyReticles.clear();
        state.furyStunnedMs.clear();
        state.furyArmed.clear();
        return;
    }

    if (!state.pullTraced)
    {
        state.pullTraced = true;
        RaidObs::MarkPull(bot->GetMap(), boss);
    }

    if (!FlameLeviathanIsVentChanneling(boss))
        state.ventClaimedBy = ObjectGuid::Empty;

    TickHodirsFury(state, botAI, bot);

    // Pursued is read off the vehicles rather than the players: the aura lands on whichever unit the
    // boss's spell picked, and a gunner's own guid never carries it. Keyed on the vehicle for the
    // same reason, which also folds a crew of four into the one entry that matters.
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

            // A claim held by an engine that froze mid-channel would block the re-election for the
            // rest of it, and the channel is only ten seconds long. Hand it back instead.
            if (state.ventClaimedBy.Get() == member->GetGUID() && !FlameLeviathanCrewUsable(member))
                state.ventClaimedBy = ObjectGuid::Empty;
        }
    }

    state.pursuedVehicle = pursued;

    ElectVentReserve(state, bot, boss);
}
}  // namespace

bool FlameLeviathanEngaged(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot || bot->GetMapId() != ULDUAR_MAP_ID)
        return false;

    Unit* boss = FlameLeviathanBoss(botAI);

    // Ahead of the combat test, because the housekeeping it drives includes the wipe reset.
    TickFlameLeviathan(botAI, bot, boss);

    if (!boss || boss->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE))
        return false;

    // His combat, not the rider's. Threat here belongs to the vehicle creature, so a bot that
    // drives far enough away drops combat while the pull is still very much on - and that used to
    // switch off both this encounter's mover and the veto that keeps the generic ones out, handing
    // the wheel to "follow". Every accepted follow move on a crewed bot across two traces happened
    // with the hull more than 100 yd out.
    if (boss->IsInCombat())
        return true;

    // He can be out of combat for a moment while the raid is driving in, so the rider still counts.
    return bot->IsInCombat();
}

Unit* FlameLeviathanRiddenVehicle(Player* bot)
{
    Unit* base = bot ? bot->GetVehicleBase() : nullptr;
    if (!base)
        return nullptr;

    // A gunner rides seat 0 of a turret creature that is itself a passenger of the real vehicle.
    if (Unit* parent = base->GetVehicleBase())
        return parent;

    return base;
}

bool FlameLeviathanIsDriver(Player* bot)
{
    Unit* base = bot ? bot->GetVehicleBase() : nullptr;
    if (!base)
        return false;

    uint32 const entry = base->GetEntry();
    return entry == NPC_SALVAGED_SIEGE_ENGINE || entry == NPC_VEHICLE_CHOPPER ||
           entry == NPC_SALVAGED_DEMOLISHER;
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

// Electroshock is a 25 yd frontal cone, so a siege engine parked across the arena would win the
// ranking and then land nothing. Range is part of eligibility, not an afterthought.
//
// IsWithinCombatRange adds *both* combat reaches, and his is 15, so asking it for 25 yd answered yes
// out to 47.7 - roughly twice what the cone covers. The cone check adds only the target's reach
// (WorldObject::GetObjectSize), so that is what this mirrors.
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
    // HasInArc splits what it is handed, so the full cone width goes in.
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

    // Outside the cone but inside CAST_ANGLE_IN_FRONT, so CastVehicleSpell would not have turned and
    // the shot would have gone nowhere. Spend the tick turning and let a later one fire.
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

    // A stunned engine cannot fire the interrupt, and claiming the channel would waste it. A Pursued
    // one would turn to face him mid-escape: 4 of 12 Pursued spans on 2026-09-16 did.
    if (!FlameLeviathanCrewUsable(bot) || FlameLeviathanIsPursued(bot))
        return false;

    // One shot per channel. Without this the ranking below re-elects on every tick of the channel:
    // the winner spends 20 energy casting, which promotes whoever is now highest, and the whole line
    // of siege engines empties into a single channel milliseconds apart.
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

        // Highest energy wins, guid breaks ties. Casting spends 20, which drops the caster to the
        // back of its own queue, so the duty rotates across channels with nobody having to be told.
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

bool FlameLeviathanCrewUsable(Player* member)
{
    if (!member || !member->IsAlive())
        return false;

    // Hodir's Fury's stun runs 60s, carries no mechanic and no dispel type, so nothing shortens it,
    // and a role elected on guid order would sit with a frozen vehicle for a third of the fight.
    //
    // The rider is checked for the stun only. Vehicle::AddPassenger roots every passenger, driver
    // included, so UNIT_STATE_NOT_MOVE on a rider is just "is seated" and asking it here reported
    // the whole fleet unusable - no tar lead, no vent interrupter, no corner post.
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

// An add on a hull that sits at a post is that post's to kill. A gunner fires from a turret bolted
// onto the hull, so either link counts as riding it.
static bool FlameLeviathanHeldByAnotherPost(Unit* add, Unit* from)
{
    Unit* victim = add->GetVictim();
    if (!victim || victim->GetEntry() != NPC_SALVAGED_SIEGE_ENGINE)
        return false;

    if (from == victim || from->GetVehicleBase() == victim)
        return false;

    for (uint8 i = 0; i < ULDUAR_FL_ARENA_CORNERS.size(); ++i)
        if (victim->GetExactDist2d(FlameLeviathanCornerPostPoint(i)) <= ULDUAR_FL_CORNER_HOLD_RADIUS)
            return true;

    return false;
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

        // Still counted in other adds' clusters: a held add is still there for the splash to hit.
        if (FlameLeviathanHeldByAnotherPost(add, from))
            continue;

        // Counts itself, which is what makes a lone add score 1 and keeps the comparison honest
        // against a pair. Skipped entirely for a single-target pick, where it would decide nothing.
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

bool FlameLeviathanInBatteringRamBlast(Unit* vehicleBase, Unit* pursued)
{
    if (!vehicleBase || !pursued || vehicleBase == pursued)
        return false;

    // Battering Ram is TARGET_DEST_TARGET_ENEMY with a 25 yd radius, cast on the boss's victim. The
    // blast is a sphere around the Pursued vehicle, so his facing has nothing to do with it - the
    // frontal-arc test this replaced was reading the wrong object and missed two thirds of the hits.
    return vehicleBase->GetExactDist2d(pursued) <= ULDUAR_FL_BATTERING_RAM_RADIUS + vehicleBase->GetObjectSize();
}

bool FlameLeviathanShouldClearBatteringRam(PlayerbotAI* botAI, Player* bot)
{
    // Standing in his own blast is the pursued vehicle's whole job, and it is already kiting.
    if (!bot || FlameLeviathanIsPursued(bot))
        return false;

    // The ridden vehicle, not the seat: a gunner's GetVehicleBase is the bolted-on turret, whose
    // position is the parent's anyway but whose object size is not.
    Unit* vehicleBase = FlameLeviathanRiddenVehicle(bot);
    Unit* pursued = FlameLeviathanPursuedVehicle(botAI, bot);
    Unit* boss = FlameLeviathanBoss(botAI);
    if (!vehicleBase || !pursued || !boss)
        return false;

    // He only fires inside his own cast test, so outside it the blast cannot land however close the
    // fleet is packed. Borrowed verbatim from the script rather than reconstructed, because
    // IsWithinCombatRange adds both combat reaches and a hand-rolled 15 yd would be far too tight.
    if (!boss->IsWithinCombatRange(pursued, ULDUAR_FL_BATTERING_RAM_CAST_RANGE))
        return false;

    return FlameLeviathanInBatteringRamBlast(vehicleBase, pursued);
}

bool FlameLeviathanIsTarLead(PlayerbotAI* /*botAI*/, Player* bot)
{
    Unit* base = bot ? bot->GetVehicleBase() : nullptr;
    if (!base || base->GetEntry() != NPC_VEHICLE_CHOPPER)
        return false;

    // A pursued chopper is kiting away from him with its back turned, which drops tar in his path
    // for free - it does not need the lead slot, and taking it would strand the role.
    if (FlameLeviathanIsPursued(bot))
        return false;

    // A frozen chopper cannot lead anything, so it must not win the election either - it would hold
    // the slot for the full 60 s and nobody else would lay tar.
    if (!FlameLeviathanCrewUsable(bot))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return true;

    ObjectGuid const myGuid = bot->GetGUID();
    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member || member == bot || !FlameLeviathanCrewUsable(member))
            continue;

        Unit* memberBase = member->GetVehicleBase();
        if (!memberBase || memberBase->GetEntry() != NPC_VEHICLE_CHOPPER)
            continue;

        if (FlameLeviathanIsPursued(member))
            continue;

        if (member->GetGUID() < myGuid)
            return false;
    }

    return true;
}

// Where this bot's siege engine sits in hull-guid order, or -1 if it is not driving one.
//
// Every live hull counts, pursued and stunned ones included. Skipping them would renumber everyone
// below on a Pursued switch - about once every 31s - and all four corners would swap engines. An
// engine that cannot hold its post just does not drive to it; its slot waits.
//
// One driver per hull, so ranking over drivers needs no dedupe: a gunner's GetVehicleBase is the
// bolted-on turret, which FlameLeviathanIsDriver already rejects.
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
        // A hull nobody was driving when the order latched. Give it a slot behind the originals
        // rather than refusing, so a bot that took a replacement engine still posts somewhere.
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

    // A pursued engine is kiting and a frozen one cannot drive, so neither drives to its corner.
    if (FlameLeviathanIsPursued(bot) || !FlameLeviathanCrewUsable(bot))
        return -1;

    if (FlameLeviathanIsVentReserve(bot))
        return -1;

    int32 rank = FlameLeviathanSiegeRank(bot);
    if (rank < 0)
        return -1;

    // Rank 0 owns no corner of its own, so it covers the one the reserve left. The reserve moves only
    // when it freezes, dies or is Pursued, so this swaps two engines at most.
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

Position FlameLeviathanCornerPostPoint(uint8 index)
{
    Position const& corner = ULDUAR_FL_ARENA_CORNERS[index % ULDUAR_FL_ARENA_CORNERS.size()];

    float centreX = 0.0f;
    float centreY = 0.0f;
    for (Position const& each : ULDUAR_FL_ARENA_CORNERS)
    {
        centreX += each.GetPositionX();
        centreY += each.GetPositionY();
    }
    centreX /= static_cast<float>(ULDUAR_FL_ARENA_CORNERS.size());
    centreY /= static_cast<float>(ULDUAR_FL_ARENA_CORNERS.size());

    // Toward the middle, so the engine ends up between the spawn point and the raid and Ram's
    // knockback drives what it catches back into the corner instead of out at the fleet.
    float dx = centreX - corner.GetPositionX();
    float dy = centreY - corner.GetPositionY();
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
bool IsFlameLeviathanTowerHazard(uint32 entry, uint32 towerMask)
{
    // Both Mimiron entries: the target is the head of the trail, and NPC_FL_MIMIRONS_INFERNO is each
    // 9 yd patch it drops behind itself. The patches outlive the head passing by a full 30s.
    return ((towerMask & FL_TOWER_STORM) && entry == NPC_FL_THORIM_HAMMER_TARGET) ||
           ((towerMask & FL_TOWER_FLAMES) &&
            (entry == NPC_FL_MIMIRONS_INFERNO_TARGET || entry == NPC_FL_MIMIRONS_INFERNO)) ||
           ((towerMask & FL_TOWER_FROST) && entry == NPC_FL_HODIRS_FURY_TARGET);
}

// The hazard markers are non-selectable trigger creatures, so they never appear in the attack-target
// lists ("possible targets"). The raw nearby-npc list has every non-player unit regardless of flags.
template <typename Visit>
void ForEachTowerHazard(PlayerbotAI* botAI, uint32 towerMask, Visit visit)
{
    Player* bot = botAI->GetBot();
    for (auto const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || !IsFlameLeviathanTowerHazard(unit->GetEntry(), towerMask))
            continue;

        // Still chasing, so it cannot strike yet, and dodging it only drags it along.
        if (unit->GetEntry() == NPC_FL_HODIRS_FURY_TARGET && !FlameLeviathanFuryArmed(bot, unit))
            continue;

        visit(unit);
    }
}
}  // namespace

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

Unit* GetFlameLeviathanNearestTowerHazard(PlayerbotAI* botAI, Unit* from, uint32 towerMask)
{
    if (!botAI || !from)
        return nullptr;

    Unit* nearest = nullptr;
    float best = ULDUAR_FL_TOWER_HAZARD_MARGIN;

    ForEachTowerHazard(botAI, towerMask,
                       [&](Unit* hazard)
                       {
                           float const gap = from->GetExactDist2d(hazard) - FlameLeviathanHazardReach(hazard, from);
                           if (gap < best)
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

float FlameLeviathanStationBearingOffset(Player* bot, Unit* vehicleBase, float radius)
{
    Group* group = bot ? bot->GetGroup() : nullptr;
    if (!group || !vehicleBase || radius <= 0.0f)
        return 0.0f;

    uint32 const entry = vehicleBase->GetEntry();
    ObjectGuid const myGuid = vehicleBase->GetGUID();

    // Rank among the live vehicles of my own class, by guid so every crew member computes the same
    // answer. Deduped on the vehicle: four riders in one hull are one slot, not four. A frozen
    // vehicle keeps its slot on purpose - dropping it would renumber everyone else's and swing the
    // whole fan across the arena for 60 s.
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

    // A chord of ULDUAR_FL_STATION_SPACING at this radius, as an angle - which is what actually has to
    // hold, because the thing being avoided is a circle on the ground and not an angular sector.
    float spread = 2.0f * std::asin(std::min(1.0f, ULDUAR_FL_STATION_SPACING / (2.0f * radius)));

    // A tight radius wants a wide angle, and five of those would wrap the fan around him and put the
    // far slots back in his front. Cap the whole fan instead and accept less spacing when it bites.
    spread = std::min(spread, ULDUAR_FL_STATION_MAX_ARC / static_cast<float>(count - 1));

    return (static_cast<float>(index) - static_cast<float>(count - 1) * 0.5f) * spread;
}

Position FlameLeviathanRearPoint(Unit* boss, float standDist, float bearingOffset)
{
    return FlameLeviathanOffsetPoint(boss, boss->GetOrientation() + M_PI + bearingOffset, standDist);
}

Position FlameLeviathanLeadPoint(Unit* boss, float standDist)
{
    return FlameLeviathanOffsetPoint(boss, boss->GetOrientation(), standDist);
}

float FlameLeviathanTarLeadDistance(Unit* boss, Unit* pursued, float size)
{
    if (!boss)
        return 0.0f;

    // Nobody being chased means no blast to stay out of, so take the whole lead.
    if (!pursued)
        return ULDUAR_FL_TAR_LEAD_DIST;

    // His path is the line to whoever he is chasing, so leading him means driving that line - and the
    // far end of it is the centre of Battering Ram. Stop short of the sphere rather than crossing it;
    // the tar still lands in front of him, which is the entire point of the slot. Measured from his
    // centre because the offset point adds his combat reach back on.
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

        // Pull each corner off the wall first, so the chamfer is cut from a point the vehicles can
        // actually path to rather than from the wall itself.
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

        // Two nodes per corner, one on each adjoining edge, so a kiting vehicle rounds the turn
        // instead of driving into the corner while the boss cuts the diagonal.
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
