#ifndef PLAYERBOTS_RAID_TOCACTIONS_JARAXXUS_H
#define PLAYERBOTS_RAID_TOCACTIONS_JARAXXUS_H

#include <string>

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"
#include "NamedObjectContext.h"
#include "Position.h"
#include "ToCActions_Shared.h"

class JaraxxusInterruptFelFireballAction : public Action
{
public:
    JaraxxusInterruptFelFireballAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus interrupt fel fireball") : Action(botAI, name) {};
    bool Execute(Event event) override;
};

class JaraxxusAvoidLegionFlameAction : public MovementAction
{
public:
    JaraxxusAvoidLegionFlameAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus avoid legion flame") : MovementAction(botAI, name) {};
    bool Execute(Event event) override;

private:
    void ClearLatches();
    bool IssueLeg(Position const& spot, uint32 now);

    Position _spot;
    Position _legStart;
    Position _lastIssued;
    bool _hasSpot = false;
    uint32 _issuedMs = 0;
    float _heading = 0.0f;
    bool _hasHeading = false;
    uint32 _lastTickMs = 0;
};

class JaraxxusBreakPinnedCastAction : public Action
{
public:
    JaraxxusBreakPinnedCastAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus break pinned cast") : Action(botAI, name) {};
    bool Execute(Event event) override;
};

class JaraxxusIntroMainTankStandAction : public MovementAction
{
public:
    JaraxxusIntroMainTankStandAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus intro main tank stand") : MovementAction(botAI, name) {};
    bool Execute(Event event) override;
};

class JaraxxusMainTankHoldBossAction : public ToCMainTankHoldAction
{
public:
    JaraxxusMainTankHoldBossAction(PlayerbotAI* botAI, std::string const name = "jaraxxus main tank hold boss")
        : ToCMainTankHoldAction(botAI, name) {};
    bool Execute(Event event) override;
};

class JaraxxusAssistTankHoldAction : public AttackAction
{
public:
    JaraxxusAssistTankHoldAction(PlayerbotAI* botAI, std::string const name) : AttackAction(botAI, name) {};

protected:
    // Index 0 marks square, 1 diamond, so each tank's rti resolves to its own add.
    bool HoldAdd(uint8 index);
};

class JaraxxusAssistTankHoldAddAction : public JaraxxusAssistTankHoldAction
{
public:
    JaraxxusAssistTankHoldAddAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus assist tank hold add")
        : JaraxxusAssistTankHoldAction(botAI, name) {};
    bool Execute(Event event) override;
};

class JaraxxusAssistTankHoldSecondAddAction : public JaraxxusAssistTankHoldAction
{
public:
    JaraxxusAssistTankHoldSecondAddAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus assist tank hold second add")
        : JaraxxusAssistTankHoldAction(botAI, name) {};
    bool Execute(Event event) override;
};

class JaraxxusRemoveNetherPowerAction : public Action
{
public:
    JaraxxusRemoveNetherPowerAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus remove nether power") : Action(botAI, name) {};
    bool Execute(Event event) override;
};

class JaraxxusFocusAddAction : public AttackAction
{
public:
    JaraxxusFocusAddAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus focus add") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class JaraxxusResetFocusAction : public Action
{
public:
    JaraxxusResetFocusAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus reset focus") : Action(botAI, name) {};
    bool Execute(Event event) override;
};

class JaraxxusHealIncinerateTargetAction : public Action
{
public:
    JaraxxusHealIncinerateTargetAction(
        PlayerbotAI* botAI, std::string const name = "jaraxxus heal incinerate target") : Action(botAI, name) {};
    bool Execute(Event event) override;
};

class ToCJaraxxusActionContext : public NamedObjectContext<Action>
{
public:
    ToCJaraxxusActionContext()
    {
        creators["jaraxxus interrupt fel fireball"] =
            &ToCJaraxxusActionContext::jaraxxus_interrupt_fel_fireball;
        creators["jaraxxus avoid legion flame"] =
            &ToCJaraxxusActionContext::jaraxxus_avoid_legion_flame;
        creators["jaraxxus break pinned cast"] =
            &ToCJaraxxusActionContext::jaraxxus_break_pinned_cast;
        creators["jaraxxus intro main tank stand"] =
            &ToCJaraxxusActionContext::jaraxxus_intro_main_tank_stand;
        creators["jaraxxus main tank hold boss"] =
            &ToCJaraxxusActionContext::jaraxxus_main_tank_hold_boss;
        creators["jaraxxus assist tank hold add"] =
            &ToCJaraxxusActionContext::jaraxxus_assist_tank_hold_add;
        creators["jaraxxus assist tank hold second add"] =
            &ToCJaraxxusActionContext::jaraxxus_assist_tank_hold_second_add;
        creators["jaraxxus remove nether power"] =
            &ToCJaraxxusActionContext::jaraxxus_remove_nether_power;
        creators["jaraxxus focus add"] =
            &ToCJaraxxusActionContext::jaraxxus_focus_add;
        creators["jaraxxus reset focus"] =
            &ToCJaraxxusActionContext::jaraxxus_reset_focus;
        creators["jaraxxus heal incinerate target"] =
            &ToCJaraxxusActionContext::jaraxxus_heal_incinerate_target;
    }

private:
    static Action* jaraxxus_interrupt_fel_fireball(PlayerbotAI* botAI) {
        return new JaraxxusInterruptFelFireballAction(botAI);
    }

    static Action* jaraxxus_avoid_legion_flame(PlayerbotAI* botAI) {
        return new JaraxxusAvoidLegionFlameAction(botAI);
    }

    static Action* jaraxxus_break_pinned_cast(PlayerbotAI* botAI) {
        return new JaraxxusBreakPinnedCastAction(botAI);
    }

    static Action* jaraxxus_intro_main_tank_stand(PlayerbotAI* botAI) {
        return new JaraxxusIntroMainTankStandAction(botAI);
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

    static Action* jaraxxus_remove_nether_power(PlayerbotAI* botAI) {
        return new JaraxxusRemoveNetherPowerAction(botAI);
    }

    static Action* jaraxxus_focus_add(PlayerbotAI* botAI) {
        return new JaraxxusFocusAddAction(botAI);
    }

    static Action* jaraxxus_reset_focus(PlayerbotAI* botAI) {
        return new JaraxxusResetFocusAction(botAI);
    }

    static Action* jaraxxus_heal_incinerate_target(PlayerbotAI* botAI) {
        return new JaraxxusHealIncinerateTargetAction(botAI);
    }
};

#endif
