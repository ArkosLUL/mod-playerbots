/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_AURIAYA_H
#define PLAYERBOTS_ULDMULTIPLIERS_AURIAYA_H

#include "Define.h"
#include "Multiplier.h"
#include "RaidAntiFear.h"

// Stands the generic pickers down so Auriaya's own nodes own targeting and, for the anchored roles,
// movement. Without the movement half the anchor oscillates: the bot reaches its spot, the trigger
// goes quiet, a generic mover walks it off, and the trigger fires again - the failure
// RazorscaleMultiplier exists to prevent.
class AuriayaMovementGuardMultiplier : public Multiplier
{
public:
    AuriayaMovementGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "auriaya movement guard multiplier") {}

    float GetValue(Action* action) override;
};

// The earth totem slot holds one totem, so the shaman's own Stoneskin / Strength of Earth nodes have
// to be held for as long as the boss can fear, or Tremor is replaced on the next GCD.
class AuriayaAntiFearTotemGuardMultiplier : public RaidAntiFearTotemGuardMultiplier
{
public:
    AuriayaAntiFearTotemGuardMultiplier(PlayerbotAI* botAI)
        : RaidAntiFearTotemGuardMultiplier(botAI, "auriaya anti fear totem guard multiplier")
    {
    }

protected:
    bool FearWindowActive() override;
};

#endif
