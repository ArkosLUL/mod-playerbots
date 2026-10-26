/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ToC/Util/ToCAnubarakPursuit.h"

#include <cmath>
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
using AnubarakPursuit::KiterPath;
using AnubarakPursuit::MakeKiterPath;
using AnubarakPursuit::Point;
using AnubarakPursuit::Pursuit;
using AnubarakPursuit::Simulate;
using AnubarakPursuit::Spike;

constexpr float IMPALE_REACH = 7.5f;
constexpr Point ROOM_CENTER = {745.0f, 135.0f};

bool Near(float a, float b, float within) { return std::fabs(a - b) <= within; }

Point Heading(float degrees)
{
    float const radians = degrees * 3.14159265f / 180.0f;
    return {std::cos(radians), std::sin(radians)};
}

// A kiter that stands still 100 yd off: the spike's run is just its speed steps added up
void SpeedStepsAddUp()
{
    KiterPath const still = MakeKiterPath({100.0f, 0.0f}, {1.0f, 0.0f}, 0.0f, {0.0f, 0.0f}, 1000.0f, 1.0f);
    Spike spike;
    spike.pos = {0.0f, 0.0f};
    spike.nextStepS = 1.0f;

    // 1 s at 3.5, then 6.125
    Pursuit const early = Simulate(spike, still, 3.0f, {{100.0f, 0.0f}}, IMPALE_REACH);
    CHECK(Near(early.spikeClosest[0], 100.0f - (3.5f + 2.0f * 6.125f), 0.01f));
    CHECK(early.catchS < 0.0f);

    // 7 s at 6.125, then 11.375
    Pursuit const late = Simulate(spike, still, 10.0f, {{100.0f, 0.0f}}, IMPALE_REACH);
    CHECK(Near(late.spikeClosest[0], 100.0f - (3.5f + 7.0f * 6.125f + 2.0f * 11.375f), 0.01f));
}

// Straight away at 7 yd/s from a spike 20 yd back at its top speed: it closes 4.375 yd/s to 7.5 yd
void StraightChaseCatches()
{
    KiterPath const run = MakeKiterPath({0.0f, 0.0f}, {1.0f, 0.0f}, 7.0f, {0.0f, 0.0f}, 1000.0f, 1.0f);
    Spike spike;
    spike.pos = {-20.0f, 0.0f};
    spike.speed = AnubarakPursuit::SPEEDS[AnubarakPursuit::LAST_STEP];
    spike.step = AnubarakPursuit::LAST_STEP;

    Pursuit const chase = Simulate(spike, run, 20.0f, {}, IMPALE_REACH);
    CHECK(Near(chase.catchS, (20.0f - IMPALE_REACH) / (spike.speed - 7.0f), AnubarakPursuit::TICK_S));

    // No faster than the kiter, never
    spike.speed = 7.0f;
    spike.step = 0;
    spike.nextStepS = 100.0f;
    CHECK(Simulate(spike, run, 20.0f, {}, IMPALE_REACH).catchS < 0.0f);
}

// At the ring heading round it the kiter keeps to the ring, the way it was headed
void PathFollowsTheRing()
{
    Point const start = {ROOM_CENTER.x + 40.0f, ROOM_CENTER.y};
    KiterPath const ccw = MakeKiterPath(start, Heading(90.0f), 7.0f, ROOM_CENTER, 40.0f, -1.0f);
    CHECK(ccw.turn > 0.0f);
    for (float t : {1.0f, 3.0f, 6.0f})
        CHECK(Near(AnubarakPursuit::Dist(ccw.At(t), ROOM_CENTER), 40.0f, 0.01f));

    CHECK(ccw.At(6.0f).y > start.y);
    CHECK(MakeKiterPath(start, Heading(270.0f), 7.0f, ROOM_CENTER, 40.0f, 1.0f).turn < 0.0f);

    // Inside the ring it runs straight out first
    KiterPath const out = MakeKiterPath(ROOM_CENTER, Heading(0.0f), 7.0f, ROOM_CENTER, 40.0f, 1.0f);
    CHECK(Near(out.straight, 40.0f, 0.01f));
    CHECK(Near(out.At(2.0f).x, ROOM_CENTER.x + 14.0f, 0.01f));
}

// A 25H pull: the kiter on Nitro Boosts at about 17.5 yd/s by the east rim, the spike at its top speed
// just behind. Round the north rim the spike cuts inside the arc and comes within 9.5 yd of the patch
// 20 yd off the centre, where it failed for real; round the south it stays far off.
void RimCutInByAPatch()
{
    Point const kiter = {781.0f, 142.0f};
    Point const patch = {748.0f, 155.0f};
    Spike spike;
    spike.pos = {777.0f, 126.0f};
    spike.speed = 11.3f;
    spike.step = AnubarakPursuit::LAST_STEP;

    auto const pass = [&](float degrees)
    {
        KiterPath const path = MakeKiterPath(kiter, Heading(degrees), 17.5f, ROOM_CENTER, 40.0f, 1.0f);
        return Simulate(spike, path, 6.0f, {patch}, IMPALE_REACH).spikeClosest[0];
    };

    CHECK(pass(90.0f) < 14.5f);
    CHECK(pass(270.0f) > 14.5f);
}
}  // namespace

int main()
{
    SpeedStepsAddUp();
    StraightChaseCatches();
    PathFollowsTheRing();
    RimCutInByAPatch();
    std::printf("anubarak_pursuit_test: ok\n");
    return 0;
}
