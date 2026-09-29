/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_SIMWEIGHTSMGR_H
#define PLAYERBOTS_SIMWEIGHTSMGR_H

#include "Define.h"
#include "SimWeights.h"
#include <memory>
#include <mutex>
#include <unordered_map>

// An immutable load of playerbots_sim_weights. Readers copy what they need out of it, since a reload
// swaps in a new one.
struct SimWeightsSnapshot
{
    static uint16 SpecKey(uint8 cls, uint8 tab) { return (uint16(cls) << 8) | tab; }

    SimWeights::Spec const* Find(uint8 cls, uint8 tab) const
    {
        auto const it = specs.find(SpecKey(cls, tab));
        return it == specs.end() ? nullptr : &it->second;
    }

    std::unordered_map<uint16, SimWeights::Spec> specs;
    uint32 rowCount = 0;
};

class SimWeightsMgr
{
public:
    static SimWeightsMgr& instance()
    {
        static SimWeightsMgr inst;
        return inst;
    }

    // Runs from PlayerbotAIConfig::Initialize, so again on every config reload. A missing table or
    // AiPlayerbot.SimWeights.Enable = 0 leaves no snapshot.
    void LoadAll();

    // null when nothing is loaded. Safe from map threads.
    std::shared_ptr<SimWeightsSnapshot const> Get() const;

    // The table's (class, tab) for a bot, false when the sim has no row for its role: tanks, healers,
    // Beast Mastery. Non-Shadow priests that don't heal get TAB_PRIEST_SMITE.
    static bool ResolveKey(uint8 cls, uint8 specTab, bool isTank, bool isHeal, uint8& simTab);

private:
    SimWeightsMgr() = default;

    void Swap(std::shared_ptr<SimWeightsSnapshot const> snapshot);

    mutable std::mutex _snapshotMutex;
    std::shared_ptr<SimWeightsSnapshot const> _snapshot;
};

#define sSimWeightsMgr SimWeightsMgr::instance()

#endif
