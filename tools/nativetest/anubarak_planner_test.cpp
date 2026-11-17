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

// The first submerge of a 25H pull: three paladins at the gather, one Hand that wouldn't be back for the third
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
        w.hands.push_back({id, gather, false, id <= 2});

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

// Re-planned every 0.3 s, the kiter walking the plan and the spike chasing it. The plan's paladin walks to
// the catch as the escort does, and its Hand lands on time.
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
            for (auto it = w.hands.begin(); it != w.hands.end(); ++it)
                if (it->id == plan.outcome.handId)
                {
                    w.spareHands -= it->spare ? 1 : 0;
                    w.hands.erase(it);
                    break;
                }
        }

        for (HandOffer& offer : w.hands)
        {
            float const d = AnubarakPursuit::Dist(offer.pos, plan.outcome.catchPoint);
            if (offer.id == plan.outcome.handId && plan.outcome.handS >= 0.0f && d > 10.0f)
                offer.pos = AnubarakKitePlanner::Detail::Lerp(offer.pos, plan.outcome.catchPoint,
                                                              std::min(1.0f, r.paladinSpeed * 0.3f / d));
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

// 2026-10-31 BH, BI and BJ at 1:22: each first kiter spent a kite patch in 4.6-8.7 s while running the ring.
// BJ's only raid-free way needs a Hand on a catch by the base speed spike, which never lands.
void FirstKitesEndLate()
{
    struct Start
    {
        Point kiter;
        Point spike;
        Point gather;
        std::vector<Patch> patches;
        bool raidFree;
    };

    std::vector<Start> const starts = {
        {{774.2f, 140.7f}, {763.5f, 177.7f}, {775.1f, 136.6f},
         {MakePatch(787.5f, 120.7f, true, 1), MakePatch(796.6f, 140.6f, true, 2), MakePatch(748.1f, 155.3f, false, 3),
          MakePatch(747.0f, 113.2f, false, 4)},
         true},
        {{764.4f, 140.8f}, {736.9f, 168.5f}, {767.8f, 141.0f},
         {MakePatch(775.8f, 120.2f, true, 1), MakePatch(789.7f, 142.5f, true, 2), MakePatch(752.8f, 131.8f, false, 3),
          MakePatch(721.2f, 115.7f, false, 4)},
         true},
        {{775.5f, 134.1f}, {762.6f, 170.9f}, {776.2f, 136.5f},
         {MakePatch(785.6f, 116.2f, true, 1), MakePatch(798.7f, 136.5f, true, 2), MakePatch(761.5f, 150.2f, false, 3),
          MakePatch(748.1f, 114.0f, false, 4)},
         false},
    };

    for (Start const& start : starts)
    {
        KiteWorld const w = Heroic(start.kiter, start.spike, start.gather, start.patches);
        Plan const plan = PlanKite(w, nullptr, 0.0f, 0.0f);
        CHECK(plan.valid);
        CHECK(plan.outcome.tankS < 0.0f && plan.outcome.kiteS < 0.0f);
        CHECK(!start.raidFree || plan.outcome.raidS < 0.0f);
        CHECK(plan.outcome.ending == Ending::Patch || plan.outcome.ending == Ending::PatchHand);
        CHECK(plan.outcome.endS >= 12.0f);

        Played const played = Play(w);
        CHECK(!played.caught && (!start.raidFree || !played.raidPass));
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
    AnubarakKitePlanner::Detail::Walk(in.track, AnubarakKitePlanner::Detail::Ticks(w, r), w.pace, w, r,
                                      AnubarakKitePlanner::Detail::LegStep{&legs, 0});

    KiteWorld normal = w;
    normal.slowFactor = 0.7f;
    normal.heroic = false;
    Candidate normalIn = in;
    normalIn.track.resize(1);
    AnubarakKitePlanner::Detail::Walk(normalIn.track, AnubarakKitePlanner::Detail::Ticks(normal, r), normal.pace,
                                      normal, r, AnubarakKitePlanner::Detail::LegStep{&legs, 0});
    Outcome const normalOut = AnubarakKitePlanner::Judge(normalIn, normal, r);
    CHECK(normalOut.ending == Ending::Patch);
    CHECK(!normalOut.caught);

    Outcome const heroicOut = AnubarakKitePlanner::Judge(in, w, r);
    CHECK(heroicOut.caught);
}

// A top speed spike 25 yd off and no patch: only Nitro outlasts it, fired once it is within 15 yd
void BoostFiresWhenTheSpikeCloses()
{
    Rules const r;
    KiteWorld w = Heroic({745.0f, 135.0f}, {745.0f, 160.0f}, {700.0f, 100.0f}, {});
    w.gather = false;
    w.hands.clear();
    w.spareHands = 0;
    w.lifeS = 9.0f;
    w.spike.step = 2;
    w.spike.speed = AnubarakPursuit::SPEEDS[2];
    Plan const plain = PlanKite(w, nullptr, 0.0f, 0.0f, r);
    CHECK(plain.valid && plain.outcome.caught);
    CHECK(plain.candidate.boostTick < 0);

    w.readyBoostSpeed = 17.5f;
    w.readyBoostS = 5.0f;
    Plan const boosted = PlanKite(w, nullptr, 0.0f, 0.0f, r);
    CHECK(boosted.valid && !boosted.outcome.caught);
    CHECK(boosted.outcome.endS > plain.outcome.endS);
    int const tick = boosted.candidate.boostTick;
    CHECK(tick > 0 && boosted.outcome.boostS >= 0.0f);

    AnubarakPursuit::Spike spike = w.spike;
    for (int i = 0; i < tick - 1; ++i)
        AnubarakPursuit::ChaseTick(spike, boosted.candidate.track[static_cast<size_t>(i)],
                                   static_cast<float>(i) * AnubarakPursuit::TICK_S);

    CHECK(AnubarakPursuit::Dist(spike.pos, boosted.candidate.track[static_cast<size_t>(tick - 1)]) > r.boostRange);
    AnubarakPursuit::ChaseTick(spike, boosted.candidate.track[static_cast<size_t>(tick - 1)],
                               static_cast<float>(tick - 1) * AnubarakPursuit::TICK_S);
    CHECK(AnubarakPursuit::Dist(spike.pos, boosted.candidate.track[static_cast<size_t>(tick)]) <= r.boostRange);
}

// A base speed spike 9 yd off is no faster than the kiter: the boost stays
void NoBoostOnASlowSpike()
{
    KiteWorld w = Heroic({745.0f, 135.0f}, {745.0f, 144.0f}, {700.0f, 100.0f}, {MakePatch(747.0f, 113.2f, false, 3)});
    w.gather = false;
    w.lifeS = 13.0f;
    w.readyBoostSpeed = 17.5f;
    w.readyBoostS = 5.0f;
    Plan const plan = PlanKite(w, nullptr, 0.0f, 0.0f);
    CHECK(plan.valid);
    CHECK(plan.candidate.boostTick < 0 && plan.outcome.boostS < 0.0f);
}

// Top speed spike, patch only reachable under a Hand: Nitro goes first and the Hand lands later
void BoostBeforeAHand()
{
    KiteWorld w = Heroic({745.0f, 160.0f}, {757.0f, 160.0f}, {765.0f, 135.0f}, {MakePatch(696.0f, 160.0f, false, 7)});
    w.spike.step = 2;
    w.spike.speed = AnubarakPursuit::SPEEDS[2];
    w.lifeS = 45.0f;
    w.patchBudget = 3;
    w.hands.resize(1);
    w.hands[0].pos = {735.0f, 150.0f};
    w.spareHands = 1;
    Plan const plain = PlanKite(w, nullptr, 0.0f, 0.0f);
    CHECK(plain.valid && plain.outcome.ending == Ending::PatchHand);

    w.readyBoostSpeed = 17.5f;
    w.readyBoostS = 5.0f;
    Plan const boosted = PlanKite(w, nullptr, 0.0f, 0.0f);
    CHECK(boosted.valid && boosted.outcome.ending == Ending::PatchHand);
    CHECK(boosted.outcome.boostS >= 0.0f && boosted.outcome.boostS < boosted.outcome.handS);
    CHECK(boosted.outcome.endS >= plain.outcome.endS + 1.0f);
}

// Dash costs nothing: taken when it reaches the same patch a second or more later
void DashWhenItEndsLater()
{
    KiteWorld w = Heroic({745.0f, 160.0f}, {745.0f, 180.0f}, {765.0f, 135.0f}, {MakePatch(745.0f, 135.0f, false, 7)});
    w.spike.step = 1;
    w.spike.speed = AnubarakPursuit::SPEEDS[1];
    w.spike.nextStepS = 5.0f;
    w.lifeS = 12.0f;
    w.hands.clear();
    w.spareHands = 0;
    Plan const plain = PlanKite(w, nullptr, 0.0f, 0.0f);
    CHECK(plain.valid && plain.outcome.ending == Ending::Patch);

    w.readyBoostSpeed = 11.9f;
    w.readyBoostS = 15.0f;
    Plan const dashed = PlanKite(w, nullptr, 0.0f, 0.0f);
    CHECK(dashed.valid && dashed.outcome.ending == Ending::Patch && dashed.outcome.boostS >= 0.0f);
    CHECK(dashed.outcome.endS >= plain.outcome.endS + 1.0f);
}

// Spike despawns before it can catch, boosted or not: the boost waits for the next submerge
void BoostKeptOnATie()
{
    KiteWorld w = Heroic({745.0f, 160.0f}, {745.0f, 146.0f}, {700.0f, 100.0f}, {});
    w.gather = false;
    w.hands.clear();
    w.spareHands = 0;
    w.spike.step = 2;
    w.spike.speed = AnubarakPursuit::SPEEDS[2];
    w.lifeS = 1.2f;
    w.readyBoostSpeed = 17.5f;
    w.readyBoostS = 5.0f;
    Plan const plan = PlanKite(w, nullptr, 0.0f, 0.0f);
    CHECK(plan.valid && !plan.outcome.caught);
    CHECK(plan.candidate.boostTick < 0 && plan.outcome.boostS < 0.0f);

    // a wider margin doesn't buy the boost, only a later end does
    Rules const r;
    Outcome kept;
    kept.endS = 1.2f;
    kept.margin = 4.0f;
    Outcome boosted = kept;
    boosted.boostS = 0.0f;
    boosted.margin = 12.0f;
    CHECK(AnubarakKitePlanner::Compare(kept, boosted, r) < 0);
    boosted.endS = 2.3f;
    CHECK(AnubarakKitePlanner::Compare(boosted, kept, r) < 0);
}

// An immunity hands the spike on at its speed: mid-life it loses to a patch, in the last seconds it is cheap
void OwnCostsByLifeLeft()
{
    Rules const r;
    KiteWorld w = Heroic({775.0f, 135.0f}, {745.0f, 170.0f}, {700.0f, 100.0f},
                         {MakePatch(747.0f, 113.2f, false, 3), MakePatch(720.0f, 150.0f, false, 4)});
    w.gather = false;
    w.lifeS = 40.0f;
    w.hands.clear();
    w.spareHands = 0;
    w.own = true;
    Plan const plan = PlanKite(w, nullptr, 0.0f, 0.0f, r);
    CHECK(plan.valid && !plan.outcome.caught);
    CHECK(plan.outcome.ending == Ending::Patch);

    KiteWorld run = Heroic({760.0f, 135.0f}, {775.0f, 135.0f}, {700.0f, 100.0f}, {});
    run.gather = false;
    run.own = true;
    run.hands.clear();
    run.spike.step = 2;
    run.spike.speed = AnubarakPursuit::SPEEDS[2];
    Candidate own;
    own.own = true;
    own.ending = Ending::Own;
    own.track.push_back(run.kiter);
    AnubarakKitePlanner::Detail::Walk(own.track, AnubarakKitePlanner::Detail::Ticks(run, r), run.pace, run, r,
                                      AnubarakKitePlanner::Detail::ArcStep{{-1.0f, 0.0f}, 0.0f, 0.0f, &r});
    run.lifeS = 40.0f;
    Outcome const mid = AnubarakKitePlanner::Judge(own, run, r);
    CHECK(mid.ending == Ending::Own && mid.endS < 3.0f);
    CHECK(std::fabs(mid.cost - r.costOwnMid) < 0.01f);

    run.lifeS = mid.endS + 4.0f;
    Outcome const late = AnubarakKitePlanner::Judge(own, run, r);
    CHECK(late.ending == Ending::Own);
    CHECK(std::fabs(late.cost - r.costOwn) < 0.01f);
}

// The spike's leg after the kite heads for the gather: a patch on it is the kite's hazard, and after an
// immunity reaching the gather is a raid pass at once
void NextLegPassesCount()
{
    Rules const r;
    // The immunity at the catch, a tank patch on the spike's way to the gather
    KiteWorld w = Heroic({760.0f, 135.0f}, {775.0f, 135.0f}, {800.0f, 135.0f}, {});
    w.own = true;
    w.hands.clear();
    w.spike.step = 2;
    w.spike.speed = AnubarakPursuit::SPEEDS[2];
    Candidate own;
    own.own = true;
    own.ending = Ending::Own;
    own.track.push_back(w.kiter);
    AnubarakKitePlanner::Detail::Walk(own.track, AnubarakKitePlanner::Detail::Ticks(w, r), w.pace, w, r,
                                      AnubarakKitePlanner::Detail::ArcStep{{-1.0f, 0.0f}, 0.0f, 0.0f, &r});
    Outcome const clear = AnubarakKitePlanner::Judge(own, w, r);
    CHECK(clear.ending == Ending::Own && clear.tankS < 0.0f && clear.kiteS < 0.0f);
    CHECK(clear.raidS == 0.0f);

    w.patches.push_back(MakePatch(790.0f, 145.0f, true, 1));
    Outcome const tank = AnubarakKitePlanner::Judge(own, w, r);
    CHECK(tank.ending == Ending::Own);
    CHECK(tank.tankS >= 0.0f && std::fabs(tank.tankS - tank.endS) < 0.01f);

    // A planned patch end, the spike restarting toward the gather past another kite patch
    KiteWorld patch = Heroic({735.0f, 135.0f}, {735.0f, 165.0f}, {790.0f, 135.0f},
                             {MakePatch(720.0f, 135.0f, false, 1)});
    patch.slowFactor = 1.0f;
    patch.hands.clear();
    patch.spike.step = 2;
    patch.spike.speed = AnubarakPursuit::SPEEDS[2];
    Candidate in;
    in.patch = 0;
    in.turnTick = 0;
    in.ending = Ending::Patch;
    in.track.push_back(patch.kiter);
    std::vector<Point> const legs = {{720.0f, 135.0f}};
    AnubarakKitePlanner::Detail::Walk(in.track, AnubarakKitePlanner::Detail::Ticks(patch, r), patch.pace, patch, r,
                                      AnubarakKitePlanner::Detail::LegStep{&legs, 0});
    Outcome const alone = AnubarakKitePlanner::Judge(in, patch, r);
    CHECK(alone.ending == Ending::Patch && Clean(alone));

    patch.patches.push_back(MakePatch(760.0f, 138.0f, false, 2));
    Outcome const kite = AnubarakKitePlanner::Judge(in, patch, r);
    CHECK(kite.ending == Ending::Patch);
    CHECK(kite.kiteS >= 0.0f && std::fabs(kite.kiteS - kite.endS) < 0.01f);
}

// The kiter walking straight on along dir to the end of the plan
Candidate Straight(KiteWorld const& w, Point dir, Rules const& r)
{
    Candidate c;
    c.track.push_back(w.kiter);
    AnubarakKitePlanner::Detail::Walk(c.track, AnubarakKitePlanner::Detail::Ticks(w, r), w.pace, w, r,
                                      AnubarakKitePlanner::Detail::ArcStep{dir, 0.0f, 0.0f, &r});
    return c;
}

// The kiter walking straight onto patch p and standing there
Candidate Into(KiteWorld const& w, int p, Rules const& r)
{
    Candidate c;
    c.patch = p;
    c.turnTick = 0;
    c.ending = Ending::Patch;
    c.track.push_back(w.kiter);
    std::vector<Point> const legs = {w.patches[static_cast<size_t>(p)].pos};
    AnubarakKitePlanner::Detail::Walk(c.track, AnubarakKitePlanner::Detail::Ticks(w, r), w.pace, w, r,
                                      AnubarakKitePlanner::Detail::LegStep{&legs, 0});
    return c;
}

// 2026-11-01 BO at 1:22: the mage at the gather blocked a base speed spike 26 yd off before it got there, though
// the spike walks on into the gather. A kite patch late in the life wins.
void NoImmunityOnASlowSpike()
{
    KiteWorld w = Heroic({777.55f, 134.34f}, {774.61f, 108.63f}, {774.96f, 134.17f},
                         {MakePatch(795.73f, 119.19f, true, 1), MakePatch(792.11f, 148.68f, true, 2),
                          MakePatch(769.09f, 166.66f, false, 3), MakePatch(730.44f, 111.3f, false, 4)});
    w.lifeS = 57.3f;
    w.spike.nextStepS = 6.9f;
    w.own = true;
    Plan const plan = PlanKite(w, nullptr, 0.0f, 0.0f);
    CHECK(plan.valid);
    CHECK(plan.outcome.ownS < 0.0f);
    CHECK(plan.outcome.ending == Ending::Patch || plan.outcome.ending == Ending::PatchHand);
    CHECK(plan.outcome.endS >= 12.0f);
}

// Walked into, a spike the kiter outruns is a plain catch: a Hand or immunity only on one faster than the kiter
void OnlyAFastSpikeTakesAHandOrImmunity()
{
    Rules const r;
    KiteWorld w = Heroic({745.0f, 135.0f}, {745.0f, 150.0f}, {745.0f, 135.0f}, {});
    w.gather = false;
    w.own = true;
    Candidate own = Straight(w, {0.0f, 1.0f}, r);
    own.own = true;
    own.ending = Ending::Own;
    Candidate hand = own;
    hand.own = false;
    hand.hand = true;
    hand.ending = Ending::Hand;

    Outcome const slowOwn = AnubarakKitePlanner::Judge(own, w, r);
    Outcome const slowHand = AnubarakKitePlanner::Judge(hand, w, r);
    CHECK(slowOwn.caught && slowOwn.ownS < 0.0f && slowOwn.ending == Ending::None);
    CHECK(slowHand.caught && slowHand.handS < 0.0f && slowHand.ending == Ending::None);

    w.spike.step = 2;
    w.spike.speed = AnubarakPursuit::SPEEDS[2];
    CHECK(AnubarakKitePlanner::Judge(own, w, r).ending == Ending::Own);
    CHECK(AnubarakKitePlanner::Judge(hand, w, r).ending == Ending::Hand);
}

// 2026-11-01 BP at 2:04: a side tank walked into a base speed spike, the catch reading raid-free though the
// spike walks on into the gather. Running off passes the raid later, so it wins.
void NoWalkIntoASlowSpike()
{
    KiteWorld w = Heroic({770.0f, 130.0f}, {770.0f, 157.0f}, {768.0f, 130.0f},
                         {MakePatch(782.32f, 114.76f, true, 1), MakePatch(795.69f, 140.62f, true, 2)});
    w.lifeS = 19.0f;
    w.hands.clear();
    w.spareHands = 0;
    Plan const plan = PlanKite(w, nullptr, 0.0f, 0.0f);
    CHECK(plan.valid);
    CHECK(plan.outcome.endS >= 7.0f);
    CHECK(plan.outcome.raidS > 0.0f);

    Played const played = Play(w);
    CHECK(played.endS >= 7.0f);
}

// An immunity in the spike's last seconds costs least, but its raid pass still counts: it beats a patch passing
// the raid as early, and loses to one that never does
void LateImmunityStillPassesTheRaid()
{
    Rules const r;
    KiteWorld w = Heroic({735.0f, 135.0f}, {735.0f, 165.0f}, {765.0f, 135.0f}, {MakePatch(720.0f, 135.0f, false, 1)});
    w.slowFactor = 1.0f;
    w.hands.clear();
    w.spareHands = 0;
    w.own = true;
    w.lifeS = 7.0f;
    w.spike.step = 2;
    w.spike.speed = AnubarakPursuit::SPEEDS[2];
    Candidate own = Straight(w, {0.0f, -1.0f}, r);
    own.own = true;
    own.ending = Ending::Own;
    Candidate const in = Into(w, 0, r);

    Outcome const ownAway = AnubarakKitePlanner::Judge(own, w, r);
    Outcome const patchAway = AnubarakKitePlanner::Judge(in, w, r);
    CHECK(ownAway.ending == Ending::Own && ownAway.raidS == 0.0f);
    CHECK(std::fabs(ownAway.cost - r.costOwn) < 0.01f);
    CHECK(patchAway.ending == Ending::Patch && Clean(patchAway));
    CHECK(AnubarakKitePlanner::Compare(patchAway, ownAway, r) < 0);

    // The spike starts by the gather and passes it within its first second either way
    w.gatherPos = {742.0f, 150.0f};
    Outcome const ownNear = AnubarakKitePlanner::Judge(own, w, r);
    Outcome const patchNear = AnubarakKitePlanner::Judge(in, w, r);
    CHECK(ownNear.ending == Ending::Own && ownNear.raidS >= 0.0f && ownNear.raidS < r.hazardBandS);
    CHECK(patchNear.ending == Ending::Patch && patchNear.raidS >= 0.0f && patchNear.raidS < r.hazardBandS);
    CHECK(AnubarakKitePlanner::Compare(ownNear, patchNear, r) < 0);
}

// 2026-11-02 BS at 2:11: a druid on Dash turned back through a top speed spike for the kite patch behind it,
// caught next to it and Impaled six times. A Hand from the paladin 15 yd off wins.
void NoCatchNextToThePatch()
{
    KiteWorld w = Heroic({757.1f, 143.9f}, {753.8f, 151.9f}, {779.4f, 127.9f},
                         {MakePatch(797.2f, 139.5f, true, 1), MakePatch(794.8f, 114.0f, true, 2),
                          MakePatch(773.7f, 172.2f, false, 3)});
    w.pace = AnubarakPursuit::Pace(7.0f, 11.9f, 5.46f);
    w.spike.step = AnubarakPursuit::LAST_STEP;
    w.spike.speed = AnubarakPursuit::SPEEDS[AnubarakPursuit::LAST_STEP];
    w.lifeS = 10.86f;
    w.bystanders = {{758.5f, 124.0f}, {745.1f, 135.7f}, {794.2f, 107.1f}};
    w.hands = {{1, {745.1f, 135.7f}, false}, {2, {779.6f, 125.8f}, false}, {3, {775.5f, 126.9f}, false}};
    Plan const plan = PlanKite(w, nullptr, 0.0f, 0.0f);
    CHECK(plan.valid && !plan.outcome.caught);
}

// A top speed spike Impales every 0.4 s: a patch it fails on before the second Impale ends the kite there, one
// it fails on later doesn't, and the spike goes on from the catch into the gather
void PatchEndsACatchWithinOneImpale()
{
    Rules const r;
    auto const world = [](float spikeX)
    {
        KiteWorld w = Heroic({735.0f, 135.0f}, {spikeX, 135.0f}, {790.0f, 135.0f}, {MakePatch(720.0f, 135.0f, false, 1)});
        w.slowFactor = 1.0f;
        w.hands.clear();
        w.spareHands = 0;
        w.lifeS = 20.0f;
        w.spike.step = AnubarakPursuit::LAST_STEP;
        w.spike.speed = AnubarakPursuit::SPEEDS[AnubarakPursuit::LAST_STEP];
        return w;
    };

    // caught 5 yd off the patch, the spike on it 0.2 s later
    KiteWorld const nearW = world(749.75f);
    Outcome const nearOut = AnubarakKitePlanner::Judge(Into(nearW, 0, r), nearW, r);
    CHECK(nearOut.caught && nearOut.catchS >= 0.0f && Clean(nearOut));

    // caught 10 yd off, 0.6 s short of it
    KiteWorld const farW = world(746.6f);
    Outcome const farOut = AnubarakKitePlanner::Judge(Into(farW, 0, r), farW, r);
    CHECK(farOut.caught && farOut.catchS >= 0.0f && farOut.kiteS < 0.0f);
    CHECK(farOut.raidS == 0.0f);
    CHECK(AnubarakKitePlanner::Compare(nearOut, farOut, r) < 0);
}

// A kite patch kept for the third submerge loses to a Hand with an own immunity and to a Hand into the
// emerge, and still beats a catch
void ReservedPatchGoesLast()
{
    Rules const r;
    KiteWorld w = Heroic({735.0f, 135.0f}, {735.0f, 165.0f}, {735.0f, 135.0f}, {MakePatch(720.0f, 135.0f, false, 1)});
    w.gather = false;
    w.slowFactor = 1.0f;
    w.own = true;
    w.lifeS = 20.0f;
    w.spike.step = AnubarakPursuit::LAST_STEP;
    w.spike.speed = AnubarakPursuit::SPEEDS[AnubarakPursuit::LAST_STEP];
    Candidate const in = Into(w, 0, r);
    Candidate const caught = Straight(w, {0.0f, -1.0f}, r);
    Candidate handOwn = caught;
    handOwn.hand = true;
    handOwn.own = true;
    handOwn.ending = Ending::Own;

    Outcome const patch = AnubarakKitePlanner::Judge(in, w, r);
    Outcome const saved = AnubarakKitePlanner::Judge(handOwn, w, r);
    Outcome const lost = AnubarakKitePlanner::Judge(caught, w, r);
    CHECK(patch.ending == Ending::Patch && Clean(patch) && !patch.caught);
    CHECK(saved.ending == Ending::Own && saved.handS >= 0.0f && Clean(saved) && !saved.caught);
    CHECK(lost.caught);
    CHECK(AnubarakKitePlanner::Compare(patch, saved, r) < 0);

    w.reservePatch = true;
    Outcome const reserved = AnubarakKitePlanner::Judge(in, w, r);
    CHECK(std::fabs(reserved.cost - r.costReservedPatch) < 0.01f);
    CHECK(AnubarakKitePlanner::Compare(AnubarakKitePlanner::Judge(handOwn, w, r), reserved, r) < 0);
    CHECK(AnubarakKitePlanner::Compare(reserved, AnubarakKitePlanner::Judge(caught, w, r), r) < 0);

    w.lifeS = 8.0f;
    w.own = false;
    Candidate hand = caught;
    hand.hand = true;
    hand.ending = Ending::Hand;
    Outcome const emerge = AnubarakKitePlanner::Judge(hand, w, r);
    CHECK(emerge.ending == Ending::Hand && !emerge.caught && Clean(emerge));
    CHECK(AnubarakKitePlanner::Compare(emerge, AnubarakKitePlanner::Judge(in, w, r), r) < 0);
}

// A Hand that wouldn't be back for the third submerge prices as the last one
void HandShortOfTheThirdCostsMore()
{
    Rules const r;
    KiteWorld w = Heroic({735.0f, 135.0f}, {735.0f, 165.0f}, {735.0f, 135.0f}, {});
    w.gather = false;
    w.lifeS = 8.0f;
    w.spike.step = AnubarakPursuit::LAST_STEP;
    w.spike.speed = AnubarakPursuit::SPEEDS[AnubarakPursuit::LAST_STEP];
    Candidate hand = Straight(w, {0.0f, -1.0f}, r);
    hand.hand = true;
    hand.ending = Ending::Hand;
    Outcome const spare = AnubarakKitePlanner::Judge(hand, w, r);
    CHECK(spare.ending == Ending::Hand && spare.handId == 1);
    CHECK(std::fabs(spare.cost - r.costHand) < 0.01f);

    for (HandOffer& offer : w.hands)
        offer.spare = false;

    w.spareHands = 0;
    Outcome const last = AnubarakKitePlanner::Judge(hand, w, r);
    CHECK(last.ending == Ending::Hand);
    CHECK(std::fabs(last.cost - r.costLastHand) < 0.01f);
}

// 2026-11-04 CD-CH: kites walked onto a patch with a Hand in reach. A spare Hand that carries the kite on to a
// later patch beats the plain walk; the Hand kept back doesn't, and a second kite with a Hand reaching the
// emerge spends no patch
void SpareHandCarriesTheKiteOn()
{
    KiteWorld w = Heroic({775.0f, 135.0f}, {745.0f, 170.0f}, {700.0f, 100.0f},
                         {MakePatch(747.0f, 113.2f, false, 3), MakePatch(720.0f, 150.0f, false, 4)});
    w.lifeS = 45.0f;
    KiteWorld plain = w;
    plain.hands.clear();
    plain.spareHands = 0;
    Plan const walk = PlanKite(plain, nullptr, 0.0f, 0.0f);
    CHECK(walk.valid && walk.outcome.ending == Ending::Patch && Clean(walk.outcome));
    CHECK(walk.outcome.uncoveredS <= 0.0f);

    Plan const hand = PlanKite(w, nullptr, 0.0f, 0.0f);
    CHECK(hand.valid && hand.outcome.ending == Ending::PatchHand && Clean(hand.outcome));
    CHECK(hand.outcome.endS >= walk.outcome.endS + 5.0f);

    KiteWorld kept = w;
    for (HandOffer& offer : kept.hands)
        offer.spare = false;

    kept.spareHands = 0;
    Plan const keep = PlanKite(kept, nullptr, 0.0f, 0.0f);
    CHECK(keep.valid && keep.outcome.ending == Ending::Patch && keep.outcome.handS < 0.0f);

    KiteWorld second = w;
    second.reservePatch = true;
    second.lifeS = 20.0f;
    Plan const emerge = PlanKite(second, nullptr, 0.0f, 0.0f);
    CHECK(emerge.valid && emerge.outcome.ending == Ending::Hand && !emerge.outcome.caught);
}

// A warlock with its circle at the room centre teleports before the catch and runs on to a later patch. No
// teleport onto a circle out of reach or too little farther off the spike, and level with a Hand the free
// teleport goes first.
void TeleportCarriesTheKiteOn()
{
    Rules const r;
    KiteWorld w = Heroic({775.0f, 135.0f}, {745.0f, 170.0f}, {700.0f, 100.0f},
                         {MakePatch(747.0f, 113.2f, false, 3), MakePatch(720.0f, 150.0f, false, 4)});
    w.lifeS = 45.0f;
    w.hands.clear();
    w.spareHands = 0;
    Plan const walk = PlanKite(w, nullptr, 0.0f, 0.0f, r);
    CHECK(walk.valid && walk.outcome.teleportS < 0.0f);

    w.teleport = true;
    w.circle = {745.0f, 135.0f};
    Plan const jump = PlanKite(w, nullptr, 0.0f, 0.0f, r);
    CHECK(jump.valid && jump.outcome.ending == Ending::Patch && Clean(jump.outcome));
    CHECK(jump.outcome.teleportS >= 0.0f && jump.candidate.teleportTick >= 0);
    CHECK(jump.outcome.endS >= walk.outcome.endS + 1.0f);
    size_t const landed = static_cast<size_t>(jump.candidate.teleportTick) + 1;
    CHECK(AnubarakPursuit::Dist(jump.candidate.track[landed], w.circle) < 0.01f);

    // Held, the plan keeps its jump until the tick it goes
    Candidate const held = AnubarakKitePlanner::HeldCandidate(jump.candidate, 0.0f, w, r);
    CHECK(held.teleportTick == jump.candidate.teleportTick);

    Rules shortReach = r;
    shortReach.teleportRange = 5.0f;
    CHECK(PlanKite(w, nullptr, 0.0f, 0.0f, shortReach).outcome.teleportS < 0.0f);

    Rules bigGain = r;
    bigGain.teleportGain = 60.0f;
    CHECK(PlanKite(w, nullptr, 0.0f, 0.0f, bigGain).outcome.teleportS < 0.0f);

    Outcome teleported;
    teleported.kitePatch = true;
    teleported.ending = Ending::Patch;
    teleported.endS = 20.2f;
    teleported.cost = r.costPatch;
    teleported.teleportS = 10.0f;
    Outcome handed = teleported;
    handed.ending = Ending::PatchHand;
    handed.endS = 20.7f;
    handed.cost = r.costPatch + r.costHand;
    handed.teleportS = -1.0f;
    handed.handS = 12.0f;
    CHECK(AnubarakKitePlanner::Compare(teleported, handed, r) < 0);
}

// After a fail the spike walks on along its heading through the idle: a tank patch past the end of that walk,
// behind the spike's way back to the gather, is the kite's hazard all the same
void DriftOverATankPatch()
{
    Rules const r;
    KiteWorld w = Heroic({735.0f, 135.0f}, {735.0f, 165.0f}, {735.0f, 165.0f}, {MakePatch(720.0f, 135.0f, false, 1)});
    w.slowFactor = 1.0f;
    w.hands.clear();
    w.spike.step = 2;
    w.spike.speed = AnubarakPursuit::SPEEDS[2];
    Candidate const in = Into(w, 0, r);
    Outcome const alone = AnubarakKitePlanner::Judge(in, w, r);
    CHECK(alone.ending == Ending::Patch && Clean(alone));

    AnubarakPursuit::Spike spike = w.spike;
    Point before = spike.pos;
    for (size_t i = 0; i + 1 < in.track.size() && AnubarakPursuit::Dist(spike.pos, w.patches[0].pos) > r.patchReach;
         ++i)
    {
        before = spike.pos;
        AnubarakPursuit::ChaseTick(spike, in.track[i], static_cast<float>(i) * AnubarakPursuit::TICK_S);
    }

    // 10 yd past the walk's end: the old restart at the fail and the leg from the new one both pass it 13+ yd off
    Point const heading = AnubarakKitePlanner::Detail::Toward(before, spike.pos);
    float const past = r.driftReach + 10.0f;
    w.patches.push_back(MakePatch(spike.pos.x + heading.x * past, spike.pos.y + heading.y * past, true, 2));
    Outcome const drift = AnubarakKitePlanner::Judge(in, w, r);
    CHECK(drift.ending == Ending::Patch);
    CHECK(drift.tankS >= 0.0f && std::fabs(drift.tankS - drift.endS) < 0.01f);
}

std::vector<Point> StraightTrack(std::vector<float> const& speeds)
{
    std::vector<Point> track = {{0.0f, 0.0f}};
    for (float speed : speeds)
        track.push_back({track.back().x + speed * AnubarakPursuit::TICK_S, 0.0f});

    return track;
}

// The move point stops 8.4 yd on, 1.2 s at most: a Nitro run at 17.5 yd/s aims 5 ticks on, not 12
void MovePointByYards()
{
    using AnubarakKitePlanner::AheadTick;
    std::vector<Point> const run = StraightTrack(std::vector<float>(40, 7.0f));
    CHECK(AheadTick(run, 0, 12, 8.4f) == 12);
    CHECK(AheadTick(run, 3, 12, 8.4f) == 15);

    std::vector<Point> const nitro = StraightTrack(std::vector<float>(40, 17.5f));
    CHECK(AheadTick(nitro, 0, 12, 8.4f) == 5);

    std::vector<Point> const slowed = StraightTrack(std::vector<float>(40, 1.4f));
    CHECK(AheadTick(slowed, 0, 12, 8.4f) == 12);

    CHECK(AheadTick(run, 35, 12, 8.4f) == 40);
    CHECK(AheadTick(run, 40, 12, 8.4f) == 40);
}

// Four usable patches, Hands, an own immunity, a ready boost and the teleport: the candidates stay bounded
void CandidatesStayBounded()
{
    KiteWorld w = Heroic({775.0f, 135.0f}, {745.0f, 170.0f}, {776.0f, 136.0f},
                         {MakePatch(787.5f, 120.7f, false, 1), MakePatch(796.6f, 140.6f, false, 2),
                          MakePatch(748.1f, 155.3f, false, 3), MakePatch(747.0f, 113.2f, false, 4)});
    w.own = true;
    w.readyBoostSpeed = 17.5f;
    w.readyBoostS = 5.0f;
    w.teleport = true;
    w.circle = {745.0f, 135.0f};
    for (int i = 0; i < 8; ++i)
        w.bystanders.push_back({745.0f + 5.0f * static_cast<float>(i), 100.0f});

    auto const start = std::chrono::steady_clock::now();
    Plan const plan = PlanKite(w, nullptr, 0.0f, 0.0f);
    auto const ms =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count() / 1000.0;
    CHECK(plan.valid);
    CHECK(plan.judged <= 5000);
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
    BoostFiresWhenTheSpikeCloses();
    NoBoostOnASlowSpike();
    BoostBeforeAHand();
    DashWhenItEndsLater();
    BoostKeptOnATie();
    OwnCostsByLifeLeft();
    NextLegPassesCount();
    NoImmunityOnASlowSpike();
    OnlyAFastSpikeTakesAHandOrImmunity();
    NoWalkIntoASlowSpike();
    LateImmunityStillPassesTheRaid();
    NoCatchNextToThePatch();
    PatchEndsACatchWithinOneImpale();
    ReservedPatchGoesLast();
    HandShortOfTheThirdCostsMore();
    SpareHandCarriesTheKiteOn();
    TeleportCarriesTheKiteOn();
    DriftOverATankPatch();
    MovePointByYards();
    CandidatesStayBounded();
    std::printf("anubarak_planner_test: ok\n");
    return 0;
}
