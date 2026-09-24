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
        creators["sara shadow resistance action"] = &RaidUlduarActionContext::sara_shadow_resistance_action;
        creators["yogg-saron shadow resistance action"] = &RaidUlduarActionContext::yogg_saron_shadow_resistance_action;
        creators["yogg-saron phase 1 spacing action"] = &RaidUlduarActionContext::yogg_saron_phase_1_spacing_action;
        creators["yogg-saron phase 1 station action"] = &RaidUlduarActionContext::yogg_saron_phase_1_station_action;
        creators["yogg-saron dark volley interrupt action"] = &RaidUlduarActionContext::yogg_saron_dark_volley_interrupt_action;
        creators["yogg-saron guardian positioning action"] = &RaidUlduarActionContext::yogg_saron_guardian_positioning_action;
        creators["yogg-saron sanity action"] = &RaidUlduarActionContext::yogg_saron_sanity_action;
        creators["yogg-saron malady of the mind action"] = &RaidUlduarActionContext::yogg_saron_malady_of_the_mind_action;
        creators["yogg-saron phase 3 control action"] = &RaidUlduarActionContext::yogg_saron_phase_3_control_action;
        creators["yogg-saron set dps priority action"] = &RaidUlduarActionContext::yogg_saron_set_dps_priority_action;
        creators["yogg-saron phase 2 spacing action"] = &RaidUlduarActionContext::yogg_saron_phase_2_spacing_action;
        creators["yogg-saron brain link action"] = &RaidUlduarActionContext::yogg_saron_brain_link_action;
        creators["yogg-saron move to enter portal action"] = &RaidUlduarActionContext::yogg_saron_move_to_enter_portal_action;
        creators["yogg-saron use portal action"] = &RaidUlduarActionContext::yogg_saron_use_portal_action;
        creators["yogg-saron fall from floor action"] = &RaidUlduarActionContext::yogg_saron_fall_from_floor_action;
        creators["yogg-saron stop following action"] = &RaidUlduarActionContext::yogg_saron_stop_following_action;
        creators["yogg-saron illusion room action"] = &RaidUlduarActionContext::yogg_saron_illusion_room_action;
        creators["yogg-saron move to exit portal action"] = &RaidUlduarActionContext::yogg_saron_move_to_exit_portal_action;
        creators["yogg-saron laughing skull action"] = &RaidUlduarActionContext::yogg_saron_laughing_skull_action;
        creators["yogg-saron lunatic gaze action"] = &RaidUlduarActionContext::yogg_saron_lunatic_gaze_action;
        creators["yogg-saron phase 3 positioning action"] = &RaidUlduarActionContext::yogg_saron_phase_3_positioning_action;
        creators["yogg-saron guardian control action"] = &RaidUlduarActionContext::yogg_saron_guardian_control_action;
        creators["yogg-saron sanity conservation action"] = &RaidUlduarActionContext::yogg_saron_sanity_conservation_action;
        creators["yogg-saron squeeze escape action"] = &RaidUlduarActionContext::yogg_saron_squeeze_escape_action;
        creators["yogg-saron squeeze rescue action"] = &RaidUlduarActionContext::yogg_saron_squeeze_rescue_action;
        creators["yogg-saron illusion facing action"] = &RaidUlduarActionContext::yogg_saron_illusion_facing_action;
        creators["yogg-saron pet guard action"] = &RaidUlduarActionContext::yogg_saron_pet_guard_action;
        creators["yogg-saron body detour action"] = &RaidUlduarActionContext::yogg_saron_body_detour_action;
        creators["yogg-saron diminish power judgement action"] = &RaidUlduarActionContext::yogg_saron_diminish_power_judgement_action;
        creators["yogg-saron illusion healer station action"] = &RaidUlduarActionContext::yogg_saron_illusion_healer_station_action;
        creators["yogg-saron brain spot action"] = &RaidUlduarActionContext::yogg_saron_brain_spot_action;
        creators["yogg-saron anti fear action"] = &RaidUlduarActionContext::yogg_saron_anti_fear_action;
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
    static Action* sara_shadow_resistance_action(PlayerbotAI* ai) { return new BossShadowResistanceAction(ai, "sara"); }
    static Action* yogg_saron_shadow_resistance_action(PlayerbotAI* ai) { return new BossShadowResistanceAction(ai, "yogg-saron"); }
    static Action* yogg_saron_phase_1_spacing_action(PlayerbotAI* ai) { return new YoggSaronPhase1SpacingAction(ai); }
    static Action* yogg_saron_phase_1_station_action(PlayerbotAI* ai) { return new YoggSaronPhase1StationAction(ai); }
    static Action* yogg_saron_dark_volley_interrupt_action(PlayerbotAI* ai) { return new YoggSaronDarkVolleyInterruptAction(ai); }
    static Action* yogg_saron_guardian_positioning_action(PlayerbotAI* ai) { return new YoggSaronGuardianPositioningAction(ai); }
    static Action* yogg_saron_sanity_action(PlayerbotAI* ai) { return new YoggSaronSanityAction(ai); }
    static Action* yogg_saron_malady_of_the_mind_action(PlayerbotAI* ai) { return new YoggSaronMaladyOfTheMindAction(ai); }
    static Action* yogg_saron_phase_3_control_action(PlayerbotAI* ai) { return new YoggSaronPhase3ControlAction(ai); }
    static Action* yogg_saron_set_dps_priority_action(PlayerbotAI* ai) { return new YoggSaronSetDpsPriorityAction(ai); }
    static Action* yogg_saron_phase_2_spacing_action(PlayerbotAI* ai) { return new YoggSaronPhase2SpacingAction(ai); }
    static Action* yogg_saron_brain_link_action(PlayerbotAI* ai) { return new YoggSaronBrainLinkAction(ai); }
    static Action* yogg_saron_move_to_enter_portal_action(PlayerbotAI* ai) { return new YoggSaronMoveToEnterPortalAction(ai); }
    static Action* yogg_saron_use_portal_action(PlayerbotAI* ai) { return new YoggSaronUsePortalAction(ai); }
    static Action* yogg_saron_fall_from_floor_action(PlayerbotAI* ai) { return new YoggSaronFallFromFloorAction(ai); }
    static Action* yogg_saron_stop_following_action(PlayerbotAI* ai) { return new YoggSaronStopFollowingAction(ai); }
    static Action* yogg_saron_illusion_room_action(PlayerbotAI* ai) { return new YoggSaronIllusionRoomAction(ai); }
    static Action* yogg_saron_move_to_exit_portal_action(PlayerbotAI* ai) { return new YoggSaronMoveToExitPortalAction(ai); }
    static Action* yogg_saron_laughing_skull_action(PlayerbotAI* ai) { return new YoggSaronLaughingSkullAction(ai); }
    static Action* yogg_saron_lunatic_gaze_action(PlayerbotAI* ai) { return new YoggSaronLunaticGazeAction(ai); }
    static Action* yogg_saron_phase_3_positioning_action(PlayerbotAI* ai) { return new YoggSaronPhase3PositioningAction(ai); }
    static Action* yogg_saron_guardian_control_action(PlayerbotAI* ai) { return new YoggSaronGuardianControlAction(ai); }
    static Action* yogg_saron_sanity_conservation_action(PlayerbotAI* ai) { return new YoggSaronSanityConservationAction(ai); }
    static Action* yogg_saron_squeeze_escape_action(PlayerbotAI* ai) { return new YoggSaronSqueezeEscapeAction(ai); }
    static Action* yogg_saron_squeeze_rescue_action(PlayerbotAI* ai) { return new YoggSaronSqueezeRescueAction(ai); }
    static Action* yogg_saron_illusion_facing_action(PlayerbotAI* ai) { return new YoggSaronIllusionFacingAction(ai); }
    static Action* yogg_saron_pet_guard_action(PlayerbotAI* ai) { return new YoggSaronPetGuardAction(ai); }
    static Action* yogg_saron_body_detour_action(PlayerbotAI* ai) { return new YoggSaronBodyDetourAction(ai); }
    static Action* yogg_saron_diminish_power_judgement_action(PlayerbotAI* ai) { return new YoggSaronDiminishPowerJudgementAction(ai); }
    static Action* yogg_saron_illusion_healer_station_action(PlayerbotAI* ai) { return new YoggSaronIllusionHealerStationAction(ai); }
    static Action* yogg_saron_brain_spot_action(PlayerbotAI* ai) { return new YoggSaronBrainSpotAction(ai); }
    static Action* yogg_saron_anti_fear_action(PlayerbotAI* ai) { return new YoggSaronAntiFearAction(ai); }
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
