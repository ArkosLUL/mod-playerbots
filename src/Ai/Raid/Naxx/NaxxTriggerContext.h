/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_NAXXTRIGGERCONTEXT_H
#define PLAYERBOTS_NAXXTRIGGERCONTEXT_H

#include "NamedObjectContext.h"
#include "NaxxDefinitions.h"
#include "NaxxTriggers.h"

class RaidNaxxTriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidNaxxTriggerContext()
    {
        creators["mutating injection melee"] = &RaidNaxxTriggerContext::mutating_injection_melee;
        creators["mutating injection ranged"] = &RaidNaxxTriggerContext::mutating_injection_ranged;
        creators["mutating injection removed"] = &RaidNaxxTriggerContext::mutating_injection_removed;
        creators["grobbulus cloud"] = &RaidNaxxTriggerContext::grobbulus_cloud;

        creators["thaddius prepull split"] = &RaidNaxxTriggerContext::thaddius_prepull_split;
        creators["thaddius phase pet"] = &RaidNaxxTriggerContext::thaddius_phase_pet;
        creators["thaddius phase pet lose aggro"] = &RaidNaxxTriggerContext::thaddius_phase_pet_lose_aggro;
        creators["thaddius phase transition"] = &RaidNaxxTriggerContext::thaddius_phase_transition;
        creators["thaddius phase thaddius"] = &RaidNaxxTriggerContext::thaddius_phase_thaddius;
        creators["thaddius redirect threat"] = &RaidNaxxTriggerContext::thaddius_redirect_threat;

        creators["sapphiron ground"] = &RaidNaxxTriggerContext::sapphiron_ground;
        creators["sapphiron flight"] = &RaidNaxxTriggerContext::sapphiron_flight;

        creators["kel'thuzad"] = &RaidNaxxTriggerContext::kelthuzad;
        creators["kel'thuzad shadow fissure"] = &RaidNaxxTriggerContext::kelthuzad_shadow_fissure;
        creators["kel'thuzad chains"] = &RaidNaxxTriggerContext::kelthuzad_chains;

        // creators["patchwerk tank"] = &RaidNaxxTriggerContext::patchwerk_tank;
        // creators["patchwerk non-tank"] = &RaidNaxxTriggerContext::patchwerk_non_tank;
        // creators["patchwerk ranged"] = &RaidNaxxTriggerContext::patchwerk_ranged;

        creators["gluth"] = &RaidNaxxTriggerContext::gluth;
        creators["gluth main tank mortal wound"] = &RaidNaxxTriggerContext::gluth_main_tank_mortal_wound;
        creators["gluth low health zombie aoe"] = &RaidNaxxTriggerContext::gluth_low_health_zombie_aoe;
        creators["gluth frenzy"] = &RaidNaxxTriggerContext::gluth_frenzy;
        creators["gluth redirect threat"] = &RaidNaxxTriggerContext::gluth_redirect_threat;

        for (EncounterDefinition const* encounter : NaxxEncounterDefinitions())
            encounter->RegisterTriggers(creators);
    }

private:
    static Trigger* mutating_injection_melee(PlayerbotAI* ai) { return new MutatingInjectionMeleeTrigger(ai); }
    static Trigger* mutating_injection_ranged(PlayerbotAI* ai) { return new MutatingInjectionRangedTrigger(ai); }
    static Trigger* mutating_injection_removed(PlayerbotAI* ai) { return new MutatingInjectionRemovedTrigger(ai); }
    static Trigger* grobbulus_cloud(PlayerbotAI* ai) { return new GrobbulusCloudTrigger(ai); }

    static Trigger* thaddius_prepull_split(PlayerbotAI* ai) { return new ThaddiusPrepullSplitTrigger(ai); }
    static Trigger* thaddius_phase_pet(PlayerbotAI* ai) { return new ThaddiusPhasePetTrigger(ai); }
    static Trigger* thaddius_phase_pet_lose_aggro(PlayerbotAI* ai) { return new ThaddiusPhasePetLoseAggroTrigger(ai); }
    static Trigger* thaddius_phase_transition(PlayerbotAI* ai) { return new ThaddiusPhaseTransitionTrigger(ai); }
    static Trigger* thaddius_phase_thaddius(PlayerbotAI* ai) { return new ThaddiusPhaseThaddiusTrigger(ai); }
    static Trigger* thaddius_redirect_threat(PlayerbotAI* ai) { return new ThaddiusRedirectThreatTrigger(ai); }

    static Trigger* sapphiron_ground(PlayerbotAI* ai) { return new SapphironGroundTrigger(ai); }
    static Trigger* sapphiron_flight(PlayerbotAI* ai) { return new SapphironFlightTrigger(ai); }
    static Trigger* kelthuzad(PlayerbotAI* ai) { return new KelthuzadTrigger(ai); }
    static Trigger* kelthuzad_shadow_fissure(PlayerbotAI* ai) { return new KelthuzadShadowFissureTrigger(ai); }
    static Trigger* kelthuzad_chains(PlayerbotAI* ai) { return new KelthuzadChainsTrigger(ai); }
    // static Trigger* patchwerk_tank(PlayerbotAI* ai) { return new PatchwerkTankTrigger(ai); }
    // static Trigger* patchwerk_non_tank(PlayerbotAI* ai) { return new PatchwerkNonTankTrigger(ai); }
    // static Trigger* patchwerk_ranged(PlayerbotAI* ai) { return new PatchwerkRangedTrigger(ai); }
    static Trigger* gluth(PlayerbotAI* ai) { return new GluthTrigger(ai); }
    static Trigger* gluth_main_tank_mortal_wound(PlayerbotAI* ai) { return new GluthMainTankMortalWoundTrigger(ai); }
    static Trigger* gluth_low_health_zombie_aoe(PlayerbotAI* ai) { return new GluthLowHealthZombieAoeTrigger(ai); }
    static Trigger* gluth_frenzy(PlayerbotAI* ai) { return new GluthFrenzyTrigger(ai); }
    static Trigger* gluth_redirect_threat(PlayerbotAI* ai) { return new GluthRedirectThreatTrigger(ai); }
};

#endif
