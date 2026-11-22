/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_VEZAX_H
#define PLAYERBOTS_ULDMULTIPLIERS_VEZAX_H

#include "Define.h"
#include "Multiplier.h"

#include <unordered_map>

// Life Tap returns nothing here: its mana half, 31818, is a bare SPELL_EFFECT_ENERGIZE that Aura of
// Despair is immune to, and the glyph proc (63320) rides that same energize.
class VezaxSuppressLifeTapMultiplier : public Multiplier
{
public:
    VezaxSuppressLifeTapMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "vezax suppress life tap multiplier")
    {
    }

    float GetValue(Action* action) override;
};

// Ranged dps don't cast outside a Shadow Crash field: full price buys mana that can't be made back.
// Healers are exempt, since a held heal lands inside the leech spikes that kill people.
class VezaxHoldCastOutsideFieldMultiplier : public Multiplier
{
public:
    VezaxHoldCastOutsideFieldMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "vezax hold cast outside field multiplier")
    {
    }

    float GetValue(Action* action) override;
};

// A dps bot on Vezax or the Animus holds its damage once its threat gets close to the main tank's, which
// also lets him win the boss back from whoever pulled it. The Animus can't be taunted at all, and in a
// field a caster's shadow damage outruns a tank's threat.
class VezaxThreatMultiplier : public Multiplier
{
public:
    VezaxThreatMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "vezax threat multiplier") {}

    float GetValue(Action* action) override;
};

// Tricks of the Trade only on a tank. At the pull the generic pick decides the tank needs no redirect and
// takes the hardest-hitting melee instead, which hands the rogue's threat to a dps.
class VezaxTricksOnTankMultiplier : public Multiplier
{
public:
    VezaxTricksOnTankMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "vezax tricks on tank multiplier") {}

    float GetValue(Action* action) override;
};

// The spell half of the hard mode hold: nothing aimed at him and no area damage. Melee, pets and the
// damaging summons are a Block rule.
class VezaxDamageHoldMultiplier : public Multiplier
{
public:
    VezaxDamageHoldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "vezax damage hold multiplier") {}

    float GetValue(Action* action) override;

private:
    enum class Reach : uint8
    {
        None,
        CurrentTarget,
        Area
    };

    std::unordered_map<Action*, Reach> reaches;
};

#endif
