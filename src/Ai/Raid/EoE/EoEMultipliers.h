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

    // GetValue runs once per queued action per bot per tick, which is dozens of calls, and the role
    // lookups behind it are not cheap - IsMainTank walks every group member and each check scans
    // that member's strategy list. None of it changes between ticks, so it is resolved once per
    // window and read from here. Strategy::InitMultipliers builds one of these per bot, so the
    // snapshot is per bot with no sharing to worry about.
    // The phase deliberately stays out of it: getPhase has its own cache, and stacking a second
    // window on top would leave the multiplier applying the previous phase's rules for up to a
    // second after the actions had moved on.
    uint32 snapshotAtMs = 0;
    bool isMainTank = false;
    bool isDps = false;
    bool isRanged = false;
    bool isHeal = false;
    bool isBossTank = false;
    bool isBossVictim = false;
};

#endif
