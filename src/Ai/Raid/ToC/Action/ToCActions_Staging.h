#ifndef PLAYERBOTS_RAID_TOCACTIONS_STAGING_H
#define PLAYERBOTS_RAID_TOCACTIONS_STAGING_H

#include "ToCActions_NorthrendBeasts.h"

// Out of combat: walks only, never attacks, or the raid pulls itself
class ToCMoveToStagingStandAction : public NorthrendBeastsWalkAction
{
public:
    static constexpr char const* Name = "toc move to staging stand";

    ToCMoveToStagingStandAction(PlayerbotAI* botAI) : NorthrendBeastsWalkAction(botAI, Name) {}
    bool Execute(Event event) override;
};

#endif
