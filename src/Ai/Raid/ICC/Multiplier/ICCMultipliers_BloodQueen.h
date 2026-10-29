/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_BLOODQUEEN_H
#define PLAYERBOTS_ICCMULTIPLIERS_BLOODQUEEN_H

#include "Multiplier.h"
#include "RaidAntiFear.h"

// A braked bot's attacks on her and its area damage next to her: its threat would take her off her holder
class IccBqlBrakeMultiplier : public Multiplier
{
public:
    IccBqlBrakeMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc bql brake") {}
    float GetValue(Action* action) override;
};

class IccBqlAntiFearTotemGuardMultiplier : public RaidAntiFearTotemGuardMultiplier
{
public:
    IccBqlAntiFearTotemGuardMultiplier(PlayerbotAI* botAI)
        : RaidAntiFearTotemGuardMultiplier(botAI, "icc bql anti fear totem guard")
    {
    }

protected:
    bool FearWindowActive() override;
};

#endif
