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

// A 25H pull: he came up past side 1's patch on the line through both patches, so the straight drag to
// the anchor crossed it and he was held there 65 s. Round the first waypoint both legs keep off it.
void DragRoutesRoundAPatch()
{
    Point const boss = {740.3f, 113.1f};
    Point const anchor = {718.6f, 145.3f};
    std::vector<Point> const patches = {{710.8f, 158.4f}, {726.4f, 132.2f}};
    constexpr float KEEPOUT = 8.5f;

    CHECK(AnubarakPursuit::SegmentDist(boss, anchor, patches[1]) < 7.5f);

    std::vector<Point> const detours = AnubarakPursuit::DragDetours(boss, anchor, patches, KEEPOUT + 3.0f, 12.0f, KEEPOUT);
    CHECK(detours.size() == 2);
    Point const first = detours[0];
    CHECK(AnubarakPursuit::Dist(boss, first) <= AnubarakPursuit::Dist(boss, detours[1]));
    CHECK(AnubarakPursuit::SegmentDist(boss, first, patches[1]) >= KEEPOUT);
    CHECK(AnubarakPursuit::SegmentDist(first, anchor, patches[1]) >= KEEPOUT);
    for (Point const& patch : patches)
        CHECK(AnubarakPursuit::Dist(first, patch) >= KEEPOUT);

    // Off the line he needs no waypoint
    CHECK(AnubarakPursuit::DragDetours({700.0f, 120.0f}, anchor, patches, KEEPOUT + 3.0f, 12.0f, KEEPOUT).empty());

    std::vector<Point> const ring = AnubarakPursuit::RingPoints(anchor, 8.0f, boss, patches, KEEPOUT);
    CHECK(!ring.empty());
    for (Point const& point : ring)
    {
        CHECK(Near(AnubarakPursuit::Dist(point, anchor), 8.0f, 0.01f));
        for (Point const& patch : patches)
            CHECK(AnubarakPursuit::Dist(point, patch) >= KEEPOUT);
    }

    CHECK(AnubarakPursuit::Dist(boss, ring.front()) <= AnubarakPursuit::Dist(boss, ring.back()));
}

using AnubarakPursuit::LegPath;
using AnubarakPursuit::PatchMargin;
using AnubarakPursuit::PatchMove;
using AnubarakPursuit::PatchRules;
using AnubarakPursuit::PatchWalk;

constexpr float PATCH_REACH = 11.5f;
constexpr float STAND_OFFSET = 8.5f;
// Heroic Permafrost: 80% slower inside its 7.5 yd
constexpr float HEROIC_SLOW = 0.2f;

PatchRules HeroicRules()
{
    PatchRules rules;
    rules.slowFactor = HEROIC_SLOW;
    return rules;
}

// STAND_OFFSET past the patch on the far side from the spike: the stand the old rule walked to
Point StandPast(Point patch, Point spike)
{
    float const d = AnubarakPursuit::Dist(spike, patch);
    return {patch.x + (patch.x - spike.x) / d * STAND_OFFSET, patch.y + (patch.y - spike.y) / d * STAND_OFFSET};
}

struct KiteOutcome
{
    float turnS = -1.0f;
    // The spike reached the patch's test before Impale's reach
    bool failedOnPatch = false;
};

// The rest of a run path from from seconds in
struct RunOn
{
    KiterPath const* base;
    float from;
    Point start;

    Point At(float t) const { return base->At(from + t); }
};

// The kite played out: the kiter runs its path, re-judged every 0.3 s. newRule turns once the best patch
// walk now fails, or would after running on 1.5 s, and walks that one's legs, slowed in the patch; the old
// one turns only once the spike outran the kiter and Impale came within the walk plus 1 s, and walks to
// the stand past the patch, re-aimed off the spike.
KiteOutcome PlayKite(Spike spike, KiterPath const& run, Point patch, float speed, bool newRule)
{
    PatchRules const rules = HeroicRules();
    KiteOutcome out;
    Point kiter = run.start;
    float runT = 0.0f;
    std::vector<Point> legs;
    LegPath walk;
    float walkT = 0.0f;
    for (int tick = 0; tick < 400; ++tick)
    {
        float const t = static_cast<float>(tick) * AnubarakPursuit::TICK_S;
        if (out.turnS < 0.0f && tick % 3 == 0)
        {
            Spike ahead = spike;
            ahead.nextStepS -= t;
            bool turn = false;
            if (newRule)
            {
                PatchWalk const now = AnubarakPursuit::BestPatchWalk(ahead, kiter, speed, patch, rules, 0.0f);
                turn = now.marginS < 0.0f ||
                       AnubarakPursuit::PatchWalkAfter(ahead, RunOn{&run, runT, kiter}, 1.5f, speed, patch, rules,
                                                       0.0f)
                               .marginS < 0.0f;
                legs = now.legs;
            }
            else if (spike.speed > speed)
            {
                KiterPath const rest = MakeKiterPath(kiter, Heading(90.0f), speed, ROOM_CENTER, 40.0f, 1.0f);
                float const impale = Simulate(ahead, rest, 20.0f, {}, IMPALE_REACH).catchS;
                float const walkS = AnubarakPursuit::Dist(kiter, StandPast(patch, spike.pos)) / speed;
                turn = AnubarakPursuit::Dist(spike.pos, kiter) <= 9.0f || (impale >= 0.0f && impale <= walkS + 1.0f);
            }

            if (turn && newRule)
            {
                walk = LegPath();
                walk.start = kiter;
                walk.legs = legs;
                walk.speed = speed;
                walk.Slow(patch, rules.slowReach, rules.slowFactor, 40.0f);
            }

            if (turn)
                out.turnS = t;
        }

        Point const from = spike.pos;
        AnubarakPursuit::ChaseTick(spike, kiter, t);
        if (AnubarakPursuit::SegmentDist(from, spike.pos, patch) <= PATCH_REACH)
        {
            out.failedOnPatch = true;
            return out;
        }

        if (out.turnS < 0.0f)
        {
            runT += AnubarakPursuit::TICK_S;
            kiter = run.At(runT);
        }
        else if (newRule)
        {
            walkT += AnubarakPursuit::TICK_S;
            kiter = walk.At(walkT);
        }
        else
        {
            LegPath step;
            step.start = kiter;
            step.legs = {StandPast(patch, spike.pos)};
            step.speed = speed;
            kiter = step.At(AnubarakPursuit::TICK_S);
        }

        if (AnubarakPursuit::Dist(spike.pos, kiter) <= IMPALE_REACH)
            return out;
    }

    return out;
}

// A 25H pull: a side tank marked on its own patch at the submerge, the spike 34 yd off at its base speed,
// the submerge's patch allowed. It ran on until the spike was at its top speed and died on the walk back.
// Judged on the spike's whole speed schedule the run turns in time, before the top speed, and the old
// rule's turn comes too late.
void PatchCommitBeatsTheSpeedUp()
{
    Point const kiter = {725.0f, 134.0f};
    Point const patch = {726.4f, 132.2f};
    Spike spike;
    spike.pos = {751.0f, 112.0f};
    constexpr float SPEED = 7.0f;

    for (float degrees : {60.0f, 90.0f, 120.0f})
    {
        KiterPath const run = MakeKiterPath(kiter, Heading(degrees), SPEED, ROOM_CENTER, 40.0f, 1.0f);
        // A run that takes the spike over the patch never needs the turn
        KiteOutcome const judged = PlayKite(spike, run, patch, SPEED, true);
        CHECK(judged.turnS < 2.0f * AnubarakPursuit::STEP_S);
        CHECK(judged.failedOnPatch);
    }

    KiterPath const run = MakeKiterPath(kiter, Heading(90.0f), SPEED, ROOM_CENTER, 40.0f, 1.0f);
    CHECK(!PlayKite(spike, run, patch, SPEED, false).failedOnPatch);
}

// Under Hand of Protection the spike sitting on the kiter can't Impale it, so a walk across the patch
// with it on its heels still lands while the hand lasts
void HandBridgesTheWalk()
{
    Spike spike;
    spike.pos = {-3.0f, 0.0f};
    spike.speed = AnubarakPursuit::SPEEDS[AnubarakPursuit::LAST_STEP];
    spike.step = AnubarakPursuit::LAST_STEP;
    LegPath walk;
    walk.start = {0.0f, 0.0f};
    walk.legs = {{25.0f, 0.0f}};
    Point const patch = {15.0f, 0.0f};

    CHECK(PatchMargin(spike, walk, patch, PATCH_REACH, IMPALE_REACH, 10.0f, 20.0f, 1.0f) > 0.0f);
    CHECK(PatchMargin(spike, walk, patch, PATCH_REACH, IMPALE_REACH, 0.0f, 20.0f, 1.0f) < 0.0f);

    // The walk stands on its last point
    CHECK(Near(walk.At(10.0f).x, 25.0f, 0.01f));
    CHECK(Near(walk.At(1.0f).x, 7.0f, 0.01f));
}

float ArriveS(Spike const& spike, Point kiter, float speed, Point patch, std::vector<Point> const& legs)
{
    LegPath walk;
    walk.start = kiter;
    walk.legs = legs;
    walk.speed = speed;
    walk.Slow(patch, 7.5f, HEROIC_SLOW, 20.0f);
    return Simulate(spike, walk, 20.0f, {patch}, IMPALE_REACH, PATCH_REACH).spikeArriveS[0];
}

// The slow holds the walk to a fifth of its speed inside 7.5 yd, and only there
void SlowedWalk()
{
    LegPath walk;
    walk.start = {20.0f, 0.0f};
    walk.legs = {{0.0f, 0.0f}};
    walk.Slow({0.0f, 0.0f}, 7.5f, HEROIC_SLOW, 20.0f);
    CHECK(Near(walk.At(1.0f).x, 13.0f, 0.01f));
    // 12.5 yd at 7, then 1.4 yd/s
    CHECK(Near(walk.At(12.5f / 7.0f + 1.0f).x, 7.5f - 1.4f, 0.2f));
    CHECK(Near(walk.At(19.0f).x, 0.0f, 0.01f));
}

// A 25H pull: under Hand of Protection with the spike on its heels, the kiter circled its patch until
// the hand ran out. Walked straight in, the spike fails on the patch with the hand still up.
void HandWalksIntoThePatch()
{
    PatchRules const rules = HeroicRules();
    Spike spike;
    spike.pos = {767.0f, 110.0f};
    spike.speed = 11.3f;
    spike.step = AnubarakPursuit::LAST_STEP;
    Point const kiter = {762.0f, 103.0f};
    Point const patch = {742.0f, 117.0f};

    PatchWalk const walk = AnubarakPursuit::BestPatchWalk(spike, kiter, 7.0f, patch, rules, 10.0f);
    CHECK(walk.valid && walk.move == PatchMove::In);
    CHECK(walk.marginS >= 0.0f);
    float const arrive = ArriveS(spike, kiter, 7.0f, patch, walk.legs);
    CHECK(arrive >= 0.0f && arrive < 4.0f);

    // Re-marked at the top speed 64 yd from its patch, 9.2 s of hand left
    spike.pos = {721.0f, 131.0f};
    spike.speed = AnubarakPursuit::SPEEDS[AnubarakPursuit::LAST_STEP];
    CHECK(AnubarakPursuit::BestPatchWalk(spike, {721.0f, 137.0f}, 7.0f, {779.0f, 111.0f}, rules, 9.2f).marginS >= 0.0f);
}

// The spike 20 yd behind the kiter at its top speed, on its line past the patch's edge: walked straight in,
// the slow lets the spike close before it fails; past the edge it fails while the kiter is still clear
void EdgeBeatsTheSlow()
{
    PatchRules const rules = HeroicRules();
    Point const patch = {0.0f, 0.0f};
    Point const kiter = {10.0f, 0.0f};

    // Left keeps the patch on the kiter's left; inside the edge ring there is no edge to pass
    std::vector<Point> left;
    CHECK(AnubarakPursuit::PatchMoveLegs(PatchMove::EdgeLeft, kiter, patch, rules, left));
    CHECK(left.size() == 2 && left[0].y > 0.0f && Near(AnubarakPursuit::Dist(left[0], patch), 8.5f, 0.01f));
    CHECK(Near(AnubarakPursuit::SegmentDist(kiter, left[1], patch), 8.5f, 0.01f));
    std::vector<Point> none;
    CHECK(!AnubarakPursuit::PatchMoveLegs(PatchMove::EdgeRight, {8.0f, 0.0f}, patch, rules, none));

    float const along = AnubarakPursuit::Dist(kiter, left[0]);
    Spike spike;
    spike.pos = {kiter.x + (kiter.x - left[0].x) / along * 20.0f, kiter.y + (kiter.y - left[0].y) / along * 20.0f};
    spike.speed = AnubarakPursuit::SPEEDS[AnubarakPursuit::LAST_STEP];
    spike.step = AnubarakPursuit::LAST_STEP;

    std::vector<Point> in;
    CHECK(AnubarakPursuit::PatchMoveLegs(PatchMove::In, kiter, patch, rules, in));
    CHECK(AnubarakPursuit::PatchWalkMargin(spike, kiter, 7.0f, patch, in, rules, 0.0f) < 0.0f);

    PatchWalk const best = AnubarakPursuit::BestPatchWalk(spike, kiter, 7.0f, patch, rules, 0.0f);
    CHECK(best.valid && best.move == PatchMove::EdgeLeft && best.marginS >= 0.0f);

    // Under Hand of Freedom the walk in is quick enough
    PatchRules free = rules;
    free.slowFactor = 1.0f;
    CHECK(AnubarakPursuit::PatchWalkMargin(spike, kiter, 7.0f, patch, in, free, 0.0f) >= 0.0f);
}

// A 25H pull: the spike between the kiter and both patches at its base speed. Every stand past a patch
// lay beyond the spike, so the kiter ran the ring and died; a walk onto one still beats the catch.
void SpikeBetweenKiterAndPatches()
{
    PatchRules const rules = HeroicRules();
    Spike spike;
    spike.pos = {733.0f, 129.0f};
    bool any = false;
    for (Point const& patch : {Point{742.0f, 117.0f}, Point{715.0f, 122.0f}})
        any = any || AnubarakPursuit::BestPatchWalk(spike, {750.0f, 142.0f}, 7.0f, patch, rules, 0.0f).marginS >= 0.0f;

    CHECK(any);
}

// A 25H pull: the kiter ran south between both tank patches, the spike fresh 35 yd off, then east round
// the ring. Judged over 6 s the heading kept the spike off both; it cut the corner over the south one 9.5 s
// in.
void PassHorizonCoversTheRun()
{
    Spike spike;
    spike.pos = {719.0f, 158.0f};
    Point const south = {744.0f, 118.0f};
    KiterPath const path = MakeKiterPath({748.0f, 138.0f}, Heading(270.0f), 7.0f, ROOM_CENTER, 40.0f, 1.0f);

    Pursuit const brief = Simulate(spike, path, 6.0f, {south}, IMPALE_REACH, PATCH_REACH);
    CHECK(brief.spikeArriveS[0] < 0.0f);
    Pursuit const full = Simulate(spike, path, 12.0f, {south}, IMPALE_REACH, PATCH_REACH);
    CHECK(full.spikeArriveS[0] > 6.0f);
}

// The patch with 12+ s to the emerge, behind any ready Hand; then a Hand, the patch as the last resort,
// the kiter's own immunity
void TerminalOrder()
{
    using AnubarakPursuit::KiteOptions;
    using AnubarakPursuit::KiteTerminal;
    using AnubarakPursuit::PickTerminal;

    // A ret paladin marked early in a submerge, its own Hand and Divine Shield ready
    KiteOptions early;
    early.catchS = 20.0f;
    early.emergeS = 58.0f;
    early.patch = true;
    early.patchReachable = true;
    early.hand = true;
    early.own = true;
    CHECK(PickTerminal(early) == KiteTerminal::PatchHand);

    KiteOptions handless = early;
    handless.hand = false;
    CHECK(PickTerminal(handless) == KiteTerminal::Patch);

    KiteOptions distant = early;
    distant.patchReachable = false;
    CHECK(PickTerminal(distant) == KiteTerminal::PatchHand);

    KiteOptions late = early;
    late.catchS = 50.0f;
    CHECK(PickTerminal(late) == KiteTerminal::Hand);

    KiteOptions outlast = early;
    outlast.catchS = 57.5f;
    CHECK(PickTerminal(outlast) == KiteTerminal::None);

    KiteOptions bare = distant;
    bare.hand = false;
    CHECK(PickTerminal(bare) == KiteTerminal::Own);
    bare.own = false;
    CHECK(PickTerminal(bare) == KiteTerminal::Patch);

    KiteOptions spent = late;
    spent.patch = false;
    spent.patchReachable = false;
    spent.hand = false;
    CHECK(PickTerminal(spent) == KiteTerminal::Own);
    spent.own = false;
    CHECK(PickTerminal(spent) == KiteTerminal::None);
}

// The 2026-10-27 N far grip: the sphere came down at (796,144), 68 yd off side 1's spot. The next link
// stands 25 yd nearer, every post in reach of the landing; a landing in reach of the spot gets the spot.
void RelayPostsCloseOnTheSpot()
{
    using AnubarakPursuit::Dist;
    using AnubarakPursuit::RelayPosts;

    Point const landing = {796.0f, 144.0f};
    Point const spot = {728.45f, 139.85f};
    std::vector<Point> const posts = RelayPosts(landing, spot, 25.0f);
    CHECK(posts.size() == 5);
    CHECK(Near(Dist(posts.front(), spot), Dist(landing, spot) - 25.0f, 0.05f));
    for (Point const& post : posts)
    {
        CHECK(Near(Dist(post, landing), 25.0f, 0.05f));
        CHECK(Dist(post, spot) >= Dist(posts.front(), spot) - 0.05f);
    }

    std::vector<Point> const near = RelayPosts({745.0f, 150.0f}, spot, 25.0f);
    CHECK(near.size() == 1);
    CHECK(Near(near.front().x, spot.x, 0.01f) && Near(near.front().y, spot.y, 0.01f));
}

// The 2026-10-27 N drag that held 56 s: the step-out left the tank 7.6 yd from side 1's patch, on its far
// side from the anchor, so every leg's walk crossed the slow. A step round him, inside his 12 yd chase
// start, clears the walk to the first detour.
void DragPreStepClearsTheStall()
{
    using AnubarakPursuit::Dist;
    using AnubarakPursuit::DragDetours;
    using AnubarakPursuit::DragPreSteps;
    using AnubarakPursuit::SegmentDist;

    constexpr float KEEPOUT = 8.5f;
    constexpr float SLOW = 7.5f;
    Point const boss = {729.8f, 125.2f};
    Point const tank = {724.6f, 129.1f};
    Point const anchor = {709.9f, 144.1f};
    std::vector<Point> const patches = {{717.6f, 134.0f}, {702.1f, 154.2f}};

    auto const clear = [&](Point from, Point to)
    {
        for (Point const& patch : patches)
            if (Dist(to, patch) < KEEPOUT || SegmentDist(from, to, patch) < SLOW)
                return false;

        return true;
    };

    std::vector<Point> const detours = DragDetours(boss, anchor, patches, KEEPOUT + 3.0f, 12.0f, KEEPOUT);
    CHECK(!detours.empty());
    Point const leg = detours.front();
    CHECK(!clear(tank, anchor));
    CHECK(!clear(tank, leg));

    std::vector<Point> const steps = DragPreSteps(boss, tank, leg, patches, {9.0f, 10.0f, 11.0f}, KEEPOUT, SLOW);
    CHECK(!steps.empty());
    CHECK(Dist(steps.front(), boss) <= 11.05f);
    CHECK(clear(tank, steps.front()) && clear(steps.front(), leg));
    for (Point const& step : steps)
        CHECK(Dist(tank, step) >= Dist(tank, steps.front()) - 0.01f);
}
}  // namespace

int main()
{
    SpeedStepsAddUp();
    StraightChaseCatches();
    PathFollowsTheRing();
    RimCutInByAPatch();
    DragRoutesRoundAPatch();
    PatchCommitBeatsTheSpeedUp();
    HandBridgesTheWalk();
    SlowedWalk();
    HandWalksIntoThePatch();
    EdgeBeatsTheSlow();
    SpikeBetweenKiterAndPatches();
    PassHorizonCoversTheRun();
    TerminalOrder();
    RelayPostsCloseOnTheSpot();
    DragPreStepClearsTheStall();
    std::printf("anubarak_pursuit_test: ok\n");
    return 0;
}
