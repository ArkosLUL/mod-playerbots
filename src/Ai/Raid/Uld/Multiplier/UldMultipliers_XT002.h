/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_XT002_H
#define PLAYERBOTS_ULDMULTIPLIERS_XT002_H

#include "Define.h"
#include "Multiplier.h"
#include "RaidSplash.h"

class XT002BurstWindowMultiplier : public Multiplier
{
public:
    XT002BurstWindowMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "xt002 burst window") {}
    float GetValue(Action* action) override;

private:
    struct Window
    {
        bool allowAll;
        bool allowLust;
    };

    Window EvaluateWindow();

    uint32 cachedAtMs = 0;
    Window cachedValue = {true, true};
};

class XT002TargetGuardMultiplier : public Multiplier
{
public:
    XT002TargetGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "xt002 target guard") {}
    float GetValue(Action* action) override;
};

class XT002ThreatBrakeMultiplier : public Multiplier
{
public:
    XT002ThreatBrakeMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "xt002 threat brake") {}
    float GetValue(Action* action) override;

private:
    RaidSplashCache _splash;
};

#endif
