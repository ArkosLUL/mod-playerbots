/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SimWeightsMgr.h"
#include "BisListMgr.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "Log.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "QueryResult.h"
#include "StatsCollector.h"
#include <algorithm>
#include <bit>
#include <map>
#include <set>
#include <string>

static_assert(SimWeights::STAT_COUNT == STATS_TYPE_MAX);

#define SIM_WEIGHTS_STAT(name) static_assert(SimWeights::StatIndex(#name) == STATS_TYPE_##name)
SIM_WEIGHTS_STAT(AGILITY);
SIM_WEIGHTS_STAT(STRENGTH);
SIM_WEIGHTS_STAT(INTELLECT);
SIM_WEIGHTS_STAT(SPIRIT);
SIM_WEIGHTS_STAT(STAMINA);
SIM_WEIGHTS_STAT(HIT);
SIM_WEIGHTS_STAT(CRIT);
SIM_WEIGHTS_STAT(HASTE);
SIM_WEIGHTS_STAT(ARMOR);
SIM_WEIGHTS_STAT(DEFENSE);
SIM_WEIGHTS_STAT(DODGE);
SIM_WEIGHTS_STAT(PARRY);
SIM_WEIGHTS_STAT(BLOCK_VALUE);
SIM_WEIGHTS_STAT(BLOCK_RATING);
SIM_WEIGHTS_STAT(RESILIENCE);
SIM_WEIGHTS_STAT(HEALTH_REGENERATION);
SIM_WEIGHTS_STAT(SPELL_POWER);
SIM_WEIGHTS_STAT(SPELL_PENETRATION);
SIM_WEIGHTS_STAT(HEAL_POWER);
SIM_WEIGHTS_STAT(MANA_REGENERATION);
SIM_WEIGHTS_STAT(ATTACK_POWER);
SIM_WEIGHTS_STAT(ARMOR_PENETRATION);
SIM_WEIGHTS_STAT(EXPERTISE);
SIM_WEIGHTS_STAT(MELEE_DPS);
SIM_WEIGHTS_STAT(RANGED_DPS);
SIM_WEIGHTS_STAT(BONUS);
#undef SIM_WEIGHTS_STAT

static_assert(SimWeights::TAB_PRIEST_SMITE != BIS_TAB_DRUID_BEAR && SimWeights::TAB_PRIEST_SMITE != BIS_TAB_DK_BLOOD_TANK &&
              SimWeights::TAB_PRIEST_SMITE != BIS_TAB_WARRIOR_FURY_PROT && SimWeights::TAB_PRIEST_SMITE != BIS_TAB_NONE);

namespace
{
char const* const TABLE = "playerbots_sim_weights";
std::set<std::string> const COLUMNS = {"class", "tab", "phase", "gear_ilvl", "stat", "weight", "anchor"};

// Querying a table that isn't there kills the worldserver, and this one only exists once its update
// SQL has run.
bool TableReady(std::string& missing)
{
    std::set<std::string> found;
    QueryResult result = PlayerbotsDatabase.Query(
        "SELECT COLUMN_NAME FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '{}'",
        TABLE);
    if (result)
    {
        do
        {
            found.insert(result->Fetch()[0].Get<std::string>());
        } while (result->NextRow());
    }

    for (std::string const& column : COLUMNS)
    {
        if (!found.contains(column))
        {
            missing = found.empty() ? std::string(TABLE) : std::string(TABLE) + '.' + column;
            return false;
        }
    }
    return true;
}
}  // namespace

void SimWeightsMgr::LoadAll()
{
    if (!sPlayerbotAIConfig.simWeightsEnable)
    {
        Swap(nullptr);
        LOG_INFO("server.loading", "Sim stat weights disabled (AiPlayerbot.SimWeights.Enable = 0)");
        return;
    }

    std::string missing;
    if (!TableReady(missing))
    {
        Swap(nullptr);
        LOG_INFO("server.loading", "Sim stat weights not loaded: {} missing", missing);
        return;
    }

    QueryResult result =
        PlayerbotsDatabase.Query("SELECT class, tab, phase, gear_ilvl, stat, weight, anchor FROM playerbots_sim_weights");
    if (!result)
    {
        Swap(nullptr);
        LOG_INFO("server.loading", "Sim stat weights not loaded: {} is empty", TABLE);
        return;
    }

    struct PhaseLoad
    {
        SimWeights::PhaseRow row;
        int anchor = -1;
        float anchorWeight = 0.0f;
        bool bad = false;
    };
    // spec key -> phase -> rows so far
    std::map<uint16, std::map<uint8, PhaseLoad>> loads;
    uint32 dropped = 0;

    do
    {
        Field* fields = result->Fetch();
        uint8 const cls = fields[0].Get<uint8>();
        uint8 const tab = fields[1].Get<uint8>();
        uint8 const phase = fields[2].Get<uint8>();
        float const ilvl = fields[3].Get<float>();
        std::string const stat = fields[4].Get<std::string>();
        float const weight = fields[5].Get<float>();
        bool const anchor = fields[6].Get<uint8>() != 0;

        int const index = SimWeights::StatIndex(stat);
        if (phase < 1 || phase > SimWeights::PHASE_MAX || index < 0 || ilvl <= 0.0f)
        {
            LOG_WARN("playerbots", "{}: dropped class {} tab {} phase {} stat {} (bad phase, stat or gear_ilvl)", TABLE, cls,
                     tab, phase, stat);
            ++dropped;
            continue;
        }

        PhaseLoad& load = loads[SimWeightsSnapshot::SpecKey(cls, tab)][phase];
        load.row.phase = phase;
        load.row.ilvl = ilvl;
        load.row.w[index] = weight;
        load.row.measured |= 1u << index;
        if (anchor)
        {
            if (load.anchor >= 0 && load.anchor != index)
                load.bad = true;
            load.anchor = index;
            load.anchorWeight = weight;
        }
    } while (result->NextRow());

    auto snapshot = std::make_shared<SimWeightsSnapshot>();
    for (auto& [key, phases] : loads)
    {
        SimWeights::Spec spec;
        for (auto& [phase, load] : phases)
        {
            // every row is relative to the anchor, so a phase without one can't be scaled
            if (load.bad || load.anchor < 0 || load.anchorWeight <= 0.0f ||
                (spec.anchor >= 0 && spec.anchor != load.anchor))
            {
                LOG_WARN("playerbots", "{}: dropped class {} tab {} phase {}: no single positive anchor row", TABLE,
                         key >> 8, key & 0xFF, phase);
                ++dropped;
                continue;
            }

            for (float& w : load.row.w)
                w /= load.anchorWeight;

            spec.anchor = load.anchor;
            spec.rows.push_back(load.row);
            snapshot->rowCount += static_cast<uint32>(std::popcount(load.row.measured));
        }

        if (spec.rows.empty())
            continue;

        std::sort(spec.rows.begin(), spec.rows.end(),
                  [](SimWeights::PhaseRow const& a, SimWeights::PhaseRow const& b) { return a.ilvl < b.ilvl; });
        snapshot->specs.emplace(key, std::move(spec));
    }

    LOG_INFO("server.loading", "Loaded {} sim stat weights for {} specs, {} dropped", snapshot->rowCount,
             static_cast<uint32>(snapshot->specs.size()), dropped);
    if (snapshot->specs.empty())
        Swap(nullptr);
    else
        Swap(std::move(snapshot));
}

std::shared_ptr<SimWeightsSnapshot const> SimWeightsMgr::Get() const
{
    std::lock_guard<std::mutex> guard(_snapshotMutex);
    return _snapshot;
}

void SimWeightsMgr::Swap(std::shared_ptr<SimWeightsSnapshot const> snapshot)
{
    std::shared_ptr<SimWeightsSnapshot const> old;
    {
        std::lock_guard<std::mutex> guard(_snapshotMutex);
        old = std::move(_snapshot);
        _snapshot = std::move(snapshot);
    }
}

bool SimWeightsMgr::ResolveKey(uint8 cls, uint8 specTab, bool isTank, bool isHeal, uint8& simTab)
{
    if (isTank || isHeal)
        return false;

    switch (cls)
    {
        case CLASS_WARRIOR:
            if (specTab == WARRIOR_TAB_PROTECTION)
                return false;
            break;
        case CLASS_PALADIN:
            if (specTab != PALADIN_TAB_RETRIBUTION)
                return false;
            break;
        case CLASS_HUNTER:
            if (specTab == HUNTER_TAB_BEAST_MASTERY)
                return false;
            break;
        case CLASS_PRIEST:
            if (specTab != PRIEST_TAB_SHADOW)
            {
                simTab = SimWeights::TAB_PRIEST_SMITE;
                return true;
            }
            break;
        case CLASS_SHAMAN:
            if (specTab == SHAMAN_TAB_RESTORATION)
                return false;
            break;
        case CLASS_DRUID:
            if (specTab == DRUID_TAB_RESTORATION)
                return false;
            break;
        case CLASS_ROGUE:
        case CLASS_DEATH_KNIGHT:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            break;
        default:
            return false;
    }

    simTab = specTab;
    return true;
}
