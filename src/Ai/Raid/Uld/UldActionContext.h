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

class RaidUlduarActionContext : public NamedObjectContext<Action>
{
public:
    RaidUlduarActionContext()
    {
        creators["flame leviathan vehicle"] = &RaidUlduarActionContext::flame_leviathan_vehicle;
        creators["flame leviathan enter vehicle"] = &RaidUlduarActionContext::flame_leviathan_enter_vehicle;
        creators["flame leviathan drive"] = &RaidUlduarActionContext::flame_leviathan_drive;
        creators["flame leviathan interrupt vents"] = &RaidUlduarActionContext::flame_leviathan_interrupt_vents;
        creators["razorscale avoid devouring flames"] = &RaidUlduarActionContext::razorscale_avoid_devouring_flames;
        creators["razorscale avoid sentinel"] = &RaidUlduarActionContext::razorscale_avoid_sentinel;
        creators["razorscale ignore flying alone"] = &RaidUlduarActionContext::razorscale_ignore_flying_alone;
        creators["razorscale avoid whirlwind"] = &RaidUlduarActionContext::razorscale_avoid_whirlwind;
        creators["razorscale grounded"] = &RaidUlduarActionContext::razorscale_grounded;
        creators["razorscale harpoon action"] = &RaidUlduarActionContext::razorscale_harpoon_action;
        creators["razorscale fuse armor action"] = &RaidUlduarActionContext::razorscale_fuse_armor_action;
        creators["razorscale kill target action"] = &RaidUlduarActionContext::razorscale_kill_target_action;
        creators["razorscale pet control action"] = &RaidUlduarActionContext::razorscale_pet_control_action;
        creators["razorscale flame breath action"] = &RaidUlduarActionContext::razorscale_flame_breath_action;
        creators["razorscale fire resistance action"] = &RaidUlduarActionContext::razorscale_fire_resistance_action;
        creators["ignis fire resistance action"] = &RaidUlduarActionContext::ignis_fire_resistance_action;
        creators["iron assembly lightning tendrils action"] = &RaidUlduarActionContext::iron_assembly_lightning_tendrils_action;
        creators["iron assembly overload action"] = &RaidUlduarActionContext::iron_assembly_overload_action;
        creators["iron assembly rune of power action"] = &RaidUlduarActionContext::iron_assembly_rune_of_power_action;
        creators["iron assembly kill order action"] = &RaidUlduarActionContext::iron_assembly_kill_order_action;
        creators["iron assembly fusion punch swap action"] = &RaidUlduarActionContext::iron_assembly_fusion_punch_swap_action;
        creators["kologarn body tank action"] = &RaidUlduarActionContext::kologarn_body_tank_action;
        creators["kologarn off tank action"] = &RaidUlduarActionContext::kologarn_off_tank_action;
        creators["kologarn rubble tank action"] = &RaidUlduarActionContext::kologarn_rubble_tank_action;
        creators["kologarn dps target action"] = &RaidUlduarActionContext::kologarn_dps_target_action;
        creators["kologarn smash swap action"] = &RaidUlduarActionContext::kologarn_smash_swap_action;
        creators["kologarn body uncovered action"] = &RaidUlduarActionContext::kologarn_body_uncovered_action;
        creators["kologarn fall from floor action"] = &RaidUlduarActionContext::kologarn_fall_from_floor_action;
        creators["kologarn nature resistance action"] = &RaidUlduarActionContext::kologarn_nature_resistance_action;
        creators["kologarn rubble slowdown action"] = &RaidUlduarActionContext::kologarn_rubble_slowdown_action;
        creators["kologarn eyebeam action"] = &RaidUlduarActionContext::kologarn_eyebeam_action;
        creators["auriaya fall from floor action"] = &RaidUlduarActionContext::auriaya_fall_from_floor_action;
        creators["hodir move snowpacked icicle"] = &RaidUlduarActionContext::hodir_move_snowpacked_icicle;
        creators["hodir biting cold jump"] = &RaidUlduarActionContext::hodir_biting_cold_jump;
        creators["hodir frost resistance action"] = &RaidUlduarActionContext::hodir_frost_resistance_action;
        creators["hodir spread storm cloud"] = &RaidUlduarActionContext::hodir_spread_storm_cloud;
        creators["hodir icicle dodge action"] = &RaidUlduarActionContext::hodir_icicle_dodge_action;
        creators["hodir raid position action"] = &RaidUlduarActionContext::hodir_raid_position_action;
        creators["hodir set dps priority action"] = &RaidUlduarActionContext::hodir_set_dps_priority_action;
        creators["hodir frozen blows swap action"] = &RaidUlduarActionContext::hodir_frozen_blows_swap_action;
        creators["freya move away nature bomb"] = &RaidUlduarActionContext::freya_move_away_nature_bomb;
        creators["freya fire resistance action"] = &RaidUlduarActionContext::freya_fire_resistance_action;
        creators["freya nature resistance action"] = &RaidUlduarActionContext::freya_nature_resistance_action;
        creators["freya set dps priority"] = &RaidUlduarActionContext::freya_set_dps_priority;
        creators["freya tank adds"] = &RaidUlduarActionContext::freya_tank_adds;
        creators["freya redirect threat"] = &RaidUlduarActionContext::freya_redirect_threat;
        creators["freya avoid detonating lasher"] = &RaidUlduarActionContext::freya_avoid_detonating_lasher;
        creators["freya move to healing spore action"] = &RaidUlduarActionContext::freya_move_to_healing_spore_action;
        creators["freya break iron roots"] = &RaidUlduarActionContext::freya_break_iron_roots;
        creators["freya dodge unstable sun beam"] = &RaidUlduarActionContext::freya_dodge_unstable_sun_beam;
        creators["thorim frost resistance action"] = &RaidUlduarActionContext::thorim_frost_resistance_action;
        creators["thorim nature resistance action"] = &RaidUlduarActionContext::thorim_nature_resistance_action;
        creators["thorim unbalancing strike action"] = &RaidUlduarActionContext::thorim_unbalancing_strike_action;
        creators["thorim mark dps target action"] = &RaidUlduarActionContext::thorim_mark_dps_target_action;
        creators["thorim arena positioning action"] = &RaidUlduarActionContext::thorim_arena_positioning_action;
        creators["thorim gauntlet positioning action"] = &RaidUlduarActionContext::thorim_gauntlet_positioning_action;
        creators["thorim phase 2 positioning action"] = &RaidUlduarActionContext::thorim_phase2_positioning_action;
        creators["mimiron fire resistance action"] = &RaidUlduarActionContext::mimiron_fire_resistance_action;
        creators["mimiron shock blast action"] = &RaidUlduarActionContext::mimiron_shock_blast_action;
        creators["mimiron phase 1 positioning action"] = &RaidUlduarActionContext::mimiron_phase_1_positioning_action;
        creators["mimiron p3wx2 laser barrage action"] = &RaidUlduarActionContext::mimiron_p3wx2_laser_barrage_action;
        creators["mimiron arc spread action"] = &RaidUlduarActionContext::mimiron_arc_spread_action;
        creators["mimiron aerial command unit action"] = &RaidUlduarActionContext::mimiron_aerial_command_unit_action;
        creators["mimiron rocket strike action"] = &RaidUlduarActionContext::mimiron_rocket_strike_action;
        creators["mimiron phase 4 focus action"] = &RaidUlduarActionContext::mimiron_phase_4_focus_action;
        creators["sara shadow resistance action"] = &RaidUlduarActionContext::sara_shadow_resistance_action;
        creators["vezax reset encounter state action"] = &RaidUlduarActionContext::vezax_reset_encounter_state_action;
        creators["vezax mark of the faceless action"] = &RaidUlduarActionContext::vezax_mark_of_the_faceless_action;
        creators["vezax vapor puddle clear action"] = &RaidUlduarActionContext::vezax_vapor_puddle_clear_action;
        creators["vezax shadow crash clear action"] = &RaidUlduarActionContext::vezax_shadow_crash_clear_action;
        creators["vezax searing flames interrupt action"] = &RaidUlduarActionContext::vezax_searing_flames_interrupt_action;
        creators["vezax surge of darkness action"] = &RaidUlduarActionContext::vezax_surge_of_darkness_action;
        creators["vezax saronite animus action"] = &RaidUlduarActionContext::vezax_saronite_animus_action;
        creators["vezax vapor soak action"] = &RaidUlduarActionContext::vezax_vapor_soak_action;
        creators["vezax kill vapor action"] = &RaidUlduarActionContext::vezax_kill_vapor_action;
        creators["vezax shadow crash soak action"] = &RaidUlduarActionContext::vezax_shadow_crash_soak_action;
        creators["vezax raid position action"] = &RaidUlduarActionContext::vezax_raid_position_action;
        creators["vezax shadow resistance action"] = &RaidUlduarActionContext::vezax_shadow_resistance_action;
        creators["yogg-saron shadow resistance action"] = &RaidUlduarActionContext::yogg_saron_shadow_resistance_action;
        creators["yogg-saron ominous cloud cheat action"] = &RaidUlduarActionContext::yogg_saron_ominous_cloud_cheat_action;
        creators["yogg-saron guardian positioning action"] = &RaidUlduarActionContext::yogg_saron_guardian_positioning_action;
        creators["yogg-saron sanity action"] = &RaidUlduarActionContext::yogg_saron_sanity_action;
        creators["yogg-saron death orb action"] = &RaidUlduarActionContext::yogg_saron_death_orb_action;
        creators["yogg-saron malady of the mind action"] = &RaidUlduarActionContext::yogg_saron_malady_of_the_mind_action;
        creators["yogg-saron mark target action"] = &RaidUlduarActionContext::yogg_saron_mark_target_action;
        creators["yogg-saron brain link action"] = &RaidUlduarActionContext::yogg_saron_brain_link_action;
        creators["yogg-saron move to enter portal action"] = &RaidUlduarActionContext::yogg_saron_move_to_enter_portal_action;
        creators["yogg-saron use portal action"] = &RaidUlduarActionContext::yogg_saron_use_portal_action;
        creators["yogg-saron fall from floor action"] = &RaidUlduarActionContext::yogg_saron_fall_from_floor_action;
        creators["yogg-saron boss room movement cheat action"] = &RaidUlduarActionContext::yogg_saron_boss_room_movement_cheat_action;
        creators["yogg-saron illusion room action"] = &RaidUlduarActionContext::yogg_saron_illusion_room_action;
        creators["yogg-saron move to exit portal action"] = &RaidUlduarActionContext::yogg_saron_move_to_exit_portal_action;
        creators["yogg-saron lunatic gaze action"] = &RaidUlduarActionContext::yogg_saron_lunatic_gaze_action;
        creators["yogg-saron phase 3 positioning action"] = &RaidUlduarActionContext::yogg_saron_phase_3_positioning_action;
        creators["yogg-saron crusher tentacle action"] = &RaidUlduarActionContext::yogg_saron_crusher_tentacle_action;
        creators["yogg-saron guardian control action"] = &RaidUlduarActionContext::yogg_saron_guardian_control_action;
        creators["yogg-saron sanity conservation action"] = &RaidUlduarActionContext::yogg_saron_sanity_conservation_action;
        creators["yogg-saron squeeze escape action"] = &RaidUlduarActionContext::yogg_saron_squeeze_escape_action;
        creators["algalon cosmic smash action"] = &RaidUlduarActionContext::algalon_cosmic_smash_action;
        creators["algalon big bang hide action"] = &RaidUlduarActionContext::algalon_big_bang_hide_action;
        creators["algalon big bang soak action"] = &RaidUlduarActionContext::algalon_big_bang_soak_action;
        creators["algalon phase punch swap action"] = &RaidUlduarActionContext::algalon_phase_punch_swap_action;
        creators["algalon constellation kite action"] = &RaidUlduarActionContext::algalon_constellation_kite_action;
        creators["algalon dark matter mark action"] = &RaidUlduarActionContext::algalon_dark_matter_mark_action;
        creators["algalon collapsing star mark action"] = &RaidUlduarActionContext::algalon_collapsing_star_mark_action;
        creators["ignis scorched ground action"] = &RaidUlduarActionContext::ignis_scorched_ground_action;
        creators["ignis construct tank action"] = &RaidUlduarActionContext::ignis_construct_tank_action;
        creators["ignis brittle construct mark action"] = &RaidUlduarActionContext::ignis_brittle_construct_mark_action;
        creators["ignis molten construct avoid action"] = &RaidUlduarActionContext::ignis_molten_construct_avoid_action;
        creators["ignis slag pot heal action"] = &RaidUlduarActionContext::ignis_slag_pot_heal_action;
        creators["auriaya seeping essence action"] = &RaidUlduarActionContext::auriaya_seeping_essence_action;
        creators["auriaya sentry taunt action"] = &RaidUlduarActionContext::auriaya_sentry_taunt_action;
        creators["auriaya raid position action"] = &RaidUlduarActionContext::auriaya_raid_position_action;
        creators["auriaya set dps priority action"] = &RaidUlduarActionContext::auriaya_set_dps_priority_action;
        creators["auriaya anti fear action"] = &RaidUlduarActionContext::auriaya_anti_fear_action;
        creators["yogg-saron anti fear action"] = &RaidUlduarActionContext::yogg_saron_anti_fear_action;
        creators["mimiron magnetic core action"] = &RaidUlduarActionContext::mimiron_magnetic_core_action;
        creators["mimiron plasma blast action"] = &RaidUlduarActionContext::mimiron_plasma_blast_action;
        creators["mimiron set dps priority action"] = &RaidUlduarActionContext::mimiron_set_dps_priority_action;
        creators["mimiron proximity mine action"] = &RaidUlduarActionContext::mimiron_proximity_mine_action;
        creators["mimiron bomb bot action"] = &RaidUlduarActionContext::mimiron_bomb_bot_action;
        creators["mimiron pet control action"] = &RaidUlduarActionContext::mimiron_pet_control_action;
        creators["thorim unbalancing strike swap action"] = &RaidUlduarActionContext::thorim_unbalancing_strike_swap_action;
        creators["thorim sif blizzard action"] = &RaidUlduarActionContext::thorim_sif_blizzard_action;
        creators["thorim sif frost nova action"] = &RaidUlduarActionContext::thorim_sif_frost_nova_action;
        creators["mimiron dodge flames action"] = &RaidUlduarActionContext::mimiron_dodge_flames_action;
        creators["mimiron frost bomb action"] = &RaidUlduarActionContext::mimiron_frost_bomb_action;
        creators["xt002 searing light spread action"] = &RaidUlduarActionContext::xt002_searing_light_spread_action;
        creators["xt002 gravity bomb spread action"] = &RaidUlduarActionContext::xt002_gravity_bomb_spread_action;
        creators["xt002 gravity bomb carrier action"] = &RaidUlduarActionContext::xt002_gravity_bomb_carrier_action;
        creators["xt002 searing light carrier action"] = &RaidUlduarActionContext::xt002_searing_light_carrier_action;
        creators["xt002 boombot avoid action"] = &RaidUlduarActionContext::xt002_boombot_avoid_action;
        creators["xt002 void zone action"] = &RaidUlduarActionContext::xt002_void_zone_action;
        creators["xt002 raid position action"] = &RaidUlduarActionContext::xt002_raid_position_action;
        creators["xt002 set dps priority action"] = &RaidUlduarActionContext::xt002_set_dps_priority_action;
        creators["xt002 pummeller taunt action"] = &RaidUlduarActionContext::xt002_pummeller_taunt_action;
        creators["xt002 redirect threat action"] = &RaidUlduarActionContext::xt002_redirect_threat_action;
    }

private:
    static Action* flame_leviathan_vehicle(PlayerbotAI* ai) { return new FlameLeviathanVehicleAction(ai); }
    static Action* flame_leviathan_enter_vehicle(PlayerbotAI* ai) { return new FlameLeviathanEnterVehicleAction(ai); }
    static Action* flame_leviathan_drive(PlayerbotAI* ai) { return new FlameLeviathanDriveAction(ai); }
    static Action* flame_leviathan_interrupt_vents(PlayerbotAI* ai) { return new FlameLeviathanInterruptVentsAction(ai); }
    static Action* razorscale_avoid_devouring_flames(PlayerbotAI* ai) { return new RazorscaleAvoidDevouringFlameAction(ai); }
    static Action* razorscale_avoid_sentinel(PlayerbotAI* ai) { return new RazorscaleAvoidSentinelAction(ai); }
    static Action* razorscale_ignore_flying_alone(PlayerbotAI* ai) { return new RazorscaleIgnoreBossAction(ai); }
    static Action* razorscale_avoid_whirlwind(PlayerbotAI* ai) { return new RazorscaleAvoidWhirlwindAction(ai); }
    static Action* razorscale_grounded(PlayerbotAI* ai) { return new RazorscaleGroundedAction(ai); }
    static Action* razorscale_harpoon_action(PlayerbotAI* ai) { return new RazorscaleHarpoonAction(ai); }
    static Action* razorscale_fuse_armor_action(PlayerbotAI* ai) { return new RazorscaleFuseArmorAction(ai); }
    static Action* razorscale_kill_target_action(PlayerbotAI* ai) { return new RazorscaleKillTargetAction(ai); }
    static Action* razorscale_pet_control_action(PlayerbotAI* ai) { return new RazorscalePetControlAction(ai); }
    static Action* razorscale_flame_breath_action(PlayerbotAI* ai) { return new RazorscaleFlameBreathAction(ai); }
    static Action* razorscale_fire_resistance_action(PlayerbotAI* ai) { return new BossFireResistanceAction(ai, "razorscale"); }
    static Action* ignis_fire_resistance_action(PlayerbotAI* ai) { return new BossFireResistanceAction(ai, "ignis the furnace master"); }
    static Action* iron_assembly_lightning_tendrils_action(PlayerbotAI* ai) { return new IronAssemblyLightningTendrilsAction(ai); }
    static Action* iron_assembly_overload_action(PlayerbotAI* ai) { return new IronAssemblyOverloadAction(ai); }
    static Action* iron_assembly_rune_of_power_action(PlayerbotAI* ai) { return new IronAssemblyRuneOfPowerAction(ai); }
    static Action* iron_assembly_kill_order_action(PlayerbotAI* ai) { return new IronAssemblyKillOrderAction(ai); }
    static Action* iron_assembly_fusion_punch_swap_action(PlayerbotAI* ai) { return new IronAssemblyFusionPunchSwapAction(ai); }
    static Action* kologarn_body_tank_action(PlayerbotAI* ai) { return new KologarnBodyTankAction(ai); }
    static Action* kologarn_off_tank_action(PlayerbotAI* ai) { return new KologarnOffTankAction(ai); }
    static Action* kologarn_rubble_tank_action(PlayerbotAI* ai) { return new KologarnRubbleTankAction(ai); }
    static Action* kologarn_dps_target_action(PlayerbotAI* ai) { return new KologarnDpsTargetAction(ai); }
    static Action* kologarn_smash_swap_action(PlayerbotAI* ai) { return new KologarnSmashSwapAction(ai); }
    static Action* kologarn_body_uncovered_action(PlayerbotAI* ai) { return new KologarnBodyUncoveredAction(ai); }
    static Action* kologarn_fall_from_floor_action(PlayerbotAI* ai) { return new KologarnFallFromFloorAction(ai); }
    static Action* kologarn_nature_resistance_action(PlayerbotAI* ai) { return new BossNatureResistanceAction(ai, "kologarn"); }
    static Action* kologarn_rubble_slowdown_action(PlayerbotAI* ai) { return new KologarnRubbleSlowdownAction(ai); }
    static Action* kologarn_eyebeam_action(PlayerbotAI* ai) { return new KologarnEyebeamAction(ai); }
    static Action* auriaya_fall_from_floor_action(PlayerbotAI* ai) { return new AuriayaFallFromFloorAction(ai); }
    static Action* hodir_move_snowpacked_icicle(PlayerbotAI* ai) { return new HodirMoveSnowpackedIcicleAction(ai); }
    static Action* hodir_biting_cold_jump(PlayerbotAI* ai) { return new HodirBitingColdJumpAction(ai); }
    static Action* hodir_frost_resistance_action(PlayerbotAI* ai) { return new BossFrostResistanceAction(ai, "hodir"); }
    static Action* hodir_spread_storm_cloud(PlayerbotAI* ai) { return new HodirSpreadStormCloudAction(ai); }
    static Action* hodir_icicle_dodge_action(PlayerbotAI* ai) { return new HodirIcicleDodgeAction(ai); }
    static Action* hodir_raid_position_action(PlayerbotAI* ai) { return new HodirRaidPositionAction(ai); }
    static Action* hodir_set_dps_priority_action(PlayerbotAI* ai) { return new HodirSetDpsPriorityAction(ai); }
    static Action* hodir_frozen_blows_swap_action(PlayerbotAI* ai) { return new HodirFrozenBlowsSwapAction(ai); }
    static Action* freya_move_away_nature_bomb(PlayerbotAI* ai) { return new FreyaMoveAwayNatureBombAction(ai); }
    static Action* freya_fire_resistance_action(PlayerbotAI* ai) { return new BossFireResistanceAction(ai, "freya"); }
    static Action* freya_nature_resistance_action(PlayerbotAI* ai) { return new BossNatureResistanceAction(ai, "freya"); }
    static Action* freya_set_dps_priority(PlayerbotAI* ai) { return new FreyaSetDpsPriorityAction(ai); }
    static Action* freya_tank_adds(PlayerbotAI* ai) { return new FreyaTankAddsAction(ai); }
    static Action* freya_redirect_threat(PlayerbotAI* ai) { return new FreyaRedirectThreatAction(ai); }
    static Action* freya_avoid_detonating_lasher(PlayerbotAI* ai) { return new FreyaAvoidDetonatingLasherAction(ai); }
    static Action* freya_move_to_healing_spore_action(PlayerbotAI* ai) { return new FreyaMoveToHealingSporeAction(ai); }
    static Action* freya_break_iron_roots(PlayerbotAI* ai) { return new FreyaBreakIronRootsAction(ai); }
    static Action* freya_dodge_unstable_sun_beam(PlayerbotAI* ai) { return new FreyaDodgeUnstableSunBeamAction(ai); }
    static Action* thorim_frost_resistance_action(PlayerbotAI* ai) { return new BossFrostResistanceAction(ai, "thorim"); }
    static Action* thorim_nature_resistance_action(PlayerbotAI* ai) { return new BossNatureResistanceAction(ai, "thorim"); }
    static Action* thorim_unbalancing_strike_action(PlayerbotAI* ai) { return new ThorimUnbalancingStrikeAction(ai); }
    static Action* thorim_mark_dps_target_action(PlayerbotAI* ai) { return new ThorimMarkDpsTargetAction(ai); }
    static Action* thorim_arena_positioning_action(PlayerbotAI* ai) { return new ThorimArenaPositioningAction(ai); }
    static Action* thorim_gauntlet_positioning_action(PlayerbotAI* ai) { return new ThorimGauntletPositioningAction(ai); }
    static Action* thorim_phase2_positioning_action(PlayerbotAI* ai) { return new ThorimPhase2PositioningAction(ai); }
    static Action* mimiron_fire_resistance_action(PlayerbotAI* ai) { return new BossFireResistanceAction(ai, "mimiron"); }
    static Action* mimiron_shock_blast_action(PlayerbotAI* ai) { return new MimironShockBlastAction(ai); }
    static Action* mimiron_phase_1_positioning_action(PlayerbotAI* ai) { return new MimironPhase1PositioningAction(ai); }
    static Action* mimiron_p3wx2_laser_barrage_action(PlayerbotAI* ai) { return new MimironP3Wx2LaserBarrageAction(ai); }
    static Action* mimiron_arc_spread_action(PlayerbotAI* ai) { return new MimironArcSpreadAction(ai); }
    static Action* mimiron_aerial_command_unit_action(PlayerbotAI* ai) { return new MimironAerialCommandUnitAction(ai); }
    static Action* mimiron_rocket_strike_action(PlayerbotAI* ai) { return new MimironRocketStrikeAction(ai); }
    static Action* mimiron_phase_4_focus_action(PlayerbotAI* ai) { return new MimironPhase4FocusAction(ai); }
    static Action* sara_shadow_resistance_action(PlayerbotAI* ai) { return new BossShadowResistanceAction(ai, "sara"); }
    static Action* vezax_reset_encounter_state_action(PlayerbotAI* ai) { return new VezaxResetEncounterStateAction(ai); }
    static Action* vezax_mark_of_the_faceless_action(PlayerbotAI* ai) { return new VezaxMarkOfTheFacelessAction(ai); }
    static Action* vezax_vapor_puddle_clear_action(PlayerbotAI* ai) { return new VezaxVaporPuddleClearAction(ai); }
    static Action* vezax_shadow_crash_clear_action(PlayerbotAI* ai) { return new VezaxShadowCrashClearAction(ai); }
    static Action* vezax_searing_flames_interrupt_action(PlayerbotAI* ai) { return new VezaxSearingFlamesInterruptAction(ai); }
    static Action* vezax_surge_of_darkness_action(PlayerbotAI* ai) { return new VezaxSurgeOfDarknessAction(ai); }
    static Action* vezax_saronite_animus_action(PlayerbotAI* ai) { return new VezaxSaroniteAnimusAction(ai); }
    static Action* vezax_vapor_soak_action(PlayerbotAI* ai) { return new VezaxVaporSoakAction(ai); }
    static Action* vezax_kill_vapor_action(PlayerbotAI* ai) { return new VezaxKillVaporAction(ai); }
    static Action* vezax_shadow_crash_soak_action(PlayerbotAI* ai) { return new VezaxShadowCrashSoakAction(ai); }
    static Action* vezax_raid_position_action(PlayerbotAI* ai) { return new VezaxRaidPositionAction(ai); }
    static Action* vezax_shadow_resistance_action(PlayerbotAI* ai) { return new BossShadowResistanceAction(ai, "general vezax"); }
    static Action* yogg_saron_shadow_resistance_action(PlayerbotAI* ai) { return new BossShadowResistanceAction(ai, "yogg-saron"); }
    static Action* yogg_saron_ominous_cloud_cheat_action(PlayerbotAI* ai) { return new YoggSaronOminousCloudCheatAction(ai); }
    static Action* yogg_saron_guardian_positioning_action(PlayerbotAI* ai) { return new YoggSaronGuardianPositioningAction(ai); }
    static Action* yogg_saron_sanity_action(PlayerbotAI* ai) { return new YoggSaronSanityAction(ai); }
    static Action* yogg_saron_death_orb_action(PlayerbotAI* ai) { return new YoggSaronDeathOrbAction(ai); }
    static Action* yogg_saron_malady_of_the_mind_action(PlayerbotAI* ai) { return new YoggSaronMaladyOfTheMindAction(ai); }
    static Action* yogg_saron_mark_target_action(PlayerbotAI* ai) { return new YoggSaronMarkTargetAction(ai); }
    static Action* yogg_saron_brain_link_action(PlayerbotAI* ai) { return new YoggSaronBrainLinkAction(ai); }
    static Action* yogg_saron_move_to_enter_portal_action(PlayerbotAI* ai) { return new YoggSaronMoveToEnterPortalAction(ai); }
    static Action* yogg_saron_use_portal_action(PlayerbotAI* ai) { return new YoggSaronUsePortalAction(ai); }
    static Action* yogg_saron_fall_from_floor_action(PlayerbotAI* ai) { return new YoggSaronFallFromFloorAction(ai); }
    static Action* yogg_saron_boss_room_movement_cheat_action(PlayerbotAI* ai) { return new YoggSaronBossRoomMovementCheatAction(ai); }
    static Action* yogg_saron_illusion_room_action(PlayerbotAI* ai) { return new YoggSaronIllusionRoomAction(ai); }
    static Action* yogg_saron_move_to_exit_portal_action(PlayerbotAI* ai) { return new YoggSaronMoveToExitPortalAction(ai); }
    static Action* yogg_saron_lunatic_gaze_action(PlayerbotAI* ai) { return new YoggSaronLunaticGazeAction(ai); }
    static Action* yogg_saron_phase_3_positioning_action(PlayerbotAI* ai) { return new YoggSaronPhase3PositioningAction(ai); }
    static Action* yogg_saron_crusher_tentacle_action(PlayerbotAI* ai) { return new YoggSaronCrusherTentacleAction(ai); }
    static Action* yogg_saron_guardian_control_action(PlayerbotAI* ai) { return new YoggSaronGuardianControlAction(ai); }
    static Action* yogg_saron_sanity_conservation_action(PlayerbotAI* ai) { return new YoggSaronSanityConservationAction(ai); }
    static Action* yogg_saron_squeeze_escape_action(PlayerbotAI* ai) { return new YoggSaronSqueezeEscapeAction(ai); }
    static Action* algalon_cosmic_smash_action(PlayerbotAI* ai) { return new AlgalonCosmicSmashAction(ai); }
    static Action* algalon_big_bang_hide_action(PlayerbotAI* ai) { return new AlgalonBigBangHideAction(ai); }
    static Action* algalon_big_bang_soak_action(PlayerbotAI* ai) { return new AlgalonBigBangSoakAction(ai); }
    static Action* algalon_phase_punch_swap_action(PlayerbotAI* ai) { return new AlgalonPhasePunchSwapAction(ai); }
    static Action* algalon_constellation_kite_action(PlayerbotAI* ai) { return new AlgalonConstellationKiteAction(ai); }
    static Action* algalon_dark_matter_mark_action(PlayerbotAI* ai) { return new AlgalonDarkMatterMarkAction(ai); }
    static Action* algalon_collapsing_star_mark_action(PlayerbotAI* ai) { return new AlgalonCollapsingStarMarkAction(ai); }
    static Action* ignis_scorched_ground_action(PlayerbotAI* ai) { return new IgnisScorchedGroundAction(ai); }
    static Action* ignis_construct_tank_action(PlayerbotAI* ai) { return new IgnisConstructTankAction(ai); }
    static Action* ignis_brittle_construct_mark_action(PlayerbotAI* ai) { return new IgnisBrittleConstructMarkAction(ai); }
    static Action* ignis_molten_construct_avoid_action(PlayerbotAI* ai) { return new IgnisMoltenConstructAvoidAction(ai); }
    static Action* ignis_slag_pot_heal_action(PlayerbotAI* ai) { return new IgnisSlagPotHealAction(ai); }
    static Action* auriaya_seeping_essence_action(PlayerbotAI* ai) { return new AuriayaSeepingEssenceAction(ai); }
    static Action* auriaya_sentry_taunt_action(PlayerbotAI* ai) { return new AuriayaSentryTauntAction(ai); }
    static Action* auriaya_raid_position_action(PlayerbotAI* ai) { return new AuriayaRaidPositionAction(ai); }
    static Action* auriaya_set_dps_priority_action(PlayerbotAI* ai) { return new AuriayaSetDpsPriorityAction(ai); }
    static Action* auriaya_anti_fear_action(PlayerbotAI* ai) { return new AuriayaAntiFearAction(ai); }
    static Action* yogg_saron_anti_fear_action(PlayerbotAI* ai) { return new YoggSaronAntiFearAction(ai); }
    static Action* mimiron_magnetic_core_action(PlayerbotAI* ai) { return new MimironMagneticCoreAction(ai); }
    static Action* mimiron_plasma_blast_action(PlayerbotAI* ai) { return new MimironPlasmaBlastAction(ai); }
    static Action* mimiron_set_dps_priority_action(PlayerbotAI* ai) { return new MimironSetDpsPriorityAction(ai); }
    static Action* mimiron_proximity_mine_action(PlayerbotAI* ai) { return new MimironProximityMineAction(ai); }
    static Action* mimiron_bomb_bot_action(PlayerbotAI* ai) { return new MimironBombBotAction(ai); }
    static Action* mimiron_pet_control_action(PlayerbotAI* ai) { return new MimironPetControlAction(ai); }
    static Action* thorim_unbalancing_strike_swap_action(PlayerbotAI* ai) { return new ThorimUnbalancingStrikeSwapAction(ai); }
    static Action* thorim_sif_blizzard_action(PlayerbotAI* ai) { return new ThorimSifBlizzardAction(ai); }
    static Action* thorim_sif_frost_nova_action(PlayerbotAI* ai) { return new ThorimSifFrostNovaAction(ai); }
    static Action* mimiron_dodge_flames_action(PlayerbotAI* ai) { return new MimironDodgeFlamesAction(ai); }
    static Action* mimiron_frost_bomb_action(PlayerbotAI* ai) { return new MimironFrostBombAction(ai); }
    static Action* xt002_searing_light_spread_action(PlayerbotAI* ai) { return new XT002SearingLightSpreadAction(ai); }
    static Action* xt002_gravity_bomb_spread_action(PlayerbotAI* ai) { return new XT002GravityBombSpreadAction(ai); }
    static Action* xt002_gravity_bomb_carrier_action(PlayerbotAI* ai) { return new XT002GravityBombCarrierAction(ai); }
    static Action* xt002_searing_light_carrier_action(PlayerbotAI* ai) { return new XT002SearingLightCarrierAction(ai); }
    static Action* xt002_boombot_avoid_action(PlayerbotAI* ai) { return new XT002BoombotAvoidAction(ai); }
    static Action* xt002_void_zone_action(PlayerbotAI* ai) { return new XT002VoidZoneAction(ai); }
    static Action* xt002_raid_position_action(PlayerbotAI* ai) { return new XT002RaidPositionAction(ai); }
    static Action* xt002_set_dps_priority_action(PlayerbotAI* ai) { return new XT002SetDpsPriorityAction(ai); }
    static Action* xt002_pummeller_taunt_action(PlayerbotAI* ai) { return new XT002PummellerTauntAction(ai); }
    static Action* xt002_redirect_threat_action(PlayerbotAI* ai) { return new XT002RedirectThreatAction(ai); }
};

#endif
