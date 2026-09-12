/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RaidObsSession.h"

#include "Cell.h"
#include "CellImpl.h"
#include "Creature.h"
#include "DBCStructure.h"
#include "DynamicObject.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"

#include <algorithm>
#include <list>

namespace RaidObs
{
// `dealt` is the running damage total for a roster member and 0 for everything else. Passed in rather
// than looked up here, because only BuildSnapshotPayload knows which rows are bots and it is already
// holding the BotTrace it comes from.
std::string UnitRow(Unit* unit, uint64 dealt, std::vector<uint32>* castSpells)
{
    uint32 castingId = 0;
    for (uint32 type = 0; type < CURRENT_MAX_SPELL; ++type)
        if (Spell* spell = unit->GetCurrentSpell(type))
        {
            castingId = spell->GetSpellInfo()->Id;
            break;
        }

    if (castingId && castSpells)
        castSpells->push_back(castingId);

    uint32 const maxMana = unit->GetMaxPower(POWER_MANA);
    float const manaPct = maxMana ? 100.0f * unit->GetPower(POWER_MANA) / maxMana : 0.0f;

    std::string row = "[";
    row += std::to_string(GuidKey(unit->GetGUID()));
    row += "," + Num(unit->GetPositionX());
    row += "," + Num(unit->GetPositionY());
    row += "," + Num(unit->GetPositionZ());
    row += "," + Num(unit->GetOrientation());
    row += "," + Num(unit->GetHealthPct());
    row += "," + Num(manaPct);
    row += "," + std::to_string(unit->GetVictim() ? GuidKey(unit->GetVictim()->GetGUID()) : 0);
    row += "," + std::string(unit->isMoving() ? "1" : "0");
    row += "," + std::to_string(static_cast<uint32>(unit->GetMotionMaster()->GetCurrentMovementGeneratorType()));
    row += "," + std::to_string(castingId);
    row += "," + std::to_string(dealt);
    row += "]";

    return row;
}

// What the dynamic object reports, falling back to the spell's own radius. Boss dynobjects arrive with
// a zero radius - all three of Hodir's icicle spells do - while every bot AoE reports correctly, so
// without the fallback the only hazards a position cannot be tested against are the lethal ones.
float HazardRadius(DynamicObject* dyn)
{
    float const radius = dyn->GetRadius();
    if (radius > 0.0f)
        return radius;

    SpellInfo const* info = sSpellMgr->GetSpellInfo(dyn->GetSpellId());
    if (!info)
        return 0.0f;

    for (std::size_t i = 0; i < info->Effects.size(); ++i)
    {
        if (!info->Effects[i].IsEffect())
            continue;

        float const calc = info->Effects[i].CalcRadius(dyn->GetCaster());
        if (calc > 0.0f)
            return calc;
    }

    return 0.0f;
}

// Which side of the pull laid this down. Answered from the caster's guid rather than a reaction check
// against a sampled roster member: the object outlives its caster, and asking a live unit gave a
// different answer on the snapshots where the caster had gone, so the same puddle flipped side
// mid-fight. A guid does not change. A pet or totem is read through its owner while it is still up.
bool HazardIsFriendly(DynamicObject* dyn)
{
    ObjectGuid const casterGuid = dyn->GetCasterGUID();
    if (casterGuid.IsPlayer())
        return true;

    Unit* caster = dyn->GetCaster();
    Unit* owner = caster ? caster->GetOwner() : nullptr;
    return owner && owner->IsPlayer();
}

// Hazards and the hostile units standing among them, from a single grid visit - a searcher each would
// walk the same 150-yard cell range twice a snapshot. Swept creatures are appended to `units`, so they
// land in the same row set as the roster.
std::string ObsSession::SweepArea(Unit* anchor, std::string& units, bool& firstUnit,
                                  std::vector<uint32>& castSpells)
{
    std::list<WorldObject*> objs;
    Acore::AllWorldObjectsInRange check(anchor, OBS_HAZARD_SWEEP_RADIUS);
    Acore::WorldObjectListSearcher<Acore::AllWorldObjectsInRange> searcher(
        anchor, objs, check, GRID_MAP_TYPE_MASK_DYNAMICOBJECT | GRID_MAP_TYPE_MASK_CREATURE);
    Cell::VisitObjects(anchor, searcher, OBS_HAZARD_SWEEP_RADIUS);

    std::string out = "[";
    bool firstHazard = true;
    std::vector<Creature*> sweptUnits;

    for (WorldObject* obj : objs)
    {
        if (obj->GetTypeId() == TYPEID_DYNAMICOBJECT)
        {
            DynamicObject* dyn = obj->ToDynObject();
            if (!dyn)
                continue;

            if (!firstHazard)
                out += ",";
            firstHazard = false;

            EnsureSpell(dyn->GetSpellId());

            out += "[" + std::to_string(dyn->GetSpellId());
            out += "," + Num(dyn->GetPositionX());
            out += "," + Num(dyn->GetPositionY());
            out += "," + Num(dyn->GetPositionZ());
            out += "," + Num(HazardRadius(dyn));
            // Without the flag, two thirds of a Hodir trace's hazard rows were the raid's own Death and
            // Decay and Consecration, indistinguishable from what was killing them.
            out += "," + std::string(HazardIsFriendly(dyn) ? "0" : "1");
            out += "]";
            continue;
        }

        // Hazard units carry no dynamic object and never enter combat, so nothing else would put them
        // in the trace: Hodir's icicles are creatures that damage whatever is under where they land.
        Creature* creature = obj->ToCreature();
        if (!creature || !creature->IsAlive())
            continue;

        if (watched.count(creature->GetGUID()) || !anchor->IsHostileTo(creature))
            continue;

        sweptUnits.push_back(creature);
    }

    // Nearest first, because the cap decides who makes it into the row set and grid order is not a
    // ranking. A Hodir pull reaches Thorim's arena, whose parked trash starts 74 yd out and by itself
    // outnumbers the cap: taken in grid order it filled every slot on three traces running, and not one
    // ice block, Toasty Fire or icicle was ever sampled - the units the sweep exists for.
    if (sweptUnits.size() > OBS_MAX_WATCHED)
    {
        std::partial_sort(sweptUnits.begin(), sweptUnits.begin() + static_cast<std::ptrdiff_t>(OBS_MAX_WATCHED),
                          sweptUnits.end(),
                          [anchor](Creature const* left, Creature const* right)
                          { return anchor->GetExactDist2dSq(left) < anchor->GetExactDist2dSq(right); });
        sweptUnits.resize(OBS_MAX_WATCHED);
    }

    for (Creature* creature : sweptUnits)
    {
        EnsureUnit(creature);

        if (!firstUnit)
            units += ",";
        firstUnit = false;
        units += UnitRow(creature, 0, &castSpells);
    }

    out += "]";
    return out;
}

// Built the same way with or without a session, so a flushed pre-roll and live sampling produce the
// same row shape. Only the sweep differs - pre-roll has no session and cannot afford it.
std::string BuildSnapshotPayload(Map* map, std::vector<ObjectGuid> const& roster,
                                 std::unordered_set<ObjectGuid> const& watched, ObsSession* session,
                                 std::vector<uint32>* castSpells,
                                 std::unordered_map<uint64, std::string>* unitRecords)
{
    // A pet or vehicle sampled without a session is named here or never: it can be gone by the drain.
    // Looked up before building, not emplaced over: the fields would be serialised on every sample of
    // every pet and thrown away, four times a second for the whole pre-roll.
    auto name = [session, unitRecords](Unit* unit)
    {
        if (session)
        {
            session->EnsureUnit(unit);
            return;
        }

        if (!unitRecords || !unit)
            return;

        uint64 const key = GuidKey(unit->GetGUID());
        if (unitRecords->find(key) == unitRecords->end())
            unitRecords->emplace(key, UnitRecordFields(unit));
    };

    std::string units = "[";
    std::vector<uint32> casting;
    bool first = true;
    Unit* anchor = nullptr;
    std::unordered_set<ObjectGuid> ridden;

    for (ObjectGuid guid : roster)
    {
        Player* player = ObjectAccessor::FindPlayer(guid);
        if (!player || !player->IsInWorld() || player->GetMap() != map)
            continue;

        if (!anchor)
            anchor = player;

        // A passenger reports its vehicle's coordinates, so without the vehicle itself a trace cannot
        // say what the raid was actually riding: its health, its facing, or whether it was moving.
        // Climb to the root, because a gunner rides a turret that rides the real vehicle and the
        // turret is only a weapon mount - the hull is the thing that takes the damage.
        Unit* vehicle = player->GetVehicleBase();
        for (int depth = 0; vehicle && depth < 4; ++depth)
        {
            Unit* parent = vehicle->GetVehicleBase();
            if (!parent)
                break;
            vehicle = parent;
        }

        if (vehicle && vehicle->IsInWorld() && vehicle->GetMap() == map && ridden.insert(vehicle->GetGUID()).second)
        {
            if (!first)
                units += ",";
            first = false;
            units += UnitRow(vehicle, 0, &casting);
            name(vehicle);
        }

        // The last health this bot was seen at, so a death the damage hooks never saw can still say
        // what it fell from and how long ago that reading was.
        uint64 dealt = 0;
        if (session)
        {
            BotTrace& trace = session->bots[GuidKey(guid)];
            trace.lastHpPct = player->GetHealthPct();
            trace.lastHpMs = getMSTime();
            dealt = trace.damageDealt;
        }

        if (!first)
            units += ",";
        first = false;
        units += UnitRow(player, dealt, &casting);

        // Pets are in nothing else: the sweep only keeps units hostile to the anchor and the watched
        // set is seeded from attackers, so a trace could say a pet cast something but never where it
        // was standing. Guardians count - a death knight's ghoul and a shaman's wolves are not pets.
        // Totems are skipped the other way: four of them, none of which ever move.
        if (!g_cfg.logPets)
            continue;

        for (Unit* pet : player->m_Controlled)
        {
            if (!pet || !pet->IsAlive() || pet->IsTotem())
                continue;

            if (!pet->IsPet() && !pet->IsGuardian())
                continue;

            if (!pet->IsInWorld() || pet->GetMap() != map || !ridden.insert(pet->GetGUID()).second)
                continue;

            // No damage column. AccrueDamageDealt already folds a pet's damage into its owner's total,
            // and a second copy here would double any window differenced out of two snapshots.
            units += "," + UnitRow(pet, 0, &casting);
            name(pet);
        }
    }

    for (ObjectGuid guid : watched)
    {
        Creature* creature = map->GetCreature(guid);
        if (!creature || !creature->IsInWorld() || !creature->IsAlive())
            continue;

        if (!first)
            units += ",";
        first = false;
        units += UnitRow(creature, 0, &casting);
    }

    // The sweep visits every grid cell within 150 yards, which is far too much to run four times a
    // second on a map that is not in a pull. Pre-roll keeps the row so the shape does not change.
    std::string const hazards = session && anchor ? session->SweepArea(anchor, units, first, casting) : "[]";
    units += "]";

    // The pre-roll has no session to name them with, so it takes the ids and sweeps them at the drain.
    if (session)
        for (uint32 spellId : casting)
            session->EnsureSpell(spellId);
    else if (castSpells)
        castSpells->insert(castSpells->end(), casting.begin(), casting.end());

    return "\"u\":" + units + ",\"hz\":" + hazards;
}

// A boss outranks trash for the bounded slots. The cap is filled first-come, so without eviction a
// boss that engages after forty adds is dropped and no death ever measures its distance.
void ObsSession::WatchCreature(Creature* creature)
{
    if (!creature || watched.count(creature->GetGUID()))
        return;

    bool const isBoss = creature->isWorldBoss() || creature->IsDungeonBoss();
    if (watched.size() >= OBS_MAX_WATCHED)
    {
        if (!isBoss)
            return;

        auto evict = watched.end();
        for (auto it = watched.begin(); it != watched.end(); ++it)
        {
            Creature* held = map ? map->GetCreature(*it) : nullptr;
            if (!held || !(held->isWorldBoss() || held->IsDungeonBoss()))
            {
                evict = it;
                break;
            }
        }

        if (evict == watched.end())
            return;

        watched.erase(evict);
    }

    watched.insert(creature->GetGUID());
    EnsureUnit(creature);
}

// Everything already swinging at the raid when the trace opens. A session started by MarkPull or by a
// boss state change arrives after those creatures entered combat, so without this a gauntlet records
// players only and every death.dist comes out empty.
void ObsSession::SeedWatched(Unit* source)
{
    if (Creature* creature = source ? source->ToCreature() : nullptr)
        WatchCreature(creature);

    if (!map)
        return;

    Map::PlayerList const& players = map->GetPlayers();
    for (Map::PlayerList::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        Player* player = it->GetSource();
        if (!player || !player->IsInWorld())
            continue;

        for (Unit* attacker : player->getAttackers())
            if (Creature* creature = attacker ? attacker->ToCreature() : nullptr)
                WatchCreature(creature);
    }
}

void ObsSession::PruneWatched()
{
    for (auto it = watched.begin(); it != watched.end();)
    {
        Creature* creature = map ? map->GetCreature(*it) : nullptr;
        if (!creature || !creature->IsInWorld() || !creature->IsAlive())
            it = watched.erase(it);
        else
            ++it;
    }
}

// Removals are held for the death rewind window rather than dropped, so this is the only thing that
// bounds the tracked set.
void ObsSession::PruneAuras(uint32 now)
{
    for (auto& entry : bots)
    {
        BotTrace& trace = entry.second;
        for (auto it = trace.auras.begin(); it != trace.auras.end();)
        {
            if (it->second.removedMs && getMSTimeDiff(it->second.removedMs, now) > g_cfg.deathRewindMs)
                it = trace.auras.erase(it);
            else
                ++it;
        }
    }
}
}  // namespace RaidObs
