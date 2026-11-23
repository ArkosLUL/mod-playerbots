/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERYOGGSARON_H
#define PLAYERBOTS_ULDENCOUNTERYOGGSARON_H

#include "ObjectGuid.h"
#include "Position.h"
#include "UldData.h"

#include <vector>

class PlayerbotAI;
class Creature;
class Item;
class Player;
class Unit;

enum UlduarYoggSaronIds
{
    ACTION_ILLUSION_DRAGONS = 1,
    ACTION_ILLUSION_ICECROWN = 2,
    ACTION_ILLUSION_STORMWIND = 3,
    NPC_GUARDIAN_OF_YS = 33136,
    NPC_YOGG_SARON = 33288,
    NPC_OMINOUS_CLOUD = 33292,
    NPC_RUBY_CONSORT = 33716,
    NPC_AZURE_CONSORT = 33717,
    NPC_BRONZE_CONSORT = 33718,
    NPC_EMERALD_CONSORT = 33719,
    NPC_OBSIDIAN_CONSORT = 33720,
    NPC_ALEXTRASZA = 33536,
    NPC_MALYGOS_ILLUSION = 33535,
    NPC_NELTHARION = 33523,
    NPC_YSERA = 33495,
    GO_DRAGON_SOUL = 194462,
    NPC_SARA_PHASE_1 = 33134,
    NPC_LICH_KING_ILLUSION = 33441,
    NPC_IMMOLATED_CHAMPION = 33442,
    NPC_SUIT_OF_ARMOR = 33433,
    NPC_GARONA = 33436,
    NPC_KING_LLANE = 33437,
    NPC_DEATHSWORN_ZEALOT = 33567,
    NPC_INFLUENCE_TENTACLE = 33943,
    NPC_DEATH_RAY = 33881,
    NPC_DEATH_ORB = 33882,
    NPC_BRAIN = 33890,
    NPC_CRUSHER_TENTACLE = 33966,
    NPC_CONSTRICTOR_TENTACLE = 33983,
    NPC_CORRUPTOR_TENTACLE = 33985,
    NPC_IMMORTAL_GUARDIAN = 33988,
    NPC_LAUGHING_SKULL = 33990,
    NPC_SANITY_WELL = 33991,
    NPC_DESCEND_INTO_MADNESS = 34072,
    NPC_MARKED_IMMORTAL_GUARDIAN = 36064,
    SPELL_SANITY = 63050,
    SPELL_SARAS_BLESSING = 63134,
    SPELL_SARAS_FERVOR = 63138,
    SPELL_BRAIN_LINK = 63802,
    SPELL_BRAIN_LINK_DAMAGE = 63803,
    SPELL_BRAIN_LINK_OK = 63804,
    SPELL_MALADY_OF_THE_MIND = 63830,
    SPELL_SHADOW_BARRIER = 63894,
    SPELL_TELEPORT_TO_CHAMBER = 63997,
    SPELL_TELEPORT_TO_ICECROWN = 63998,
    SPELL_TELEPORT_TO_STORMWIND = 63989,
    SPELL_TELEPORT_BACK = 63992,
    SPELL_CANCEL_ILLUSION_AURA = 63993,
    SPELL_INDUCE_MADNESS = 64059,
    SPELL_LUNATIC_GAZE_YS = 64163,
    SPELL_DARK_VOLLEY = 63038,
    SPELL_DARK_VOLLEY_H = 65330,
    SPELL_SQUEEZE = 64125,
    SPELL_WEAKENED = 64162,
    SPELL_KNOCK_BACK_TRIGGERED = 64020,
    SPELL_CRUSH_CONE = 64147,
    SPELL_DIMINISH_POWER = 64145,
    SPELL_LUNATIC_GAZE_SKULL = 64168,
    SPELL_SHADOW_NOVA_SARA = 65719,
    SPELL_SHATTERED_ILLUSION = 64173,
    SPELL_SHADOW_BEACON = 64465,
    SPELL_EMPOWERING_SHADOWS_10 = 64468,
    SPELL_EMPOWERING_SHADOWS_25 = 64486,
    SPELL_GUARDIAN_DRAIN_LIFE = 64159,
    SPELL_GUARDIAN_DRAIN_LIFE_H = 64160,
    SPELL_AURA_MASTERY = 31821,
    SPELL_CONCENTRATION_AURA = 19746,
    SPELL_DEAFENING_ROAR = 64189,
    SPELL_DISTRACTING_SHOT = 20736,
    SPELL_WARRIOR_TAUNT = 355,
    SPELL_PALADIN_HAND_OF_RECKONING = 62124,
    SPELL_DEATH_KNIGHT_DARK_COMMAND = 56222,
    SPELL_DRUID_GROWL = 6795,
    SPELL_DRUID_BEAR_FORM = 5487,
    SPELL_DRUID_DIRE_BEAR_FORM = 9634,
    SPELL_HEROISM_SHAMAN = 32182,
    SPELL_BLOODLUST_SHAMAN = 2825,
    SPELL_SATED = 57724,
    SPELL_EXHAUSTION = 57723,
    ITEM_VALANYR = 46017,
    GO_FLEE_TO_THE_SURFACE_PORTAL = 194625,
    GO_CHAMBER_ILLUSION_DOORS = 194635,
    GO_ICECROWN_ILLUSION_DOORS = 194636,
    GO_STORMWIND_ILLUSION_DOORS = 194637,
};

inline constexpr float ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT = 300.0f;
inline constexpr float ULDUAR_YOGG_SARON_BRAIN_ROOM_AXIS_Z_PATHING_ISSUE_DETECT = 200.0f;
inline constexpr float ULDUAR_YOGG_SARON_SEARCH_RANGE = 200.0f;
inline constexpr float ULDUAR_YOGG_SARON_STORMWIND_KEEPER_RADIUS = 60.0f;
inline constexpr float ULDUAR_YOGG_SARON_ICECROWN_CITADEL_RADIUS = 60.0f;
inline constexpr float ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_RADIUS = 60.0f;
inline constexpr float ULDUAR_YOGG_SARON_BRAIN_ROOM_RADIUS = 50.0f;

inline constexpr float ULDUAR_YOGG_SARON_BRAIN_LEVEL_MIN_Z = 230.0f;
inline constexpr float ULDUAR_YOGG_SARON_BRAIN_LEVEL_MAX_Z = 250.0f;

inline constexpr uint32 ULDUAR_YOGG_SARON_SANITY_CONSERVE_THRESHOLD = 15;

inline constexpr uint32 ULDUAR_YOGG_SARON_BRAIN_TEAM_MIN_SANITY = 60;

inline constexpr uint32 ULDUAR_YOGG_SARON_SANITY_LOW = 40;

inline constexpr float ULDUAR_YOGG_SARON_NITRO_MIN_WALK = 40.0f;

inline constexpr float ULDUAR_YOGG_SARON_BRAIN_CHARGE_MIN_GAIN = 8.0f;

inline constexpr float ULDUAR_YOGG_SARON_CLOUD_AVOID_RADIUS = 9.5f;

inline constexpr uint32 ULDUAR_YOGG_SARON_CLOUD_LEAD_MS = 6000;

inline constexpr uint32 ULDUAR_YOGG_SARON_WALK_CLOUD_STEPS = 8;

inline constexpr float ULDUAR_YOGG_SARON_SHADOW_NOVA_TRIGGER_RADIUS = 17.0f;
inline constexpr float ULDUAR_YOGG_SARON_SHADOW_NOVA_CLEAR_RADIUS = 20.0f;

inline constexpr float ULDUAR_YOGG_SARON_FERVOR_HOLD_RADIUS = 22.0f;

inline constexpr float ULDUAR_YOGG_SARON_GUARDIAN_NOVA_HEALTH_PCT = 20.0f;

inline constexpr float ULDUAR_YOGG_SARON_FERVOR_NOVA_HEALTH_PCT = 50.0f;

inline constexpr uint32 ULDUAR_YOGG_SARON_NOVA_EXPECTED_DAMAGE = 28600;
inline constexpr uint32 ULDUAR_YOGG_SARON_NOVA_RESISTED_DAMAGE = 25740;
inline constexpr uint32 ULDUAR_YOGG_SARON_NOVA_RESISTED_AT = 130;

inline constexpr float ULDUAR_YOGG_SARON_NOVA_SOON_HEALTH_PCT = 20.0f;
inline constexpr float ULDUAR_YOGG_SARON_NOVA_NOW_HEALTH_PCT = 10.0f;
inline constexpr uint32 ULDUAR_YOGG_SARON_NOVA_SOON_WINDOW_MS = 12000;
inline constexpr uint32 ULDUAR_YOGG_SARON_NOVA_NOW_DUE_MS = 4000;
inline constexpr uint32 ULDUAR_YOGG_SARON_NOVA_OVERDUE_NEED_MS = 1;

inline constexpr uint32 ULDUAR_YOGG_SARON_NOVA_MAX_CHAIN = 2;

inline constexpr uint32 ULDUAR_YOGG_SARON_NOVA_RAID_COUNT = 3;

inline constexpr uint32 ULDUAR_YOGG_SARON_NOVA_WINDOW_MS = 10000;

inline constexpr float ULDUAR_YOGG_SARON_P1_CLOUD_FREE_RADIUS = 2.8f;
inline constexpr float ULDUAR_YOGG_SARON_P1_RANGED_STATION_RADIUS = 21.5f;
inline constexpr float ULDUAR_YOGG_SARON_P1_RANGED_BAND_TOLERANCE = 1.0f;

inline constexpr float ULDUAR_YOGG_SARON_CLOUD_ORBITS[] = {11.6f, 21.4f, 31.2f, 41.0f, 50.9f, 60.8f};
inline constexpr float ULDUAR_YOGG_SARON_CLOUD_SUMMON_REACH = 8.5f;

inline constexpr float ULDUAR_YOGG_SARON_P1_ROOM_RADIUS =
    ULDUAR_YOGG_SARON_CLOUD_ORBITS[5] + ULDUAR_YOGG_SARON_CLOUD_SUMMON_REACH;

inline constexpr float ULDUAR_YOGG_SARON_P1_DOOR_RADIUS = 90.0f;

inline constexpr float ULDUAR_YOGG_SARON_P1_LEASH = 6.5f;
inline constexpr float ULDUAR_YOGG_SARON_P1_LEASH_RELEASE = ULDUAR_YOGG_SARON_P1_CLOUD_FREE_RADIUS;

inline constexpr float ULDUAR_YOGG_SARON_P1_TANK_PULL_OFFSET = 1.5f;
inline constexpr float ULDUAR_YOGG_SARON_P1_TANK_PULL_ARRIVED = 1.0f;

inline constexpr float ULDUAR_YOGG_SARON_P1_PILE_LEASH =
    ULDUAR_YOGG_SARON_CLOUD_ORBITS[0] - ULDUAR_YOGG_SARON_CLOUD_SUMMON_REACH;
inline constexpr float ULDUAR_YOGG_SARON_P1_PILE_RELEASE = 2.0f;

inline constexpr float ULDUAR_YOGG_SARON_P1_TAUNT_RANGE = 30.0f;

inline constexpr float ULDUAR_YOGG_SARON_P1_SARA_NOVA_RADIUS = 15.0f;

inline constexpr float ULDUAR_YOGG_SARON_P1_PARK_HEALTH_PCT = 35.0f;

inline constexpr uint32 ULDUAR_YOGG_SARON_P1_NOVA_GAP_MS = 6000;

inline constexpr float ULDUAR_YOGG_SARON_P1_NOVA_GAP_HEALTH_PCT = 30.0f;

inline constexpr float ULDUAR_YOGG_SARON_P1_NOVA_CHAIN_RADIUS = 18.0f;

inline constexpr float ULDUAR_YOGG_SARON_P1_AOE_HOLD_RADIUS = 10.0f;

inline constexpr float ULDUAR_YOGG_SARON_P1_RANGED_STACK_RADIUS = 5.0f;
inline constexpr float ULDUAR_YOGG_SARON_P1_PARKED_STACK_SCALE = 1.5f;

inline constexpr float ULDUAR_YOGG_SARON_SPACING_SEARCH_RADIUS = 25.0f;
inline constexpr float ULDUAR_YOGG_SARON_SPACING_MAX_FROM_MIDDLE = 35.0f;
inline constexpr float ULDUAR_YOGG_SARON_SWEEP_STEP = 2.0f;
inline constexpr float ULDUAR_YOGG_SARON_SWEEP_ANGLE_STEP = static_cast<float>(M_PI) / 8.0f;

inline constexpr uint32 ULDUAR_YOGG_SARON_SPACING_HOLD_MS = 3000;

inline constexpr float ULDUAR_YOGG_SARON_PET_TARGET_RADIUS = 40.0f;

inline constexpr float ULDUAR_YOGG_SARON_PET_LEFTOVER_SEARCH_RADIUS = 70.0f;

inline constexpr float ULDUAR_YOGG_SARON_CRUSH_RANGE = 25.0f;
inline constexpr float ULDUAR_YOGG_SARON_CRUSH_TRIGGER_ARC = 8.0f;
inline constexpr float ULDUAR_YOGG_SARON_CRUSH_CLEAR_ARC = 14.0f;

inline constexpr float ULDUAR_YOGG_SARON_CRUSHER_REACH_TRIGGER_RADIUS = 11.0f;
inline constexpr float ULDUAR_YOGG_SARON_CRUSHER_REACH_CLEAR_RADIUS = 13.0f;
inline constexpr float ULDUAR_YOGG_SARON_CRUSHER_AREA_HOLD_SLACK = 2.0f;

inline constexpr uint32 ULDUAR_YOGG_SARON_STUN_EXIT_LEAD_MS = 4000;
inline constexpr float ULDUAR_YOGG_SARON_STUN_BRAIN_FLOOR_PCT = 35.0f;

inline constexpr float ULDUAR_YOGG_SARON_BRAIN_HOLD_PCT = 32.0f;
inline constexpr float ULDUAR_YOGG_SARON_BRAIN_DOT_STOP_PCT = 36.0f;
inline constexpr uint32 ULDUAR_YOGG_SARON_BRAIN_RESUME_MS = 8000;
inline constexpr float ULDUAR_YOGG_SARON_BRAIN_HOLD_GUARD_PCT = 31.0f;
inline constexpr uint32 ULDUAR_YOGG_SARON_BRAIN_HOLD_SEEN_MS = 2000;
inline constexpr float ULDUAR_YOGG_SARON_BRAIN_PHASE_3_SHARE = 0.3f;

inline constexpr float ULDUAR_YOGG_SARON_DEATH_RAY_TRIGGER_RADIUS = 9.0f;
inline constexpr float ULDUAR_YOGG_SARON_DEATH_RAY_CLEAR_RADIUS = 14.0f;

inline constexpr float ULDUAR_YOGG_SARON_P2_SPACING_SEARCH_RADIUS = 35.0f;

inline constexpr float ULDUAR_YOGG_SARON_P2_SPACING_MAX_FROM_MIDDLE = 55.0f;

inline constexpr float ULDUAR_YOGG_SARON_BODY_KNOCKBACK_RADIUS = 13.3f;
inline constexpr float ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS = 15.0f;

inline constexpr uint32 ULDUAR_YOGG_SARON_HANDOVER_HOLD_MS = 6000;

inline constexpr float ULDUAR_YOGG_SARON_BODY_DETOUR_RADIUS = 24.0f;

inline constexpr float ULDUAR_YOGG_SARON_BRAIN_LINK_RANGE = 20.0f;
inline constexpr float ULDUAR_YOGG_SARON_BRAIN_LINK_CLOSE = 15.0f;
inline constexpr uint32 ULDUAR_YOGG_SARON_BRAIN_LINK_PAIR_TTL_MS = 3000;

inline constexpr float ULDUAR_YOGG_SARON_MALADY_SPREAD_RADIUS = 15.0f;

inline constexpr float ULDUAR_YOGG_SARON_HAND_OF_PROTECTION_RANGE = 30.0f;
inline constexpr uint32 ULDUAR_YOGG_SARON_SQUEEZE_CLAIM_MS = 3000;

inline constexpr uint32 ULDUAR_YOGG_SARON_PORTAL_FIRST_WAVE_MS = 60000;
inline constexpr uint32 ULDUAR_YOGG_SARON_PORTAL_WAVE_PERIOD_MS = 80000;
inline constexpr uint32 ULDUAR_YOGG_SARON_PORTAL_WAVE_DEBOUNCE_MS = 30000;

inline constexpr uint32 ULDUAR_YOGG_SARON_PORTAL_SPREAD_LEAD_FLOOR_MS = 8000;
inline constexpr float ULDUAR_YOGG_SARON_PORTAL_SPREAD_LEAD_SAFETY = 2.0f;
inline constexpr float ULDUAR_YOGG_SARON_PORTAL_SEARCH_RADIUS = 100.0f;

inline constexpr float ULDUAR_YOGG_SARON_PORTAL_CLICK_RADIUS = 2.0f;

inline constexpr uint32 ULDUAR_YOGG_SARON_PORTAL_SPOTS_10MAN = 4;
inline constexpr uint32 ULDUAR_YOGG_SARON_PORTAL_SPOTS_25MAN = 10;

inline constexpr float ULDUAR_YOGG_SARON_LAUGHING_SKULL_RADIUS = 30.0f;

inline constexpr float ULDUAR_YOGG_SARON_FACING_TOLERANCE = 0.1f;

inline constexpr uint32 ULDUAR_YOGG_SARON_SKULL_HEADINGS = 16;

inline constexpr float ULDUAR_YOGG_SARON_SKULL_TARGET_TURN = 0.6981f;
inline constexpr float ULDUAR_YOGG_SARON_SKULL_TARGET_STEP = 0.0873f;

inline constexpr float ULDUAR_YOGG_SARON_ILLUSION_FACING_SEARCH_RADIUS = 15.0f;

inline constexpr float ULDUAR_YOGG_SARON_SKULL_CLEAR_RADIUS = 5.0f;

inline constexpr float ULDUAR_YOGG_SARON_ILLUSION_HEALER_STATION_RADIUS = 10.0f;

inline constexpr float ULDUAR_YOGG_SARON_BRAIN_SPOT_ARRIVED_RADIUS = 3.0f;

inline constexpr uint32 ULDUAR_YOGG_SARON_EXIT_LEAD_FLOOR_MS = 3000;
inline constexpr float ULDUAR_YOGG_SARON_EXIT_LEAD_SAFETY = 1.5f;

inline constexpr float ULDUAR_YOGG_SARON_PHASE_3_STATION_RADIUS = 15.0f;
inline constexpr float ULDUAR_YOGG_SARON_PHASE_3_TANK_LEASH = 30.0f;
inline constexpr float ULDUAR_YOGG_SARON_PHASE_3_STACK_RADIUS = 5.0f;

inline constexpr float ULDUAR_YOGG_SARON_PHASE_3_RANGED_CRUSHER_CLEARANCE = 18.0f;

inline constexpr float ULDUAR_YOGG_SARON_PHASE_3_MELEE_GUARDIAN_RANGE = 20.0f;

inline constexpr float ULDUAR_YOGG_SARON_CONSTRICTOR_MELEE_WALK = 7.0f;

inline constexpr float ULDUAR_YOGG_SARON_PHASE_3_GUARDIAN_FLOOR_PCT = 30.0f;
inline constexpr float ULDUAR_YOGG_SARON_GUARDIAN_WEAKENED_PCT = 10.0f;

inline constexpr float ULDUAR_YOGG_SARON_PHASE_3_AREA_RADIUS = 8.0f;
inline constexpr uint32 ULDUAR_YOGG_SARON_PHASE_3_AREA_MIN_WORTH = 3;
inline constexpr int32 ULDUAR_YOGG_SARON_PHASE_3_DOT_REFRESH_MS = 1000;
inline constexpr float ULDUAR_YOGG_SARON_COMBAT_REACH = 30.0f;

inline constexpr float ULDUAR_YOGG_SARON_GUARDIAN_THREAT_START_SHARE = 0.8f;
inline constexpr float ULDUAR_YOGG_SARON_GUARDIAN_THREAT_KEEP_SHARE = 0.95f;

inline constexpr float ULDUAR_YOGG_SARON_TANK_THREAT_SWITCH_LEAD = 1.5f;

inline constexpr float ULDUAR_YOGG_SARON_GAZE_SAFE_SEPARATION = 1.1345f;

inline constexpr uint32 ULDUAR_YOGG_SARON_WALK_GIVE_UP_MS = 6000;
inline constexpr float ULDUAR_YOGG_SARON_WALK_ARRIVED_RADIUS = 5.0f;

inline constexpr uint32 ULDUAR_YOGG_SARON_WALK_RETRY_MS = 4000;

inline constexpr uint32 ULDUAR_YOGG_SARON_WALK_STALL_MS = 500;

inline constexpr float ULDUAR_YOGG_SARON_REPRISAL_STOP_PCT = 40.0f;
inline constexpr float ULDUAR_YOGG_SARON_REPRISAL_RESUME_PCT = 70.0f;

inline constexpr uint32 ULDUAR_YOGG_SARON_REPRISAL_ALL_IN_MS = 15000;

inline constexpr float ULDUAR_YOGG_SARON_REPRISAL_REGROUP_RANGE = 25.0f;
inline constexpr float ULDUAR_YOGG_SARON_REPRISAL_REGROUP_STOP = 15.0f;

inline constexpr float ULDUAR_YOGG_SARON_STORMWIND_HELPER_RADIUS = 40.0f;

inline constexpr uint32 ULDUAR_YOGG_SARON_SHARE_LIFT_MS = 25000;

inline constexpr float ULDUAR_YOGG_SARON_STAGE_ARRIVED_RADIUS = 1.5f;

inline constexpr uint32 ULDUAR_YOGG_SARON_OBS_SCAN_INTERVAL_MS = 1000;

inline constexpr float ULDUAR_YOGG_SARON_INTERRUPT_SEARCH_RADIUS = 40.0f;

inline constexpr uint32 ULDUAR_YOGG_SARON_INTERRUPT_CLAIM_MS = 1500;

inline constexpr float ULDUAR_YOGG_SARON_BEACON_DRAG_SEARCH_RADIUS = 70.0f;
inline constexpr uint32 ULDUAR_YOGG_SARON_BEACON_HEAL_WAIT_MS = 3000;
inline constexpr float ULDUAR_YOGG_SARON_BEACON_DRAG_ARRIVED_RADIUS = 3.0f;

inline constexpr uint32 ULDUAR_YOGG_SARON_BEACON_TAUNT_CHECK_MS = 1500;
inline constexpr uint32 ULDUAR_YOGG_SARON_BEACON_TAUNT_LAND_MS = 1000;

inline constexpr uint32 ULDUAR_YOGG_SARON_BEACON_PERIOD_MS = 45000;
inline constexpr uint32 ULDUAR_YOGG_SARON_BEACON_STAGE_LEAD_MS = 5000;
inline constexpr uint32 ULDUAR_YOGG_SARON_BEACON_STAGE_TIMEOUT_MS = 8000;
inline constexpr uint32 ULDUAR_YOGG_SARON_BEACON_STAGERS_10 = 1;
inline constexpr uint32 ULDUAR_YOGG_SARON_BEACON_STAGERS_25 = 3;

inline constexpr uint32 ULDUAR_YOGG_SARON_MARK_KICKERS = 2;
inline constexpr uint32 ULDUAR_YOGG_SARON_MARK_KICKER_READY_MS = 2000;

inline constexpr float ULDUAR_YOGG_SARON_BEACON_DANGER_HEALTH_PCT = 40.0f;
inline constexpr uint32 ULDUAR_YOGG_SARON_BEACON_RETURN_MS = 8000;
inline constexpr uint32 ULDUAR_YOGG_SARON_BEACON_EXTERNAL_CLAIM_MS = 8000;
inline constexpr uint32 ULDUAR_YOGG_SARON_BEACON_PILE_RESET_GUARDIANS = 2;
inline constexpr float ULDUAR_YOGG_SARON_BEACON_TANK_EXTERNAL_PCT = 50.0f;

inline constexpr float ULDUAR_YOGG_SARON_BEACON_HEAL_RADIUS = 20.0f;
inline constexpr float ULDUAR_YOGG_SARON_BEACON_HEAL_SLACK = 5.0f;

inline constexpr int32 ULDUAR_YOGG_SARON_HEALING_CUT_PCT = -50;

inline constexpr float ULDUAR_YOGG_SARON_DRAIN_LIFE_HEALTH_PCT = 85.0f;

inline constexpr float ULDUAR_YOGG_SARON_TANK_SWAP_PCT = 50.0f;
inline constexpr float ULDUAR_YOGG_SARON_TANK_SWAP_TAKER_PCT = 70.0f;
inline constexpr uint32 ULDUAR_YOGG_SARON_TANK_SWAP_MIN_GUARDIANS = 3;
inline constexpr uint32 ULDUAR_YOGG_SARON_TANK_SWAP_MS = 8000;

inline constexpr float ULDUAR_YOGG_SARON_TANK_LOH_PCT = 20.0f;
inline constexpr uint32 ULDUAR_YOGG_SARON_TANK_LOH_CLAIM_MS = 3000;
inline constexpr float ULDUAR_YOGG_SARON_TANK_LOH_RANGE = 40.0f;

inline constexpr uint32 ULDUAR_YOGG_SARON_TANK_WRATH_LOCK_MS = 30000;

inline constexpr uint32 ULDUAR_YOGG_SARON_KICK_LAST_RESORT_MS = 1500;
inline constexpr uint32 ULDUAR_YOGG_SARON_KICK_WAIT_MS = 1000;
inline constexpr uint32 ULDUAR_YOGG_SARON_KICK_NUDGE_MS = 2000;
inline constexpr int32 ULDUAR_YOGG_SARON_KICK_MIN_LEFT_MS = 1000;
inline constexpr uint32 ULDUAR_YOGG_SARON_BEACON_SHOT_LEFT_MS = 5000;
inline constexpr uint32 ULDUAR_YOGG_SARON_BEACON_FEIGN_DELAY_MS = 1000;
inline constexpr float ULDUAR_YOGG_SARON_DISTRACTING_SHOT_RANGE = 34.0f;

inline constexpr uint32 ULDUAR_YOGG_SARON_KEEPER_COUNT = 4;
inline constexpr uint32 ULDUAR_YOGG_SARON_ROAR_FIRST_MS = 30000;
inline constexpr uint32 ULDUAR_YOGG_SARON_ROAR_PERIOD_MS = 60000;
inline constexpr uint32 ULDUAR_YOGG_SARON_ROAR_PRESWITCH_MS = 10000;
inline constexpr uint32 ULDUAR_YOGG_SARON_ROAR_HANDOFF_MS = 500;

inline constexpr float ULDUAR_YOGG_SARON_LUST_POSITION_SHARE = 0.8f;
inline constexpr uint32 ULDUAR_YOGG_SARON_LUST_FALLBACK_MS = 30000;

extern const std::vector<uint32> ULDUAR_YOGG_SARON_ILLUSION_MOBS;

extern const Position ULDUAR_YOGG_SARON_PORTAL_SPOTS[ULDUAR_YOGG_SARON_PORTAL_SPOTS_25MAN];

extern const Position ULDUAR_YOGG_SARON_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE;

inline constexpr uint8 ULDUAR_YOGG_SARON_ICECROWN_CLUSTER_COUNT = 3;
extern const Position ULDUAR_YOGG_SARON_ICECROWN_CLUSTERS[ULDUAR_YOGG_SARON_ICECROWN_CLUSTER_COUNT];

extern const Position ULDUAR_YOGG_SARON_ICECROWN_STAGE_SPOT;
extern const Position ULDUAR_YOGG_SARON_STORMWIND_STAGE_SPOT;

extern const Position ULDUAR_YOGG_SARON_BRAIN_MELEE_SPOT;

extern const Position ULDUAR_YOGG_SARON_BRAIN_WEST_SPOT;
extern const Position ULDUAR_YOGG_SARON_BRAIN_SOUTH_SPOT;
extern const Position ULDUAR_YOGG_SARON_BRAIN_NORTH_SPOT;
extern const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_P1_RANGED_SPOT;

extern const Position ULDUAR_YOGG_SARON_HANDOVER_MELEE_SPOT;
extern const Position ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT;
extern const Position ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT;

extern const std::vector<Position> ULDUAR_YOGG_SARON_PHASE_3_RANGED_STANDBY_SPOTS;

extern const Position ULDUAR_YOGG_SARON_BEACON_DRAG_NORTH;
extern const Position ULDUAR_YOGG_SARON_BEACON_DRAG_SOUTH;
extern const Position ULDUAR_YOGG_SARON_BEACON_DRAG_NORTHEAST;

extern const Position ULDUAR_YOGG_SARON_BEACON_HUNTER_NORTH;
extern const Position ULDUAR_YOGG_SARON_BEACON_HUNTER_SOUTH;

extern const Position ULDUAR_YOGG_SARON_BEACON_STAGE_NORTH;
extern const Position ULDUAR_YOGG_SARON_BEACON_STAGE_SOUTH;
extern const Position ULDUAR_YOGG_SARON_BEACON_STAGE_NORTHEAST;

uint32 YoggSaronPhase(PlayerbotAI* botAI);

Creature* YoggSaronNearestCreature(PlayerbotAI* botAI, uint32 entry);

bool YoggSaronEncounterActive(PlayerbotAI* botAI);
bool YoggSaronInPhase1(PlayerbotAI* botAI);
bool YoggSaronInPhase2(PlayerbotAI* botAI);
bool YoggSaronInPhase3(PlayerbotAI* botAI);

enum YoggSaronRoom : uint32
{
    YOGG_SARON_ROOM_NONE = 0,
    YOGG_SARON_ROOM_ARENA = 1,
    YOGG_SARON_ROOM_STORMWIND = 2,
    YOGG_SARON_ROOM_ICECROWN = 3,
    YOGG_SARON_ROOM_CHAMBER = 4,
    YOGG_SARON_ROOM_BRAIN = 5,
};

YoggSaronRoom YoggSaronRoomOf(Player* player);
bool YoggSaronOnBrainLevel(Player* player);

enum YoggSaronRoomState : uint32
{
    YOGG_SARON_ROOM_STATE_NONE = 0,
    YOGG_SARON_ROOM_STATE_WALKING_IN = 1,
    YOGG_SARON_ROOM_STATE_FIGHTING = 2,
    YOGG_SARON_ROOM_STATE_DOOR_SHUT = 3,
    YOGG_SARON_ROOM_STATE_TO_BRAIN = 4,
    YOGG_SARON_ROOM_STATE_AT_BRAIN = 5,
    YOGG_SARON_ROOM_STATE_REPRISAL = 6,
    YOGG_SARON_ROOM_STATE_STAGED = 7,
};

YoggSaronRoomState YoggSaronRoomStateOf(PlayerbotAI* botAI);

bool YoggSaronRoomMiddle(Player* player, Position& middle);

bool YoggSaronBrainRoomApproachable(PlayerbotAI* botAI);

bool YoggSaronShouldLeaveBrainLevel(PlayerbotAI* botAI);

bool YoggSaronLeavingBrainLevel(Player* bot);

uint32 YoggSaronCancelAbilityLock(Player* bot);

uint32 YoggSaronSanityOf(Player* player);

Position YoggSaronBrainSpot(Player* bot);

Position const& YoggSaronPhaseThreeSpot(Player* bot);

std::vector<Unit*> GetYoggSaronSkullsInArc(PlayerbotAI* botAI);

std::vector<Position> GetYoggSaronSkullsInRange(PlayerbotAI* botAI);

float YoggSaronHeadingGap(float from, float to);

uint32 YoggSaronSkullsAhead(std::vector<Position> const& skulls, float x, float y, float heading);

uint32 YoggSaronAngledSkullHeading(std::vector<Position> const& skulls, float x, float y, float targetBearing,
                                   float& heading);

bool YoggSaronFacingClearOfSkulls(std::vector<Position> const& skulls, float x, float y, float targetX,
                                  float targetY);

Player* YoggSaronSqueezeVictim(PlayerbotAI* botAI);

bool ClaimYoggSaronSqueezeRescue(PlayerbotAI* botAI, Player* victim);

Unit* YoggSaronPetFallbackTarget(Creature* pet);

Unit* YoggSaronPhase3PetLeftover(PlayerbotAI* botAI);

bool YoggSaronBeaconHealReaches(Unit* unit);

void YoggSaronNoteBrainLinkPair(Unit* owner, Unit* partner);

Player* YoggSaronBrainLinkTarget(PlayerbotAI* botAI);

struct YoggSaronPortalWave
{
    bool active = false;
    uint32 ordinal = 0;
    uint32 msToNextWave = 0;
    bool portalsUp = false;
};

YoggSaronPortalWave YoggSaronPortalWaveState(PlayerbotAI* botAI);

std::vector<Player*> GetYoggSaronBrainTeam(PlayerbotAI* botAI);

enum YoggSaronPortalIntent : uint32
{
    YOGG_SARON_PORTAL_NOT_TEAM = 0,
    YOGG_SARON_PORTAL_WAITING = 1,
    YOGG_SARON_PORTAL_SPREADING = 2,
    YOGG_SARON_PORTAL_HOLDING = 3,
    YOGG_SARON_PORTAL_LATE = 4,
};

YoggSaronPortalIntent YoggSaronPortalPlan(PlayerbotAI* botAI, Position& spot);

bool YoggSaronPortalWalkPending(PlayerbotAI* botAI);

bool YoggSaronHoldingPortalSpot(PlayerbotAI* botAI);

bool YoggSaronReprisalHold(PlayerbotAI* botAI);

Player* YoggSaronRoomHealer(PlayerbotAI* botAI);

std::vector<Unit*> GetYoggSaronInterruptCasters(PlayerbotAI* botAI);

bool YoggSaronMarkCaster(Unit* caster);

bool YoggSaronMarkKicker(Player* bot);

std::vector<char const*> YoggSaronInterruptSpells(Player* bot);
bool YoggSaronCanInterrupt(Player* bot);

std::vector<char const*> YoggSaronInterruptSpellsFor(Player* bot, Unit* caster);

bool YoggSaronInterruptReady(PlayerbotAI* botAI, char const* spell, Unit* caster);

bool YoggSaronInterruptClaimedByOther(Player* bot, Unit* caster);
void ClaimYoggSaronInterrupt(Player* bot, Unit* caster);

std::vector<Unit*> GetYoggSaronChannellingCrushers(PlayerbotAI* botAI);

std::vector<char const*> YoggSaronJudgementSpells(Player* bot);

std::vector<Unit*> GetYoggSaronNovaThreats(PlayerbotAI* botAI, float radius);

uint32 YoggSaronNovaDamageOn(Unit* target);

bool YoggSaronNovaWouldHurt(Unit* target);

uint32 YoggSaronNovaNeedMs(Unit* target, bool now);

bool YoggSaronNovaNearby(PlayerbotAI* botAI);

bool YoggSaronNovaHold(PlayerbotAI* botAI);

uint32 YoggSaronNovaHurtCount(PlayerbotAI* botAI);

bool IsYoggSaronShadowAuraPaladin(Player* bot);

bool YoggSaronShadowAuraHeld(PlayerbotAI* botAI);

bool YoggSaronShadowAuraOtherHeld(PlayerbotAI* botAI);

char const* YoggSaronNovaRaidCover(PlayerbotAI* botAI);

bool YoggSaronBattleRezHeld(PlayerbotAI* botAI);

Player* YoggSaronNovaVictim(PlayerbotAI* botAI);

bool ClaimYoggSaronNovaExternal(PlayerbotAI* botAI);
bool ClaimYoggSaronNovaWindow(Player* bot);

void AppendYoggSaronTargetExclusions(PlayerbotAI* botAI, GuidSet& exclusions, GuidVector const& candidates);

bool YoggSaronPhase1GuardianPreferred(Unit* candidate, Unit* incumbent);

bool YoggSaronGuardianCountsForSara(Unit* guardian);
bool YoggSaronGuardianOnTheStack(Unit* guardian);
bool YoggSaronPhase1TankPullSpot(PlayerbotAI* botAI, Position& spot);

bool YoggSaronInPhase1Room(Player* bot);

bool YoggSaronInPhase1Approach(Player* bot);

void YoggSaronNoteGuardianDeath(Unit* guardian);

bool YoggSaronGuardianDiedWithin(Player* bot, uint32 ms);

bool YoggSaronPhase1GuardianKillable(PlayerbotAI* botAI, Unit* guardian);

Unit* YoggSaronPhase1Focus(PlayerbotAI* botAI);

bool YoggSaronPhase1AoeHold(PlayerbotAI* botAI);

bool YoggSaronLeftoverGuardianKillable(PlayerbotAI* botAI, Unit* guardian);

bool YoggSaronLowGuardianTakesOver(Unit* candidate, Unit* current);

bool YoggSaronLeftoverAoeHold(PlayerbotAI* botAI);

Unit* YoggSaronPhase1TauntTarget(PlayerbotAI* botAI);

Unit* YoggSaronPhase1StackTarget(PlayerbotAI* botAI);

bool YoggSaronBotTankAlive(PlayerbotAI* botAI);

Position YoggSaronCloudLead(Creature* cloud, uint32 leadMs = ULDUAR_YOGG_SARON_CLOUD_LEAD_MS);

bool YoggSaronRouteClearOfClouds(Player* bot, std::vector<Position> const& clouds, float x, float y);

bool YoggSaronWalkCrossesCloud(Player* bot, Position const& to, Position const* exempt = nullptr,
                               float exemptRadius = 0.0f);

struct YoggSaronHandover
{
    bool active = false;
    bool clearing = false;
};

YoggSaronHandover YoggSaronHandoverState(PlayerbotAI* botAI);

void YoggSaronTick(PlayerbotAI* botAI);

bool YoggSaronInBodyKnockback(Player* player);
bool YoggSaronRouteClearOfBody(Player* bot, float x, float y);

bool YoggSaronRouteClearOfDeathRays(Player* bot, std::vector<Position> const& rays, float x, float y);

bool YoggSaronDeathRayNear(PlayerbotAI* botAI);

bool YoggSaronBodyDetour(Player* bot, Position const& destination, Position& waypoint);

Position YoggSaronBodyRoute(Player* bot, Position const& spot);

bool IsYoggSaronHoldableGuardian(Unit* unit);

Unit* YoggSaronBeaconDragGuardian(PlayerbotAI* botAI, bool claim);

void YoggSaronBeaconTaunted(Player* bot);

bool YoggSaronBeaconDragging(Player* bot);

Position const* YoggSaronBeaconStageSpot(Player* bot);

Position const& YoggSaronBeaconDragSpot(Player* bot);
Position const& YoggSaronBeaconHunterSpot(Player* bot);

Unit* YoggSaronBeaconReturning(Player* bot);

uint8 YoggSaronLooseGuardianTier(Player* bot, Unit* guardian);

Unit* YoggSaronHealedGuardianOn(PlayerbotAI* botAI, Player* victim);

Player* YoggSaronBeaconExternalVictim(PlayerbotAI* botAI);
bool ClaimYoggSaronBeaconExternal(PlayerbotAI* botAI, Player* victim);

uint32 YoggSaronGuardiansOn(Player* tank);

uint32 YoggSaronPileHealedOn(Player* tank);

Unit* YoggSaronMortalStrikeTarget(PlayerbotAI* botAI, char const*& reason);

char const* YoggSaronWarlockSummon(PlayerbotAI* botAI);

char const* YoggSaronPhase3YoggDot(PlayerbotAI* botAI, Unit*& yogg);

Player* YoggSaronTankSwapTake(PlayerbotAI* botAI);

bool YoggSaronTankShedding(PlayerbotAI* botAI);

bool YoggSaronTankLowHold(Player* bot);

bool YoggSaronTankWrathHeld(PlayerbotAI* botAI);

bool YoggSaronBeforePhase3(PlayerbotAI* botAI);

Player* YoggSaronTankLayOnHandsTarget(PlayerbotAI* botAI);
bool ClaimYoggSaronTankLayOnHands(PlayerbotAI* botAI, Player* tank);

bool YoggSaronHealerTargetless(PlayerbotAI* botAI);

bool YoggSaronBeaconImmunityHeld(PlayerbotAI* botAI);

bool YoggSaronBeaconHunterHold(PlayerbotAI* botAI);

bool YoggSaronBeaconBearHeld(PlayerbotAI* botAI);

bool YoggSaronBearReady(Player* druid);

bool YoggSaronRoarExpected(PlayerbotAI* botAI);
bool YoggSaronRoarCasting(PlayerbotAI* botAI);

Player* YoggSaronRoarPaladin(PlayerbotAI* botAI);

bool YoggSaronRoarAuraHeld(PlayerbotAI* botAI);

bool ClaimYoggSaronRoar(PlayerbotAI* botAI);

bool YoggSaronBrainHeld(PlayerbotAI* botAI);
bool YoggSaronBrainDotHeld(PlayerbotAI* botAI);

bool YoggSaronGazeActive(PlayerbotAI* botAI);
bool YoggSaronOutsideGaze(Player* bot, Unit* yogg, Unit* target);

bool YoggSaronPhase3LustReady(PlayerbotAI* botAI);
bool YoggSaronHoldableGuardianWithin(Player* bot, float radius);

GuidVector GetYoggSaronGuardiansInSight(PlayerbotAI* botAI);

bool YoggSaronGuardianLoose(PlayerbotAI* botAI);
bool YoggSaronGuardianLoose(PlayerbotAI* botAI, GuidVector const& guardians);

bool YoggSaronGuardianThreatAllows(Player* bot, Unit* guardian, bool holding);

float YoggSaronTankThreatLead(Player* tank, Unit* guardian);

bool YoggSaronConstrictorHolding(Unit* unit);

Unit* YoggSaronLiveIllusionMob(PlayerbotAI* botAI, float radius, Position const* centre = nullptr);

std::vector<Unit*> GetYoggSaronRoomIllusionMobs(PlayerbotAI* botAI);

bool YoggSaronIllusionTentacleAllowed(PlayerbotAI* botAI, Unit* tentacle);

bool YoggSaronIllusionStaged(PlayerbotAI* botAI);

bool YoggSaronIllusionStageSpot(Player* bot, Position& spot);

bool YoggSaronIllusionHealerStation(PlayerbotAI* botAI, Position& station);

bool YoggSaronInfluenceTentaclesCleared(PlayerbotAI* botAI);

bool YoggSaronPlatformIdle(PlayerbotAI* botAI);

std::vector<Position> GetYoggSaronCrushWedges(PlayerbotAI* botAI, float searchRadius);
bool InYoggSaronCrushWedge(std::vector<Position> const& wedges, float x, float y, float arcDegrees);

std::vector<Position> GetYoggSaronCrusherReaches(PlayerbotAI* botAI, float searchRadius);
bool YoggSaronInCrusherReach(PlayerbotAI* botAI, float radius);

bool YoggSaronCrusherStunHolds(PlayerbotAI* botAI, Unit* crusher);

bool YoggSaronInSwingingCrusherReach(PlayerbotAI* botAI, float slack);

bool YoggSaronWalkMakingProgress(PlayerbotAI* botAI, char const* node, Position const& destination);

void YoggSaronReleaseStalledWalk(PlayerbotAI* botAI, char const* node, Position const& destination);

bool YoggSaronFearWindowActive(PlayerbotAI* botAI);

#endif
