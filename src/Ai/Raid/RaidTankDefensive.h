/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_RAIDTANKDEFENSIVE_H
#define PLAYERBOTS_RAIDTANKDEFENSIVE_H

#include "Define.h"

#include <string>
#include <vector>

class Player;
class PlayerbotAI;

// The weakest cooldown the tank can cast right now, or nullptr - either because none is off cooldown
// or because one is already running. Ordered by cooldown length, which is what "weakest" means here.
// `noteKind` names the trace row to record the pick under, or nullptr for no row: the act stream sees
// one action name whichever button it ends up casting, and the order they go in is the whole point.
// `physicalOnly` skips what only helps against magic, for a hit Anti-Magic Shell would waste itself on.
char const* NextTankDefensive(PlayerbotAI* botAI, Player* bot, char const* noteKind, bool physicalOnly = false);

// The same pick for an incoming magic hit, and for any role rather than only a tank's buttons. `needMs`
// is how long the hit is still away, so a 5 s button is not spent on something 11 s out: a row whose
// own duration would run out first is skipped. Pass 0 to ask only whether anything is available, which
// is how one bot reads whether another is already covered.
char const* NextMagicDefensive(PlayerbotAI* botAI, Player* bot, char const* noteKind, uint32 needMs);

// How much of that hit the bot can still take off by itself, as a percentage, so somebody else can work
// out whether a second layer is needed on top. The cut of a button already running when there is one,
// and 0 when the bot has nothing castable or only something that adds health rather than cutting damage.
uint8 BestMagicDefensiveCut(PlayerbotAI* botAI, Player* bot, uint32 needMs);

// True for the cast names in that table, so an encounter can hold the class nodes off them until the
// window it wants them spent in. The tank variant lists only the buttons NextTankDefensive offers.
bool IsHeldTankDefensive(std::string const& actionName);
std::vector<std::string> HeldTankDefensiveNames();
std::vector<std::string> HeldMagicDefensiveNames();

#endif
