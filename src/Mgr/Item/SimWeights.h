/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_SIMWEIGHTS_H
#define PLAYERBOTS_SIMWEIGHTS_H

// Per-phase stat weights measured by wowsim (playerbots_sim_weights), and how they blend into a bot's
// hand-written weights. Std-only so tools/nativetest can build it.

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace SimWeights
{

// StatsType order, checked against StatsCollector.h in SimWeightsMgr.cpp
constexpr std::size_t STAT_COUNT = 26;

constexpr std::array<std::string_view, STAT_COUNT> STAT_NAMES = {
    "AGILITY", "STRENGTH", "INTELLECT", "SPIRIT", "STAMINA", "HIT", "CRIT", "HASTE", "ARMOR",
    "DEFENSE", "DODGE", "PARRY", "BLOCK_VALUE", "BLOCK_RATING", "RESILIENCE", "HEALTH_REGENERATION",
    "SPELL_POWER", "SPELL_PENETRATION", "HEAL_POWER", "MANA_REGENERATION", "ATTACK_POWER",
    "ARMOR_PENETRATION", "EXPERTISE", "MELEE_DPS", "RANGED_DPS", "BONUS"};

// Smite priests have no talent tab of their own. Mustn't collide with BisSpecTab's sentinels.
constexpr uint8_t TAB_PRIEST_SMITE = 13;

constexpr uint8_t PHASE_MAX = 5;

// Below this (in anchor units) a measured stat counts as worthless, and a negative hand weight on it
// stays: those are there to push off-role items away (spell power on a warrior, say).
constexpr float NEGLIGIBLE = 0.05f;

constexpr int StatIndex(std::string_view name)
{
    for (std::size_t i = 0; i < STAT_COUNT; ++i)
        if (STAT_NAMES[i] == name)
            return static_cast<int>(i);
    return -1;
}

struct PhaseRow
{
    uint8_t phase = 0;
    // average item level of the phase's BiS gear, as Player::GetAverageItemLevelForDF counts it
    float ilvl = 0.0f;
    // per point, anchor stat = 1.0
    std::array<float, STAT_COUNT> w{};
    // bit per stat the sim measured
    uint32_t measured = 0;
};

struct Spec
{
    int anchor = -1;
    // sorted by ilvl
    std::vector<PhaseRow> rows;
};

// Which rows a lookup used: t = 0 is all lo, t = 1 all hi.
struct Blend
{
    uint8_t lo = 0;
    uint8_t hi = 0;
    float t = 0.0f;
};

// Linear between the two rows around ilvl, clamped to the first and last row. Only stats both rows
// measured count as measured. rows must be sorted by ilvl and non-empty.
inline Blend Interpolate(std::vector<PhaseRow> const& rows, float ilvl, std::array<float, STAT_COUNT>& out,
                         uint32_t& measured)
{
    PhaseRow const* lo = &rows.front();
    PhaseRow const* hi = lo;
    float t = 0.0f;

    if (ilvl >= rows.back().ilvl)
        lo = hi = &rows.back();
    else if (ilvl > rows.front().ilvl)
    {
        for (std::size_t i = 1; i < rows.size(); ++i)
        {
            if (ilvl < rows[i].ilvl)
            {
                lo = &rows[i - 1];
                hi = &rows[i];
                t = (ilvl - lo->ilvl) / (hi->ilvl - lo->ilvl);
                break;
            }
        }
    }

    for (std::size_t i = 0; i < STAT_COUNT; ++i)
        out[i] = lo->w[i] + (hi->w[i] - lo->w[i]) * t;
    measured = lo->measured & hi->measured;

    return {lo->phase, hi->phase, t};
}

// Replaces the measured stats in hand (the hand-written weights) with the sim's, scaled so the anchor
// keeps its hand-written value. Unmeasured stats keep theirs, and so does a negative hand weight the
// sim finds worthless.
inline void Merge(std::array<float, STAT_COUNT> const& sim, uint32_t measured, int anchor, float* hand)
{
    float const scale = anchor >= 0 && hand[anchor] > 0.0f ? hand[anchor] : 1.0f;

    for (std::size_t i = 0; i < STAT_COUNT; ++i)
    {
        if (!(measured & (1u << i)))
            continue;

        if (hand[i] < 0.0f && std::fabs(sim[i]) < NEGLIGIBLE)
            continue;

        hand[i] = sim[i] * scale;
    }
}

}  // namespace SimWeights

#endif
