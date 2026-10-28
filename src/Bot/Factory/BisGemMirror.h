/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_BISGEMMIRROR_H
#define PLAYERBOTS_BISGEMMIRROR_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <numeric>
#include <vector>

// std only, so tools/nativetest can build it
namespace BisGemMirror
{
// How many of the sim's gems line up with an item's sockets in order, the added socket last.
// The export leaves empty sockets out, so any other count can't be placed and gives 0.
// addedSocket: the item has a socket from an enchant. Pass true for the sim's own copy, where a
// gem past the template sockets is the only sign of one.
constexpr std::size_t AlignedGems(std::size_t gems, std::size_t templateSockets, bool addedSocket)
{
    if (gems == templateSockets)
        return gems;

    if (gems != templateSockets + 1)
        return 0;

    // without the socket the sim's last gem has nowhere to go
    return addedSocket ? gems : templateSockets;
}

// The gems the sim uses per socket color, color 0 being a socket added by an enchant.
class Tally
{
public:
    // limited: unique-equipped or in a limit category, so only a few copies fit
    void Add(uint8_t color, uint32_t gem, bool limited)
    {
        std::vector<Entry>& entries = _byColor[color];
        for (Entry& entry : entries)
        {
            if (entry.gem == gem)
            {
                ++entry.count;
                return;
            }
        }

        entries.push_back({gem, 1, limited});
    }

    // Limited gems first: on count alone the few copies the sim can place never win a color.
    // Then most used, ties in the order added.
    std::vector<uint32_t> Ranked(uint8_t color) const
    {
        auto const found = _byColor.find(color);
        if (found == _byColor.end())
            return {};

        // Sorts indexes rather than a copy of the entries: g++ 13 at -O3 reports a false
        // -Wfree-nonheap-object on the copy, which fails upstream CI's -Werror build.
        std::vector<Entry> const& entries = found->second;
        std::vector<std::size_t> order(entries.size());
        std::iota(order.begin(), order.end(), std::size_t{0});
        std::sort(order.begin(), order.end(), [&entries](std::size_t a, std::size_t b)
        {
            if (entries[a].limited != entries[b].limited)
                return entries[a].limited;

            if (entries[a].count != entries[b].count)
                return entries[a].count > entries[b].count;

            return a < b;
        });

        std::vector<uint32_t> gems;
        gems.reserve(order.size());
        for (std::size_t i : order)
            gems.push_back(entries[i].gem);

        return gems;
    }

private:
    struct Entry
    {
        uint32_t gem;
        uint32_t count;
        bool limited;
    };

    std::map<uint8_t, std::vector<Entry>> _byColor;
};
}  // namespace BisGemMirror

#endif
