/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_RAZORSCALE_H
#define PLAYERBOTS_ULDMULTIPLIERS_RAZORSCALE_H

#include "Define.h"
#include "Multiplier.h"

// Razorscale: keeps the generic movers off a bot that is clearing a Devouring Flame patch. The dodge
// action wins on priority, but it releases the tick the moment the bot is standing clear, and the
// movers then walk it straight back onto the 5yd patch it just left.
class RazorscaleMultiplier : public Multiplier
{
public:
    RazorscaleMultiplier(PlayerbotAI* ai) : Multiplier(ai, "razorscale") {}
    float GetValue(Action* action) override;

private:
    // Walks the npc list and, for the destination test, the grid. Every generic mover in the queue
    // asks the same question, so the verdict is memoised for the rest of the tick.
    bool MoversBlocked();

    uint32 cachedAtMs = 0;
    bool cachedBlocked = false;
};

#endif
