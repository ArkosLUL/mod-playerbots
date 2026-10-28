/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_GUNSHIP_H
#define PLAYERBOTS_ICCMULTIPLIERS_GUNSHIP_H

#include "Multiplier.h"

// Zeroes a reach, heal reach or charge at a unit on the other deck: the walk goes straight off the edge.
// A rule can't see the action's target, hence a hand-written one.
class IccGunshipDeckGuardMultiplier : public Multiplier
{
public:
    IccGunshipDeckGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc gunship deck guard") {}
    float GetValue(Action* action) override;
};

#endif
