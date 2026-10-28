/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCActions_Rotface.h"

#include <cmath>
#include <string>

#include "ICCHelpers_Shared.h"
#include "LastMovementValue.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "RaidTankDefensive.h"
#include "Timer.h"

namespace
{
constexpr float WALK_SPOT_TOLERANCE = 0.5f;
// A spline issued last tick may not show in isMoving yet
constexpr uint32 WALK_STALL_MS = 500;
// A carrier re-aims its walk once the ooze it walks to has moved this far
constexpr float CARRIER_REAIM = 2.0f;

void NoteDodge(Player* bot, std::string const& branch)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "rf.dodge", branch);
}
}  // namespace

Position const* IccRotfaceWalkAction::WalkInFlight()
{
    // Moving on some other node's walk doesn't count
    if (!walking || !bot->isMoving() || !BookedOnWalkSpot())
        return nullptr;

    return &walkSpot;
}

bool IccRotfaceWalkAction::WalkTo(Position const& spot, MovementPriority priority)
{
    // A walk issued mid-cast never starts but still books its travel time, and the same spot is refused
    // until that runs out
    if (bot->IsMovementPreventedByCasting())
        return false;

    // A stun or an item use stopped the last walk short, and its booking would refuse this one
    uint32 const now = getMSTime();
    if (walking && !bot->isMoving() && getMSTimeDiff(walkIssuedMs, now) > WALK_STALL_MS && BookedOnWalkSpot())
        AI_VALUE(LastMovement&, "last movement").clear();

    walkSpot = spot;
    walking = true;
    walkIssuedMs = now;
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false, false,
                  false, priority);
}

bool IccRotfaceWalkAction::BookedOnWalkSpot()
{
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    return std::fabs(last.lastMoveToX - walkSpot.GetPositionX()) <= WALK_SPOT_TOLERANCE &&
           std::fabs(last.lastMoveToY - walkSpot.GetPositionY()) <= WALK_SPOT_TOLERANCE;
}

bool IccRotfaceDodgeAction::Execute(Event /*event*/)
{
    char const* kind = GetRfHazardKind(botAI);
    if (!kind)
    {
        DropWalk();
        return false;
    }

    if (!botAI->CanMove())
    {
        NoteDodge(bot, "pinned");
        return false;
    }

    Position const* inFlight = WalkInFlight();
    if (inFlight && IsRfDodgeSpotClear(botAI, *inFlight))
    {
        NoteDodge(bot, "hold");
        return true;
    }

    Position spot;
    bool interrupt = false;
    std::string branch;
    if (!FindRfDodgeSpot(botAI, spot, interrupt, branch))
    {
        NoteDodge(bot, std::string(kind) + " none");
        return false;
    }

    // Spray, a landing mark, a puddle or a flood tick: each costs more than the cast
    if (interrupt && bot->IsMovementPreventedByCasting())
        bot->InterruptNonMeleeSpells(true);

    // Our own walk still holds the FORCED booking, and a booking only yields to a higher priority
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    bool const lowered = inFlight && last.priority == MovementPriority::MOVEMENT_FORCED;
    if (lowered)
        last.priority = MovementPriority::MOVEMENT_COMBAT;

    float const travel = bot->GetExactDist2d(spot);
    if (!WalkTo(spot, MovementPriority::MOVEMENT_FORCED))
    {
        if (lowered)
            last.priority = MovementPriority::MOVEMENT_FORCED;

        NoteDodge(bot, std::string(kind) + " locked");
        return false;
    }

    NoteDodge(bot, std::string(kind) + " " + branch + " " + std::to_string(static_cast<int32>(std::lround(travel))));
    return true;
}

bool IccRotfaceCastTankDefensiveAction::Execute(Event /*event*/)
{
    uint32 const spellId = GetRfTankDefensive(botAI, "rf.defensive");
    return spellId && botAI->CastSpell(spellId, bot);
}

bool IccRotfaceDispelAction::Execute(Event /*event*/)
{
    char const* spell = nullptr;
    Player* carrier = GetRfDispelTarget(botAI, spell);
    return carrier && spell && botAI->CastSpell(spell, carrier);
}

bool IccRotfaceOozeTauntAction::Execute(Event /*event*/)
{
    Unit* ooze = GetRfOozeToTaunt(botAI);
    if (!ooze || !IccCastClassTaunt(bot, botAI, ooze))
        return false;

    // A taunt moves the ooze, not the bot's own target
    AttackDirect(ooze);
    return true;
}

bool IccRotfaceKiteAction::Execute(Event /*event*/)
{
    if (Position const* inFlight = WalkInFlight())
        if (IsRfKiteSpotSafe(botAI, *inFlight))
            return true;

    Position spot;
    int8 dir = 0;
    if (!GetRfKiteWalk(botAI, spot, dir))
    {
        DropWalk();
        return false;
    }

    if (!WalkTo(spot, MovementPriority::MOVEMENT_COMBAT))
        return false;

    CommitRfKiteDir(botAI, dir);
    return true;
}

bool IccRotfaceCarrierAction::Execute(Event /*event*/)
{
    Position spot;
    if (!GetRfCarrierWalk(botAI, spot))
    {
        DropWalk();
        return false;
    }

    if (Position const* inFlight = WalkInFlight())
        if (inFlight->GetExactDist2d(spot) <= CARRIER_REAIM)
            return true;

    return WalkTo(spot, MovementPriority::MOVEMENT_COMBAT);
}

bool IccRotfaceStandAction::Execute(Event /*event*/)
{
    Position spot;
    if (!GetRfStandWalk(botAI, spot))
    {
        DropWalk();
        return false;
    }

    if (Position const* inFlight = WalkInFlight())
        if (inFlight->GetExactDist2d(spot) <= WALK_SPOT_TOLERANCE)
            return true;

    return WalkTo(spot, MovementPriority::MOVEMENT_COMBAT);
}
