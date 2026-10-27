/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_BLOODPRINCES_H
#define PLAYERBOTS_ICCHELPERS_BLOODPRINCES_H

#include "ICCData.h"
#include "Playerbots.h"

inline const Position ICC_BPC_OT_POSITION = Position(4649.2236f, 2796.0972f, 361.1815f);
inline const Position ICC_BPC_MT_POSITION = Position(4648.5674f, 2744.847f, 361.18222f);
inline const Position ICC_BPC_CENTER_POSITION = Position(4638.7056f, 2769.3713f, 361.17108f);

static float const BPC_FLOOR_Z = 361.18222f;

// The prince Invocation of Blood is on, the only one that takes damage: Valanar from the pull, then one of
// the other two at random, then the last, moving every 46 s. Null while none carries it. `present` says
// whether the bot sees any prince at all. Publishes bpc.phase.
Unit* IccBpcEmpoweredPrince(PlayerbotAI* botAI, bool* present = nullptr);
// The Kinetic Bomb this ranged dps bot is to shoot, from one greedy pass over the lowest bombs: hunters
// first, then druids, then anyone within 80 yd. Null for a bot left without one. Publishes bpc.bomb.
Unit* IccBpcAssignedKineticBomb(PlayerbotAI* botAI);

#endif
