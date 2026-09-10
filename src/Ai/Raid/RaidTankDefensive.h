/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_RAIDTANKDEFENSIVE_H
#define PLAYERBOTS_RAIDTANKDEFENSIVE_H

#include "Define.h"

#include <string>

class Player;
class PlayerbotAI;

// The weakest cooldown the tank can cast right now, or nullptr - either because none is off cooldown
// or because one is already running. Ordered by cooldown length, which is what "weakest" means here.
// `noteKind` names the trace row to record the pick under, or nullptr for no row: the act stream sees
// one action name whichever button it ends up casting, and the order they go in is the whole point.
char const* NextTankDefensive(PlayerbotAI* botAI, Player* bot, char const* noteKind);

// True for the cast names in that table, so an encounter can hold the class nodes off them until the
// window it wants them spent in.
bool IsHeldTankDefensive(std::string const& actionName);

#endif
