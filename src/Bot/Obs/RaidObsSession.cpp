/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RaidObsSession.h"

#include "Creature.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"

#include <cctype>
#include <cstdio>

namespace RaidObs
{
std::atomic<bool> g_active{false};

ObsConfig g_cfg;
std::string g_logsDir;

std::mutex g_registryMutex;
std::unordered_map<uint32, std::unique_ptr<ObsSession>> g_sessions;
std::unordered_map<uint32, PreRollRing> g_preRoll;
std::unordered_map<uint32, std::vector<uint32>> g_pendingBossState;
std::atomic<uint32> g_registryGeneration{0};

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

// --- registry -----------------------------------------------------------------

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

// --- the write path -----------------------------------------------------------

// `force` writes past the size cap. Only the end record uses it: without one, postmortem.py reports
// a clean kill that happened to hit the cap as a trace cut short by a crash.
void ObsSession::Write(std::string const& line, bool force)
{
    if (capped && !force)
        return;

    buffer += line;
    buffer += '\n';
    uint64 const total = bytes.load(std::memory_order_relaxed) + line.size() + 1;
    bytes.store(total, std::memory_order_relaxed);

    if (!capped && total >= g_cfg.maxFileBytes)
    {
        buffer += "{\"e\":\"truncated\"}\n";
        capped = true;
    }

    if (buffer.size() >= 64 * 1024)
    {
        file << buffer;
        buffer.clear();
    }
}

void ObsSession::Flush()
{
    if (!buffer.empty())
    {
        file << buffer;
        buffer.clear();
    }

    file.flush();
}

void ObsSession::Emit(uint32 ms, char const* event, std::string const& fields, bool force)
{
    std::string line = "{\"t\":";
    line += std::to_string(Stamp(ms));
    line += ",\"e\":\"";
    line += event;
    line += "\"";
    if (!fields.empty())
    {
        line += ",";
        line += fields;
    }
    line += "}";

    Write(line, force);
}

// Emitted once per guid per trace, so later records can carry a bare guid instead of repeating names.
void ObsSession::EnsureUnit(Unit* unit)
{
    if (!unit)
        return;

    uint64 const key = GuidKey(unit->GetGUID());
    if (!seenUnits.insert(key).second)
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

    Emit(getMSTime(), "unit", fields);
}

// Same trick as EnsureUnit, for spells. Without it a death block reads "spell 63511" and nobody can
// tell that from Biting Cold without a DBC to hand, which defeats the point of a self-contained file.
void ObsSession::EnsureSpell(uint32 spellId)
{
    if (!spellId || !seenSpells.insert(spellId).second)
        return;

    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    char const* name = info ? info->SpellName[LOCALE_enUS] : nullptr;

    std::string fields = "\"sp\":" + std::to_string(spellId);
    fields += ",\"n\":" + Quoted(name ? name : "?");

    Emit(getMSTime(), "spell", fields);
}

// Any player standing on the recorded map, not only the roster the header snapshotted: a bot
// summoned or resurrected mid-pull has to be traced from its first event rather than from the next
// roster refresh three seconds later. Writing the name record here is the only chance a late
// arrival gets one.
bool ObsSession::Tracks(Unit* unit)
{
    if (!unit || !unit->IsPlayer() || unit->GetMap() != map)
        return false;

    EnsureUnit(unit);
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

void ObsSession::RebuildRoster()
{
    roster.clear();

    if (map)
    {
        Map::PlayerList const& players = map->GetPlayers();
        for (Map::PlayerList::const_iterator it = players.begin(); it != players.end(); ++it)
            if (Player* player = it->GetSource())
                if (player->IsInWorld())
                    roster.push_back(player->GetGUID());
    }

    rosterSize.store(static_cast<uint32>(roster.size()), std::memory_order_relaxed);
}

std::string ObsSession::RosterJson()
{
    std::string out = "[";
    bool first = true;
    for (ObjectGuid guid : roster)
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

ProbeTarget::ProbeTarget(Unit* unit)
{
    if (!Active() || !unit)
        return;

    ObsSession* session = SessionFor(unit);
    if (!session || !session->Tracks(unit))
        return;

    _session = session;
    _key = GuidKey(unit->GetGUID());
}

JsonFields& JsonFields::Raw(char const* key, std::string const& json)
{
    if (!_out.empty())
        _out += ",";

    _out += "\"";
    _out += key;
    _out += "\":";
    _out += json;
    return *this;
}

JsonFields& JsonFields::Text(char const* key, std::string const& text) { return Raw(key, Quoted(text)); }
JsonFields& JsonFields::Int(char const* key, int64 value) { return Raw(key, std::to_string(value)); }
JsonFields& JsonFields::Real(char const* key, float value) { return Raw(key, Num(value)); }
}  // namespace RaidObs
