/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_ROTFACE_H
#define PLAYERBOTS_ICCMULTIPLIERS_ROTFACE_H

#include "Multiplier.h"

// Holds a damage dealer's area damage near an ooze: the damage buys threat on it, and a Big Ooze taken off
// its kiter walks into the raid.
class IccRotfaceOozeSplashMultiplier : public Multiplier
{
public:
    IccRotfaceOozeSplashMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc rotface ooze splash") {}
    float GetValue(Action* action) override;
};

#endif
