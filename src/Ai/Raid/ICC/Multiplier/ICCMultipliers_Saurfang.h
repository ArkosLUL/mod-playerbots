/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_SAURFANG_H
#define PLAYERBOTS_ICCMULTIPLIERS_SAURFANG_H

#include "Multiplier.h"

class IccAddsDbsMultiplier : public Multiplier
{
public:
    IccAddsDbsMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc adds dbs") {}
    float GetValue(Action* action) override;
};

#endif
