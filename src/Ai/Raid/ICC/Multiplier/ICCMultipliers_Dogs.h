/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_DOGS_H
#define PLAYERBOTS_ICCMULTIPLIERS_DOGS_H

#include "Multiplier.h"

class IccDogsMultiplier : public Multiplier
{
public:
    IccDogsMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc dogs") {}
    float GetValue(Action* action) override;
};

#endif
