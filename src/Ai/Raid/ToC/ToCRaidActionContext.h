#ifndef PLAYERBOTS_RAID_TOCRAIDACTIONCONTEXT_H
#define PLAYERBOTS_RAID_TOCRAIDACTIONCONTEXT_H

#include "NamedObjectContext.h"
#include "ToCActions_FactionChampionsDefence.h"
#include "ToCDefinitions.h"

class RaidTrialOfTheCrusaderActionContext : public NamedObjectContext<Action>
{
public:
    RaidTrialOfTheCrusaderActionContext()
    {
        for (EncounterDefinition const* encounter : ToCEncounterDefinitions())
            encounter->RegisterActions(creators);

        // its node rides the chat command's "pull" trigger, which no row can register
        creators["faction champions chain request"] = &RaidTrialOfTheCrusaderActionContext::chain_request;
    }

private:
    static Action* chain_request(PlayerbotAI* botAI) { return new FactionChampionsChainRequestAction(botAI); }
};

#endif
