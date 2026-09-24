/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_IRONASSEMBLY_H
#define PLAYERBOTS_ULDMULTIPLIERS_IRONASSEMBLY_H

#include "Define.h"
#include "Multiplier.h"

// Hard mode only, where the kill order saves Steelbreaker for last. Everything before him is spent on
// one shared health pool, so burst held back loses the raid nothing, and the phase it is held for is
// the one that kills them: Supercharge and then Electrical Charge compound on the survivor, and both
// traced pulls lost two thirds of the raid inside it.
class IronAssemblyHoldDpsCooldownsMultiplier : public Multiplier
{
public:
    IronAssemblyHoldDpsCooldownsMultiplier(PlayerbotAI* ai) : Multiplier(ai, "iron assembly hold dps cooldowns") {}
    float GetValue(Action* action) override;
};

// Whoever carries Overwhelming Power keeps the empowered Steelbreaker until Meltdown, and the tank node
// already follows that. The class taunt nodes didn't: traced, a bot tank back from a soulstone took
// him off a human carrying it, died to him 9s later and left nobody to inherit.
class IronAssemblyTauntGuardMultiplier : public Multiplier
{
public:
    IronAssemblyTauntGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "iron assembly taunt guard") {}
    float GetValue(Action* action) override;
};

#endif
