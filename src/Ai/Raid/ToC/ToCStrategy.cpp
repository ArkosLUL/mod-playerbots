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

    // Anub'arak
    triggers.push_back(new TriggerNode("anubarak engaged by main tank", {
        NextAction("anubarak main tank hold boss", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("anubarak burrower needs assist tank", {
        NextAction("anubarak assist tank hold burrower", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode("anubarak burrower should be focused", {
        NextAction("anubarak focus burrower", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode("anubarak scarab on raid", {
        NextAction("anubarak focus scarab", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode("anubarak ranged should seed permafrost", {
        NextAction("anubarak destroy frost sphere", ACTION_RAID) }));

    // The spike-chase target drops everything to kite the spike into Permafrost
    triggers.push_back(new TriggerNode("anubarak pursued by spike", {
        NextAction("anubarak kite spike to permafrost", ACTION_EMERGENCY + 8) }));

    // Faction Champions. Interrupts are intentionally not handled here: every caster bot already runs
    // an always-on class behaviour that interrupts enemy healers (mage "counterspell on enemy healer",
    // rogue kick, etc.), so focusing the healer is enough for those kicks to land.
    triggers.push_back(new TriggerNode("faction champions should focus", {
        NextAction("faction champions focus priority", ACTION_RAID + 2) }));

    // Twin Val'kyr. The twins share health, so the main tank skull-marks Fjola and non-tank DPS focus
    // her via the default "dps assist" (killing one kills both). The colour-matching Essence system is
    // the survival core: bots grab an essence at the pull and swap to match the active Vortex / heroic
    // Touch. Powering Up (orb collection) is intentionally out of scope; the enrage is met via the
    // shared-health focus-fire, not the DPS buff.
    triggers.push_back(new TriggerNode("twin valkyr engaged by main tank", {
        NextAction("twin valkyr main tank hold light twin", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("twin valkyr darkbane needs assist tank", {
        NextAction("twin valkyr assist tank hold dark twin", ACTION_RAID + 2) }));

    // Touch outranks Vortex: a touched bot is taking a personal heavy DoT that only the colour swap stops
    triggers.push_back(new TriggerNode("twin valkyr touched requires essence", {
        NextAction("twin valkyr swap essence for touch", ACTION_EMERGENCY + 6) }));

    triggers.push_back(new TriggerNode("twin valkyr vortex requires essence", {
        NextAction("twin valkyr swap essence for vortex", ACTION_EMERGENCY + 5) }));

    triggers.push_back(new TriggerNode("twin valkyr needs initial essence", {
        NextAction("twin valkyr acquire initial essence", ACTION_RAID) }));
}

void RaidTrialOfTheCrusaderStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new IcehowlSuppressMovementDuringChargeMultiplier(botAI));
    multipliers.push_back(new NorthrendBeastsControlTankMovementMultiplier(botAI));
    multipliers.push_back(new JaraxxusControlTankMovementMultiplier(botAI));
    multipliers.push_back(new AnubarakControlTankMovementMultiplier(botAI));
    multipliers.push_back(new AnubarakProtectSpikeKiteMultiplier(botAI));
    multipliers.push_back(new AnubarakDelayBloodlustUntilLeechingSwarmMultiplier(botAI));
    multipliers.push_back(new FactionChampionsSuppressAoeMultiplier(botAI));
    multipliers.push_back(new TwinValkyrControlTankMovementMultiplier(botAI));
    multipliers.push_back(new TwinValkyrPrioritizeEssenceSwapMultiplier(botAI));
}
