/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ReforgeCaps.h"

#include <cstdint>
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
using ReforgeCaps::Reforge;
using ReforgeCaps::Rooms;

constexpr uint32_t CRIT = 32;
constexpr uint32_t HASTE = 36;
constexpr uint32_t HIT = ReforgeCaps::MOD_HIT;
constexpr uint32_t EXP = ReforgeCaps::MOD_EXPERTISE;

bool Same(Reforge const& a, Reforge const& b)
{
    return a.from == b.from && a.to == b.to;
}

Rooms MeleeRooms(float hit, float expertise)
{
    Rooms rooms;
    rooms[ReforgeCaps::HIT_MELEE] = hit;
    rooms[ReforgeCaps::EXPERTISE] = expertise;
    return rooms;
}

void FollowsSimWhenCapped()
{
    // 50 hit past the cap, the reforge takes 40
    Reforge const sim{HIT, HASTE, 40.0f};
    CHECK(Same(ReforgeCaps::Pick(MeleeRooms(-50.0f, -10.0f), sim, {}), sim));
}

void RefusesToDropBelowCap()
{
    Reforge const sim{HIT, HASTE, 40.0f};
    CHECK(Same(ReforgeCaps::Pick(MeleeRooms(-10.0f, -10.0f), sim, {}), Reforge{}));
}

void RemovesACurrentReforgeThatHoldsTheBotUnder()
{
    // the sim still wants it, but other gear changed and the bot is 20 short with it on
    Reforge const sim{HIT, HASTE, 40.0f};
    CHECK(Same(ReforgeCaps::Pick(MeleeRooms(20.0f, -10.0f), sim, sim), Reforge{}));
}

void KeepsACurrentReforgeIntoHit()
{
    // taking crit -> hit off leaves the bot 15 short
    Reforge const current{CRIT, HIT, 30.0f};
    Reforge const sim{CRIT, HASTE, 30.0f};
    CHECK(Same(ReforgeCaps::Pick(MeleeRooms(-15.0f, -10.0f), sim, current), current));
    CHECK(Same(ReforgeCaps::Pick(MeleeRooms(-15.0f, -10.0f), Reforge{}, current), current));
    // 40 over with it on: switching still leaves the bot capped
    CHECK(Same(ReforgeCaps::Pick(MeleeRooms(-40.0f, -10.0f), sim, current), sim));
}

void UncappedStatsFollowTheSim()
{
    // already short, but the reforge doesn't touch hit or expertise
    Reforge const sim{CRIT, HASTE, 30.0f};
    CHECK(Same(ReforgeCaps::Pick(MeleeRooms(60.0f, 20.0f), sim, {}), sim));
    Reforge const current{HASTE, CRIT, 30.0f};
    CHECK(Same(ReforgeCaps::Pick(MeleeRooms(60.0f, 20.0f), sim, current), sim));
}

void ReforgesIntoHitWhenShort()
{
    Reforge const sim{HASTE, HIT, 30.0f};
    CHECK(Same(ReforgeCaps::Pick(MeleeRooms(60.0f, 0.0f), sim, {}), sim));
}

void NoCapsNoGuard()
{
    // a healer's hit, or a caster's expertise
    Rooms const none{};
    Reforge const sim{HIT, HASTE, 40.0f};
    CHECK(Same(ReforgeCaps::Pick(none, sim, {}), sim));

    Rooms spell{};
    spell[ReforgeCaps::HIT_SPELL] = -100.0f;
    Reforge const expertise{EXP, CRIT, 20.0f};
    CHECK(Same(ReforgeCaps::Pick(spell, expertise, {}), expertise));
}

void ExpertiseCountsToo()
{
    Reforge const sim{EXP, CRIT, 20.0f};
    CHECK(Same(ReforgeCaps::Pick(MeleeRooms(-50.0f, 5.0f), sim, {}), Reforge{}));
    CHECK(Same(ReforgeCaps::Pick(MeleeRooms(-50.0f, -25.0f), sim, {}), sim));
}

void TradesBetweenCaps()
{
    // hit -> expertise while short on both: 40 more hit missing against 40 less expertise missing
    Reforge const sim{HIT, EXP, 40.0f};
    CHECK(Same(ReforgeCaps::Pick(MeleeRooms(10.0f, 60.0f), sim, {}), sim));
    // only 20 of the expertise is needed, the rest goes to waste while hit falls short
    CHECK(Same(ReforgeCaps::Pick(MeleeRooms(10.0f, 20.0f), sim, {}), Reforge{}));
}

void SpellHitFromGenericHit()
{
    Rooms spell{};
    spell[ReforgeCaps::HIT_SPELL] = 5.0f;
    CHECK(Same(ReforgeCaps::Pick(spell, {HIT, HASTE, 40.0f}, {}), Reforge{}));
    // melee-only hit rating never feeds a spell cap
    Reforge const meleeHit{ReforgeCaps::MOD_HIT_MELEE, HASTE, 40.0f};
    CHECK(Same(ReforgeCaps::Pick(spell, meleeHit, {}), meleeHit));
}
}  // namespace

int main()
{
    FollowsSimWhenCapped();
    RefusesToDropBelowCap();
    RemovesACurrentReforgeThatHoldsTheBotUnder();
    KeepsACurrentReforgeIntoHit();
    UncappedStatsFollowTheSim();
    ReforgesIntoHitWhenShort();
    NoCapsNoGuard();
    ExpertiseCountsToo();
    TradesBetweenCaps();
    SpellHitFromGenericHit();
    std::puts("reforge_caps_test: ok");
    return 0;
}
