/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SimWeights.h"

#include <array>
#include <cmath>
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

using Weights = std::array<float, SimWeights::STAT_COUNT>;

constexpr int STR = SimWeights::StatIndex("STRENGTH");
constexpr int AGI = SimWeights::StatIndex("AGILITY");
constexpr int HIT = SimWeights::StatIndex("HIT");
constexpr int ARP = SimWeights::StatIndex("ARMOR_PENETRATION");
constexpr int SP = SimWeights::StatIndex("SPELL_POWER");
constexpr int INT = SimWeights::StatIndex("INTELLECT");
constexpr int STA = SimWeights::StatIndex("STAMINA");

bool Near(float a, float b)
{
    return std::fabs(a - b) < 1e-4f;
}

SimWeights::PhaseRow Row(uint8_t phase, float ilvl, float hit, float arp, uint32_t extraMeasured = 0)
{
    SimWeights::PhaseRow row;
    row.phase = phase;
    row.ilvl = ilvl;
    row.w[STR] = 1.0f;
    row.w[HIT] = hit;
    row.w[ARP] = arp;
    row.measured = (1u << STR) | (1u << HIT) | (1u << ARP) | extraMeasured;
    return row;
}

std::vector<SimWeights::PhaseRow> Rows()
{
    return {Row(1, 200.0f, 1.0f, 0.8f), Row(2, 219.0f, 1.2f, 1.0f), Row(3, 232.0f, 1.4f, 1.3f, 1u << AGI),
            Row(5, 258.0f, 1.8f, 1.5f)};
}

void StatNamesMatchTheTable()
{
    static_assert(SimWeights::StatIndex("AGILITY") == 0);
    static_assert(SimWeights::StatIndex("BONUS") == 25);
    static_assert(SimWeights::StatIndex("agility") == -1);
    static_assert(SimWeights::StatIndex("STATS_TYPE_HIT") == -1);
}

void InterpolatesBetweenPhases()
{
    Weights out{};
    uint32_t measured = 0;
    SimWeights::Blend const b = SimWeights::Interpolate(Rows(), 225.5f, out, measured);

    CHECK(b.lo == 2 && b.hi == 3);
    CHECK(Near(b.t, 0.5f));
    CHECK(Near(out[HIT], 1.3f));
    CHECK(Near(out[ARP], 1.15f));
    CHECK(Near(out[STR], 1.0f));
    // agility only in the P3 row, so it doesn't count as measured between P2 and P3
    CHECK(!(measured & (1u << AGI)));
    CHECK(measured & (1u << HIT));
}

void ClampsBelowTheFirstPhase()
{
    Weights out{};
    uint32_t measured = 0;
    SimWeights::Blend b = SimWeights::Interpolate(Rows(), 150.0f, out, measured);

    CHECK(b.lo == 1 && b.hi == 1);
    CHECK(Near(out[HIT], 1.0f));

    b = SimWeights::Interpolate(Rows(), 200.0f, out, measured);
    CHECK(b.lo == 1 && b.hi == 1);
    CHECK(Near(out[HIT], 1.0f));
}

void ClampsAboveTheLastPhase()
{
    Weights out{};
    uint32_t measured = 0;
    SimWeights::Blend const b = SimWeights::Interpolate(Rows(), 280.0f, out, measured);

    CHECK(b.lo == 5 && b.hi == 5);
    CHECK(Near(out[HIT], 1.8f));
    CHECK(Near(out[ARP], 1.5f));
}

void HitsARowExactly()
{
    Weights out{};
    uint32_t measured = 0;
    SimWeights::Blend const b = SimWeights::Interpolate(Rows(), 232.0f, out, measured);

    CHECK(b.lo == 3 && b.hi == 5);
    CHECK(Near(b.t, 0.0f));
    CHECK(Near(out[HIT], 1.4f));
}

void SingleRowSpec()
{
    std::vector<SimWeights::PhaseRow> const rows = {Row(4, 245.0f, 1.5f, 1.2f)};
    Weights out{};
    uint32_t measured = 0;

    SimWeights::Blend b = SimWeights::Interpolate(rows, 200.0f, out, measured);
    CHECK(b.lo == 4 && b.hi == 4 && Near(out[HIT], 1.5f));
    b = SimWeights::Interpolate(rows, 300.0f, out, measured);
    CHECK(b.lo == 4 && b.hi == 4 && Near(out[HIT], 1.5f));
}

void MergeScalesByTheHandAnchor()
{
    Weights sim{};
    sim[STR] = 1.0f;
    sim[HIT] = 1.9f;
    uint32_t const measured = (1u << STR) | (1u << HIT);

    float hand[SimWeights::STAT_COUNT] = {};
    hand[STR] = 2.5f;
    hand[HIT] = 2.3f;
    hand[STA] = 0.1f;

    SimWeights::Merge(sim, measured, STR, hand);

    CHECK(Near(hand[STR], 2.5f));
    CHECK(Near(hand[HIT], 4.75f));
    CHECK(Near(hand[STA], 0.1f));
}

void MergeKeepsNegativeWeightsTheSimFindsWorthless()
{
    Weights sim{};
    sim[STR] = 1.0f;
    sim[INT] = 0.01f;
    sim[SP] = 0.3f;
    uint32_t const measured = (1u << STR) | (1u << INT) | (1u << SP);

    float hand[SimWeights::STAT_COUNT] = {};
    hand[STR] = 2.5f;
    hand[INT] = -2.0f;
    hand[SP] = -2.0f;

    SimWeights::Merge(sim, measured, STR, hand);

    CHECK(Near(hand[INT], -2.0f));
    // worth something to the sim, so the repel goes
    CHECK(Near(hand[SP], 0.75f));
}

void MergeWithoutAPositiveHandAnchor()
{
    Weights sim{};
    sim[SP] = 1.0f;
    sim[HIT] = 0.8f;
    uint32_t const measured = (1u << SP) | (1u << HIT);

    float hand[SimWeights::STAT_COUNT] = {};
    hand[SP] = 0.0f;
    hand[HIT] = 1.1f;

    SimWeights::Merge(sim, measured, SP, hand);

    CHECK(Near(hand[SP], 1.0f));
    CHECK(Near(hand[HIT], 0.8f));

    hand[SP] = -1.0f;
    SimWeights::Merge(sim, measured, SP, hand);
    CHECK(Near(hand[SP], 1.0f));
}

}  // namespace

int main()
{
    StatNamesMatchTheTable();
    InterpolatesBetweenPhases();
    ClampsBelowTheFirstPhase();
    ClampsAboveTheLastPhase();
    HitsARowExactly();
    SingleRowSpec();
    MergeScalesByTheHandAnchor();
    MergeKeepsNegativeWeightsTheSimFindsWorthless();
    MergeWithoutAPositiveHandAnchor();
    std::puts("sim_weights_test: ok");
    return 0;
}
