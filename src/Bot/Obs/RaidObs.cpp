/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RaidObs.h"

#include "Cell.h"
#include "CellImpl.h"
#include "Config.h"
#include "Creature.h"
#include "DBCStructure.h"
#include "DynamicObject.h"
#include "Group.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace RaidObs
{
std::atomic<bool> g_active{false};
}

namespace
{
using namespace RaidObs;

constexpr uint32 OBS_ROSTER_REFRESH_MS = 3000;
constexpr uint32 OBS_FLUSH_INTERVAL_MS = 2000;
// One anchor covers a boss room; a raid spread wider than this loses the far edge of the sweep.
constexpr float OBS_HAZARD_SWEEP_RADIUS = 150.0f;
// Bounds a snapshot row count when a pull drags half the zone into combat.
constexpr std::size_t OBS_MAX_WATCHED = 40;
// A stuck bot re-offers the same rejected destination every tick; once a second is enough to see it.
constexpr uint32 OBS_MOVE_REJECT_THROTTLE_MS = 1000;
// How long an unchanged engine pass stays deduplicated before it is written again, so a bot that has
// been losing the same tick for minutes still leaves a trail without one record per tick.
constexpr uint32 OBS_ACTION_REPEAT_MS = 10000;
// How far back a death record reaches for auras that came off just before it. Unit::Kill strips every
// non-passive aura before the death hook runs, so whatever the boss had applied is already gone by
// then; much longer and a debuff that expired on its own would read as still up.
constexpr uint32 OBS_DEATH_AURA_GRACE_MS = 2000;
// Bounds one engine pass' verdict buffer. iterationsPerTick already limits it; this is the backstop.
constexpr std::size_t OBS_MAX_TICK_ENTRIES = 64;
// Past this share of the roster dead, the pull was a wipe whatever state the instance script settled on.
constexpr float OBS_WIPE_DEAD_SHARE = 0.5f;

struct ObsConfig
{
    bool enabled = false;
    std::string dir = "botobs";
    std::unordered_set<uint32> maps;
    bool allMaps = true;
    uint32 snapshotIntervalMs = 250;
    uint32 preRollMs = 30000;
    bool logHeals = true;
    bool logAuras = true;
    uint32 minDamage = 0;
    uint32 deathRewindMs = 15000;
    uint32 deathVerdictMs = 10000;
    uint32 idleCloseMs = 30000;
    uint32 retentionDays = 7;
    uint64 maxDirBytes = 5120ull * 1024 * 1024;
    uint64 maxFileBytes = 256ull * 1024 * 1024;
};

ObsConfig g_cfg;
std::string g_logsDir;

// --- small formatting helpers -------------------------------------------------

std::string JsonEscape(std::string const& in)
{
    std::string out;
    out.reserve(in.size() + 8);
    for (char c : in)
    {
        switch (c)
        {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                }
                else
                {
                    out += c;
                }
                break;
        }
    }

    return out;
}

std::string Quoted(std::string const& in) { return "\"" + JsonEscape(in) + "\""; }

std::string Num(float v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", v);
    return buf;
}

// The counter is a separate numbering space per guid type - a creature and a player both start at 1 -
// so it cannot key a record on its own. Players keep the bare counter because they are the bulk of
// every snapshot; the tag stays small enough that the result is still an exact JSON integer, and
// ObjectGuid::Empty still comes out as 0.
uint64 GuidKey(ObjectGuid guid)
{
    uint64 tag;
    switch (guid.GetHigh())
    {
        case HighGuid::Player:        tag = 0; break;
        case HighGuid::Unit:          tag = 1; break;
        case HighGuid::Pet:           tag = 2; break;
        case HighGuid::Vehicle:       tag = 3; break;
        case HighGuid::GameObject:    tag = 4; break;
        case HighGuid::DynamicObject: tag = 5; break;
        default:                      tag = 7; break;
    }

    return (tag << 32) | guid.GetCounter();
}

char const* MoveKindName(MoveKind kind)
{
    switch (kind)
    {
        case MoveKind::Jump:   return "jump";
        case MoveKind::Follow: return "follow";
        case MoveKind::Chase:  return "chase";
        default:               return "point";
    }
}

char const* MoveReason(MoveOutcome outcome)
{
    switch (outcome)
    {
        case MoveOutcome::Duplicate:    return "dup";
        case MoveOutcome::Waiting:      return "wait";
        case MoveOutcome::NotAllowed:   return "blocked";
        case MoveOutcome::NoPath:       return "nopath";
        case MoveOutcome::AlreadyThere: return "there";
        default:                        return "";
    }
}

// Spelled out rather than written as the enum's ordinal, for the same reason the spell dictionary
// exists: a trace that needs the enum to hand is not self-contained.
char const* MovePriorityName(MovePriority priority)
{
    switch (priority)
    {
        case MovePriority::Idle:   return "idle";
        case MovePriority::Wander: return "wander";
        case MovePriority::Normal: return "normal";
        case MovePriority::Combat: return "combat";
        case MovePriority::Forced: return "forced";
        default:                   return "";
    }
}

std::string SlugOf(std::string const& name)
{
    std::string out;
    out.reserve(name.size());
    for (char c : name)
    {
        if (std::isalnum(static_cast<unsigned char>(c)))
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        else if (!out.empty() && out.back() != '-')
            out += '-';
    }

    while (!out.empty() && out.back() == '-')
        out.pop_back();

    return out.empty() ? "pull" : out;
}

// --- session ------------------------------------------------------------------

struct DamageEntry
{
    uint32 ms;
    uint64 source;
    uint32 spellId;
    uint32 amount;
};

// One verdict out of an engine pass. A veto carries the multiplier that zeroed the action in
// `verdict` and has no relevance of its own.
struct TickEntry
{
    uint32 ms;
    bool veto;
    std::string action;
    float relevance;
    std::string verdict;
};

// A whole engine pass' ordered verdict set, plus how many passes running produced it. A pass walks
// several action nodes and emits a verdict for each, in the same order every tick, so no single
// verdict is ever news on its own - the set is, and the repeat count is the steady state.
struct TickRecord
{
    uint32 firstMs;
    uint32 lastMs;
    uint32 repeats;
    std::vector<TickEntry> entries;
};

// What was on a bot and when. Removal stamps `removedMs` rather than dropping the entry, because the
// core's own aura strip runs through the same hook moments before a death record is built.
struct AuraState
{
    uint64 caster = 0;
    uint32 stacks = 0;
    int32 duration = 0;
    uint32 appliedMs = 0;
    uint32 removedMs = 0;
    bool positive = false;
};

struct ActionLatch
{
    std::string verdict;
    uint32 ms = 0;
    bool seen = false;
};

// Per-bot state that only exists to answer "has this changed since last time", plus the rewind rings a
// death record is built from.
struct BotTrace
{
    uint32 lastTarget = 0;
    std::deque<DamageEntry> damage;
    // The blow that took the bot under, caught in Unit::DealDamage. The combat-log hooks the rest of
    // the damage stream comes from miss anything that sends no log packet - environmental damage, a
    // script kill - and a bot that dies to one of those otherwise leaves no trace of what did it.
    uint64 killBlowSource = 0;
    uint32 killBlowAmount = 0;
    float lastHpPct = 0.0f;
    uint32 lastHpMs = 0;
    // The pass being buffered, and the run-length-encoded history a death record replays.
    std::vector<TickEntry> tick;
    std::deque<TickRecord> ticks;
    // Last verdict written to the stream per action. The whole-pass compare below it is the wrong
    // grain on its own: relevance drifts reorder the priority queue every tick, so two passes almost
    // never match, while the individual verdicts inside them hold for seconds at a time.
    std::unordered_map<std::string, ActionLatch> emitted;
    std::unordered_map<uint32 /*spellId*/, AuraState> auras;
    // Last value emitted for each NoteDerived key, so a helper can be probed on every call.
    std::unordered_map<std::string, std::string> derived;
    float lastMoveX = 0.0f, lastMoveY = 0.0f, lastMoveZ = 0.0f;
    std::string lastMoveBy;
    bool hasMove = false;
    uint32 lastRejectMs = 0;
    std::string lastRejectBy;
    MoveKind lastRejectKind = MoveKind::Point;
    bool hasReject = false;
    // Follow and Chase have no destination to compare, so they latch on who is being tracked instead.
    std::string lastTrackBy;
    MoveKind lastTrackKind = MoveKind::Point;
    uint64 lastTrackTarget = 0;
    bool hasTrack = false;
};

struct PreRollEntry
{
    uint32 ms;
    std::string payload;
};

// The sample accumulator lives with the ring rather than in a thread_local, so both are dropped
// together when the instance goes away instead of one node per instance leaking for the process life.
struct PreRollRing
{
    uint32 accumMs = 0;
    std::deque<PreRollEntry> entries;
};

struct ObsSession
{
    uint32 mapId = 0;
    uint32 instanceId = 0;
    Map* map = nullptr;
    std::string bossSlug;
    std::string path;

    uint32 startMs = 0;
    std::ofstream file;
    std::string buffer;
    // Read from the world thread by the .playerbots debug obs command while the owning map thread is
    // still writing, so these two cannot be plain fields. Everything else here is map-thread only.
    std::atomic<uint64> bytes{0};
    std::atomic<uint32> rosterSize{0};
    bool capped = false;

    uint32 sinceSnapshotMs = 0;
    uint32 sinceRosterMs = 0;
    uint32 sinceFlushMs = 0;
    uint32 lastCombatMs = 0;
    bool sawMostlyDead = false;

    std::vector<ObjectGuid> roster;
    std::unordered_set<uint64> seenUnits;
    std::unordered_set<uint32> seenSpells;
    std::unordered_set<ObjectGuid> watched;
    std::unordered_map<uint64, BotTrace> bots;

    // getMSTime() wraps every ~49.7 days. The unsigned subtraction wraps with it, so reading the
    // result back as int32 gives the true signed gap in both directions - pre-roll samples are older
    // than the start and must stay negative, which is what marks them as leading up to the pull.
    int64 Stamp(uint32 ms) const { return static_cast<int32>(ms - startMs); }
};

std::mutex g_registryMutex;
std::unordered_map<uint32, std::unique_ptr<ObsSession>> g_sessions;
std::unordered_map<uint32, PreRollRing> g_preRoll;
// Boss ids whose state the instance script tried to change, waiting for the next map update to read
// back what the core actually settled on.
std::unordered_map<uint32, std::vector<uint32>> g_pendingBossState;
// Bumped on every open and close so map threads can tell their cached answers are stale.
std::atomic<uint32> g_registryGeneration{0};

bool MapIsTracked(Map* map);

// The registry is the only shared structure; a session body is touched solely by the map thread that
// owns it, so it needs no lock of its own.
//
// Damage, heal and aura probes call this once per event, so the answer is memoised per map thread and
// thrown away whenever a session opens or closes. Without that, one recording raid would put every
// other instance on the server through the same global lock thousands of times a tick.
ObsSession* FindSession(uint32 instanceId)
{
    thread_local uint32 t_cachedGeneration = 0;
    thread_local std::unordered_map<uint32, ObsSession*> t_cache;

    uint32 const generation = g_registryGeneration.load(std::memory_order_acquire);
    if (generation != t_cachedGeneration)
    {
        t_cache.clear();
        t_cachedGeneration = generation;
    }

    auto cached = t_cache.find(instanceId);
    if (cached != t_cache.end())
        return cached->second;

    // A server that goes a long time between pulls never bumps the generation, so the cache would
    // otherwise keep one node per instance this thread has ever seen.
    if (t_cache.size() >= 256)
        t_cache.clear();

    ObsSession* found = nullptr;
    {
        std::lock_guard<std::mutex> guard(g_registryMutex);
        auto it = g_sessions.find(instanceId);
        found = it == g_sessions.end() ? nullptr : it->second.get();
    }

    // Only memoise an answer nothing invalidated while it was being looked up.
    if (g_registryGeneration.load(std::memory_order_acquire) == generation)
        t_cache[instanceId] = found;

    return found;
}

ObsSession* SessionFor(Unit* unit)
{
    if (!unit || !unit->IsInWorld())
        return nullptr;

    Map* map = unit->GetMap();

    // Filter on the map before the registry lock. Damage hooks fire for the whole server, so without
    // this every open-world fight would contend the session mutex whenever a raid was recording.
    if (!map || !MapIsTracked(map))
        return nullptr;

    return FindSession(map->GetInstanceId());
}

// Call with the registry lock held and after the map itself has changed.
void RefreshActiveFlag()
{
    g_active.store(!g_sessions.empty(), std::memory_order_release);
    g_registryGeneration.fetch_add(1, std::memory_order_acq_rel);
}

bool MapIsTracked(Map* map)
{
    if (!map || !map->IsDungeon())
        return false;

    // Raids only unless the map is named explicitly. The pre-roll ring samples every tracked map for
    // as long as players stand on it, so defaulting to every 5-man would have the whole server
    // building snapshots four times a second for dungeons nobody is diagnosing.
    if (g_cfg.allMaps)
        return map->IsRaid();

    return g_cfg.maps.count(map->GetId()) != 0;
}

// `force` writes past the size cap. Only the end record uses it: without one, postmortem.py reports
// a clean kill that happened to hit the cap as a trace cut short by a crash.
void RawWrite(ObsSession& s, std::string const& line, bool force = false)
{
    if (s.capped && !force)
        return;

    s.buffer += line;
    s.buffer += '\n';
    uint64 const total = s.bytes.load(std::memory_order_relaxed) + line.size() + 1;
    s.bytes.store(total, std::memory_order_relaxed);

    if (!s.capped && total >= g_cfg.maxFileBytes)
    {
        s.buffer += "{\"e\":\"truncated\"}\n";
        s.capped = true;
    }

    if (s.buffer.size() >= 64 * 1024)
    {
        s.file << s.buffer;
        s.buffer.clear();
    }
}

void Flush(ObsSession& s)
{
    if (!s.buffer.empty())
    {
        s.file << s.buffer;
        s.buffer.clear();
    }

    s.file.flush();
}

void Emit(ObsSession& s, uint32 ms, char const* event, std::string const& fields, bool force = false)
{
    std::string line = "{\"t\":";
    line += std::to_string(s.Stamp(ms));
    line += ",\"e\":\"";
    line += event;
    line += "\"";
    if (!fields.empty())
    {
        line += ",";
        line += fields;
    }
    line += "}";

    RawWrite(s, line, force);
}

std::string RoleOf(Player* player);

// Emitted once per guid per trace, so later records can carry a bare guid instead of repeating names.
void EnsureUnit(ObsSession& s, Unit* unit)
{
    if (!unit)
        return;

    uint64 const key = GuidKey(unit->GetGUID());
    if (!s.seenUnits.insert(key).second)
        return;

    Creature* creature = unit->ToCreature();
    std::string fields = "\"g\":" + std::to_string(key);
    fields += ",\"en\":" + std::to_string(creature ? creature->GetEntry() : 0);
    fields += ",\"n\":" + Quoted(unit->GetName());
    fields += ",\"lvl\":" + std::to_string(unit->GetLevel());
    fields += ",\"mhp\":" + std::to_string(unit->GetMaxHealth());
    fields += ",\"b\":" + std::string(creature && (creature->isWorldBoss() || creature->IsDungeonBoss()) ? "1" : "0");

    // Class and role for a player, so somebody who zoned in after the header was written still reads
    // as a name and a role rather than a bare guid.
    if (Player* player = unit->ToPlayer())
    {
        fields += ",\"c\":" + std::to_string(player->getClass());
        fields += ",\"r\":" + Quoted(RoleOf(player));
        fields += ",\"h\":" + std::string(GET_PLAYERBOT_AI(player) ? "0" : "1");
    }

    Emit(s, getMSTime(), "unit", fields);
}

// Same trick as EnsureUnit, for spells. Without it a death block reads "spell 63511" and nobody can
// tell that from Biting Cold without a DBC to hand, which defeats the point of a self-contained file.
void EnsureSpell(ObsSession& s, uint32 spellId)
{
    if (!spellId || !s.seenSpells.insert(spellId).second)
        return;

    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    char const* name = info ? info->SpellName[LOCALE_enUS] : nullptr;

    std::string fields = "\"sp\":" + std::to_string(spellId);
    fields += ",\"n\":" + Quoted(name ? name : "?");

    Emit(s, getMSTime(), "spell", fields);
}

// Any player standing on the recorded map, not only the roster the header snapshotted: a bot
// summoned or resurrected mid-pull has to be traced from its first event rather than from the next
// roster refresh three seconds later. Writing the name record here is the only chance a late
// arrival gets one.
bool TracksPlayer(ObsSession& s, Unit* unit)
{
    if (!unit || !unit->IsPlayer() || unit->GetMap() != s.map)
        return false;

    EnsureUnit(s, unit);
    return true;
}

std::string RoleOf(Player* player)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(player);
    if (!botAI)
        return "human";

    if (botAI->IsTank(player))
        return "tank";
    if (botAI->IsHeal(player))
        return "heal";
    if (botAI->IsRanged(player))
        return "ranged";

    return "melee";
}

void RebuildRoster(ObsSession& s)
{
    s.roster.clear();

    Map* map = s.map;
    if (map)
    {
        Map::PlayerList const& players = map->GetPlayers();
        for (Map::PlayerList::const_iterator it = players.begin(); it != players.end(); ++it)
            if (Player* player = it->GetSource())
                if (player->IsInWorld())
                    s.roster.push_back(player->GetGUID());
    }

    s.rosterSize.store(static_cast<uint32>(s.roster.size()), std::memory_order_relaxed);
}

std::string RosterJson(ObsSession& s)
{
    std::string out = "[";
    bool first = true;
    for (ObjectGuid guid : s.roster)
    {
        Player* player = ObjectAccessor::FindPlayer(guid);
        if (!player)
            continue;

        if (!first)
            out += ",";
        first = false;

        out += "{\"g\":" + std::to_string(GuidKey(guid));
        out += ",\"n\":" + Quoted(player->GetName());
        out += ",\"c\":" + std::to_string(player->getClass());
        out += ",\"r\":" + Quoted(RoleOf(player));
        out += ",\"h\":" + std::string(GET_PLAYERBOT_AI(player) ? "0" : "1");
        out += "}";
    }

    out += "]";
    return out;
}

// --- snapshots ----------------------------------------------------------------

std::string UnitRow(Unit* unit)
{
    uint32 castingId = 0;
    for (uint32 type = 0; type < CURRENT_MAX_SPELL; ++type)
        if (Spell* spell = unit->GetCurrentSpell(type))
        {
            castingId = spell->GetSpellInfo()->Id;
            break;
        }

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
std::string SweepArea(ObsSession& s, Unit* anchor, std::string& units, bool& firstUnit)
{
    std::list<WorldObject*> objs;
    Acore::AllWorldObjectsInRange check(anchor, OBS_HAZARD_SWEEP_RADIUS);
    Acore::WorldObjectListSearcher<Acore::AllWorldObjectsInRange> searcher(
        anchor, objs, check, GRID_MAP_TYPE_MASK_DYNAMICOBJECT | GRID_MAP_TYPE_MASK_CREATURE);
    Cell::VisitObjects(anchor, searcher, OBS_HAZARD_SWEEP_RADIUS);

    std::string out = "[";
    bool firstHazard = true;
    std::size_t swept = 0;

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

            EnsureSpell(s, dyn->GetSpellId());

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
        if (!creature || !creature->IsAlive() || swept >= OBS_MAX_WATCHED)
            continue;

        if (s.watched.count(creature->GetGUID()) || !anchor->IsHostileTo(creature))
            continue;

        ++swept;
        EnsureUnit(s, creature);

        if (!firstUnit)
            units += ",";
        firstUnit = false;
        units += UnitRow(creature);
    }

    out += "]";
    return out;
}

// Built the same way with or without a session, so a flushed pre-roll and live sampling produce the
// same row shape. Only the sweep differs - pre-roll has no session and cannot afford it.
std::string BuildSnapshotPayload(Map* map, std::vector<ObjectGuid> const& roster,
                                 std::unordered_set<ObjectGuid> const& watched, ObsSession* session)
{
    std::string units = "[";
    bool first = true;
    Unit* anchor = nullptr;

    for (ObjectGuid guid : roster)
    {
        Player* player = ObjectAccessor::FindPlayer(guid);
        if (!player || !player->IsInWorld() || player->GetMap() != map)
            continue;

        if (!anchor)
            anchor = player;

        // The last health this bot was seen at, so a death the damage hooks never saw can still say
        // what it fell from and how long ago that reading was.
        if (session)
        {
            BotTrace& trace = session->bots[GuidKey(guid)];
            trace.lastHpPct = player->GetHealthPct();
            trace.lastHpMs = getMSTime();
        }

        if (!first)
            units += ",";
        first = false;
        units += UnitRow(player);
    }

    for (ObjectGuid guid : watched)
    {
        Creature* creature = map->GetCreature(guid);
        if (!creature || !creature->IsInWorld() || !creature->IsAlive())
            continue;

        if (!first)
            units += ",";
        first = false;
        units += UnitRow(creature);
    }

    // The sweep visits every grid cell within 150 yards, which is far too much to run four times a
    // second on a map that is not in a pull. Pre-roll keeps the row so the shape does not change.
    std::string const hazards = session && anchor ? SweepArea(*session, anchor, units, first) : "[]";
    units += "]";

    return "\"u\":" + units + ",\"hz\":" + hazards;
}

// A boss outranks trash for the bounded slots. The cap is filled first-come, so without eviction a
// boss that engages after forty adds is dropped and no death ever measures its distance.
void WatchCreature(ObsSession& s, Creature* creature)
{
    if (!creature || s.watched.count(creature->GetGUID()))
        return;

    bool const isBoss = creature->isWorldBoss() || creature->IsDungeonBoss();
    if (s.watched.size() >= OBS_MAX_WATCHED)
    {
        if (!isBoss)
            return;

        auto evict = s.watched.end();
        for (auto it = s.watched.begin(); it != s.watched.end(); ++it)
        {
            Creature* held = s.map ? s.map->GetCreature(*it) : nullptr;
            if (!held || !(held->isWorldBoss() || held->IsDungeonBoss()))
            {
                evict = it;
                break;
            }
        }

        if (evict == s.watched.end())
            return;

        s.watched.erase(evict);
    }

    s.watched.insert(creature->GetGUID());
    EnsureUnit(s, creature);
}

// Everything already swinging at the raid when the trace opens. A session started by MarkPull or by a
// boss state change arrives after those creatures entered combat, so without this a gauntlet records
// players only and every death.dist comes out empty.
void SeedWatched(ObsSession& s, Unit* source)
{
    if (Creature* creature = source ? source->ToCreature() : nullptr)
        WatchCreature(s, creature);

    Map* map = s.map;
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
                WatchCreature(s, creature);
    }
}

void PruneWatched(ObsSession& s)
{
    for (auto it = s.watched.begin(); it != s.watched.end();)
    {
        Creature* creature = s.map ? s.map->GetCreature(*it) : nullptr;
        if (!creature || !creature->IsInWorld() || !creature->IsAlive())
            it = s.watched.erase(it);
        else
            ++it;
    }
}

// Removals are held for the death rewind window rather than dropped, so this is the only thing that
// bounds the tracked set.
void PruneAuras(ObsSession& s, uint32 now)
{
    for (auto& entry : s.bots)
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

// --- verdict ticks ------------------------------------------------------------

// Relevance is deliberately left out: it drifts by thousandths between passes, and comparing it would
// make every pass look new - which is the bug a per-verdict comparison had.
bool SameTick(std::vector<TickEntry> const& left, std::vector<TickEntry> const& right)
{
    if (left.size() != right.size())
        return false;

    for (std::size_t i = 0; i < left.size(); ++i)
        if (left[i].veto != right[i].veto || left[i].action != right[i].action ||
            left[i].verdict != right[i].verdict)
            return false;

    return true;
}

void EmitTickEntry(ObsSession& s, uint64 key, TickEntry const& entry)
{
    std::string fields = "\"g\":" + std::to_string(key);
    if (entry.veto)
    {
        fields += ",\"m\":" + Quoted(entry.verdict);
        fields += ",\"a\":" + Quoted(entry.action);
        Emit(s, entry.ms, "veto", fields);
        return;
    }

    fields += ",\"a\":" + Quoted(entry.action);
    fields += ",\"rel\":" + Num(entry.relevance);
    fields += ",\"vd\":" + Quoted(entry.verdict);
    Emit(s, entry.ms, "act", fields);
}

// Measured on the first v4 Hodir trace: 40% of the file was act records repeating the previous pass'
// answer for the same action. A pass repeating verbatim is rare; a verdict repeating is the norm.
bool ShouldEmit(BotTrace& trace, TickEntry const& entry)
{
    ActionLatch& latch = trace.emitted[(entry.veto ? "v" : "a") + entry.action];
    if (latch.seen && latch.verdict == entry.verdict &&
        getMSTimeDiff(latch.ms, entry.ms) < OBS_ACTION_REPEAT_MS)
        return false;

    latch.seen = true;
    latch.verdict = entry.verdict;
    latch.ms = entry.ms;
    return true;
}

// Closes the pass being buffered. Only a pass whose verdict set differs from the one before is written
// out; the rest become a repeat count on the record already held, which is what a death record replays.
void FlushTick(ObsSession& s, uint64 key, BotTrace& trace)
{
    if (trace.tick.empty())
        return;

    uint32 const firstMs = trace.tick.front().ms;

    // A pass that decided exactly what the last one decided is not news - but a bot that has been
    // losing the same tick for minutes still has to leave a trail, or its timeline reads as a gap.
    if (!trace.ticks.empty() && SameTick(trace.ticks.back().entries, trace.tick) &&
        getMSTimeDiff(trace.ticks.back().firstMs, firstMs) < OBS_ACTION_REPEAT_MS)
    {
        ++trace.ticks.back().repeats;
        trace.ticks.back().lastMs = firstMs;
        trace.tick.clear();
        return;
    }

    for (TickEntry const& entry : trace.tick)
        if (ShouldEmit(trace, entry))
            EmitTickEntry(s, key, entry);

    trace.ticks.push_back({firstMs, firstMs, 1, std::move(trace.tick)});
    trace.tick.clear();

    while (!trace.ticks.empty() && getMSTimeDiff(trace.ticks.front().lastMs, firstMs) > g_cfg.deathVerdictMs)
        trace.ticks.pop_front();
}

// --- lifecycle ----------------------------------------------------------------

std::string ResolveBossName(Map* map, Unit* source)
{
    if (!source)
        return map ? map->GetMapName() : "pull";

    // The DBC encounter list is keyed by the credit creature's entry, not by the boss index the
    // instance script reports, so the engaging creature is what can be matched against it.
    if (Creature* creature = source->ToCreature())
    {
        DungeonEncounterList const* encounters =
            sObjectMgr->GetDungeonEncounterList(map->GetId(), map->GetDifficulty());
        if (encounters)
            for (DungeonEncounter const* encounter : *encounters)
                if (encounter->creditEntry == creature->GetEntry() && encounter->dbcEntry)
                    return encounter->dbcEntry->encounterName[0];
    }

    return source->GetName();
}

// The instance script reports a boss index, not a creature, and the DBC encounter list is keyed by
// creature entry - so a trace opened off a state change has to find the boss the raid is swinging at
// before it can name itself anything better than the map.
Unit* FindEngagedBoss(Map* map)
{
    if (!map)
        return nullptr;

    Map::PlayerList const& players = map->GetPlayers();
    for (Map::PlayerList::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        Player* player = it->GetSource();
        if (!player || !player->IsInWorld())
            continue;

        for (Unit* attacker : player->getAttackers())
            if (Creature* creature = attacker ? attacker->ToCreature() : nullptr)
                if (creature->isWorldBoss() || creature->IsDungeonBoss())
                    return creature;
    }

    return nullptr;
}

void OpenSession(Map* map, Unit* source, char const* trigger)
{
    if (!g_cfg.enabled || !MapIsTracked(map))
        return;

    uint32 const instanceId = map->GetInstanceId();
    if (FindSession(instanceId))
        return;

    auto session = std::make_unique<ObsSession>();
    ObsSession& s = *session;
    s.map = map;
    s.mapId = map->GetId();
    s.instanceId = instanceId;
    s.startMs = getMSTime();
    s.lastCombatMs = s.startMs;
    s.bossSlug = SlugOf(ResolveBossName(map, source));

    RebuildRoster(s);
    if (s.roster.empty())
        return;

    std::error_code ec;
    std::filesystem::path dir = std::filesystem::path(g_logsDir) / g_cfg.dir;
    std::filesystem::create_directories(dir, ec);

    time_t const now = time(nullptr);
    std::string name = std::to_string(s.mapId) + "_" + std::to_string(instanceId) + "_" + s.bossSlug + "_" +
                       std::to_string(static_cast<uint64>(now)) + ".ndjson";
    s.path = (dir / name).string();

    s.file.open(s.path, std::ios::out | std::ios::trunc);
    if (!s.file.is_open())
    {
        LOG_ERROR("playerbots", "RaidObs: cannot open trace {}", s.path);
        return;
    }

    std::string hdr = "{\"v\":" + std::to_string(SCHEMA_VERSION);
    hdr += ",\"e\":\"hdr\",\"ts\":" + std::to_string(static_cast<uint64>(now) * 1000);
    hdr += ",\"map\":" + std::to_string(s.mapId);
    hdr += ",\"inst\":" + std::to_string(instanceId);
    hdr += ",\"diff\":" + std::to_string(static_cast<uint32>(map->GetDifficulty()));
    hdr += ",\"boss\":" + Quoted(s.bossSlug);
    hdr += ",\"roster\":" + RosterJson(s);
    hdr += "}";
    RawWrite(s, hdr);

    // Pre-roll first, so the trace opens before the pull rather than at it. These carry a negative
    // stamp, which is what marks them as leading up to the engage.
    //
    // Taken out from under the lock before being written: a 25-man ring is hundreds of kilobytes and
    // flushing it inline would block every other map thread's probe path on a disk write.
    std::deque<PreRollEntry> preRoll;
    {
        std::lock_guard<std::mutex> guard(g_registryMutex);
        auto it = g_preRoll.find(instanceId);
        if (it != g_preRoll.end())
        {
            preRoll = std::move(it->second.entries);
            g_preRoll.erase(it);
        }
    }

    for (PreRollEntry const& entry : preRoll)
        Emit(s, entry.ms, "snap", entry.payload);

    Emit(s, s.startMs, "pull",
         "\"boss\":" + Quoted(s.bossSlug) + ",\"src\":\"" + trigger + "\"");

    SeedWatched(s, source);

    {
        std::lock_guard<std::mutex> guard(g_registryMutex);
        g_sessions[instanceId] = std::move(session);
        RefreshActiveFlag();
    }

    LOG_INFO("playerbots", "RaidObs: recording {} -> {}", s.bossSlug, s.path);
}

// Only asked at the close, and never for `shutdown` or `mapgone`, which are the two outcomes that can
// reach here with the map thread pool already gone.
bool RosterMostlyDead(ObsSession& s)
{
    uint32 present = 0;
    uint32 dead = 0;
    for (ObjectGuid guid : s.roster)
    {
        Player* player = ObjectAccessor::FindPlayer(guid);
        if (!player || !player->IsInWorld())
            continue;

        ++present;
        if (!player->IsAlive())
            ++dead;
    }

    return present && static_cast<float>(dead) / static_cast<float>(present) > OBS_WIPE_DEAD_SHARE;
}

void CloseSession(uint32 instanceId, char const* outcome)
{
    std::unique_ptr<ObsSession> session;
    {
        std::lock_guard<std::mutex> guard(g_registryMutex);
        auto it = g_sessions.find(instanceId);
        if (it == g_sessions.end())
            return;

        session = std::move(it->second);
        g_sessions.erase(it);
        RefreshActiveFlag();
    }

    // The pass each bot was mid-way through has nothing after it to close it.
    for (auto& entry : session->bots)
        FlushTick(*session, entry.first, entry.second);

    // Hodir's script reports NOT_STARTED when the raid releases, so a 23-of-24 wipe was filed under
    // `reset`. What the roster looked like outranks what the script settled on - taken from the latch
    // first, since by the time an idle close runs the raid has had 30 seconds to run back alive.
    char const* result = outcome;
    if ((!strcmp(outcome, "reset") || !strcmp(outcome, "idle")) &&
        (session->sawMostlyDead || RosterMostlyDead(*session)))
        result = "wipe";

    Emit(*session, getMSTime(), "end", std::string("\"out\":\"") + result + "\"", true);
    Flush(*session);
    session->file.close();

    LOG_INFO("playerbots", "RaidObs: closed {} ({}, {} bytes)", session->path, result,
             session->bytes.load(std::memory_order_relaxed));
}

// The instance-script hook fires before the core has decided anything: it discards the change while
// a boss loads from the DB, and refuses DONE while a world-boss minion is still alive. So the hook
// only queues the boss id and the state that actually stuck is read here, one map update later -
// which is also after the engage hook has had its chance to name the trace after the creature.
void ProcessPendingBossState(Map* map, uint32 instanceId)
{
    std::vector<uint32> pending;
    {
        std::lock_guard<std::mutex> guard(g_registryMutex);
        auto it = g_pendingBossState.find(instanceId);
        if (it == g_pendingBossState.end())
            return;

        pending = std::move(it->second);
        g_pendingBossState.erase(it);
    }

    InstanceMap* instance = map->ToInstanceMap();
    InstanceScript* script = instance ? instance->GetInstanceScript() : nullptr;
    if (!script)
        return;

    for (uint32 bossId : pending)
    {
        switch (script->GetBossState(bossId))
        {
            case IN_PROGRESS:
                OpenSession(map, FindEngagedBoss(map), "bossstate");
                break;
            case DONE:
                CloseSession(instanceId, "kill");
                break;
            case FAIL:
                CloseSession(instanceId, "wipe");
                break;
            case NOT_STARTED:
                CloseSession(instanceId, "reset");
                break;
            default:
                break;
        }
    }
}

bool AnyRaidMemberInCombat(ObsSession& s)
{
    for (ObjectGuid guid : s.roster)
        if (Player* player = ObjectAccessor::FindPlayer(guid))
            if (player->IsInWorld() && player->IsInCombat())
                return true;

    return false;
}

// --- retention ----------------------------------------------------------------

void ApplyRetention()
{
    if (g_logsDir.empty())
        return;

    std::filesystem::path dir = std::filesystem::path(g_logsDir) / g_cfg.dir;
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec))
        return;

    struct Entry
    {
        std::filesystem::path path;
        std::filesystem::file_time_type time;
        uint64 size;
    };

    std::vector<Entry> entries;
    uint64 total = 0;

    for (auto const& item : std::filesystem::directory_iterator(dir, ec))
    {
        // A trace another process still holds open - a copy in flight, a scanner - reports neither a
        // size nor a write time. Taking those failures as zero-byte and epoch-old would sort the file
        // first and delete it, so a stat that failed drops the entry instead.
        std::error_code itemEc;
        if (!item.is_regular_file(itemEc) || itemEc || item.path().extension() != ".ndjson")
            continue;

        uint64 const size = static_cast<uint64>(item.file_size(itemEc));
        if (itemEc)
            continue;

        auto const written = item.last_write_time(itemEc);
        if (itemEc)
            continue;

        entries.push_back({item.path(), written, size});
        total += size;
    }

    std::sort(entries.begin(), entries.end(), [](Entry const& a, Entry const& b) { return a.time < b.time; });

    auto const now = std::filesystem::file_time_type::clock::now();
    uint32 removed = 0;

    for (Entry const& entry : entries)
    {
        bool expired = false;
        if (g_cfg.retentionDays)
        {
            auto const age = std::chrono::duration_cast<std::chrono::hours>(now - entry.time).count();
            expired = age > static_cast<long long>(g_cfg.retentionDays) * 24;
        }

        bool overSize = g_cfg.maxDirBytes && total > g_cfg.maxDirBytes;

        if (!expired && !overSize)
            continue;

        std::filesystem::remove(entry.path, ec);
        total -= std::min(total, entry.size);
        ++removed;
    }

    if (removed)
        LOG_INFO("playerbots", "RaidObs: retention removed {} trace(s)", removed);
}

thread_local char const* t_currentAction = nullptr;
thread_local Player* t_currentBot = nullptr;
}  // namespace

namespace RaidObs
{
void LoadConfig()
{
    g_cfg.enabled = sPlayerbotAIConfig.obsEnabled;
    g_cfg.dir = sPlayerbotAIConfig.obsDir;
    g_cfg.snapshotIntervalMs = sPlayerbotAIConfig.obsSnapshotIntervalMs;
    g_cfg.preRollMs = sPlayerbotAIConfig.obsPreRollSeconds * 1000;
    g_cfg.logHeals = sPlayerbotAIConfig.obsLogHeals;
    g_cfg.logAuras = sPlayerbotAIConfig.obsLogAuras;
    g_cfg.minDamage = sPlayerbotAIConfig.obsMinDamageToLog;
    g_cfg.deathRewindMs = sPlayerbotAIConfig.obsDeathRewindMs;
    g_cfg.deathVerdictMs = sPlayerbotAIConfig.obsDeathVerdictMs;
    g_cfg.idleCloseMs = sPlayerbotAIConfig.obsIdleCloseSeconds * 1000;
    g_cfg.retentionDays = sPlayerbotAIConfig.obsRetentionDays;
    g_cfg.maxDirBytes = static_cast<uint64>(sPlayerbotAIConfig.obsMaxDirMB) * 1024 * 1024;
    g_cfg.maxFileBytes = static_cast<uint64>(sPlayerbotAIConfig.obsMaxFileMB) * 1024 * 1024;

    g_cfg.maps.clear();
    std::string const& raw = sPlayerbotAIConfig.obsMaps;
    g_cfg.allMaps = raw.empty();
    std::size_t start = 0;
    while (!raw.empty() && start <= raw.size())
    {
        std::size_t const comma = raw.find(',', start);
        std::string token = raw.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        token.erase(0, token.find_first_not_of(" \t"));
        if (auto const last = token.find_last_not_of(" \t"); last != std::string::npos)
            token.erase(last + 1);

        if (!token.empty())
            g_cfg.maps.insert(static_cast<uint32>(std::strtoul(token.c_str(), nullptr, 10)));

        if (comma == std::string::npos)
            break;

        start = comma + 1;
    }

    g_logsDir = sConfigMgr->GetOption<std::string>("LogsDir", "", false);
    if (!g_logsDir.empty() && g_logsDir.back() != '/' && g_logsDir.back() != '\\')
        g_logsDir += '/';

    if (g_cfg.enabled)
    {
        ApplyRetention();
        LOG_INFO("playerbots", "RaidObs: enabled, dir={}{}, snapshot={}ms", g_logsDir, g_cfg.dir,
                 g_cfg.snapshotIntervalMs);
    }
}

// Runs from Main.cpp after WorldUpdateLoop has returned and the map thread pool is gone, which is
// the only reason a world-thread caller may close sessions and write their files.
void Shutdown()
{
    std::vector<uint32> ids;
    {
        std::lock_guard<std::mutex> guard(g_registryMutex);
        for (auto const& entry : g_sessions)
            ids.push_back(entry.first);
    }

    for (uint32 id : ids)
        CloseSession(id, "shutdown");
}

void OnMapUpdate(Map* map, uint32 diff)
{
    if (!g_cfg.enabled || !MapIsTracked(map))
        return;

    uint32 const instanceId = map->GetInstanceId();
    uint32 const now = getMSTime();

    ProcessPendingBossState(map, instanceId);

    ObsSession* session = FindSession(instanceId);
    if (!session)
    {
        // No pull open: keep sampling into a ring so the trace can start before the engage.
        if (!g_cfg.preRollMs)
            return;

        // The accumulator lives with the ring rather than in a thread_local, so both are dropped
        // together when the instance goes away instead of one node leaking per instance ever created.
        {
            std::lock_guard<std::mutex> guard(g_registryMutex);
            PreRollRing& ring = g_preRoll[instanceId];
            ring.accumMs += diff;
            if (ring.accumMs < g_cfg.snapshotIntervalMs)
                return;

            ring.accumMs = 0;
        }

        std::vector<ObjectGuid> roster;
        Map::PlayerList const& players = map->GetPlayers();
        for (Map::PlayerList::const_iterator it = players.begin(); it != players.end(); ++it)
            if (Player* player = it->GetSource())
                if (player->IsInWorld())
                    roster.push_back(player->GetGUID());

        if (roster.empty())
            return;

        std::string payload = BuildSnapshotPayload(map, roster, {}, nullptr);

        std::lock_guard<std::mutex> guard(g_registryMutex);
        PreRollRing& ring = g_preRoll[instanceId];
        ring.entries.push_back({now, std::move(payload)});
        while (!ring.entries.empty() && getMSTimeDiff(ring.entries.front().ms, now) > g_cfg.preRollMs)
            ring.entries.pop_front();

        return;
    }

    ObsSession& s = *session;
    s.sinceSnapshotMs += diff;
    s.sinceRosterMs += diff;
    s.sinceFlushMs += diff;

    if (s.sinceRosterMs >= OBS_ROSTER_REFRESH_MS)
    {
        s.sinceRosterMs = 0;
        RebuildRoster(s);
        PruneWatched(s);
        PruneAuras(s, now);
    }

    if (s.sinceSnapshotMs >= g_cfg.snapshotIntervalMs)
    {
        // Subtracted rather than cleared: clearing throws away the overshoot every tick, which turned a
        // configured 250 ms into a measured 314 ms. A tick long enough to owe two samples banks no
        // credit for the one it missed - that sample is gone either way.
        s.sinceSnapshotMs -= g_cfg.snapshotIntervalMs;
        if (s.sinceSnapshotMs >= g_cfg.snapshotIntervalMs)
            s.sinceSnapshotMs = 0;

        Emit(s, now, "snap", BuildSnapshotPayload(map, s.roster, s.watched, &s));
    }

    if (s.sinceFlushMs >= OBS_FLUSH_INTERVAL_MS)
    {
        s.sinceFlushMs = 0;
        Flush(s);
    }

    if (AnyRaidMemberInCombat(s))
    {
        s.lastCombatMs = now;

        // Sampled during the fight rather than at the close, where idleCloseMs has already given the
        // raid 30 seconds to release and run back - long enough that the check in CloseSession sees a
        // healthy roster and files a 31-death attempt as `idle`.
        s.sawMostlyDead = s.sawMostlyDead || RosterMostlyDead(s);
    }
    else if (g_cfg.idleCloseMs && getMSTimeDiff(s.lastCombatMs, now) > g_cfg.idleCloseMs)
        CloseSession(instanceId, "idle");
}

void OnMapDestroyed(Map* map)
{
    if (!map)
        return;

    uint32 const instanceId = map->GetInstanceId();
    CloseSession(instanceId, "mapgone");

    std::lock_guard<std::mutex> guard(g_registryMutex);
    g_preRoll.erase(instanceId);
    g_pendingBossState.erase(instanceId);
}

void OnBossState(uint32 bossId, Map* map)
{
    if (!g_cfg.enabled || !MapIsTracked(map))
        return;

    std::lock_guard<std::mutex> guard(g_registryMutex);
    std::vector<uint32>& pending = g_pendingBossState[map->GetInstanceId()];
    if (std::find(pending.begin(), pending.end(), bossId) == pending.end())
        pending.push_back(bossId);
}

void OnCreatureEngage(Unit* creature, Unit* victim)
{
    if (!g_cfg.enabled || !creature || !victim || !victim->IsPlayer())
        return;

    Creature* asCreature = creature->ToCreature();
    if (!asCreature)
        return;

    Map* map = creature->GetMap();
    if (!MapIsTracked(map))
        return;

    bool const isBoss = asCreature->isWorldBoss() || asCreature->IsDungeonBoss();

    ObsSession* session = FindSession(map->GetInstanceId());
    if (!session)
    {
        if (isBoss)
            OpenSession(map, creature, "engage");

        session = FindSession(map->GetInstanceId());
        if (!session)
            return;
    }

    WatchCreature(*session, asCreature);
}

void MarkPull(Map* map, Unit* source) { OpenSession(map, source, "mark"); }

void NoteDamage(Unit* attacker, Unit* victim, SpellInfo const* spell, uint32 amount, int32 overkill,
                uint32 schoolMask, uint32 absorb, uint32 resist)
{
    if (!Active() || !victim || amount < g_cfg.minDamage)
        return;

    ObsSession* session = SessionFor(victim);
    if (!session || !TracksPlayer(*session, victim))
        return;

    ObsSession& s = *session;
    uint32 const now = getMSTime();

    if (attacker)
        EnsureUnit(s, attacker);

    uint64 const src = attacker ? GuidKey(attacker->GetGUID()) : 0;
    uint32 const spellId = spell ? spell->Id : 0;

    BotTrace& trace = s.bots[GuidKey(victim->GetGUID())];
    trace.damage.push_back({now, src, spellId, amount});
    while (!trace.damage.empty() && getMSTimeDiff(trace.damage.front().ms, now) > g_cfg.deathRewindMs)
        trace.damage.pop_front();

    EnsureSpell(s, spellId);

    std::string fields = "\"s\":" + std::to_string(src);
    fields += ",\"d\":" + std::to_string(GuidKey(victim->GetGUID()));
    fields += ",\"sp\":" + std::to_string(spellId);
    fields += ",\"a\":" + std::to_string(amount);
    fields += ",\"ok\":" + std::to_string(overkill);
    fields += ",\"sc\":" + std::to_string(schoolMask);
    fields += ",\"ab\":" + std::to_string(absorb);
    fields += ",\"rs\":" + std::to_string(resist);
    fields += ",\"hp\":" + Num(victim->GetHealthPct());

    Emit(s, now, "dmg", fields);
}

void NoteKillingBlow(Unit* attacker, Unit* victim, uint32 amount)
{
    if (!Active() || !victim || !amount || amount < victim->GetHealth())
        return;

    ObsSession* session = SessionFor(victim);
    if (!session || !TracksPlayer(*session, victim))
        return;

    if (attacker)
        EnsureUnit(*session, attacker);

    BotTrace& trace = session->bots[GuidKey(victim->GetGUID())];
    trace.killBlowSource = attacker ? GuidKey(attacker->GetGUID()) : 0;
    trace.killBlowAmount = amount;
}

void NoteHeal(Unit* healer, Unit* target, SpellInfo const* spell, uint32 amount, uint32 overheal)
{
    if (!Active() || !g_cfg.logHeals || !target)
        return;

    ObsSession* session = SessionFor(target);
    if (!session || !TracksPlayer(*session, target))
        return;

    ObsSession& s = *session;
    EnsureSpell(s, spell ? spell->Id : 0);

    std::string fields = "\"s\":" + std::to_string(healer ? GuidKey(healer->GetGUID()) : 0);
    fields += ",\"d\":" + std::to_string(GuidKey(target->GetGUID()));
    fields += ",\"sp\":" + std::to_string(spell ? spell->Id : 0);
    fields += ",\"a\":" + std::to_string(amount);
    fields += ",\"oh\":" + std::to_string(overheal);
    fields += ",\"hp\":" + Num(target->GetHealthPct());

    Emit(s, getMSTime(), "heal", fields);
}

void NoteAbsorb(Unit* victim, Unit* absorbCaster, SpellInfo const* absorbSpell, uint32 amount)
{
    if (!Active() || !victim || !absorbSpell || !amount)
        return;

    ObsSession* session = SessionFor(victim);
    if (!session || !TracksPlayer(*session, victim))
        return;

    EnsureSpell(*session, absorbSpell->Id);

    std::string fields = "\"d\":" + std::to_string(GuidKey(victim->GetGUID()));
    fields += ",\"s\":" + std::to_string(absorbCaster ? GuidKey(absorbCaster->GetGUID()) : 0);
    fields += ",\"sp\":" + std::to_string(absorbSpell->Id);
    fields += ",\"a\":" + std::to_string(amount);

    Emit(*session, getMSTime(), "abs", fields);
}

// Runs inside Unit::_ApplyAura, so it gets the stamp the client-update hook below cannot: that one
// only fires on an apply once Unit::_UpdateSpells flushes the pending flag, while a removal goes out
// synchronously. Hodir's Fury lands and kills in the same tick, so without this the death record has
// only a removal to build from and falls back to the -1 sentinel. Fires twice on a stack refresh,
// which is harmless here because the stamp is idempotent and nothing is emitted.
void NoteAuraApplied(Unit* target, Aura* aura)
{
    if (!Active() || !target || !aura)
        return;

    ObsSession* session = SessionFor(target);
    if (!session || !TracksPlayer(*session, target))
        return;

    // Same guard as NoteAura: appliedMs is the first application of the current uninterrupted run, not
    // the last refresh. The two feeds must not disagree about that.
    AuraState& state = session->bots[GuidKey(target->GetGUID())].auras[aura->GetId()];
    uint32 const now = getMSTime();
    if (!state.appliedMs || state.removedMs)
        state.appliedMs = now;

    state.removedMs = 0;

    // Also filled in NoteAura, but an aura that finds no free visible slot fires neither client update,
    // so this is the only place a complete row for it gets written.
    SpellInfo const* info = aura->GetSpellInfo();

    state.caster = GuidKey(aura->GetCasterGUID());
    state.stacks = aura->GetStackAmount();
    state.duration = aura->GetDuration();
    state.positive = info && info->IsPositive();
}

void NoteAura(Unit* target, Aura* aura, bool removed)
{
    if (!Active() || !target || !aura)
        return;

    ObsSession* session = SessionFor(target);
    if (!session || !TracksPlayer(*session, target))
        return;

    ObsSession& s = *session;
    uint32 const now = getMSTime();
    uint32 const spellId = aura->GetId();
    uint32 const stacks = aura->GetStackAmount();
    int32 const duration = aura->GetDuration();
    uint64 const key = GuidKey(target->GetGUID());

    // Tracked ahead of the logAuras gate. This set is what a death record reads, and turning the aura
    // stream off must not also empty every death record.
    AuraState& state = s.bots[key].auras[spellId];
    if (removed)
    {
        state.removedMs = now;
    }
    else
    {
        if (!state.appliedMs || state.removedMs)
            state.appliedMs = now;

        state.removedMs = 0;
    }

    // Whether this helps or hurts, from the spell rather than from who cast it. The caster is not a
    // usable proxy in either direction: totems and pets buff from a creature guid, and Biting Cold is
    // applied to the player by the player, through the zone aura's trigger.
    SpellInfo const* info = aura->GetSpellInfo();

    state.caster = GuidKey(aura->GetCasterGUID());
    state.stacks = stacks;
    state.duration = duration;
    state.positive = info && info->IsPositive();

    if (!g_cfg.logAuras)
        return;

    EnsureSpell(s, spellId);

    // Null once the caster is gone, which EnsureUnit handles. The guid below stays valid either way,
    // so a caster that despawned before this fired keeps its bare number - the honest answer.
    EnsureUnit(s, aura->GetCaster());

    std::string fields = "\"d\":" + std::to_string(key);
    fields += ",\"s\":" + std::to_string(state.caster);
    fields += ",\"sp\":" + std::to_string(spellId);
    fields += ",\"r\":" + std::string(removed ? "1" : "0");
    fields += ",\"st\":" + std::to_string(stacks);
    fields += ",\"dur\":" + std::to_string(duration);
    fields += ",\"p\":" + std::string(state.positive ? "1" : "0");

    Emit(s, now, "aura", fields);
}

void NoteCast(Unit* caster, SpellInfo const* spell, Unit* target, uint32 castTimeMs)
{
    if (!Active() || !caster || !spell)
        return;

    ObsSession* session = SessionFor(caster);
    if (!session)
        return;

    ObsSession& s = *session;

    // Same instance is not the same pull: a Hodir trace picked up 253 casts from a Freya-area mob two
    // rooms away and none at all from its own raid. The roster, its pets and totems, and whatever is
    // being watched are the pull; everything else on the map is somebody else's.
    bool relevant = false;
    if (caster->IsPlayer())
        relevant = TracksPlayer(s, caster);
    else if (s.watched.count(caster->GetGUID()))
        relevant = true;
    else if (Unit* owner = caster->GetOwner())
        relevant = owner->IsPlayer() && TracksPlayer(s, owner);

    if (!relevant)
        return;

    EnsureUnit(s, caster);
    EnsureUnit(s, target);
    EnsureSpell(s, spell->Id);

    std::string fields = "\"s\":" + std::to_string(GuidKey(caster->GetGUID()));
    fields += ",\"sp\":" + std::to_string(spell->Id);
    fields += ",\"tgt\":" + std::to_string(target ? GuidKey(target->GetGUID()) : 0);
    fields += ",\"ct\":" + std::to_string(castTimeMs);

    Emit(s, getMSTime(), "cast", fields);
}

void BeginTick(Player* bot)
{
    if (!Active() || !bot)
        return;

    ObsSession* session = SessionFor(bot);
    if (!session || !TracksPlayer(*session, bot))
        return;

    uint64 const key = GuidKey(bot->GetGUID());
    FlushTick(*session, key, session->bots[key]);
}

// Buffered rather than emitted. What a pass decided is only news as a whole ordered set - the engine
// walks several nodes and reports a verdict for each, in the same order every tick - and the set is
// not complete until BeginTick closes it.
void NoteAction(Player* bot, char const* action, float relevance, char const* verdict)
{
    if (!Active() || !bot || !action || !verdict)
        return;

    ObsSession* session = SessionFor(bot);
    if (!session || !TracksPlayer(*session, bot))
        return;

    BotTrace& trace = session->bots[GuidKey(bot->GetGUID())];
    if (trace.tick.size() >= OBS_MAX_TICK_ENTRIES)
        return;

    trace.tick.push_back({getMSTime(), false, action, relevance, verdict});
}

void NoteVeto(Player* bot, char const* multiplier, char const* action)
{
    if (!Active() || !bot || !multiplier || !action)
        return;

    ObsSession* session = SessionFor(bot);
    if (!session || !TracksPlayer(*session, bot))
        return;

    BotTrace& trace = session->bots[GuidKey(bot->GetGUID())];
    if (trace.tick.size() >= OBS_MAX_TICK_ENTRIES)
        return;

    trace.tick.push_back({getMSTime(), true, action, 0.0f, multiplier});
}

void NoteMove(Player* bot, MoveKind kind, float x, float y, float z, ObjectGuid target, MoveOutcome outcome,
              MovePriority priority, MovePriority holder, uint32 holdMs)
{
    if (!Active() || !bot)
        return;

    ObsSession* session = SessionFor(bot);
    if (!session || !TracksPlayer(*session, bot))
        return;

    ObsSession& s = *session;
    uint32 const now = getMSTime();
    BotTrace& trace = s.bots[GuidKey(bot->GetGUID())];
    char const* by = t_currentAction ? t_currentAction : "";
    bool const issued = outcome == MoveOutcome::Issued;
    // Follow and Chase steer at a unit that keeps moving, so their coordinates change every tick and
    // only a changed target is news. A point destination re-stamped every tick is the opposite - that
    // repetition is the bug the trace exists to show.
    bool const tracking = kind == MoveKind::Follow || kind == MoveKind::Chase;
    uint64 const targetKey = GuidKey(target);

    if (!issued)
    {
        // A destination that did not reach the MotionMaster is a candidate, not a command - MoveNear
        // sweeps eight angles a tick and MoveToLOS retries. Latching one would make the death record
        // measure `arrived` against a position no MotionMaster ever saw, so these are throttled and
        // never touch the latch.
        if (trace.hasReject && trace.lastRejectBy == by && trace.lastRejectKind == kind &&
            getMSTimeDiff(trace.lastRejectMs, now) < OBS_MOVE_REJECT_THROTTLE_MS)
            return;

        trace.hasReject = true;
        trace.lastRejectMs = now;
        trace.lastRejectBy = by;
        trace.lastRejectKind = kind;
    }
    else if (tracking)
    {
        if (trace.hasTrack && trace.lastTrackBy == by && trace.lastTrackKind == kind &&
            trace.lastTrackTarget == targetKey)
            return;

        trace.hasTrack = true;
        trace.lastTrackBy = by;
        trace.lastTrackKind = kind;
        trace.lastTrackTarget = targetKey;
    }
    else
    {
        if (trace.hasMove && trace.lastMoveBy == by && std::fabs(trace.lastMoveX - x) < 0.5f &&
            std::fabs(trace.lastMoveY - y) < 0.5f && std::fabs(trace.lastMoveZ - z) < 0.5f)
            return;

        trace.hasMove = true;
        trace.lastMoveX = x;
        trace.lastMoveY = y;
        trace.lastMoveZ = z;
        trace.lastMoveBy = by;
    }

    std::string fields = "\"g\":" + std::to_string(GuidKey(bot->GetGUID()));
    fields += ",\"k\":\"" + std::string(MoveKindName(kind)) + "\"";
    fields += ",\"x\":" + Num(x);
    fields += ",\"y\":" + Num(y);
    fields += ",\"z\":" + Num(z);
    fields += ",\"tgt\":" + std::to_string(targetKey);
    fields += ",\"ok\":" + std::string(issued ? "1" : "0");
    fields += ",\"r\":\"" + std::string(MoveReason(outcome)) + "\"";
    fields += ",\"by\":" + Quoted(by);
    fields += ",\"pr\":\"" + std::string(MovePriorityName(priority)) + "\"";

    // Only on a refusal the gate actually made. Everywhere else there is no walk in flight to name.
    if (outcome == MoveOutcome::Waiting)
    {
        fields += ",\"hpr\":\"" + std::string(MovePriorityName(holder)) + "\"";
        fields += ",\"hms\":" + std::to_string(holdMs);
    }

    Emit(s, now, "move", fields);
}

ActionScope::ActionScope(std::string action) : _action(std::move(action)), _previous(t_currentAction)
{
    t_currentAction = _action.c_str();
}

ActionScope::~ActionScope() { t_currentAction = _previous; }

BotContext::BotContext(Player* bot) : _previous(t_currentBot) { t_currentBot = bot; }
BotContext::~BotContext() { t_currentBot = _previous; }

void Note(Player* bot, char const* kind, std::string const& text)
{
    if (!Active() || !kind)
        return;

    // A latch with no bot of its own belongs to whichever bot's tick flipped it, which is also the
    // most useful attribution: it names who made the call.
    if (!bot)
        bot = t_currentBot;

    ObsSession* session = bot ? SessionFor(bot) : nullptr;
    if (!session)
        return;

    std::string fields = "\"g\":" + std::to_string(GuidKey(bot->GetGUID()));
    fields += ",\"k\":\"" + std::string(kind) + "\"";
    fields += ",\"txt\":" + Quoted(text);

    Emit(*session, getMSTime(), "note", fields);
}

void NoteAssignment(ObjectGuid guid, char const* kind, std::string const& value)
{
    if (!Active() || !kind)
        return;

    Player* player = ObjectAccessor::FindPlayer(guid);
    if (!player)
        return;

    ObsSession* session = SessionFor(player);
    if (!session)
        return;

    std::string fields = "\"g\":" + std::to_string(GuidKey(guid));
    fields += ",\"k\":\"" + std::string(kind) + "\"";
    fields += ",\"txt\":" + Quoted(value);

    Emit(*session, getMSTime(), "note", fields);
}

void NoteDerived(Player* bot, char const* key, std::string const& value)
{
    if (!Active() || !key)
        return;

    if (!bot)
        bot = t_currentBot;

    ObsSession* session = bot ? SessionFor(bot) : nullptr;
    if (!session || !TracksPlayer(*session, bot))
        return;

    ObsSession& s = *session;
    uint64 const guidKey = GuidKey(bot->GetGUID());
    std::string& held = s.bots[guidKey].derived[key];
    if (held == value)
        return;

    held = value;

    std::string fields = "\"g\":" + std::to_string(guidKey);
    fields += ",\"k\":\"" + std::string(key) + "\"";
    fields += ",\"txt\":" + Quoted(value);

    Emit(s, getMSTime(), "note", fields);
}

void NoteHazard(Map* map, uint32 spellId, Position const& origin, char const* shape, std::string const& params,
                uint32 ttlMs)
{
    if (!Active() || !map || !shape)
        return;

    ObsSession* session = FindSession(map->GetInstanceId());
    if (!session)
        return;

    // A hazard with no world object is the one spell nothing else in the trace has to mention, so this
    // is the only chance to name it.
    EnsureSpell(*session, spellId);

    std::string fields = "\"sp\":" + std::to_string(spellId);
    fields += ",\"shape\":\"" + std::string(shape) + "\"";
    fields += ",\"x\":" + Num(origin.GetPositionX());
    fields += ",\"y\":" + Num(origin.GetPositionY());
    fields += ",\"z\":" + Num(origin.GetPositionZ());
    fields += ",\"ttl\":" + std::to_string(ttlMs);
    if (!params.empty())
        fields += "," + params;

    Emit(*session, getMSTime(), "haz", fields);
}

void NoteHazardCircle(Map* map, uint32 spellId, Position const& pos, float radius, uint32 ttlMs)
{
    NoteHazard(map, spellId, pos, "circle", "\"rad\":" + Num(radius), ttlMs);
}

void NoteDeath(Unit* unit, Unit* killer)
{
    if (!Active() || !unit || !unit->IsPlayer())
        return;

    ObsSession* session = SessionFor(unit);
    if (!session || !TracksPlayer(*session, unit))
        return;

    ObsSession& s = *session;
    uint32 const now = getMSTime();
    uint64 const key = GuidKey(unit->GetGUID());
    BotTrace& trace = s.bots[key];

    // The pass the bot died in is the one worth reading and nothing else will close it.
    FlushTick(s, key, trace);

    std::string fields = "\"g\":" + std::to_string(key);
    fields += ",\"killer\":" + std::to_string(killer ? GuidKey(killer->GetGUID()) : 0);
    fields += ",\"x\":" + Num(unit->GetPositionX());
    fields += ",\"y\":" + Num(unit->GetPositionY());
    fields += ",\"z\":" + Num(unit->GetPositionZ());

    std::string dist = "{";
    bool firstDist = true;
    for (ObjectGuid guid : s.watched)
    {
        Creature* creature = s.map ? s.map->GetCreature(guid) : nullptr;
        if (!creature || !creature->IsInWorld())
            continue;

        if (!firstDist)
            dist += ",";
        firstDist = false;
        dist += "\"" + std::to_string(GuidKey(guid)) + "\":" + Num(unit->GetExactDist(creature));
    }
    dist += "}";
    fields += ",\"dist\":" + dist;

    // Read from the tracked set, never from the unit. Unit::Kill calls RemoveAllAurasOnDeath long
    // before OnUnitDeath fires, so by the time this runs the only things still applied are passives and
    // death-persistent auras - which is why v3 death records listed 57 talents and no boss debuff.
    // Anything dropped inside the grace window is still reported, flagged with when it came off.
    std::string auras = "[";
    bool firstAura = true;
    for (auto const& entry : trace.auras)
    {
        AuraState const& state = entry.second;
        if (state.removedMs && getMSTimeDiff(state.removedMs, now) > OBS_DEATH_AURA_GRACE_MS)
            continue;

        if (!firstAura)
            auras += ",";
        firstAura = false;
        auras += "[" + std::to_string(entry.first);
        auras += "," + std::to_string(state.stacks);
        auras += "," + std::to_string(state.duration);
        auras += "," + std::to_string(state.caster);
        auras += "," + std::to_string(state.appliedMs ? s.Stamp(state.appliedMs) : int64(-1));
        auras += "," + std::to_string(state.removedMs ? s.Stamp(state.removedMs) : int64(-1));
        auras += "," + std::string(state.positive ? "1" : "0");
        auras += "]";

        EnsureSpell(s, entry.first);
    }
    auras += "]";
    fields += ",\"auras\":" + auras;

    // Trimmed here as well as on push: the ring only ever loses its front when something new arrives,
    // so a bot that goes untouched for a minute and then dies would otherwise report the hits that
    // landed a minute ago as though they were the ones that killed it. Chronological, because the
    // question a rewind answers is what came last, not what hit hardest.
    std::vector<DamageEntry> rewind;
    for (DamageEntry const& entry : trace.damage)
        if (getMSTimeDiff(entry.ms, now) <= g_cfg.deathRewindMs)
            rewind.push_back(entry);

    std::sort(rewind.begin(), rewind.end(),
              [](DamageEntry const& a, DamageEntry const& b) { return a.ms < b.ms; });

    std::string rewindJson = "[";
    for (std::size_t i = 0; i < rewind.size(); ++i)
    {
        if (i)
            rewindJson += ",";
        rewindJson += "[" + std::to_string(s.Stamp(rewind[i].ms));
        rewindJson += "," + std::to_string(rewind[i].source);
        rewindJson += "," + std::to_string(rewind[i].spellId);
        rewindJson += "," + std::to_string(rewind[i].amount);
        rewindJson += "]";

        EnsureSpell(s, rewind[i].spellId);
    }
    rewindJson += "]";
    fields += ",\"rewind\":" + rewindJson;

    if (trace.killBlowAmount)
    {
        fields += ",\"blow\":[" + std::to_string(trace.killBlowSource);
        fields += "," + std::to_string(trace.killBlowAmount) + "]";
    }

    trace.killBlowSource = 0;
    trace.killBlowAmount = 0;

    if (trace.lastHpMs)
    {
        fields += ",\"hplast\":[" + Num(trace.lastHpPct);
        fields += "," + std::to_string(s.Stamp(trace.lastHpMs)) + "]";
    }

    // One row per verdict per distinct pass, carrying how many passes running produced it. A raw list
    // of every verdict ran to 1300 entries and made up most of the record; the passes repeat verbatim,
    // so this collapses to a handful of rows with the interleaving order intact.
    std::string acts = "[";
    bool firstAct = true;
    for (TickRecord const& record : trace.ticks)
    {
        for (TickEntry const& entry : record.entries)
        {
            if (!firstAct)
                acts += ",";
            firstAct = false;
            acts += "[" + std::to_string(s.Stamp(record.firstMs));
            acts += "," + std::to_string(s.Stamp(record.lastMs));
            acts += "," + Quoted(entry.action);
            acts += "," + Num(entry.relevance);
            acts += "," + Quoted(entry.veto ? "VETO:" + entry.verdict : entry.verdict);
            acts += "," + std::to_string(record.repeats);
            acts += "]";
        }
    }
    acts += "]";
    fields += ",\"acts\":" + acts;

    if (trace.hasMove)
    {
        std::string move = "{\"x\":" + Num(trace.lastMoveX);
        move += ",\"y\":" + Num(trace.lastMoveY);
        move += ",\"z\":" + Num(trace.lastMoveZ);
        move += ",\"by\":" + Quoted(trace.lastMoveBy);
        move += ",\"arrived\":" +
                std::string(unit->GetExactDist2d(trace.lastMoveX, trace.lastMoveY) < 3.0f ? "1" : "0");
        move += "}";
        fields += ",\"lastmove\":" + move;
    }

    Emit(s, now, "death", fields);
    Flush(s);
}

std::string DescribeAssignment(bool value) { return value ? "1" : "0"; }
std::string DescribeAssignment(ObjectGuid const& value) { return std::to_string(GuidKey(value)); }

std::string DescribeAssignment(Position const& value)
{
    return Num(value.GetPositionX()) + "," + Num(value.GetPositionY()) + "," + Num(value.GetPositionZ());
}

std::string DescribeDerived(Position const& value)
{
    char buf[48];
    snprintf(buf, sizeof(buf), "%.0f,%.0f,%.0f", value.GetPositionX(), value.GetPositionY(),
             value.GetPositionZ());
    return buf;
}

std::string Status()
{
    std::lock_guard<std::mutex> guard(g_registryMutex);

    if (!g_cfg.enabled)
        return "RaidObs: disabled";

    if (g_sessions.empty())
        return "RaidObs: enabled, no trace open";

    std::string out = "RaidObs: " + std::to_string(g_sessions.size()) + " trace(s) open";
    for (auto const& entry : g_sessions)
    {
        ObsSession const& s = *entry.second;
        // path is fixed before the session is published; the other two are atomics for exactly this,
        // because the console command runs on the world thread while a map thread is still writing.
        out += "\n  " + s.path + " (" + std::to_string(s.bytes.load(std::memory_order_relaxed)) +
               " bytes, " + std::to_string(s.rosterSize.load(std::memory_order_relaxed)) + " members)";
    }

    return out;
}
}  // namespace RaidObs
