/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */


#include "OSActions.h"
#include "OSTriggers.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "Random.h"
#include "Timer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

using namespace OsHelpers;

// Main tank: the pull drag, the corridor holds, the cooldown order and the druid form guard.
// Off-tank: his own hold and the drake landing intercept.

bool OsMainTankHoldAction::Execute(Event /*event*/)
{
    Unit* boss = GetSartharion(bot);
    if (!boss)
        return false;

    // Gated on the victim rather than on the "current target" AI value. That value can never hold a
    // friendly - AttackAction::Attack bails on IsFriendlyTo long before it writes it - but the
    // client-side selection is overwritten by every buff the tank casts on a raid member, and keying
    // off the AI value means the boss is never re-selected afterwards. Acquiring must not cost the
    // tick either, so this falls through to the move rather than returning.
    if (bot->GetVictim() != boss)
        Attack(boss);
    else if (bot->GetTarget() != boss->GetGUID())
        bot->SetSelection(boss->GetGUID());

    // The tank walks Sartharion off his home spot down to the entrance end and holds him there,
    // dodging along Y with everyone else. He follows on threat, and there is no leash to hit:
    // Creature::_IsTargetAcceptable skips the home-distance check outright inside a dungeon.
    float x = TankHoldX(bot);
    float y = SafeCorridorY(bot);

    if (!MainTankDragDone(boss))
    {
        // The one-shot pull drag into the southern tip. Sartharion stops chasing 20.83yd short of the
        // tank, so holding a corridor from the start parks him wherever that ray happens to end,
        // aimed diagonally through the raid. Overshooting to the corner once puts him at
        // (3228.5, 504.7), inside his reach from both corridor holds.
        //
        // Deliberately not clamped: this is a hand-measured position past the south edge of the
        // platform box, and it is already inside the Range Marker radius, so both clamps would only
        // damage it.
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

        // Two ways out. The normal one is the full dwell with the boss in melee range - the tank gets
        // to the corner in about 3s and the boss needs 7-9s to cross the room, so arrival on its own
        // proves nothing, and neither does the dwell if a player is holding threat and the boss never
        // came. The timeout is the bail for exactly that case, and waiting past it is worse than an
        // imperfect facing: waves start at 20s and the corner is lethal under a left one.
        bool const dwellDone = arrivedMs && getMSTimeDiff(arrivedMs, now) >= MAIN_TANK_DRAG_DWELL_MS &&
                               boss->GetExactDist2d(bot) <= bot->GetMeleeRange(boss);
        bool const timedOut = getMSTimeDiff(dragStartedMs, now) > MAIN_TANK_DRAG_TIMEOUT_MS;

        if (dwellDone || timedOut)
        {
            // The corner is a hand-measured position past where the navmesh floor starts, so the drag
            // failing and MoveTo silently refusing the destination look identical from in game. One
            // line, once per pull, tells them apart.
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

    // The off-tank waits on the spot he will hold the drake from, so it faces the right way from the
    // moment it touches down rather than being crossed over afterwards. Melee meet it east of the
    // touchdown point and never west: a drake stops at melee range of whoever it is coming for and
    // faces them, so arriving from the east aims the 15yd Shadow Breath cone at empty platform instead
    // of through the raid. They then get arced off the front by "rear flank" once it is their target.
    Position const* spot = IsOffTank(bot) ? DrakeTankSpotFor(drake->GetEntry()) : nullptr;

    float x = spot ? spot->GetPositionX() : landing->GetPositionX() + OFFTANK_EAST_OFFSET;

    // Every landing coord, and every tank spot, sits inside a lethal band under one wave pattern or
    // both, so a wave that can reach this Y overrides it outright.
    float y = spot ? spot->GetPositionY() : landing->GetPositionY();
    if (!WaveClearsY(y, ClassifyTsunamiWave(bot)))
        y = SafeCorridorY(bot);

    return MoveToClamped(x, y);
}

bool OsOffTankHoldAction::Execute(Event /*event*/)
{
    // Ahead of everything else, because the one tick this is in range is the tick he sets off: he
    // attacks the newest drake only, and a handover walks him 35.5yd away from the one he was holding.
    if (Unit* stray = OffTankTauntTarget(bot))
    {
        char const* taunt = TauntSpellFor(bot);
        if (taunt && botAI->CanCastSpell(taunt, stray) && botAI->CastSpell(taunt, stray))
            return true;
    }

    Unit* primary = OffTankChargeFor(bot);

    if (primary)
    {
        // Acquiring the target must not cost the tick. Returning here is what let the off-tank chase
        // a drake to wherever it stood - including the perch it sits on before it is called - instead
        // of taunting it from the anchor and letting it come.
        if (bot->GetVictim() != primary)
            Attack(primary);
        else if (bot->GetTarget() != primary->GetGUID())
            bot->SetSelection(primary->GetGUID());
    }
    else
    {
        // Nothing to hold, so let go of Sartharion. The multiplier blocks re-acquiring him but cannot
        // drop a target already picked up, and AttackStop on its own is not enough: it clears the swing
        // and leaves "current target" set, which is what the class rotation casts at and what
        // ReachTargetAction walks to. He picks the boss up in the first place because every trigger
        // here needs him in combat, so the pull itself still runs on generic tank behaviour.
        Unit* boss = GetSartharion(bot);
        if (boss && (bot->GetVictim() == boss || AI_VALUE(Unit*, "current target") == boss))
        {
            bot->AttackStop();
            context->GetValue<Unit*>("current target")->Set(nullptr);
        }
    }

    // A drake is held beside where it lands, a Lava Blaze or a whelp is dragged to the east end past
    // the raid, and with nothing at all he waits on Sartharion's empty spawn spot - the east end is a
    // few yards off the rim and there is no reason to stand there through the first 20s, or through
    // any gap between adds.
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
    // Dire bear first: it is the same form with more armour, and the class strategy falls back the
    // same way for a druid who has not learned it.
    for (char const* form : { "dire bear form", "bear form" })
    {
        if (botAI->CanCastSpell(form, bot) && botAI->CastSpell(form, bot))
            return true;
    }
    return false;
}

bool OsMainTankCooldownAction::Execute(Event /*event*/)
{
    char const* spell = NextTankDefensive(botAI, bot);
    return spell && botAI->CastSpell(spell, bot);
}
