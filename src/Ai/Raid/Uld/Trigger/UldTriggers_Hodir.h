#ifndef PLAYERBOTS_ULDTRIGGERS_HODIR_H
#define PLAYERBOTS_ULDTRIGGERS_HODIR_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "Trigger.h"

class HodirBitingColdTrigger : public Trigger
{
public:
    static constexpr char const* Name = "hodir biting cold";

    HodirBitingColdTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class HodirNearSnowpackedIcicleTrigger : public Trigger
{
public:
    static constexpr char const* Name = "hodir near snowpacked icicle";

    HodirNearSnowpackedIcicleTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class HodirFrostResistanceTrigger : public Trigger
{
public:
    static constexpr char const* Name = "hodir frost resistance trigger";

    HodirFrostResistanceTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class HodirIcicleDodgeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "hodir icicle dodge";

    HodirIcicleDodgeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class HodirRaidPositionTrigger : public Trigger
{
public:
    static constexpr char const* Name = "hodir raid position";

    HodirRaidPositionTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class HodirSetDpsPriorityTrigger : public Trigger
{
public:
    static constexpr char const* Name = "hodir set dps priority";

    HodirSetDpsPriorityTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class HodirFrozenBlowsSwapTrigger : public Trigger
{
public:
    static constexpr char const* Name = "hodir frozen blows swap";

    HodirFrozenBlowsSwapTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class HodirTankDefensiveTrigger : public Trigger
{
public:
    static constexpr char const* Name = "hodir tank defensive";

    HodirTankDefensiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class HodirSpreadStormCloudTrigger : public Trigger
{
public:
    static constexpr char const* Name = "hodir spread storm cloud";

    HodirSpreadStormCloudTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class HodirCollectStormPowerTrigger : public Trigger
{
public:
    static constexpr char const* Name = "hodir collect storm power";

    HodirCollectStormPowerTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class HodirRedirectThreatTrigger : public Trigger
{
public:
    static constexpr char const* Name = "hodir redirect threat";

    HodirRedirectThreatTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
