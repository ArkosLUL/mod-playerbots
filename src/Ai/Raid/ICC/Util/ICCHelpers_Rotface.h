/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_ROTFACE_H
#define PLAYERBOTS_ICCHELPERS_ROTFACE_H

#include "ICCData.h"
#include "Playerbots.h"
#include <vector>

inline const Position ICC_ROTFACE_RANGED_POSITION_HC_1 = Position(4453.2085f, 3108.7488f, 360.38626f);
inline const Position ICC_ROTFACE_RANGED_POSITION_HC_2 = Position(4459.4390f, 3118.0210f, 360.38626f);
inline const Position ICC_ROTFACE_RANGED_POSITION_HC_3 = Position(4473.4062f, 3126.2861f, 360.38626f);
inline const Position ICC_ROTFACE_RANGED_POSITION_HC_4 = Position(4469.9370f, 3137.0173f, 360.38626f);
inline const Position ICC_ROTFACE_RANGED_POSITION_HC_5 = Position(4476.7583f, 3145.7160f, 360.38626f);
inline const Position ICC_ROTFACE_RANGED_POSITION_HC_6 = Position(4466.2100f, 3150.0630f, 360.38626f);
inline const Position ICC_ROTFACE_RANGED_POSITION_HC_7 = Position(4464.4297f, 3161.2317f, 360.38626f);
inline const Position ICC_ROTFACE_RANGED_POSITION_HC_8 = Position(4447.2900f, 3163.0322f, 360.38626f);
inline const Position ICC_ROTFACE_RANGED_POSITION_HC_9 = Position(4437.1250f, 3168.4062f, 360.38626f);
inline const Position ICC_ROTFACE_RANGED_POSITION_HC_10 = Position(4417.2217f, 3148.8909f, 360.38626f);
inline const Position ICC_ROTFACE_RANGED_POSITION_HC_11 = Position(4420.9727f, 3137.7700f, 360.38626f);
inline const Position ICC_ROTFACE_RANGED_POSITION_HC_12 = Position(4414.8716f, 3127.9534f, 360.38626f);
inline const Position ICC_ROTFACE_RANGED_POSITION_HC_13 = Position(4425.1553f, 3123.4717f, 360.38626f);
inline const Position ICC_ROTFACE_RANGED_POSITION_HC_14 = Position(4426.3760f, 3113.4153f, 360.38626f);
inline const Position ICC_ROTFACE_RANGED_POSITION_HC_15 = Position(4443.8027f, 3113.9207f, 360.38626f);
inline const Position ICC_ROTFACE_RANGED_POSITION_HC_16 = Position(4432.477f, 3156.7651f, 360.38568f);
inline const Position ICC_ROTFACE_RANGED_POSITION_HC_17 = Position(4458.083f, 3132.5842f, 360.38565f);
inline const Position ICC_ROTFACE_RANGED_POSITION_HC_18 = Position(4457.4565f, 3144.8442f, 360.38565f);
inline const Position ICC_ROTFACE_RANGED_POSITION_HC_19 = Position(4422.5460f, 3158.0435f, 360.38565f);
inline const Position ICC_ROTFACE_RANGED_POSITION_HC_20 = Position(4432.1646f, 3142.3418f, 360.38565f);
inline const Position ICC_ROTFACE_RANGED_POSITION_HC_21 = Position(4436.649f, 3126.1245f, 360.38565f);
inline const Position ICC_ROTFACE_RANGED_POSITION_HC_22 = Position(4450.2363f, 3122.4033f, 360.38565f);
inline const Position ICC_ROTFACE_EXPLOSION_POSITION_1 = Position(4481.433f, 3137.0117f, 360.38522f);
inline const Position ICC_ROTFACE_EXPLOSION_POSITION_2 = Position(4474.4565f, 3156.9617f, 360.38522f);
inline const Position ICC_ROTFACE_EXPLOSION_POSITION_3 = Position(4452.0586f, 3170.2231f, 360.38522f);
inline const Position ICC_ROTFACE_EXPLOSION_POSITION_4 = Position(4421.2437f, 3161.2275f, 360.38522f);
inline const Position ICC_ROTFACE_EXPLOSION_POSITION_5 = Position(4410.904f, 3136.9976f, 360.38522f);
inline const Position ICC_ROTFACE_EXPLOSION_POSITION_6 = Position(4417.603f, 3119.4712f, 360.38522f);
inline const Position ICC_ROTFACE_EXPLOSION_POSITION_7 = Position(4442.514f, 3105.2234f, 360.38522f);
inline const Position ICC_ROTFACE_EXPLOSION_POSITION_8 = Position(4465.7173f, 3113.6284f, 360.38522f);
inline const Position ICC_ROTFACE_TANK_POSITION = Position(4447.061f, 3150.9758f, 360.38568f);
inline const Position ICC_ROTFACE_BIG_OOZE_POSITION = Position(4432.687f, 3142.3035f, 360.38568f);
inline const Position ICC_ROTFACE_SAFE_POSITION = Position(4446.557f, 3065.6594f, 360.38568f);
inline const Position ICC_ROTFACE_CENTER_POSITION = Position(4446.0547f, 3144.8677f, 360.38568f); //actual center 4.74089 4445.6616f, 3137.1526f, 360.38608
inline const Position ICC_ROTFACE_CENTER_POSITION_BOSS = Position(4445.656f, 3137.1663f, 360.38565f);

const std::vector<uint32> spellEntriesFlood           = {
                                                        69782, 69783, 69796, 69797, 69798,
                                                        69799, 69801, 69802, 69795};

#endif
