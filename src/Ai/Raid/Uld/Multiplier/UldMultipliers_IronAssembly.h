/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_IRONASSEMBLY_H
#define PLAYERBOTS_ULDMULTIPLIERS_IRONASSEMBLY_H

#include "Define.h"
#include "Multiplier.h"

// The Iron Assembly picks every non-tank's target in code, because killing one council member
// restores the other two to full - damage the generic picker sprays across three health bars is
// simply thrown away. Suppressed for every role and for the whole fight, not only while the boss
// node has something to say: the terminal fallback is always a living council member, so nothing is
// ever left nodeless. It matches on action type alone and never asks for a role, which is what keeps
// healers out of it - IsRanged() returns true for them.
class IronAssemblyDisableAutomaticTargetingMultiplier : public Multiplier
{
public:
    IronAssemblyDisableAutomaticTargetingMultiplier(PlayerbotAI* ai)
        : Multiplier(ai, "iron assembly disable automatic targeting")
    {
    }
    float GetValue(Action* action) override;
};

// Scoped to the hazard window rather than the whole fight. A permanent guard would leave the raid
// parked wherever its last dodge ended, because the encounter's own position node yields once it
// arrives and nothing else would be left to walk anyone back - the Void Reaver failure.
class IronAssemblyMovementGuardMultiplier : public Multiplier
{
public:
    IronAssemblyMovementGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "iron assembly movement guard") {}
    float GetValue(Action* action) override;
};

// A gap-closer travels in a straight line, consults nothing about the ground, and is a CastSpellAction
// rather than a MovementAction - so the movement guard above cannot see it and the server's own spell
// effect drops the bot back on the boss. Traced: the only three melee that cast one during an Overload
// were the only three that took Overload damage, one of them more than the tank, while the five with
// no gap-closer took none.
class IronAssemblyChargeGuardMultiplier : public Multiplier
{
public:
    IronAssemblyChargeGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "iron assembly charge guard") {}
    float GetValue(Action* action) override;
};

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

#endif
