/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_ALGALON_H
#define PLAYERBOTS_ULDMULTIPLIERS_ALGALON_H

#include "Define.h"
#include "Multiplier.h"

// Algalon: holds the designated Big Bang soaker's escape cooldown for the cast it is meant to
// survive. Every other priest keeps Dispersion and Guardian Spirit for their normal uses.
class AlgalonSoakCooldownReserveMultiplier : public Multiplier
{
public:
    AlgalonSoakCooldownReserveMultiplier(PlayerbotAI* ai) : Multiplier(ai, "algalon soak cooldown reserve") {}
    float GetValue(Action* action) override;
};

// Algalon: Collapsing Stars are killed one at a time on purpose - each death is 16-21k to the whole
// raid - so an area attack that clips a second one undoes the pacing.
class AlgalonCollapsingStarAoeMultiplier : public Multiplier
{
public:
    AlgalonCollapsingStarAoeMultiplier(PlayerbotAI* ai) : Multiplier(ai, "algalon collapsing star aoe") {}
    float GetValue(Action* action) override;
};

// Algalon: keeps the raid off a Living Constellation, which nobody but its kiter should be touching,
// and off Algalon himself in the one window where a Collapsing Star matters more than he does.
class AlgalonTargetGuardMultiplier : public Multiplier
{
public:
    AlgalonTargetGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "algalon target guard") {}
    float GetValue(Action* action) override;
};

// Algalon: the generic movers would walk the ranged half straight off their formation slots, and the
// slot node would then re-fire next tick and pace them all fight.
class AlgalonControlMovementMultiplier : public Multiplier
{
public:
    AlgalonControlMovementMultiplier(PlayerbotAI* ai) : Multiplier(ai, "algalon control movement") {}
    float GetValue(Action* action) override;
};

#endif
