#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_JORMUNGARS_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_JORMUNGARS_H

#include "ToCTriggers_NorthrendBeasts.h"
#include "Trigger.h"

class NorthrendWormsMobileTankDutyTrigger : public Trigger
{
public:
    static constexpr char const* Name = "northrend worms mobile tank duty";

    NorthrendWormsMobileTankDutyTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class NorthrendWormsStationaryTankDutyTrigger : public Trigger
{
public:
    static constexpr char const* Name = "northrend worms stationary tank duty";

    NorthrendWormsStationaryTankDutyTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class NorthrendWormsRedirectThreatTrigger : public Trigger
{
public:
    static constexpr char const* Name = "northrend worms redirect threat";

    NorthrendWormsRedirectThreatTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class NorthrendWormsMisplacedTrigger : public Trigger
{
public:
    static constexpr char const* Name = "northrend worms misplaced";

    NorthrendWormsMisplacedTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class NorthrendWormsPoolAfterWormsTrigger : public Trigger
{
public:
    static constexpr char const* Name = "northrend worms pool after worms";

    NorthrendWormsPoolAfterWormsTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class NorthrendWormsToxinCarrierHurtTrigger : public Trigger
{
public:
    static constexpr char const* Name = "northrend worms toxin carrier hurt";

    NorthrendWormsToxinCarrierHurtTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class NorthrendWormsToxinStuckTrigger : public Trigger
{
public:
    static constexpr char const* Name = "northrend worms toxin stuck";

    NorthrendWormsToxinStuckTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
