/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_BLOODPRINCES_H
#define PLAYERBOTS_ICCMULTIPLIERS_BLOODPRINCES_H

#include "Multiplier.h"

// Holds a damage dealer's area damage near a prince without the Invocation, whose damage turns into threat
// on the dealer, and near a Dark Nucleus, which a cleave or a chain takes off its tank.
class IccBpcSplashMultiplier : public Multiplier
{
public:
    IccBpcSplashMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc bpc splash") {}
    float GetValue(Action* action) override;
};

#endif
