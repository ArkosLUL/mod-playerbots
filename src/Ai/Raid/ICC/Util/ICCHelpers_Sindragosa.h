/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_SINDRAGOSA_H
#define PLAYERBOTS_ICCHELPERS_SINDRAGOSA_H

#include "ICCData.h"
#include "Playerbots.h"

inline const Position ICC_SINDRAGOSA_TANK_POSITION = Position(4408.016f, 2508.0647f, 203.37955f); //X: 4183.136 Y: 2492.9358 Z: 364.87595 Orientation: 5.3975997
inline const Position ICC_SINDRAGOSA_FLYING_POSITION = Position(4525.6f, 2485.15f, 245.082f);
inline const Position ICC_SINDRAGOSA_RANGED_POSITION = Position(4441.572f, 2484.482f, 203.37836f);
inline const Position ICC_SINDRAGOSA_MELEE_POSITION = Position(4423.1646f, 2486.6792f, 203.3749f); //old 4423.4546f, 2491.7175f, 203.37686f
inline const Position ICC_SINDRAGOSA_BLISTERING_COLD_POSITION = Position(4473.6616f, 2484.8489f, 203.38258f);
inline const Position ICC_SINDRAGOSA_THOMB1_POSITION = Position(4433.6484f, 2469.4133f, 203.3806f);
inline const Position ICC_SINDRAGOSA_THOMB2_POSITION = Position(4434.143f, 2486.201f, 203.37473f);
inline const Position ICC_SINDRAGOSA_THOMB3_POSITION = Position(4436.1147f, 2501.464f, 203.38266f);
inline const Position ICC_SINDRAGOSA_UNCHAINEDMAGIC1_POSITION = Position(4444.9707f, 2455.7322f, 203.38701f);
inline const Position ICC_SINDRAGOSA_UNCHAINEDMAGIC2_POSITION = Position(4461.3945f, 2463.5513f, 203.38727f);
inline const Position ICC_SINDRAGOSA_UNCHAINEDMAGIC3_POSITION = Position(4473.6616f, 2484.8489f, 203.38258f);
inline const Position ICC_SINDRAGOSA_UNCHAINEDMAGIC4_POSITION = Position(4459.9336f, 2507.409f, 203.38606f);
inline const Position ICC_SINDRAGOSA_UNCHAINEDMAGIC5_POSITION = Position(4442.3096f, 2512.4688f, 203.38647f);
inline const Position ICC_SINDRAGOSA_CENTER_POSITION = Position(4408.0464f, 2484.478f, 203.37529f);
inline const Position ICC_SINDRAGOSA_THOMBMB2_POSITION = Position(4436.895f, 2498.1401f, 203.38133f);
inline const Position ICC_SINDRAGOSA_FBOMB_POSITION = Position(4453.861f, 2485.524f, 203.3787f);
inline const Position ICC_SINDRAGOSA_FBOMB10_POSITION = Position(4453.861f, 2485.524f, 203.3787f);
inline const Position ICC_SINDRAGOSA_LOS2_POSITION = Position(4441.8286f, 2501.946f, 203.38435f);

enum class IccSindragosaPhaseId : uint8
{
    Ground,
    Air,
    Last,
};

// Sindragosa's phase as her script runs it: Air from takeoff to touchdown, while it holds her passive,
// else Last once her health is no longer above 35%. Publishes sg.phase.
IccSindragosaPhaseId IccSindragosaPhase(Player* bot, Unit const* boss);
// In the air and within `radius` of her far air station, where the Frost Bombs fall. 30 yd covers that
// station alone, 50 yd also the near one 49.6 yd off, where she casts the Frost Beacons.
bool IccSindragosaOverAirStation(Player* bot, Unit const* boss, float radius);

bool IccBossCastingBlisteringCold(Unit* boss);

#endif
