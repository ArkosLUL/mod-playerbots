/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_RAIDHUMANTANKCALL_H
#define PLAYERBOTS_RAIDHUMANTANKCALL_H

#include "Define.h"

#include <string>

class Player;

constexpr uint32 RAID_HUMAN_CALL_REPEAT_MS = 6000;

// Tells a human tank to press `spellId` now. `sender` is a bot in his group. Shows as a DBM warning through
// the PlayerbotCalls add-on (tools/addons/PlayerbotCalls), or as a raid boss whisper without it.
// `hitInMs` is how far off the hit is, 0 when unknown. Sends nothing while one of his buttons is running
// or within `repeatMs` of his last call. `why` must not contain a tab. True when sent.
bool CallHumanTankDefensive(Player* sender, Player* human, uint32 spellId, uint32 hitInMs, std::string const& why,
                            char const* traceKind, uint32 repeatMs = RAID_HUMAN_CALL_REPEAT_MS);

#endif
