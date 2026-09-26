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
        creators["sapphiron ground"] = &RaidNaxxTriggerContext::sapphiron_ground;
        creators["sapphiron flight"] = &RaidNaxxTriggerContext::sapphiron_flight;

        creators["kel'thuzad"] = &RaidNaxxTriggerContext::kelthuzad;
        creators["kel'thuzad shadow fissure"] = &RaidNaxxTriggerContext::kelthuzad_shadow_fissure;
        creators["kel'thuzad chains"] = &RaidNaxxTriggerContext::kelthuzad_chains;

        // creators["patchwerk tank"] = &RaidNaxxTriggerContext::patchwerk_tank;
        // creators["patchwerk non-tank"] = &RaidNaxxTriggerContext::patchwerk_non_tank;
        // creators["patchwerk ranged"] = &RaidNaxxTriggerContext::patchwerk_ranged;

        for (EncounterDefinition const* encounter : NaxxEncounterDefinitions())
            encounter->RegisterTriggers(creators);
    }

private:
    static Trigger* sapphiron_ground(PlayerbotAI* ai) { return new SapphironGroundTrigger(ai); }
    static Trigger* sapphiron_flight(PlayerbotAI* ai) { return new SapphironFlightTrigger(ai); }
    static Trigger* kelthuzad(PlayerbotAI* ai) { return new KelthuzadTrigger(ai); }
    static Trigger* kelthuzad_shadow_fissure(PlayerbotAI* ai) { return new KelthuzadShadowFissureTrigger(ai); }
    static Trigger* kelthuzad_chains(PlayerbotAI* ai) { return new KelthuzadChainsTrigger(ai); }
    // static Trigger* patchwerk_tank(PlayerbotAI* ai) { return new PatchwerkTankTrigger(ai); }
    // static Trigger* patchwerk_non_tank(PlayerbotAI* ai) { return new PatchwerkNonTankTrigger(ai); }
    // static Trigger* patchwerk_ranged(PlayerbotAI* ai) { return new PatchwerkRangedTrigger(ai); }
};

#endif
