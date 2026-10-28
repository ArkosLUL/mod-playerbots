/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_Gunship.h"

#include "Action.h"
#include "ICCHelpers_Gunship.h"
#include "PlayerbotAI.h"

float IccGunshipDeckGuardMultiplier::GetValue(Action* action)
{
    GsbSide const* side = GsbSideOf(bot);
    Unit* target = side ? action->GetTarget() : nullptr;
    if (!target || target == bot)
        return 1.0f;

    // A bot in the air goes where its jump or its fall takes it
    GsbDeck const deck = GsbDeckOf(*side, bot);
    if (deck != GsbDeck::Home && deck != GsbDeck::Enemy)
        return 1.0f;

    // A gunner in its cannon stands on the raid's deck
    GsbDeck theirs = GsbDeckOf(*side, target);
    if (theirs == GsbDeck::Cannon)
        theirs = GsbDeck::Home;

    return theirs == deck ? 1.0f : 0.0f;
}
