/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCActions_Saurfang.h"

#include <cmath>

#include "Creature.h"
#include "ICCHelpers_Shared.h"
#include "LastMovementValue.h"
#include "Playerbots.h"
#include "RaidCombatHelpers.h"
#include "Timer.h"

namespace
{
constexpr float WALK_SPOT_TOLERANCE = 0.5f;
// A spline issued last tick may not show in isMoving yet
constexpr uint32 WALK_STALL_MS = 500;
}  // namespace

bool IccDbsRuneSwapAction::Execute(Event /*event*/)
{
    Creature* boss = GetDbsBoss(botAI);
    if (!boss || !IsDbsSwapTaunter(botAI) || !IccCastClassTaunt(bot, botAI, boss))
        return false;

    AttackDirect(boss);
    return true;
}

bool IccDbsRuneYieldAction::Execute(Event /*event*/)
{
    Creature* boss = GetDbsBoss(botAI);
    if (!boss)
        return false;

    if (bot->GetVictim() == boss)
        bot->AttackStop();

    if (AI_VALUE(Unit*, "current target") == boss)
        context->GetValue<Unit*>("current target")->Set(nullptr);

    return true;
}

bool IccDbsBeastTauntAction::Execute(Event /*event*/)
{
    Unit* beast = GetDbsBeastToTaunt(botAI);
    if (!beast || !IccCastClassTaunt(bot, botAI, beast))
        return false;

    // A taunt moves the beast, not the bot's own target
    AttackDirect(beast);
    return true;
}

bool IccDbsBeastControlAction::Execute(Event /*event*/)
{
    char const* spell = nullptr;
    Unit* beast = GetDbsControlDuty(botAI, spell);
    return beast && spell && botAI->CastSpell(spell, beast);
}

bool IccDbsMarkHealAction::Execute(Event /*event*/)
{
    bool beacon = false;
    Player* mark = GetDbsMarkWork(botAI, beacon);
    if (!mark)
        return false;

    if (beacon)
        return botAI->CastSpell("beacon of light", mark);

    return CastRaidThroughputHeal(botAI, mark);
}

bool IccDbsStandAction::Execute(Event /*event*/)
{
    Position spot;
    if (!GetDbsStandWalk(botAI, spot))
    {
        walking = false;
        return false;
    }

    // Moving on some other node's walk doesn't count
    if (walking && bot->isMoving() && walkSpot.GetExactDist2d(spot) <= WALK_SPOT_TOLERANCE && BookedOn(spot))
        return true;

    // A walk issued mid-cast never starts but still books its travel time
    if (bot->IsMovementPreventedByCasting())
        return false;

    // A stun or an item use stopped the last walk short, and its booking would refuse the same spot
    uint32 const now = getMSTime();
    if (walking && !bot->isMoving() && getMSTimeDiff(walkIssuedMs, now) > WALK_STALL_MS && BookedOn(spot))
        AI_VALUE(LastMovement&, "last movement").clear();

    walkSpot = spot;
    walking = true;
    walkIssuedMs = now;
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false, false,
                  false, MovementPriority::MOVEMENT_COMBAT);
}

bool IccDbsStandAction::BookedOn(Position const& spot)
{
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    return std::fabs(last.lastMoveToX - spot.GetPositionX()) <= WALK_SPOT_TOLERANCE &&
           std::fabs(last.lastMoveToY - spot.GetPositionY()) <= WALK_SPOT_TOLERANCE;
}
