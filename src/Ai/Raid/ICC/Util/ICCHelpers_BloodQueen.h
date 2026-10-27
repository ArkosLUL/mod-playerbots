/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_BLOODQUEEN_H
#define PLAYERBOTS_ICCHELPERS_BLOODQUEEN_H

#include "ICCData.h"
#include "Playerbots.h"

inline const Position ICC_BQL_CENTER_POSITION = Position(4595.0f, 2769.0f, 400.0f);
inline const Position ICC_BQL_LWALL1_POSITION = Position(4624.685f, 2789.4895f, 400.13834f);
inline const Position ICC_BQL_LWALL2_POSITION = Position(4600.749f, 2805.7568f, 400.1374f);
inline const Position ICC_BQL_LWALL3_POSITION = Position(4572.857f, 2797.3872f, 400.1374f);
inline const Position ICC_BQL_RWALL1_POSITION = Position(4625.724f, 2748.9917f, 400.13693f);
inline const Position ICC_BQL_RWALL2_POSITION = Position(4608.3774f, 2735.7466f, 400.13693f);
inline const Position ICC_BQL_RWALL3_POSITION = Position(4576.813f, 2739.6067f, 400.13693f);
inline const Position ICC_BQL_LRWALL4_POSITION = Position(4539.345f, 2769.3853f, 403.7267f);
inline const Position ICC_BQL_TANK_POSITION = Position(4633.7964f, 2769.2515f, 401.74777f); //old 4629.746f, 2769.6396f, 401.7479f

enum class IccBqlPhase : uint8
{
    Ground,
    Center,  // passive and walking to the room's centre, then Incite Terror there
    Air,     // gravity off, from takeoff until she lands
};

// From her own air phase: REACT_PASSIVE from the walk to the centre until she lands, gravity off from
// takeoff to landing. She carries CREATURE_FLAG_EXTRA_NO_MOVE_FLAGS_UPDATE, so only her script moves
// the gravity flag. Publishes bql.phase.
IccBqlPhase IccBqlPhaseOf(Player* bot, Unit* boss);

#endif
