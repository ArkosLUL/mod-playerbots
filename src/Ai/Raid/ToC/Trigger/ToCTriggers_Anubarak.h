#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_ANUBARAK_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_ANUBARAK_H

#include <vector>

#include "NamedObjectContext.h"
#include "Trigger.h"

class AnubarakPursuedBySpikeTrigger : public Trigger
{
public:
    AnubarakPursuedBySpikeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "anubarak pursued by spike") {}
    bool IsActive() override;
};

class AnubarakSpikeNearbyTrigger : public Trigger
{
public:
    AnubarakSpikeNearbyTrigger(PlayerbotAI* botAI) : Trigger(botAI, "anubarak spike nearby") {}
    bool IsActive() override;
};

class AnubarakBurrowerCastingShadowStrikeTrigger : public Trigger
{
public:
    AnubarakBurrowerCastingShadowStrikeTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, "anubarak burrower casting shadow strike") {}
    bool IsActive() override;
};

class AnubarakLeechingSwarmOnTankTrigger : public Trigger
{
public:
    AnubarakLeechingSwarmOnTankTrigger(PlayerbotAI* botAI) : Trigger(botAI, "anubarak leeching swarm on tank") {}
    bool IsActive() override;
};

class AnubarakRangedShouldSeedPermafrostTrigger : public Trigger
{
public:
    AnubarakRangedShouldSeedPermafrostTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, "anubarak ranged should seed permafrost") {}
    bool IsActive() override;
};

class AnubarakBurrowerShouldBeFocusedTrigger : public Trigger
{
public:
    AnubarakBurrowerShouldBeFocusedTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, "anubarak burrower should be focused") {}
    bool IsActive() override;
};

class AnubarakBurrowerNeedsAssistTankTrigger : public Trigger
{
public:
    AnubarakBurrowerNeedsAssistTankTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, "anubarak burrower needs assist tank") {}
    bool IsActive() override;
};

class AnubarakScarabOnRaidTrigger : public Trigger
{
public:
    AnubarakScarabOnRaidTrigger(PlayerbotAI* botAI) : Trigger(botAI, "anubarak scarab on raid") {}
    bool IsActive() override;
};

class AnubarakEngagedByMainTankTrigger : public Trigger
{
public:
    AnubarakEngagedByMainTankTrigger(PlayerbotAI* botAI) : Trigger(botAI, "anubarak engaged by main tank") {}
    bool IsActive() override;
};

class AnubarakPenetratingColdOnRaidTrigger : public Trigger
{
public:
    AnubarakPenetratingColdOnRaidTrigger(PlayerbotAI* botAI) : Trigger(botAI, "anubarak penetrating cold on raid") {}
    bool IsActive() override;
};

class ToCAnubarakTriggerContext : public NamedObjectContext<Trigger>
{
public:
    ToCAnubarakTriggerContext() : NamedObjectContext<Trigger>()
    {
        creators["anubarak pursued by spike"] =
            &ToCAnubarakTriggerContext::anubarak_pursued_by_spike;
        creators["anubarak spike nearby"] =
            &ToCAnubarakTriggerContext::anubarak_spike_nearby;
        creators["anubarak burrower casting shadow strike"] =
            &ToCAnubarakTriggerContext::anubarak_burrower_casting_shadow_strike;
        creators["anubarak leeching swarm on tank"] =
            &ToCAnubarakTriggerContext::anubarak_leeching_swarm_on_tank;
        creators["anubarak ranged should seed permafrost"] =
            &ToCAnubarakTriggerContext::anubarak_ranged_should_seed_permafrost;
        creators["anubarak burrower should be focused"] =
            &ToCAnubarakTriggerContext::anubarak_burrower_should_be_focused;
        creators["anubarak burrower needs assist tank"] =
            &ToCAnubarakTriggerContext::anubarak_burrower_needs_assist_tank;
        creators["anubarak scarab on raid"] =
            &ToCAnubarakTriggerContext::anubarak_scarab_on_raid;
        creators["anubarak engaged by main tank"] =
            &ToCAnubarakTriggerContext::anubarak_engaged_by_main_tank;
        creators["anubarak penetrating cold on raid"] =
            &ToCAnubarakTriggerContext::anubarak_penetrating_cold_on_raid;
    }

private:
    static Trigger* anubarak_pursued_by_spike(PlayerbotAI* botAI) {
        return new AnubarakPursuedBySpikeTrigger(botAI);
    }

    static Trigger* anubarak_spike_nearby(PlayerbotAI* botAI) {
        return new AnubarakSpikeNearbyTrigger(botAI);
    }

    static Trigger* anubarak_burrower_casting_shadow_strike(PlayerbotAI* botAI) {
        return new AnubarakBurrowerCastingShadowStrikeTrigger(botAI);
    }

    static Trigger* anubarak_leeching_swarm_on_tank(PlayerbotAI* botAI) {
        return new AnubarakLeechingSwarmOnTankTrigger(botAI);
    }

    static Trigger* anubarak_ranged_should_seed_permafrost(PlayerbotAI* botAI) {
        return new AnubarakRangedShouldSeedPermafrostTrigger(botAI);
    }

    static Trigger* anubarak_burrower_should_be_focused(PlayerbotAI* botAI) {
        return new AnubarakBurrowerShouldBeFocusedTrigger(botAI);
    }

    static Trigger* anubarak_burrower_needs_assist_tank(PlayerbotAI* botAI) {
        return new AnubarakBurrowerNeedsAssistTankTrigger(botAI);
    }

    static Trigger* anubarak_scarab_on_raid(PlayerbotAI* botAI) {
        return new AnubarakScarabOnRaidTrigger(botAI);
    }

    static Trigger* anubarak_engaged_by_main_tank(PlayerbotAI* botAI) {
        return new AnubarakEngagedByMainTankTrigger(botAI);
    }

    static Trigger* anubarak_penetrating_cold_on_raid(PlayerbotAI* botAI) {
        return new AnubarakPenetratingColdOnRaidTrigger(botAI);
    }
};

void AddToCAnubarakTriggerNodes(std::vector<TriggerNode*>& triggers);

#endif
