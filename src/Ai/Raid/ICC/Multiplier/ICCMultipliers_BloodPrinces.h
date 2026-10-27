/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_BLOODPRINCES_H
#define PLAYERBOTS_ICCMULTIPLIERS_BLOODPRINCES_H

#include "Multiplier.h"

class IccBpcAssistMultiplier : public Multiplier
{
public:
    IccBpcAssistMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc bpc assist") {}
    float GetValue(Action* action) override;

private:
    uint32 _flamesCheckedMs = 0;
    bool _infernoFlameUp = false;
    bool _chasedByFlame = false;
};

#endif
