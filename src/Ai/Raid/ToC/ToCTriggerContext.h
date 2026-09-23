#ifndef PLAYERBOTS_RAID_TOCTRIGGERCONTEXT_H
#define PLAYERBOTS_RAID_TOCTRIGGERCONTEXT_H

#include "Log.h"
#include "NamedObjectContext.h"
#include "ToCRaidTriggers.h"

// Boss stems register their triggers in their own contexts, this one only merges them.
class RaidTrialOfTheCrusaderTriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidTrialOfTheCrusaderTriggerContext() : NamedObjectContext<Trigger>()
    {
        Absorb(ToCGormokTriggerContext());
        Absorb(ToCJormungarsTriggerContext());
        Absorb(ToCIcehowlTriggerContext());
        Absorb(ToCNorthrendBeastsTriggerContext());
        Absorb(ToCJaraxxusTriggerContext());
        Absorb(ToCAnubarakTriggerContext());
        Absorb(ToCFactionChampionsTriggerContext());
        Absorb(ToCTwinValkyrTriggerContext());
    }

private:
    void Absorb(NamedObjectContext<Trigger> const& stem)
    {
        for (auto const& [name, creator] : stem.creators)
            if (!creators.emplace(name, creator).second)
                LOG_ERROR("playerbots", "ToC trigger context: duplicate creator '{}'", name);
    }
};

#endif
