/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_BURSTCOOLDOWNS_H
#define PLAYERBOTS_BURSTCOOLDOWNS_H

#include <string>

#include "Define.h"
#include "ObjectGuid.h"

class Player;
class Unit;

// True for offensive throughput cooldowns and raid-wide haste only: tank mitigation and healer mana
// cooldowns must keep firing on their own logic. Takes the name rather than the Action because
// Action::getName() returns by value and callers are on a per-action-per-tick path.
bool IsBurstCooldownAction(std::string const& actionName);

// Caller-owned dwell state, keyed on the boss guid so engaging a different boss re-arms the gate
// instead of inheriting the previous boss's timer.
struct BurstHoldState
{
    ObjectGuid boss;
    uint32 sinceMs = 0;

    void Reset()
    {
        boss.Clear();
        sinceMs = 0;
    }
};

// True when the boss's victim has been a tank in the bot's group continuously for at least dwellMs.
// Any tank counts, not only the main one: a raid that hands a boss-flagged add to its off-tank has
// established threat just as well, and insisting on the main tank held every burst cooldown for whole
// fights. State is zeroed whenever the victim is not a group tank, so a tank swap or a tank death
// re-arms the gate. Callers must also Reset() on any path that skips this call, otherwise a stale
// timer from the previous fight satisfies the dwell instantly on the next pull.
bool TankHasHeldBoss(Player* bot, Unit* boss, BurstHoldState& state, uint32 dwellMs);

#endif
