/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_BURSTWINDOWSTRATEGY_H
#define PLAYERBOTS_BURSTWINDOWSTRATEGY_H

#include "BurstCooldowns.h"
#include "Strategy.h"

class PlayerbotAI;

class HoldBurstUntilTankEngagedMultiplier : public Multiplier
{
public:
    HoldBurstUntilTankEngagedMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "hold burst until tank engaged") {}

    float GetValue(Action* action) override;

private:
    // Lust goes out a second earlier so the personal cooldowns land inside the haste window.
    static constexpr uint32 LUST_DWELL_MS = 3000;
    static constexpr uint32 BURST_DWELL_MS = 4000;

    BurstHoldState holdState;
};

class BurstWindowStrategy : public Strategy
{
public:
    BurstWindowStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
    std::string const getName() override { return "burst"; }
};

#endif
