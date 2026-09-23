#ifndef PLAYERBOTS_RAID_TOCACTIONS_ICEHOWL_H
#define PLAYERBOTS_RAID_TOCACTIONS_ICEHOWL_H

#include "Action.h"
#include "MovementActions.h"
#include "NamedObjectContext.h"
#include "ToCActions_Shared.h"

class IcehowlMainTankHoldBossAction : public ToCMainTankHoldAction
{
public:
    IcehowlMainTankHoldBossAction(PlayerbotAI* botAI, std::string const name = "icehowl main tank hold boss")
        : ToCMainTankHoldAction(botAI, name) {};
    bool Execute(Event event) override;
};

class IcehowlClearChargePathAction : public MovementAction
{
public:
    IcehowlClearChargePathAction(
        PlayerbotAI* botAI, std::string const name = "icehowl clear charge path") : MovementAction(botAI, name) {};
    bool Execute(Event event) override;
};

class ToCIcehowlActionContext : public NamedObjectContext<Action>
{
public:
    ToCIcehowlActionContext()
    {
        creators["icehowl main tank hold boss"] =
            &ToCIcehowlActionContext::icehowl_main_tank_hold_boss;
        creators["icehowl clear charge path"] =
            &ToCIcehowlActionContext::icehowl_clear_charge_path;
    }

private:
    static Action* icehowl_main_tank_hold_boss(PlayerbotAI* botAI) {
        return new IcehowlMainTankHoldBossAction(botAI);
    }

    static Action* icehowl_clear_charge_path(PlayerbotAI* botAI) {
        return new IcehowlClearChargePathAction(botAI);
    }
};

#endif
