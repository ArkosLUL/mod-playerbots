/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCActions_BloodQueen.h"

#include <cmath>

#include "Creature.h"
#include "EncounterHelpers.h"
#include "ICCHelpers_Shared.h"
#include "ICCSeams.h"
#include "LastMovementValue.h"
#include "Playerbots.h"
#include "RaidDefensive.h"
#include "SpellMgr.h"
#include "Timer.h"

namespace
{
constexpr float WALK_SPOT_TOLERANCE = 0.5f;
constexpr uint32 WALK_STALL_MS = 500;
constexpr float PLAYER_REAIM = 1.5f;
constexpr float BITE_REACH = 3.0f;
constexpr float PACT_NITRO_FROM = 12.0f;
}

Position const* IccBqlWalkAction::WalkInFlight()
{
    if (!walking || !bot->isMoving() || !BookedOnWalkSpot())
        return nullptr;

    return &walkSpot;
}

bool IccBqlWalkAction::WalkTo(Position const& spot, MovementPriority priority, bool interrupt)
{
    if (bot->IsMovementPreventedByCasting())
    {
        if (!interrupt)
            return false;

        bot->InterruptNonMeleeSpells(true);
    }

    uint32 const now = getMSTime();
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    bool const ours = walking && BookedOnWalkSpot();
    if (ours && !bot->isMoving() && getMSTimeDiff(walkIssuedMs, now) > WALK_STALL_MS)
        last.clear();

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

Position IccBqlWalkAction::NextLeg(Position const& goal)
{
    Position const next = GetBqlWalkVia(botAI, goal, detouring ? &detour : nullptr);
    detouring = next.GetExactDist2d(goal) > WALK_SPOT_TOLERANCE;
    if (detouring)
        detour = next;

    return next;
}

bool IccBqlWalkAction::BookedOnWalkSpot()
{
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    return std::fabs(last.lastMoveToX - walkSpot.GetPositionX()) <= WALK_SPOT_TOLERANCE &&
           std::fabs(last.lastMoveToY - walkSpot.GetPositionY()) <= WALK_SPOT_TOLERANCE;
}

bool IccBqlDodgeAction::Execute(Event /*event*/)
{
    if (!IsBqlInShadow(botAI))
    {
        DropWalk();
        return false;
    }

    if (WalkInFlight())
        return true;

    Position spot;
    if (!FindBqlDodgeSpot(botAI, spot))
        return false;

    return WalkTo(spot, MovementPriority::MOVEMENT_FORCED, true);
}

bool IccBqlBiteAction::Execute(Event /*event*/)
{
    Player* target = GetBqlBiteTarget(botAI);
    if (!target)
    {
        DropWalk();
        return false;
    }

    if (botAI->HasAura("ice block", bot))
        botAI->RemoveAura("ice block");

    if (bot->GetExactDist2d(target) > BITE_REACH)
    {
        Position const leg = NextLeg(target->GetPosition());
        float const reaim = leg.GetExactDist2d(target) <= WALK_SPOT_TOLERANCE ? PLAYER_REAIM : WALK_SPOT_TOLERANCE;
        if (Position const* inFlight = WalkInFlight())
            if (inFlight->GetExactDist2d(leg) <= reaim)
                return true;

        return WalkTo(leg, MovementPriority::MOVEMENT_FORCED, true);
    }

    DropWalk();
    uint32 const bite = sSpellMgr->GetSpellIdForDifficulty(SPELL_BQL_VAMPIRIC_BITE, bot);
    return botAI->CanCastSpell(bite, target, false) && botAI->CastSpell(bite, target);
}

bool IccBqlShadowsAction::Execute(Event /*event*/)
{
    Position spot;
    if (!GetBqlShadowsWalk(botAI, spot))
    {
        DropWalk();
        return false;
    }

    if (Position const* inFlight = WalkInFlight())
        if (inFlight->GetExactDist2d(spot) <= WALK_SPOT_TOLERANCE)
            return true;

    return WalkTo(spot, MovementPriority::MOVEMENT_FORCED, true);
}

bool IccBqlPactAction::Execute(Event /*event*/)
{
    Position spot;
    if (!GetBqlPactWalk(botAI, spot))
    {
        DropWalk();
        return false;
    }

    Position const leg = NextLeg(spot);
    float const reaim = leg.GetExactDist2d(spot) <= WALK_SPOT_TOLERANCE ? PLAYER_REAIM : WALK_SPOT_TOLERANCE;
    if (Position const* inFlight = WalkInFlight())
        if (inFlight->GetExactDist2d(leg) <= reaim)
            return true;

    // boots after the walk, they refuse a bot mid-cast and the walk is what drops the cast
    bool const walked = WalkTo(leg, MovementPriority::MOVEMENT_FORCED, true);
    if (walked && bot->GetExactDist2d(spot) > PACT_NITRO_FROM && EncounterHelpers::NitroBoostsReady(bot))
        EncounterHelpers::UseNitroBoosts(bot);

    return walked;
}

bool IccBqlCastWhirlDefensiveAction::Execute(Event /*event*/)
{
    RaidPredictedHit hit;
    if (IsBqlBiting(botAI) || !GetBqlWhirlHit(botAI, hit))
        return false;

    RaidPersonalPick const pick = GetRaidPersonalDefensive(botAI, ICC_DEFENSIVE_SCOPE, &hit);
    if (!pick)
        return false;

    if (bot->IsNonMeleeSpellCast(false))
        bot->InterruptNonMeleeSpells(false);

    return pick.consumable ? botAI->DoSpecificAction(pick.consumable, Event(), true)
                           : botAI->CastSpell(pick.spellId, bot);
}

bool IccBqlCastWhirlCoverAction::Execute(Event /*event*/)
{
    RaidCover const cover = GetBqlWhirlCover(botAI);
    uint32 const spellId = RaidCoverSpellId(botAI, cover);
    if (!spellId)
        return false;

    if (bot->IsNonMeleeSpellCast(false))
        bot->InterruptNonMeleeSpells(false);

    if (IsRaidCoverChannel(cover) && bot->isMoving())
        bot->StopMoving();

    if (!botAI->CastSpell(spellId, bot))
        return false;

    FinishRaidCover(botAI, cover);
    return true;
}

bool IccBqlCastTankDefensiveAction::Execute(Event /*event*/)
{
    uint32 const spellId = GetBqlTankDefensive(botAI, "bql.defensive");
    return spellId && botAI->CastSpell(spellId, bot);
}

bool IccBqlAntiFearAction::Execute(Event event)
{
    if (IsBqlSelfWardDue(botAI))
        return botAI->CastSpell("fear ward", bot);

    return RaidAntiFearAction::Execute(event);
}

bool IccBqlAntiFearAction::isUseful() { return IsBqlSelfWardDue(botAI) || RaidAntiFearAction::isUseful(); }

bool IccBqlAntiFearAction::FearWindowActive() { return IsBqlFearWindow(botAI); }

bool IccBqlMassDispelAction::Execute(Event /*event*/)
{
    Player* centre = GetBqlMassDispelTarget(botAI);
    if (!centre)
        return false;

    if (bot->isMoving())
        bot->StopMoving();

    return botAI->CastSpell("mass dispel", centre);
}

bool IccBqlFrenzyCcAction::Execute(Event /*event*/)
{
    uint32 spellId = 0;
    Unit* victim = GetBqlFrenzyCc(botAI, spellId);
    return victim && botAI->CastSpell(spellId, victim);
}

bool IccBqlTauntAction::Execute(Event /*event*/)
{
    Unit* queen = GetBqlTauntTarget(botAI);
    if (!queen || !IccCastClassTaunt(bot, botAI, queen))
        return false;

    AttackDirect(queen);
    return true;
}

bool IccBqlBrakeStopAction::Execute(Event /*event*/)
{
    Creature* queen = GetBqlQueen(botAI);
    if (!queen || !IsBqlBraked(botAI))
        return false;

    bool stopped = false;
    if (bot->GetVictim() == queen)
    {
        bot->AttackStop();
        stopped = true;
    }

    if (AI_VALUE(Unit*, "current target") == queen)
    {
        context->GetValue<Unit*>("current target")->Set(nullptr);
        stopped = true;
    }

    return stopped;
}

bool IccBqlStandAction::Execute(Event /*event*/)
{
    Position spot;
    if (!GetBqlStandWalk(botAI, spot))
    {
        DropWalk();
        return false;
    }

    if (Position const* inFlight = WalkInFlight())
        if (inFlight->GetExactDist2d(spot) <= WALK_SPOT_TOLERANCE)
            return true;

    return WalkTo(spot, MovementPriority::MOVEMENT_COMBAT);
}
