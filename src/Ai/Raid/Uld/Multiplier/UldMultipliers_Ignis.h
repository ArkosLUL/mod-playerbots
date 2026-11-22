/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_IGNIS_H
#define PLAYERBOTS_ULDMULTIPLIERS_IGNIS_H

#include "Define.h"
#include "Multiplier.h"

class IgnisFlameJetsHoldCastMultiplier : public Multiplier
{
public:
    IgnisFlameJetsHoldCastMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "ignis flame jets hold cast") {}
    float GetValue(Action* action) override;

private:
    int32 EvaluateWindow();

    uint32 cachedAtMs = 0;
    int32 cachedRemainingMs = 0;
};

#endif
