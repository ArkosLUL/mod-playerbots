/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_YOGGSARON_H
#define PLAYERBOTS_ULDMULTIPLIERS_YOGGSARON_H

#include "Define.h"
#include "Multiplier.h"
#include "RaidAntiFear.h"

// Companion to yogg-saron set dps priority action: that node owns every non-tank's target from phase 2
// on, so the generic picker has to stand down rather than pull bots back onto whatever is nearest.
class YoggSaronDpsTargetGuardMultiplier : public Multiplier
{
public:
    YoggSaronDpsTargetGuardMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "yogg-saron dps target guard multiplier")
    {
    }

    float GetValue(Action* action) override;
};

class YoggSaronAntiFearTotemGuardMultiplier : public RaidAntiFearTotemGuardMultiplier
{
public:
    YoggSaronAntiFearTotemGuardMultiplier(PlayerbotAI* botAI)
        : RaidAntiFearTotemGuardMultiplier(botAI, "yogg-saron anti fear totem guard multiplier")
    {
    }

protected:
    bool FearWindowActive() override;
};

#endif
