#ifndef PLAYERBOTS_RAID_TOCACTIONS_TWINVALKYR_H
#define PLAYERBOTS_RAID_TOCACTIONS_TWINVALKYR_H

#include "AttackAction.h"
#include "MovementActions.h"
#include "NamedObjectContext.h"
#include "ToCActions_Shared.h"

class TwinValkyrMainTankHoldLightTwinAction : public ToCMainTankHoldAction
{
public:
    TwinValkyrMainTankHoldLightTwinAction(
        PlayerbotAI* botAI, std::string const name = "twin valkyr main tank hold light twin")
        : ToCMainTankHoldAction(botAI, name) {};
    bool Execute(Event event) override;
};

class TwinValkyrAssistTankHoldDarkTwinAction : public AttackAction
{
public:
    TwinValkyrAssistTankHoldDarkTwinAction(
        PlayerbotAI* botAI, std::string const name = "twin valkyr assist tank hold dark twin") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

// Shared base for the essence-acquiring actions: walks to the nearest portal of the wanted colour and
// triggers its gossip-hello hook (which casts the essence aura on the bot).
class TwinValkyrEssenceActionBase : public MovementAction
{
public:
    TwinValkyrEssenceActionBase(
        PlayerbotAI* botAI, std::string const name) : MovementAction(botAI, name) {};

protected:
    bool AcquireEssence(bool wantLight);
};

class TwinValkyrSwapEssenceForVortexAction : public TwinValkyrEssenceActionBase
{
public:
    TwinValkyrSwapEssenceForVortexAction(
        PlayerbotAI* botAI, std::string const name = "twin valkyr swap essence for vortex") : TwinValkyrEssenceActionBase(botAI, name) {};
    bool Execute(Event event) override;
};

class TwinValkyrSwapEssenceForTouchAction : public TwinValkyrEssenceActionBase
{
public:
    TwinValkyrSwapEssenceForTouchAction(
        PlayerbotAI* botAI, std::string const name = "twin valkyr swap essence for touch") : TwinValkyrEssenceActionBase(botAI, name) {};
    bool Execute(Event event) override;
};

class TwinValkyrAcquireInitialEssenceAction : public TwinValkyrEssenceActionBase
{
public:
    TwinValkyrAcquireInitialEssenceAction(
        PlayerbotAI* botAI, std::string const name = "twin valkyr acquire initial essence") : TwinValkyrEssenceActionBase(botAI, name) {};
    bool Execute(Event event) override;
};

class TwinValkyrInterruptPactAction : public AttackAction
{
public:
    TwinValkyrInterruptPactAction(
        PlayerbotAI* botAI, std::string const name = "twin valkyr interrupt pact") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class ToCTwinValkyrActionContext : public NamedObjectContext<Action>
{
public:
    ToCTwinValkyrActionContext()
    {
        creators["twin valkyr main tank hold light twin"] =
            &ToCTwinValkyrActionContext::twin_valkyr_main_tank_hold_light_twin;
        creators["twin valkyr assist tank hold dark twin"] =
            &ToCTwinValkyrActionContext::twin_valkyr_assist_tank_hold_dark_twin;
        creators["twin valkyr swap essence for vortex"] =
            &ToCTwinValkyrActionContext::twin_valkyr_swap_essence_for_vortex;
        creators["twin valkyr swap essence for touch"] =
            &ToCTwinValkyrActionContext::twin_valkyr_swap_essence_for_touch;
        creators["twin valkyr acquire initial essence"] =
            &ToCTwinValkyrActionContext::twin_valkyr_acquire_initial_essence;
        creators["twin valkyr interrupt pact"] =
            &ToCTwinValkyrActionContext::twin_valkyr_interrupt_pact;
    }

private:
    static Action* twin_valkyr_main_tank_hold_light_twin(PlayerbotAI* botAI) {
        return new TwinValkyrMainTankHoldLightTwinAction(botAI);
    }

    static Action* twin_valkyr_assist_tank_hold_dark_twin(PlayerbotAI* botAI) {
        return new TwinValkyrAssistTankHoldDarkTwinAction(botAI);
    }

    static Action* twin_valkyr_swap_essence_for_vortex(PlayerbotAI* botAI) {
        return new TwinValkyrSwapEssenceForVortexAction(botAI);
    }

    static Action* twin_valkyr_swap_essence_for_touch(PlayerbotAI* botAI) {
        return new TwinValkyrSwapEssenceForTouchAction(botAI);
    }

    static Action* twin_valkyr_acquire_initial_essence(PlayerbotAI* botAI) {
        return new TwinValkyrAcquireInitialEssenceAction(botAI);
    }

    static Action* twin_valkyr_interrupt_pact(PlayerbotAI* botAI) {
        return new TwinValkyrInterruptPactAction(botAI);
    }
};

#endif
