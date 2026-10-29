/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCActions_Sindragosa.h"

#include <cmath>
#include <string>

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
constexpr float NITRO_FROM = 12.0f;
}  // namespace

bool IccSindragosaBossWalkAction::WalkFor(bool (*getter)(PlayerbotAI*, Position&), MovementPriority priority,
                                          bool interrupt, bool nitro)
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

    // Nitro Boosts refuse a bot mid-cast, so they go after the walk drops it
    bool const walked = WalkTo(spot, priority, interrupt);
    if (walked && nitro && bot->GetExactDist2d(spot) > NITRO_FROM && EncounterHelpers::NitroBoostsReady(bot))
        EncounterHelpers::UseNitroBoosts(bot);

    return walked;
}

Position const* IccSindragosaBossWalkAction::WalkInFlight()
{
    // Moving on some other node's walk doesn't count
    if (!walking || !bot->isMoving() || !BookedOnWalkSpot())
        return nullptr;

    return &walkSpot;
}

bool IccSindragosaBossWalkAction::WalkTo(Position const& spot, MovementPriority priority, bool interrupt)
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

bool IccSindragosaBossWalkAction::BookedOnWalkSpot()
{
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    return std::fabs(last.lastMoveToX - walkSpot.GetPositionX()) <= WALK_SPOT_TOLERANCE &&
           std::fabs(last.lastMoveToY - walkSpot.GetPositionY()) <= WALK_SPOT_TOLERANCE;
}

bool IccSindragosaColdAction::Execute(Event /*event*/)
{
    // 30-60k frost, which costs more than any cast
    return WalkFor(&GetSgColdWalk, MovementPriority::MOVEMENT_FORCED, true, true);
}

bool IccSindragosaSplashAction::Execute(Event /*event*/)
{
    return WalkFor(&GetSgSplashWalk, MovementPriority::MOVEMENT_FORCED, true, false);
}

bool IccSindragosaBombHideAction::Execute(Event /*event*/)
{
    return WalkFor(&GetSgBombHideWalk, MovementPriority::MOVEMENT_FORCED, true, false);
}

bool IccSindragosaBeaconAction::Execute(Event /*event*/)
{
    // The tomb forms 7 s after the beacon, wherever the bot stands by then
    return WalkFor(&GetSgBeaconWalk, MovementPriority::MOVEMENT_FORCED, true, true);
}

bool IccSindragosaCastTankDefensiveAction::Execute(Event /*event*/)
{
    uint32 const spellId = GetSgTankDefensive(botAI, "sg.defensive");
    return spellId && botAI->CastSpell(spellId, bot);
}

bool IccSindragosaTauntAction::Execute(Event /*event*/)
{
    bool swap = false;
    Unit* boss = GetSgTauntTarget(botAI, swap);
    if (!boss || !IccCastClassTaunt(bot, botAI, boss))
        return false;

    if (swap)
        NoteSgSwapTaunt(botAI);

    // A taunt moves her, not the bot's own target
    AttackDirect(boss);
    return true;
}

bool IccSindragosaBuffetHideAction::Execute(Event /*event*/)
{
    return WalkFor(&GetSgBuffetHideWalk, MovementPriority::MOVEMENT_COMBAT, false, false);
}

bool IccSindragosaUnchainedAction::Execute(Event /*event*/)
{
    // A cast finished on the way out stacks Instability
    return WalkFor(&GetSgUnchainedWalk, MovementPriority::MOVEMENT_COMBAT, true, false);
}

bool IccSindragosaFreedomAction::Execute(Event /*event*/)
{
    Unit* target = GetSgFreedomTarget(botAI);
    return target && botAI->CastSpell("hand of freedom", target);
}

bool IccSindragosaBeaconHealAction::Execute(Event /*event*/)
{
    std::string spell;
    Unit* target = GetSgBeaconHealTarget(botAI, spell);
    return target && botAI->CastSpell(spell, target);
}

bool IccSindragosaHoldAction::Execute(Event /*event*/)
{
    uint8 const due = GetSgHoldDue(botAI);
    if (!due)
        return false;

    Creature* boss = GetSgBoss(botAI);
    Unit* victim = bot->GetVictim();
    Unit* target = AI_VALUE(Unit*, "current target");
    Spell* autoRepeat = bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL);
    Unit* shot = autoRepeat ? autoRepeat->m_targets.GetUnitTarget() : nullptr;

    auto const stops = [&](Unit* unit)
    {
        if (!unit)
            return false;

        return (due & SG_HOLD_ALL) || ((due & SG_HOLD_HER) && unit == boss) ||
               ((due & SG_HOLD_TOMB) && IsSgAtTombFloor(unit) && IsSgHeldTomb(botAI, unit));
    };

    bool acted = false;
    if (autoRepeat && ((due & SG_HOLD_ALL) || stops(shot)))
    {
        bot->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
        acted = true;
    }

    if (stops(victim))
    {
        bot->AttackStop();
        acted = true;
    }

    if (stops(target))
    {
        context->GetValue<Unit*>("current target")->Set(nullptr);
        acted = true;
    }

    // Every spell finished stacks Instability again
    if (due & SG_HOLD_CAST)
    {
        bot->InterruptNonMeleeSpells(false);
        acted = true;
    }

    return acted;
}

bool IccSindragosaStandAction::Execute(Event /*event*/)
{
    return WalkFor(&GetSgStandWalk, MovementPriority::MOVEMENT_COMBAT, false, false);
}
