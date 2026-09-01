/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDMULTIPLIERS_FLAMELEVIATHAN_H
#define PLAYERBOTS_ULDMULTIPLIERS_FLAMELEVIATHAN_H

#include "Define.h"
#include "Multiplier.h"

// Flame Leviathan: one action owns every vehicle's movement, so the generic movers are shut out
// entirely while a bot is riding. Two actions steering one MotionMaster bounce rather than
// compromise, and lowering a priority only decides who wins each alternating tick.
class FlameLeviathanVehicleMovementMultiplier : public Multiplier
{
public:
    FlameLeviathanVehicleMovementMultiplier(PlayerbotAI* ai) : Multiplier(ai, "flame leviathan vehicle movement") {}
    float GetValue(Action* action) override;
};

#endif
