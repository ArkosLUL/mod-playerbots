#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_JORMUNGARS_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_JORMUNGARS_H

#include <vector>

#include "NamedObjectContext.h"
#include "Trigger.h"

class WormsMobileEngagedByMainTankTrigger : public Trigger
{
public:
    WormsMobileEngagedByMainTankTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "northrend worms mobile engaged by main tank") {}
    bool IsActive() override;
};

class WormsStationaryNeedsAssistTankTrigger : public Trigger
{
public:
    WormsStationaryNeedsAssistTankTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "northrend worms stationary needs assist tank") {}
    bool IsActive() override;
};

class WormsRangedShouldSpreadTrigger : public Trigger
{
public:
    WormsRangedShouldSpreadTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "northrend worms ranged should spread") {}
    bool IsActive() override;
};

class WormsAfflictedByBurningTrigger : public Trigger
{
public:
    WormsAfflictedByBurningTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "northrend worms afflicted by burning") {}
    bool IsActive() override;
};

class WormsSlimePoolNearbyTrigger : public Trigger
{
public:
    WormsSlimePoolNearbyTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "northrend worms slime pool nearby") {}
    bool IsActive() override;
};

class WormsSweepFrontalTrigger : public Trigger
{
public:
    WormsSweepFrontalTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "northrend worms sweep frontal") {}
    bool IsActive() override;
};

class ToCJormungarsTriggerContext : public NamedObjectContext<Trigger>
{
public:
    ToCJormungarsTriggerContext() : NamedObjectContext<Trigger>()
    {
        creators["northrend worms mobile engaged by main tank"] =
            &ToCJormungarsTriggerContext::worms_mobile_engaged_by_main_tank;
        creators["northrend worms stationary needs assist tank"] =
            &ToCJormungarsTriggerContext::worms_stationary_needs_assist_tank;
        creators["northrend worms ranged should spread"] =
            &ToCJormungarsTriggerContext::worms_ranged_should_spread;
        creators["northrend worms afflicted by burning"] =
            &ToCJormungarsTriggerContext::worms_afflicted_by_burning;
        creators["northrend worms slime pool nearby"] =
            &ToCJormungarsTriggerContext::worms_slime_pool_nearby;
        creators["northrend worms sweep frontal"] =
            &ToCJormungarsTriggerContext::worms_sweep_frontal;
    }

private:
    static Trigger* worms_mobile_engaged_by_main_tank(PlayerbotAI* botAI) {
        return new WormsMobileEngagedByMainTankTrigger(botAI);
    }

    static Trigger* worms_stationary_needs_assist_tank(PlayerbotAI* botAI) {
        return new WormsStationaryNeedsAssistTankTrigger(botAI);
    }

    static Trigger* worms_ranged_should_spread(PlayerbotAI* botAI) {
        return new WormsRangedShouldSpreadTrigger(botAI);
    }

    static Trigger* worms_afflicted_by_burning(PlayerbotAI* botAI) {
        return new WormsAfflictedByBurningTrigger(botAI);
    }

    static Trigger* worms_slime_pool_nearby(PlayerbotAI* botAI) {
        return new WormsSlimePoolNearbyTrigger(botAI);
    }

    static Trigger* worms_sweep_frontal(PlayerbotAI* botAI) {
        return new WormsSweepFrontalTrigger(botAI);
    }
};

void AddToCJormungarsTriggerNodes(std::vector<TriggerNode*>& triggers);

#endif
