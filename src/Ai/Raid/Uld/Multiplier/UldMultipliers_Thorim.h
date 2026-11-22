/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_THORIM_H
#define PLAYERBOTS_ULDMULTIPLIERS_THORIM_H

#include "Define.h"
#include "Multiplier.h"

class ThorimRunicBarrierMultiplier : public Multiplier
{
public:
    ThorimRunicBarrierMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "thorim runic barrier") {}
    float GetValue(Action* action) override;
};

class ThorimArenaTargetGuardMultiplier : public Multiplier
{
public:
    ThorimArenaTargetGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "thorim arena target guard") {}
    float GetValue(Action* action) override;
};

class ThorimBalconyGuardMultiplier : public Multiplier
{
public:
    ThorimBalconyGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "thorim balcony guard") {}
    float GetValue(Action* action) override;
};

#endif
