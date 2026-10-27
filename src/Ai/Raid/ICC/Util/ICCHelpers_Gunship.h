/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_GUNSHIP_H
#define PLAYERBOTS_ICCHELPERS_GUNSHIP_H

#include "ICCData.h"
#include "Playerbots.h"
#include <vector>

inline const Position ICC_GUNSHIP_ROCKET_JUMP_ALLY = Position (-370.04645f, 1993.3536f, 466.65656f);
inline const Position ICC_GUNSHIP_ROCKET_JUMP_ALLY2 = Position (-392.66208f, 2064.893f, 466.5672f);
inline const Position ICC_GUNSHIP_ROCKET_JUMP_ALLY_MIDDLE_POINT = Position(-383.01166f, 2022.8046f, 467.38193f);
inline const Position ICC_GUNSHIP_ROCKET_JUMP_ALLY_FRIENDLY_POINT = Position(-389.26788f, 2045.9136f, 467.56168f);
inline const Position ICC_GUNSHIP_ROCKET_JUMP_HORDE = Position (-449.5343f, 2477.2024f, 471.31906f);
inline const Position ICC_GUNSHIP_ROCKET_JUMP_HORDE2 = Position (-429.81586f, 2400.6804f, 471.56537f);
inline const Position ICC_GUNSHIP_ROCKET_JUMP_HORDE_MIDDLE_POINT = Position(-449.17645f, 2449.2705f, 470.9257f);
inline const Position ICC_GUNSHIP_ROCKET_JUMP_HORDE_FRIENDLY_POINT = Position(-444.74533f, 2420.1208f, 470.78748f);

const std::vector<uint32> availableTargetsGS          = {
                                                        NPC_KOR_KRON_ROCKETEER,        NPC_KOR_KRON_AXETHROWER,       NPC_KOR_KRON_BATTLE_MAGE, NPC_IGB_HIGH_OVERLORD_SAURFANG,
                                                        NPC_SKYBREAKER_MORTAR_SOLDIER, NPC_SKYBREAKER_RIFLEMAN,       NPC_SKYBREAKER_SORCERER,  NPC_IGB_MURADIN_BRONZEBEARD};

// The enemy captain within `range`, alive and hostile: High Overlord Saurfang to a raid on the Skybreaker,
// Muradin to one on Orgrim's Hammer. Null off the gunship.
Unit* IccGunshipEnemyCaptain(Player* bot, float range);
// Below Zero on the bot's own cannons: the enemy mage's 15 min freeze, which sends the raid across to
// kill him.
bool IccGunshipCannonsFrozen(Player* bot);

#endif
