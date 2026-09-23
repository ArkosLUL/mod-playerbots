#include "ToCTriggers_FactionChampions.h"
#include "ToCHelpers_FactionChampions.h"
#include "Playerbots.h"
#include "Strategy.h"

using namespace TrialOfTheCrusaderHelpers;

bool FactionChampionsShouldFocusTrigger::IsActive()
{
    // Healers keep healing the raid; every other role burns the focus target. There is no boss to
    // tank here (threat is artificial), so tanks join the damage dealers on the kill target.
    if (botAI->IsHeal(bot))
        return false;

    return GetPriorityFactionChampion(botAI) != nullptr;
}

void AddToCFactionChampionsTriggerNodes(std::vector<TriggerNode*>& triggers)
{
    // Faction Champions. Interrupts are intentionally not handled here: every caster bot already runs
    // an always-on class behaviour that interrupts enemy healers (mage "counterspell on enemy healer",
    // rogue kick, etc.), so focusing the healer is enough for those kicks to land.
    triggers.push_back(new TriggerNode("faction champions should focus", {
        NextAction("faction champions focus priority", ACTION_RAID + 2) }));
}
