#ifndef PLAYERBOTS_RAID_TOCACTIONS_TWINVALKYR_H
#define PLAYERBOTS_RAID_TOCACTIONS_TWINVALKYR_H

#include "Action.h"
#include "AttackAction.h"
#include "MovementActions.h"
#include "NamedObjectContext.h"
#include "RaidRedirectThreat.h"
#include "ToCActions_Shared.h"
#include "ToCHelpers_TwinValkyr.h"

class TwinValkyrInterruptPactAction : public Action
{
public:
    TwinValkyrInterruptPactAction(
        PlayerbotAI* botAI, std::string const name = "twin valkyr interrupt pact") : Action(botAI, name) {}
    bool Execute(Event event) override;
};

// Runs only while GetWantedEssence names this action's reason, and walks to that colour's portal.
class TwinValkyrEssenceActionBase : public MovementAction
{
public:
    TwinValkyrEssenceActionBase(PlayerbotAI* botAI, std::string const name,
                                TrialOfTheCrusaderHelpers::TwinEssenceReason reason)
        : MovementAction(botAI, name), reason(reason) {}
    bool Execute(Event event) override;

protected:
    bool AcquireEssence(TrialOfTheCrusaderHelpers::TwinColour colour);

private:
    TrialOfTheCrusaderHelpers::TwinEssenceReason const reason;
};

class TwinValkyrSwapEssenceForTouchAction : public TwinValkyrEssenceActionBase
{
public:
    TwinValkyrSwapEssenceForTouchAction(
        PlayerbotAI* botAI, std::string const name = "twin valkyr swap essence for touch")
        : TwinValkyrEssenceActionBase(botAI, name, TrialOfTheCrusaderHelpers::TwinEssenceReason::Touch) {}
};

class TwinValkyrSwapEssenceForVortexAction : public TwinValkyrEssenceActionBase
{
public:
    TwinValkyrSwapEssenceForVortexAction(
        PlayerbotAI* botAI, std::string const name = "twin valkyr swap essence for vortex")
        : TwinValkyrEssenceActionBase(botAI, name, TrialOfTheCrusaderHelpers::TwinEssenceReason::Vortex) {}
};

class TwinValkyrSwapEssenceForShieldAction : public TwinValkyrEssenceActionBase
{
public:
    TwinValkyrSwapEssenceForShieldAction(
        PlayerbotAI* botAI, std::string const name = "twin valkyr swap essence for shield")
        : TwinValkyrEssenceActionBase(botAI, name, TrialOfTheCrusaderHelpers::TwinEssenceReason::Shield) {}
};

class TwinValkyrTakeBaseEssenceAction : public TwinValkyrEssenceActionBase
{
public:
    TwinValkyrTakeBaseEssenceAction(
        PlayerbotAI* botAI, std::string const name = "twin valkyr take base essence")
        : TwinValkyrEssenceActionBase(botAI, name, TrialOfTheCrusaderHelpers::TwinEssenceReason::Base) {}
};

class TwinValkyrDodgeOrbAction : public MovementAction
{
public:
    TwinValkyrDodgeOrbAction(
        PlayerbotAI* botAI, std::string const name = "twin valkyr dodge orb") : MovementAction(botAI, name) {}
    bool Execute(Event event) override;

private:
    Position dodgeSpot;
    uint32 dodgeSpotMs = 0;
};

class TwinValkyrTankHoldAction : public ToCMainTankHoldAction
{
public:
    TwinValkyrTankHoldAction(PlayerbotAI* botAI, std::string const name) : ToCMainTankHoldAction(botAI, name) {}

protected:
    bool HoldTwin(Unit* twin);
};

class TwinValkyrMainTankHoldLightTwinAction : public TwinValkyrTankHoldAction
{
public:
    TwinValkyrMainTankHoldLightTwinAction(
        PlayerbotAI* botAI, std::string const name = "twin valkyr main tank hold light twin")
        : TwinValkyrTankHoldAction(botAI, name) {}
    bool Execute(Event event) override;
};

class TwinValkyrAssistTankHoldDarkTwinAction : public TwinValkyrTankHoldAction
{
public:
    TwinValkyrAssistTankHoldDarkTwinAction(
        PlayerbotAI* botAI, std::string const name = "twin valkyr assist tank hold dark twin")
        : TwinValkyrTankHoldAction(botAI, name) {}
    bool Execute(Event event) override;
};

class TwinValkyrRedirectThreatAction : public RaidRedirectThreatAction
{
public:
    TwinValkyrRedirectThreatAction(
        PlayerbotAI* botAI, std::string const name = "twin valkyr redirect threat")
        : RaidRedirectThreatAction(botAI, name) {}

protected:
    Player* GetRedirectTank() override;
    Unit* GetThreatDumpTarget() override;
};

class TwinValkyrFocusTwinAction : public AttackAction
{
public:
    TwinValkyrFocusTwinAction(
        PlayerbotAI* botAI, std::string const name = "twin valkyr focus twin") : AttackAction(botAI, name) {}
    bool Execute(Event event) override;
};

class ToCTwinValkyrActionContext : public NamedObjectContext<Action>
{
public:
    ToCTwinValkyrActionContext()
    {
        creators["twin valkyr interrupt pact"] =
            &ToCTwinValkyrActionContext::twin_valkyr_interrupt_pact;
        creators["twin valkyr swap essence for touch"] =
            &ToCTwinValkyrActionContext::twin_valkyr_swap_essence_for_touch;
        creators["twin valkyr swap essence for vortex"] =
            &ToCTwinValkyrActionContext::twin_valkyr_swap_essence_for_vortex;
        creators["twin valkyr dodge orb"] =
            &ToCTwinValkyrActionContext::twin_valkyr_dodge_orb;
        creators["twin valkyr swap essence for shield"] =
            &ToCTwinValkyrActionContext::twin_valkyr_swap_essence_for_shield;
        creators["twin valkyr take base essence"] =
            &ToCTwinValkyrActionContext::twin_valkyr_take_base_essence;
        creators["twin valkyr main tank hold light twin"] =
            &ToCTwinValkyrActionContext::twin_valkyr_main_tank_hold_light_twin;
        creators["twin valkyr assist tank hold dark twin"] =
            &ToCTwinValkyrActionContext::twin_valkyr_assist_tank_hold_dark_twin;
        creators["twin valkyr redirect threat"] =
            &ToCTwinValkyrActionContext::twin_valkyr_redirect_threat;
        creators["twin valkyr focus twin"] =
            &ToCTwinValkyrActionContext::twin_valkyr_focus_twin;
    }

private:
    static Action* twin_valkyr_interrupt_pact(PlayerbotAI* botAI) {
        return new TwinValkyrInterruptPactAction(botAI);
    }

    static Action* twin_valkyr_swap_essence_for_touch(PlayerbotAI* botAI) {
        return new TwinValkyrSwapEssenceForTouchAction(botAI);
    }

    static Action* twin_valkyr_swap_essence_for_vortex(PlayerbotAI* botAI) {
        return new TwinValkyrSwapEssenceForVortexAction(botAI);
    }

    static Action* twin_valkyr_dodge_orb(PlayerbotAI* botAI) {
        return new TwinValkyrDodgeOrbAction(botAI);
    }

    static Action* twin_valkyr_swap_essence_for_shield(PlayerbotAI* botAI) {
        return new TwinValkyrSwapEssenceForShieldAction(botAI);
    }

    static Action* twin_valkyr_take_base_essence(PlayerbotAI* botAI) {
        return new TwinValkyrTakeBaseEssenceAction(botAI);
    }

    static Action* twin_valkyr_main_tank_hold_light_twin(PlayerbotAI* botAI) {
        return new TwinValkyrMainTankHoldLightTwinAction(botAI);
    }

    static Action* twin_valkyr_assist_tank_hold_dark_twin(PlayerbotAI* botAI) {
        return new TwinValkyrAssistTankHoldDarkTwinAction(botAI);
    }

    static Action* twin_valkyr_redirect_threat(PlayerbotAI* botAI) {
        return new TwinValkyrRedirectThreatAction(botAI);
    }

    static Action* twin_valkyr_focus_twin(PlayerbotAI* botAI) {
        return new TwinValkyrFocusTwinAction(botAI);
    }
};

#endif
