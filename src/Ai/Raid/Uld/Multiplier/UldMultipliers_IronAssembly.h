/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_IRONASSEMBLY_H
#define PLAYERBOTS_ULDMULTIPLIERS_IRONASSEMBLY_H

#include "Define.h"
#include "Multiplier.h"

class IronAssemblyHoldDpsCooldownsMultiplier : public Multiplier
{
public:
    IronAssemblyHoldDpsCooldownsMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "iron assembly hold dps cooldowns") {}
    float GetValue(Action* action) override;
};

class IronAssemblyTauntGuardMultiplier : public Multiplier
{
public:
    IronAssemblyTauntGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "iron assembly taunt guard") {}
    float GetValue(Action* action) override;
};

#endif
