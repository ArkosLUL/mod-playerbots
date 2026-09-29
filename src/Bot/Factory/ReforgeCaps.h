/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_REFORGECAPS_H
#define PLAYERBOTS_REFORGECAPS_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <optional>

// std only, so tools/nativetest can build it
namespace ReforgeCaps
{
enum Rating : uint8_t
{
    HIT_MELEE,
    HIT_RANGED,
    HIT_SPELL,
    EXPERTISE,
    RATING_COUNT
};

// ItemModType ids, checked against the core enum in BisReforge.cpp
constexpr uint32_t MOD_HIT_MELEE = 16;
constexpr uint32_t MOD_HIT_RANGED = 17;
constexpr uint32_t MOD_HIT_SPELL = 18;
constexpr uint32_t MOD_HIT = 31;
constexpr uint32_t MOD_EXPERTISE = 37;

constexpr bool Feeds(uint32_t stat, Rating rating)
{
    switch (rating)
    {
        case HIT_MELEE:
            return stat == MOD_HIT || stat == MOD_HIT_MELEE;
        case HIT_RANGED:
            return stat == MOD_HIT || stat == MOD_HIT_RANGED;
        case HIT_SPELL:
            return stat == MOD_HIT || stat == MOD_HIT_SPELL;
        case EXPERTISE:
            return stat == MOD_EXPERTISE;
        default:
            return false;
    }
}

// from 0: no reforge
struct Reforge
{
    uint32_t from = 0;
    uint32_t to = 0;
    float amount = 0.0f;
};

// rating the reforge adds over the item left unreforged
constexpr float Delta(Reforge const& reforge, Rating rating)
{
    if (!reforge.from)
        return 0.0f;

    return (Feeds(reforge.to, rating) ? reforge.amount : 0.0f) - (Feeds(reforge.from, rating) ? reforge.amount : 0.0f);
}

// Per rating, what the bot can still add before its cap while the item carries its current
// reforge, negative past it. nullopt where the bot plays to no cap.
using Rooms = std::array<std::optional<float>, RATING_COUNT>;

// rating still missing below the caps with the item switched from current to reforge
inline float Shortfall(Rooms const& rooms, Reforge const& reforge, Reforge const& current)
{
    float total = 0.0f;
    for (uint8_t i = 0; i < RATING_COUNT; ++i)
    {
        if (!rooms[i])
            continue;

        Rating const rating = static_cast<Rating>(i);
        total += std::max(0.0f, *rooms[i] - (Delta(reforge, rating) - Delta(current, rating)));
    }

    return total;
}

// The sim's reforge, unless the item's current reforge or none at all leaves the bot less short
// of its caps. Ties go in that order, so a bot that stays capped either way follows the sim.
inline Reforge Pick(Rooms const& rooms, Reforge const& sim, Reforge const& current)
{
    Reforge best = sim;
    float bestShortfall = Shortfall(rooms, sim, current);
    for (Reforge const& candidate : {current, Reforge{}})
    {
        float const shortfall = Shortfall(rooms, candidate, current);
        // float noise from summing ratings must not beat a real tie
        if (shortfall + 0.01f < bestShortfall)
        {
            best = candidate;
            bestShortfall = shortfall;
        }
    }

    return best;
}
}  // namespace ReforgeCaps

#endif
