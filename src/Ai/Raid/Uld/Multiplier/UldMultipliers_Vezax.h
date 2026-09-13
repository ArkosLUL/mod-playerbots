/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_VEZAX_H
#define PLAYERBOTS_ULDMULTIPLIERS_VEZAX_H

#include "Define.h"
#include "Multiplier.h"

// Vezax pins the ranged half and the healers to derived arc slots, so the generic movers have to
// stand down for them. Melee and the tank are untouched: they hold the boss and own every generic
// mover, including the de-clump the position node falls through to.
class VezaxControlMovementMultiplier : public Multiplier
{
public:
    VezaxControlMovementMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "vezax control movement multiplier")
    {
    }

    float GetValue(Action* action) override;
};

// Aura of Despair stops mana regeneration, which makes Life Tap the only mana a warlock can find
// here - and the generic rotation taps on cooldown whether or not there is anything to spend it on.
// One traced pull had both locks casting it every 1.25s for the whole fight at 94-100% mana, paying
// 2000 health a time, and both bled out having dealt a tenth of what the comparable caster did.
class VezaxSuppressLifeTapMultiplier : public Multiplier
{
public:
    VezaxSuppressLifeTapMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "vezax suppress life tap multiplier")
    {
    }

    float GetValue(Action* action) override;
};

#endif
