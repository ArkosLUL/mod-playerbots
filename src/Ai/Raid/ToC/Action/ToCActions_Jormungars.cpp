#include "ToCActions_Jormungars.h"

#include <cmath>
#include <functional>
#include <string>
#include <vector>

#include "EncounterHelpers.h"
#include "LastMovementValue.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"
#include "RaidObs.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "ToCData.h"
#include "ToCHelpers_Icehowl.h"
#include "ToCHelpers_Jormungars.h"
#include "ToCHelpers_NorthrendBeasts.h"
#include "Unit.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

namespace
{
constexpr float APPROACH_ARRIVE = 1.0f;
constexpr float APPROACH_REAIM_DISTANCE = 3.0f;
constexpr float SWEEP_RADIUS = 30.0f;
constexpr float SWEEP_DISTANCE_STEP = 2.0f;
constexpr float SWEEP_ANGLE_STEP = static_cast<float>(M_PI) / 8.0f;
constexpr float WALK_SPOT_TOLERANCE = 0.5f;
constexpr uint32 WALK_STALL_MS = 500;
constexpr float CURE_DIRECT_INSET = 1.0f;
constexpr float CURE_PARTIAL_TURN = static_cast<float>(M_PI) / 8.0f;
constexpr float FEET_CLEARANCE = 1.0f;

char const* const CLASS_TAUNTS[] = {"taunt", "hand of reckoning", "dark command", "growl"};

bool TryClassTaunt(PlayerbotAI* botAI, Player* bot, Unit* worm)
{
    for (char const* taunt : CLASS_TAUNTS)
    {
        if (!botAI->CanCastSpell(taunt, worm))
            continue;

        uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", taunt)->Get();
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
        if (info && bot->IsWithinCombatRange(worm, bot->GetSpellMaxRangeForTarget(worm, info)) &&
            bot->IsWithinLOSInMap(worm))
            return CastClassTaunt(botAI, worm);
    }

    return false;
}

void NoteWormMove(Player* bot, std::string const& branch)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "nb.wormmove", branch);
}

void NotePoolMove(Player* bot, std::string const& branch)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "nb.poolmove", branch);
}

void NoteNitro(Player* bot, std::string const& reason)
{
    if (RaidObs::Active())
        RaidObs::Note(bot, "nb.nitro", reason);
}

std::string Yards(float distance) { return std::to_string(static_cast<int32>(std::lround(distance))); }

bool BookedOn(LastMovement const& last, Position const& spot)
{
    return std::fabs(last.lastMoveToX - spot.GetPositionX()) <= WALK_SPOT_TOLERANCE &&
           std::fabs(last.lastMoveToY - spot.GetPositionY()) <= WALK_SPOT_TOLERANCE;
}

bool ClearOfCircles(std::vector<HazardCircle> const& circles, Position const& spot)
{
    for (HazardCircle const& circle : circles)
        if (circle.first.GetExactDist2d(spot) < circle.second)
            return false;

    return true;
}

bool OnWormFloor(float x, float y) { return ARENA_CENTER.GetExactDist2d(x, y) <= WORM_FLOOR_RADIUS; }

bool OnWormEdge(float x, float y)
{
    float const dist = ARENA_CENTER.GetExactDist2d(x, y);
    return dist >= WORM_EDGE_MIN && dist <= WORM_EDGE_MAX;
}

void DropForeignBooking(LastMovement& last, Position const& own, MovementPriority upTo)
{
    if (last.msTime && last.priority <= upTo && !BookedOn(last, own))
        last.clear();
}

bool DirectCureSpot(Player* bot, WormMovePlan const& plan, Position& spot)
{
    Position const& partner = plan.partnerPos;
    float const reach = plan.partnerReach - CURE_DIRECT_INSET;
    float const step = 2.0f * static_cast<float>(M_PI) / WORM_CURE_DIRECT_BEARINGS;
    bool found = false;
    float nearest = 0.0f;
    for (uint32 bearing = 0; bearing < WORM_CURE_DIRECT_BEARINGS; ++bearing)
    {
        float const angle = step * bearing;
        Position const candidate(partner.GetPositionX() + reach * std::cos(angle),
                                 partner.GetPositionY() + reach * std::sin(angle), partner.GetPositionZ());
        float const walk = bot->GetExactDist2d(candidate);
        if ((found && walk >= nearest) || !WormSpotStillSafe(plan, candidate))
            continue;

        spot = candidate;
        nearest = walk;
        found = true;
    }

    return found;
}

bool DetourCureSpot(Player* bot, WormMovePlan const& plan, Position& spot)
{
    if (!plan.acceptVia || !plan.escapeAccept)
        return false;

    Position const& partner = plan.partnerPos;
    float const reach = plan.partnerReach - CURE_DIRECT_INSET;
    float const step = 2.0f * static_cast<float>(M_PI) / WORM_CURE_DIRECT_BEARINGS;
    std::vector<Position> ends;
    for (uint32 bearing = 0; bearing < WORM_CURE_DIRECT_BEARINGS; ++bearing)
    {
        Position const end(partner.GetPositionX() + reach * std::cos(step * bearing),
                           partner.GetPositionY() + reach * std::sin(step * bearing), partner.GetPositionZ());
        if (ClearOfCircles(plan.urgentCircles, end))
            ends.push_back(end);
    }

    bool found = false;
    float shortest = 0.0f;
    for (uint32 bearing = 0; bearing < WORM_CURE_DIRECT_BEARINGS && !ends.empty(); ++bearing)
    {
        for (float leg = WORM_CURE_DETOUR_STEP; leg <= WORM_SEARCH_RADIUS; leg += WORM_CURE_DETOUR_STEP)
        {
            Position const via(bot->GetPositionX() + leg * std::cos(step * bearing),
                               bot->GetPositionY() + leg * std::sin(step * bearing), bot->GetPositionZ());
            if ((found && leg >= shortest) || !ClearOfCircles(plan.circles, via) ||
                !plan.escapeAccept(via.GetPositionX(), via.GetPositionY()))
                continue;

            for (Position const& end : ends)
            {
                float const walk = leg + via.GetExactDist2d(end);
                if ((found && walk >= shortest) || !plan.acceptVia(via, end.GetPositionX(), end.GetPositionY()))
                    continue;

                spot = via;
                shortest = walk;
                found = true;
            }
        }
    }

    return found;
}

bool PartialCureSpot(Player* bot, WormMovePlan const& plan, Position& spot)
{
    if (bot->GetExactDist2d(plan.partnerPos) <= WORM_SEARCH_RADIUS)
        return false;

    float const heading = bot->GetAngle(plan.partnerPos.GetPositionX(), plan.partnerPos.GetPositionY());
    for (float const turn : {0.0f, CURE_PARTIAL_TURN, -CURE_PARTIAL_TURN, 2.0f * CURE_PARTIAL_TURN,
                             -2.0f * CURE_PARTIAL_TURN})
    {
        float const x = bot->GetPositionX() + WORM_CURE_PARTIAL_STEP * std::cos(heading + turn);
        float const y = bot->GetPositionY() + WORM_CURE_PARTIAL_STEP * std::sin(heading + turn);
        Position const candidate(x, y, bot->GetPositionZ());
        if (ClearOfCircles(plan.circles, candidate) && (!plan.escapeAccept || plan.escapeAccept(x, y)))
        {
            spot = candidate;
            return true;
        }
    }

    return false;
}

bool FindWormSpot(Player* bot, WormMovePlan const& plan, Position& spot, WormSpotTier& tier)
{
    auto const fits = [&plan](float x, float y)
    { return (!plan.accept || plan.accept(x, y)) && (!plan.roleAccept || plan.roleAccept(x, y)); };

    if (plan.hasStand && ClearOfCircles(plan.circles, plan.stand) && ClearOfCircles(plan.spreadCircles, plan.stand) &&
        fits(plan.stand.GetPositionX(), plan.stand.GetPositionY()))
    {
        spot = plan.stand;
        tier = WormSpotTier::Stand;
        return true;
    }

    HazardSweepCache sweep;
    std::vector<HazardCircle> const ownSpot = {{bot->GetPosition(), 0.0f}};
    auto const search = [&](std::vector<HazardCircle> const& hazards, std::function<bool(float, float)> const& accept,
                            WormSpotTier found)
    {
        Position const clear =
            FindNearestPositionClearOfHazards(bot, hazards.empty() ? ownSpot : hazards, WORM_SEARCH_RADIUS,
                                              SWEEP_DISTANCE_STEP, SWEEP_ANGLE_STEP, &plan.preferNear, accept, &sweep);
        if (clear == Position())
            return false;

        spot = clear;
        tier = found;
        return true;
    };

    if (plan.escapeFirst && search(plan.escapeCircles, plan.escapeAccept, WormSpotTier::Escape))
        return true;

    std::vector<HazardCircle> withSpread = plan.circles;
    withSpread.insert(withSpread.end(), plan.spreadCircles.begin(), plan.spreadCircles.end());
    if (search(withSpread, fits, WormSpotTier::Full))
        return true;

    if (plan.pairable)
    {
        std::vector<HazardCircle> withPair = plan.circles;
        withPair.insert(withPair.end(), plan.pairCircles.begin(), plan.pairCircles.end());
        if (search(withPair, fits, WormSpotTier::Pair))
            return true;
    }

    if (!plan.spreadStrict && !plan.spreadCircles.empty() && search(plan.circles, fits, WormSpotTier::NoSpread))
        return true;

    bool const cureWalk = IsWormCureReason(plan.reason);
    if (cureWalk && search(plan.circles, plan.accept, WormSpotTier::Partner))
        return true;

    if (plan.urgent)
    {
        if (!plan.escapeFirst && search(plan.escapeCircles, plan.escapeAccept, WormSpotTier::Escape))
            return true;

        if (plan.looseCircles.size() != plan.escapeCircles.size() &&
            search(plan.looseCircles, plan.escapeAccept, WormSpotTier::Loose))
            return true;
    }

    if (cureWalk)
    {
        if (DirectCureSpot(bot, plan, spot))
        {
            tier = WormSpotTier::Direct;
            return true;
        }

        if (DetourCureSpot(bot, plan, spot))
        {
            tier = WormSpotTier::Detour;
            return true;
        }

        if (PartialCureSpot(bot, plan, spot))
        {
            tier = WormSpotTier::Partial;
            return true;
        }
    }

    return plan.urgent && (search(plan.escapeCircles, plan.crossAccept, WormSpotTier::Cross) ||
                           (plan.looseCircles.size() != plan.escapeCircles.size() &&
                            search(plan.looseCircles, plan.crossAccept, WormSpotTier::Cross)));
}
}

bool NorthrendWormsTankHoldAction::Execute(Event /*event*/)
{
    Unit* worm = GetBeastOfDuty(botAI, _duty);
    if (!worm)
    {
        _walking = WalkKind::None;
        return false;
    }

    bool const mobile = _duty == BeastsTankDuty::WormMobile;

    if (IsWormSubmerged(worm))
        return ApproachSubmerged(worm);

    if (_walking == WalkKind::Approach)
        _walking = WalkKind::None;

    if (worm->GetVictim() != bot && !(mobile && IsWormSpewingAtOtherTank(botAI, worm)) &&
        TryClassTaunt(botAI, bot, worm))
        return true;

    if (AI_VALUE(Unit*, "current target") != worm)
        return Attack(worm);

    return mobile && DragOffHazards(worm);
}

bool NorthrendWormsTankHoldAction::ApproachSubmerged(Unit* worm)
{
    float const stand = IsWormMobile(worm) ? WORM_TAUNT_STAND : WORM_STATIONARY_MELEE_RANGE - 2.0f;

    Position chargeStart;
    Position chargeEnd;
    if (bot->GetExactDist2d(worm) <= stand + APPROACH_ARRIVE || IcehowlChargeLatched(botAI, chargeStart, chargeEnd))
    {
        _walking = WalkKind::None;
        return false;
    }

    if (WalkInFlight(WalkKind::Approach) && worm->GetExactDist2d(_approachFrom) < APPROACH_REAIM_DISTANCE)
        return true;

    float const angle = worm->GetAngle(bot);
    Position const spot(worm->GetPositionX() + stand * std::cos(angle),
                        worm->GetPositionY() + stand * std::sin(angle), worm->GetPositionZ());
    float const travel = bot->GetExactDist2d(spot);
    if (!WalkTo(WalkKind::Approach, spot))
        return false;

    _approachFrom = worm->GetPosition();
    NoteWormMove(bot, "approach " + Yards(travel));
    return true;
}

bool NorthrendWormsTankHoldAction::DragOffHazards(Unit* worm)
{
    if (IsWormSpewing(botAI, worm))
    {
        if (WalkInFlight(WalkKind::Drag))
        {
            bot->StopMoving();
            AI_VALUE(LastMovement&, "last movement").clear();
        }

        _walking = WalkKind::None;
        return false;
    }

    Position chargeStart;
    Position chargeEnd;
    WormDragPlan plan;
    if (IcehowlChargeLatched(botAI, chargeStart, chargeEnd) || !GetMobileWormDrag(botAI, worm, plan))
    {
        _walking = WalkKind::None;
        return false;
    }

    Position const* inFlight = WalkInFlight(WalkKind::Drag);
    if (inFlight && ClearOfCircles(plan.poolsOnly, *inFlight))
        return true;

    if (!inFlight && MobileWormDragWaits(botAI))
    {
        NoteWormMove(bot, "drag wait");
        return false;
    }

    HazardSweepCache sweep;
    Position spot;
    bool edge = false;
    auto const search = [&](std::vector<HazardCircle> hazards, bool onEdge)
    {
        if (spot != Position() || (onEdge && !plan.edge))
            return;

        hazards.emplace_back(bot->GetPosition(), FEET_CLEARANCE);
        spot = FindNearestPositionClearOfHazards(bot, hazards, SWEEP_RADIUS, SWEEP_DISTANCE_STEP, SWEEP_ANGLE_STEP,
                                                 &plan.preferNear, onEdge ? OnWormEdge : OnWormFloor, &sweep);
        edge = onEdge && spot != Position();
    };

    search(plan.hazards, true);
    search(plan.hazards, false);
    search(plan.poolsOnly, true);
    search(plan.poolsOnly, false);
    if (spot == Position())
        return false;

    float const travel = bot->GetExactDist2d(spot);
    if (!WalkTo(WalkKind::Drag, spot))
        return false;

    NoteWormMove(bot, std::string(edge ? "drag edge " : "drag ") + Yards(travel));
    return true;
}

bool NorthrendWormsTankHoldAction::BookedOnWalkSpot()
{
    return BookedOn(AI_VALUE(LastMovement&, "last movement"), _walkSpot);
}

Position const* NorthrendWormsTankHoldAction::WalkInFlight(WalkKind kind)
{
    if (_walking != kind || !BookedOnWalkSpot())
        return nullptr;

    if (!bot->isMoving() && getMSTimeDiff(_walkIssuedMs, getMSTime()) >= WALK_STALL_MS)
        return nullptr;

    return &_walkSpot;
}

bool NorthrendWormsTankHoldAction::WalkTo(WalkKind kind, Position const& spot)
{
    if (bot->IsMovementPreventedByCasting())
        return false;

    if (BookedOnWalkSpot())
        AI_VALUE(LastMovement&, "last movement").clear();

    _walkSpot = spot;
    _walking = kind;
    _walkIssuedMs = getMSTime();
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_COMBAT);
}

Player* NorthrendWormsRedirectThreatAction::GetRedirectTank()
{
    Player* holder = nullptr;
    return GetWormRedirectTarget(botAI, holder) ? holder : nullptr;
}

Unit* NorthrendWormsRedirectThreatAction::GetThreatDumpTarget()
{
    Player* holder = nullptr;
    Unit* worm = GetWormRedirectTarget(botAI, holder);
    return worm && !IsWormSubmerged(worm) ? worm : nullptr;
}

bool NorthrendWormsRepositionAction::Execute(Event /*event*/)
{
    WormMovePlan plan;
    if (!GetWormMovePlan(botAI, plan) || plan.reason == WormMoveReason::None)
        return false;

    if (!botAI->CanMove())
    {
        NoteWormMove(bot, "stunned");
        return false;
    }

    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    Position spot;
    WormMoveReason reason = plan.reason;
    WormSpotTier tier = WormSpotTier::Full;
    if (plan.held)
    {
        uint32 const age = getMSTimeDiff(plan.hold.issuedMs, getMSTime());
        if (BookedOn(last, plan.hold.spot) && age < WORM_WALK_LATCH_MS && (bot->isMoving() || age < WALK_STALL_MS))
        {
            NoteWormMove(bot, std::string("hold ") + WormMoveReasonName(plan.hold.reason));
            return true;
        }

        spot = plan.hold.spot;
        reason = plan.hold.reason;
        tier = plan.hold.tier;
    }

    bool const pinned = bot->IsMovementPreventedByCasting();
    if (pinned && !plan.urgent)
    {
        NoteWormMove(bot, "locked");
        return false;
    }

    if (!plan.held && !FindWormSpot(bot, plan, spot, tier))
    {
        NoteWormMove(bot, std::string("none ") + WormMoveReasonName(reason));
        if (plan.urgent)
            return true;

        BackOffWormSearch(botAI);
        return false;
    }

    if (pinned)
        bot->InterruptNonMeleeSpells(true);

    if ((_hasIssuedSpot && BookedOn(last, _issuedSpot)) || (plan.held && BookedOn(last, plan.hold.spot)))
        last.clear();

    DropForeignBooking(last, spot, MovementPriority::MOVEMENT_COMBAT);

    MovementPriority const priority =
        plan.urgent || IsWormCureReason(reason) ? MovementPriority::MOVEMENT_FORCED : MovementPriority::MOVEMENT_COMBAT;
    float const travel = bot->GetExactDist2d(spot);
    switch (TryMoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                      false, true, priority, true))
    {
        case MoveOutcome::Issued:
            SetWormHold(botAI, spot, reason, tier, false);
            _issuedSpot = spot;
            _hasIssuedSpot = true;
            NoteWormMove(bot, "move " + Yards(travel) + " " + WormMoveReasonName(reason) + " " +
                                  WormSpotTierName(tier));
            if (IsWormCureReason(reason) && travel > WORM_CURE_NITRO_FROM && UseNitroBoosts(bot))
                NoteNitro(bot, "cure " + Yards(travel));

            return true;
        case MoveOutcome::AlreadyThere:
            if (WormHoldParks(reason, tier))
                SetWormHold(botAI, spot, reason, tier, true);

            NoteWormMove(bot, "clear");
            return false;
        default:
            NoteWormMove(bot, "locked");
            return plan.urgent;
    }
}

bool NorthrendWormsLeavePoolAction::Execute(Event /*event*/)
{
    if (!InSlimePoolWithoutWorms(botAI))
    {
        _hasExit = false;
        return false;
    }

    if (!botAI->CanMove())
    {
        NotePoolMove(bot, "stunned");
        return false;
    }

    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    uint32 const now = getMSTime();
    if (_hasExit)
    {
        uint32 const age = getMSTimeDiff(_exitMs, now);
        bool const exitClear = ClearOfCircles(GetSlimePoolCircles(botAI, 0, WORM_POOL_TRIGGER_PAD), _exitSpot);
        if (exitClear && BookedOn(last, _exitSpot) && age < WORM_WALK_LATCH_MS &&
            (bot->isMoving() || age < WALK_STALL_MS))
        {
            NotePoolMove(bot, "hold");
            return true;
        }

        if (BookedOn(last, _exitSpot))
            last.clear();

        _hasExit = false;
    }

    Position spot;
    if (!FindSlimePoolExit(botAI, spot))
    {
        NotePoolMove(bot, "none");
        return false;
    }

    if (bot->IsMovementPreventedByCasting())
        bot->InterruptNonMeleeSpells(true);

    DropForeignBooking(last, spot, MovementPriority::MOVEMENT_COMBAT);
    float const travel = bot->GetExactDist2d(spot);
    switch (TryMoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false,
                      false, true, MovementPriority::MOVEMENT_FORCED, true))
    {
        case MoveOutcome::Issued:
            _exitSpot = spot;
            _exitMs = now;
            _hasExit = true;
            NotePoolMove(bot, "move " + Yards(travel));
            return true;
        case MoveOutcome::AlreadyThere:
            NotePoolMove(bot, "clear");
            return false;
        default:
            NotePoolMove(bot, "locked");
            return true;
    }
}

bool NorthrendWormsHealToxinCarrierAction::Execute(Event /*event*/)
{
    Player* carrier = GetWormToxinHealTarget(botAI);
    return carrier && CastRaidThroughputHeal(botAI, carrier);
}

bool NorthrendWormsPurgeToxinAction::Execute(Event /*event*/)
{
    char const* purge = GetWormToxinSelfPurge(botAI);
    return purge && botAI->CastSpell(purge, bot);
}
