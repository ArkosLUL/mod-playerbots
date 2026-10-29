/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_SINDRAGOSAADDS_H
#define PLAYERBOTS_ICCMULTIPLIERS_SINDRAGOSAADDS_H

#include "Multiplier.h"
#include "RaidAntiFear.h"

// A braked bot's attacks on its drake and its area damage next to it: its threat would take the drake
// off its holder
class IccSgDrakeBrakeMultiplier : public Multiplier
{
public:
    IccSgDrakeBrakeMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc sindragosa drake brake") {}
    float GetValue(Action* action) override;
};

class IccSgDrakeAntiFearTotemGuardMultiplier : public RaidAntiFearTotemGuardMultiplier
{
public:
    IccSgDrakeAntiFearTotemGuardMultiplier(PlayerbotAI* botAI)
        : RaidAntiFearTotemGuardMultiplier(botAI, "icc sindragosa drake anti fear totem guard")
    {
    }

protected:
    bool FearWindowActive() override;
};

#endif
