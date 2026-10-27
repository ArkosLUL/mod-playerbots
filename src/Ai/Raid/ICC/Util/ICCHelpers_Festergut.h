/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_FESTERGUT_H
#define PLAYERBOTS_ICCHELPERS_FESTERGUT_H

#include "ICCData.h"
#include "Playerbots.h"

inline const Position ICC_FESTERGUT_TANK_POSITION = Position(4269.1772f, 3144.7673f, 360.38577f);
inline const Position ICC_FESTERGUT_RANGED_SPORE = Position(4264.3623f, 3120.889f, 360.38565f); //old closer to gates 4261.143f, 3109.4146f, 360.38605f
inline const Position ICC_FESTERGUT_RANGED_SPORE_2 = Position(4288.7974f, 3115.6274f, 360.38577f); //new position
inline const Position ICC_FESTERGUT_MELEE_SPORE = Position(4269.1772f, 3144.7673f, 360.38577f);

// Festergut's spore cycle: true while any group member carries Gas Spore. Publishes fg.phase.
bool IccFestergutSporeCycle(Player* bot);

#endif
