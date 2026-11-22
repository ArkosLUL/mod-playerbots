/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_SHARED_H
#define PLAYERBOTS_ULDMULTIPLIERS_SHARED_H

#include "Define.h"
#include "Multiplier.h"

class UldThreatRedirectMultiplier : public Multiplier
{
public:
    UldThreatRedirectMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "uld threat redirect") {}
    float GetValue(Action* action) override;
};

class UlduarBurstWindowMultiplier : public Multiplier
{
public:
    UlduarBurstWindowMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "ulduar burst window") {}
    float GetValue(Action* action) override;

private:
    struct BurstWindow
    {
        bool allowAll = true;
        bool allowLust = true;
    };

    BurstWindow EvaluateWindow();

    static constexpr float FREYA_LUST_FALLBACK_PCT = 25.0f;

    uint32 cachedAtMs = 0;
    BurstWindow cachedValue;

    uint32 mimironPhase4LiveMs = 0;
};

#endif
