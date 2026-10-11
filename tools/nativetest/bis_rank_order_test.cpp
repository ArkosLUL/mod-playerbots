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
using BisRankOrder::Compare;
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
}  // namespace

int main()
{
    BetterRankWinsBothWays();
    PhaseAboveTheCapIsIgnored();
    LatestPhaseListingBothDecides();
    TieInTheLatestPhaseIsNoVerdict();
    UnlistedIsNoVerdict();
    ListingOrderDoesNotMatter();
    std::puts("bis_rank_order_test: ok");
    return 0;
}
