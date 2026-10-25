#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_JORMUNGARS_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_JORMUNGARS_H

#include <vector>

#include "NamedObjectContext.h"
#include "ToCTriggers_NorthrendBeasts.h"
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

// Pools, Spew, Sweep, Bile, the spray spread, melee stands, reach round a pool and the Toxin cure
// walk, all through one mover. Off while the bot stands on its held spot.
class NorthrendWormsMisplacedTrigger : public Trigger
{
public:
    NorthrendWormsMisplacedTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "northrend worms misplaced") {}
    bool IsActive() override;
};

// A healer's Toxin carrier, one no Bile can cure, is hurt and in heal reach
class NorthrendWormsToxinCarrierHurtTrigger : public Trigger
{
public:
    NorthrendWormsToxinCarrierHurtTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "northrend worms toxin carrier hurt") {}
    bool IsActive() override;
};

// The bot's own uncurable Toxin has it stuck and its immunity is ready
class NorthrendWormsToxinStuckTrigger : public Trigger
{
public:
    NorthrendWormsToxinStuckTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "northrend worms toxin stuck") {}
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
        creators["northrend worms dps target"] =
            &ToCJormungarsTriggerContext::northrend_worms_dps_target;
        creators["northrend worms toxin carrier hurt"] =
            &ToCJormungarsTriggerContext::northrend_worms_toxin_carrier_hurt;
        creators["northrend worms toxin stuck"] =
            &ToCJormungarsTriggerContext::northrend_worms_toxin_stuck;
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

    static Trigger* northrend_worms_dps_target(PlayerbotAI* botAI) {
        return new NorthrendBeastsDpsTargetTrigger(botAI, "northrend worms dps target", NorthrendBeastsStem::Worms);
    }

    static Trigger* northrend_worms_toxin_carrier_hurt(PlayerbotAI* botAI) {
        return new NorthrendWormsToxinCarrierHurtTrigger(botAI);
    }

    static Trigger* northrend_worms_toxin_stuck(PlayerbotAI* botAI) {
        return new NorthrendWormsToxinStuckTrigger(botAI);
    }
};

void AddToCJormungarsTriggerNodes(std::vector<TriggerNode*>& triggers);

#endif
