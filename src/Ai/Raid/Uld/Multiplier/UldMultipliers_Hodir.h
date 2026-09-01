/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_HODIR_H
#define PLAYERBOTS_ULDMULTIPLIERS_HODIR_H

#include "Define.h"
#include "Multiplier.h"

// Stands the generic pickers down so Hodir's own nodes own targeting and, for the anchored roles,
// movement. Both halves are load-bearing: without the targeting half DpsTargetValue is never null,
// so the generic node retakes the target every other tick and bots drift off the ice block that is
// about to get an ally killed; without the movement half the anchor oscillates, the failure
// RazorscaleMultiplier exists to prevent.
class HodirGuardMultiplier : public Multiplier
{
public:
    HodirGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "hodir guard multiplier") {}

    float GetValue(Action* action) override;
};

// Holds the paladin aura slot open for Frost Resistance. The slot is exclusive, so without this the
// paladin's own buff strategy re-casts Retribution or Devotion on the next GCD and the two trade the
// slot for the whole fight - the same failure RaidAntiFearTotemGuardMultiplier exists to prevent for
// the shaman earth totem. Only the one chosen paladin is held; the rest keep their own auras.
class HodirPaladinAuraMultiplier : public Multiplier
{
public:
    HodirPaladinAuraMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "hodir paladin aura multiplier") {}

    float GetValue(Action* action) override;
};

#endif
