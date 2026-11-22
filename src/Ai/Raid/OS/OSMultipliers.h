/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_OSMULTIPLIERS_H
#define PLAYERBOTS_OSMULTIPLIERS_H

#include "Multiplier.h"

class SartharionMainTankTargetMultiplier : public Multiplier
{
public:
    SartharionMainTankTargetMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "sartharion") {}
    float GetValue(Action* action) override;
};

class SartharionOffTankBossMultiplier : public Multiplier
{
public:
    SartharionOffTankBossMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "sartharion") {}
    float GetValue(Action* action) override;
};

class SartharionRearFlankMultiplier : public Multiplier
{
public:
    SartharionRearFlankMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "sartharion") {}
    float GetValue(Action* action) override;
};

class SartharionBossReachMultiplier : public Multiplier
{
public:
    SartharionBossReachMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "sartharion") {}
    float GetValue(Action* action) override;
};

class SartharionBurstWindowMultiplier : public Multiplier
{
public:
    SartharionBurstWindowMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "sartharion burst window") {}
    float GetValue(Action* action) override;

private:
    float EvaluateWindow();

    static constexpr float SARTHARION_ENRAGE_PCT = 30.0f;

    uint32 cachedAtMs = 0;
    float cachedValue = 1.0f;
};

#endif
