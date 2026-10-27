/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_RAIDBURSTWINDOW_H
#define PLAYERBOTS_RAIDBURSTWINDOW_H

#include <string>

#include "Define.h"
#include "Multiplier.h"

class Action;
class PlayerbotAI;

// What one encounter lets through the burst gate right now. Default lets everything through, so only
// the base tank-hold gate applies. Lust is its own flag: one 10-minute raid cooldown, personal
// cooldowns come back mid-fight.
struct RaidBurstWindow
{
    bool allowAll = true;
    bool allowLust = true;
};

// Holds burst cooldowns to the row window answers for the live encounter, default between pulls. One
// multiplier for every encounter, so the IsBurstCooldownAction early-out runs once per action rather
// than once per boss.
class RaidBurstWindowMultiplier : public Multiplier
{
public:
    using Window = RaidBurstWindow (*)(PlayerbotAI* botAI);

    RaidBurstWindowMultiplier(PlayerbotAI* botAI, std::string const name, Window window)
        : Multiplier(botAI, name), _window(window) {}

    float GetValue(Action* action) override;

private:
    Window _window;
    uint32 cachedAtMs = 0;
    RaidBurstWindow cachedValue;
};

#endif
