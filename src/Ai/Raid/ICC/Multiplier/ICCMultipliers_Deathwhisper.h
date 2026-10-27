/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_DEATHWHISPER_H
#define PLAYERBOTS_ICCMULTIPLIERS_DEATHWHISPER_H

#include "Multiplier.h"

class IccLadyDeathwhisperMultiplier : public Multiplier
{
public:
    IccLadyDeathwhisperMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc lady deathwhisper") {}
    float GetValue(Action* action) override;

private:
    uint32 _checkedMs = 0;
    bool _bossPresent = false;
    bool _shadeOnBot = false;
};

#endif
