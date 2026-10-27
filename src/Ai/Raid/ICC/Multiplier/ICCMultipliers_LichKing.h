/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_LICHKING_H
#define PLAYERBOTS_ICCMULTIPLIERS_LICHKING_H

#include "Multiplier.h"
#include "ObjectGuid.h"

class IccLichKingAddsMultiplier : public Multiplier
{
public:
    IccLichKingAddsMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc lich king adds") {}
    float GetValue(Action* action) override;

private:
    uint32 _checkedMs = 0;
    bool _nearValithria = false;
    bool _nearHeroicTerenas = false;
    ObjectGuid _bossGuid;
    bool _phaseOne = false;
    bool _winter = false;
    bool _castingWinter = false;
};

class IccLichKingSpiritBombMultiplier : public Multiplier
{
public:
    IccLichKingSpiritBombMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc lich king spirit bomb") {}
    float GetValue(Action* action) override;
};

#endif
