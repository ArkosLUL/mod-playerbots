#include "ToCTriggers_Jaraxxus.h"
#include "ToCData.h"
#include "ToCHelpers_Jaraxxus.h"
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

bool JaraxxusIntroMainTankTrigger::IsActive()
{
    if (!botAI->IsMainTank(bot))
        return false;

    Unit* jaraxxus = GetJaraxxusInIntro(botAI);
    return jaraxxus && bot->GetExactDist2d(jaraxxus) > JARAXXUS_INTRO_STAND + 1.0f;
}

bool JaraxxusEngagedByMainTankTrigger::IsActive()
{
    if (!botAI->IsMainTank(bot))
        return false;

    // out of combat he's the respawn after a wipe, attackable: the retry pull is the leader's
    Unit* jaraxxus = GetJaraxxus(botAI);
    return jaraxxus && jaraxxus->IsInCombat();
}

bool JaraxxusAddNeedsAssistTankTrigger::IsActive()
{
    return botAI->IsAssistTankOfIndex(bot, 0, true) && GetJaraxxusAssistTankAdd(botAI, 0);
}

bool JaraxxusSecondAddNeedsAssistTankTrigger::IsActive()
{
    return botAI->IsAssistTankOfIndex(bot, 1, true) && GetJaraxxusAssistTankAdd(botAI, 1);
}

bool JaraxxusNetherPowerActiveTrigger::IsActive()
{
    // stack read first, every bot's tick keeps jaraxxus.netherpower current
    return JaraxxusNetherPowerStacks(botAI) > 0 && JaraxxusIsNetherPowerRemover(bot);
}

bool JaraxxusAddShouldBeFocusedTrigger::IsActive()
{
    if (PlayerbotAI::IsTank(bot) || PlayerbotAI::IsTank(bot, true) || botAI->IsHeal(bot))
        return false;

    return GetJaraxxusFocusAdd(botAI) != nullptr;
}

bool JaraxxusFocusStaleTrigger::IsActive()
{
    return JaraxxusOwnsRti(bot) && !JaraxxusAnyAddAlive(botAI);
}

bool JaraxxusIncinerateFleshOnRaidTrigger::IsActive()
{
    if (!botAI->IsHeal(bot))
        return false;

    Unit* target = GetIncinerateFleshTarget(botAI);
    return target && bot->GetDistance(target) <= sPlayerbotAIConfig.healDistance && bot->IsWithinLOSInMap(target);
}

void AddToCJaraxxusTriggerNodes(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("jaraxxus fel fireball interruptible", {
        NextAction("jaraxxus interrupt fel fireball", ACTION_EMERGENCY + 9) }));

    triggers.push_back(new TriggerNode("jaraxxus legion flame nearby", {
        NextAction("jaraxxus avoid legion flame", ACTION_EMERGENCY + 8) }));

    triggers.push_back(new TriggerNode("jaraxxus pinned cast", {
        NextAction("jaraxxus break pinned cast", ACTION_EMERGENCY + 7) }));

    triggers.push_back(new TriggerNode("jaraxxus intro main tank", {
        NextAction("jaraxxus intro main tank stand", ACTION_RAID + 6) }));

    triggers.push_back(new TriggerNode("jaraxxus engaged by main tank", {
        NextAction("jaraxxus main tank hold boss", ACTION_RAID + 5) }));

    triggers.push_back(new TriggerNode("jaraxxus add needs assist tank", {
        NextAction("jaraxxus assist tank hold add", ACTION_RAID + 4) }));

    triggers.push_back(new TriggerNode("jaraxxus second add needs assist tank", {
        NextAction("jaraxxus assist tank hold second add", ACTION_RAID + 3) }));

    triggers.push_back(new TriggerNode("jaraxxus nether power active", {
        NextAction("jaraxxus remove nether power", ACTION_RAID + 2) }));

    triggers.push_back(new TriggerNode("jaraxxus add should be focused", {
        NextAction("jaraxxus focus add", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("jaraxxus focus stale", {
        NextAction("jaraxxus reset focus", ACTION_RAID) }));

    // health reads full under the absorb, so no generic heal picks the target. stays under critical heals
    triggers.push_back(new TriggerNode("jaraxxus incinerate flesh on raid", {
        NextAction("jaraxxus heal incinerate target", ACTION_MEDIUM_HEAL + 5) }));
}
