#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_JORMUNGARS_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_JORMUNGARS_H

#include <vector>

#include "NamedObjectContext.h"
#include "Trigger.h"

class NorthrendWormsMobileTankDutyTrigger : public Trigger
{
public:
    NorthrendWormsMobileTankDutyTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "northrend worms mobile tank duty") {}
    bool IsActive() override;
};

class NorthrendWormsStationaryTankDutyTrigger : public Trigger
{
public:
    NorthrendWormsStationaryTankDutyTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "northrend worms stationary tank duty") {}
    bool IsActive() override;
};

class NorthrendWormsRedirectThreatTrigger : public Trigger
{
public:
    NorthrendWormsRedirectThreatTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "northrend worms redirect threat") {}
    bool IsActive() override;
};

// Pools, Spew, Sweep, Bile, the spray spread and the Toxin cure walk, all through one mover
class NorthrendWormsMisplacedTrigger : public Trigger
{
public:
    NorthrendWormsMisplacedTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "northrend worms misplaced") {}
    bool IsActive() override;
};

class ToCJormungarsTriggerContext : public NamedObjectContext<Trigger>
{
public:
    ToCJormungarsTriggerContext() : NamedObjectContext<Trigger>()
    {
        creators["northrend worms mobile tank duty"] =
            &ToCJormungarsTriggerContext::northrend_worms_mobile_tank_duty;
        creators["northrend worms stationary tank duty"] =
            &ToCJormungarsTriggerContext::northrend_worms_stationary_tank_duty;
        creators["northrend worms redirect threat"] =
            &ToCJormungarsTriggerContext::northrend_worms_redirect_threat;
        creators["northrend worms misplaced"] =
            &ToCJormungarsTriggerContext::northrend_worms_misplaced;
    }

private:
    static Trigger* northrend_worms_mobile_tank_duty(PlayerbotAI* botAI) {
        return new NorthrendWormsMobileTankDutyTrigger(botAI);
    }

    static Trigger* northrend_worms_stationary_tank_duty(PlayerbotAI* botAI) {
        return new NorthrendWormsStationaryTankDutyTrigger(botAI);
    }

    static Trigger* northrend_worms_redirect_threat(PlayerbotAI* botAI) {
        return new NorthrendWormsRedirectThreatTrigger(botAI);
    }

    static Trigger* northrend_worms_misplaced(PlayerbotAI* botAI) {
        return new NorthrendWormsMisplacedTrigger(botAI);
    }
};

void AddToCJormungarsTriggerNodes(std::vector<TriggerNode*>& triggers);

#endif
