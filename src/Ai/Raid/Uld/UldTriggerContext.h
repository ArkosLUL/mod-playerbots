/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDTRIGGERCONTEXT_H
#define PLAYERBOTS_ULDTRIGGERCONTEXT_H

#include "BossAuraTriggers.h"
#include "NamedObjectContext.h"
#include "UldEncounterGate.h"
#include "UldTriggers.h"

class RaidUlduarTriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidUlduarTriggerContext()
    {
        creators["flame leviathan on vehicle"] = &RaidUlduarTriggerContext::flame_leviathan_on_vehicle;
        creators["flame leviathan vehicle near"] = &RaidUlduarTriggerContext::flame_leviathan_vehicle_near;
        creators["flame leviathan flame vents"] = &RaidUlduarTriggerContext::flame_leviathan_flame_vents;
        creators["flame leviathan drive urgent"] = &RaidUlduarTriggerContext::flame_leviathan_drive_urgent;
        creators["razorscale flying alone"] = &RaidUlduarTriggerContext::razorscale_flying_alone;
        creators["razorscale avoid devouring flames"] = &RaidUlduarTriggerContext::razorscale_avoid_devouring_flames;
        creators["razorscale avoid sentinel"] = &RaidUlduarTriggerContext::razorscale_avoid_sentinel;
        creators["razorscale avoid whirlwind"] = &RaidUlduarTriggerContext::razorscale_avoid_whirlwind;
        creators["razorscale grounded"] = &RaidUlduarTriggerContext::razorscale_grounded;
        creators["razorscale harpoon trigger"] = &RaidUlduarTriggerContext::razorscale_harpoon_trigger;
        creators["razorscale fuse armor trigger"] = &RaidUlduarTriggerContext::razorscale_fuse_armor_trigger;
        creators["razorscale kill target trigger"] = &RaidUlduarTriggerContext::razorscale_kill_target_trigger;
        creators["razorscale pet control trigger"] = &RaidUlduarTriggerContext::razorscale_pet_control_trigger;
        creators["razorscale flame breath trigger"] = &RaidUlduarTriggerContext::razorscale_flame_breath_trigger;
        creators["razorscale fire resistance trigger"] = &RaidUlduarTriggerContext::razorscale_fire_resistance_trigger;
        creators["ignis fire resistance trigger"] = &RaidUlduarTriggerContext::ignis_fire_resistance_trigger;
        creators["iron assembly reset encounter state trigger"] = &RaidUlduarTriggerContext::iron_assembly_reset_encounter_state_trigger;
        creators["iron assembly overwhelming power run out trigger"] = &RaidUlduarTriggerContext::iron_assembly_overwhelming_power_run_out_trigger;
        creators["iron assembly lightning tendrils trigger"] = &RaidUlduarTriggerContext::iron_assembly_lightning_tendrils_trigger;
        creators["iron assembly overload trigger"] = &RaidUlduarTriggerContext::iron_assembly_overload_trigger;
        creators["iron assembly rune of death trigger"] = &RaidUlduarTriggerContext::iron_assembly_rune_of_death_trigger;
        creators["iron assembly interrupt trigger"] = &RaidUlduarTriggerContext::iron_assembly_interrupt_trigger;
        creators["iron assembly tank assignment trigger"] = &RaidUlduarTriggerContext::iron_assembly_tank_assignment_trigger;
        creators["iron assembly overwhelming power swap trigger"] = &RaidUlduarTriggerContext::iron_assembly_overwhelming_power_swap_trigger;
        creators["iron assembly shield of runes trigger"] = &RaidUlduarTriggerContext::iron_assembly_shield_of_runes_trigger;
        creators["iron assembly fusion punch dispel trigger"] = &RaidUlduarTriggerContext::iron_assembly_fusion_punch_dispel_trigger;
        creators["iron assembly redirect threat trigger"] = &RaidUlduarTriggerContext::iron_assembly_redirect_threat_trigger;
        creators["iron assembly rune of power trigger"] = &RaidUlduarTriggerContext::iron_assembly_rune_of_power_trigger;
        creators["iron assembly rune of power soak trigger"] = &RaidUlduarTriggerContext::iron_assembly_rune_of_power_soak_trigger;
        creators["iron assembly set dps priority trigger"] = &RaidUlduarTriggerContext::iron_assembly_set_dps_priority_trigger;
        creators["iron assembly raid position trigger"] = &RaidUlduarTriggerContext::iron_assembly_raid_position_trigger;
        creators["kologarn body tank trigger"] = &RaidUlduarTriggerContext::kologarn_body_tank_trigger;
        creators["kologarn off tank trigger"] = &RaidUlduarTriggerContext::kologarn_off_tank_trigger;
        creators["kologarn rubble tank trigger"] = &RaidUlduarTriggerContext::kologarn_rubble_tank_trigger;
        creators["kologarn dps target trigger"] = &RaidUlduarTriggerContext::kologarn_dps_target_trigger;
        creators["kologarn smash swap trigger"] = &RaidUlduarTriggerContext::kologarn_smash_swap_trigger;
        creators["kologarn body uncovered trigger"] = &RaidUlduarTriggerContext::kologarn_body_uncovered_trigger;
        creators["kologarn fall from floor trigger"] = &RaidUlduarTriggerContext::kologarn_fall_from_floor_trigger;
        creators["kologarn nature resistance trigger"] = &RaidUlduarTriggerContext::kologarn_nature_resistance_trigger;
        creators["kologarn rubble slowdown trigger"] = &RaidUlduarTriggerContext::kologarn_rubble_slowdown_trigger;
        creators["kologarn eyebeam trigger"] = &RaidUlduarTriggerContext::kologarn_eyebeam_trigger;
        creators["auriaya fall from floor trigger"] = &RaidUlduarTriggerContext::auriaya_fall_from_floor_trigger;
        creators["hodir biting cold"] = &RaidUlduarTriggerContext::hodir_biting_cold;
        creators["hodir near snowpacked icicle"] = &RaidUlduarTriggerContext::hodir_near_snowpacked_icicle;
        creators["hodir frost resistance trigger"] = &RaidUlduarTriggerContext::hodir_frost_resistance_trigger;
        creators["hodir spread storm cloud"] = &RaidUlduarTriggerContext::hodir_spread_storm_cloud;
        creators["hodir icicle dodge"] = &RaidUlduarTriggerContext::hodir_icicle_dodge;
        creators["hodir raid position"] = &RaidUlduarTriggerContext::hodir_raid_position;
        creators["hodir set dps priority"] = &RaidUlduarTriggerContext::hodir_set_dps_priority;
        creators["hodir frozen blows swap"] = &RaidUlduarTriggerContext::hodir_frozen_blows_swap;
        creators["hodir redirect threat"] = &RaidUlduarTriggerContext::hodir_redirect_threat;
        creators["freya near nature bomb"] = &RaidUlduarTriggerContext::freya_near_nature_bomb;
        creators["freya tank nature bomb"] = &RaidUlduarTriggerContext::freya_tank_nature_bomb;
        creators["freya fire resistance trigger"] = &RaidUlduarTriggerContext::freya_fire_resistance_trigger;
        creators["freya nature resistance trigger"] = &RaidUlduarTriggerContext::freya_nature_resistance_trigger;
        creators["freya set dps priority"] = &RaidUlduarTriggerContext::freya_set_dps_priority;
        creators["freya tank adds"] = &RaidUlduarTriggerContext::freya_tank_adds;
        creators["freya redirect threat"] = &RaidUlduarTriggerContext::freya_redirect_threat;
        creators["freya move to healing spore trigger"] = &RaidUlduarTriggerContext::freya_move_to_healing_spore_trigger;
        creators["freya break iron roots"] = &RaidUlduarTriggerContext::freya_break_iron_roots;
        creators["freya dodge unstable sun beam"] = &RaidUlduarTriggerContext::freya_dodge_unstable_sun_beam;
        creators["freya lasher about to blow"] = &RaidUlduarTriggerContext::freya_lasher_about_to_blow;
        creators["freya ranged camp"] = &RaidUlduarTriggerContext::freya_ranged_camp;
        creators["freya summon army"] = &RaidUlduarTriggerContext::freya_summon_army;
        creators["freya frost nova lashers"] = &RaidUlduarTriggerContext::freya_frost_nova_lashers;
        creators["freya trap lashers"] = &RaidUlduarTriggerContext::freya_trap_lashers;
        creators["freya ground tremor hold cast"] = &RaidUlduarTriggerContext::freya_ground_tremor_hold_cast;
        creators["freya nature fury bail"] = &RaidUlduarTriggerContext::freya_nature_fury_bail;
        creators["freya step out of sunbeam"] = &RaidUlduarTriggerContext::freya_step_out_of_sunbeam;
        creators["freya tank hold freya"] = &RaidUlduarTriggerContext::freya_tank_hold_freya;
        creators["thorim frost resistance trigger"] = &RaidUlduarTriggerContext::thorim_frost_resistance_trigger;
        creators["thorim nature resistance trigger"] = &RaidUlduarTriggerContext::thorim_nature_resistance_trigger;
        creators["thorim unbalancing strike trigger"] = &RaidUlduarTriggerContext::thorim_unbalancing_strike_trigger;
        creators["thorim dps priority trigger"] = &RaidUlduarTriggerContext::thorim_dps_priority_trigger;
        creators["thorim arena positioning trigger"] = &RaidUlduarTriggerContext::thorim_arena_positioning_trigger;
        creators["thorim gauntlet positioning trigger"] = &RaidUlduarTriggerContext::thorim_gauntlet_positioning_trigger;
        creators["thorim balcony advance trigger"] = &RaidUlduarTriggerContext::thorim_balcony_advance_trigger;
        creators["thorim fall from floor trigger"] = &RaidUlduarTriggerContext::thorim_fall_from_floor_trigger;
        creators["thorim phase 2 positioning trigger"] = &RaidUlduarTriggerContext::thorim_phase2_positioning_trigger;
        creators["mimiron reset encounter state trigger"] = &RaidUlduarTriggerContext::mimiron_reset_encounter_state_trigger;
        creators["mimiron fire resistance trigger"] = &RaidUlduarTriggerContext::mimiron_fire_resistance_trigger;
        creators["mimiron shock blast trigger"] = &RaidUlduarTriggerContext::mimiron_shock_blast_trigger;
        creators["mimiron phase 1 positioning trigger"] = &RaidUlduarTriggerContext::mimiron_phase_1_positioning_trigger;
        creators["mimiron p3wx2 laser barrage trigger"] = &RaidUlduarTriggerContext::mimiron_p3wx2_laser_barrage_trigger;
        creators["mimiron arc spread trigger"] = &RaidUlduarTriggerContext::mimiron_arc_spread_trigger;
        creators["mimiron aerial command unit trigger"] = &RaidUlduarTriggerContext::mimiron_aerial_command_unit_trigger;
        creators["mimiron rocket strike trigger"] = &RaidUlduarTriggerContext::mimiron_rocket_strike_trigger;
        creators["mimiron phase 4 focus trigger"] = &RaidUlduarTriggerContext::mimiron_phase_4_focus_trigger;
        creators["sara shadow resistance trigger"] = &RaidUlduarTriggerContext::sara_shadow_resistance_trigger;
        creators["vezax reset encounter state"] = &RaidUlduarTriggerContext::vezax_reset_encounter_state;
        creators["vezax mark of the faceless"] = &RaidUlduarTriggerContext::vezax_mark_of_the_faceless;
        creators["vezax vapor puddle clear"] = &RaidUlduarTriggerContext::vezax_vapor_puddle_clear;
        creators["vezax shadow crash dodge"] = &RaidUlduarTriggerContext::vezax_shadow_crash_dodge;
        creators["vezax searing flames interrupt"] = &RaidUlduarTriggerContext::vezax_searing_flames_interrupt;
        creators["vezax surge of darkness"] = &RaidUlduarTriggerContext::vezax_surge_of_darkness;
        creators["vezax saronite animus"] = &RaidUlduarTriggerContext::vezax_saronite_animus;
        creators["vezax vapor soak"] = &RaidUlduarTriggerContext::vezax_vapor_soak;
        creators["vezax kill vapor"] = &RaidUlduarTriggerContext::vezax_kill_vapor;
        creators["vezax shadow crash soak"] = &RaidUlduarTriggerContext::vezax_shadow_crash_soak;
        creators["vezax raid position"] = &RaidUlduarTriggerContext::vezax_raid_position;
        creators["vezax shadow resistance"] = &RaidUlduarTriggerContext::vezax_shadow_resistance;
        creators["yogg-saron shadow resistance trigger"] = &RaidUlduarTriggerContext::yogg_saron_shadow_resistance_trigger;
        creators["yogg-saron ominous cloud cheat trigger"] = &RaidUlduarTriggerContext::yogg_saron_ominous_cloud_cheat_trigger;
        creators["yogg-saron guardian positioning trigger"] = &RaidUlduarTriggerContext::yogg_saron_guardian_positioning_trigger;
        creators["yogg-saron sanity trigger"] = &RaidUlduarTriggerContext::yogg_saron_sanity_trigger;
        creators["yogg-saron death orb trigger"] = &RaidUlduarTriggerContext::yogg_saron_death_orb_trigger;
        creators["yogg-saron malady of the mind trigger"] = &RaidUlduarTriggerContext::yogg_saron_malady_of_the_mind_trigger;
        creators["yogg-saron mark target trigger"] = &RaidUlduarTriggerContext::yogg_saron_mark_target_trigger;
        creators["yogg-saron brain link trigger"] = &RaidUlduarTriggerContext::yogg_saron_brain_link_trigger;
        creators["yogg-saron move to enter portal trigger"] = &RaidUlduarTriggerContext::yogg_saron_move_to_enter_portal_trigger;
        creators["yogg-saron use portal trigger"] = &RaidUlduarTriggerContext::yogg_saron_use_portal_trigger;
        creators["yogg-saron fall from floor trigger"] = &RaidUlduarTriggerContext::yogg_saron_fall_from_floor_trigger;
        creators["yogg-saron boss room movement cheat trigger"] = &RaidUlduarTriggerContext::yogg_saron_boss_room_movement_cheat_trigger;
        creators["yogg-saron illusion room trigger"] = &RaidUlduarTriggerContext::yogg_saron_illusion_room_trigger;
        creators["yogg-saron move to exit portal trigger"] = &RaidUlduarTriggerContext::yogg_saron_move_to_exit_portal_trigger;
        creators["yogg-saron lunatic gaze trigger"] = &RaidUlduarTriggerContext::yogg_saron_lunatic_gaze_trigger;
        creators["yogg-saron phase 3 positioning trigger"] = &RaidUlduarTriggerContext::yogg_saron_phase_3_positioning_trigger;
        creators["yogg-saron crusher tentacle trigger"] = &RaidUlduarTriggerContext::yogg_saron_crusher_tentacle_trigger;
        creators["yogg-saron guardian control trigger"] = &RaidUlduarTriggerContext::yogg_saron_guardian_control_trigger;
        creators["yogg-saron sanity conservation trigger"] = &RaidUlduarTriggerContext::yogg_saron_sanity_conservation_trigger;
        creators["yogg-saron squeeze escape trigger"] = &RaidUlduarTriggerContext::yogg_saron_squeeze_escape_trigger;
        creators["algalon reset encounter state"] = &RaidUlduarTriggerContext::algalon_reset_encounter_state;
        creators["algalon big bang hide"] = &RaidUlduarTriggerContext::algalon_big_bang_hide;
        creators["algalon big bang soak"] = &RaidUlduarTriggerContext::algalon_big_bang_soak;
        creators["algalon cosmic smash"] = &RaidUlduarTriggerContext::algalon_cosmic_smash;
        creators["algalon leave black hole"] = &RaidUlduarTriggerContext::algalon_leave_black_hole;
        creators["algalon phase punch swap"] = &RaidUlduarTriggerContext::algalon_phase_punch_swap;
        creators["algalon constellation taunt"] = &RaidUlduarTriggerContext::algalon_constellation_taunt;
        creators["algalon constellation kite"] = &RaidUlduarTriggerContext::algalon_constellation_kite;
        creators["algalon collapsing star focus"] = &RaidUlduarTriggerContext::algalon_collapsing_star_focus;
        creators["algalon dark matter tank"] = &RaidUlduarTriggerContext::algalon_dark_matter_tank;
        creators["algalon dark matter mark"] = &RaidUlduarTriggerContext::algalon_dark_matter_mark;
        creators["algalon raid position"] = &RaidUlduarTriggerContext::algalon_raid_position;
        creators["ignis scorched ground trigger"] = &RaidUlduarTriggerContext::ignis_scorched_ground_trigger;
        creators["ignis main tank position trigger"] = &RaidUlduarTriggerContext::ignis_main_tank_position_trigger;
        creators["ignis construct tank trigger"] = &RaidUlduarTriggerContext::ignis_construct_tank_trigger;
        creators["ignis attack brittle construct trigger"] = &RaidUlduarTriggerContext::ignis_attack_brittle_construct_trigger;
        creators["ignis attack boss trigger"] = &RaidUlduarTriggerContext::ignis_attack_boss_trigger;
        creators["ignis flame jets trigger"] = &RaidUlduarTriggerContext::ignis_flame_jets_trigger;
        creators["ignis molten construct avoid trigger"] = &RaidUlduarTriggerContext::ignis_molten_construct_avoid_trigger;
        creators["ignis slag pot heal trigger"] = &RaidUlduarTriggerContext::ignis_slag_pot_heal_trigger;
        creators["auriaya seeping essence trigger"] = &RaidUlduarTriggerContext::auriaya_seeping_essence_trigger;
        creators["auriaya sentry taunt trigger"] = &RaidUlduarTriggerContext::auriaya_sentry_taunt_trigger;
        creators["auriaya raid position trigger"] = &RaidUlduarTriggerContext::auriaya_raid_position_trigger;
        creators["auriaya set dps priority trigger"] = &RaidUlduarTriggerContext::auriaya_set_dps_priority_trigger;
        creators["auriaya anti fear trigger"] = &RaidUlduarTriggerContext::auriaya_anti_fear_trigger;
        creators["yogg-saron anti fear trigger"] = &RaidUlduarTriggerContext::yogg_saron_anti_fear_trigger;
        creators["mimiron magnetic core trigger"] = &RaidUlduarTriggerContext::mimiron_magnetic_core_trigger;
        creators["mimiron plasma blast trigger"] = &RaidUlduarTriggerContext::mimiron_plasma_blast_trigger;
        creators["mimiron set dps priority trigger"] = &RaidUlduarTriggerContext::mimiron_set_dps_priority_trigger;
        creators["mimiron proximity mine trigger"] = &RaidUlduarTriggerContext::mimiron_proximity_mine_trigger;
        creators["mimiron bomb bot trigger"] = &RaidUlduarTriggerContext::mimiron_bomb_bot_trigger;
        creators["mimiron pet control trigger"] = &RaidUlduarTriggerContext::mimiron_pet_control_trigger;
        creators["mimiron slow bomb bot trigger"] = &RaidUlduarTriggerContext::mimiron_slow_bomb_bot_trigger;
        creators["thorim unbalancing strike swap trigger"] = &RaidUlduarTriggerContext::thorim_unbalancing_strike_swap_trigger;
        creators["thorim tank pickup trigger"] = &RaidUlduarTriggerContext::thorim_tank_pickup_trigger;
        creators["thorim sif blizzard trigger"] = &RaidUlduarTriggerContext::thorim_sif_blizzard_trigger;
        creators["thorim sif frost nova trigger"] = &RaidUlduarTriggerContext::thorim_sif_frost_nova_trigger;
        creators["thorim runic smash trigger"] = &RaidUlduarTriggerContext::thorim_runic_smash_trigger;
        creators["thorim runic barrier bail trigger"] = &RaidUlduarTriggerContext::thorim_runic_barrier_bail_trigger;
        creators["thorim lightning charge trigger"] = &RaidUlduarTriggerContext::thorim_lightning_charge_trigger;
        creators["thorim reset encounter state trigger"] = &RaidUlduarTriggerContext::thorim_reset_encounter_state_trigger;
        creators["thorim charged orb trigger"] = &RaidUlduarTriggerContext::thorim_charged_orb_trigger;
        creators["thorim pet leash trigger"] = &RaidUlduarTriggerContext::thorim_pet_leash_trigger;
        creators["thorim arena leash trigger"] = &RaidUlduarTriggerContext::thorim_arena_leash_trigger;
        creators["mimiron dodge flames trigger"] = &RaidUlduarTriggerContext::mimiron_dodge_flames_trigger;
        creators["mimiron frost bomb trigger"] = &RaidUlduarTriggerContext::mimiron_frost_bomb_trigger;
        creators["xt002 debuff carrier trigger"] = &RaidUlduarTriggerContext::xt002_debuff_carrier_trigger;
        creators["xt002 avoid hazard trigger"] = &RaidUlduarTriggerContext::xt002_avoid_hazard_trigger;
        creators["xt002 raid position trigger"] = &RaidUlduarTriggerContext::xt002_raid_position_trigger;
        creators["xt002 set dps priority trigger"] = &RaidUlduarTriggerContext::xt002_set_dps_priority_trigger;
        creators["xt002 pummeller taunt trigger"] = &RaidUlduarTriggerContext::xt002_pummeller_taunt_trigger;
        creators["xt002 redirect threat trigger"] = &RaidUlduarTriggerContext::xt002_redirect_threat_trigger;

        // Applied over the whole table rather than in 165 trigger classes: every name here carries its
        // encounter, so one pass can gate them all. UldEncounterGate.h has what it closes and why.
        for (auto& entry : creators)
        {
            uint32 bossId = 0;
            if (!UldEncounterOfTrigger(entry.first, bossId))
                continue;

            ObjectCreator inner = entry.second;
            entry.second = [inner, bossId](PlayerbotAI* ai) -> Trigger*
            { return new UldGatedTrigger(ai, inner(ai), bossId); };
        }
    }

private:
    static Trigger* flame_leviathan_on_vehicle(PlayerbotAI* ai) { return new FlameLeviathanOnVehicleTrigger(ai); }
    static Trigger* flame_leviathan_vehicle_near(PlayerbotAI* ai) { return new FlameLeviathanVehicleNearTrigger(ai); }
    static Trigger* flame_leviathan_flame_vents(PlayerbotAI* ai) { return new FlameLeviathanFlameVentsTrigger(ai); }
    static Trigger* flame_leviathan_drive_urgent(PlayerbotAI* ai) { return new FlameLeviathanDriveUrgentTrigger(ai); }
    static Trigger* razorscale_flying_alone(PlayerbotAI* ai) { return new RazorscaleFlyingAloneTrigger(ai); }
    static Trigger* razorscale_avoid_devouring_flames(PlayerbotAI* ai) { return new RazorscaleDevouringFlamesTrigger(ai); }
    static Trigger* razorscale_avoid_sentinel(PlayerbotAI* ai) { return new RazorscaleAvoidSentinelTrigger(ai); }
    static Trigger* razorscale_avoid_whirlwind(PlayerbotAI* ai) { return new RazorscaleAvoidWhirlwindTrigger(ai); }
    static Trigger* razorscale_grounded(PlayerbotAI* ai) { return new RazorscaleGroundedTrigger(ai); }
    static Trigger* razorscale_harpoon_trigger(PlayerbotAI* ai) { return new RazorscaleHarpoonAvailableTrigger(ai); }
    static Trigger* razorscale_fuse_armor_trigger(PlayerbotAI* ai) { return new RazorscaleFuseArmorTrigger(ai); }
    static Trigger* razorscale_kill_target_trigger(PlayerbotAI* ai) { return new RazorscaleKillTargetTrigger(ai); }
    static Trigger* razorscale_pet_control_trigger(PlayerbotAI* ai) { return new RazorscalePetControlTrigger(ai); }
    static Trigger* razorscale_flame_breath_trigger(PlayerbotAI* ai) { return new RazorscaleFlameBreathTrigger(ai); }
    static Trigger* razorscale_fire_resistance_trigger(PlayerbotAI* ai) { return new BossFireResistanceTrigger(ai, "razorscale"); }
    static Trigger* ignis_fire_resistance_trigger(PlayerbotAI* ai) { return new BossFireResistanceTrigger(ai, "ignis the furnace master"); }
    static Trigger* iron_assembly_reset_encounter_state_trigger(PlayerbotAI* ai) { return new IronAssemblyResetEncounterStateTrigger(ai); }
    static Trigger* iron_assembly_overwhelming_power_run_out_trigger(PlayerbotAI* ai) { return new IronAssemblyOverwhelmingPowerRunOutTrigger(ai); }
    static Trigger* iron_assembly_lightning_tendrils_trigger(PlayerbotAI* ai) { return new IronAssemblyLightningTendrilsTrigger(ai); }
    static Trigger* iron_assembly_overload_trigger(PlayerbotAI* ai) { return new IronAssemblyOverloadTrigger(ai); }
    static Trigger* iron_assembly_rune_of_death_trigger(PlayerbotAI* ai) { return new IronAssemblyRuneOfDeathTrigger(ai); }
    static Trigger* iron_assembly_interrupt_trigger(PlayerbotAI* ai) { return new IronAssemblyInterruptTrigger(ai); }
    static Trigger* iron_assembly_tank_assignment_trigger(PlayerbotAI* ai) { return new IronAssemblyTankAssignmentTrigger(ai); }
    static Trigger* iron_assembly_overwhelming_power_swap_trigger(PlayerbotAI* ai) { return new IronAssemblyOverwhelmingPowerSwapTrigger(ai); }
    static Trigger* iron_assembly_shield_of_runes_trigger(PlayerbotAI* ai) { return new IronAssemblyShieldOfRunesTrigger(ai); }
    static Trigger* iron_assembly_fusion_punch_dispel_trigger(PlayerbotAI* ai) { return new IronAssemblyFusionPunchDispelTrigger(ai); }
    static Trigger* iron_assembly_redirect_threat_trigger(PlayerbotAI* ai) { return new IronAssemblyRedirectThreatTrigger(ai); }
    static Trigger* iron_assembly_rune_of_power_trigger(PlayerbotAI* ai) { return new IronAssemblyRuneOfPowerTrigger(ai); }
    static Trigger* iron_assembly_rune_of_power_soak_trigger(PlayerbotAI* ai) { return new IronAssemblyRuneOfPowerSoakTrigger(ai); }
    static Trigger* iron_assembly_set_dps_priority_trigger(PlayerbotAI* ai) { return new IronAssemblySetDpsPriorityTrigger(ai); }
    static Trigger* iron_assembly_raid_position_trigger(PlayerbotAI* ai) { return new IronAssemblyRaidPositionTrigger(ai); }
    static Trigger* kologarn_body_tank_trigger(PlayerbotAI* ai) { return new KologarnBodyTankTrigger(ai); }
    static Trigger* kologarn_off_tank_trigger(PlayerbotAI* ai) { return new KologarnOffTankTrigger(ai); }
    static Trigger* kologarn_rubble_tank_trigger(PlayerbotAI* ai) { return new KologarnRubbleTankTrigger(ai); }
    static Trigger* kologarn_dps_target_trigger(PlayerbotAI* ai) { return new KologarnDpsTargetTrigger(ai); }
    static Trigger* kologarn_smash_swap_trigger(PlayerbotAI* ai) { return new KologarnSmashSwapTrigger(ai); }
    static Trigger* kologarn_body_uncovered_trigger(PlayerbotAI* ai) { return new KologarnBodyUncoveredTrigger(ai); }
    static Trigger* kologarn_fall_from_floor_trigger(PlayerbotAI* ai) { return new KologarnFallFromFloorTrigger(ai); }
    static Trigger* kologarn_nature_resistance_trigger(PlayerbotAI* ai) { return new BossNatureResistanceTrigger(ai, "kologarn"); }
    static Trigger* kologarn_rubble_slowdown_trigger(PlayerbotAI* ai) { return new KologarnRubbleSlowdownTrigger(ai); }
    static Trigger* kologarn_eyebeam_trigger(PlayerbotAI* ai) { return new KologarnEyebeamTrigger(ai); }
    static Trigger* auriaya_fall_from_floor_trigger(PlayerbotAI* ai) { return new AuriayaFallFromFloorTrigger(ai); }
    static Trigger* hodir_biting_cold(PlayerbotAI* ai) { return new HodirBitingColdTrigger(ai); }
    static Trigger* hodir_near_snowpacked_icicle(PlayerbotAI* ai) { return new HodirNearSnowpackedIcicleTrigger(ai); }
    static Trigger* hodir_frost_resistance_trigger(PlayerbotAI* ai) { return new HodirFrostResistanceTrigger(ai); }
    static Trigger* hodir_spread_storm_cloud(PlayerbotAI* ai) { return new HodirSpreadStormCloudTrigger(ai); }
    static Trigger* hodir_icicle_dodge(PlayerbotAI* ai) { return new HodirIcicleDodgeTrigger(ai); }
    static Trigger* hodir_raid_position(PlayerbotAI* ai) { return new HodirRaidPositionTrigger(ai); }
    static Trigger* hodir_set_dps_priority(PlayerbotAI* ai) { return new HodirSetDpsPriorityTrigger(ai); }
    static Trigger* hodir_frozen_blows_swap(PlayerbotAI* ai) { return new HodirFrozenBlowsSwapTrigger(ai); }
    static Trigger* hodir_redirect_threat(PlayerbotAI* ai) { return new HodirRedirectThreatTrigger(ai); }
    static Trigger* freya_near_nature_bomb(PlayerbotAI* ai) { return new FreyaNearNatureBombTrigger(ai); }
    static Trigger* freya_tank_nature_bomb(PlayerbotAI* ai) { return new FreyaTankNatureBombTrigger(ai); }
    static Trigger* freya_fire_resistance_trigger(PlayerbotAI* ai) { return new BossFireResistanceTrigger(ai, "freya"); }
    static Trigger* freya_nature_resistance_trigger(PlayerbotAI* ai) { return new BossNatureResistanceTrigger(ai, "freya"); }
    static Trigger* freya_set_dps_priority(PlayerbotAI* ai) { return new FreyaSetDpsPriorityTrigger(ai); }
    static Trigger* freya_tank_adds(PlayerbotAI* ai) { return new FreyaTankAddsTrigger(ai); }
    static Trigger* freya_redirect_threat(PlayerbotAI* ai) { return new FreyaRedirectThreatTrigger(ai); }
    static Trigger* freya_move_to_healing_spore_trigger(PlayerbotAI* ai) { return new FreyaMoveToHealingSporeTrigger(ai); }
    static Trigger* freya_break_iron_roots(PlayerbotAI* ai) { return new FreyaBreakIronRootsTrigger(ai); }
    static Trigger* freya_dodge_unstable_sun_beam(PlayerbotAI* ai) { return new FreyaDodgeUnstableSunBeamTrigger(ai); }
    static Trigger* freya_lasher_about_to_blow(PlayerbotAI* ai) { return new FreyaLasherAboutToBlowTrigger(ai); }
    static Trigger* freya_ranged_camp(PlayerbotAI* ai) { return new FreyaRangedCampTrigger(ai); }
    static Trigger* freya_summon_army(PlayerbotAI* ai) { return new FreyaSummonArmyTrigger(ai); }
    static Trigger* freya_frost_nova_lashers(PlayerbotAI* ai) { return new FreyaFrostNovaLashersTrigger(ai); }
    static Trigger* freya_trap_lashers(PlayerbotAI* ai) { return new FreyaTrapLashersTrigger(ai); }
    static Trigger* freya_ground_tremor_hold_cast(PlayerbotAI* ai) { return new FreyaGroundTremorHoldCastTrigger(ai); }
    static Trigger* freya_nature_fury_bail(PlayerbotAI* ai) { return new FreyaNaturesFuryBailTrigger(ai); }
    static Trigger* freya_step_out_of_sunbeam(PlayerbotAI* ai) { return new FreyaStepOutOfSunbeamTrigger(ai); }
    static Trigger* freya_tank_hold_freya(PlayerbotAI* ai) { return new FreyaTankHoldFreyaTrigger(ai); }
    static Trigger* thorim_frost_resistance_trigger(PlayerbotAI* ai) { return new BossFrostResistanceTrigger(ai, "thorim"); }
    static Trigger* thorim_nature_resistance_trigger(PlayerbotAI* ai) { return new BossNatureResistanceTrigger(ai, "thorim"); }
    static Trigger* thorim_unbalancing_strike_trigger(PlayerbotAI* ai) { return new ThorimUnbalancingStrikeTrigger(ai); }
    static Trigger* thorim_dps_priority_trigger(PlayerbotAI* ai) { return new ThorimDpsPriorityTrigger(ai); }
    static Trigger* thorim_arena_positioning_trigger(PlayerbotAI* ai) { return new ThorimArenaPositioningTrigger(ai); }
    static Trigger* thorim_gauntlet_positioning_trigger(PlayerbotAI* ai) { return new ThorimGauntletPositioningTrigger(ai); }
    static Trigger* thorim_balcony_advance_trigger(PlayerbotAI* ai) { return new ThorimBalconyAdvanceTrigger(ai); }
    static Trigger* thorim_fall_from_floor_trigger(PlayerbotAI* ai) { return new ThorimFallFromFloorTrigger(ai); }
    static Trigger* thorim_phase2_positioning_trigger(PlayerbotAI* ai) { return new ThorimPhase2PositioningTrigger(ai); }
    static Trigger* mimiron_fire_resistance_trigger(PlayerbotAI* ai) { return new BossFireResistanceTrigger(ai, "mimiron"); }
    static Trigger* mimiron_shock_blast_trigger(PlayerbotAI* ai) { return new MimironShockBlastTrigger(ai); }
    static Trigger* mimiron_reset_encounter_state_trigger(PlayerbotAI* ai) { return new MimironResetEncounterStateTrigger(ai); }
    static Trigger* mimiron_phase_1_positioning_trigger(PlayerbotAI* ai) { return new MimironPhase1PositioningTrigger(ai); }
    static Trigger* mimiron_p3wx2_laser_barrage_trigger(PlayerbotAI* ai) { return new MimironP3Wx2LaserBarrageTrigger(ai); }
    static Trigger* mimiron_arc_spread_trigger(PlayerbotAI* ai) { return new MimironArcSpreadTrigger(ai); }
    static Trigger* mimiron_aerial_command_unit_trigger(PlayerbotAI* ai) { return new MimironAerialCommandUnitTrigger(ai); }
    static Trigger* mimiron_rocket_strike_trigger(PlayerbotAI* ai) { return new MimironRocketStrikeTrigger(ai); }
    static Trigger* mimiron_phase_4_focus_trigger(PlayerbotAI* ai) { return new MimironPhase4FocusTrigger(ai); }
    static Trigger* sara_shadow_resistance_trigger(PlayerbotAI* ai) { return new BossShadowResistanceTrigger(ai, "sara"); }
    static Trigger* vezax_reset_encounter_state(PlayerbotAI* ai) { return new VezaxResetEncounterStateTrigger(ai); }
    static Trigger* vezax_mark_of_the_faceless(PlayerbotAI* ai) { return new VezaxMarkOfTheFacelessTrigger(ai); }
    static Trigger* vezax_vapor_puddle_clear(PlayerbotAI* ai) { return new VezaxVaporPuddleClearTrigger(ai); }
    static Trigger* vezax_shadow_crash_dodge(PlayerbotAI* ai) { return new VezaxShadowCrashDodgeTrigger(ai); }
    static Trigger* vezax_searing_flames_interrupt(PlayerbotAI* ai) { return new VezaxSearingFlamesInterruptTrigger(ai); }
    static Trigger* vezax_surge_of_darkness(PlayerbotAI* ai) { return new VezaxSurgeOfDarknessTrigger(ai); }
    static Trigger* vezax_saronite_animus(PlayerbotAI* ai) { return new VezaxSaroniteAnimusTrigger(ai); }
    static Trigger* vezax_vapor_soak(PlayerbotAI* ai) { return new VezaxVaporSoakTrigger(ai); }
    static Trigger* vezax_kill_vapor(PlayerbotAI* ai) { return new VezaxKillVaporTrigger(ai); }
    static Trigger* vezax_shadow_crash_soak(PlayerbotAI* ai) { return new VezaxShadowCrashSoakTrigger(ai); }
    static Trigger* vezax_raid_position(PlayerbotAI* ai) { return new VezaxRaidPositionTrigger(ai); }
    static Trigger* vezax_shadow_resistance(PlayerbotAI* ai) { return new BossShadowResistanceTrigger(ai, "general vezax"); }
    static Trigger* yogg_saron_shadow_resistance_trigger(PlayerbotAI* ai) { return new BossShadowResistanceTrigger(ai, "yogg-saron"); }
    static Trigger* yogg_saron_ominous_cloud_cheat_trigger(PlayerbotAI* ai) { return new YoggSaronOminousCloudCheatTrigger(ai); }
    static Trigger* yogg_saron_guardian_positioning_trigger(PlayerbotAI* ai) { return new YoggSaronGuardianPositioningTrigger(ai); }
    static Trigger* yogg_saron_sanity_trigger(PlayerbotAI* ai) { return new YoggSaronSanityTrigger(ai); }
    static Trigger* yogg_saron_death_orb_trigger(PlayerbotAI* ai) { return new YoggSaronDeathOrbTrigger(ai); }
    static Trigger* yogg_saron_malady_of_the_mind_trigger(PlayerbotAI* ai) { return new YoggSaronMaladyOfTheMindTrigger(ai); }
    static Trigger* yogg_saron_mark_target_trigger(PlayerbotAI* ai) { return new YoggSaronMarkTargetTrigger(ai); }
    static Trigger* yogg_saron_brain_link_trigger(PlayerbotAI* ai) { return new YoggSaronBrainLinkTrigger(ai); }
    static Trigger* yogg_saron_move_to_enter_portal_trigger(PlayerbotAI* ai) { return new YoggSaronMoveToEnterPortalTrigger(ai); }
    static Trigger* yogg_saron_use_portal_trigger(PlayerbotAI* ai) { return new YoggSaronUsePortalTrigger(ai); }
    static Trigger* yogg_saron_fall_from_floor_trigger(PlayerbotAI* ai) { return new YoggSaronFallFromFloorTrigger(ai); }
    static Trigger* yogg_saron_boss_room_movement_cheat_trigger(PlayerbotAI* ai) { return new YoggSaronBossRoomMovementCheatTrigger(ai); }
    static Trigger* yogg_saron_illusion_room_trigger(PlayerbotAI* ai) { return new YoggSaronIllusionRoomTrigger(ai); }
    static Trigger* yogg_saron_move_to_exit_portal_trigger(PlayerbotAI* ai) { return new YoggSaronMoveToExitPortalTrigger(ai); }
    static Trigger* yogg_saron_lunatic_gaze_trigger(PlayerbotAI* ai) { return new YoggSaronLunaticGazeTrigger(ai); }
    static Trigger* yogg_saron_phase_3_positioning_trigger(PlayerbotAI* ai) { return new YoggSaronPhase3PositioningTrigger(ai); }
    static Trigger* yogg_saron_crusher_tentacle_trigger(PlayerbotAI* ai) { return new YoggSaronCrusherTentacleTrigger(ai); }
    static Trigger* yogg_saron_guardian_control_trigger(PlayerbotAI* ai) { return new YoggSaronGuardianControlTrigger(ai); }
    static Trigger* yogg_saron_sanity_conservation_trigger(PlayerbotAI* ai) { return new YoggSaronSanityConservationTrigger(ai); }
    static Trigger* yogg_saron_squeeze_escape_trigger(PlayerbotAI* ai) { return new YoggSaronSqueezeEscapeTrigger(ai); }
    static Trigger* algalon_reset_encounter_state(PlayerbotAI* ai) { return new AlgalonResetEncounterStateTrigger(ai); }
    static Trigger* algalon_big_bang_hide(PlayerbotAI* ai) { return new AlgalonBigBangHideTrigger(ai); }
    static Trigger* algalon_big_bang_soak(PlayerbotAI* ai) { return new AlgalonBigBangSoakTrigger(ai); }
    static Trigger* algalon_cosmic_smash(PlayerbotAI* ai) { return new AlgalonCosmicSmashTrigger(ai); }
    static Trigger* algalon_leave_black_hole(PlayerbotAI* ai) { return new AlgalonLeaveBlackHoleTrigger(ai); }
    static Trigger* algalon_phase_punch_swap(PlayerbotAI* ai) { return new AlgalonPhasePunchSwapTrigger(ai); }
    static Trigger* algalon_constellation_taunt(PlayerbotAI* ai) { return new AlgalonConstellationTauntTrigger(ai); }
    static Trigger* algalon_constellation_kite(PlayerbotAI* ai) { return new AlgalonConstellationKiteTrigger(ai); }
    static Trigger* algalon_collapsing_star_focus(PlayerbotAI* ai) { return new AlgalonCollapsingStarFocusTrigger(ai); }
    static Trigger* algalon_dark_matter_tank(PlayerbotAI* ai) { return new AlgalonDarkMatterTankTrigger(ai); }
    static Trigger* algalon_dark_matter_mark(PlayerbotAI* ai) { return new AlgalonDarkMatterMarkTrigger(ai); }
    static Trigger* algalon_raid_position(PlayerbotAI* ai) { return new AlgalonRaidPositionTrigger(ai); }
    static Trigger* ignis_scorched_ground_trigger(PlayerbotAI* ai) { return new IgnisScorchedGroundTrigger(ai); }
    static Trigger* ignis_main_tank_position_trigger(PlayerbotAI* ai) { return new IgnisMainTankPositionTrigger(ai); }
    static Trigger* ignis_construct_tank_trigger(PlayerbotAI* ai) { return new IgnisConstructTankTrigger(ai); }
    static Trigger* ignis_attack_brittle_construct_trigger(PlayerbotAI* ai) { return new IgnisAttackBrittleConstructTrigger(ai); }
    static Trigger* ignis_attack_boss_trigger(PlayerbotAI* ai) { return new IgnisAttackBossTrigger(ai); }
    static Trigger* ignis_flame_jets_trigger(PlayerbotAI* ai) { return new IgnisFlameJetsTrigger(ai); }
    static Trigger* ignis_molten_construct_avoid_trigger(PlayerbotAI* ai) { return new IgnisMoltenConstructAvoidTrigger(ai); }
    static Trigger* ignis_slag_pot_heal_trigger(PlayerbotAI* ai) { return new IgnisSlagPotHealTrigger(ai); }
    static Trigger* auriaya_seeping_essence_trigger(PlayerbotAI* ai) { return new AuriayaSeepingEssenceTrigger(ai); }
    static Trigger* auriaya_sentry_taunt_trigger(PlayerbotAI* ai) { return new AuriayaSentryTauntTrigger(ai); }
    static Trigger* auriaya_raid_position_trigger(PlayerbotAI* ai) { return new AuriayaRaidPositionTrigger(ai); }
    static Trigger* auriaya_set_dps_priority_trigger(PlayerbotAI* ai) { return new AuriayaSetDpsPriorityTrigger(ai); }
    static Trigger* auriaya_anti_fear_trigger(PlayerbotAI* ai) { return new AuriayaAntiFearTrigger(ai); }
    static Trigger* yogg_saron_anti_fear_trigger(PlayerbotAI* ai) { return new YoggSaronAntiFearTrigger(ai); }
    static Trigger* mimiron_magnetic_core_trigger(PlayerbotAI* ai) { return new MimironMagneticCoreTrigger(ai); }
    static Trigger* mimiron_plasma_blast_trigger(PlayerbotAI* ai) { return new MimironPlasmaBlastTrigger(ai); }
    static Trigger* mimiron_set_dps_priority_trigger(PlayerbotAI* ai) { return new MimironSetDpsPriorityTrigger(ai); }
    static Trigger* mimiron_proximity_mine_trigger(PlayerbotAI* ai) { return new MimironProximityMineTrigger(ai); }
    static Trigger* mimiron_bomb_bot_trigger(PlayerbotAI* ai) { return new MimironBombBotTrigger(ai); }
    static Trigger* mimiron_pet_control_trigger(PlayerbotAI* ai) { return new MimironPetControlTrigger(ai); }
    static Trigger* mimiron_slow_bomb_bot_trigger(PlayerbotAI* ai) { return new MimironSlowBombBotTrigger(ai); }
    static Trigger* thorim_unbalancing_strike_swap_trigger(PlayerbotAI* ai) { return new ThorimUnbalancingStrikeSwapTrigger(ai); }
    static Trigger* thorim_tank_pickup_trigger(PlayerbotAI* ai) { return new ThorimTankPickupTrigger(ai); }
    static Trigger* thorim_sif_blizzard_trigger(PlayerbotAI* ai) { return new ThorimSifBlizzardTrigger(ai); }
    static Trigger* thorim_sif_frost_nova_trigger(PlayerbotAI* ai) { return new ThorimSifFrostNovaTrigger(ai); }
    static Trigger* thorim_runic_smash_trigger(PlayerbotAI* ai) { return new ThorimRunicSmashTrigger(ai); }
    static Trigger* thorim_runic_barrier_bail_trigger(PlayerbotAI* ai) { return new ThorimRunicBarrierBailTrigger(ai); }
    static Trigger* thorim_lightning_charge_trigger(PlayerbotAI* ai) { return new ThorimLightningChargeTrigger(ai); }
    static Trigger* thorim_reset_encounter_state_trigger(PlayerbotAI* ai) { return new ThorimResetEncounterStateTrigger(ai); }
    static Trigger* thorim_charged_orb_trigger(PlayerbotAI* ai) { return new ThorimChargedOrbTrigger(ai); }
    static Trigger* thorim_pet_leash_trigger(PlayerbotAI* ai) { return new ThorimPetLeashTrigger(ai); }
    static Trigger* thorim_arena_leash_trigger(PlayerbotAI* ai) { return new ThorimArenaLeashTrigger(ai); }
    static Trigger* mimiron_dodge_flames_trigger(PlayerbotAI* ai) { return new MimironDodgeFlamesTrigger(ai); }
    static Trigger* mimiron_frost_bomb_trigger(PlayerbotAI* ai) { return new MimironFrostBombTrigger(ai); }
    static Trigger* xt002_debuff_carrier_trigger(PlayerbotAI* ai) { return new XT002DebuffCarrierTrigger(ai); }
    static Trigger* xt002_avoid_hazard_trigger(PlayerbotAI* ai) { return new XT002AvoidHazardTrigger(ai); }
    static Trigger* xt002_raid_position_trigger(PlayerbotAI* ai) { return new XT002RaidPositionTrigger(ai); }
    static Trigger* xt002_set_dps_priority_trigger(PlayerbotAI* ai) { return new XT002SetDpsPriorityTrigger(ai); }
    static Trigger* xt002_pummeller_taunt_trigger(PlayerbotAI* ai) { return new XT002PummellerTauntTrigger(ai); }
    static Trigger* xt002_redirect_threat_trigger(PlayerbotAI* ai) { return new XT002RedirectThreatTrigger(ai); }
};

#endif
