/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ToC/Util/ToCAnubarakKitePlanner.h"

#include <chrono>
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
using AnubarakKitePlanner::Candidate;
using AnubarakKitePlanner::Ending;
using AnubarakKitePlanner::HandOffer;
using AnubarakKitePlanner::KiteWorld;
using AnubarakKitePlanner::Outcome;
using AnubarakKitePlanner::Patch;
using AnubarakKitePlanner::Plan;
using AnubarakKitePlanner::PlanKite;
using AnubarakKitePlanner::Rules;
using AnubarakPursuit::Point;

Patch MakePatch(float x, float y, bool tank, uint64_t id)
{
    Patch patch;
    patch.pos = {x, y};
    patch.tank = tank;
    patch.usable = !tank;
    patch.id = id;
    return patch;
}

// The first submerge of a 25H pull: three paladins at the gather, one Hand kept for the next submerge
KiteWorld Heroic(Point kiter, Point spike, Point gather, std::vector<Patch> const& patches)
{
    KiteWorld w;
    w.kiter = kiter;
    w.pace = AnubarakPursuit::Pace(7.0f);
    w.spike.pos = spike;
    w.lifeS = 59.5f;
    w.patches = patches;
    w.gather = true;
    w.gatherPos = gather;
    w.slowFactor = 0.2f;
    for (uint64_t id = 1; id <= 3; ++id)
        w.hands.push_back({id, gather, false});

    w.spareHands = 2;
    for (Patch const& patch : patches)
        w.patchBudget += patch.usable ? 1 : 0;

    return w;
}

bool Clean(Outcome const& out) { return out.tankS < 0.0f && out.kiteS < 0.0f && out.raidS < 0.0f; }

struct Played
{
    // The patch the spike failed on, -1 for none
    int patch = -1;
    bool caught = false;
    bool raidPass = false;
    float endS = 0.0f;
};

// Re-planned every 0.3 s, the kiter walking the plan and the spike chasing it; a planned Hand lands on time
Played Play(KiteWorld w, Rules const& r = Rules())
{
    Played out;
    Candidate held;
    bool hasHeld = false;
    float age = 0.0f;
    float handUntil = -1.0f;
    while (w.lifeS > 0.0f)
    {
        Plan const plan = PlanKite(w, hasHeld ? &held : nullptr, age, 0.3f, r);
        CHECK(plan.valid);
        if (plan.outcome.handS >= 0.0f && plan.outcome.handS < 0.3f && handUntil < out.endS)
        {
            handUntil = out.endS + plan.outcome.handS + r.handS;
            w.hands.pop_back();
            w.spareHands = std::max(0, w.spareHands - 1);
        }

        age = plan.kept ? age + 0.3f : 0.3f;
        held = plan.candidate;
        hasHeld = true;
        for (int i = 0; i < 3; ++i)
        {
            AnubarakPursuit::ChaseTick(w.spike, w.kiter, static_cast<float>(i) * AnubarakPursuit::TICK_S);
            w.kiter = plan.candidate.track[static_cast<size_t>(i + 1)];
            out.endS += AnubarakPursuit::TICK_S;
            for (size_t k = 0; k < w.patches.size(); ++k)
                if (AnubarakPursuit::Dist(w.spike.pos, w.patches[k].pos) <= r.patchReach)
                {
                    out.patch = static_cast<int>(k);
                    return out;
                }

            out.raidPass = out.raidPass ||
                           (w.gather && AnubarakPursuit::Dist(w.spike.pos, w.gatherPos) < r.gatherClearance);
            if (AnubarakPursuit::Dist(w.spike.pos, w.kiter) <= r.impaleReach && out.endS > handUntil + 0.05f)
            {
                out.caught = true;
                return out;
            }
        }

        w.spike.nextStepS -= 0.3f;
        w.lifeS -= 0.3f;
        w.handLeftS = std::max(0.0f, handUntil - out.endS);
    }

    return out;
}

// 2026-10-31 BH, BI and BJ at 1:22: each first kiter spent a kite patch in 4.6-8.7 s while running the ring
void FirstKitesEndLate()
{
    struct Start
    {
        Point kiter;
        Point spike;
        Point gather;
        std::vector<Patch> patches;
    };

    std::vector<Start> const starts = {
        {{774.2f, 140.7f}, {763.5f, 177.7f}, {775.1f, 136.6f},
         {MakePatch(787.5f, 120.7f, true, 1), MakePatch(796.6f, 140.6f, true, 2), MakePatch(748.1f, 155.3f, false, 3),
          MakePatch(747.0f, 113.2f, false, 4)}},
        {{764.4f, 140.8f}, {736.9f, 168.5f}, {767.8f, 141.0f},
         {MakePatch(775.8f, 120.2f, true, 1), MakePatch(789.7f, 142.5f, true, 2), MakePatch(752.8f, 131.8f, false, 3),
          MakePatch(721.2f, 115.7f, false, 4)}},
        {{775.5f, 134.1f}, {762.6f, 170.9f}, {776.2f, 136.5f},
         {MakePatch(785.6f, 116.2f, true, 1), MakePatch(798.7f, 136.5f, true, 2), MakePatch(761.5f, 150.2f, false, 3),
          MakePatch(748.1f, 114.0f, false, 4)}},
    };

    for (Start const& start : starts)
    {
        KiteWorld const w = Heroic(start.kiter, start.spike, start.gather, start.patches);
        Plan const plan = PlanKite(w, nullptr, 0.0f, 0.0f);
        CHECK(plan.valid);
        CHECK(Clean(plan.outcome));
        CHECK(plan.outcome.ending == Ending::Patch || plan.outcome.ending == Ending::PatchHand);
        CHECK(plan.outcome.endS >= 12.0f);

        Played const played = Play(w);
        CHECK(!played.caught && !played.raidPass);
        CHECK(played.patch >= 0 && !w.patches[static_cast<size_t>(played.patch)].tank);
        CHECK(played.endS >= 12.0f);
    }
}

// A spike that restarts 11 yd from a tank patch fails on it at its next test unless it is led out first
void RestartByATankPatch()
{
    Rules const r;
    std::vector<Patch> const patches = {MakePatch(760.0f, 135.0f, true, 1)};
    KiteWorld beyond = Heroic({735.0f, 135.0f}, {771.0f, 135.0f}, {700.0f, 100.0f}, patches);
    beyond.gather = false;
    Plan const trapped = PlanKite(beyond, nullptr, 0.0f, 0.0f, r);
    CHECK(trapped.valid);
    CHECK(trapped.outcome.tankS >= 0.0f);

    // Kiter on the far side from the patch: every step pulls the spike out of its reach
    KiteWorld away = beyond;
    away.kiter = {785.0f, 135.0f};
    Plan const clear = PlanKite(away, nullptr, 0.0f, 0.0f, r);
    CHECK(clear.valid);
    CHECK(clear.outcome.tankS < 0.0f);
}

// Re-planned every 300 ms off the rim with two ways round open, the kiter never turns back within 1 s
void NoReversalAtTheRim()
{
    Rules const r;
    KiteWorld w = Heroic({790.0f, 135.0f}, {760.0f, 135.0f}, {700.0f, 100.0f}, {});
    w.gather = false;
    w.hands.clear();
    w.spareHands = 0;

    Candidate held;
    bool hasHeld = false;
    float heldAge = 0.0f;
    float heldElapsed = 0.0f;
    Point lastHeading = {0.0f, 0.0f};
    float sinceTurn = 10.0f;
    for (int step = 0; step < 16; ++step)
    {
        Plan const plan = PlanKite(w, hasHeld ? &held : nullptr, heldAge, heldElapsed, r);
        CHECK(plan.valid);
        Point const heading = AnubarakKitePlanner::Detail::Heading(plan.candidate.track);
        float const turn = AnubarakKitePlanner::Detail::TurnDegrees(lastHeading, heading);
        if (step > 0 && turn > 90.0f)
            CHECK(sinceTurn >= 1.0f - 0.001f);

        sinceTurn = turn > 90.0f ? 0.0f : sinceTurn + 0.3f;
        lastHeading = heading;
        heldAge = plan.kept ? heldAge + 0.3f : 0.3f;
        heldElapsed = 0.3f;
        held = plan.candidate;
        hasHeld = true;

        // 0.3 s along the plan, the spike chasing
        for (int i = 0; i < 3; ++i)
        {
            AnubarakPursuit::ChaseTick(w.spike, w.kiter, static_cast<float>(i) * AnubarakPursuit::TICK_S);
            w.kiter = plan.candidate.track[static_cast<size_t>(i + 1)];
        }

        w.spike.nextStepS -= 0.3f;
        w.lifeS -= 0.3f;
    }
}

// A 57.5 s life: two kite patches and a Hand end the first kite on a patch
void TwoPatchesAndAHand()
{
    KiteWorld w = Heroic({774.2f, 140.7f}, {763.5f, 177.7f}, {775.1f, 136.6f},
                         {MakePatch(787.5f, 120.7f, true, 1), MakePatch(796.6f, 140.6f, true, 2),
                          MakePatch(748.1f, 155.3f, false, 3), MakePatch(747.0f, 113.2f, false, 4)});
    w.lifeS = 57.5f;
    w.hands.resize(1);
    w.spareHands = 1;
    Plan const plan = PlanKite(w, nullptr, 0.0f, 0.0f);
    CHECK(plan.valid);
    CHECK(plan.outcome.ending == Ending::Patch || plan.outcome.ending == Ending::PatchHand);
    CHECK(plan.outcome.uncoveredS <= 0.0f);
}

// No patch: a Hand covers the end of the life, but a long life keeps it, the catch coming either way
void HandsWithoutPatches()
{
    KiteWorld w = Heroic({775.0f, 135.0f}, {745.0f, 165.0f}, {700.0f, 100.0f}, {});
    w.gather = false;
    w.hands.resize(2);
    w.spareHands = 1;
    w.lifeS = 20.0f;
    Plan const late = PlanKite(w, nullptr, 0.0f, 0.0f);
    CHECK(late.valid);
    CHECK(late.outcome.ending == Ending::Hand);
    CHECK(!late.outcome.caught);

    w.lifeS = 57.5f;
    Plan const early = PlanKite(w, nullptr, 0.0f, 0.0f);
    CHECK(early.valid);
    CHECK(early.outcome.ending != Ending::Hand);
}

// One patch, no Hand, an own immunity: the kite ends covered on the patch or the immunity
void OnePatchAndOwn()
{
    KiteWorld w = Heroic({775.0f, 135.0f}, {745.0f, 170.0f}, {700.0f, 100.0f}, {MakePatch(747.0f, 113.2f, false, 3)});
    w.gather = false;
    w.lifeS = 57.5f;
    w.hands.clear();
    w.spareHands = 0;
    w.own = true;
    Plan const plan = PlanKite(w, nullptr, 0.0f, 0.0f);
    CHECK(plan.valid);
    CHECK(!plan.outcome.caught);
    CHECK(plan.outcome.ending == Ending::Patch || plan.outcome.ending == Ending::Own);
    CHECK(Clean(plan.outcome));
}

// From the slow's edge with the top speed spike 18 yd back, straight in makes it on normal's 30% and not on
// heroic's 80%: the kiter has to get within 3 yd of the centre before the spike reaches Impale's reach
void SlowDecidesTheWalkIn()
{
    Rules const r;
    KiteWorld w = Heroic({772.5f, 135.0f}, {754.5f, 135.0f}, {700.0f, 100.0f}, {MakePatch(780.0f, 135.0f, false, 1)});
    w.gather = false;
    w.hands.clear();
    w.spike.step = 2;
    w.spike.speed = AnubarakPursuit::SPEEDS[2];

    Candidate in;
    in.patch = 0;
    in.turnTick = 0;
    in.ending = Ending::Patch;
    in.track.push_back(w.kiter);
    std::vector<Point> const legs = {{780.0f, 135.0f}};
    AnubarakKitePlanner::Detail::Walk(in.track, AnubarakKitePlanner::Detail::Ticks(w, r), w, r,
                                      AnubarakKitePlanner::Detail::LegStep{&legs, 0});

    KiteWorld normal = w;
    normal.slowFactor = 0.7f;
    normal.heroic = false;
    Candidate normalIn = in;
    normalIn.track.resize(1);
    AnubarakKitePlanner::Detail::Walk(normalIn.track, AnubarakKitePlanner::Detail::Ticks(normal, r), normal, r,
                                      AnubarakKitePlanner::Detail::LegStep{&legs, 0});
    Outcome const normalOut = AnubarakKitePlanner::Judge(normalIn, normal, r);
    CHECK(normalOut.ending == Ending::Patch);
    CHECK(!normalOut.caught);

    Outcome const heroicOut = AnubarakKitePlanner::Judge(in, w, r);
    CHECK(heroicOut.caught);
}

// Four usable patches, Hands and an own immunity: the candidates stay bounded
void CandidatesStayBounded()
{
    KiteWorld w = Heroic({775.0f, 135.0f}, {745.0f, 170.0f}, {776.0f, 136.0f},
                         {MakePatch(787.5f, 120.7f, false, 1), MakePatch(796.6f, 140.6f, false, 2),
                          MakePatch(748.1f, 155.3f, false, 3), MakePatch(747.0f, 113.2f, false, 4)});
    w.own = true;
    for (int i = 0; i < 8; ++i)
        w.bystanders.push_back({745.0f + 5.0f * static_cast<float>(i), 100.0f});

    auto const start = std::chrono::steady_clock::now();
    Plan const plan = PlanKite(w, nullptr, 0.0f, 0.0f);
    auto const ms =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count() / 1000.0;
    CHECK(plan.valid);
    CHECK(plan.judged <= 2500);
    std::printf("anubarak planner: %d candidates in %.1f ms\n", plan.judged, ms);
}
}  // namespace

int main()
{
    FirstKitesEndLate();
    RestartByATankPatch();
    NoReversalAtTheRim();
    TwoPatchesAndAHand();
    HandsWithoutPatches();
    OnePatchAndOwn();
    SlowDecidesTheWalkIn();
    CandidatesStayBounded();
    std::printf("anubarak_planner_test: ok\n");
    return 0;
}
