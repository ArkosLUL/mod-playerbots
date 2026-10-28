/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_DOGS_H
#define PLAYERBOTS_ICCMULTIPLIERS_DOGS_H

#include "Multiplier.h"

// Holds a yielding tank's spells and swings on the dog, and its area damage near it. Heals pass.
class IccDogsYieldMultiplier : public Multiplier
{
public:
    IccDogsYieldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc dogs yield") {}
    float GetValue(Action* action) override;
};

#endif
