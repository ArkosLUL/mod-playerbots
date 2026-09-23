#ifndef PLAYERBOTS_RAID_TOCACTIONS_JORMUNGARS_H
#define PLAYERBOTS_RAID_TOCACTIONS_JORMUNGARS_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"
#include "NamedObjectContext.h"
#include "ToCActions_Shared.h"

class WormsMainTankHoldMobileWormAction : public AttackAction
{
public:
    WormsMainTankHoldMobileWormAction(
        PlayerbotAI* botAI, std::string const name = "northrend worms main tank hold mobile worm") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class WormsAssistTankHoldStationaryWormAction : public AttackAction
{
public:
    WormsAssistTankHoldStationaryWormAction(
        PlayerbotAI* botAI, std::string const name = "northrend worms assist tank hold stationary worm") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class WormsSpreadAction : public MovementAction
{
public:
    WormsSpreadAction(
        PlayerbotAI* botAI, std::string const name = "northrend worms spread") : MovementAction(botAI, name) {};
    bool Execute(Event event) override;
};

class WormsKeepMovingAction : public MovementAction
{
public:
    WormsKeepMovingAction(
        PlayerbotAI* botAI, std::string const name = "northrend worms keep moving") : MovementAction(botAI, name) {};
    bool Execute(Event event) override;
};

class WormsAvoidSlimePoolAction : public AvoidCreatureClusterAction
{
public:
    WormsAvoidSlimePoolAction(
        PlayerbotAI* botAI, std::string const name = "northrend worms avoid slime pool") : AvoidCreatureClusterAction(botAI, name) {};
    bool Execute(Event event) override;
};

class WormsAvoidSweepAction : public MovementAction
{
public:
    WormsAvoidSweepAction(
        PlayerbotAI* botAI, std::string const name = "northrend worms avoid sweep") : MovementAction(botAI, name) {};
    bool Execute(Event event) override;
};

class ToCJormungarsActionContext : public NamedObjectContext<Action>
{
public:
    ToCJormungarsActionContext()
    {
        creators["northrend worms main tank hold mobile worm"] =
            &ToCJormungarsActionContext::worms_main_tank_hold_mobile_worm;
        creators["northrend worms assist tank hold stationary worm"] =
            &ToCJormungarsActionContext::worms_assist_tank_hold_stationary_worm;
        creators["northrend worms spread"] =
            &ToCJormungarsActionContext::worms_spread;
        creators["northrend worms keep moving"] =
            &ToCJormungarsActionContext::worms_keep_moving;
        creators["northrend worms avoid slime pool"] =
            &ToCJormungarsActionContext::worms_avoid_slime_pool;
        creators["northrend worms avoid sweep"] =
            &ToCJormungarsActionContext::worms_avoid_sweep;
    }

private:
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

    static Action* worms_avoid_slime_pool(PlayerbotAI* botAI) {
        return new WormsAvoidSlimePoolAction(botAI);
    }

    static Action* worms_avoid_sweep(PlayerbotAI* botAI) {
        return new WormsAvoidSweepAction(botAI);
    }
};

#endif
