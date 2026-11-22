#include "ToCActions_Gormok.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>
#include <vector>

#include "EncounterHelpers.h"
#include "LastMovementValue.h"
#include "PaladinHelper.h"
#include "Playerbots.h"
#include "RaidTankDefensive.h"
#include "ToCData.h"
#include "ToCHelpers_Gormok.h"
#include "ToCHelpers_Icehowl.h"
#include "ToCHelpers_NorthrendBeasts.h"
#include "Unit.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

namespace
{
constexpr float CARRIER_AIM_INSET = 3.0f;
constexpr float CARRIER_KEEP_INSET = 1.0f;
constexpr float STOMP_SWEEP_RADIUS = 30.0f;

void NoteBomb(Player* bot, std::string const& branch)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "nb.bomb", branch);
}

bool ClearsBombs(Position const& spot, std::vector<Position> const& bombs, float clearance)
{
    return std::none_of(bombs.begin(), bombs.end(),
                        [&spot, clearance](Position const& bomb) { return bomb.GetExactDist2d(spot) < clearance; });
}
}

bool GormokTankHoldBossAction::Execute(Event /*event*/)
{
    Unit* gormok = GetEngagedBeast(botAI, NorthrendBeast::Gormok);
    if (!gormok)
        return false;

    if (AI_VALUE(Unit*, "current target") != gormok)
        return Attack(gormok);

    Unit* victim = gormok->GetVictim();
    Player* victimPlayer = victim ? victim->ToPlayer() : nullptr;
    if (victim && !(victimPlayer && IsBeastsTank(victimPlayer)) && CastClassTaunt(botAI, gormok))
        return true;

    Position chargeStart;
    Position chargeEnd;
    if (IcehowlChargeLatched(botAI, chargeStart, chargeEnd))
        return false;

    return DragBossToAnchor(gormok, ARENA_CENTER);
}

bool GormokSwapTankHoldAction::Execute(Event /*event*/)
{
    return AttackDirect(GetEngagedBeast(botAI, NorthrendBeast::Gormok));
}

bool GormokTankSwapTauntAction::Execute(Event /*event*/)
{
    Unit* gormok = GetGormokSwapTauntTarget(botAI);
    if (!gormok)
        return false;

    if (CastClassTaunt(botAI, gormok))
        return true;

    if (AI_VALUE(Unit*, "current target") != gormok)
        return Attack(gormok);

    return false;
}

bool GormokTankDefensiveAction::Execute(Event /*event*/)
{
    uint32 const defensive = NextTankDefensive(botAI, bot, "nb.defensive");
    return defensive && botAI->CastSpell(defensive, bot);
}

bool GormokHandOfProtectionAction::Execute(Event /*event*/)
{
    Player* target = GetGormokImpaleResetTarget(botAI);
    uint32 const spellId = target ? AI_VALUE2(uint32, "spell id", "hand of protection") : 0;
    return spellId && botAI->CastSpell(spellId, target);
}

bool GormokCancelHandOfProtectionAction::Execute(Event /*event*/)
{
    if (!GormokHandOfProtectionToCancel(botAI))
        return false;

    for (uint32 rank : {ai::paladin::SPELL_HAND_OF_PROTECTION, ai::paladin::SPELL_HAND_OF_PROTECTION_RANK_2,
                        ai::paladin::SPELL_HAND_OF_PROTECTION_RANK_3})
        bot->RemoveAurasDueToSpell(rank, ObjectGuid::Empty, 0, AURA_REMOVE_BY_CANCEL);

    return true;
}

bool GormokBringSnoboldToMeleeAction::Execute(Event /*event*/)
{
    Unit* gormok = GetGormokForSnoboldCarrier(botAI);
    if (!gormok || bot->GetExactDist2d(gormok) <= GORMOK_CARRIER_MELEE_RANGE)
    {
        DropWalk();
        return false;
    }

    if (Position const* spot = WalkInFlight())
        if (gormok->GetExactDist2d(spot) <= GORMOK_CARRIER_MELEE_RANGE - CARRIER_KEEP_INSET)
            return true;

    float const angle = gormok->GetAngle(bot);
    float const reach = GORMOK_CARRIER_MELEE_RANGE - CARRIER_AIM_INSET;
    Position const spot(gormok->GetPositionX() + reach * std::cos(angle),
                        gormok->GetPositionY() + reach * std::sin(angle), gormok->GetPositionZ());
    return WalkTo(spot);
}

bool GormokLeaveStompRangeAction::Execute(Event /*event*/)
{
    Unit* gormok = GetGormokStompThreat(botAI);
    if (!gormok)
    {
        DropWalk();
        return false;
    }

    std::vector<Position> young;
    std::vector<Position> old;
    GetFireBombs(botAI, young, old);

    if (Position const* spot = WalkInFlight())
        if (gormok->GetExactDist2d(spot) >= GORMOK_STOMP_TRIGGER &&
            (!_spotClearsBombs || ClearsBombs(*spot, young, FIRE_BOMB_CLEARANCE)))
            return true;

    Position tank;
    Position const* preferNear = nullptr;
    if (Unit* victim = gormok->GetVictim())
    {
        tank = victim->GetPosition();
        preferNear = &tank;
    }

    HazardCircle const stomp(gormok->GetPosition(), GORMOK_STOMP_CLEARANCE);
    std::vector<HazardCircle> hazards = { stomp };
    for (Position const& bomb : young)
        hazards.emplace_back(bomb, FIRE_BOMB_CLEARANCE);

    HazardSweepCache sweep;
    auto const findSpot = [this, &preferNear, &sweep](std::vector<HazardCircle> const& circles)
    {
        return FindNearestPositionClearOfHazards(bot, circles, STOMP_SWEEP_RADIUS, 2.0f,
                                                 static_cast<float>(M_PI) / 8.0f, preferNear, {}, &sweep);
    };

    Position spot = findSpot(hazards);
    _spotClearsBombs = spot != Position();
    if (!_spotClearsBombs && hazards.size() > 1)
        spot = findSpot({ stomp });

    if (spot == Position())
        return false;

    return WalkTo(spot);
}

bool GormokDodgeFireBombAction::Execute(Event /*event*/)
{
    if (!IsInFireBombImpact(botAI))
    {
        DropWalk();
        return false;
    }

    if (!botAI->CanMove())
    {
        NoteBomb(bot, "stunned");
        return false;
    }

    std::vector<Position> young;
    std::vector<Position> old;
    GetFireBombs(botAI, young, old);

    Position const* inFlight = WalkInFlight();
    if (inFlight && ClearsBombs(*inFlight, young, FIRE_BOMB_TIGHT_CLEARANCE))
    {
        NoteBomb(bot, "hold");
        return true;
    }

    if (bot->IsMovementPreventedByCasting())
    {
        NoteBomb(bot, "pinned");
        return false;
    }

    Position anchor;
    Position const* preferNear = GetFireBombDodgeAnchor(botAI, anchor) ? &anchor : nullptr;

    std::vector<HazardCircle> circles;
    for (Position const& bomb : young)
        circles.emplace_back(bomb, FIRE_BOMB_CLEARANCE);
    for (Position const& bomb : old)
        circles.emplace_back(bomb, FIRE_BOMB_PULSE_CLEARANCE);

    size_t const youngCount = young.size();
    size_t const bombCount = circles.size();
    if (Unit* gormok = GetGormokForStompCaster(botAI))
        circles.emplace_back(gormok->GetPosition(), GORMOK_STOMP_CLEARANCE);

    HazardSweepCache sweep;
    auto const findSpot = [this, &preferNear, &sweep](std::vector<HazardCircle> const& hazards)
    {
        return FindNearestPositionClearOfHazards(bot, hazards, FIRE_BOMB_SWEEP_RADIUS, 2.0f,
                                                 static_cast<float>(M_PI) / 8.0f, preferNear, {}, &sweep);
    };

    Position spot = findSpot(circles);
    if (spot == Position() && circles.size() > bombCount)
    {
        circles.resize(bombCount);
        spot = findSpot(circles);
    }

    if (spot == Position() && bombCount > youngCount)
    {
        circles.resize(youngCount);
        spot = findSpot(circles);
    }

    bool tight = false;
    if (spot == Position())
    {
        tight = true;
        circles.resize(youngCount);
        for (HazardCircle& circle : circles)
            circle.second = FIRE_BOMB_TIGHT_CLEARANCE;
        spot = findSpot(circles);
    }

    if (spot == Position())
    {
        NoteBomb(bot, "none");
        return false;
    }

    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    bool const lowered = inFlight && last.priority == MovementPriority::MOVEMENT_FORCED;
    if (lowered)
        last.priority = MovementPriority::MOVEMENT_COMBAT;

    float const travel = bot->GetExactDist2d(spot);
    if (!WalkTo(spot, MovementPriority::MOVEMENT_FORCED))
    {
        if (lowered)
            last.priority = MovementPriority::MOVEMENT_FORCED;

        NoteBomb(bot, "locked");
        return false;
    }

    NoteBomb(bot, tight ? "tight" : "move " + std::to_string(static_cast<int32>(std::lround(travel))));
    return true;
}
