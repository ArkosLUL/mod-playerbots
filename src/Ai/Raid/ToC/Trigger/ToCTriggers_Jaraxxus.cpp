#include "ToCTriggers_Jaraxxus.h"
#include "ToCData.h"
#include "ToCHelpers_Defensive.h"
#include "ToCHelpers_Jaraxxus.h"
#include "ToCHelpers_Shared.h"
#include "Playerbots.h"
#include "PlayerbotAIConfig.h"
#include "Strategy.h"

using namespace TrialOfTheCrusaderHelpers;

bool JaraxxusFelFireballInterruptibleTrigger::IsActive()
{
    // every tick, not only mid-cast, so jaraxxus.interrupter drops as the cast ends
    Unit* jaraxxus = GetJaraxxus(botAI);
    return jaraxxus && JaraxxusIsFelFireballInterrupter(bot, jaraxxus);
}

bool JaraxxusLegionFlameNearbyTrigger::IsActive()
{
    return GetJaraxxusFlameRole(bot) != JaraxxusFlameRole::None;
}

bool JaraxxusPinnedCastTrigger::IsActive()
{
    return !GetJaraxxusPinnedCasters(bot).empty();
}

bool JaraxxusPortalSpawnTrigger::IsActive()
{
    Unit* portal = GetJaraxxusPortalToClear(botAI);
    return portal && bot->GetExactDist2d(portal) < JARAXXUS_PORTAL_TRIGGER;
}

bool JaraxxusRaidCoverTrigger::IsActive()
{
    ToCPredictedHit hit;
    return GetToCRaidCover(botAI, GetJaraxxusBurningInferno(botAI, hit) ? &hit : nullptr) != ToCRaidCover::None;
}

bool JaraxxusPersonalDefensiveTrigger::IsActive()
{
    ToCPredictedHit hit;
    return static_cast<bool>(GetToCPersonalDefensive(botAI, GetJaraxxusPersonalHit(botAI, hit) ? &hit : nullptr));
}

bool JaraxxusEngagedByMainTankTrigger::IsActive() { return JaraxxusMainTankHolds(botAI); }

bool JaraxxusAddNeedsAssistTankTrigger::IsActive() { return JaraxxusAssistTankHolds(botAI, 0); }

bool JaraxxusSecondAddNeedsAssistTankTrigger::IsActive() { return JaraxxusAssistTankHolds(botAI, 1); }

bool JaraxxusNetherPowerActiveTrigger::IsActive()
{
    // stack read first, every bot's tick keeps jaraxxus.netherpower current
    return JaraxxusNetherPowerStacks(botAI) > 0 && JaraxxusIsNetherPowerRemover(bot);
}

bool JaraxxusDpsTargetTrigger::IsActive()
{
    Unit* target = GetJaraxxusDpsTarget(botAI);
    return target && (AI_VALUE(Unit*, "current target") != target || ToCPetOffPick(botAI, target));
}

bool JaraxxusIncinerateFleshOnRaidTrigger::IsActive()
{
    if (!PlayerbotAI::IsHeal(bot))
        return false;

    Unit* target = GetIncinerateFleshTarget(botAI);
    return target && bot->GetDistance(target) <= sPlayerbotAIConfig.HealDistance && bot->IsWithinLOSInMap(target);
}

void AddToCJaraxxusTriggerNodes(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("jaraxxus fel fireball interruptible", {
        NextAction("jaraxxus interrupt fel fireball", ACTION_EMERGENCY + 9) }));

    // above the flame walk: an instant goes out on the move, and a carrier's walk holds every tick
    triggers.push_back(new TriggerNode("jaraxxus raid cover", {
        NextAction("jaraxxus cast raid cover", ACTION_EMERGENCY + 8.6f) }));

    triggers.push_back(new TriggerNode("jaraxxus personal defensive", {
        NextAction("jaraxxus cast personal defensive", ACTION_EMERGENCY + 8.4f) }));

    triggers.push_back(new TriggerNode("jaraxxus legion flame nearby", {
        NextAction("jaraxxus avoid legion flame", ACTION_EMERGENCY + 8) }));

    triggers.push_back(new TriggerNode("jaraxxus pinned cast", {
        NextAction("jaraxxus break pinned cast", ACTION_EMERGENCY + 7) }));

    triggers.push_back(new TriggerNode("jaraxxus portal spawn", {
        NextAction("jaraxxus leave portal", ACTION_EMERGENCY + 6) }));

    triggers.push_back(new TriggerNode("jaraxxus engaged by main tank", {
        NextAction("jaraxxus main tank hold boss", ACTION_RAID + 5) }));

    triggers.push_back(new TriggerNode("jaraxxus add needs assist tank", {
        NextAction("jaraxxus assist tank hold add", ACTION_RAID + 4) }));

    triggers.push_back(new TriggerNode("jaraxxus second add needs assist tank", {
        NextAction("jaraxxus assist tank hold second add", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode("jaraxxus nether power active", {
        NextAction("jaraxxus remove nether power", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode("jaraxxus dps target", {
        NextAction("jaraxxus dps target", ACTION_RAID + 1) }));

    // health reads full under the absorb, so no generic heal picks the target. stays under critical heals
    triggers.push_back(new TriggerNode("jaraxxus incinerate flesh on raid", {
        NextAction("jaraxxus heal incinerate target", ACTION_MEDIUM_HEAL + 5) }));
}
