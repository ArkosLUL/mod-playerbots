/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_BISRANKORDER_H
#define PLAYERBOTS_BISRANKORDER_H

#include <cstdint>
#include <utility>
#include <vector>

// std only, so tools/nativetest can build it
namespace BisRankOrder
{
// one item's (phase, rank) pairs, as BisDatasetSubject::ranks holds them
using Listings = std::vector<std::pair<uint8_t, uint8_t>>;

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
