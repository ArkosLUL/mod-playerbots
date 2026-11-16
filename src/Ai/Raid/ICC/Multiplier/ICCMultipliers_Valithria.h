/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCMULTIPLIERS_VALITHRIA_H
#define PLAYERBOTS_ICCMULTIPLIERS_VALITHRIA_H

#include "Multiplier.h"
#include "RaidSplash.h"

// Holds a damage dealer's area damage near a Blistering Zombie on somebody else: the threat would turn the
// zombie onto the dealer, and its swings one-shot all but a tank.
class IccValithriaSplashMultiplier : public Multiplier
{
public:
    IccValithriaSplashMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icc valithria splash") {}
    float GetValue(Action* action) override;

private:
    RaidSplashCache _splash;
};

#endif
