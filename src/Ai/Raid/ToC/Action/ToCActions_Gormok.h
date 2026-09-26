#ifndef PLAYERBOTS_RAID_TOCACTIONS_GORMOK_H
#define PLAYERBOTS_RAID_TOCACTIONS_GORMOK_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"
#include "NamedObjectContext.h"
#include "Position.h"
#include "ToCActions_Shared.h"

class GormokTankHoldBossAction : public ToCMainTankHoldAction
{
public:
    GormokTankHoldBossAction(PlayerbotAI* botAI, std::string const name = "gormok tank hold boss")
        : ToCMainTankHoldAction(botAI, name) {};
    bool Execute(Event event) override;
};

class GormokFocusSnoboldAction : public AttackAction
{
public:
    GormokFocusSnoboldAction(
        PlayerbotAI* botAI, std::string const name = "gormok focus snobold") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class GormokTankSwapTauntAction : public AttackAction
{
public:
    GormokTankSwapTauntAction(
        PlayerbotAI* botAI, std::string const name = "gormok tank swap taunt") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class GormokTankDefensiveAction : public Action
{
public:
    GormokTankDefensiveAction(
        PlayerbotAI* botAI, std::string const name = "gormok tank defensive") : Action(botAI, name) {};
    bool Execute(Event event) override;
};

// Holds its spot while the walk there is in flight. Every MoveTo clears the MotionMaster, so a spot
// re-derived each tick from a bot that's still walking never lands.
class GormokWalkAction : public MovementAction
{
public:
    GormokWalkAction(PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {};

protected:
    // The latched spot while the bot is still walking to it, else nullptr
    Position const* WalkInFlight();
    // False without booking anything while a cast pins the bot's feet
    bool WalkTo(Position const& spot);
    void DropWalk() { walking = false; }

private:
    bool BookedOnWalkSpot();

    Position walkSpot;
    uint32 walkIssuedMs = 0;
    bool walking = false;
};

class GormokBringSnoboldToMeleeAction : public GormokWalkAction
{
public:
    GormokBringSnoboldToMeleeAction(PlayerbotAI* botAI, std::string const name = "gormok bring snobold to melee")
        : GormokWalkAction(botAI, name) {};
    bool Execute(Event event) override;
};

class GormokLeaveStompRangeAction : public GormokWalkAction
{
public:
    GormokLeaveStompRangeAction(
        PlayerbotAI* botAI, std::string const name = "gormok leave stomp range") : GormokWalkAction(botAI, name) {};
    bool Execute(Event event) override;
};

class ToCGormokActionContext : public NamedObjectContext<Action>
{
public:
    ToCGormokActionContext()
    {
        creators["gormok tank hold boss"] =
            &ToCGormokActionContext::gormok_tank_hold_boss;
        creators["gormok focus snobold"] =
            &ToCGormokActionContext::gormok_focus_snobold;
        creators["gormok tank swap taunt"] =
            &ToCGormokActionContext::gormok_tank_swap_taunt;
        creators["gormok tank defensive"] =
            &ToCGormokActionContext::gormok_tank_defensive;
        creators["gormok bring snobold to melee"] =
            &ToCGormokActionContext::gormok_bring_snobold_to_melee;
        creators["gormok leave stomp range"] =
            &ToCGormokActionContext::gormok_leave_stomp_range;
    }

private:
    static Action* gormok_tank_hold_boss(PlayerbotAI* botAI) {
        return new GormokTankHoldBossAction(botAI);
    }

    static Action* gormok_focus_snobold(PlayerbotAI* botAI) {
        return new GormokFocusSnoboldAction(botAI);
    }

    static Action* gormok_tank_swap_taunt(PlayerbotAI* botAI) {
        return new GormokTankSwapTauntAction(botAI);
    }

    static Action* gormok_tank_defensive(PlayerbotAI* botAI) {
        return new GormokTankDefensiveAction(botAI);
    }

    static Action* gormok_bring_snobold_to_melee(PlayerbotAI* botAI) {
        return new GormokBringSnoboldToMeleeAction(botAI);
    }

    static Action* gormok_leave_stomp_range(PlayerbotAI* botAI) {
        return new GormokLeaveStompRangeAction(botAI);
    }
};

#endif
