/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDACTIONCONTEXT_H
#define PLAYERBOTS_ULDACTIONCONTEXT_H

#include "Action.h"
#include "BossAuraActions.h"
#include "NamedObjectContext.h"
#include "UldActions.h"
#include "UldDefinitions.h"

class RaidUlduarActionContext : public NamedObjectContext<Action>
{
public:
    RaidUlduarActionContext()
    {
        creators["mimiron reset encounter state action"] = &RaidUlduarActionContext::mimiron_reset_encounter_state_action;
        creators["mimiron fire resistance action"] = &RaidUlduarActionContext::mimiron_fire_resistance_action;
        creators["mimiron frost resistance action"] = &RaidUlduarActionContext::mimiron_frost_resistance_action;
        creators["mimiron shock blast action"] = &RaidUlduarActionContext::mimiron_shock_blast_action;
        creators["mimiron phase 1 positioning action"] = &RaidUlduarActionContext::mimiron_phase_1_positioning_action;
        creators["mimiron p3wx2 laser barrage action"] = &RaidUlduarActionContext::mimiron_p3wx2_laser_barrage_action;
        creators["mimiron arc spread action"] = &RaidUlduarActionContext::mimiron_arc_spread_action;
        creators["mimiron aerial command unit action"] = &RaidUlduarActionContext::mimiron_aerial_command_unit_action;
        creators["mimiron rocket strike action"] = &RaidUlduarActionContext::mimiron_rocket_strike_action;
        creators["mimiron phase 4 focus action"] = &RaidUlduarActionContext::mimiron_phase_4_focus_action;
        creators["mimiron magnetic core action"] = &RaidUlduarActionContext::mimiron_magnetic_core_action;
        creators["mimiron plasma blast defensive action"] =
            &RaidUlduarActionContext::mimiron_plasma_blast_defensive_action;
        creators["mimiron redirect threat action"] =
            &RaidUlduarActionContext::mimiron_redirect_threat_action;
        creators["mimiron set dps priority action"] = &RaidUlduarActionContext::mimiron_set_dps_priority_action;
        creators["mimiron proximity mine action"] = &RaidUlduarActionContext::mimiron_proximity_mine_action;
        creators["mimiron bomb bot action"] = &RaidUlduarActionContext::mimiron_bomb_bot_action;
        creators["mimiron pet control action"] = &RaidUlduarActionContext::mimiron_pet_control_action;
        creators["mimiron slow bomb bot action"] = &RaidUlduarActionContext::mimiron_slow_bomb_bot_action;
        creators["mimiron approach target action"] =
            &RaidUlduarActionContext::mimiron_approach_target_action;
        creators["mimiron dodge flames action"] = &RaidUlduarActionContext::mimiron_dodge_flames_action;
        creators["mimiron frost bomb action"] = &RaidUlduarActionContext::mimiron_frost_bomb_action;
        creators["mimiron fire bot action"] = &RaidUlduarActionContext::mimiron_fire_bot_action;

        for (EncounterDefinition const* encounter : UldEncounterDefinitions())
            encounter->RegisterActions(creators);
    }

private:
    static Action* mimiron_fire_resistance_action(PlayerbotAI* ai) { return new BossFireResistanceAction(ai, "mimiron"); }
    static Action* mimiron_frost_resistance_action(PlayerbotAI* ai) { return new MimironFrostResistanceAction(ai); }
    static Action* mimiron_shock_blast_action(PlayerbotAI* ai) { return new MimironShockBlastAction(ai); }
    static Action* mimiron_reset_encounter_state_action(PlayerbotAI* ai) { return new MimironResetEncounterStateAction(ai); }
    static Action* mimiron_phase_1_positioning_action(PlayerbotAI* ai) { return new MimironPhase1PositioningAction(ai); }
    static Action* mimiron_p3wx2_laser_barrage_action(PlayerbotAI* ai) { return new MimironP3Wx2LaserBarrageAction(ai); }
    static Action* mimiron_arc_spread_action(PlayerbotAI* ai) { return new MimironArcSpreadAction(ai); }
    static Action* mimiron_aerial_command_unit_action(PlayerbotAI* ai) { return new MimironAerialCommandUnitAction(ai); }
    static Action* mimiron_rocket_strike_action(PlayerbotAI* ai) { return new MimironRocketStrikeAction(ai); }
    static Action* mimiron_phase_4_focus_action(PlayerbotAI* ai) { return new MimironPhase4FocusAction(ai); }
    static Action* mimiron_magnetic_core_action(PlayerbotAI* ai) { return new MimironMagneticCoreAction(ai); }
    static Action* mimiron_plasma_blast_defensive_action(PlayerbotAI* ai)
    {
        return new MimironPlasmaBlastDefensiveAction(ai);
    }
    static Action* mimiron_redirect_threat_action(PlayerbotAI* ai)
    {
        return new MimironRedirectThreatAction(ai);
    }
    static Action* mimiron_set_dps_priority_action(PlayerbotAI* ai) { return new MimironSetDpsPriorityAction(ai); }
    static Action* mimiron_proximity_mine_action(PlayerbotAI* ai) { return new MimironProximityMineAction(ai); }
    static Action* mimiron_bomb_bot_action(PlayerbotAI* ai) { return new MimironBombBotAction(ai); }
    static Action* mimiron_pet_control_action(PlayerbotAI* ai) { return new MimironPetControlAction(ai); }
    static Action* mimiron_slow_bomb_bot_action(PlayerbotAI* ai) { return new MimironSlowBombBotAction(ai); }
    static Action* mimiron_approach_target_action(PlayerbotAI* ai)
    {
        return new MimironApproachTargetAction(ai);
    }
    static Action* mimiron_dodge_flames_action(PlayerbotAI* ai) { return new MimironDodgeFlamesAction(ai); }
    static Action* mimiron_frost_bomb_action(PlayerbotAI* ai) { return new MimironFrostBombAction(ai); }
    static Action* mimiron_fire_bot_action(PlayerbotAI* ai) { return new MimironFireBotAction(ai); }
};

#endif
