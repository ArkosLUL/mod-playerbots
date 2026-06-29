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

        // Lord Jaraxxus
        creators["jaraxxus engaged by main tank"] =
            &RaidTrialOfTheCrusaderTriggerContext::jaraxxus_engaged_by_main_tank;
        creators["jaraxxus add needs assist tank"] =
            &RaidTrialOfTheCrusaderTriggerContext::jaraxxus_add_needs_assist_tank;
        creators["jaraxxus second add needs assist tank"] =
            &RaidTrialOfTheCrusaderTriggerContext::jaraxxus_second_add_needs_assist_tank;
        creators["jaraxxus add should be focused"] =
            &RaidTrialOfTheCrusaderTriggerContext::jaraxxus_add_should_be_focused;
        creators["jaraxxus legion flame nearby"] =
            &RaidTrialOfTheCrusaderTriggerContext::jaraxxus_legion_flame_nearby;
        creators["jaraxxus incinerate flesh on raid"] =
            &RaidTrialOfTheCrusaderTriggerContext::jaraxxus_incinerate_flesh_on_raid;
        creators["jaraxxus nether power active"] =
            &RaidTrialOfTheCrusaderTriggerContext::jaraxxus_nether_power_active;
        creators["jaraxxus fel fireball interruptible"] =
            &RaidTrialOfTheCrusaderTriggerContext::jaraxxus_fel_fireball_interruptible;
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

    static Trigger* jaraxxus_engaged_by_main_tank(PlayerbotAI* botAI) {
        return new JaraxxusEngagedByMainTankTrigger(botAI);
    }

    static Trigger* jaraxxus_add_needs_assist_tank(PlayerbotAI* botAI) {
        return new JaraxxusAddNeedsAssistTankTrigger(botAI);
    }

    static Trigger* jaraxxus_second_add_needs_assist_tank(PlayerbotAI* botAI) {
        return new JaraxxusSecondAddNeedsAssistTankTrigger(botAI);
    }

    static Trigger* jaraxxus_add_should_be_focused(PlayerbotAI* botAI) {
        return new JaraxxusAddShouldBeFocusedTrigger(botAI);
    }

    static Trigger* jaraxxus_legion_flame_nearby(PlayerbotAI* botAI) {
        return new JaraxxusLegionFlameNearbyTrigger(botAI);
    }

    static Trigger* jaraxxus_incinerate_flesh_on_raid(PlayerbotAI* botAI) {
        return new JaraxxusIncinerateFleshOnRaidTrigger(botAI);
    }

    static Trigger* jaraxxus_nether_power_active(PlayerbotAI* botAI) {
        return new JaraxxusNetherPowerActiveTrigger(botAI);
    }

    static Trigger* jaraxxus_fel_fireball_interruptible(PlayerbotAI* botAI) {
        return new JaraxxusFelFireballInterruptibleTrigger(botAI);
    }
};

#endif
