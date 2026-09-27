/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BisDatasetMgr.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAIConfig.h"
#include "QueryResult.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "StringFormat.h"
#include <algorithm>
#include <fmt/ranges.h>
#include <initializer_list>
#include <map>
#include <set>

static_assert(BisWire::TAB_FERAL_TANK == BIS_TAB_DRUID_BEAR);
static_assert(BisWire::TAB_BLOOD_TANK == BIS_TAB_DK_BLOOD_TANK);
static_assert(BisWire::TAB_FURY_PROT == BIS_TAB_WARRIOR_FURY_PROT);

static_assert(BisWire::EQUIP_SLOT[0] == EQUIPMENT_SLOT_HEAD);
static_assert(BisWire::EQUIP_SLOT[1] == EQUIPMENT_SLOT_NECK);
static_assert(BisWire::EQUIP_SLOT[2] == EQUIPMENT_SLOT_SHOULDERS);
static_assert(BisWire::EQUIP_SLOT[3] == EQUIPMENT_SLOT_BACK);
static_assert(BisWire::EQUIP_SLOT[4] == EQUIPMENT_SLOT_CHEST);
static_assert(BisWire::EQUIP_SLOT[5] == EQUIPMENT_SLOT_WRISTS);
static_assert(BisWire::EQUIP_SLOT[6] == EQUIPMENT_SLOT_HANDS);
static_assert(BisWire::EQUIP_SLOT[7] == EQUIPMENT_SLOT_WAIST);
static_assert(BisWire::EQUIP_SLOT[8] == EQUIPMENT_SLOT_LEGS);
static_assert(BisWire::EQUIP_SLOT[9] == EQUIPMENT_SLOT_FEET);
static_assert(BisWire::EQUIP_SLOT[10] == EQUIPMENT_SLOT_FINGER1);
static_assert(BisWire::EQUIP_SLOT[11] == EQUIPMENT_SLOT_FINGER2);
static_assert(BisWire::EQUIP_SLOT[12] == EQUIPMENT_SLOT_TRINKET1);
static_assert(BisWire::EQUIP_SLOT[13] == EQUIPMENT_SLOT_TRINKET2);
static_assert(BisWire::EQUIP_SLOT[14] == EQUIPMENT_SLOT_MAINHAND);
static_assert(BisWire::EQUIP_SLOT[15] == EQUIPMENT_SLOT_OFFHAND);
static_assert(BisWire::EQUIP_SLOT[16] == EQUIPMENT_SLOT_RANGED);

namespace
{
struct Table
{
    std::string name;
    std::vector<std::string> columns;
};

// columns in the order Load reads the fields
Table const DATASET_TABLE = {"bistooltip_dataset", {"version", "sim_commit", "catalog_date", "objective"}};
Table const SUBJECT_TABLE = {"bistooltip_subject", {"id", "kind", "guid", "name", "class_id", "spec_name"}};
Table const BLOCK_TABLE = {"bistooltip_block", {"subject_id", "content_phase", "payload", "checksum"}};

// mod-bis-tooltip accepts phases up to 9, playerbots has no phase past Ruby Sanctum
constexpr uint8 MAX_EXPORT_PHASE = 9;

// ids past this many are summed up, not listed
constexpr std::size_t MAX_AUDIT_IDS = 30;

std::string SelectQuery(Table const& table)
{
    return Acore::StringFormat("SELECT `{}` FROM `{}`", fmt::join(table.columns, "`, `"), table.name);
}

// what the world DB lacks of tables: `table` for a whole table, else `table.column`
std::vector<std::string> FindMissing(std::initializer_list<Table const*> tables)
{
    std::vector<std::string_view> names;
    for (Table const* table : tables)
        names.push_back(table->name);

    std::map<std::string, std::set<std::string>> existing;
    QueryResult result = WorldDatabase.Query(
        "SELECT TABLE_NAME, COLUMN_NAME FROM information_schema.COLUMNS "
        "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME IN ('{}')",
        fmt::join(names, "', '"));
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            existing[fields[0].Get<std::string>()].insert(fields[1].Get<std::string>());
        } while (result->NextRow());
    }

    std::vector<std::string> missing;
    for (Table const* table : tables)
    {
        auto const it = existing.find(table->name);
        if (it == existing.end())
        {
            missing.push_back(table->name);
            continue;
        }

        for (std::string const& column : table->columns)
            if (!it->second.contains(column))
                missing.push_back(table->name + '.' + column);
    }

    return missing;
}

std::vector<std::string> ReadVersions()
{
    std::vector<std::string> versions;
    QueryResult result = WorldDatabase.Query("SELECT `version` FROM `bistooltip_dataset` ORDER BY `version`");
    if (result)
    {
        do
            versions.push_back(result->Fetch()[0].Get<std::string>());
        while (result->NextRow());
    }

    return versions;
}

std::string ListIds(std::set<uint32> const& ids)
{
    if (ids.empty())
        return "none";

    std::vector<uint32> shown(ids.begin(), ids.end());
    std::string text = std::to_string(ids.size()) + ": ";
    if (shown.size() > MAX_AUDIT_IDS)
    {
        shown.resize(MAX_AUDIT_IDS);
        return text + Acore::StringFormat("{} and {} more", fmt::join(shown, ", "), ids.size() - MAX_AUDIT_IDS);
    }

    return text + Acore::StringFormat("{}", fmt::join(shown, ", "));
}
}  // namespace

uint8 BisDatasetSubject::PhaseAtOrBelow(uint8 cap) const
{
    for (uint8 phase = std::min<uint8>(cap, BIS_PHASE_MAX); phase > 0; --phase)
        if (blocks[phase])
            return phase;

    return 0;
}

uint8 BisDatasetSubject::RankFor(uint32 itemId, uint8 cap, uint8* outPhase) const
{
    if (outPhase)
        *outPhase = 0;

    auto const it = ranks.find(itemId);
    if (it == ranks.end())
        return 0;

    uint8 best = 0;
    uint8 bestPhase = 0;
    for (auto const& [phase, rank] : it->second)
    {
        if (phase > cap)
            continue;

        if (!best || rank < best)
        {
            best = rank;
            bestPhase = phase;
        }
        else if (rank == best && phase > bestPhase)
            bestPhase = phase;
    }

    if (outPhase)
        *outPhase = bestPhase;

    return best;
}

std::shared_ptr<BisDatasetSnapshot const> BisDatasetMgr::Get() const
{
    std::lock_guard<std::mutex> guard(_snapshotMutex);
    return _snapshot;
}

void BisDatasetMgr::Swap(std::shared_ptr<BisDatasetSnapshot const> snapshot)
{
    // freeing a snapshot takes a while, so not under the lock map threads read through
    std::shared_ptr<BisDatasetSnapshot const> old;
    {
        std::lock_guard<std::mutex> guard(_snapshotMutex);
        old = std::move(_snapshot);
        _snapshot = std::move(snapshot);
    }
}

BisDatasetMgr::Observed BisDatasetMgr::Observe()
{
    Observed seen;
    if (!FindMissing({&DATASET_TABLE, &SUBJECT_TABLE, &BLOCK_TABLE}).empty())
        return seen;

    seen.tables = true;
    seen.versions = Acore::StringFormat("{}", fmt::join(ReadVersions(), ","));
    return seen;
}

bool BisDatasetMgr::Load(std::unique_ptr<BisDatasetSnapshot>& out, Observed& seen,
                         std::vector<std::string>& warnings, std::string& why, std::string& error)
{
    out.reset();

    // MySQLConnection aborts the server on a missing table or column, so check before selecting
    std::vector<std::string> const missing = FindMissing({&DATASET_TABLE, &SUBJECT_TABLE, &BLOCK_TABLE});
    if (!missing.empty())
    {
        why = Acore::StringFormat("the world DB has no {}", fmt::join(missing, ", "));
        return true;
    }

    seen.tables = true;
    auto snapshot = std::make_unique<BisDatasetSnapshot>();

    QueryResult result = WorldDatabase.Query(SelectQuery(DATASET_TABLE) + " ORDER BY `version`");
    std::vector<std::string> versions;
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            versions.push_back(fields[0].Get<std::string>());
            snapshot->version = fields[0].Get<std::string>();
            snapshot->simCommit = fields[1].Get<std::string>();
            snapshot->catalogDate = fields[2].Get<std::string>();
            snapshot->objective = fields[3].Get<std::string>();
        } while (result->NextRow());
    }

    seen.versions = Acore::StringFormat("{}", fmt::join(versions, ","));
    if (versions.empty())
    {
        why = "bistooltip_dataset is empty";
        return true;
    }

    if (versions.size() != 1)
    {
        error = Acore::StringFormat("bistooltip_dataset has {} rows, want 1", versions.size());
        return false;
    }

    std::vector<BisDatasetSubject>& subjects = snapshot->subjects;
    result = WorldDatabase.Query(SelectQuery(SUBJECT_TABLE) + " ORDER BY `id`");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            BisDatasetSubject subject;
            subject.id = fields[0].Get<uint16>();
            subject.kind = fields[1].Get<uint8>();
            subject.guid = fields[2].Get<uint32>();
            subject.name = fields[3].Get<std::string>();
            subject.cls = fields[4].Get<uint8>();
            subject.specName = fields[5].Get<std::string>();

            if (subject.kind != BisDatasetSubject::KIND_ROSTER && subject.kind != BisDatasetSubject::KIND_SPEC)
            {
                error = Acore::StringFormat("subject {} has kind {}", subject.id, subject.kind);
                return false;
            }

            if (std::optional<BisWire::SpecKey> key = BisWire::SpecKeyFor(subject.cls, subject.specName))
            {
                subject.tab = key->tab;
                subject.variant = key->variant;
            }
            else
                warnings.push_back(Acore::StringFormat("subject {}: class {} spec '{}' has no BiS list tab, unused",
                                                       subject.id, subject.cls, subject.specName));

            subjects.push_back(std::move(subject));
        } while (result->NextRow());
    }

    std::unordered_map<uint16, BisDatasetSubject*> byId;
    for (BisDatasetSubject& subject : subjects)
        byId[subject.id] = &subject;

    std::unordered_set<std::string> payloads;
    result = WorldDatabase.Query(SelectQuery(BLOCK_TABLE));
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint16 const subjectId = fields[0].Get<uint16>();
            uint8 const phase = fields[1].Get<uint8>();
            std::string payload = fields[2].Get<std::string>();
            std::string const checksum = fields[3].Get<std::string>();

            auto const it = byId.find(subjectId);
            if (it == byId.end())
            {
                error = Acore::StringFormat("block {}/{} has no subject", subjectId, phase);
                return false;
            }

            if (phase < 1 || phase > MAX_EXPORT_PHASE)
            {
                error = Acore::StringFormat("block {}/{} has content phase {}", subjectId, phase, phase);
                return false;
            }

            if (phase > BIS_PHASE_MAX)
            {
                warnings.push_back(Acore::StringFormat("block {}/{}: phase {} is past Ruby Sanctum, skipped",
                                                       subjectId, phase, phase));
                continue;
            }

            if (BisWire::Checksum(payload) != checksum)
            {
                error = Acore::StringFormat("block {}/{} checksum {} doesn't match its payload ({})", subjectId,
                                            phase, checksum, BisWire::Checksum(payload));
                return false;
            }

            BisWire::Block block;
            std::string decodeError;
            if (!BisWire::Decode(payload, block, decodeError))
            {
                error = Acore::StringFormat("block {}/{}: {}", subjectId, phase, decodeError);
                return false;
            }

            it->second->blocks[phase] = std::move(block);
            payloads.insert(std::move(payload));
            ++snapshot->blockCount;
        } while (result->NextRow());
    }

    // an import between the queries above would mix two datasets under one version
    std::vector<std::string> const recheck = ReadVersions();
    if (recheck.size() != 1 || recheck.front() != snapshot->version)
    {
        error = "the dataset changed while it was loading";
        return false;
    }

    // The unconfigured export puts one payload on every subject. Loading it would hand every class
    // the same gear, so it counts as no dataset at all.
    std::set<uint8> classes;
    for (BisDatasetSubject const& subject : subjects)
        if (subject.PhaseAtOrBelow(BIS_PHASE_MAX))
            classes.insert(subject.cls);

    if (payloads.size() == 1 && classes.size() >= 2)
    {
        error = Acore::StringFormat("dataset {} is a placeholder: one payload on the subjects of {} classes",
                                    snapshot->version, classes.size());
        return false;
    }

    for (BisDatasetSubject& subject : subjects)
    {
        for (uint8 phase = 1; phase <= BIS_PHASE_MAX; ++phase)
        {
            BisWire::Block const* block = subject.BlockFor(phase);
            if (!block)
                continue;

            for (BisWire::Slot const& slot : block->slots)
            {
                for (std::size_t i = 0; i < slot.items.size(); ++i)
                {
                    uint8 const rank = uint8(i + 1);
                    std::vector<std::pair<uint8, uint8>>& entries = subject.ranks[slot.items[i]];
                    auto const entry = std::find_if(entries.begin(), entries.end(),
                                                    [phase](auto const& e) { return e.first == phase; });
                    if (entry == entries.end())
                        entries.emplace_back(phase, rank);
                    else
                        entry->second = std::min(entry->second, rank);

                    snapshot->items.insert(slot.items[i]);
                }
            }
        }
    }

    // subjects is final from here on, the indexes point into it; ordered by id, so the first wins
    for (BisDatasetSubject const& subject : subjects)
    {
        if (subject.tab == BIS_TAB_NONE || !subject.PhaseAtOrBelow(BIS_PHASE_MAX))
            continue;

        if (subject.kind == BisDatasetSubject::KIND_ROSTER)
        {
            if (!subject.guid)
            {
                warnings.push_back(Acore::StringFormat("roster subject {} ({}) has no character guid, unused",
                                                       subject.id, subject.name));
                continue;
            }

            auto const [it, added] = snapshot->rosterByGuid.emplace(subject.guid, &subject);
            if (!added)
                warnings.push_back(Acore::StringFormat("roster subjects {} and {} share guid {}, using {}",
                                                       it->second->id, subject.id, subject.guid, it->second->id));
            continue;
        }

        uint16 const key = BisDatasetSnapshot::SpecKey(subject.cls, subject.tab);
        auto const [it, added] = snapshot->specByKey.emplace(key, &subject);
        if (added)
            continue;

        BisDatasetSubject const* kept = it->second;
        if (kept->variant && !subject.variant)
            it->second = &subject;
        else if (!kept->variant && !subject.variant)
            warnings.push_back(Acore::StringFormat("spec subjects {} and {} are both class {} '{}', using {}",
                                                   kept->id, subject.id, subject.cls, subject.specName, kept->id));
    }

    out = std::move(snapshot);
    return true;
}

bool BisDatasetMgr::Reload(std::string& error)
{
    std::lock_guard<std::mutex> guard(_reloadMutex);
    error.clear();

    if (!sPlayerbotAIConfig.bisDatasetEnable)
    {
        Swap(nullptr);
        _seen.reset();
        _lastFailed = false;
        _lastOutcome = "disabled by AiPlayerbot.BisDataset.Enable";
        return true;
    }

    std::unique_ptr<BisDatasetSnapshot> loaded;
    Observed seen;
    std::vector<std::string> warnings;
    std::string why;
    bool const ok = Load(loaded, seen, warnings, why, error);
    _seen = seen;

    for (std::string const& warning : warnings)
        LOG_WARN("playerbots", "Sim BiS dataset: {}", warning);

    if (!ok)
    {
        _lastFailed = true;
        _lastOutcome = error;
        LOG_WARN("playerbots", "Sim BiS dataset not loaded{}: {}", Get() ? ", keeping the previous one" : "", error);
        return false;
    }

    _lastFailed = false;
    if (!loaded)
    {
        Swap(nullptr);
        _lastOutcome = "no dataset: " + why;
        LOG_INFO("playerbots", "Sim BiS dataset: none, {}", why);
        return true;
    }

    _lastOutcome = Acore::StringFormat("loaded {} with {} warnings", loaded->version, warnings.size());
    LOG_INFO("playerbots", "Sim BiS dataset {} loaded: {} subjects, {} blocks, {} roster raiders, {} specs, {} items",
             loaded->version, loaded->subjects.size(), loaded->blockCount, loaded->rosterByGuid.size(),
             loaded->specByKey.size(), loaded->items.size());
    Swap(std::move(loaded));
    return true;
}

void BisDatasetMgr::Update(uint32 diff)
{
    uint64 const interval = uint64(sPlayerbotAIConfig.bisDatasetPollSeconds) * IN_MILLISECONDS;
    if (!interval)
        return;

    _pollTimer += diff;
    if (_pollTimer < interval)
        return;

    _pollTimer = 0;

    std::string error;
    if (!sPlayerbotAIConfig.bisDatasetEnable)
    {
        if (Get())
            Reload(error);

        return;
    }

    Observed const now = Observe();
    {
        std::lock_guard<std::mutex> guard(_reloadMutex);
        if (_seen && *_seen == now)
            return;
    }

    Reload(error);
}

std::vector<std::string> BisDatasetMgr::Describe() const
{
    std::vector<std::string> lines;
    std::shared_ptr<BisDatasetSnapshot const> const snapshot = Get();
    if (snapshot)
        lines.push_back(Acore::StringFormat(
            "Sim BiS dataset {} (sim {}, catalog {}, objective {}): {} subjects, {} blocks, {} roster raiders, "
            "{} specs, {} items",
            snapshot->version, snapshot->simCommit, snapshot->catalogDate, snapshot->objective,
            snapshot->subjects.size(), snapshot->blockCount, snapshot->rosterByGuid.size(),
            snapshot->specByKey.size(), snapshot->items.size()));
    else
        lines.push_back("Sim BiS dataset: none loaded, bots use the BiS lists");

    {
        std::lock_guard<std::mutex> guard(_reloadMutex);
        if (!_lastOutcome.empty())
            lines.push_back(Acore::StringFormat("Last reload {}: {}", _lastFailed ? "failed" : "ok", _lastOutcome));
    }

    uint32 const pollSeconds = sPlayerbotAIConfig.bisDatasetPollSeconds;
    lines.push_back(Acore::StringFormat("Enable {}, Enhancements {}, Reforges {}, {}",
                                        sPlayerbotAIConfig.bisDatasetEnable,
                                        sPlayerbotAIConfig.bisDatasetEnhancements,
                                        sPlayerbotAIConfig.bisDatasetReforges,
                                        pollSeconds ? Acore::StringFormat("version poll every {}s", pollSeconds)
                                                    : std::string("version poll off")));
    return lines;
}

std::vector<std::string> BisDatasetMgr::Audit() const
{
    std::shared_ptr<BisDatasetSnapshot const> const snapshot = Get();
    if (!snapshot)
        return {};

    std::set<uint32> badEnchants;
    std::set<uint32> badGems;
    for (BisDatasetSubject const& subject : snapshot->subjects)
    {
        for (std::optional<BisWire::Block> const& block : subject.blocks)
        {
            if (!block)
                continue;

            for (BisWire::Slot const& slot : block->slots)
            {
                if (slot.enchantSpell)
                {
                    SpellInfo const* spell = sSpellMgr->GetSpellInfo(slot.enchantSpell);
                    if (!spell || !spell->HasEffect(SPELL_EFFECT_ENCHANT_ITEM))
                        badEnchants.insert(slot.enchantSpell);
                }

                for (uint32 gem : slot.gems)
                {
                    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(gem);
                    if (!proto || !proto->GemProperties || !sGemPropertiesStore.LookupEntry(proto->GemProperties))
                        badGems.insert(gem);
                }
            }
        }
    }

    return {"Enchant spells without SPELL_EFFECT_ENCHANT_ITEM, " + ListIds(badEnchants),
            "Gems without GemProperties, " + ListIds(badGems)};
}
