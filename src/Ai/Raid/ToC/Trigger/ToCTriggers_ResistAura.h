#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_RESISTAURA_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_RESISTAURA_H

#include <vector>

#include "NamedObjectContext.h"
#include "RaidResistAura.h"
#include "ToCHelpers_ResistAura.h"
#include "Trigger.h"

// No encounter prefix, so the gate never wraps it: the wanted schools carry their own window, staging
// included.
class ToCResistAuraTriggerContext : public NamedObjectContext<Trigger>
{
public:
    ToCResistAuraTriggerContext() : NamedObjectContext<Trigger>()
    {
        creators["toc resistance aura"] = &ToCResistAuraTriggerContext::toc_resistance_aura;
    }

private:
    static Trigger* toc_resistance_aura(PlayerbotAI* botAI)
    {
        return new RaidResistAuraTrigger(botAI, "toc resistance aura", TrialOfTheCrusaderHelpers::TOC_RESIST_AURA);
    }
};

void AddToCResistAuraTriggerNodes(std::vector<TriggerNode*>& triggers);

#endif
