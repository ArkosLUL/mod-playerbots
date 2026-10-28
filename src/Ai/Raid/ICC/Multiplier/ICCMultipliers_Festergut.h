/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_FESTERGUT_H
#define PLAYERBOTS_ICCMULTIPLIERS_FESTERGUT_H

#include "Multiplier.h"

// Holds a braked tank's spells and swings on him, and its area damage near him. Heals pass.
class IccFestergutBrakeMultiplier : public Multiplier
{
public:
    IccFestergutBrakeMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc festergut brake") {}
    float GetValue(Action* action) override;
};

#endif
