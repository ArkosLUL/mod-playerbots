/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_BISDATASETMGR_H
#define PLAYERBOTS_BISDATASETMGR_H

#include "BisListMgr.h"
#include "BisWire.h"
#include "Define.h"
#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// One bistooltip_subject row: a roster raider (by character guid) or a class spec, with its sim BiS
// block per content phase.
struct BisDatasetSubject
{
    enum Kind : uint8
    {
        KIND_ROSTER = 0,
        KIND_SPEC = 1
    };

    uint16 id = 0;
    uint8 kind = KIND_SPEC;
    // character guid low, roster only
    uint32 guid = 0;
    std::string name;
    uint8 cls = 0;
    std::string specName;
    // BisListMgr's tab for the spec, BIS_TAB_NONE when the spec name maps to none
    uint8 tab = BIS_TAB_NONE;
    uint8 variant = 0;
    // indexed by content phase 1..BIS_PHASE_MAX, [0] always empty
    std::array<std::optional<BisWire::Block>, BIS_PHASE_MAX + 1> blocks;
    // item -> (phase, best rank over every sim slot listing it in that phase)
    std::unordered_map<uint32, std::vector<std::pair<uint8, uint8>>> ranks;

    BisWire::Block const* BlockFor(uint8 phase) const
    {
        return phase < blocks.size() && blocks[phase] ? &*blocks[phase] : nullptr;
    }

    // Latest phase at or below cap that has a block, 0 for none.
    uint8 PhaseAtOrBelow(uint8 cap) const;

    // Best rank at or below cap and the latest phase holding it, like BisListMgr::GetBisRankFor.
    uint8 RankFor(uint32 itemId, uint8 cap, uint8* outPhase = nullptr) const;
};

// An immutable load of the dataset. Readers keep the shared_ptr while they use any subject in it,
// since a reload swaps in a new one.
struct BisDatasetSnapshot
{
    BisDatasetSnapshot() = default;
    // the indexes point into subjects
    BisDatasetSnapshot(BisDatasetSnapshot const&) = delete;
    BisDatasetSnapshot& operator=(BisDatasetSnapshot const&) = delete;

    static uint16 SpecKey(uint8 cls, uint8 tab) { return (uint16(cls) << 8) | tab; }

    std::string version;
    std::string simCommit;
    std::string catalogDate;
    std::string objective;
    // by id
    std::vector<BisDatasetSubject> subjects;
    uint32 blockCount = 0;
    // subjects with at least one block and a mapped spec only
    std::unordered_map<uint32, BisDatasetSubject const*> rosterByGuid;
    // SpecKey -> spec subject; a canonical build beats a variant on the same tab
    std::unordered_map<uint16, BisDatasetSubject const*> specByKey;
    // every item ranked anywhere in the dataset
    std::unordered_set<uint32> items;
};

// Loads the BisTooltipAC tables (bistooltip_dataset/subject/block in the world DB). Reload runs
// before DBCs and ObjectMgr load, so it only decodes and indexes; Audit is the part that checks spells
// and gems.
class BisDatasetMgr
{
public:
    static BisDatasetMgr& instance()
    {
        static BisDatasetMgr inst;
        return inst;
    }

    // World thread only. A failure keeps the previous snapshot; error says why. No tables, no dataset
    // row or AiPlayerbot.BisDataset.Enable = 0 all succeed with no snapshot.
    bool Reload(std::string& error);

    // null when no dataset is loaded. Safe from map threads.
    std::shared_ptr<BisDatasetSnapshot const> Get() const;

    // World thread: every PollSeconds, reloads when the dataset version or the tables change.
    void Update(uint32 diff);

    // What is loaded and how the last reload went, one line each.
    std::vector<std::string> Describe() const;

    // Dataset enchants that aren't item-enchant spells and gems without gem properties. Needs the
    // spell and item stores, so not before the world is up.
    std::vector<std::string> Audit() const;

private:
    BisDatasetMgr() = default;

    // what the poll compares: whether the tables exist and the dataset versions they hold
    struct Observed
    {
        bool tables = false;
        std::string versions;

        bool operator==(Observed const& other) const
        {
            return tables == other.tables && versions == other.versions;
        }
    };

    static Observed Observe();
    // false with error set rejects the load; true with out null and why set means no dataset
    static bool Load(std::unique_ptr<BisDatasetSnapshot>& out, Observed& seen, std::vector<std::string>& warnings,
                     std::string& why, std::string& error);
    void Swap(std::shared_ptr<BisDatasetSnapshot const> snapshot);

    mutable std::mutex _snapshotMutex;
    std::shared_ptr<BisDatasetSnapshot const> _snapshot;

    // reload bookkeeping, world thread
    mutable std::mutex _reloadMutex;
    std::optional<Observed> _seen;
    std::string _lastOutcome;
    bool _lastFailed = false;
    uint32 _pollTimer = 0;
};

#define sBisDatasetMgr BisDatasetMgr::instance()

#endif
