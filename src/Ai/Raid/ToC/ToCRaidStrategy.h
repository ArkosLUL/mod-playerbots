#ifndef PLAYERBOTS_RAID_TOCRAIDSTRATEGY_H
#define PLAYERBOTS_RAID_TOCRAIDSTRATEGY_H

#include "Strategy.h"

class RaidTrialOfTheCrusaderStrategy : public Strategy
{
public:
    RaidTrialOfTheCrusaderStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    std::string const getName() override { return "trialofthecrusader"; }

    void OnTick() override;
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;

private:
    bool _legacyRtiChecked = false;
};

#endif
