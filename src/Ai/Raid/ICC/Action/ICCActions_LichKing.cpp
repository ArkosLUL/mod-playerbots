/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCActions_LichKing.h"

#include <cmath>

#include "Creature.h"
#include "EncounterHelpers.h"
#include "ICCHelpers_Shared.h"
#include "Playerbots.h"
#include "Spell.h"
#include "Timer.h"

namespace
{
constexpr float WALK_SPOT_TOLERANCE = 0.5f;
// A spline issued last tick may not show in isMoving yet
constexpr uint32 WALK_STALL_MS = 500;
// Nitro Boosts for a walk this long
constexpr float NITRO_FROM = 15.0f;
constexpr float FM_FLOOR_REACH = 50.0f;
}  // namespace

bool IccLkWalkAction::WalkFor(bool (*getter)(PlayerbotAI*, Position&), MovementPriority priority, bool interrupt,
                              bool nitro)
{
    Position spot;
    if (!getter(botAI, spot))
    {
        walking = false;
        return false;
    }

    if (Position const* inFlight = WalkInFlight())
        if (inFlight->GetExactDist2d(spot) <= WALK_SPOT_TOLERANCE)
            return true;

    // boots after the walk, they refuse a bot mid-cast and the walk is what drops the cast
    bool const walked = WalkTo(spot, priority, interrupt);
    if (walked && nitro && bot->GetExactDist2d(spot) > NITRO_FROM && EncounterHelpers::NitroBoostsReady(bot))
        EncounterHelpers::UseNitroBoosts(bot);

    return walked;
}

Position const* IccLkWalkAction::WalkInFlight()
{
    // Moving on some other node's walk doesn't count
    if (!walking || !bot->isMoving() || !BookedOnWalkSpot())
        return nullptr;

    return &walkSpot;
}

bool IccLkWalkAction::WalkTo(Position const& spot, MovementPriority priority, bool interrupt)
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
    // Neither floor is on the navmesh, so the path search refuses any spot the gameobject collision doesn't
    // answer for: a straight line at the floor's own height, which every walk here is planned as
    float const z = spot.GetExactDist2d(LK_FROSTMOURNE) <= FM_FLOOR_REACH ? LK_FROSTMOURNE.GetPositionZ()
                                                                          : LK_CENTER.GetPositionZ();
    if (MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), z, false, false, false, true, priority))
        return true;

    if (lowered)
        last.priority = MovementPriority::MOVEMENT_FORCED;

    return false;
}

bool IccLkWalkAction::BookedOnWalkSpot()
{
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    return std::fabs(last.lastMoveToX - walkSpot.GetPositionX()) <= WALK_SPOT_TOLERANCE &&
           std::fabs(last.lastMoveToY - walkSpot.GetPositionY()) <= WALK_SPOT_TOLERANCE;
}

bool IccLichKingQuakeAction::Execute(Event /*event*/)
{
    // The ring shatters 5 s after the 1.5 s cast
    return WalkFor(&GetLkQuakeWalk, MovementPriority::MOVEMENT_FORCED, true, true);
}

bool IccLichKingShadowTrapAction::Execute(Event /*event*/)
{
    // The trap arms 5 s after it lands, and its knockback throws everyone within 10 yd off the platform
    return WalkFor(&GetLkTrapWalk, MovementPriority::MOVEMENT_FORCED, true, false);
}

bool IccLichKingCastTankDefensiveAction::Execute(Event /*event*/)
{
    uint32 const spellId = GetLkTankDefensive(botAI, "lk.defensive");
    return spellId && botAI->CastSpell(spellId, bot);
}

bool IccLichKingTauntAction::Execute(Event /*event*/)
{
    uint32 aoeSpell = 0;
    Unit* unit = GetLkTauntTarget(botAI, aoeSpell);
    if (!unit)
        return false;

    bool const cast = aoeSpell ? botAI->CastSpell(aoeSpell, bot) : IccCastClassTaunt(bot, botAI, unit);
    if (!cast)
        return false;

    AttackDirect(unit);
    return true;
}

bool IccLichKingPlagueCureAction::Execute(Event /*event*/)
{
    uint32 spellId = 0;
    Unit* carrier = GetLkCureTarget(botAI, spellId);
    return carrier && botAI->CastSpell(spellId, carrier);
}

bool IccLichKingPlagueWalkAction::Execute(Event /*event*/)
{
    // First tick 5 s after the plague lands, no time to finish a cast first
    return WalkFor(&GetLkPlagueWalk, MovementPriority::MOVEMENT_FORCED, true, true);
}

bool IccLichKingTranquilizeAction::Execute(Event /*event*/)
{
    Unit* add = GetLkTranquilizeTarget(botAI);
    return add && botAI->CastSpell("tranquilizing shot", add);
}

bool IccLichKingStunAction::Execute(Event /*event*/)
{
    uint32 spellId = 0;
    Unit* add = GetLkStunTarget(botAI, spellId);
    return add && LkCastControl(botAI, add, spellId);
}

bool IccLichKingSilenceDispelAction::Execute(Event /*event*/)
{
    uint32 spellId = 0;
    Unit* member = GetLkSilenceDispelTarget(botAI, spellId);
    return member && botAI->CastSpell(spellId, member);
}

bool IccLichKingTotemAction::Execute(Event /*event*/)
{
    char const* totem = GetLkTotemSwap(botAI);
    return totem && botAI->CastSpell(totem, bot);
}

bool IccLichKingBrakeStopAction::Execute(Event /*event*/)
{
    Creature* boss = IsLkBraked(botAI) ? GetLkBoss(botAI) : nullptr;
    if (!boss)
        return false;

    bool stopped = false;
    if (bot->GetVictim() == boss)
    {
        bot->AttackStop();
        stopped = true;
    }

    Spell* autoRepeat = bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL);
    if (autoRepeat && autoRepeat->m_targets.GetUnitTarget() == boss)
    {
        bot->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
        stopped = true;
    }

    if (AI_VALUE(Unit*, "current target") == boss)
    {
        context->GetValue<Unit*>("current target")->Set(nullptr);
        stopped = true;
    }

    return stopped;
}

bool IccLichKingStandAction::Execute(Event /*event*/)
{
    // Winter slots race Remorseless Winter's first ticks, phase one stands don't
    bool const urgent = IsLkStandUrgent(botAI);
    return WalkFor(&GetLkStandWalk, urgent ? MovementPriority::MOVEMENT_FORCED : MovementPriority::MOVEMENT_COMBAT,
                   false, urgent);
}
