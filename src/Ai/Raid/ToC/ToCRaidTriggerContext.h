#ifndef PLAYERBOTS_RAID_TOCRAIDTRIGGERCONTEXT_H
#define PLAYERBOTS_RAID_TOCRAIDTRIGGERCONTEXT_H

#include "NamedObjectContext.h"
#include "ToCDefinitions.h"

class RaidTrialOfTheCrusaderTriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidTrialOfTheCrusaderTriggerContext() : NamedObjectContext<Trigger>()
    {
        for (EncounterDefinition const* encounter : ToCEncounterDefinitions())
            encounter->RegisterTriggers(creators);
    }
};

#endif
