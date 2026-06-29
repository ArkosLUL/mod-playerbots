#ifndef PLAYERBOTS_TOCSTRATEGY_H
#define PLAYERBOTS_TOCSTRATEGY_H

#include "Strategy.h"

class RaidTrialOfTheCrusaderStrategy : public Strategy
{
public:
    RaidTrialOfTheCrusaderStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    std::string const getName() override { return "trialofthecrusader"; }

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
};

#endif
