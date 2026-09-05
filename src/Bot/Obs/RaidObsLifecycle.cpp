/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RaidObsSession.h"

#include "Creature.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Timer.h"

#include <cstring>
#include <ctime>
#include <filesystem>
#include <utility>

namespace RaidObs
{
// --- opening and closing a trace ----------------------------------------------

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

    s.RebuildRoster();
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
    hdr += ",\"roster\":" + s.RosterJson();
    hdr += "}";
    s.Write(hdr);

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
    {
        for (uint32 spellId : entry.castSpells)
            s.EnsureSpell(spellId);

        s.Emit(entry.ms, "snap", entry.payload);
    }

    s.Emit(s.startMs, "pull",
         "\"boss\":" + Quoted(s.bossSlug) + ",\"src\":\"" + trigger + "\"");

    s.SeedWatched(source);

    {
        std::lock_guard<std::mutex> guard(g_registryMutex);
        g_sessions[instanceId] = std::move(session);
        RefreshActiveFlag();
    }

    LOG_INFO("playerbots", "RaidObs: recording {} -> {}", s.bossSlug, s.path);
}

// Only asked at the close, and never for `shutdown` or `mapgone`, which are the two outcomes that can
// reach here with the map thread pool already gone.
bool ObsSession::RosterMostlyDead()
{
    uint32 present = 0;
    uint32 dead = 0;
    for (ObjectGuid guid : roster)
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
        session->FlushTick(entry.first, entry.second);

    // Hodir's script reports NOT_STARTED when the raid releases, so a 23-of-24 wipe was filed under
    // `reset`. What the roster looked like outranks what the script settled on - taken from the latch
    // first, since by the time an idle close runs the raid has had 30 seconds to run back alive.
    char const* result = outcome;
    if ((!strcmp(outcome, "reset") || !strcmp(outcome, "idle")) &&
        (session->sawMostlyDead || session->RosterMostlyDead()))
        result = "wipe";

    session->Emit(getMSTime(), "end", std::string("\"out\":\"") + result + "\"", true);
    session->Flush();
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

bool ObsSession::AnyRaidMemberInCombat()
{
    for (ObjectGuid guid : roster)
        if (Player* player = ObjectAccessor::FindPlayer(guid))
            if (player->IsInWorld() && player->IsInCombat())
                return true;

    return false;
}

// --- map and script events ----------------------------------------------------

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

        std::vector<uint32> casting;
        std::string payload = BuildSnapshotPayload(map, roster, {}, nullptr, &casting);

        std::lock_guard<std::mutex> guard(g_registryMutex);
        PreRollRing& ring = g_preRoll[instanceId];
        ring.entries.push_back({now, std::move(payload), std::move(casting)});
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
        s.RebuildRoster();
        s.PruneWatched();
        s.PruneAuras(now);
    }

    if (s.sinceSnapshotMs >= g_cfg.snapshotIntervalMs)
    {
        // Subtracted rather than cleared: clearing throws away the overshoot every tick, which turned a
        // configured 250 ms into a measured 314 ms. A tick long enough to owe two samples banks no
        // credit for the one it missed - that sample is gone either way.
        s.sinceSnapshotMs -= g_cfg.snapshotIntervalMs;
        if (s.sinceSnapshotMs >= g_cfg.snapshotIntervalMs)
            s.sinceSnapshotMs = 0;

        s.Emit(now, "snap", BuildSnapshotPayload(map, s.roster, s.watched, &s));
    }

    if (s.sinceFlushMs >= OBS_FLUSH_INTERVAL_MS)
    {
        s.sinceFlushMs = 0;
        s.Flush();
    }

    if (s.AnyRaidMemberInCombat())
    {
        s.lastCombatMs = now;

        // Sampled during the fight rather than at the close, where idleCloseMs has already given the
        // raid 30 seconds to release and run back - long enough that the check in CloseSession sees a
        // healthy roster and files a 31-death attempt as `idle`.
        s.sawMostlyDead = s.sawMostlyDead || s.RosterMostlyDead();
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

// The first boss to actually swing at the raid, for a trace that opened before it could see one.
void ObsSession::UpgradeBossName(Creature* boss)
{
    if (!map || !boss)
        return;

    UpgradeBossName(SlugOf(ResolveBossName(map, boss)));
}

// A session opened off a boss state change, or off a MarkPull whose boss lookup came back empty, can
// only name itself after the map - two of ten traces on 2026-08-31 were filed as `ulduar`, one an Iron
// Assembly wipe and one a Thorim wipe, and a Yogg-Saron wipe on 2026-09-04. The filename and hdr.boss
// are the only way to pick a trace, so anything that later learns the encounter fixes both. Only ever
// upgrades the map-name fallback: a session that already named itself is never renamed.
void ObsSession::UpgradeBossName(std::string const& slug)
{
    if (!map || path.empty() || slug.empty())
        return;

    if (bossSlug != SlugOf(map->GetMapName()) || slug == bossSlug)
        return;

    std::filesystem::path const from(path);
    std::filesystem::path to = from;
    // Same stem apart from the slug, so the timestamp that pairs a trace with a server log survives.
    to.replace_filename(std::to_string(mapId) + "_" + std::to_string(instanceId) + "_" + slug + "_" +
                        from.stem().string().substr(from.stem().string().find_last_of('_') + 1) + ".ndjson");

    Flush();
    file.close();

    std::error_code ec;
    std::filesystem::rename(from, to, ec);
    if (ec)
    {
        // Reopen the original and carry on under the map name. A trace that keeps recording under a
        // poor name beats one that stops.
        file.open(path, std::ios::out | std::ios::app);
        LOG_ERROR("playerbots", "RaidObs: cannot rename trace {} -> {}", path, to.string());
        return;
    }

    path = to.string();
    file.open(path, std::ios::out | std::ios::app);
    if (!file.is_open())
    {
        LOG_ERROR("playerbots", "RaidObs: lost trace {} after rename", path);
        return;
    }

    std::string const was = bossSlug;
    bossSlug = slug;

    // hdr.boss is on line one of a file that is only ever appended to, so the correction goes in the
    // stream instead. Readers that trust the header get the old name; the filename is right either way.
    Emit(getMSTime(), "pull", "\"boss\":" + Quoted(bossSlug) + ",\"src\":\"rename\",\"was\":" + Quoted(was));

    LOG_INFO("playerbots", "RaidObs: renamed {} -> {}", was, path);
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

    if (isBoss)
        session->UpgradeBossName(asCreature);

    session->WatchCreature(asCreature);
}

// Renames rather than doing nothing when a trace is already open: a session that beat the strategy
// to the pull is exactly the one carrying the map name, since whatever opened it had no boss to name
// it after.
void MarkPull(Map* map, Unit* source)
{
    if (!map)
        return;

    if (ObsSession* session = FindSession(map->GetInstanceId()))
    {
        if (Creature* boss = source ? source->ToCreature() : nullptr)
            session->UpgradeBossName(boss);

        return;
    }

    OpenSession(map, source, "mark");
}

void NamePull(Map* map, char const* bossName)
{
    if (!Active() || !map || !bossName || !*bossName)
        return;

    if (ObsSession* session = FindSession(map->GetInstanceId()))
        session->UpgradeBossName(SlugOf(bossName));
}
}  // namespace RaidObs
