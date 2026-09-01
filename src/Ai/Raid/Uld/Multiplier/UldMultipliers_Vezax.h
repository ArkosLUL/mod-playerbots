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

#endif
