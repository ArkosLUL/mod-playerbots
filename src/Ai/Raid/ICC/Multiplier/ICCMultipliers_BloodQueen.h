/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_BLOODQUEEN_H
#define PLAYERBOTS_ICCMULTIPLIERS_BLOODQUEEN_H

#include "Multiplier.h"
#include "RaidAntiFear.h"
#include "RaidSplash.h"

class IccBqlBrakeMultiplier : public Multiplier
{
public:
    IccBqlBrakeMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc bql brake") {}
    float GetValue(Action* action) override;

private:
    RaidSplashCache _splash;
};

class IccBqlSplashMultiplier : public Multiplier
{
public:
    IccBqlSplashMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc bql splash") {}
    float GetValue(Action* action) override;

private:
    RaidSplashCache _splash;
    uint32 _checkedMs = 0;
    bool _vampireNear = false;
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
