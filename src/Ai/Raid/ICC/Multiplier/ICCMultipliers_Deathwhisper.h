/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_DEATHWHISPER_H
#define PLAYERBOTS_ICCMULTIPLIERS_DEATHWHISPER_H

#include "Multiplier.h"
#include "RaidSplash.h"

class IccLadyDeathwhisperMultiplier : public Multiplier
{
public:
    IccLadyDeathwhisperMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc lady deathwhisper") {}
    float GetValue(Action* action) override;

private:
    RaidSplashCache _splash;
    uint32 _checkedMs = 0;
    bool _victimNear = false;
};

class IccLadyDeathwhisperBrakeMultiplier : public Multiplier
{
public:
    IccLadyDeathwhisperBrakeMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc lady deathwhisper brake") {}
    float GetValue(Action* action) override;

private:
    RaidSplashCache _splash;
};

#endif
