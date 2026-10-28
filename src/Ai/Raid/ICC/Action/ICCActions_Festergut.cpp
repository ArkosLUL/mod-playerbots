/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCActions_Festergut.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Creature.h"
#include "ICCHelpers_Shared.h"
#include "ICCSeams.h"
#include "LastMovementValue.h"
#include "Playerbots.h"
#include "RaidDefensive.h"
#include "Timer.h"

namespace
{
constexpr float WALK_SPOT_TOLERANCE = 0.5f;
// A spline issued last tick may not show in isMoving yet
constexpr uint32 WALK_STALL_MS = 500;
}  // namespace

Position const* IccFestergutWalkAction::WalkInFlight()
{
    // Moving on some other node's walk doesn't count
    if (!walking || !bot->isMoving() || !BookedOnWalkSpot())
        return nullptr;

    return &walkSpot;
}

bool IccFestergutWalkAction::WalkTo(Position const& spot, MovementPriority priority)
{
    // A walk issued mid-cast never starts but still books its travel time, and the same spot is refused
    // until that runs out
    if (bot->IsMovementPreventedByCasting())
        return false;

    // A stun or a channel stopped the last walk short, and its booking would refuse this one
    uint32 const now = getMSTime();
    if (walking && !bot->isMoving() && getMSTimeDiff(walkIssuedMs, now) > WALK_STALL_MS && BookedOnWalkSpot())
        AI_VALUE(LastMovement&, "last movement").clear();

    walkSpot = spot;
    walking = true;
    walkIssuedMs = now;
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false, false,
                  false, priority);
}

bool IccFestergutWalkAction::BookedOnWalkSpot()
{
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    return std::fabs(last.lastMoveToX - walkSpot.GetPositionX()) <= WALK_SPOT_TOLERANCE &&
           std::fabs(last.lastMoveToY - walkSpot.GetPositionY()) <= WALK_SPOT_TOLERANCE;
}

bool IccFestergutDodgeGooAction::Execute(Event /*event*/)
{
    std::vector<Position> impacts;
    if (!IsFgInGoo(botAI) || !GetFgGoo(botAI, impacts))
    {
        DropWalk();
        return false;
    }

    if (!botAI->CanMove())
        return false;

    Position const* inFlight = WalkInFlight();
    if (inFlight && std::none_of(impacts.begin(), impacts.end(), [inFlight](Position const& impact)
                                 { return impact.GetExactDist2d(*inFlight) < FG_GOO_CLEARANCE; }))
        return true;

    Position spot;
    if (!GetFgGooDodge(botAI, spot))
        return false;

    // 15k and a 20 s slow to swings and casts: the cast is worth less than the step out
    if (bot->IsMovementPreventedByCasting())
        bot->InterruptNonMeleeSpells(true);

    // Our own walk still holds the FORCED booking, and a booking only yields to a higher priority
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    bool const lowered = inFlight && last.priority == MovementPriority::MOVEMENT_FORCED;
    if (lowered)
        last.priority = MovementPriority::MOVEMENT_COMBAT;

    if (WalkTo(spot, MovementPriority::MOVEMENT_FORCED))
        return true;

    if (lowered)
        last.priority = MovementPriority::MOVEMENT_FORCED;

    return false;
}

bool IccFestergutCastPungentDefensiveAction::Execute(Event /*event*/)
{
    RaidPredictedHit hit;
    if (!GetFgPungentHit(botAI, hit, true))
        return false;

    RaidPersonalPick const pick = GetRaidPersonalDefensive(botAI, ICC_DEFENSIVE_SCOPE, &hit);
    if (!pick)
        return false;

    if (bot->IsNonMeleeSpellCast(false))
        bot->InterruptNonMeleeSpells(false);

    return pick.consumable ? botAI->DoSpecificAction(pick.consumable, Event(), true)
                           : botAI->CastSpell(pick.spellId, bot);
}

bool IccFestergutCastPungentCoverAction::Execute(Event /*event*/)
{
    RaidCover const cover = GetFgPungentCover(botAI);
    uint32 const spellId = RaidCoverSpellId(botAI, cover);
    if (!spellId)
        return false;

    // A cast in progress fails this one's check, and a moving bot breaks its own channel
    if (bot->IsNonMeleeSpellCast(false))
        bot->InterruptNonMeleeSpells(false);

    if (IsRaidCoverChannel(cover) && bot->isMoving())
        bot->StopMoving();

    if (!botAI->CastSpell(spellId, bot))
        return false;

    FinishRaidCover(botAI, cover);
    return true;
}

bool IccFestergutCastTankDefensiveAction::Execute(Event /*event*/)
{
    uint32 const spellId = GetFgTankDefensive(botAI, "fg.defensive");
    if (!spellId || !botAI->CastSpell(spellId, bot))
        return false;

    LatchFgTankDefensive(botAI);
    return true;
}

bool IccFestergutBloatSwapAction::Execute(Event /*event*/)
{
    Creature* boss = GetFgBoss(botAI);
    if (!boss || !IsFgSwapTaunter(botAI) || !IccCastClassTaunt(bot, botAI, boss))
        return false;

    AttackDirect(boss);
    return true;
}

bool IccFestergutBrakeStopAction::Execute(Event /*event*/)
{
    Creature* boss = GetFgBoss(botAI);
    if (!boss || !IsFgBraked(botAI))
        return false;

    if (bot->GetVictim() == boss)
        bot->AttackStop();

    if (AI_VALUE(Unit*, "current target") == boss)
        context->GetValue<Unit*>("current target")->Set(nullptr);

    return true;
}

bool IccFestergutStandAction::Execute(Event /*event*/)
{
    Position spot;
    if (!GetFgStandWalk(botAI, spot))
    {
        DropWalk();
        return false;
    }

    if (Position const* inFlight = WalkInFlight())
        if (inFlight->GetExactDist2d(spot) <= WALK_SPOT_TOLERANCE)
            return true;

    return WalkTo(spot, MovementPriority::MOVEMENT_COMBAT);
}
