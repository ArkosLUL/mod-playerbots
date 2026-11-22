/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCActions_Svalna.h"

#include <cmath>

#include "Creature.h"
#include "ICCHelpers_Shared.h"
#include "Item.h"
#include "LastMovementValue.h"
#include "Playerbots.h"
#include "Timer.h"

namespace
{
constexpr float HURL_WALK_TO = 18.0f;
constexpr float HURL_RANGE = 24.0f;

constexpr float WALK_SPOT_TOLERANCE = 0.5f;
constexpr uint32 WALK_STALL_MS = 500;
}

bool IccSvalnaSpearAction::Execute(Event /*event*/)
{
    Creature* spear = GetSsSpearToPull(botAI);
    if (!spear)
        return false;

    if (bot->IsMovementPreventedByCasting())
        bot->InterruptNonMeleeSpells(true);

    if (!spear->IsWithinDistInMap(bot, INTERACTION_DISTANCE))
        return MoveTo(spear, 1.0f, MovementPriority::MOVEMENT_FORCED);

    botAI->RemoveShapeshift();
    if (bot->IsNonMeleeSpellCast(false))
        bot->InterruptNonMeleeSpells(false);

    bot->StopMoving();
    spear->HandleSpellClick(bot);
    return true;
}

bool IccSvalnaHurlAction::Execute(Event /*event*/)
{
    Item* spear = GetSsSpearToThrow(botAI);
    Creature* svalna = GetSsSvalna(botAI);
    if (!spear || !svalna)
        return false;

    if (bot->GetExactDist(svalna) > HURL_RANGE || !bot->IsWithinLOSInMap(svalna))
        return MoveTo(svalna, HURL_WALK_TO, MovementPriority::MOVEMENT_COMBAT);

    botAI->RemoveShapeshift();
    if (bot->IsNonMeleeSpellCast(false))
        bot->InterruptNonMeleeSpells(false);

    botAI->ImbueItem(spear, svalna);
    return true;
}

bool IccSvalnaInterruptAction::Execute(Event /*event*/)
{
    char const* spell = nullptr;
    Unit* arnath = GetSsInterrupt(botAI, spell);
    return arnath && spell && botAI->CastSpell(spell, arnath);
}

bool IccSvalnaCastTankDefensiveAction::Execute(Event /*event*/)
{
    uint32 const spellId = GetSsTankDefensive(botAI, "ss.defensive");
    return spellId && botAI->CastSpell(spellId, bot);
}

bool IccSvalnaTauntAction::Execute(Event /*event*/)
{
    Unit* captain = GetSsCaptainToTaunt(botAI);
    if (!captain || !IccCastClassTaunt(bot, botAI, captain))
        return false;

    AttackDirect(captain);
    return true;
}

Position const* IccSsWalkAction::WalkInFlight()
{
    if (!walking || !bot->isMoving() || !BookedOnWalkSpot())
        return nullptr;

    return &walkSpot;
}

bool IccSsWalkAction::WalkTo(Position const& spot, MovementPriority priority)
{
    if (bot->IsMovementPreventedByCasting())
        bot->InterruptNonMeleeSpells(true);

    uint32 const now = getMSTime();
    if (walking && !bot->isMoving() && getMSTimeDiff(walkIssuedMs, now) > WALK_STALL_MS && BookedOnWalkSpot())
        AI_VALUE(LastMovement&, "last movement").clear();

    walkSpot = spot;
    walking = true;
    walkIssuedMs = now;
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false, false,
                  false, priority);
}

bool IccSsWalkAction::BookedOnWalkSpot()
{
    LastMovement& last = AI_VALUE(LastMovement&, "last movement");
    return std::fabs(last.lastMoveToX - walkSpot.GetPositionX()) <= WALK_SPOT_TOLERANCE &&
           std::fabs(last.lastMoveToY - walkSpot.GetPositionY()) <= WALK_SPOT_TOLERANCE;
}

bool IccSvalnaGauntletDodgeAction::Execute(Event /*event*/)
{
    if (!IsSsGauntletInDanger(botAI))
    {
        DropWalk();
        return false;
    }

    if (WalkInFlight())
        return true;

    Position spot;
    if (!FindSsGauntletDodgeSpot(botAI, spot))
        return false;

    return WalkTo(spot, MovementPriority::MOVEMENT_FORCED);
}

bool IccSvalnaGauntletInterruptAction::Execute(Event /*event*/)
{
    char const* spell = nullptr;
    Unit* caster = GetSsGauntletInterrupt(botAI, spell);
    return caster && spell && botAI->CastSpell(spell, caster);
}

bool IccSvalnaGauntletCastTankDefensiveAction::Execute(Event /*event*/)
{
    uint32 const spellId = GetSsGauntletTankDefensive(botAI, "ss.gauntlet.defensive");
    return spellId && botAI->CastSpell(spellId, bot);
}

bool IccSvalnaGauntletTauntAction::Execute(Event /*event*/)
{
    Unit* add = GetSsGauntletTauntTarget(botAI);
    if (!add || !IccCastClassTaunt(bot, botAI, add))
        return false;

    AttackDirect(add);
    return true;
}
