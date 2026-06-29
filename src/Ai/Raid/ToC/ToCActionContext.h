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
};

#endif
