#ifndef PLAYERBOTS_RAID_TOCACTIONS_JARAXXUS_H
#define PLAYERBOTS_RAID_TOCACTIONS_JARAXXUS_H

#include "Action.h"
#include "AttackAction.h"
#include "NamedObjectContext.h"
#include "ToCActions_Shared.h"

class JaraxxusMainTankHoldBossAction : public ToCMainTankHoldAction
{
public:
    JaraxxusMainTankHoldBossAction(PlayerbotAI* botAI, std::string const name = "jaraxxus main tank hold boss")
        : ToCMainTankHoldAction(botAI, name) {};
    bool Execute(Event event) override;
};

class JaraxxusAssistTankHoldAddAction : public AttackAction
{
public:
    JaraxxusAssistTankHoldAddAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus assist tank hold add") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class JaraxxusAssistTankHoldSecondAddAction : public AttackAction
{
public:
    JaraxxusAssistTankHoldSecondAddAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus assist tank hold second add") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class JaraxxusFocusAddAction : public AttackAction
{
public:
    JaraxxusFocusAddAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus focus add") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class JaraxxusAvoidLegionFlameAction : public AvoidCreatureClusterAction
{
public:
    JaraxxusAvoidLegionFlameAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus avoid legion flame") : AvoidCreatureClusterAction(botAI, name) {};
    bool Execute(Event event) override;
};

class JaraxxusHealIncinerateTargetAction : public Action
{
public:
    JaraxxusHealIncinerateTargetAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus heal incinerate target") : Action(botAI, name) {};
    bool Execute(Event event) override;
};

class JaraxxusRemoveNetherPowerAction : public Action
{
public:
    JaraxxusRemoveNetherPowerAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus remove nether power") : Action(botAI, name) {};
    bool Execute(Event event) override;
};

class JaraxxusInterruptFelFireballAction : public AttackAction
{
public:
    JaraxxusInterruptFelFireballAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus interrupt fel fireball") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class ToCJaraxxusActionContext : public NamedObjectContext<Action>
{
public:
    ToCJaraxxusActionContext()
    {
        creators["jaraxxus main tank hold boss"] =
            &ToCJaraxxusActionContext::jaraxxus_main_tank_hold_boss;
        creators["jaraxxus assist tank hold add"] =
            &ToCJaraxxusActionContext::jaraxxus_assist_tank_hold_add;
        creators["jaraxxus assist tank hold second add"] =
            &ToCJaraxxusActionContext::jaraxxus_assist_tank_hold_second_add;
        creators["jaraxxus focus add"] =
            &ToCJaraxxusActionContext::jaraxxus_focus_add;
        creators["jaraxxus avoid legion flame"] =
            &ToCJaraxxusActionContext::jaraxxus_avoid_legion_flame;
        creators["jaraxxus heal incinerate target"] =
            &ToCJaraxxusActionContext::jaraxxus_heal_incinerate_target;
        creators["jaraxxus remove nether power"] =
            &ToCJaraxxusActionContext::jaraxxus_remove_nether_power;
        creators["jaraxxus interrupt fel fireball"] =
            &ToCJaraxxusActionContext::jaraxxus_interrupt_fel_fireball;
    }

private:
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
};

#endif
