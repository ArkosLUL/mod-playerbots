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
        creators["flame leviathan vehicle"] = &RaidUlduarActionContext::flame_leviathan_vehicle;
        creators["flame leviathan enter vehicle"] = &RaidUlduarActionContext::flame_leviathan_enter_vehicle;
        creators["flame leviathan drive"] = &RaidUlduarActionContext::flame_leviathan_drive;
        creators["flame leviathan interrupt vents"] = &RaidUlduarActionContext::flame_leviathan_interrupt_vents;
        creators["ignis fire resistance action"] = &RaidUlduarActionContext::ignis_fire_resistance_action;
        creators["iron assembly reset encounter state action"] = &RaidUlduarActionContext::iron_assembly_reset_encounter_state_action;
        creators["iron assembly lightning tendrils action"] = &RaidUlduarActionContext::iron_assembly_lightning_tendrils_action;
        creators["iron assembly overload action"] = &RaidUlduarActionContext::iron_assembly_overload_action;
        creators["iron assembly rune of death action"] = &RaidUlduarActionContext::iron_assembly_rune_of_death_action;
        creators["iron assembly interrupt action"] = &RaidUlduarActionContext::iron_assembly_interrupt_action;
        creators["iron assembly tank assignment action"] = &RaidUlduarActionContext::iron_assembly_tank_assignment_action;
        creators["iron assembly shield of runes action"] = &RaidUlduarActionContext::iron_assembly_shield_of_runes_action;
        creators["iron assembly fusion punch dispel action"] = &RaidUlduarActionContext::iron_assembly_fusion_punch_dispel_action;
        creators["iron assembly redirect threat action"] = &RaidUlduarActionContext::iron_assembly_redirect_threat_action;
        creators["iron assembly rune of power soak action"] = &RaidUlduarActionContext::iron_assembly_rune_of_power_soak_action;
        creators["iron assembly set dps priority action"] = &RaidUlduarActionContext::iron_assembly_set_dps_priority_action;
        creators["iron assembly raid position action"] = &RaidUlduarActionContext::iron_assembly_raid_position_action;
        creators["hodir move snowpacked icicle"] = &RaidUlduarActionContext::hodir_move_snowpacked_icicle;
        creators["hodir biting cold shed"] = &RaidUlduarActionContext::hodir_biting_cold_shed;
        creators["hodir frost resistance action"] = &RaidUlduarActionContext::hodir_frost_resistance_action;
        creators["hodir spread storm cloud"] = &RaidUlduarActionContext::hodir_spread_storm_cloud;
        creators["hodir collect storm power"] = &RaidUlduarActionContext::hodir_collect_storm_power;
        creators["hodir icicle dodge action"] = &RaidUlduarActionContext::hodir_icicle_dodge_action;
        creators["hodir raid position action"] = &RaidUlduarActionContext::hodir_raid_position_action;
        creators["hodir set dps priority action"] = &RaidUlduarActionContext::hodir_set_dps_priority_action;
        creators["hodir frozen blows swap action"] = &RaidUlduarActionContext::hodir_frozen_blows_swap_action;
        creators["hodir redirect threat action"] = &RaidUlduarActionContext::hodir_redirect_threat_action;
        creators["thorim frost resistance action"] = &RaidUlduarActionContext::thorim_frost_resistance_action;
        creators["thorim nature resistance action"] = &RaidUlduarActionContext::thorim_nature_resistance_action;
        creators["thorim dps priority action"] = &RaidUlduarActionContext::thorim_dps_priority_action;
        creators["thorim arena positioning action"] = &RaidUlduarActionContext::thorim_arena_positioning_action;
        creators["thorim gauntlet positioning action"] = &RaidUlduarActionContext::thorim_gauntlet_positioning_action;
        creators["thorim balcony advance action"] = &RaidUlduarActionContext::thorim_balcony_advance_action;
        creators["thorim phase 2 positioning action"] = &RaidUlduarActionContext::thorim_phase2_positioning_action;
        creators["thorim fall from floor action"] = &RaidUlduarActionContext::thorim_fall_from_floor_action;
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
        creators["ignis scorched ground action"] = &RaidUlduarActionContext::ignis_scorched_ground_action;
        creators["ignis main tank position action"] = &RaidUlduarActionContext::ignis_main_tank_position_action;
        creators["ignis construct tank action"] = &RaidUlduarActionContext::ignis_construct_tank_action;
        creators["ignis attack brittle construct action"] = &RaidUlduarActionContext::ignis_attack_brittle_construct_action;
        creators["ignis attack boss action"] = &RaidUlduarActionContext::ignis_attack_boss_action;
        creators["ignis flame jets hold cast action"] = &RaidUlduarActionContext::ignis_flame_jets_hold_cast_action;
        creators["ignis molten construct avoid action"] = &RaidUlduarActionContext::ignis_molten_construct_avoid_action;
        creators["ignis slag pot heal action"] = &RaidUlduarActionContext::ignis_slag_pot_heal_action;
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
        creators["thorim unbalancing strike swap action"] = &RaidUlduarActionContext::thorim_unbalancing_strike_swap_action;
        creators["thorim tank pickup action"] = &RaidUlduarActionContext::thorim_tank_pickup_action;
        creators["thorim sif blizzard action"] = &RaidUlduarActionContext::thorim_sif_blizzard_action;
        creators["thorim runic smash action"] = &RaidUlduarActionContext::thorim_runic_smash_action;
        creators["thorim runic barrier bail action"] = &RaidUlduarActionContext::thorim_runic_barrier_bail_action;
        creators["thorim reset encounter state action"] = &RaidUlduarActionContext::thorim_reset_encounter_state_action;
        creators["thorim charged orb action"] = &RaidUlduarActionContext::thorim_charged_orb_action;
        creators["thorim pet leash action"] = &RaidUlduarActionContext::thorim_pet_leash_action;
        creators["thorim arena leash action"] = &RaidUlduarActionContext::thorim_arena_leash_action;
        creators["mimiron approach target action"] =
            &RaidUlduarActionContext::mimiron_approach_target_action;
        creators["mimiron dodge flames action"] = &RaidUlduarActionContext::mimiron_dodge_flames_action;
        creators["mimiron frost bomb action"] = &RaidUlduarActionContext::mimiron_frost_bomb_action;
        creators["mimiron fire bot action"] = &RaidUlduarActionContext::mimiron_fire_bot_action;

        for (EncounterDefinition const* encounter : UldEncounterDefinitions())
            encounter->RegisterActions(creators);
    }

private:
    static Action* flame_leviathan_vehicle(PlayerbotAI* ai) { return new FlameLeviathanVehicleAction(ai); }
    static Action* flame_leviathan_enter_vehicle(PlayerbotAI* ai) { return new FlameLeviathanEnterVehicleAction(ai); }
    static Action* flame_leviathan_drive(PlayerbotAI* ai) { return new FlameLeviathanDriveAction(ai); }
    static Action* flame_leviathan_interrupt_vents(PlayerbotAI* ai) { return new FlameLeviathanInterruptVentsAction(ai); }
    static Action* ignis_fire_resistance_action(PlayerbotAI* ai) { return new BossFireResistanceAction(ai, "ignis the furnace master"); }
    static Action* iron_assembly_reset_encounter_state_action(PlayerbotAI* ai) { return new IronAssemblyResetEncounterStateAction(ai); }
    static Action* iron_assembly_lightning_tendrils_action(PlayerbotAI* ai) { return new IronAssemblyLightningTendrilsAction(ai); }
    static Action* iron_assembly_overload_action(PlayerbotAI* ai) { return new IronAssemblyOverloadAction(ai); }
    static Action* iron_assembly_rune_of_death_action(PlayerbotAI* ai) { return new IronAssemblyRuneOfDeathAction(ai); }
    static Action* iron_assembly_interrupt_action(PlayerbotAI* ai) { return new IronAssemblyInterruptAction(ai); }
    static Action* iron_assembly_tank_assignment_action(PlayerbotAI* ai) { return new IronAssemblyTankAssignmentAction(ai); }
    static Action* iron_assembly_shield_of_runes_action(PlayerbotAI* ai) { return new IronAssemblyShieldOfRunesAction(ai); }
    static Action* iron_assembly_fusion_punch_dispel_action(PlayerbotAI* ai) { return new IronAssemblyFusionPunchDispelAction(ai); }
    static Action* iron_assembly_redirect_threat_action(PlayerbotAI* ai) { return new IronAssemblyRedirectThreatAction(ai); }
    static Action* iron_assembly_rune_of_power_soak_action(PlayerbotAI* ai) { return new IronAssemblyRuneOfPowerSoakAction(ai); }
    static Action* iron_assembly_set_dps_priority_action(PlayerbotAI* ai) { return new IronAssemblySetDpsPriorityAction(ai); }
    static Action* iron_assembly_raid_position_action(PlayerbotAI* ai) { return new IronAssemblyRaidPositionAction(ai); }
    static Action* hodir_move_snowpacked_icicle(PlayerbotAI* ai) { return new HodirMoveSnowpackedIcicleAction(ai); }
    static Action* hodir_biting_cold_shed(PlayerbotAI* ai) { return new HodirBitingColdShedAction(ai); }
    static Action* hodir_frost_resistance_action(PlayerbotAI* ai) { return new HodirFrostResistanceAction(ai); }
    static Action* hodir_spread_storm_cloud(PlayerbotAI* ai) { return new HodirSpreadStormCloudAction(ai); }
    static Action* hodir_collect_storm_power(PlayerbotAI* ai) { return new HodirCollectStormPowerAction(ai); }
    static Action* hodir_icicle_dodge_action(PlayerbotAI* ai) { return new HodirIcicleDodgeAction(ai); }
    static Action* hodir_raid_position_action(PlayerbotAI* ai) { return new HodirRaidPositionAction(ai); }
    static Action* hodir_set_dps_priority_action(PlayerbotAI* ai) { return new HodirSetDpsPriorityAction(ai); }
    static Action* hodir_frozen_blows_swap_action(PlayerbotAI* ai) { return new HodirFrozenBlowsSwapAction(ai); }
    static Action* hodir_redirect_threat_action(PlayerbotAI* ai) { return new HodirRedirectThreatAction(ai); }
    static Action* thorim_frost_resistance_action(PlayerbotAI* ai) { return new BossFrostResistanceAction(ai, "thorim"); }
    static Action* thorim_nature_resistance_action(PlayerbotAI* ai) { return new BossNatureResistanceAction(ai, "thorim"); }
    static Action* thorim_dps_priority_action(PlayerbotAI* ai) { return new ThorimDpsPriorityAction(ai); }
    static Action* thorim_arena_positioning_action(PlayerbotAI* ai) { return new ThorimArenaPositioningAction(ai); }
    static Action* thorim_gauntlet_positioning_action(PlayerbotAI* ai) { return new ThorimGauntletPositioningAction(ai); }
    static Action* thorim_balcony_advance_action(PlayerbotAI* ai) { return new ThorimBalconyAdvanceAction(ai); }
    static Action* thorim_phase2_positioning_action(PlayerbotAI* ai) { return new ThorimPhase2PositioningAction(ai); }
    static Action* thorim_fall_from_floor_action(PlayerbotAI* ai) { return new ThorimFallFromFloorAction(ai); }
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
    static Action* ignis_scorched_ground_action(PlayerbotAI* ai) { return new IgnisScorchedGroundAction(ai); }
    static Action* ignis_main_tank_position_action(PlayerbotAI* ai) { return new IgnisMainTankPositionAction(ai); }
    static Action* ignis_construct_tank_action(PlayerbotAI* ai) { return new IgnisConstructTankAction(ai); }
    static Action* ignis_attack_brittle_construct_action(PlayerbotAI* ai) { return new IgnisAttackBrittleConstructAction(ai); }
    static Action* ignis_attack_boss_action(PlayerbotAI* ai) { return new IgnisAttackBossAction(ai); }
    static Action* ignis_flame_jets_hold_cast_action(PlayerbotAI* ai) { return new IgnisFlameJetsHoldCastAction(ai); }
    static Action* ignis_molten_construct_avoid_action(PlayerbotAI* ai) { return new IgnisMoltenConstructAvoidAction(ai); }
    static Action* ignis_slag_pot_heal_action(PlayerbotAI* ai) { return new IgnisSlagPotHealAction(ai); }
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
    static Action* thorim_unbalancing_strike_swap_action(PlayerbotAI* ai) { return new ThorimUnbalancingStrikeSwapAction(ai); }
    static Action* thorim_tank_pickup_action(PlayerbotAI* ai) { return new ThorimTankPickupAction(ai); }
    static Action* thorim_sif_blizzard_action(PlayerbotAI* ai) { return new ThorimSifBlizzardAction(ai); }
    static Action* thorim_runic_smash_action(PlayerbotAI* ai) { return new ThorimRunicSmashAction(ai); }
    static Action* thorim_runic_barrier_bail_action(PlayerbotAI* ai) { return new ThorimRunicBarrierBailAction(ai); }
    static Action* thorim_reset_encounter_state_action(PlayerbotAI* ai) { return new ThorimResetEncounterStateAction(ai); }
    static Action* thorim_charged_orb_action(PlayerbotAI* ai) { return new ThorimChargedOrbAction(ai); }
    static Action* thorim_pet_leash_action(PlayerbotAI* ai) { return new ThorimPetLeashAction(ai); }
    static Action* thorim_arena_leash_action(PlayerbotAI* ai) { return new ThorimArenaLeashAction(ai); }
    static Action* mimiron_approach_target_action(PlayerbotAI* ai)
    {
        return new MimironApproachTargetAction(ai);
    }
    static Action* mimiron_dodge_flames_action(PlayerbotAI* ai) { return new MimironDodgeFlamesAction(ai); }
    static Action* mimiron_frost_bomb_action(PlayerbotAI* ai) { return new MimironFrostBombAction(ai); }
    static Action* mimiron_fire_bot_action(PlayerbotAI* ai) { return new MimironFireBotAction(ai); }
};

#endif
