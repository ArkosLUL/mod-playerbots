/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_LICHKING_H
#define PLAYERBOTS_ICCHELPERS_LICHKING_H

#include "ICCData.h"
#include "Playerbots.h"

inline const Position ICC_LICH_KING_ADDS_POSITION = Position(476.7332f, -2095.3894f, 840.857f);  // old 486.63647f, -2095.7915f, 840.857f
inline const Position ICC_LICH_KING_MELEE_POSITION = Position(503.5546f, -2106.8213f, 840.857f);
inline const Position ICC_LICH_KING_RANGED_POSITION = Position(501.3563f, -2085.1816f, 840.857f);
inline const Position ICC_LICH_KING_ASSISTHC_POSITION = Position(517.2145f, -2125.0674f, 840.857f);
inline const Position ICC_LICH_KING_CENTER_POSITION = Position(503.62036f, -2124.7336f, 840.857f);
inline const Position ICC_LK_FROST1_POSITION = Position(503.96548f, -2183.216f, 840.857f);
inline const Position ICC_LK_FROST2_POSITION = Position(563.07166f, -2125.7578f, 840.857f);
inline const Position ICC_LK_FROST3_POSITION = Position(503.40182f, -2067.3435f, 840.857f);
inline const Position ICC_LK_FROSTR1_POSITION = Position(481.168f, -2177.8723f, 840.857f);
inline const Position ICC_LK_FROSTR2_POSITION = Position(562.20807f, -2100.2393f, 840.857f);
inline const Position ICC_LK_FROSTR3_POSITION = Position(526.35297f, -2071.0317f, 840.857f);
inline const Position ICC_LK_VILE_SPIRIT1_POSITION = Position(505.24002f, -2086.7778f, 840.857f);
inline const Position ICC_LK_VILE_SPIRIT2_POSITION = Position(532.668f, -2122.603f, 840.857f);
inline const Position ICC_LK_VILE_SPIRIT3_POSITION = Position(502.8796f, -2159.7466f, 840.857f);

// The Lich King's spelldifficulty rows, 10N/25N/10H/25H. The first Remorseless Winter (70%) and the
// second (40%) are separate rows, which is how the phase tells them apart.
inline constexpr uint32 ICC_LK_FIRST_WINTER[]  = {68981, 74270, 74271, 74272};
inline constexpr uint32 ICC_LK_SECOND_WINTER[] = {72259, 74273, 74274, 74275};
// Normal mode's single-player Harvest Soul, and heroic's Harvest Souls on the whole raid
inline constexpr uint32 ICC_LK_HARVEST_SOUL[]  = {68980, 74325, 74326, 74327};
inline constexpr uint32 ICC_LK_HARVEST_SOULS[] = {73654, 74295, 74296, 74297};

// Ordered, so "past the first threshold" reads phase >= Winter1.
enum class IccLichKingPhaseId : uint8
{
    One,
    Winter1,
    Two,
    Winter2,
    Three,
    Frostmourne,
};

// The Lich King's phase as his script runs it, for the bot asking: a winter phase from the turn to
// passive at 70% or 40% until he turns aggressive 5 s after the Quake, Frostmourne while heroic's
// Harvest Souls holds him passive. Publishes lk.phase. A unit that is not the Lich King reads One.
IccLichKingPhaseId IccLichKingPhase(Player* bot, Unit* boss);
// Inside the Frostmourne room, the 40 yd round its centre that the script itself tests.
bool IccLichKingInFrostmourne(Unit const* unit);

// 1 for the first Remorseless Winter, 2 for the second, 0 for none: the aura or its 2.5 s cast.
uint8 IccRemorselessWinterOf(Unit* boss);
bool IccBossHasRemorselessWinter(Unit* boss);
bool IccBossCastingRemorselessWinter(Unit* boss);

#endif
