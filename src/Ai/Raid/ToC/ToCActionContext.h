#ifndef PLAYERBOTS_TOCACTIONCONTEXT_H
#define PLAYERBOTS_TOCACTIONCONTEXT_H

#include "ToCActions.h"
#include "NamedObjectContext.h"

class RaidTrialOfTheCrusaderActionContext : public NamedObjectContext<Action>
{
public:
    RaidTrialOfTheCrusaderActionContext()
    {
        // Gormok the Impaler
        creators["gormok main tank hold boss"] =
            &RaidTrialOfTheCrusaderActionContext::gormok_main_tank_hold_boss;
        creators["gormok focus snobold"] =
            &RaidTrialOfTheCrusaderActionContext::gormok_focus_snobold;

        // Acidmaw & Dreadscale
        creators["northrend worms main tank hold mobile worm"] =
            &RaidTrialOfTheCrusaderActionContext::worms_main_tank_hold_mobile_worm;
        creators["northrend worms assist tank hold stationary worm"] =
            &RaidTrialOfTheCrusaderActionContext::worms_assist_tank_hold_stationary_worm;
        creators["northrend worms spread"] =
            &RaidTrialOfTheCrusaderActionContext::worms_spread;
        creators["northrend worms keep moving"] =
            &RaidTrialOfTheCrusaderActionContext::worms_keep_moving;

        // Icehowl
        creators["icehowl main tank hold boss"] =
            &RaidTrialOfTheCrusaderActionContext::icehowl_main_tank_hold_boss;
        creators["icehowl clear charge path"] =
            &RaidTrialOfTheCrusaderActionContext::icehowl_clear_charge_path;

        // Lord Jaraxxus
        creators["jaraxxus main tank hold boss"] =
            &RaidTrialOfTheCrusaderActionContext::jaraxxus_main_tank_hold_boss;
        creators["jaraxxus assist tank hold add"] =
            &RaidTrialOfTheCrusaderActionContext::jaraxxus_assist_tank_hold_add;
        creators["jaraxxus assist tank hold second add"] =
            &RaidTrialOfTheCrusaderActionContext::jaraxxus_assist_tank_hold_second_add;
        creators["jaraxxus focus add"] =
            &RaidTrialOfTheCrusaderActionContext::jaraxxus_focus_add;
        creators["jaraxxus avoid legion flame"] =
            &RaidTrialOfTheCrusaderActionContext::jaraxxus_avoid_legion_flame;
        creators["jaraxxus heal incinerate target"] =
            &RaidTrialOfTheCrusaderActionContext::jaraxxus_heal_incinerate_target;
        creators["jaraxxus remove nether power"] =
            &RaidTrialOfTheCrusaderActionContext::jaraxxus_remove_nether_power;
        creators["jaraxxus interrupt fel fireball"] =
            &RaidTrialOfTheCrusaderActionContext::jaraxxus_interrupt_fel_fireball;

        // Anub'arak
        creators["anubarak main tank hold boss"] =
            &RaidTrialOfTheCrusaderActionContext::anubarak_main_tank_hold_boss;
        creators["anubarak assist tank hold burrower"] =
            &RaidTrialOfTheCrusaderActionContext::anubarak_assist_tank_hold_burrower;
        creators["anubarak focus burrower"] =
            &RaidTrialOfTheCrusaderActionContext::anubarak_focus_burrower;
        creators["anubarak focus scarab"] =
            &RaidTrialOfTheCrusaderActionContext::anubarak_focus_scarab;
        creators["anubarak kite spike to permafrost"] =
            &RaidTrialOfTheCrusaderActionContext::anubarak_kite_spike_to_permafrost;
        creators["anubarak destroy frost sphere"] =
            &RaidTrialOfTheCrusaderActionContext::anubarak_destroy_frost_sphere;
    }

private:
    static Action* gormok_main_tank_hold_boss(PlayerbotAI* botAI) {
        return new GormokMainTankHoldBossAction(botAI);
    }

    static Action* gormok_focus_snobold(PlayerbotAI* botAI) {
        return new GormokFocusSnoboldAction(botAI);
    }

    static Action* worms_main_tank_hold_mobile_worm(PlayerbotAI* botAI) {
        return new WormsMainTankHoldMobileWormAction(botAI);
    }

    static Action* worms_assist_tank_hold_stationary_worm(PlayerbotAI* botAI) {
        return new WormsAssistTankHoldStationaryWormAction(botAI);
    }

    static Action* worms_spread(PlayerbotAI* botAI) {
        return new WormsSpreadAction(botAI);
    }

    static Action* worms_keep_moving(PlayerbotAI* botAI) {
        return new WormsKeepMovingAction(botAI);
    }

    static Action* icehowl_main_tank_hold_boss(PlayerbotAI* botAI) {
        return new IcehowlMainTankHoldBossAction(botAI);
    }

    static Action* icehowl_clear_charge_path(PlayerbotAI* botAI) {
        return new IcehowlClearChargePathAction(botAI);
    }

    static Action* jaraxxus_main_tank_hold_boss(PlayerbotAI* botAI) {
        return new JaraxxusMainTankHoldBossAction(botAI);
    }

    static Action* jaraxxus_assist_tank_hold_add(PlayerbotAI* botAI) {
        return new JaraxxusAssistTankHoldAddAction(botAI);
    }

    static Action* jaraxxus_assist_tank_hold_second_add(PlayerbotAI* botAI) {
        return new JaraxxusAssistTankHoldSecondAddAction(botAI);
    }

    static Action* jaraxxus_focus_add(PlayerbotAI* botAI) {
        return new JaraxxusFocusAddAction(botAI);
    }

    static Action* jaraxxus_avoid_legion_flame(PlayerbotAI* botAI) {
        return new JaraxxusAvoidLegionFlameAction(botAI);
    }

    static Action* jaraxxus_heal_incinerate_target(PlayerbotAI* botAI) {
        return new JaraxxusHealIncinerateTargetAction(botAI);
    }

    static Action* jaraxxus_remove_nether_power(PlayerbotAI* botAI) {
        return new JaraxxusRemoveNetherPowerAction(botAI);
    }

    static Action* jaraxxus_interrupt_fel_fireball(PlayerbotAI* botAI) {
        return new JaraxxusInterruptFelFireballAction(botAI);
    }

    static Action* anubarak_main_tank_hold_boss(PlayerbotAI* botAI) {
        return new AnubarakMainTankHoldBossAction(botAI);
    }

    static Action* anubarak_assist_tank_hold_burrower(PlayerbotAI* botAI) {
        return new AnubarakAssistTankHoldBurrowerAction(botAI);
    }

    static Action* anubarak_focus_burrower(PlayerbotAI* botAI) {
        return new AnubarakFocusBurrowerAction(botAI);
    }

    static Action* anubarak_focus_scarab(PlayerbotAI* botAI) {
        return new AnubarakFocusScarabAction(botAI);
    }

    static Action* anubarak_kite_spike_to_permafrost(PlayerbotAI* botAI) {
        return new AnubarakKiteSpikeToPermafrostAction(botAI);
    }

    static Action* anubarak_destroy_frost_sphere(PlayerbotAI* botAI) {
        return new AnubarakDestroyFrostSphereAction(botAI);
    }
};

#endif
