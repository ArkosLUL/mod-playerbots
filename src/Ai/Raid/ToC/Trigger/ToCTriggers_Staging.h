#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_STAGING_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_STAGING_H

#include "Trigger.h"

class ToCStagingStandTrigger : public Trigger
{
public:
    static constexpr char const* Name = "toc staging stand";

    ToCStagingStandTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
