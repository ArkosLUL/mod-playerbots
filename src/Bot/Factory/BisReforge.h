/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_BISREFORGE_H
#define PLAYERBOTS_BISREFORGE_H

#include "Define.h"

class Item;
class Player;

// Safe from map threads, the reforge runs later on the world thread. from and to are ItemModType
// ids, both 0 asks to remove the item's reforge. No-op without mod-reforging.
void RequestBisReforge(Player* bot, Item* item, uint32 from, uint32 to);

#endif
