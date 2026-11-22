/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDTARGETS_H
#define PLAYERBOTS_ULDTARGETS_H

#include "Define.h"

class PlayerbotAI;
class Unit;

Unit* GetUldUnitByEntry(PlayerbotAI* botAI, uint32 entry);

#endif
