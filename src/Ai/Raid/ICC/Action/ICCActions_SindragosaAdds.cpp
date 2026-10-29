/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCActions_SindragosaAdds.h"

#include <cmath>

#include "Creature.h"
#include "ICCHelpers_Shared.h"
#include "LastMovementValue.h"
#include "Playerbots.h"
#include "Timer.h"

namespace
{
constexpr float WALK_SPOT_TOLERANCE = 0.5f;
// A spline issued last tick may not show in isMoving yet
constexpr uint32 WALK_STALL_MS = 500;
}  // namespace

Position const* IccSgAddsWalkAction::WalkInFlight()
{
    // Moving on some other node's walk doesn't count
    if (!walking || !bot->isMoving() || !BookedOnWalkSpot())
        return nullptr;

    return &walkSpot;
}

bool IccSgAddsWalkAction::WalkTo(Position const& spot, MovementPriority priority, bool interrupt)
{
    // A walk issued mid-cast never starts but still books its travel time, and the same spot is refused
    // until that runs out
    if (bot->IsMovementPreventedByCasting())
    {
        if (!interrupt)
            return false;

        bot->InterruptNonMeleeSpells(true);
    }

    // A stun or an item use stopped the last walk short, and its booking would refuse this one
    uint32 const now = getMSTime();
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    bool const ours = walking && BookedOnWalkSpot();
    if (ours && !bot->isMoving() && getMSTimeDiff(walkIssuedMs, now) > WALK_STALL_MS)
        last.clear();

    // A booking only yields to a higher priority, so our own forced walk would refuse its next leg
    bool const lowered = ours && bot->isMoving() && last.priority == MovementPriority::MOVEMENT_FORCED;
    if (lowered)
        last.priority = MovementPriority::MOVEMENT_COMBAT;

    walkSpot = spot;
    walking = true;
    walkIssuedMs = now;
    if (MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false, false,
               false, priority))
        return true;

    if (lowered)
        last.priority = MovementPriority::MOVEMENT_FORCED;

    return false;
}

bool IccSgAddsWalkAction::BookedOnWalkSpot()
{
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    return std::fabs(last.lastMoveToX - walkSpot.GetPositionX()) <= WALK_SPOT_TOLERANCE &&
           std::fabs(last.lastMoveToY - walkSpot.GetPositionY()) <= WALK_SPOT_TOLERANCE;
}

bool IccSgDrakeDodgeAction::Execute(Event /*event*/)
{
    if (!IsSgDrakeInHazard(botAI))
    {
        DropWalk();
        return false;
    }

    if (WalkInFlight())
        return true;

    Position spot;
    if (!FindSgDrakeDodgeSpot(botAI, spot))
        return false;

    // An Icy Blast lands for 5-6k, and its puddle slows by 75% while it ticks for 40 s
    return WalkTo(spot, MovementPriority::MOVEMENT_FORCED, true);
}

bool IccSgDrakeCastTankDefensiveAction::Execute(Event /*event*/)
{
    uint32 const spellId = GetSgDrakeTankDefensive(botAI, "sg.drake.defensive");
    return spellId && botAI->CastSpell(spellId, bot);
}

bool IccSgDrakeAntiFearAction::FearWindowActive() { return IsSgSpinestalkerFearWindow(botAI); }

bool IccSgDrakeBrakeStopAction::Execute(Event /*event*/)
{
    Creature* drake = GetSgDrakeBrakeTarget(botAI);
    if (!drake)
        return false;

    bool stopped = false;
    if (bot->GetVictim() == drake)
    {
        bot->AttackStop();
        stopped = true;
    }

    if (AI_VALUE(Unit*, "current target") == drake)
    {
        context->GetValue<Unit*>("current target")->Set(nullptr);
        stopped = true;
    }

    return stopped;
}

bool IccSgDrakeStandAction::Execute(Event /*event*/)
{
    Position spot;
    if (!GetSgDrakeStandWalk(botAI, spot))
    {
        DropWalk();
        return false;
    }

    if (Position const* inFlight = WalkInFlight())
        if (inFlight->GetExactDist2d(spot) <= WALK_SPOT_TOLERANCE)
            return true;

    return WalkTo(spot, MovementPriority::MOVEMENT_COMBAT);
}

bool IccSgGauntletDodgeAction::Execute(Event /*event*/)
{
    if (!IsSgGauntletInBlade(botAI))
    {
        DropWalk();
        return false;
    }

    if (WalkInFlight())
        return true;

    Position spot;
    if (!FindSgGauntletDodgeSpot(botAI, spot))
        return false;

    // The whirl hits every second for 6 s and comes back 9 s later, which costs more than any cast
    return WalkTo(spot, MovementPriority::MOVEMENT_FORCED, true);
}

bool IccSgGauntletTauntAction::Execute(Event /*event*/)
{
    Unit* add = GetSgGauntletTauntTarget(botAI);
    if (!add || !IccCastClassTaunt(bot, botAI, add))
        return false;

    // A taunt moves the add, not the bot's own target
    AttackDirect(add);
    return true;
}
