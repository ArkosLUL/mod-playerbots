/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_ROTFACE_H
#define PLAYERBOTS_ICCMULTIPLIERS_ROTFACE_H

#include "Multiplier.h"
#include "RaidSplash.h"

class IccRotfaceOozeSplashMultiplier : public Multiplier
{
public:
    IccRotfaceOozeSplashMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc rotface ooze splash") {}
    float GetValue(Action* action) override;

private:
    RaidSplashCache _splash;
};

class IccRotfaceSelfCureMultiplier : public Multiplier
{
public:
    IccRotfaceSelfCureMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc rotface self cure") {}
    float GetValue(Action* action) override;
};

#endif
