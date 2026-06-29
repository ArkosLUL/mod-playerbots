#include "ToCStrategy.h"
#include "ToCMultipliers.h"

void RaidTrialOfTheCrusaderStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Northrend Beasts - Gormok the Impaler
    triggers.push_back(new TriggerNode("gormok engaged by main tank", {
        NextAction("gormok main tank hold boss", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("gormok snobold on raid", {
        NextAction("gormok focus snobold", ACTION_RAID + 2) }));

    // Northrend Beasts - Acidmaw & Dreadscale
    triggers.push_back(new TriggerNode("northrend worms mobile engaged by main tank", {
        NextAction("northrend worms main tank hold mobile worm", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("northrend worms stationary needs assist tank", {
        NextAction("northrend worms assist tank hold stationary worm", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("northrend worms ranged should spread", {
        NextAction("northrend worms spread", ACTION_EMERGENCY + 5) }));

    triggers.push_back(new TriggerNode("northrend worms afflicted by burning", {
        NextAction("northrend worms keep moving", ACTION_EMERGENCY + 5) }));

    // Northrend Beasts - Icehowl
    triggers.push_back(new TriggerNode("icehowl engaged by main tank", {
        NextAction("icehowl main tank hold boss", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("icehowl charge incoming", {
        NextAction("icehowl clear charge path", ACTION_EMERGENCY + 8) }));
}

void RaidTrialOfTheCrusaderStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new IcehowlSuppressMovementDuringChargeMultiplier(botAI));
    multipliers.push_back(new NorthrendBeastsControlTankMovementMultiplier(botAI));
}
