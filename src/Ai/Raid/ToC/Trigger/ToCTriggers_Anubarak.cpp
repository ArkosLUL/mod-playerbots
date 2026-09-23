#include "ToCTriggers_Anubarak.h"
#include "ToCData.h"
#include "ToCHelpers_Anubarak.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "Creature.h"
#include "Strategy.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

bool AnubarakEngagedByMainTankTrigger::IsActive()
{
    // The boss is unselectable while submerged, so it drops out of "possible targets" and this
    // naturally goes idle during phase 2; it picks back up on the surface (phase 1 and phase 3).
    return botAI->IsMainTank(bot) &&
           GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_ANUBARAK));
}

bool AnubarakBurrowerNeedsAssistTankTrigger::IsActive()
{
    return botAI->IsAssistTankOfIndex(bot, 0, false) &&
           GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_NERUBIAN_BURROWER));
}

bool AnubarakBurrowerShouldBeFocusedTrigger::IsActive()
{
    // Damage dealers burn the burrowers; tanks and healers keep their assignments
    if (botAI->IsTank(bot) || botAI->IsHeal(bot))
        return false;

    return GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_NERUBIAN_BURROWER)) != nullptr;
}

bool AnubarakScarabOnRaidTrigger::IsActive()
{
    // Melee peel onto the scarabs during submerge; ranged seed Permafrost instead (see below)
    if (botAI->IsTank(bot) || botAI->IsHeal(bot) || !botAI->IsMelee(bot))
        return false;

    return GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_SWARM_SCARAB)) != nullptr;
}

bool AnubarakPursuedBySpikeTrigger::IsActive()
{
    return bot->HasAura(static_cast<uint32>(ToCSpells::SPELL_MARK));
}

bool AnubarakRangedShouldSeedPermafrostTrigger::IsActive()
{
    // Ranged destroy flying Frost Spheres while the boss is burrowed, dropping Permafrost patches
    // the spike-chase target can be kited through. The marked player never breaks off to do this.
    if (!botAI->IsRanged(bot) || botAI->IsHeal(bot))
        return false;

    if (bot->HasAura(static_cast<uint32>(ToCSpells::SPELL_MARK)) || !AnubarakSubmerged(botAI))
        return false;

    // A flying sphere is an alive Frost Sphere that has not yet grounded into a Permafrost patch
    std::list<Creature*> spheres;
    bot->GetCreatureListWithEntryInGrid(spheres, static_cast<uint32>(ToCNpcs::NPC_FROST_SPHERE), 100.0f);
    for (Creature* sphere : spheres)
    {
        if (sphere->IsAlive() && !sphere->HasAura(static_cast<uint32>(ToCSpells::SPELL_PERMAFROST)))
            return true;
    }

    return false;
}

void AddToCAnubarakTriggerNodes(std::vector<TriggerNode*>& triggers)
{
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
}
