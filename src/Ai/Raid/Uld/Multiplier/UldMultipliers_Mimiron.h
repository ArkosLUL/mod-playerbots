/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_MIMIRON_H
#define PLAYERBOTS_ULDMULTIPLIERS_MIMIRON_H

#include "Define.h"
#include "Multiplier.h"

class MimironTankAnchorGuardMultiplier : public Multiplier
{
public:
    MimironTankAnchorGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "mimiron tank anchor guard") {}
    float GetValue(Action* action) override;
};

class MimironFireBotAoeGuardMultiplier : public Multiplier
{
public:
    MimironFireBotAoeGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "mimiron fire bot aoe guard") {}
    float GetValue(Action* action) override;
};

class MimironPlasmaDefensiveHoldMultiplier : public Multiplier
{
public:
    MimironPlasmaDefensiveHoldMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "mimiron plasma defensive hold") {}
    float GetValue(Action* action) override;
};

#endif
