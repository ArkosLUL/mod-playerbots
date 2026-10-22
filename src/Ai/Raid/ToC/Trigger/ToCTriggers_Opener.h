#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_OPENER_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_OPENER_H

#include <vector>

#include "NamedObjectContext.h"
#include "Trigger.h"

// A DPS bot's auto attack running on a boss still in its opener: picking the boss starts the swing.
// No encounter prefix, the opener carries its own window.
class ToCOpenerSwingTrigger : public Trigger
{
public:
    ToCOpenerSwingTrigger(PlayerbotAI* botAI) : Trigger(botAI, "toc opener swing") {}
    bool IsActive() override;
};

// A hunter or rogue waiting on a boss a tank holds, with Misdirection or Tricks of the Trade ready
class ToCOpenerRedirectTrigger : public Trigger
{
public:
    ToCOpenerRedirectTrigger(PlayerbotAI* botAI) : Trigger(botAI, "toc opener redirect") {}
    bool IsActive() override;
};

class ToCOpenerTriggerContext : public NamedObjectContext<Trigger>
{
public:
    ToCOpenerTriggerContext() : NamedObjectContext<Trigger>()
    {
        creators["toc opener swing"] = &ToCOpenerTriggerContext::toc_opener_swing;
        creators["toc opener redirect"] = &ToCOpenerTriggerContext::toc_opener_redirect;
    }

private:
    static Trigger* toc_opener_swing(PlayerbotAI* botAI) { return new ToCOpenerSwingTrigger(botAI); }
    static Trigger* toc_opener_redirect(PlayerbotAI* botAI) { return new ToCOpenerRedirectTrigger(botAI); }
};

void AddToCOpenerTriggerNodes(std::vector<TriggerNode*>& triggers);

#endif
