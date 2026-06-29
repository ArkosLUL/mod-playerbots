#ifndef PLAYERBOTS_TOCTRIGGERCONTEXT_H
#define PLAYERBOTS_TOCTRIGGERCONTEXT_H

#include "ToCTriggers.h"
#include "NamedObjectContext.h"

class RaidTrialOfTheCrusaderTriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidTrialOfTheCrusaderTriggerContext() : NamedObjectContext<Trigger>()
    {
        // Gormok the Impaler
        creators["gormok engaged by main tank"] =
            &RaidTrialOfTheCrusaderTriggerContext::gormok_engaged_by_main_tank;
        creators["gormok snobold on raid"] =
            &RaidTrialOfTheCrusaderTriggerContext::gormok_snobold_on_raid;

        // Acidmaw & Dreadscale
        creators["northrend worms mobile engaged by main tank"] =
            &RaidTrialOfTheCrusaderTriggerContext::worms_mobile_engaged_by_main_tank;
        creators["northrend worms stationary needs assist tank"] =
            &RaidTrialOfTheCrusaderTriggerContext::worms_stationary_needs_assist_tank;
        creators["northrend worms ranged should spread"] =
            &RaidTrialOfTheCrusaderTriggerContext::worms_ranged_should_spread;
        creators["northrend worms afflicted by burning"] =
            &RaidTrialOfTheCrusaderTriggerContext::worms_afflicted_by_burning;

        // Icehowl
        creators["icehowl engaged by main tank"] =
            &RaidTrialOfTheCrusaderTriggerContext::icehowl_engaged_by_main_tank;
        creators["icehowl charge incoming"] =
            &RaidTrialOfTheCrusaderTriggerContext::icehowl_charge_incoming;
    }

private:
    static Trigger* gormok_engaged_by_main_tank(PlayerbotAI* botAI) {
        return new GormokEngagedByMainTankTrigger(botAI);
    }

    static Trigger* gormok_snobold_on_raid(PlayerbotAI* botAI) {
        return new GormokSnoboldOnRaidTrigger(botAI);
    }

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

    static Trigger* icehowl_engaged_by_main_tank(PlayerbotAI* botAI) {
        return new IcehowlEngagedByMainTankTrigger(botAI);
    }

    static Trigger* icehowl_charge_incoming(PlayerbotAI* botAI) {
        return new IcehowlChargeIncomingTrigger(botAI);
    }
};

#endif
