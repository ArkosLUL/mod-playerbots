/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_FESTERGUT_H
#define PLAYERBOTS_ICCMULTIPLIERS_FESTERGUT_H

#include "Multiplier.h"
#include "RaidSplash.h"

class IccFestergutBrakeMultiplier : public Multiplier
{
public:
    IccFestergutBrakeMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc festergut brake") {}
    float GetValue(Action* action) override;

private:
    RaidSplashCache _splash;
};

#endif
