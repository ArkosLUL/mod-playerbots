/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RaidObsSession.h"

#include "Config.h"
#include "Log.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <vector>

namespace RaidObs
{
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

// --- config and lifecycle -----------------------------------------------------

void LoadConfig()
{
    g_cfg.enabled = sPlayerbotAIConfig.obsEnabled;
    g_cfg.dir = sPlayerbotAIConfig.obsDir;
    g_cfg.snapshotIntervalMs = sPlayerbotAIConfig.obsSnapshotIntervalMs;
    g_cfg.preRollMs = sPlayerbotAIConfig.obsPreRollSeconds * 1000;
    g_cfg.logHeals = sPlayerbotAIConfig.obsLogHeals;
    g_cfg.logAuras = sPlayerbotAIConfig.obsLogAuras;
    g_cfg.logPets = sPlayerbotAIConfig.obsLogPets;
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
