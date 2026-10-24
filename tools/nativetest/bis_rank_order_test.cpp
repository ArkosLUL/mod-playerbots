/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BisRankOrder.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

// Not assert: these have to fail under NDEBUG too.
#define CHECK(cond)                                                                \
    do                                                                             \
    {                                                                              \
        if (!(cond))                                                               \
        {                                                                          \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            std::exit(1);                                                          \
        }                                                                          \
    } while (0)

namespace
{
using BisRankOrder::AddBlock;
using BisRankOrder::Best;
using BisRankOrder::Compare;
using BisRankOrder::Hand;
using BisRankOrder::ItemRanks;
using BisRankOrder::Listings;

// listings of a Beast Mastery hunter's roster subject, phases 2 and 3
Listings const IRON_STUDDED_MANTLE = {{2, 1}, {3, 6}};
Listings const SHOULDERS_OF_MISFORTUNE = {{2, 2}};
Listings const WINDRUNNERS_SPAULDERS = {{3, 3}};
Listings const BRASS_LINED_BOOTS = {{2, 1}};
Listings const GREAVES_OF_SWIFT_VENGEANCE = {{2, 3}, {3, 6}};
Listings const CINDERSHARD_RING = {{2, 1}, {3, 4}};
Listings const BRANNS_SEALING_RING = {{2, 2}, {3, 2}};
Listings const TITANIUM_IMPACT_BAND = {{2, 3}, {3, 4}};

void BetterRankWinsBothWays()
{
    CHECK(Compare(IRON_STUDDED_MANTLE, SHOULDERS_OF_MISFORTUNE, 3) > 0);
    CHECK(Compare(IRON_STUDDED_MANTLE, SHOULDERS_OF_MISFORTUNE, 2) > 0);
    CHECK(Compare(SHOULDERS_OF_MISFORTUNE, IRON_STUDDED_MANTLE, 3) < 0);
    CHECK(Compare(SHOULDERS_OF_MISFORTUNE, IRON_STUDDED_MANTLE, 2) < 0);
}

void PhaseAboveTheCapIsIgnored()
{
    CHECK(Compare(IRON_STUDDED_MANTLE, WINDRUNNERS_SPAULDERS, 3) < 0);
    CHECK(Compare(IRON_STUDDED_MANTLE, WINDRUNNERS_SPAULDERS, 2) == 0);
}

// phase 3 lists only the greaves, so phase 2 is the one that compares the two
void LatestPhaseListingBothDecides()
{
    CHECK(Compare(BRASS_LINED_BOOTS, GREAVES_OF_SWIFT_VENGEANCE, 3) > 0);
    CHECK(Compare(GREAVES_OF_SWIFT_VENGEANCE, BRASS_LINED_BOOTS, 3) < 0);

    CHECK(Compare(CINDERSHARD_RING, BRANNS_SEALING_RING, 3) < 0);
    CHECK(Compare(CINDERSHARD_RING, BRANNS_SEALING_RING, 2) > 0);
}

void TieInTheLatestPhaseIsNoVerdict()
{
    CHECK(Compare(CINDERSHARD_RING, TITANIUM_IMPACT_BAND, 3) == 0);
    CHECK(Compare(CINDERSHARD_RING, TITANIUM_IMPACT_BAND, 2) > 0);
}

void UnlistedIsNoVerdict()
{
    CHECK(Compare(IRON_STUDDED_MANTLE, {}, 3) == 0);
    CHECK(Compare({}, IRON_STUDDED_MANTLE, 3) == 0);
    CHECK(Compare(IRON_STUDDED_MANTLE, SHOULDERS_OF_MISFORTUNE, 1) == 0);
}

// the order the listings come in must not matter
void ListingOrderDoesNotMatter()
{
    Listings const mantleReversed = {{3, 6}, {2, 1}};
    Listings const greavesReversed = {{3, 6}, {2, 3}};
    CHECK(Compare(mantleReversed, SHOULDERS_OF_MISFORTUNE, 3) > 0);
    CHECK(Compare(mantleReversed, WINDRUNNERS_SPAULDERS, 3) < 0);
    CHECK(Compare(BRASS_LINED_BOOTS, greavesReversed, 3) > 0);
}

BisWire::Slot SimSlot(uint8_t simSlot, std::vector<uint32_t> items)
{
    BisWire::Slot slot;
    slot.simSlot = simSlot;
    slot.items = std::move(items);
    return slot;
}

// sim slots 14 and 15 are the main and off hand, 10 a finger
void HandsKeepTheirOwnRanks()
{
    constexpr uint32_t MAIN_ONLY = 1;
    constexpr uint32_t BOTH = 2;
    constexpr uint32_t RING = 3;

    BisWire::Block block;
    block.slots = {SimSlot(10, {RING}), SimSlot(14, {MAIN_ONLY, BOTH}), SimSlot(15, {BOTH})};

    std::unordered_map<uint32_t, ItemRanks> ranks;
    AddBlock(ranks, block, 2);

    ItemRanks const& mainOnly = ranks[MAIN_ONLY];
    CHECK(Best(mainOnly.For(Hand::Main), 5) == 1);
    CHECK(Best(mainOnly.For(Hand::Off), 5) == 0);
    CHECK(Best(mainOnly.For(Hand::Any), 5) == 1);

    ItemRanks const& both = ranks[BOTH];
    CHECK(Best(both.For(Hand::Main), 5) == 2);
    CHECK(Best(both.For(Hand::Off), 5) == 1);
    CHECK(Best(both.For(Hand::Any), 5) == 1);

    ItemRanks const& ring = ranks[RING];
    CHECK(Best(ring.For(Hand::Any), 5) == 1);
    CHECK(ring.main.empty() && ring.off.empty());
}

// one entry per phase with the best rank of that phase; Best takes the latest phase at that rank
void PhasesMergeAndCap()
{
    constexpr uint32_t ITEM = 7;

    BisWire::Block early;
    early.slots = {SimSlot(14, {9, ITEM})};
    BisWire::Block late;
    late.slots = {SimSlot(14, {ITEM}), SimSlot(15, {9, 8, ITEM})};

    std::unordered_map<uint32_t, ItemRanks> ranks;
    AddBlock(ranks, early, 1);
    AddBlock(ranks, late, 3);

    ItemRanks const& item = ranks[ITEM];
    CHECK(item.any.size() == 2);

    uint8_t phase = 0;
    CHECK(Best(item.any, 5, &phase) == 1 && phase == 3);
    CHECK(Best(item.any, 2, &phase) == 2 && phase == 1);
    CHECK(Best(item.off, 5, &phase) == 3 && phase == 3);
    CHECK(Best(item.off, 2) == 0);
}
}  // namespace

int main()
{
    BetterRankWinsBothWays();
    PhaseAboveTheCapIsIgnored();
    LatestPhaseListingBothDecides();
    TieInTheLatestPhaseIsNoVerdict();
    UnlistedIsNoVerdict();
    ListingOrderDoesNotMatter();
    HandsKeepTheirOwnRanks();
    PhasesMergeAndCap();
    std::puts("bis_rank_order_test: ok");
    return 0;
}
