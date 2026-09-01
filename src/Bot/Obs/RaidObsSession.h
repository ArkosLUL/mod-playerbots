/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_RAIDOBSSESSION_H
#define PLAYERBOTS_RAIDOBSSESSION_H

#include "RaidObs.h"

#include <atomic>
#include <deque>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Creature;
class DynamicObject;
class Map;
class Player;
class Unit;

// Shared between the RaidObs*.cpp parts and nothing else. The contract the rest of the module builds
// against is RaidObs.h; every name here is an implementation detail that may move without notice.
//
// Threading: a session body belongs to the map thread that owns its map, so it carries no lock of its
// own. The registry is the one shared structure and g_registryMutex guards it. Status() and Shutdown()
// are the two documented world-thread callers - see docs/systems/observability.md.
namespace RaidObs
{
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
    bool logPets = true;
    uint32 minDamage = 0;
    uint32 deathRewindMs = 15000;
    uint32 deathVerdictMs = 10000;
    uint32 idleCloseMs = 30000;
    uint32 retentionDays = 7;
    uint64 maxDirBytes = 5120ull * 1024 * 1024;
    uint64 maxFileBytes = 256ull * 1024 * 1024;
};

extern ObsConfig g_cfg;
extern std::string g_logsDir;

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
    // Set by NoteScriptedWipe when the master's `wipe` command is what killed the bot, so the death
    // record can say so instead of naming the bot as its own killer with nothing behind it.
    bool scriptedWipe = false;
    float lastHpPct = 0.0f;
    uint32 lastHpMs = 0;
    // Everything this bot has landed on something outside the raid, running total. Cumulative rather
    // than a per-snapshot delta, so a coalesced or dropped sample costs nothing and any window still
    // differences cleanly out of two snapshots.
    uint64 damageDealt = 0;
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

// One open trace. Data stays public: this is an internal aggregate, and the parts that build records
// reach straight into it. What the methods own is the write path and the per-session bookkeeping.
class ObsSession
{
public:
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

    // --- write path (RaidObsSession.cpp) ---

    // `force` writes past the size cap. Only the end record uses it: without one, postmortem.py
    // reports a clean kill that happened to hit the cap as a trace cut short by a crash.
    void Write(std::string const& line, bool force = false);
    void Flush();
    void Emit(uint32 ms, char const* event, std::string const& fields, bool force = false);

    // Emitted once per guid per trace, so later records can carry a bare guid instead of a name.
    void EnsureUnit(Unit* unit);
    void EnsureSpell(uint32 spellId);
    bool Tracks(Unit* unit);

    void RebuildRoster();
    std::string RosterJson();

    // --- watched set and snapshots (RaidObsSnapshot.cpp) ---

    void WatchCreature(Creature* creature);
    void SeedWatched(Unit* source);
    void PruneWatched();
    void PruneAuras(uint32 now);
    std::string SweepArea(Unit* anchor, std::string& units, bool& firstUnit);

    // --- verdict ticks (RaidObsEngine.cpp) ---

    void EmitTickEntry(uint64 key, TickEntry const& entry);
    // Closes the pass being buffered for one bot and writes whatever in it is news.
    void FlushTick(uint64 key, BotTrace& trace);

    // --- lifecycle (RaidObsLifecycle.cpp) ---

    bool RosterMostlyDead();
    bool AnyRaidMemberInCombat();
    void UpgradeBossName(Creature* boss);
};

// The registry is the only shared structure here. Bumped on every open and close so map threads can
// tell their cached answers are stale.
extern std::mutex g_registryMutex;
extern std::unordered_map<uint32, std::unique_ptr<ObsSession>> g_sessions;
extern std::unordered_map<uint32, PreRollRing> g_preRoll;
// Boss ids whose state the instance script tried to change, waiting for the next map update to read
// back what the core actually settled on.
extern std::unordered_map<uint32, std::vector<uint32>> g_pendingBossState;
extern std::atomic<uint32> g_registryGeneration;

ObsSession* FindSession(uint32 instanceId);
ObsSession* SessionFor(Unit* unit);
bool MapIsTracked(Map* map);
// Call with the registry lock held and after the map itself has changed.
void RefreshActiveFlag();

// --- formatting (RaidObsSession.cpp) ---

std::string JsonEscape(std::string const& in);
std::string Quoted(std::string const& in);
std::string Num(float v);
std::string SlugOf(std::string const& name);
std::string RoleOf(Player* player);
char const* MoveKindName(MoveKind kind);
char const* MoveReason(MoveOutcome outcome);
char const* MovePriorityName(MovePriority priority);

// The counter is a separate numbering space per guid type - a creature and a player both start at 1 -
// so it cannot key a record on its own. Players keep the bare counter because they are the bulk of
// every snapshot; the tag stays small enough that the result is still an exact JSON integer, and
// ObjectGuid::Empty still comes out as 0.
//
// Inline because every probe calls it per event, which the anonymous namespace used to cover.
inline uint64 GuidKey(ObjectGuid guid)
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

// --- snapshots (RaidObsSnapshot.cpp) ---

std::string UnitRow(Unit* unit, uint64 dealt = 0);
float HazardRadius(DynamicObject* dyn);
bool HazardIsFriendly(DynamicObject* dyn);
// `session` is null on the pre-roll path, which samples a map before any trace exists.
std::string BuildSnapshotPayload(Map* map, std::vector<ObjectGuid> const& roster,
                                 std::unordered_set<ObjectGuid> const& watched, ObsSession* session);

// --- verdict ticks (RaidObsEngine.cpp) ---

bool SameTick(std::vector<TickEntry> const& left, std::vector<TickEntry> const& right);
bool ShouldEmit(BotTrace& trace, TickEntry const& entry);

// --- lifecycle (RaidObsLifecycle.cpp) ---

std::string ResolveBossName(Map* map, Unit* source);
Unit* FindEngagedBoss(Map* map);
void OpenSession(Map* map, Unit* source, char const* trigger);
void CloseSession(uint32 instanceId, char const* outcome);
void ProcessPendingBossState(Map* map, uint32 instanceId);

// --- config and retention (RaidObsConfig.cpp) ---

void ApplyRetention();

// --- combat (RaidObsCombat.cpp) ---

void AccrueDamageDealt(Unit* attacker, Unit* victim, uint32 amount);

// What a probe needs before it can write, or nothing at all. Falsy covers the three ways a probe has
// no work: nothing is recording, no trace is open on this unit's map, and the open trace does not
// follow this unit - which includes every unit that is not a player.
//
// Trace() inserts on first use and is deliberately not resolved up front - the note and assignment
// probes never touch the bot map, and an eager insert would quietly change what it holds.
class ProbeTarget
{
public:
    explicit ProbeTarget(Unit* unit);

    explicit operator bool() const { return _session != nullptr; }
    ObsSession& Session() const { return *_session; }
    BotTrace& Trace() const { return _session->bots[_key]; }
    uint64 Key() const { return _key; }

private:
    ObsSession* _session = nullptr;
    uint64 _key = 0;
};

// The "key":value list for one Emit, built in the order the fields are added. Hand-concatenating
// these is where a stray comma or a missing quote gets in; the output is byte-identical either way.
class JsonFields
{
public:
    // A value that is already JSON - a number, an array, a nested object.
    JsonFields& Raw(char const* key, std::string const& json);
    // A string value, quoted and escaped.
    JsonFields& Text(char const* key, std::string const& text);
    JsonFields& Int(char const* key, int64 value);
    JsonFields& Real(char const* key, float value);

    std::string const& Done() const { return _out; }

private:
    std::string _out;
};
}  // namespace RaidObs

#endif
