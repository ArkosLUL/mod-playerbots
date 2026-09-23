#ifndef PLAYERBOTS_RAID_TOCACTIONCONTEXT_H
#define PLAYERBOTS_RAID_TOCACTIONCONTEXT_H

#include "Log.h"
#include "NamedObjectContext.h"
#include "ToCRaidActions.h"

// Boss stems register their actions in their own contexts, this one only merges them.
class RaidTrialOfTheCrusaderActionContext : public NamedObjectContext<Action>
{
public:
    RaidTrialOfTheCrusaderActionContext()
    {
        Absorb(ToCGormokActionContext());
        Absorb(ToCJormungarsActionContext());
        Absorb(ToCIcehowlActionContext());
        Absorb(ToCNorthrendBeastsActionContext());
        Absorb(ToCJaraxxusActionContext());
        Absorb(ToCAnubarakActionContext());
        Absorb(ToCFactionChampionsActionContext());
        Absorb(ToCTwinValkyrActionContext());
    }

private:
    void Absorb(NamedObjectContext<Action> const& stem)
    {
        for (auto const& [name, creator] : stem.creators)
            if (!creators.emplace(name, creator).second)
                LOG_ERROR("playerbots", "ToC action context: duplicate creator '{}'", name);
    }
};

#endif
