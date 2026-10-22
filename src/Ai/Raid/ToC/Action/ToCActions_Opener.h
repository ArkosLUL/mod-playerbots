#ifndef PLAYERBOTS_RAID_TOCACTIONS_OPENER_H
#define PLAYERBOTS_RAID_TOCACTIONS_OPENER_H

#include "Action.h"
#include "NamedObjectContext.h"
#include "RaidRedirectThreat.h"

// Ends the swing a boss pick started. The boss stays the bot's target, so melee keep walking in.
class ToCOpenerStopSwingAction : public Action
{
public:
    ToCOpenerStopSwingAction(PlayerbotAI* botAI) : Action(botAI, "toc opener stop swing") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Misdirection or Tricks of the Trade on the tank holding the boss, which frees the bot's damage
class ToCOpenerRedirectAction : public RaidRedirectThreatAction
{
public:
    ToCOpenerRedirectAction(PlayerbotAI* botAI) : RaidRedirectThreatAction(botAI, "toc opener redirect") {}

protected:
    Player* GetRedirectTank() override;
    Unit* GetThreatDumpTarget() override { return nullptr; }
};

class ToCOpenerActionContext : public NamedObjectContext<Action>
{
public:
    ToCOpenerActionContext()
    {
        creators["toc opener stop swing"] = &ToCOpenerActionContext::toc_opener_stop_swing;
        creators["toc opener redirect"] = &ToCOpenerActionContext::toc_opener_redirect;
    }

private:
    static Action* toc_opener_stop_swing(PlayerbotAI* botAI) { return new ToCOpenerStopSwingAction(botAI); }
    static Action* toc_opener_redirect(PlayerbotAI* botAI) { return new ToCOpenerRedirectAction(botAI); }
};

#endif
