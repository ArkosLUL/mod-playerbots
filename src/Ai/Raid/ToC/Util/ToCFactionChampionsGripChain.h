#ifndef PLAYERBOTS_RAID_TOCFACTIONCHAMPIONSGRIPCHAIN_H
#define PLAYERBOTS_RAID_TOCFACTIONCHAMPIONSGRIPCHAIN_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace FactionChampionsGripChain
{
inline constexpr std::size_t MAX_LINKS = 3;
inline constexpr uint32_t FIRST_LINK_MS = 15000;
inline constexpr uint32_t NEXT_LINK_MS = 10000;
inline constexpr uint32_t WAIT_MS = 3000;
inline constexpr uint32_t LAND_MS = 300;

struct Dk
{
    uint64_t guid = 0;
    bool tank = false;
};

inline std::vector<uint64_t> OrderLinks(std::vector<Dk> dks)
{
    std::sort(dks.begin(), dks.end(),
              [](Dk const& a, Dk const& b) { return a.tank != b.tank ? a.tank : a.guid < b.guid; });

    std::vector<uint64_t> order;
    for (Dk const& dk : dks)
        order.push_back(dk.guid);

    return order;
}

enum class Skip : uint8_t
{
    None,
    Gone,
    Cooldown,
    Timeout,
};

inline char const* SkipName(Skip skip)
{
    switch (skip)
    {
        case Skip::Gone:
            return "gone";
        case Skip::Cooldown:
            return "cooldown";
        case Skip::Timeout:
            return "timeout";
        default:
            return "none";
    }
}

struct LinkState
{
    bool present = false;
    uint32_t gripCooldownMs = 0;
};

struct Chain
{
    std::vector<uint64_t> links;
    std::size_t due = 0;
    uint32_t turnMs = 0;
    uint32_t gripMs = 0;
    bool gripped = false;
};

inline Chain Open(std::vector<uint64_t> links, uint32_t now)
{
    if (links.size() > MAX_LINKS)
        links.resize(MAX_LINKS);

    Chain chain;
    chain.links = std::move(links);
    chain.turnMs = now;
    return chain;
}

inline bool Closed(Chain const& chain) { return chain.due >= chain.links.size(); }

inline std::size_t PostOf(Chain const& chain, uint64_t guid)
{
    auto const found = std::find(chain.links.begin(), chain.links.end(), guid);
    return static_cast<std::size_t>(found - chain.links.begin());
}

struct Skipped
{
    uint64_t guid = 0;
    Skip reason = Skip::None;
};

template <typename StateOf>
std::vector<Skipped> Advance(Chain& chain, uint32_t now, StateOf stateOf)
{
    std::vector<Skipped> skipped;
    while (!Closed(chain))
    {
        uint64_t const guid = chain.links[chain.due];
        LinkState const link = stateOf(guid);
        uint32_t const limit = chain.due ? NEXT_LINK_MS : FIRST_LINK_MS;
        Skip reason = Skip::None;
        if (!link.present)
            reason = Skip::Gone;
        else if (link.gripCooldownMs > WAIT_MS)
            reason = Skip::Cooldown;
        else if (now - chain.turnMs > limit)
            reason = Skip::Timeout;

        if (reason == Skip::None)
            break;

        skipped.push_back({guid, reason});
        ++chain.due;
        chain.turnMs = now;
    }

    return skipped;
}

inline void TrackGrip(Chain& chain, uint64_t guid, uint32_t now)
{
    if (Closed(chain) || chain.links[chain.due] != guid)
        return;

    chain.gripped = true;
    chain.gripMs = now;
    ++chain.due;
    chain.turnMs = now;
}

inline bool Due(Chain const& chain, uint64_t guid, uint32_t now, bool midJump)
{
    return !Closed(chain) && chain.links[chain.due] == guid &&
           (!chain.gripped || (now - chain.gripMs >= LAND_MS && !midJump));
}

inline bool Pending(Chain const& chain, uint64_t guid)
{
    std::size_t const post = PostOf(chain, guid);
    return post < chain.links.size() && post >= chain.due;
}
}

#endif
