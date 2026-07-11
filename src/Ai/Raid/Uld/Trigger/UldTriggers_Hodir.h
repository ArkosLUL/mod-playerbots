#ifndef PLAYERBOTS_ULDTRIGGERS_HODIR_H
#define PLAYERBOTS_ULDTRIGGERS_HODIR_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "UldBossHelper.h"
#include "Trigger.h"

//
// Hodir
//
class HodirBitingColdTrigger : public Trigger
{
public:
    HodirBitingColdTrigger(PlayerbotAI* ai) : Trigger(ai, "hodir biting cold") {}
    bool IsActive() override;
};

class HodirNearSnowpackedIcicleTrigger : public Trigger
{
public:
    HodirNearSnowpackedIcicleTrigger(PlayerbotAI* ai) : Trigger(ai, "hodir near snowpacked icicle") {}
    bool IsActive() override;
};

#endif
