#ifndef PLAYERBOTS_RAID_TOCACTIONS_RESISTAURA_H
#define PLAYERBOTS_RAID_TOCACTIONS_RESISTAURA_H

#include "Action.h"
#include "NamedObjectContext.h"

// Casts the duty's aura directly, no ChangeStrategy: an added strategy is never taken off again and
// outlives the raid. The hold rules keep the bot's own aura nodes off the slot meanwhile.
class ToCRaiseResistAuraAction : public Action
{
public:
    ToCRaiseResistAuraAction(PlayerbotAI* botAI) : Action(botAI, "toc raise resistance aura") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class ToCResistAuraActionContext : public NamedObjectContext<Action>
{
public:
    ToCResistAuraActionContext()
    {
        creators["toc raise resistance aura"] = &ToCResistAuraActionContext::toc_raise_resistance_aura;
    }

private:
    static Action* toc_raise_resistance_aura(PlayerbotAI* botAI) { return new ToCRaiseResistAuraAction(botAI); }
};

#endif
