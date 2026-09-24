/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_THORIM_H
#define PLAYERBOTS_ULDMULTIPLIERS_THORIM_H

#include "Define.h"
#include "Multiplier.h"

// Thorim: Runic Barrier answers every melee swing with 2000 arcane, so a bot that has backed out on
// the health band must not keep walking back in. Only the swing and the chase are held - the target
// is deliberately kept, and everything that is not a melee hit carries on from out there.
class ThorimRunicBarrierMultiplier : public Multiplier
{
public:
    ThorimRunicBarrierMultiplier(PlayerbotAI* ai) : Multiplier(ai, "thorim runic barrier") {}
    float GetValue(Action* action) override;
};

// Without this the leash and the chase take turns: the leash walks the bot back, the picker still
// holds the corridor mob, and it walks straight out again. Zeroing the attack drops the target instead.
class ThorimArenaTargetGuardMultiplier : public Multiplier
{
public:
    ThorimArenaTargetGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "thorim arena target guard") {}
    float GetValue(Action* action) override;
};

// Thorim: the corridor squad once it is up on the platform. It holds Thorim as a target from 130 yd
// out - the generic picker reads threat, not range, so an untouchable boss is still in the list - and
// then "reach melee" walks it up the middle of the hallway into the Paralytic Field bunnies. Once he
// is on the floor there is no walkable way down, so any reach walks the bot back through the hallway.
class ThorimBalconyGuardMultiplier : public Multiplier
{
public:
    ThorimBalconyGuardMultiplier(PlayerbotAI* ai) : Multiplier(ai, "thorim balcony guard") {}
    float GetValue(Action* action) override;
};

#endif
