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
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <vector>

namespace RaidObs
{
uint64 g_binaryMs = 0;

// --- build identity and run settings ------------------------------------------

// The worldserver's own mtime, which is the same number pitfalls.md tells a human to fetch with
// `docker exec ac-worldserver ls -l --time-style=+%F_%R env/dist/bin/worldserver`. It stands in for a
// build hash because nothing cheaper exists: this module has no CMakeLists of its own to stamp one
// from, and AzerothCore's revision names the *core*, which says nothing about a module.
//
// read_symlink simply fails off Linux and leaves the stamp 0; the deployment target is the only place
// it has to work, and a 0 reads as "unknown" rather than as a wrong answer.
static void ResolveBinaryStamp()
{
    std::error_code ec;
    std::filesystem::path const exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec)
        return;

    auto const written = std::filesystem::last_write_time(exe, ec);
    if (ec)
        return;

    // file_clock and system_clock have no common epoch before C++20's clock_cast, which libstdc++ did
    // not carry until well after the compiler this builds with. Differencing against both clocks' own
    // "now" is the portable conversion; it is accurate to the few ms between the two reads, which is
    // far below the granularity anyone compares a build time at.
    auto const sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        written - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    auto const ms = std::chrono::duration_cast<std::chrono::milliseconds>(sys.time_since_epoch()).count();
    if (ms > 0)
        g_binaryMs = static_cast<uint64>(ms);
}

std::string EnvFieldsJson()
{
    std::string out = ",\"bin\":" + std::to_string(g_binaryMs);

    std::string cheats;
    for (std::string const& cheat : sPlayerbotAIConfig.botCheats)
    {
        if (!cheats.empty())
            cheats += ",";
        cheats += cheat;
    }

    // Only the knobs that decide whether a pull is the one that was meant to run. A full config dump
    // would age with every added option and bury the handful anyone checks. The hard-mode keys are the
    // conf's own boss names, so postmortem.py can join them against hdr.boss.
    out += ",\"cfg\":{\"cheats\":" + Quoted(cheats);
    out += ",\"mapthreads\":" + std::to_string(sConfigMgr->GetOption<uint32>("MapUpdate.Threads", 1, false));
    out += ",\"hardmode\":{";
    out += "\"flame-leviathan\":" + std::to_string(sPlayerbotAIConfig.ulduarFlameLeviathanHardMode ? 1 : 0);
    out += ",\"xt-002\":" + std::to_string(sPlayerbotAIConfig.ulduarXT002HardMode ? 1 : 0);
    out += ",\"iron-assembly\":" + std::to_string(sPlayerbotAIConfig.ulduarIronAssemblyHardMode ? 1 : 0);
    out += ",\"thorim\":" + std::to_string(sPlayerbotAIConfig.ulduarThorimHardMode ? 1 : 0);
    out += ",\"freya\":" + std::to_string(sPlayerbotAIConfig.ulduarFreyaHardMode ? 1 : 0);
    out += ",\"mimiron\":" + std::to_string(sPlayerbotAIConfig.ulduarMimironHardMode ? 1 : 0);
    out += ",\"vezax\":" + std::to_string(sPlayerbotAIConfig.ulduarVezaxHardMode ? 1 : 0);
    out += ",\"yogg-saron\":" + std::to_string(sPlayerbotAIConfig.ulduarYoggSaronHardMode ? 1 : 0);
    out += "}}";

    return out;
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

// --- config and lifecycle -----------------------------------------------------

void LoadConfig()
{
    ResolveBinaryStamp();

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
