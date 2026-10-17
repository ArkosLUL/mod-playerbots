#include "ToCTriggers_ResistAura.h"
#include "ToCHelpers_ResistAura.h"
#include "Playerbots.h"
#include "Strategy.h"

using namespace TrialOfTheCrusaderHelpers;

bool ToCResistAuraTrigger::IsActive() { return ToCResistAuraMissing(botAI); }

void AddToCResistAuraTriggerNodes(std::vector<TriggerNode*>& triggers)
{
    // Above the staging walk: the aura is instant, and the walk holds the tick while in flight
    triggers.push_back(new TriggerNode("toc resistance aura", {
        NextAction("toc raise resistance aura", ACTION_RAID + 1) }));
}
