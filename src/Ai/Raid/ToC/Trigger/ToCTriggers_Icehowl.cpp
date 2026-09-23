#include "ToCTriggers_Icehowl.h"
#include "ToCData.h"
#include "ToCHelpers_Icehowl.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "Strategy.h"

using namespace TrialOfTheCrusaderHelpers;
using namespace EncounterHelpers;

bool IcehowlEngagedByMainTankTrigger::IsActive()
{
    return botAI->IsMainTank(bot) &&
           GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_ICEHOWL));
}

bool IcehowlChargeIncomingTrigger::IsActive()
{
    Unit* icehowl = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_ICEHOWL));
    if (!icehowl)
        return false;

    // During the jump/charge sequence Icehowl drops his victim; the Massive Crash aura is the
    // early warning. Require him to be in combat so this never fires before the pull, and only
    // react if the bot is actually inside the charge corridor.
    bool const chargePhase = HasMassiveCrashAura(bot) || (icehowl->IsInCombat() && !icehowl->GetVictim());
    if (!chargePhase)
        return false;

    constexpr float corridorHalfWidth = 14.0f;
    return IsBotInChargeCorridor(bot, icehowl, corridorHalfWidth);
}

void AddToCIcehowlTriggerNodes(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("icehowl engaged by main tank", {
        NextAction("icehowl main tank hold boss", ACTION_RAID + 1) }));

    triggers.push_back(new TriggerNode("icehowl charge incoming", {
        NextAction("icehowl clear charge path", ACTION_EMERGENCY + 8) }));
}
