#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_JARAXXUS_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_JARAXXUS_H

#include <vector>

#include "NamedObjectContext.h"
#include "Trigger.h"

class JaraxxusEngagedByMainTankTrigger : public Trigger
{
public:
    JaraxxusEngagedByMainTankTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "jaraxxus engaged by main tank") {}
    bool IsActive() override;
};

class JaraxxusAddNeedsAssistTankTrigger : public Trigger
{
public:
    JaraxxusAddNeedsAssistTankTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "jaraxxus add needs assist tank") {}
    bool IsActive() override;
};

class JaraxxusSecondAddNeedsAssistTankTrigger : public Trigger
{
public:
    JaraxxusSecondAddNeedsAssistTankTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "jaraxxus second add needs assist tank") {}
    bool IsActive() override;
};

class JaraxxusAddShouldBeFocusedTrigger : public Trigger
{
public:
    JaraxxusAddShouldBeFocusedTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "jaraxxus add should be focused") {}
    bool IsActive() override;
};

class JaraxxusLegionFlameNearbyTrigger : public Trigger
{
public:
    JaraxxusLegionFlameNearbyTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "jaraxxus legion flame nearby") {}
    bool IsActive() override;
};

class JaraxxusIncinerateFleshOnRaidTrigger : public Trigger
{
public:
    JaraxxusIncinerateFleshOnRaidTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "jaraxxus incinerate flesh on raid") {}
    bool IsActive() override;
};

class JaraxxusNetherPowerActiveTrigger : public Trigger
{
public:
    JaraxxusNetherPowerActiveTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "jaraxxus nether power active") {}
    bool IsActive() override;
};

class JaraxxusFelFireballInterruptibleTrigger : public Trigger
{
public:
    JaraxxusFelFireballInterruptibleTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "jaraxxus fel fireball interruptible") {}
    bool IsActive() override;
};

class ToCJaraxxusTriggerContext : public NamedObjectContext<Trigger>
{
public:
    ToCJaraxxusTriggerContext() : NamedObjectContext<Trigger>()
    {
        creators["jaraxxus engaged by main tank"] =
            &ToCJaraxxusTriggerContext::jaraxxus_engaged_by_main_tank;
        creators["jaraxxus add needs assist tank"] =
            &ToCJaraxxusTriggerContext::jaraxxus_add_needs_assist_tank;
        creators["jaraxxus second add needs assist tank"] =
            &ToCJaraxxusTriggerContext::jaraxxus_second_add_needs_assist_tank;
        creators["jaraxxus add should be focused"] =
            &ToCJaraxxusTriggerContext::jaraxxus_add_should_be_focused;
        creators["jaraxxus legion flame nearby"] =
            &ToCJaraxxusTriggerContext::jaraxxus_legion_flame_nearby;
        creators["jaraxxus incinerate flesh on raid"] =
            &ToCJaraxxusTriggerContext::jaraxxus_incinerate_flesh_on_raid;
        creators["jaraxxus nether power active"] =
            &ToCJaraxxusTriggerContext::jaraxxus_nether_power_active;
        creators["jaraxxus fel fireball interruptible"] =
            &ToCJaraxxusTriggerContext::jaraxxus_fel_fireball_interruptible;
    }

private:
    static Trigger* jaraxxus_engaged_by_main_tank(PlayerbotAI* botAI) {
        return new JaraxxusEngagedByMainTankTrigger(botAI);
    }

    static Trigger* jaraxxus_add_needs_assist_tank(PlayerbotAI* botAI) {
        return new JaraxxusAddNeedsAssistTankTrigger(botAI);
    }

    static Trigger* jaraxxus_second_add_needs_assist_tank(PlayerbotAI* botAI) {
        return new JaraxxusSecondAddNeedsAssistTankTrigger(botAI);
    }

    static Trigger* jaraxxus_add_should_be_focused(PlayerbotAI* botAI) {
        return new JaraxxusAddShouldBeFocusedTrigger(botAI);
    }

    static Trigger* jaraxxus_legion_flame_nearby(PlayerbotAI* botAI) {
        return new JaraxxusLegionFlameNearbyTrigger(botAI);
    }

    static Trigger* jaraxxus_incinerate_flesh_on_raid(PlayerbotAI* botAI) {
        return new JaraxxusIncinerateFleshOnRaidTrigger(botAI);
    }

    static Trigger* jaraxxus_nether_power_active(PlayerbotAI* botAI) {
        return new JaraxxusNetherPowerActiveTrigger(botAI);
    }

    static Trigger* jaraxxus_fel_fireball_interruptible(PlayerbotAI* botAI) {
        return new JaraxxusFelFireballInterruptibleTrigger(botAI);
    }
};

void AddToCJaraxxusTriggerNodes(std::vector<TriggerNode*>& triggers);

#endif
