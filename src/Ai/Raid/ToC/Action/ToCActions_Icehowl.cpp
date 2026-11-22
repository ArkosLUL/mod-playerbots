#include "ToCActions_Icehowl.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "Creature.h"
#include "EncounterHelpers.h"
#include "LastMovementValue.h"
#include "Playerbots.h"
#include "RaidTankDefensive.h"
#include "Timer.h"
#include "ToCHelpers_Icehowl.h"
#include "ToCHelpers_Jormungars.h"
#include "ToCHelpers_NorthrendBeasts.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

namespace
{
constexpr float DODGE_SEARCH_RADIUS = 30.0f;
constexpr float DODGE_ARRIVE = 1.0f;
constexpr uint32 DODGE_LATCH_MS = 5000;
constexpr uint32 DODGE_STALL_MS = 500;
constexpr float DODGE_SPOT_TOLERANCE = 0.5f;
constexpr float LANE_CIRCLE_SPACING = 4.0f;
constexpr uint32 DAZE_NITRO_RETRY_MS = 15000;

void NoteDodge(Player* bot, std::string const& branch)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "nb.dodge", branch);
}

std::vector<HazardCircle> LaneCircles(Position const& start, Position const& end, float clearance)
{
    float const dx = end.GetPositionX() - start.GetPositionX();
    float const dy = end.GetPositionY() - start.GetPositionY();
    float const length = std::hypot(dx, dy);
    uint32 const steps = std::max<uint32>(1, static_cast<uint32>(std::ceil(length / LANE_CIRCLE_SPACING)));

    std::vector<HazardCircle> circles;
    circles.reserve(steps + 1);
    for (uint32 i = 0; i <= steps; ++i)
    {
        float const t = static_cast<float>(i) / static_cast<float>(steps);
        circles.emplace_back(
            Position(start.GetPositionX() + t * dx, start.GetPositionY() + t * dy, start.GetPositionZ()), clearance);
    }

    return circles;
}

Position FindLaneClearSpot(Player* bot, Position const& start, Position const& end, float clearance,
                           std::vector<HazardCircle> const& pools, HazardSweepCache* sweep)
{
    std::vector<HazardCircle> hazards = LaneCircles(start, end, clearance);
    hazards.insert(hazards.end(), pools.begin(), pools.end());
    return FindNearestPositionClearOfHazards(
        bot, hazards, DODGE_SEARCH_RADIUS, 2.0f, static_cast<float>(M_PI) / 8.0f, nullptr,
        [&start, &end, clearance](float x, float y) { return DistanceToIcehowlLane(start, end, x, y) >= clearance; },
        sweep);
}

bool InAnyCircle(std::vector<HazardCircle> const& circles, Position const& at)
{
    return std::any_of(circles.begin(), circles.end(),
                       [&at](HazardCircle const& circle) { return circle.first.GetExactDist2d(at) < circle.second; });
}
}

bool IcehowlTankHoldBossAction::Execute(Event /*event*/)
{
    Unit* icehowl = GetEngagedBeast(botAI, NorthrendBeast::Icehowl);
    if (!icehowl)
        return false;

    Creature* creature = icehowl->ToCreature();
    bool const passive = creature && creature->GetReactState() == REACT_PASSIVE;
    Unit* victim = icehowl->GetVictim();
    Player* victimPlayer = victim ? victim->ToPlayer() : nullptr;
    bool const victimIsTank = victimPlayer && IsBeastsTank(victimPlayer);
    if (!passive && !victimIsTank && CastClassTaunt(botAI, icehowl))
        return true;

    if (AI_VALUE(Unit*, "current target") != icehowl)
        return Attack(icehowl);

    return false;
}

bool IcehowlClearChargePathAction::Execute(Event /*event*/)
{
    Position start;
    Position end;
    if (!IcehowlChargeLatched(botAI, start, end))
    {
        _hasDodgeSpot = false;
        return false;
    }

    if (!botAI->CanMove())
    {
        NoteDodge(bot, "stunned");
        return false;
    }

    float const fromLane = DistanceToIcehowlLane(start, end, bot->GetPositionX(), bot->GetPositionY());
    bool const inLane = fromLane < ICEHOWL_TRAMPLE_RADIUS;
    uint32 const now = getMSTime();

    bool replacing = false;
    if (_hasDodgeSpot)
    {
        uint32 const age = getMSTimeDiff(_dodgeSpotMs, now);
        bool const spotClear = DistanceToIcehowlLane(start, end, _dodgeSpot.GetPositionX(),
                                                     _dodgeSpot.GetPositionY()) >= ICEHOWL_CHARGE_TIGHT_CLEARANCE;
        if (age < DODGE_LATCH_MS && spotClear)
        {
            if (bot->GetExactDist2d(_dodgeSpot.GetPositionX(), _dodgeSpot.GetPositionY()) <= DODGE_ARRIVE)
            {
                NoteDodge(bot, "clear");
                return false;
            }

            LastMovement& last = AI_VALUE(LastMovement&, "last movement");
            bool const bookedHere = std::fabs(last.lastMoveToX - _dodgeSpot.GetPositionX()) <= DODGE_SPOT_TOLERANCE &&
                                    std::fabs(last.lastMoveToY - _dodgeSpot.GetPositionY()) <= DODGE_SPOT_TOLERANCE;
            if (!_dodgeParked && bookedHere && !bot->IsMovementPreventedByCasting() &&
                (bot->isMoving() || age < DODGE_STALL_MS))
            {
                NoteDodge(bot, "hold");
                return true;
            }
        }

        replacing = !_dodgeParked;
        _hasDodgeSpot = false;
    }

    HazardSweepCache sweep;
    std::vector<HazardCircle> const pools = GetSlimePoolCircles(botAI, WORM_POOL_SOON_TICKS, WORM_POOL_CLEARANCE_PAD);
    bool tight = false;
    Position spot = FindLaneClearSpot(bot, start, end, ICEHOWL_CHARGE_CLEARANCE, pools, &sweep);
    if (spot == Position())
    {
        if (fromLane >= ICEHOWL_CHARGE_TIGHT_CLEARANCE && !InAnyCircle(pools, bot->GetPosition()))
        {
            _dodgeSpot = bot->GetPosition();
            _dodgeSpotMs = now;
            _hasDodgeSpot = true;
            _dodgeParked = true;
            NoteDodge(bot, "clear");
            return false;
        }

        tight = true;
        spot = FindLaneClearSpot(bot, start, end, ICEHOWL_CHARGE_TIGHT_CLEARANCE, pools, &sweep);
        if (spot == Position() && !pools.empty())
            spot = FindLaneClearSpot(bot, start, end, ICEHOWL_CHARGE_TIGHT_CLEARANCE, {}, &sweep);
    }

    if (spot == Position())
    {
        NoteDodge(bot, "none");
        return inLane;
    }

    if (bot->IsMovementPreventedByCasting())
        bot->InterruptNonMeleeSpells(true);

    float const travel = bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY());
    switch (IssueDodge(spot, replacing))
    {
        case MoveOutcome::Issued:
            break;
        case MoveOutcome::AlreadyThere:
            NoteDodge(bot, "clear");
            return false;
        default:
            NoteDodge(bot, "locked");
            return inLane;
    }

    _dodgeSpot = spot;
    _dodgeSpotMs = now;
    _hasDodgeSpot = true;
    _dodgeParked = false;

    if (tight)
        NoteDodge(bot, "tight");
    else
        NoteDodge(bot, "move " + std::to_string(static_cast<int32>(std::lround(travel))));

    return true;
}

MoveOutcome IcehowlClearChargePathAction::IssueDodge(Position const& spot, bool replacing)
{
    if (replacing)
        AI_VALUE(LastMovement&, "last movement").clear();

    auto const issue = [this, &spot]()
    {
        return TryMoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false,
                         false, false, true, MovementPriority::MOVEMENT_FORCED, true);
    };

    MoveOutcome const outcome = issue();
    if (outcome != MoveOutcome::Duplicate && outcome != MoveOutcome::Waiting)
        return outcome;

    if (bot->isMoving())
        return outcome == MoveOutcome::Duplicate ? MoveOutcome::Issued : outcome;

    AI_VALUE(LastMovement&, "last movement").clear();
    return issue();
}

bool IcehowlTankDefensiveAction::Execute(Event /*event*/)
{
    uint32 const defensive = NextTankDefensive(botAI, bot, "nb.defensive");
    return defensive && botAI->CastSpell(defensive, bot);
}

bool IcehowlMoveToBreathStandAction::Execute(Event /*event*/)
{
    IcehowlBreathStand stand;
    if (!GetIcehowlBreathStand(botAI, stand) ||
        IsOnIcehowlBreathStand(stand, bot->GetPositionX(), bot->GetPositionY(), ICEHOWL_SPREAD_ARRIVE_DEG,
                               stand.arrive))
    {
        DropWalk();
        return false;
    }

    if (Position const* spot = WalkInFlight())
        if (IsOnIcehowlBreathStand(stand, spot->GetPositionX(), spot->GetPositionY(), ICEHOWL_SPREAD_TRIGGER_DEG,
                                   stand.trigger))
            return true;

    return WalkTo(stand.spot);
}

bool IcehowlDazeNitroAction::isUseful()
{
    return !_tried || getMSTimeDiff(_triedMs, getMSTime()) >= DAZE_NITRO_RETRY_MS;
}

bool IcehowlDazeNitroAction::Execute(Event /*event*/)
{
    _tried = true;
    _triedMs = getMSTime();

    Unit* icehowl = GetEngagedBeast(botAI, NorthrendBeast::Icehowl);
    if (!icehowl || !UseNitroBoosts(bot))
        return false;

    if (RaidObs::Active())
    {
        float const walk = bot->GetExactDist2d(icehowl) - ICEHOWL_MELEE_RANGE;
        RaidObs::Note(bot, "nb.nitro", "daze " + std::to_string(static_cast<int32>(std::lround(walk))));
    }

    return true;
}
