/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_FREYA_H
#define PLAYERBOTS_ULDMULTIPLIERS_FREYA_H

#include "Define.h"
#include "Multiplier.h"

class FreyaTrioSyncMultiplier : public Multiplier
{
public:
    FreyaTrioSyncMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "freya trio sync") {}
    float GetValue(Action* action) override;
};

class FreyaLasherFinishAoeMultiplier : public Multiplier
{
public:
    FreyaLasherFinishAoeMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "freya lasher finish aoe") {}
    float GetValue(Action* action) override;
};

class FreyaLasherTrapReserveMultiplier : public Multiplier
{
public:
    FreyaLasherTrapReserveMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "freya lasher trap reserve") {}
    float GetValue(Action* action) override;
};

class FreyaArmyRuneReserveMultiplier : public Multiplier
{
public:
    FreyaArmyRuneReserveMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "freya army rune reserve") {}
    float GetValue(Action* action) override;

private:
    bool ReserveThisTick();

    uint32 cachedAtMs = 0;
    bool cachedReserve = false;
};

class FreyaGroundTremorCastGateMultiplier : public Multiplier
{
public:
    FreyaGroundTremorCastGateMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "freya ground tremor cast gate") {}
    float GetValue(Action* action) override;

private:
    int32 EvaluateWindow();

    uint32 cachedAtMs = 0;
    int32 cachedRemainingMs = 0;
};

#endif
