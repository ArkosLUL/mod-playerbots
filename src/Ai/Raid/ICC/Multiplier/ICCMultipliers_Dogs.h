/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_DOGS_H
#define PLAYERBOTS_ICCMULTIPLIERS_DOGS_H

#include "Multiplier.h"
#include "RaidSplash.h"

class IccDogsYieldMultiplier : public Multiplier
{
public:
    IccDogsYieldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc dogs yield") {}
    float GetValue(Action* action) override;

private:
    RaidSplashCache _splash;
};

#endif
