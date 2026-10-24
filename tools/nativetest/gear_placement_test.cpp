/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "GearPlacement.h"

#include <cstdio>
#include <cstdlib>

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
using namespace GearPlacement;

constexpr float BAR = 1.07f;

Change Over(float newScore, float oldScore, float threshold = BAR) { return {newScore, oldScore, threshold}; }

Option Single(Change change)
{
    Option option;
    option.Add(change);
    return option;
}

Option Pair(Change a, Change b)
{
    Option option;
    option.Add(a);
    option.Add(b);
    return option;
}

// main hand 100, off hand 80: a 90 weapon only beats the off hand
void BetterThanTheOffHandGoesThere()
{
    std::vector<Option> options = {
        Single(Over(90, 100)),               // new to main
        Pair(Over(90, 100), Over(100, 80)),  // new to main, old main to off
        Single(Over(90, 80)),                // new to off
        Pair(Over(90, 80), Over(80, 100)),   // new to off, old off to main
    };
    CHECK(Choose(options, false) == 2);
}

void JustUnderTheBarKeeps()
{
    std::vector<Option> options = {Single(Over(85, 80))};
    CHECK(!options[0].Wanted());
    CHECK(Choose(options, false) == -1);

    options[0] = Single(Over(86, 80));
    CHECK(Choose(options, false) == 0);
}

// 110 isn't 7% over the main hand's 105, but the main hand moving over the 60 off hand is
void SwapLayoutQualifies()
{
    std::vector<Option> options = {
        Single(Over(110, 105)),
        Pair(Over(110, 105), Over(105, 60)),
    };
    CHECK(!options[0].Wanted());
    CHECK(Choose(options, false) == 1);
}

// the bar sums per slot, a misfit worn item counting at 1.0
void BarSumsPerSlot()
{
    Option option = Pair(Over(100, 100, BAR), Over(60, 50, 1.0f));
    CHECK(option.Bar() == 100 * BAR + 50);
    CHECK(option.NewSum() > option.Bar());

    option = Pair(Over(100, 100, BAR), Over(55, 50, BAR));
    CHECK(!option.Wanted());
}

// both rings worn: the weaker one goes
void RingReplacesTheWeakerOne()
{
    std::vector<Option> options = {Single(Over(100, 90)), Single(Over(100, 60))};
    CHECK(Choose(options, false) == 1);
}

void EmptySlotComesFirst()
{
    std::vector<Option> options = {Single(Over(100, 20)), Single(Over(100, 0))};
    options[1].preference = 2;
    CHECK(Choose(options, false) == 1);
}

// the sim ranks the stronger worn ring lower, so that one goes
void WornRankOrderPicksTheSlot()
{
    std::vector<Option> options = {Single(Over(100, 60)), Single(Over(100, 90))};
    options[1].preference = 1;
    CHECK(Choose(options, false) == 1);
}

void UniqueBlockedSlotIsSkipped()
{
    std::vector<Option> options = {Single(Over(100, 20)), Single(Over(100, 90))};
    options[0].legal = false;
    CHECK(Choose(options, false) == 1);

    options[1].legal = false;
    CHECK(Choose(options, false) == -1);
    CHECK(Choose(options, true) == -1);
}

void RankOrderBeatsScores()
{
    Option better = Single(Over(50, 100));
    better.rankOrder = 1;
    CHECK(better.Wanted());

    Option worse = Single(Over(200, 100));
    worse.rankOrder = -1;
    CHECK(!worse.Wanted());

    Option ignored = Single(Over(200, 100));
    ignored.rankOrder = -1;
    ignored.legal = false;
    CHECK(Choose({ignored}, false) == -1);
}

void BrokenWornItemIsReplaced()
{
    Option option = Single(Over(10, 100));
    CHECK(!option.Wanted());
    option.replacesBroken = true;
    CHECK(option.Wanted());
}

void ForceTakesTheBestLegalOption()
{
    std::vector<Option> options = {Single(Over(50, 100)), Single(Over(70, 100))};
    CHECK(Choose(options, false) == -1);
    CHECK(Choose(options, true) == 1);

    // a wanted option still wins under force
    options.push_back(Single(Over(120, 100)));
    options.push_back(Single(Over(30, 0)));
    options[3].preference = 2;
    CHECK(Choose(options, true) == 3);
}

Hand OneHand() { return {Kind::OneHand, false, false}; }
Hand TwoHand(bool titanGripType) { return {Kind::TwoHand, titanGripType, false}; }
Hand Shield() { return {Kind::OffHandItem, false, false}; }
Hand None() { return {}; }

void OffHandNeedsDualWield()
{
    Bot const noDualWield{false, false, false};
    CHECK(LayoutLegal(Layout::NewToMain, OneHand(), OneHand(), Shield(), noDualWield));
    CHECK(!LayoutLegal(Layout::NewToOff, OneHand(), OneHand(), Shield(), noDualWield));
    CHECK(!LayoutLegal(Layout::NewToMainOldToOff, OneHand(), OneHand(), Shield(), noDualWield));
}

void LoneTwoHandClosesTheOffHand()
{
    Bot const dualWield{true, false, false};
    CHECK(!LayoutLegal(Layout::NewToOff, OneHand(), TwoHand(true), None(), dualWield));
    CHECK(!LayoutLegal(Layout::NewToMainOldToOff, OneHand(), TwoHand(true), None(), dualWield));
    CHECK(LayoutLegal(Layout::NewToMain, OneHand(), TwoHand(true), None(), dualWield));
    // a new lone 2H only ever goes to the main hand
    CHECK(LayoutLegal(Layout::NewToMain, TwoHand(false), OneHand(), OneHand(), dualWield));
    CHECK(!LayoutLegal(Layout::NewToOff, TwoHand(false), OneHand(), OneHand(), dualWield));
    CHECK(!LayoutLegal(Layout::NewToMainOldToOff, TwoHand(false), OneHand(), OneHand(), dualWield));
}

void TitanGripTakesTwoHandsInBoth()
{
    Bot const titanGrip{true, true, false};
    CHECK(LayoutLegal(Layout::NewToOff, TwoHand(true), TwoHand(true), None(), titanGrip));
    CHECK(LayoutLegal(Layout::NewToMainOldToOff, TwoHand(true), TwoHand(true), TwoHand(true), titanGrip));
    CHECK(!LayoutLegal(Layout::NewToMain, TwoHand(false), OneHand(), None(), titanGrip));
}

void PolearmNeverGoesToTheOffHand()
{
    Bot const dualWield{true, false, false};
    Hand const polearm{Kind::OneHand, false, true};
    CHECK(!LayoutLegal(Layout::NewToOff, polearm, OneHand(), None(), dualWield));
    CHECK(!LayoutLegal(Layout::NewToMainOldToOff, OneHand(), polearm, None(), dualWield));
    CHECK(LayoutLegal(Layout::NewToMain, polearm, OneHand(), None(), dualWield));
}

void TankKeepsItsShield()
{
    Bot const tank{true, false, true};
    CHECK(!LayoutLegal(Layout::NewToOff, OneHand(), OneHand(), Shield(), tank));
    CHECK(!LayoutLegal(Layout::NewToMain, TwoHand(false), OneHand(), Shield(), tank));
    CHECK(LayoutLegal(Layout::NewToMain, OneHand(), OneHand(), Shield(), tank));
}

// with the main hand empty, a weapon that fits there goes there
void EmptyMainHandIsFilledFirst()
{
    Bot const dualWield{true, false, false};
    CHECK(!LayoutLegal(Layout::NewToOff, OneHand(), None(), OneHand(), dualWield));
    Hand const offHandOnly{Kind::OffHandOnly, false, false};
    CHECK(LayoutLegal(Layout::NewToOff, offHandOnly, None(), None(), dualWield));
    CHECK(!LayoutLegal(Layout::NewToOffOldToMain, OneHand(), OneHand(), None(), dualWield));
}
}  // namespace

int main()
{
    BetterThanTheOffHandGoesThere();
    JustUnderTheBarKeeps();
    SwapLayoutQualifies();
    BarSumsPerSlot();
    RingReplacesTheWeakerOne();
    EmptySlotComesFirst();
    WornRankOrderPicksTheSlot();
    UniqueBlockedSlotIsSkipped();
    RankOrderBeatsScores();
    BrokenWornItemIsReplaced();
    ForceTakesTheBestLegalOption();
    OffHandNeedsDualWield();
    LoneTwoHandClosesTheOffHand();
    TitanGripTakesTwoHandsInBoth();
    PolearmNeverGoesToTheOffHand();
    TankKeepsItsShield();
    EmptyMainHandIsFilledFirst();
    std::puts("gear_placement_test: ok");
    return 0;
}
