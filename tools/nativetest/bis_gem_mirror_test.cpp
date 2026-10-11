/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BisGemMirror.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

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
using BisGemMirror::AlignedGems;
using BisGemMirror::Tally;

constexpr uint8_t RED = 2;
constexpr uint8_t YELLOW = 4;
constexpr uint8_t BLUE = 8;
constexpr uint8_t ADDED = 0;

constexpr uint32_t DELICATE_RUBY = 40112;
constexpr uint32_t THICK_AMBER = 40126;
constexpr uint32_t SHIFTING_DREADSTONE = 40130;
constexpr uint32_t RESOLUTE_AMETRINE = 40163;
constexpr uint32_t DELICATE_DRAGONS_EYE = 42143;
constexpr uint32_t NIGHTMARE_TEAR = 49110;

void GemPerTemplateSocket()
{
    CHECK(AlignedGems(2, 2, false) == 2);
    CHECK(AlignedGems(2, 2, true) == 2);
    CHECK(AlignedGems(0, 0, false) == 0);
}

void ExtraGemNeedsTheAddedSocket()
{
    CHECK(AlignedGems(3, 2, true) == 3);
    // the sim had a belt buckle this item lacks
    CHECK(AlignedGems(3, 2, false) == 2);
    // bracers with only the blacksmith socket
    CHECK(AlignedGems(1, 0, true) == 1);
    CHECK(AlignedGems(1, 0, false) == 0);
}

void OtherCountsDoNotLineUp()
{
    // an empty socket was left out, no telling which
    CHECK(AlignedGems(1, 2, false) == 0);
    CHECK(AlignedGems(1, 2, true) == 0);
    CHECK(AlignedGems(4, 2, true) == 0);
    CHECK(AlignedGems(0, 1, true) == 0);
}

void MostUsedFirst()
{
    Tally tally;
    tally.Add(YELLOW, RESOLUTE_AMETRINE, false);
    tally.Add(YELLOW, THICK_AMBER, false);
    tally.Add(YELLOW, THICK_AMBER, false);
    CHECK((tally.Ranked(YELLOW) == std::vector<uint32_t>{THICK_AMBER, RESOLUTE_AMETRINE}));
}

void TiesKeepTheOrderAdded()
{
    Tally tally;
    tally.Add(RED, DELICATE_RUBY, false);
    tally.Add(RED, SHIFTING_DREADSTONE, false);
    CHECK((tally.Ranked(RED) == std::vector<uint32_t>{DELICATE_RUBY, SHIFTING_DREADSTONE}));
}

void LimitedGemsGoFirst()
{
    Tally tally;
    for (int i = 0; i < 5; ++i)
        tally.Add(BLUE, SHIFTING_DREADSTONE, false);
    tally.Add(BLUE, NIGHTMARE_TEAR, true);
    CHECK((tally.Ranked(BLUE) == std::vector<uint32_t>{NIGHTMARE_TEAR, SHIFTING_DREADSTONE}));

    // two limited gems still go by count
    tally.Add(BLUE, DELICATE_DRAGONS_EYE, true);
    tally.Add(BLUE, DELICATE_DRAGONS_EYE, true);
    CHECK((tally.Ranked(BLUE) ==
           std::vector<uint32_t>{DELICATE_DRAGONS_EYE, NIGHTMARE_TEAR, SHIFTING_DREADSTONE}));
}

void ColorsStayApart()
{
    Tally tally;
    tally.Add(RED, DELICATE_RUBY, false);
    tally.Add(ADDED, SHIFTING_DREADSTONE, false);
    CHECK((tally.Ranked(RED) == std::vector<uint32_t>{DELICATE_RUBY}));
    CHECK((tally.Ranked(ADDED) == std::vector<uint32_t>{SHIFTING_DREADSTONE}));
    CHECK(tally.Ranked(YELLOW).empty());
}
}  // namespace

int main()
{
    GemPerTemplateSocket();
    ExtraGemNeedsTheAddedSocket();
    OtherCountsDoNotLineUp();
    MostUsedFirst();
    TiesKeepTheOrderAdded();
    LimitedGemsGoFirst();
    ColorsStayApart();
    std::puts("bis_gem_mirror_test: ok");
    return 0;
}
