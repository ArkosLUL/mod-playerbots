#include "ToCActions_Gormok.h"

#include <cmath>
#include <vector>

#include "EncounterHelpers.h"
#include "LastMovementValue.h"
#include "Playerbots.h"
#include "RaidTankDefensive.h"
#include "Timer.h"
#include "ToCData.h"
#include "ToCHelpers_Gormok.h"
#include "ToCHelpers_Icehowl.h"
#include "ToCHelpers_NorthrendBeasts.h"
#include "Unit.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

namespace
{
// A carrier aims this far inside the melee range, since it stops short and he drifts with the drag
constexpr float CARRIER_AIM_INSET = 3.0f;
// A latched carrier spot still counts while it sits this far inside the range
constexpr float CARRIER_KEEP_INSET = 1.0f;
// Room past the clearance: a caster on his centre has all of it to walk
constexpr float STOMP_SWEEP_RADIUS = 30.0f;
constexpr float WALK_SPOT_TOLERANCE = 0.5f;
// A spline issued last tick may not show in isMoving yet
constexpr uint32 WALK_STALL_MS = 500;
}

bool GormokTankHoldBossAction::Execute(Event /*event*/)
{
    Unit* gormok = GetEngagedBeast(botAI, NorthrendBeast::Gormok);
    if (!gormok)
        return false;

    MarkTargetWithSkull(bot, gormok);
    SetRtiTarget(botAI, "skull", gormok);

    if (AI_VALUE(Unit*, "current target") != gormok)
        return Attack(gormok);

    // Only take him back off a non-tank. The swap tank taunts on its own node, and a hold that taunted
    // back whenever it lost him would undo every swap.
    Unit* victim = gormok->GetVictim();
    Player* victimPlayer = victim ? victim->ToPlayer() : nullptr;
    if (victim && !(victimPlayer && IsBeastsTank(victimPlayer)) && CastClassTaunt(botAI, gormok))
        return true;

    // Every Icehowl charge line crosses the centre, so no drag toward it while one is latched
    Position chargeStart;
    Position chargeEnd;
    if (IcehowlChargeLatched(botAI, chargeStart, chargeEnd))
        return false;

    // Keep the boss near the centre of the arena so ranged can spread and melee have room
    return DragBossToAnchor(gormok, ARENA_CENTER);
}

bool GormokFocusSnoboldAction::Execute(Event /*event*/)
{
    Unit* snobold = GetGormokSnoboldPick(botAI);
    if (!snobold || AI_VALUE(Unit*, "current target") == snobold)
        return false;

    return Attack(snobold);
}

bool GormokTankSwapTauntAction::Execute(Event /*event*/)
{
    Unit* gormok = GetGormokSwapTauntTarget(botAI);
    if (!gormok)
        return false;

    if (CastClassTaunt(botAI, gormok))
        return true;

    // Taunt on cooldown or resisted: build threat on him meanwhile
    if (AI_VALUE(Unit*, "current target") != gormok)
        return Attack(gormok);

    return false;
}

bool GormokTankDefensiveAction::Execute(Event /*event*/)
{
    char const* defensive = NextTankDefensive(botAI, bot, "nb.defensive");
    return defensive && botAI->CastSpell(defensive, bot);
}

bool GormokWalkAction::BookedOnWalkSpot()
{
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    return std::fabs(last.lastMoveToX - walkSpot.GetPositionX()) <= WALK_SPOT_TOLERANCE &&
           std::fabs(last.lastMoveToY - walkSpot.GetPositionY()) <= WALK_SPOT_TOLERANCE;
}

Position const* GormokWalkAction::WalkInFlight()
{
    // Moving on some other node's walk doesn't count
    if (!walking || !bot->isMoving() || !BookedOnWalkSpot())
        return nullptr;

    return &walkSpot;
}

bool GormokWalkAction::WalkTo(Position const& spot)
{
    // A walk issued mid cast never starts but still books its travel time, and the same spot is
    // refused until that runs out. Wait for the cast to end instead.
    if (bot->IsMovementPreventedByCasting())
        return false;

    // Head Crack or a channel stopped our last walk short, and its booking would refuse this one
    uint32 const now = getMSTime();
    if (walking && !bot->isMoving() && getMSTimeDiff(walkIssuedMs, now) > WALK_STALL_MS && BookedOnWalkSpot())
        AI_VALUE(LastMovement&, "last movement").clear();

    walkSpot = spot;
    walking = true;
    walkIssuedMs = now;
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_COMBAT);
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

    if (Position const* spot = WalkInFlight())
        if (gormok->GetExactDist2d(spot) >= GORMOK_STOMP_TRIGGER)
            return true;

    // Ties go toward his tank, so healers stay in range of whoever they're keeping up
    Position tank;
    Position const* preferNear = nullptr;
    if (Unit* victim = gormok->GetVictim())
    {
        tank = victim->GetPosition();
        preferNear = &tank;
    }

    std::vector<HazardCircle> const hazards = { { gormok->GetPosition(), GORMOK_STOMP_CLEARANCE } };
    Position const spot = FindNearestPositionClearOfHazards(bot, hazards, STOMP_SWEEP_RADIUS, 2.0f,
                                                            static_cast<float>(M_PI) / 8.0f, preferNear);
    if (spot == Position())
        return false;

    return WalkTo(spot);
}
