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

// Life Tap does nothing at all on this boss. Aura of Despair carries EFFECT_IMMUNITY to
// SPELL_EFFECT_ENERGIZE, and the mana half of Life Tap is 31818, whose only effect is exactly that -
// so the tap pays 2000 health and returns nothing. Nor does the Glyph of Life Tap buff land: 63320
// procs on PROC_FLAG_DONE_SPELL_NONE_DMG_CLASS_POS, which is the energize, and it dies with it.
// Traced: 289 taps across three warlocks, zero mana gained and zero 63321 applied, against +15 to
// +22 points and one buff per tap for the same bots on Hodir.
class VezaxSuppressLifeTapMultiplier : public Multiplier
{
public:
    VezaxSuppressLifeTapMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "vezax suppress life tap multiplier")
    {
    }

    float GetValue(Action* action) override;
};

// Saronite Vapors are hostile, they pulse damage on anyone near them, and the generic pickers will
// happily take one. Killing one calls DoAction(1) on the boss and ends hard mode for good, so the
// raid has to leave them alone: this silences the pickers, and the drop-vapor-target node clears a
// vapor a bot already holds.
class VezaxTargetGuardMultiplier : public Multiplier
{
public:
    VezaxTargetGuardMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "vezax target guard multiplier")
    {
    }

    float GetValue(Action* action) override;
};

// Outside a Shadow Crash field a caster pays full price for mana it cannot make back, so ranged dps
// walk to a field rather than cast where they stand. Healers are the exception - they soak, but a
// held heal lands inside the leech spikes that kill people.
class VezaxHoldCastOutsideFieldMultiplier : public Multiplier
{
public:
    VezaxHoldCastOutsideFieldMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "vezax hold cast outside field multiplier")
    {
    }

    float GetValue(Action* action) override;
};

#endif
