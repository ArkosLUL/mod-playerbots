/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_EOEMULTIPLIERS_H
#define PLAYERBOTS_EOEMULTIPLIERS_H

#include "Multiplier.h"

class MalygosMultiplier : public Multiplier
{
public:
    MalygosMultiplier(PlayerbotAI* ai) : Multiplier(ai, "malygos") {}

    float GetValue(Action* action) override;

private:
    void RefreshSnapshot();

    // GetValue runs once per queued action per bot per tick, and the role lookups behind it walk
    // the whole group. The phase deliberately stays out: getPhase has its own window, and a
    // second one on top would enforce the previous phase's rules after the actions moved on.
    uint32 snapshotAtMs = 0;
    bool isMainTank = false;
    bool isDps = false;
    bool isRanged = false;
    bool isHeal = false;
    bool isBossTank = false;
    bool isBossVictim = false;
};

#endif
