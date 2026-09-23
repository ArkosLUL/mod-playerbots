#ifndef PLAYERBOTS_RAID_TOCACTIONS_ANUBARAK_H
#define PLAYERBOTS_RAID_TOCACTIONS_ANUBARAK_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"
#include "NamedObjectContext.h"
#include "ToCActions_Shared.h"

class AnubarakMainTankHoldBossAction : public ToCMainTankHoldAction
{
public:
    AnubarakMainTankHoldBossAction(PlayerbotAI* botAI, std::string const name = "anubarak main tank hold boss")
        : ToCMainTankHoldAction(botAI, name) {};
    bool Execute(Event event) override;
};

class AnubarakAssistTankHoldBurrowerAction : public AttackAction
{
public:
    AnubarakAssistTankHoldBurrowerAction(
        PlayerbotAI* botAI, std::string const name = "anubarak assist tank hold burrower") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class AnubarakFocusBurrowerAction : public AttackAction
{
public:
    AnubarakFocusBurrowerAction(
        PlayerbotAI* botAI, std::string const name = "anubarak focus burrower") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class AnubarakFocusScarabAction : public AttackAction
{
public:
    AnubarakFocusScarabAction(
        PlayerbotAI* botAI, std::string const name = "anubarak focus scarab") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class AnubarakKiteSpikeToPermafrostAction : public MovementAction
{
public:
    AnubarakKiteSpikeToPermafrostAction(
        PlayerbotAI* botAI, std::string const name = "anubarak kite spike to permafrost") : MovementAction(botAI, name) {};
    bool Execute(Event event) override;
};

class AnubarakDestroyFrostSphereAction : public AttackAction
{
public:
    AnubarakDestroyFrostSphereAction(
        PlayerbotAI* botAI, std::string const name = "anubarak destroy frost sphere") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class ToCAnubarakActionContext : public NamedObjectContext<Action>
{
public:
    ToCAnubarakActionContext()
    {
        creators["anubarak main tank hold boss"] =
            &ToCAnubarakActionContext::anubarak_main_tank_hold_boss;
        creators["anubarak assist tank hold burrower"] =
            &ToCAnubarakActionContext::anubarak_assist_tank_hold_burrower;
        creators["anubarak focus burrower"] =
            &ToCAnubarakActionContext::anubarak_focus_burrower;
        creators["anubarak focus scarab"] =
            &ToCAnubarakActionContext::anubarak_focus_scarab;
        creators["anubarak kite spike to permafrost"] =
            &ToCAnubarakActionContext::anubarak_kite_spike_to_permafrost;
        creators["anubarak destroy frost sphere"] =
            &ToCAnubarakActionContext::anubarak_destroy_frost_sphere;
    }

private:
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
