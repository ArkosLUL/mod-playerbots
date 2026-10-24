/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_BISRANKORDER_H
#define PLAYERBOTS_BISRANKORDER_H

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include "BisWire.h"

// std only, so tools/nativetest can build it
namespace BisRankOrder
{
// one item's (phase, rank) pairs, the best rank per phase
using Listings = std::vector<std::pair<uint8_t, uint8_t>>;

// Any is the best rank over every slot that lists the item
enum class Hand : uint8_t
{
    Any,
    Main,
    Off
};

struct ItemRanks
{
    Listings any;
    Listings main;
    Listings off;

    Listings const& For(Hand hand) const
    {
        switch (hand)
        {
            case Hand::Main:
                return main;
            case Hand::Off:
                return off;
            default:
                return any;
        }
    }
};

inline void Merge(Listings& listings, uint8_t phase, uint8_t rank)
{
    auto const entry =
        std::find_if(listings.begin(), listings.end(), [phase](auto const& e) { return e.first == phase; });
    if (entry == listings.end())
        listings.emplace_back(phase, rank);
    else
        entry->second = std::min(entry->second, rank);
}

// Adds one phase's block. A weapon only gets a hand's ranks from the sim slot of that hand.
inline void AddBlock(std::unordered_map<uint32_t, ItemRanks>& ranks, BisWire::Block const& block, uint8_t phase)
{
    constexpr uint8_t MAIN_HAND = 15;
    constexpr uint8_t OFF_HAND = 16;

    for (BisWire::Slot const& slot : block.slots)
    {
        uint8_t const equipSlot = BisWire::EQUIP_SLOT[slot.simSlot];
        for (std::size_t i = 0; i < slot.items.size(); ++i)
        {
            uint8_t const rank = uint8_t(i + 1);
            ItemRanks& item = ranks[slot.items[i]];
            Merge(item.any, phase, rank);
            if (equipSlot == MAIN_HAND)
                Merge(item.main, phase, rank);
            else if (equipSlot == OFF_HAND)
                Merge(item.off, phase, rank);
        }
    }
}

// Best rank at or below cap and the latest phase holding it, 0 when nothing qualifies.
inline uint8_t Best(Listings const& listings, uint8_t cap, uint8_t* outPhase = nullptr)
{
    uint8_t best = 0;
    uint8_t bestPhase = 0;
    for (auto const& [phase, rank] : listings)
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

// > 0: a ranks higher, < 0: b. 0 when no phase at or below cap lists both, or they tie in the
// latest one that does. An earlier phase never breaks that tie, it was simmed on older gear.
inline int8_t Compare(Listings const& a, Listings const& b, uint8_t cap)
{
    uint8_t phase = 0;
    uint8_t rankA = 0;
    uint8_t rankB = 0;
    for (auto const& [phaseA, listedA] : a)
    {
        if (phaseA > cap || phaseA <= phase)
            continue;

        for (auto const& [phaseB, listedB] : b)
        {
            if (phaseB != phaseA)
                continue;

            phase = phaseA;
            rankA = listedA;
            rankB = listedB;
            break;
        }
    }

    if (!phase || rankA == rankB)
        return 0;

    return rankA < rankB ? 1 : -1;
}
}  // namespace BisRankOrder

#endif
