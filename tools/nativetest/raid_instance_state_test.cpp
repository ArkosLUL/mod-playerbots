/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RaidInstanceState.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>
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

struct Latches
{
    int value = 0;
};

struct Slots
{
    std::vector<int> taken;
};

void ForCreatesOnceThenReturnsTheSameEntry()
{
    RaidInstanceState<Latches> store;
    Latches& first = store.For(1);
    CHECK(first.value == 0);

    first.value = 7;
    CHECK(&store.For(1) == &first);
    CHECK(store.For(1).value == 7);
}

void FindNeverCreates()
{
    RaidInstanceState<Latches> store;
    CHECK(store.Find(1) == nullptr);
    CHECK(store.Find(1) == nullptr);

    store.For(1).value = 3;
    CHECK(store.Find(1) != nullptr && store.Find(1)->value == 3);
}

void ResetLeavesOtherInstances()
{
    RaidInstanceState<Latches> store;
    store.For(1).value = 1;
    store.For(2).value = 2;

    store.Reset(1);
    CHECK(store.Find(1) == nullptr);
    CHECK(store.Find(2) != nullptr && store.Find(2)->value == 2);
    CHECK(store.For(1).value == 0);
}

void DropErasesFromEveryStore()
{
    RaidInstanceState<Latches> latches;
    RaidInstanceState<Slots> slots;
    latches.For(1).value = 1;
    latches.For(2).value = 2;
    slots.For(1).taken.push_back(1);
    slots.For(2).taken.push_back(2);

    RaidInstanceStateDrop(1);
    CHECK(latches.Find(1) == nullptr);
    CHECK(slots.Find(1) == nullptr);
    CHECK(latches.Find(2) != nullptr && latches.Find(2)->value == 2);
    CHECK(slots.Find(2) != nullptr && slots.Find(2)->taken.size() == 1);
}

// A dead store left in the registry would be a use after free here, which the sanitizer reports.
void DestroyedStoreLeavesTheRegistry()
{
    {
        RaidInstanceState<Latches> gone;
        gone.For(1).value = 1;
    }
    RaidInstanceStateDrop(1);
}

// One thread keeps writing through its reference, the way a bot does during its map's update,
// while other instances insert enough entries to force rehashes.
void ReferenceSurvivesConcurrentInserts()
{
    RaidInstanceState<Latches> store;
    Latches& held = store.For(1);

    std::atomic<bool> start{false};
    std::vector<std::thread> workers;
    for (std::uint32_t worker = 0; worker < 4; ++worker)
    {
        workers.emplace_back(
            [&store, &start, worker]
            {
                while (!start.load())
                    std::this_thread::yield();

                for (std::uint32_t i = 0; i < 5000; ++i)
                    store.For(2 + worker * 5000 + i).value = static_cast<int>(i);
            });
    }

    start.store(true);
    for (int i = 0; i < 20000; ++i)
        ++held.value;

    for (std::thread& worker : workers)
        worker.join();

    CHECK(&store.For(1) == &held);
    CHECK(held.value == 20000);
    CHECK(store.Find(2 + 3 * 5000 + 4999) != nullptr);
}

}  // namespace

int main()
{
    ForCreatesOnceThenReturnsTheSameEntry();
    FindNeverCreates();
    ResetLeavesOtherInstances();
    DropErasesFromEveryStore();
    DestroyedStoreLeavesTheRegistry();
    ReferenceSurvivesConcurrentInserts();
    std::puts("raid_instance_state_test: all passed");
    return 0;
}
