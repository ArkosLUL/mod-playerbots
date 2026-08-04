/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ONYMULTIPLIERS_H
#define PLAYERBOTS_ONYMULTIPLIERS_H

#include "RaidAntiFear.h"

// Keeps Tremor Totem in the earth slot for the whole of phase 3, where Bellowing Roar repeats.
class OnyxiaAntiFearTotemGuardMultiplier : public RaidAntiFearTotemGuardMultiplier
{
public:
    OnyxiaAntiFearTotemGuardMultiplier(PlayerbotAI* botAI)
        : RaidAntiFearTotemGuardMultiplier(botAI, "ony anti fear totem guard multiplier")
    {
    }

protected:
    bool FearWindowActive() override;
};

#endif
