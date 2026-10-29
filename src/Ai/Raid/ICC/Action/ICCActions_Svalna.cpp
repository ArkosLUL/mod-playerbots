/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCActions_Svalna.h"

#include "Creature.h"
#include "ICCHelpers_Shared.h"
#include "Item.h"
#include "Playerbots.h"

namespace
{
// Hurl Spear reaches 25 yd; a thrower out of range walks to this far from her
constexpr float HURL_WALK_TO = 18.0f;
constexpr float HURL_RANGE = 24.0f;
}  // namespace

bool IccSvalnaSpearAction::Execute(Event /*event*/)
{
    Creature* spear = GetSsSpearToPull(botAI);
    if (!spear)
        return false;

    // The victim loses 10% of its health a second, so a cast in progress costs more than it brings
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

    // Hurl Spear refuses a shapeshifted thrower, and a cast in progress would refuse the item use
    botAI->RemoveShapeshift();
    if (bot->IsNonMeleeSpellCast(false))
        bot->InterruptNonMeleeSpells(false);

    botAI->ImbueItem(spear, svalna);
    return true;
}

bool IccSvalnaTauntAction::Execute(Event /*event*/)
{
    Unit* captain = GetSsCaptainToTaunt(botAI);
    if (!captain || !IccCastClassTaunt(bot, botAI, captain))
        return false;

    // A taunt moves the captain, not the bot's own target
    AttackDirect(captain);
    return true;
}
