#ifndef PLAYERBOTS_RAID_TOCACTIONS_ICEHOWL_H
#define PLAYERBOTS_RAID_TOCACTIONS_ICEHOWL_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"
#include "NamedObjectContext.h"
#include "RaidObs.h"

class IcehowlTankHoldBossAction : public AttackAction
{
public:
    IcehowlTankHoldBossAction(PlayerbotAI* botAI, std::string const name = "icehowl tank hold boss")
        : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class IcehowlClearChargePathAction : public MovementAction
{
public:
    IcehowlClearChargePathAction(
        PlayerbotAI* botAI, std::string const name = "icehowl clear charge path") : MovementAction(botAI, name) {};
    bool Execute(Event event) override;

private:
    RaidObs::MoveOutcome IssueDodge(Position const& spot, bool replacing);

    Position dodgeSpot;
    uint32 dodgeSpotMs = 0;
    bool hasDodgeSpot = false;
    bool dodgeParked = false;  // stood still because it already cleared the tight line
};

class IcehowlTankDefensiveAction : public Action
{
public:
    IcehowlTankDefensiveAction(PlayerbotAI* botAI) : Action(botAI, "icehowl tank defensive") {}
    bool Execute(Event event) override;
};

class ToCIcehowlActionContext : public NamedObjectContext<Action>
{
public:
    ToCIcehowlActionContext()
    {
        creators["icehowl tank hold boss"] =
            &ToCIcehowlActionContext::icehowl_tank_hold_boss;
        creators["icehowl clear charge path"] =
            &ToCIcehowlActionContext::icehowl_clear_charge_path;
        creators["icehowl tank defensive"] =
            &ToCIcehowlActionContext::icehowl_tank_defensive;
    }

private:
    static Action* icehowl_tank_hold_boss(PlayerbotAI* botAI) {
        return new IcehowlTankHoldBossAction(botAI);
    }

    static Action* icehowl_clear_charge_path(PlayerbotAI* botAI) {
        return new IcehowlClearChargePathAction(botAI);
    }

    static Action* icehowl_tank_defensive(PlayerbotAI* botAI) {
        return new IcehowlTankDefensiveAction(botAI);
    }
};

#endif
