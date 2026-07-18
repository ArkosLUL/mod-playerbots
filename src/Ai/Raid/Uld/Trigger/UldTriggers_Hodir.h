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

//
// Hodir hard mode (config-gated): DPS-race behaviours to beat the 3-minute Rare Cache timer.
//

// A flash-frozen helper ice block is alive nearby - kill it to free the helper and unlock its buffs.
class HodirFreeFrozenHelperTrigger : public Trigger
{
public:
    HodirFreeFrozenHelperTrigger(PlayerbotAI* ai) : Trigger(ai, "hodir free frozen helper") {}
    bool IsActive() override;
};

// The bot carries Storm Cloud but is off on its own, so its Storm Power crit buff spreads to nobody.
class HodirSpreadStormCloudTrigger : public Trigger
{
public:
    HodirSpreadStormCloudTrigger(PlayerbotAI* ai) : Trigger(ai, "hodir spread storm cloud") {}
    bool IsActive() override;
};

// Biting Cold is stacking on the bot and it is not standing in a Toasty Fire (no-cheat mitigation).
class HodirMoveToToastyFireTrigger : public Trigger
{
public:
    HodirMoveToToastyFireTrigger(PlayerbotAI* ai) : Trigger(ai, "hodir move to toasty fire") {}
    bool IsActive() override;
};

#endif
