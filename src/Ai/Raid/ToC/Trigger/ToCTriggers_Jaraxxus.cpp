#include "ToCTriggers_Jaraxxus.h"
#include "ToCData.h"
#include "ToCHelpers_Shared.h"
#include "ToCHelpers_Jaraxxus.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "Strategy.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

bool JaraxxusEngagedByMainTankTrigger::IsActive()
{
    return botAI->IsMainTank(bot) &&
           GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_JARAXXUS));
}

bool JaraxxusAddNeedsAssistTankTrigger::IsActive()
{
    return botAI->IsAssistTankOfIndex(bot, 0, false) && GetPriorityJaraxxusAdd(botAI);
}

bool JaraxxusSecondAddNeedsAssistTankTrigger::IsActive()
{
    // A second assist tank picks up the overlapping add so it never free-roams onto the raid
    return botAI->IsAssistTankOfIndex(bot, 1, false) && GetSecondaryJaraxxusAdd(botAI);
}

bool JaraxxusAddShouldBeFocusedTrigger::IsActive()
{
    // Damage dealers burn the adds down; tanks and healers keep their assignments
    if (botAI->IsTank(bot) || botAI->IsHeal(bot))
        return false;

    return GetPriorityJaraxxusAdd(botAI) != nullptr;
}

bool JaraxxusLegionFlameNearbyTrigger::IsActive()
{
    // The pursuing fire is lethal to anyone standing in it, tanks included; everyone steps out
    constexpr float legionFlameRadius = 6.0f;
    return GetNearestCreatureByEntry(bot, static_cast<uint32>(ToCNpcs::NPC_LEGION_FLAME), legionFlameRadius) != nullptr;
}

bool JaraxxusIncinerateFleshOnRaidTrigger::IsActive()
{
    if (!botAI->IsHeal(bot))
        return false;

    GuidVector const& members = AI_VALUE(GuidVector, "group members");
    for (ObjectGuid const& guid : members)
    {
        Unit* member = botAI->GetUnit(guid);
        if (member && member->IsAlive() &&
            member->HasAura(static_cast<uint32>(ToCSpells::SPELL_INCINERATE_FLESH)))
        {
            return true;
        }
    }

    return false;
}

bool JaraxxusNetherPowerActiveTrigger::IsActive()
{
    // Only classes with an offensive magic dispel can strip the buff
    switch (bot->getClass())
    {
        case CLASS_MAGE:
        case CLASS_PRIEST:
        case CLASS_SHAMAN:
            break;
        default:
            return false;
    }

    return JaraxxusHasNetherPower(GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_JARAXXUS)));
}

bool JaraxxusFelFireballInterruptibleTrigger::IsActive()
{
    // Reinforce the interrupt only when there are no adds to fight, so add damage is not lost.
    // While adds are up the boss tank's own class interrupt handles Fel Fireball.
    if (botAI->IsTank(bot) || botAI->IsHeal(bot) || GetPriorityJaraxxusAdd(botAI))
        return false;

    Unit* jaraxxus = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_JARAXXUS));
    return jaraxxus && jaraxxus->FindCurrentSpellBySpellId(static_cast<uint32>(ToCSpells::SPELL_FEL_FIREBALL));
}

void AddToCJaraxxusTriggerNodes(std::vector<TriggerNode*>& triggers)
{
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
