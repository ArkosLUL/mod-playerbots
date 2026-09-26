/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_NAXXACTIONCONTEXT_H
#define PLAYERBOTS_NAXXACTIONCONTEXT_H

#include "Action.h"
#include "NamedObjectContext.h"
#include "NaxxActions.h"
#include "NaxxDefinitions.h"

class RaidNaxxActionContext : public NamedObjectContext<Action>
{
public:
    RaidNaxxActionContext()
    {
        creators["sapphiron ground position"] = &RaidNaxxActionContext::sapphiron_ground_position;
        creators["sapphiron flight position"] = &RaidNaxxActionContext::sapphiron_flight_position;

        creators["kel'thuzad choose target"] = &RaidNaxxActionContext::kelthuzad_choose_target;
        creators["kel'thuzad position"] = &RaidNaxxActionContext::kelthuzad_position;
        creators["kel'thuzad flee shadow fissure"] = &RaidNaxxActionContext::kelthuzad_flee_shadow_fissure;
        creators["kel'thuzad misdirect boss to main tank"] =
            &RaidNaxxActionContext::kelthuzad_misdirect_boss_to_main_tank;
        creators["kel'thuzad cyclone chained"] = &RaidNaxxActionContext::kelthuzad_cyclone_chained;

        // creators["patchwerk ranged position"] = &RaidNaxxActionContext::patchwerk_ranged_position;

        for (EncounterDefinition const* encounter : NaxxEncounterDefinitions())
            encounter->RegisterActions(creators);
    }

private:
    static Action* sapphiron_ground_position(PlayerbotAI* ai) { return new SapphironGroundPositionAction(ai); }
    static Action* sapphiron_flight_position(PlayerbotAI* ai) { return new SapphironFlightPositionAction(ai); }
    static Action* kelthuzad_choose_target(PlayerbotAI* ai) { return new KelthuzadChooseTargetAction(ai); }
    static Action* kelthuzad_position(PlayerbotAI* ai) { return new KelthuzadPositionAction(ai); }
    static Action* kelthuzad_flee_shadow_fissure(PlayerbotAI* ai) { return new KelthuzadFleeShadowFissureAction(ai); }
    static Action* kelthuzad_misdirect_boss_to_main_tank(PlayerbotAI* ai)
    {
        return new KelthuzadMisdirectBossToMainTankAction(ai);
    }
    static Action* kelthuzad_cyclone_chained(PlayerbotAI* ai) { return new KelthuzadCycloneChainedAction(ai); }
    // static Action* patchwerk_ranged_position(PlayerbotAI* ai) { return new PatchwerkRangedPositionAction(ai); }
};

#endif
