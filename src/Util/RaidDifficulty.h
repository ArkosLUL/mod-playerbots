/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_RAIDDIFFICULTY_H
#define PLAYERBOTS_RAIDDIFFICULTY_H

#include "Map.h"
#include "Player.h"

// A player's own raid difficulty is the group's setting, never the instance's. A group left on 25
// Heroic walks into Ulduar, Naxxramas or Eye of Eternity, which have no heroic mode, the map is
// downscaled to 25 Normal, and every member still reports 25 Heroic for the whole pull - so a check
// against RAID_DIFFICULTY_25MAN_NORMAL reads as 10-man. Encounter code asks the map instead.
inline Difficulty GetRaidMapDifficulty(Player* bot)
{
    Map* map = bot ? bot->GetMap() : nullptr;
    return map ? map->GetDifficulty() : RAID_DIFFICULTY_10MAN_NORMAL;
}

// True on 25 Normal and 25 Heroic alike.
inline bool IsRaid25Man(Player* bot)
{
    Map* map = bot ? bot->GetMap() : nullptr;
    return map && map->Is25ManRaid();
}

#endif
