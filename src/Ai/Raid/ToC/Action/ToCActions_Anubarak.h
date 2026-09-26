#ifndef PLAYERBOTS_RAID_TOCACTIONS_ANUBARAK_H
#define PLAYERBOTS_RAID_TOCACTIONS_ANUBARAK_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"
#include "NamedObjectContext.h"
#include "ObjectGuid.h"
#include "Position.h"
#include "ToCActions_Shared.h"
#include "ToCHelpers_Anubarak.h"

class AnubarakKiteSpikeToPermafrostAction : public MovementAction
{
public:
    AnubarakKiteSpikeToPermafrostAction(PlayerbotAI* botAI,
                                        std::string const name = "anubarak kite spike to permafrost")
        : MovementAction(botAI, name) {};
    bool Execute(Event event) override;

private:
    // Last tick's stand, handed back to the helper so the kite doesn't re-aim every tick. Only good
    // for the spike it was picked against.
    TrialOfTheCrusaderHelpers::AnubarakKiteStand latched;
    ObjectGuid latchedSpike;
    bool hasLatched = false;
};

class AnubarakAvoidSpikeAction : public MovementAction
{
public:
    AnubarakAvoidSpikeAction(
        PlayerbotAI* botAI, std::string const name = "anubarak avoid spike") : MovementAction(botAI, name) {};
    bool Execute(Event event) override;

private:
    Position spot;
    uint32 spotMs = 0;
    bool hasSpot = false;
};

class AnubarakInterruptShadowStrikeAction : public MovementAction
{
public:
    AnubarakInterruptShadowStrikeAction(PlayerbotAI* botAI, std::string const name = "anubarak interrupt shadow strike")
        : MovementAction(botAI, name) {};
    bool Execute(Event event) override;
};

class AnubarakTankDefensiveAction : public Action
{
public:
    AnubarakTankDefensiveAction(
        PlayerbotAI* botAI, std::string const name = "anubarak tank defensive") : Action(botAI, name) {};
    bool Execute(Event event) override;
};

class AnubarakDestroyFrostSphereAction : public AttackAction
{
public:
    AnubarakDestroyFrostSphereAction(
        PlayerbotAI* botAI, std::string const name = "anubarak destroy frost sphere") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class AnubarakFocusBurrowerAction : public AttackAction
{
public:
    AnubarakFocusBurrowerAction(
        PlayerbotAI* botAI, std::string const name = "anubarak focus burrower") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class AnubarakAssistTankHoldBurrowerAction : public AttackAction
{
public:
    AnubarakAssistTankHoldBurrowerAction(PlayerbotAI* botAI,
                                         std::string const name = "anubarak assist tank hold burrower")
        : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class AnubarakTankPickUpScarabAction : public AttackAction
{
public:
    AnubarakTankPickUpScarabAction(
        PlayerbotAI* botAI, std::string const name = "anubarak tank pick up scarab") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class AnubarakMainTankHoldBossAction : public ToCMainTankHoldAction
{
public:
    AnubarakMainTankHoldBossAction(PlayerbotAI* botAI, std::string const name = "anubarak main tank hold boss")
        : ToCMainTankHoldAction(botAI, name) {};
    bool Execute(Event event) override;

private:
    // Leads him onto point, within ANUBARAK_DRAG_ARRIVE measured on him. DragBossToAnchor stops the
    // bot within 12 yd, so coming in from north or south he ends up on a tank patch.
    bool DragBossOnto(Unit* boss, Position const& point);
};

class AnubarakHealPenetratingColdAction : public Action
{
public:
    AnubarakHealPenetratingColdAction(
        PlayerbotAI* botAI, std::string const name = "anubarak heal penetrating cold") : Action(botAI, name) {};
    bool Execute(Event event) override;
};

class ToCAnubarakActionContext : public NamedObjectContext<Action>
{
public:
    ToCAnubarakActionContext()
    {
        creators["anubarak kite spike to permafrost"] =
            &ToCAnubarakActionContext::anubarak_kite_spike_to_permafrost;
        creators["anubarak avoid spike"] =
            &ToCAnubarakActionContext::anubarak_avoid_spike;
        creators["anubarak interrupt shadow strike"] =
            &ToCAnubarakActionContext::anubarak_interrupt_shadow_strike;
        creators["anubarak tank defensive"] =
            &ToCAnubarakActionContext::anubarak_tank_defensive;
        creators["anubarak destroy frost sphere"] =
            &ToCAnubarakActionContext::anubarak_destroy_frost_sphere;
        creators["anubarak focus burrower"] =
            &ToCAnubarakActionContext::anubarak_focus_burrower;
        creators["anubarak assist tank hold burrower"] =
            &ToCAnubarakActionContext::anubarak_assist_tank_hold_burrower;
        creators["anubarak tank pick up scarab"] =
            &ToCAnubarakActionContext::anubarak_tank_pick_up_scarab;
        creators["anubarak main tank hold boss"] =
            &ToCAnubarakActionContext::anubarak_main_tank_hold_boss;
        creators["anubarak heal penetrating cold"] =
            &ToCAnubarakActionContext::anubarak_heal_penetrating_cold;
    }

private:
    static Action* anubarak_kite_spike_to_permafrost(PlayerbotAI* botAI) {
        return new AnubarakKiteSpikeToPermafrostAction(botAI);
    }

    static Action* anubarak_avoid_spike(PlayerbotAI* botAI) {
        return new AnubarakAvoidSpikeAction(botAI);
    }

    static Action* anubarak_interrupt_shadow_strike(PlayerbotAI* botAI) {
        return new AnubarakInterruptShadowStrikeAction(botAI);
    }

    static Action* anubarak_tank_defensive(PlayerbotAI* botAI) {
        return new AnubarakTankDefensiveAction(botAI);
    }

    static Action* anubarak_destroy_frost_sphere(PlayerbotAI* botAI) {
        return new AnubarakDestroyFrostSphereAction(botAI);
    }

    static Action* anubarak_focus_burrower(PlayerbotAI* botAI) {
        return new AnubarakFocusBurrowerAction(botAI);
    }

    static Action* anubarak_assist_tank_hold_burrower(PlayerbotAI* botAI) {
        return new AnubarakAssistTankHoldBurrowerAction(botAI);
    }

    static Action* anubarak_tank_pick_up_scarab(PlayerbotAI* botAI) {
        return new AnubarakTankPickUpScarabAction(botAI);
    }

    static Action* anubarak_main_tank_hold_boss(PlayerbotAI* botAI) {
        return new AnubarakMainTankHoldBossAction(botAI);
    }

    static Action* anubarak_heal_penetrating_cold(PlayerbotAI* botAI) {
        return new AnubarakHealPenetratingColdAction(botAI);
    }
};

#endif
