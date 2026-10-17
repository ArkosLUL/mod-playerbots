#ifndef PLAYERBOTS_RAID_TOCACTIONS_STAGING_H
#define PLAYERBOTS_RAID_TOCACTIONS_STAGING_H

#include <string>

#include "NamedObjectContext.h"
#include "ToCActions_NorthrendBeasts.h"

// Out of combat: walks only, never attacks, or the raid pulls itself
class ToCMoveToStagingStandAction : public NorthrendBeastsWalkAction
{
public:
    ToCMoveToStagingStandAction(PlayerbotAI* botAI, std::string const name = "toc move to staging stand")
        : NorthrendBeastsWalkAction(botAI, name) {};
    bool Execute(Event event) override;
};

class ToCStagingActionContext : public NamedObjectContext<Action>
{
public:
    ToCStagingActionContext()
    {
        creators["toc move to staging stand"] = &ToCStagingActionContext::toc_move_to_staging_stand;
    }

private:
    static Action* toc_move_to_staging_stand(PlayerbotAI* botAI) { return new ToCMoveToStagingStandAction(botAI); }
};

#endif
