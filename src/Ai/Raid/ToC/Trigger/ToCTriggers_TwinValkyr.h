#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_TWINVALKYR_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_TWINVALKYR_H

#include <vector>

#include "NamedObjectContext.h"
#include "Trigger.h"

class TwinValkyrEngagedByMainTankTrigger : public Trigger
{
public:
    TwinValkyrEngagedByMainTankTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "twin valkyr engaged by main tank") {}
    bool IsActive() override;
};

class TwinValkyrDarkbaneNeedsAssistTankTrigger : public Trigger
{
public:
    TwinValkyrDarkbaneNeedsAssistTankTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "twin valkyr darkbane needs assist tank") {}
    bool IsActive() override;
};

class TwinValkyrVortexRequiresEssenceTrigger : public Trigger
{
public:
    TwinValkyrVortexRequiresEssenceTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "twin valkyr vortex requires essence") {}
    bool IsActive() override;
};

class TwinValkyrTouchedRequiresEssenceTrigger : public Trigger
{
public:
    TwinValkyrTouchedRequiresEssenceTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "twin valkyr touched requires essence") {}
    bool IsActive() override;
};

class TwinValkyrNeedsInitialEssenceTrigger : public Trigger
{
public:
    TwinValkyrNeedsInitialEssenceTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "twin valkyr needs initial essence") {}
    bool IsActive() override;
};

class TwinValkyrPactInterruptibleTrigger : public Trigger
{
public:
    TwinValkyrPactInterruptibleTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "twin valkyr pact interruptible") {}
    bool IsActive() override;
};

class ToCTwinValkyrTriggerContext : public NamedObjectContext<Trigger>
{
public:
    ToCTwinValkyrTriggerContext() : NamedObjectContext<Trigger>()
    {
        creators["twin valkyr engaged by main tank"] =
            &ToCTwinValkyrTriggerContext::twin_valkyr_engaged_by_main_tank;
        creators["twin valkyr darkbane needs assist tank"] =
            &ToCTwinValkyrTriggerContext::twin_valkyr_darkbane_needs_assist_tank;
        creators["twin valkyr vortex requires essence"] =
            &ToCTwinValkyrTriggerContext::twin_valkyr_vortex_requires_essence;
        creators["twin valkyr touched requires essence"] =
            &ToCTwinValkyrTriggerContext::twin_valkyr_touched_requires_essence;
        creators["twin valkyr needs initial essence"] =
            &ToCTwinValkyrTriggerContext::twin_valkyr_needs_initial_essence;
        creators["twin valkyr pact interruptible"] =
            &ToCTwinValkyrTriggerContext::twin_valkyr_pact_interruptible;
    }

private:
    static Trigger* twin_valkyr_engaged_by_main_tank(PlayerbotAI* botAI) {
        return new TwinValkyrEngagedByMainTankTrigger(botAI);
    }

    static Trigger* twin_valkyr_darkbane_needs_assist_tank(PlayerbotAI* botAI) {
        return new TwinValkyrDarkbaneNeedsAssistTankTrigger(botAI);
    }

    static Trigger* twin_valkyr_vortex_requires_essence(PlayerbotAI* botAI) {
        return new TwinValkyrVortexRequiresEssenceTrigger(botAI);
    }

    static Trigger* twin_valkyr_touched_requires_essence(PlayerbotAI* botAI) {
        return new TwinValkyrTouchedRequiresEssenceTrigger(botAI);
    }

    static Trigger* twin_valkyr_needs_initial_essence(PlayerbotAI* botAI) {
        return new TwinValkyrNeedsInitialEssenceTrigger(botAI);
    }

    static Trigger* twin_valkyr_pact_interruptible(PlayerbotAI* botAI) {
        return new TwinValkyrPactInterruptibleTrigger(botAI);
    }
};

void AddToCTwinValkyrTriggerNodes(std::vector<TriggerNode*>& triggers);

#endif
