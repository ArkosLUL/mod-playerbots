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

    // Lord Jaraxxus
    triggers.push_back(new TriggerNode("jaraxxus engaged by main tank", {
        NextAction("jaraxxus main tank hold boss", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("jaraxxus add needs assist tank", {
        NextAction("jaraxxus assist tank hold add", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode("jaraxxus second add needs assist tank", {
        NextAction("jaraxxus assist tank hold second add", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode("jaraxxus add should be focused", {
        NextAction("jaraxxus focus add", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode("jaraxxus nether power active", {
        NextAction("jaraxxus remove nether power", ACTION_RAID + 1) }));

    // Above routine maintenance healing (so the absorb gets topped off promptly) but below a
    // critical heal, so a genuinely dying tank is never starved to feed the Incinerate target.
    triggers.push_back(new TriggerNode("jaraxxus incinerate flesh on raid", {
        NextAction("jaraxxus heal incinerate target", ACTION_MEDIUM_HEAL + 5) }));

    triggers.push_back(new TriggerNode("jaraxxus fel fireball interruptible", {
        NextAction("jaraxxus interrupt fel fireball", ACTION_EMERGENCY) }));

    triggers.push_back(new TriggerNode("jaraxxus legion flame nearby", {
        NextAction("jaraxxus avoid legion flame", ACTION_EMERGENCY + 8) }));
}

void RaidTrialOfTheCrusaderStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new IcehowlSuppressMovementDuringChargeMultiplier(botAI));
    multipliers.push_back(new NorthrendBeastsControlTankMovementMultiplier(botAI));
    multipliers.push_back(new JaraxxusControlTankMovementMultiplier(botAI));
}
