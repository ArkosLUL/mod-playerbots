#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_RESISTAURA_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_RESISTAURA_H

#include <vector>

#include "NamedObjectContext.h"
#include "Trigger.h"

// A paladin or hunter whose resistance duty isn't up yet. No encounter prefix, so the gate never
// wraps it: the wanted schools carry their own window, staging included.
class ToCResistAuraTrigger : public Trigger
{
public:
    ToCResistAuraTrigger(PlayerbotAI* botAI) : Trigger(botAI, "toc resistance aura") {}
    bool IsActive() override;
};

class ToCResistAuraTriggerContext : public NamedObjectContext<Trigger>
{
public:
    ToCResistAuraTriggerContext() : NamedObjectContext<Trigger>()
    {
        creators["toc resistance aura"] = &ToCResistAuraTriggerContext::toc_resistance_aura;
    }

private:
    static Trigger* toc_resistance_aura(PlayerbotAI* botAI) { return new ToCResistAuraTrigger(botAI); }
};

void AddToCResistAuraTriggerNodes(std::vector<TriggerNode*>& triggers);

#endif
