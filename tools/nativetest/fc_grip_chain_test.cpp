/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ToC/Util/ToCFactionChampionsGripChain.h"

#include <cstdio>
#include <cstdlib>
#include <map>
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
using namespace FactionChampionsGripChain;

// Every link present with Death Grip ready unless the map says otherwise
struct Game
{
    std::map<uint64_t, LinkState> links;

    LinkState operator()(uint64_t guid) const
    {
        auto const found = links.find(guid);
        return found != links.end() ? found->second : LinkState{true, 0};
    }
};

void TankDksTakeTheFirstPosts()
{
    std::vector<uint64_t> const order = OrderLinks({{30, false}, {10, false}, {20, true}, {40, true}});
    CHECK((order == std::vector<uint64_t>{20, 40, 10, 30}));
}

void DpsDkOpensWithNoTank()
{
    Chain const chain = Open(OrderLinks({{50, false}, {40, false}}), 1000);
    CHECK(chain.links.size() == 2);
    CHECK(Due(chain, 40, 1000, false));
    CHECK(!Due(chain, 50, 1000, false));
    CHECK(Pending(chain, 50));
    CHECK(PostOf(chain, 50) == 1);
}

void FourthDkWaitsAsSpare()
{
    Chain const chain = Open(OrderLinks({{1, true}, {2, false}, {3, false}, {4, false}}), 0);
    CHECK(chain.links.size() == MAX_LINKS);
    CHECK(PostOf(chain, 3) == 2);
    CHECK(PostOf(chain, 4) == chain.links.size());
    CHECK(!Pending(chain, 4));
}

void EachLinkWaitsForTheLanding()
{
    Game const game;
    Chain chain = Open({1, 2, 3}, 1000);
    CHECK(Advance(chain, 1000, game).empty());
    CHECK(Due(chain, 1, 1000, false));

    // a grip from a link whose turn it isn't changes nothing
    TrackGrip(chain, 2, 1500);
    CHECK(chain.due == 0);

    TrackGrip(chain, 1, 2000);
    CHECK(!Pending(chain, 1));
    CHECK(!Due(chain, 2, 2100, false));
    CHECK(!Due(chain, 2, 2300, true));
    CHECK(Due(chain, 2, 2300, false));
    CHECK(!Due(chain, 3, 2300, false));

    TrackGrip(chain, 2, 2400);
    TrackGrip(chain, 3, 3000);
    CHECK(Closed(chain));
    CHECK(!Due(chain, 3, 9000, false));
}

// the grip starts its own cooldown, so it has to be tracked before the next Advance reads it
void GripTrackedBeforeItsCooldownIsRead()
{
    Game game;
    Chain chain = Open({1, 2}, 1000);
    game.links[1] = {true, WAIT_MS + 30000};
    TrackGrip(chain, 1, 1000);
    CHECK(Advance(chain, 1000, game).empty());
    CHECK(!Due(chain, 2, 1000 + LAND_MS - 1, false));
    CHECK(Due(chain, 2, 1000 + LAND_MS, false));
}

void CooldownAndDeathSkipToTheNext()
{
    Game game;
    game.links[1] = {true, WAIT_MS + 1};
    game.links[2] = {false, 0};
    Chain chain = Open({1, 2, 3}, 0);
    std::vector<Skipped> const skipped = Advance(chain, 100, game);
    CHECK(skipped.size() == 2);
    CHECK(skipped[0].guid == 1 && skipped[0].reason == Skip::Cooldown);
    CHECK(skipped[1].guid == 2 && skipped[1].reason == Skip::Gone);

    // the turn passed with no grip, so the third link grips at once
    CHECK(Due(chain, 3, 100, false));

    // a grip this close to ready still waits for it
    game.links[3] = {true, WAIT_MS};
    CHECK(Advance(chain, 200, game).empty());
}

void TimeoutsCountFromTheTurn()
{
    Game const game;
    Chain chain = Open({1, 2}, 0);
    CHECK(Advance(chain, FIRST_LINK_MS, game).empty());

    std::vector<Skipped> skipped = Advance(chain, FIRST_LINK_MS + 1, game);
    CHECK(skipped.size() == 1 && skipped[0].reason == Skip::Timeout);
    CHECK(Due(chain, 2, FIRST_LINK_MS + 1, false));

    CHECK(Advance(chain, FIRST_LINK_MS + 1 + NEXT_LINK_MS, game).empty());
    skipped = Advance(chain, FIRST_LINK_MS + 2 + NEXT_LINK_MS, game);
    CHECK(skipped.size() == 1 && skipped[0].guid == 2);
    CHECK(Closed(chain));

    // after a grip the next link's timeout starts at the grip
    chain = Open({1, 2}, 0);
    TrackGrip(chain, 1, 12000);
    CHECK(Advance(chain, 12000 + NEXT_LINK_MS, game).empty());
    CHECK(!Closed(chain));
}

void ClockWrapKeepsTheTurn()
{
    Game const game;
    uint32_t const start = 0xFFFFFF00u;
    Chain chain = Open({1, 2}, start);
    TrackGrip(chain, 1, start);
    uint32_t const later = start + 1000u;
    CHECK(Due(chain, 2, later, false));
    CHECK(Advance(chain, later, game).empty());
}

void NoLinksIsClosed()
{
    Chain const chain = Open({}, 0);
    CHECK(Closed(chain));
    CHECK(!Pending(chain, 1));
}
}  // namespace

int main()
{
    TankDksTakeTheFirstPosts();
    DpsDkOpensWithNoTank();
    FourthDkWaitsAsSpare();
    EachLinkWaitsForTheLanding();
    GripTrackedBeforeItsCooldownIsRead();
    CooldownAndDeathSkipToTheNext();
    TimeoutsCountFromTheTurn();
    ClockWrapKeepsTheTurn();
    NoLinksIsClosed();
    std::printf("fc_grip_chain_test: ok\n");
    return 0;
}
