#include "ToCTriggers_Gormok.h"
#include "ToCData.h"
#include "ToCHelpers_Gormok.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "Strategy.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

bool GormokEngagedByMainTankTrigger::IsActive()
{
    return botAI->IsMainTank(bot) &&
           GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_GORMOK));
}

bool GormokSnoboldOnRaidTrigger::IsActive()
{
    // Only melee DPS peel onto Snobolds; ranged keep damaging Gormok so the boss still dies
    if (botAI->IsTank(bot) || botAI->IsHeal(bot) || !botAI->IsMelee(bot))
        return false;

    return GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_GORMOK)) &&
           GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_SNOBOLD_VASSAL));
}

bool GormokTankSwapNeededTrigger::IsActive()
{
    // Either tank (main or first assist) taunts when the OTHER tank is the one currently holding Gormok
    // and is carrying a lethal Impale stack count. With two tanks this ping-pongs the boss between them.
    if (!botAI->IsMainTank(bot) && !botAI->IsAssistTankOfIndex(bot, 0, false))
        return false;

    Unit* gormok = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_GORMOK));
    if (!gormok)
        return false;

    Unit* victim = gormok->GetVictim();
    if (!victim || victim == bot)
        return false;

    return GetGormokImpaleStacks(victim) >= GORMOK_IMPALE_SWAP_STACKS;
}

void AddToCGormokTriggerNodes(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("gormok engaged by main tank", {
        NextAction("gormok main tank hold boss", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("gormok snobold on raid", {
        NextAction("gormok focus snobold", ACTION_RAID + 2) }));

    // Off-tank taunts once the current tank's Impale bleed stacks up, so the two tanks trade the boss
    triggers.push_back(new TriggerNode("gormok tank swap needed", {
        NextAction("gormok tank swap taunt", ACTION_RAID + 5) }));
}
