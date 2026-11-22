/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_BLOODPRINCES_H
#define PLAYERBOTS_ICCMULTIPLIERS_BLOODPRINCES_H

#include "Multiplier.h"
#include "RaidSplash.h"

class IccBpcSplashMultiplier : public Multiplier
{
public:
    IccBpcSplashMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc bpc splash") {}
    float GetValue(Action* action) override;

private:
    RaidSplashCache _splash;
};

class IccBpcBrakeMultiplier : public Multiplier
{
public:
    IccBpcBrakeMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc bpc brake") {}
    float GetValue(Action* action) override;

private:
    RaidSplashCache _splash;
};

class IccBpcDirectHitMultiplier : public Multiplier
{
public:
    IccBpcDirectHitMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc bpc direct hit") {}
    float GetValue(Action* action) override;
};

#endif
