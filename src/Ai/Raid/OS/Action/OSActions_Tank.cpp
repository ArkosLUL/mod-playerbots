/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OSActions.h"
#include "OSTriggers.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "RaidObs.h"
#include "Random.h"
#include "Timer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

using namespace OsHelpers;

bool OsMainTankHoldAction::Execute(Event /*event*/)
{
    Unit* boss = GetSartharion(bot);
    if (!boss)
        return false;

    if (bot->GetVictim() != boss)
        Attack(boss);
    else if (bot->GetTarget() != boss->GetGUID())
        bot->SetSelection(boss->GetGUID());

    float x = TankHoldX(bot);
    float y = SafeCorridorY(bot);

    if (!MainTankDragDone(boss))
    {
        uint32 const now = getMSTime();
        uint32 dragStartedMs = MainTankDragStartedMs(boss);
        if (!dragStartedMs)
        {
            dragStartedMs = now;
            SetMainTankDragStartedMs(boss, dragStartedMs);
        }

        uint32 arrivedMs = MainTankDragArrivedMs(boss);
        bool const onPoint =
            bot->GetExactDist2d(MAIN_TANK_DRAG_X, MAIN_TANK_DRAG_Y) <= MAIN_TANK_DRAG_TOLERANCE;

        if (!onPoint)
            arrivedMs = 0;
        else if (!arrivedMs)
            arrivedMs = now;

        SetMainTankDragArrivedMs(boss, arrivedMs);

        bool const dwellDone = arrivedMs && getMSTimeDiff(arrivedMs, now) >= MAIN_TANK_DRAG_DWELL_MS &&
                               boss->GetExactDist2d(bot) <= bot->GetMeleeRange(boss);
        bool const timedOut = getMSTimeDiff(dragStartedMs, now) > MAIN_TANK_DRAG_TIMEOUT_MS;

        if (RaidObs::Active())
        {
            char const* phase = "walking";
            if (dwellDone)
                phase = "settled";
            else if (timedOut)
                phase = "timeout";
            else if (onPoint)
                phase = "onpoint";

            RaidObs::NoteDerived(bot, "sartharion.drag", phase);
        }

        if (dwellDone || timedOut)
        {
            if (!dwellDone)
            {
                LOG_WARN("playerbots",
                         "Obsidian Sanctum: main tank gave up the pull drag at ({:.1f}, {:.1f}), "
                         "{:.1f}yd short of the corner",
                         bot->GetPositionX(), bot->GetPositionY(),
                         bot->GetExactDist2d(MAIN_TANK_DRAG_X, MAIN_TANK_DRAG_Y));
            }

            SetMainTankDragged(boss);
            return false;
        }

        if (onPoint)
            return false;

        return IssueMove(MAIN_TANK_DRAG_X, MAIN_TANK_DRAG_Y, MovementPriority::MOVEMENT_COMBAT, false);
    }

    ClampDestination(x, y);

    if (FissureBlocks(bot, x, y))
        return false;

    if (bot->GetExactDist2d(x, y) <= CorridorToleranceFor(bot))
        return false;

    return IssueMove(x, y, MovementPriority::MOVEMENT_COMBAT, false);
}

bool OsDrakeLandingPositionAction::Execute(Event /*event*/)
{
    Unit* drake = FindInboundDrake(bot);
    if (!drake)
        return false;

    Position const* landing = LandingPositionFor(drake->GetEntry());
    if (!landing)
        return false;

    Position const* spot = IsOffTank(bot) ? DrakeTankSpotFor(drake->GetEntry()) : nullptr;

    float x = spot ? spot->GetPositionX() : landing->GetPositionX() + OFFTANK_EAST_OFFSET;

    float y = spot ? spot->GetPositionY() : landing->GetPositionY();
    if (!WaveClearsY(y, ClassifyTsunamiWave(bot)))
        y = SafeCorridorY(bot);

    return MoveToClamped(x, y);
}

bool OsOffTankHoldAction::Execute(Event /*event*/)
{
    if (Unit* stray = OffTankTauntTarget(bot))
    {
        char const* taunt = TauntSpellFor(bot);
        if (taunt && botAI->CanCastSpell(taunt, stray) && botAI->CastSpell(taunt, stray))
            return true;
    }

    Unit* primary = OffTankChargeFor(bot);

    if (primary)
    {
        if (bot->GetVictim() != primary)
            Attack(primary);
        else if (bot->GetTarget() != primary->GetGUID())
            bot->SetSelection(primary->GetGUID());
    }
    else
    {
        Unit* boss = GetSartharion(bot);
        if (boss && (bot->GetVictim() == boss || AI_VALUE(Unit*, "current target") == boss))
        {
            bot->AttackStop();
            context->GetValue<Unit*>("current target")->Set(nullptr);
        }
    }

    Position const anchor = OffTankAnchor(bot);
    float x = anchor.GetPositionX();
    float y = anchor.GetPositionY();
    ClampDestination(x, y);

    if (FissureBlocks(bot, x, y))
        return false;

    if (bot->GetExactDist2d(x, y) <= CORRIDOR_ARRIVAL_TOLERANCE)
        return false;

    return IssueMove(x, y, MovementPriority::MOVEMENT_COMBAT, false);
}

bool OsTankShapeshiftAction::Execute(Event /*event*/)
{
    for (char const* form : { "dire bear form", "bear form" })
    {
        if (botAI->CanCastSpell(form, bot) && botAI->CastSpell(form, bot))
            return true;
    }
    return false;
}

bool OsMainTankCooldownAction::Execute(Event /*event*/)
{
    uint32 const spell = NextTankDefensive(botAI, bot, "sartharion.defensive");
    return spell && botAI->CastSpell(spell, bot);
}
