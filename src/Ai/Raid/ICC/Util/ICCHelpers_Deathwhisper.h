/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_DEATHWHISPER_H
#define PLAYERBOTS_ICCHELPERS_DEATHWHISPER_H

#include "ICCData.h"
#include "Playerbots.h"
#include <array>

inline const Position ICC_DARK_RECKONING_SAFE_POSITION = Position(-523.33386f, 2211.2031f, 62.823116f);
inline const Position ICC_LDW_TANK_POSTION = Position(-593.7436f, 2211.298f, 49.476673f); //old closer to stairs -570.1f, 2211.2456f, 49.476616f
inline const Position ICC_LDW_RANGED_POSITION = Position(-653.11096f, 2211.9568f,51.551437f); //old in frint of the boss (-607.0631f, 2212.2488f, 49.47009f)

// All fanatics and adherents entry ids Lady Deathwhisper
static const std::array<uint32, 23> addEntriesLady    = {
                                                        37949, 38394, 38625, 38626, 38010, 38397, 39000, 39001,
                                                        38136, 38396, 38632, 38633, 37890, 38393, 38628, 38629,
                                                        38135, 38395, 38634, 38009, 38398, 38630, 38631};

#endif
