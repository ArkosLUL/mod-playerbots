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

#endif
