#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_ANUBARAK_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_ANUBARAK_H

#include <vector>

#include "NamedObjectContext.h"
#include "Trigger.h"

class AnubarakKiterNeedsImmunityTrigger : public Trigger
{
public:
    AnubarakKiterNeedsImmunityTrigger(PlayerbotAI* botAI) : Trigger(botAI, "anubarak kiter needs immunity") {}
    bool IsActive() override;
};

class AnubarakPursuedBySpikeTrigger : public Trigger
{
public:
    AnubarakPursuedBySpikeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "anubarak pursued by spike") {}
    bool IsActive() override;
};

class AnubarakKiterNeedsProtectionTrigger : public Trigger
{
public:
    AnubarakKiterNeedsProtectionTrigger(PlayerbotAI* botAI) : Trigger(botAI, "anubarak kiter needs protection") {}
    bool IsActive() override;
};

class AnubarakKiterNeedsFreedomTrigger : public Trigger
{
public:
    AnubarakKiterNeedsFreedomTrigger(PlayerbotAI* botAI) : Trigger(botAI, "anubarak kiter needs freedom") {}
    bool IsActive() override;
};

class AnubarakHopEscortTrigger : public Trigger
{
public:
    AnubarakHopEscortTrigger(PlayerbotAI* botAI) : Trigger(botAI, "anubarak hop escort") {}
    bool IsActive() override;
};

class AnubarakRedirectThreatTrigger : public Trigger
{
public:
    AnubarakRedirectThreatTrigger(PlayerbotAI* botAI) : Trigger(botAI, "anubarak redirect threat") {}
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

class AnubarakDpsTargetTrigger : public Trigger
{
public:
    AnubarakDpsTargetTrigger(PlayerbotAI* botAI) : Trigger(botAI, "anubarak dps target") {}
    bool IsActive() override;
};

class AnubarakBurrowerNeedsAssistTankTrigger : public Trigger
{
public:
    AnubarakBurrowerNeedsAssistTankTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, "anubarak burrower needs assist tank") {}
    bool IsActive() override;
};

class AnubarakSideTankReadyTrigger : public Trigger
{
public:
    AnubarakSideTankReadyTrigger(PlayerbotAI* botAI) : Trigger(botAI, "anubarak side tank ready") {}
    bool IsActive() override;
};

class AnubarakSideTankNeedsDefensiveTrigger : public Trigger
{
public:
    AnubarakSideTankNeedsDefensiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, "anubarak side tank needs defensive") {}
    bool IsActive() override;
};

class AnubarakSideTankDeadTrigger : public Trigger
{
public:
    AnubarakSideTankDeadTrigger(PlayerbotAI* botAI) : Trigger(botAI, "anubarak side tank dead") {}
    bool IsActive() override;
};

class AnubarakGripFrostSphereTrigger : public Trigger
{
public:
    AnubarakGripFrostSphereTrigger(PlayerbotAI* botAI) : Trigger(botAI, "anubarak grip frost sphere") {}
    bool IsActive() override;
};

class AnubarakRaidGathersTrigger : public Trigger
{
public:
    AnubarakRaidGathersTrigger(PlayerbotAI* botAI) : Trigger(botAI, "anubarak raid gathers") {}
    bool IsActive() override;
};

class AnubarakHealerStandTrigger : public Trigger
{
public:
    AnubarakHealerStandTrigger(PlayerbotAI* botAI) : Trigger(botAI, "anubarak healer stand") {}
    bool IsActive() override;
};

class AnubarakOnPermafrostTrigger : public Trigger
{
public:
    AnubarakOnPermafrostTrigger(PlayerbotAI* botAI) : Trigger(botAI, "anubarak on permafrost") {}
    bool IsActive() override;
};

class AnubarakMeleeBurrowerStandTrigger : public Trigger
{
public:
    AnubarakMeleeBurrowerStandTrigger(PlayerbotAI* botAI) : Trigger(botAI, "anubarak melee burrower stand") {}
    bool IsActive() override;
};

class AnubarakDemonicCircleDueTrigger : public Trigger
{
public:
    AnubarakDemonicCircleDueTrigger(PlayerbotAI* botAI) : Trigger(botAI, "anubarak demonic circle due") {}
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
        creators["anubarak kiter needs immunity"] =
            &ToCAnubarakTriggerContext::anubarak_kiter_needs_immunity;
        creators["anubarak pursued by spike"] =
            &ToCAnubarakTriggerContext::anubarak_pursued_by_spike;
        creators["anubarak kiter needs protection"] =
            &ToCAnubarakTriggerContext::anubarak_kiter_needs_protection;
        creators["anubarak kiter needs freedom"] =
            &ToCAnubarakTriggerContext::anubarak_kiter_needs_freedom;
        creators["anubarak hop escort"] =
            &ToCAnubarakTriggerContext::anubarak_hop_escort;
        creators["anubarak redirect threat"] =
            &ToCAnubarakTriggerContext::anubarak_redirect_threat;
        creators["anubarak spike nearby"] =
            &ToCAnubarakTriggerContext::anubarak_spike_nearby;
        creators["anubarak burrower casting shadow strike"] =
            &ToCAnubarakTriggerContext::anubarak_burrower_casting_shadow_strike;
        creators["anubarak leeching swarm on tank"] =
            &ToCAnubarakTriggerContext::anubarak_leeching_swarm_on_tank;
        creators["anubarak ranged should seed permafrost"] =
            &ToCAnubarakTriggerContext::anubarak_ranged_should_seed_permafrost;
        creators["anubarak dps target"] =
            &ToCAnubarakTriggerContext::anubarak_dps_target;
        creators["anubarak burrower needs assist tank"] =
            &ToCAnubarakTriggerContext::anubarak_burrower_needs_assist_tank;
        creators["anubarak side tank ready"] =
            &ToCAnubarakTriggerContext::anubarak_side_tank_ready;
        creators["anubarak side tank needs defensive"] =
            &ToCAnubarakTriggerContext::anubarak_side_tank_needs_defensive;
        creators["anubarak side tank dead"] =
            &ToCAnubarakTriggerContext::anubarak_side_tank_dead;
        creators["anubarak grip frost sphere"] =
            &ToCAnubarakTriggerContext::anubarak_grip_frost_sphere;
        creators["anubarak raid gathers"] =
            &ToCAnubarakTriggerContext::anubarak_raid_gathers;
        creators["anubarak healer stand"] =
            &ToCAnubarakTriggerContext::anubarak_healer_stand;
        creators["anubarak on permafrost"] =
            &ToCAnubarakTriggerContext::anubarak_on_permafrost;
        creators["anubarak melee burrower stand"] =
            &ToCAnubarakTriggerContext::anubarak_melee_burrower_stand;
        creators["anubarak demonic circle due"] =
            &ToCAnubarakTriggerContext::anubarak_demonic_circle_due;
        creators["anubarak scarab on raid"] =
            &ToCAnubarakTriggerContext::anubarak_scarab_on_raid;
        creators["anubarak engaged by main tank"] =
            &ToCAnubarakTriggerContext::anubarak_engaged_by_main_tank;
        creators["anubarak penetrating cold on raid"] =
            &ToCAnubarakTriggerContext::anubarak_penetrating_cold_on_raid;
    }

private:
    static Trigger* anubarak_kiter_needs_immunity(PlayerbotAI* botAI) {
        return new AnubarakKiterNeedsImmunityTrigger(botAI);
    }

    static Trigger* anubarak_pursued_by_spike(PlayerbotAI* botAI) {
        return new AnubarakPursuedBySpikeTrigger(botAI);
    }

    static Trigger* anubarak_kiter_needs_protection(PlayerbotAI* botAI) {
        return new AnubarakKiterNeedsProtectionTrigger(botAI);
    }

    static Trigger* anubarak_kiter_needs_freedom(PlayerbotAI* botAI) {
        return new AnubarakKiterNeedsFreedomTrigger(botAI);
    }

    static Trigger* anubarak_hop_escort(PlayerbotAI* botAI) {
        return new AnubarakHopEscortTrigger(botAI);
    }

    static Trigger* anubarak_redirect_threat(PlayerbotAI* botAI) {
        return new AnubarakRedirectThreatTrigger(botAI);
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

    static Trigger* anubarak_dps_target(PlayerbotAI* botAI) {
        return new AnubarakDpsTargetTrigger(botAI);
    }

    static Trigger* anubarak_burrower_needs_assist_tank(PlayerbotAI* botAI) {
        return new AnubarakBurrowerNeedsAssistTankTrigger(botAI);
    }

    static Trigger* anubarak_side_tank_ready(PlayerbotAI* botAI) {
        return new AnubarakSideTankReadyTrigger(botAI);
    }

    static Trigger* anubarak_side_tank_needs_defensive(PlayerbotAI* botAI) {
        return new AnubarakSideTankNeedsDefensiveTrigger(botAI);
    }

    static Trigger* anubarak_side_tank_dead(PlayerbotAI* botAI) {
        return new AnubarakSideTankDeadTrigger(botAI);
    }

    static Trigger* anubarak_grip_frost_sphere(PlayerbotAI* botAI) {
        return new AnubarakGripFrostSphereTrigger(botAI);
    }

    static Trigger* anubarak_raid_gathers(PlayerbotAI* botAI) {
        return new AnubarakRaidGathersTrigger(botAI);
    }

    static Trigger* anubarak_healer_stand(PlayerbotAI* botAI) {
        return new AnubarakHealerStandTrigger(botAI);
    }

    static Trigger* anubarak_on_permafrost(PlayerbotAI* botAI) {
        return new AnubarakOnPermafrostTrigger(botAI);
    }

    static Trigger* anubarak_melee_burrower_stand(PlayerbotAI* botAI) {
        return new AnubarakMeleeBurrowerStandTrigger(botAI);
    }

    static Trigger* anubarak_demonic_circle_due(PlayerbotAI* botAI) {
        return new AnubarakDemonicCircleDueTrigger(botAI);
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
