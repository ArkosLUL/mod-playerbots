/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERALGALON_H
#define PLAYERBOTS_ULDENCOUNTERALGALON_H

#include "EncounterHelpers.h"
#include "ObjectGuid.h"
#include "Position.h"
#include "RaidObs.h"
#include "UldData.h"

#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class Player;
class PlayerbotAI;
class Unit;

enum UlduarAlgalonIds
{
    PB_NPC_ALGALON = 32871,
    SPELL_ALGALON_COSMIC_SMASH_DAMAGE = 62311,
    SPELL_ALGALON_COSMIC_SMASH_DAMAGE_25 = 64596,
    PB_NPC_LIVING_CONSTELLATION = 33052,
    PB_NPC_COLLAPSING_STAR = 32955,
    PB_NPC_BLACK_HOLE = 32953,
    PB_NPC_WORM_HOLE = 34099,
    PB_NPC_UNLEASHED_DARK_MATTER = 34097,
    NPC_ALGALON_ASTEROID_TARGET_1 = 33104,
    NPC_ALGALON_ASTEROID_TARGET_2 = 33105,
    SPELL_ALGALON_BIG_BANG = 64443,
    SPELL_ALGALON_BIG_BANG_25 = 64584,
    SPELL_ALGALON_PHASE_PUNCH = 64412,
    SPELL_ALGALON_PAIN_SUPPRESSION = 33206,
    SPELL_ALGALON_GUARDIAN_SPIRIT = 47788,
    SPELL_ALGALON_HAND_OF_SACRIFICE = 6940,
    SPELL_ALGALON_DISPERSION = 47585,
    SPELL_ALGALON_FORBEARANCE = 25771,
    SPELL_ALGALON_WEAKENED_SOUL = 6788,
    SPELL_ALGALON_ICE_BLOCK = 45438,
    SPELL_ALGALON_DIVINE_SHIELD = 642,
    SPELL_ALGALON_CLOAK_OF_SHADOWS = 31224,
    SPELL_ALGALON_ANTI_MAGIC_SHELL = 48707,
    SPELL_ALGALON_COLLAPSE = 62018,
    SPELL_ALGALON_AURA_MASTERY = 31821,
    SPELL_ALGALON_DIVINE_SACRIFICE = 64205,
    SPELL_ALGALON_DIVINE_GUARDIAN = 70940,
    SPELL_ALGALON_DIVINE_GUARDIAN_RANK_1 = 53527,
    SPELL_ALGALON_DIVINE_GUARDIAN_RANK_2 = 53530,
    SPELL_ALGALON_EXPLOSIVE_SHOT_TICK = 53352,
};

inline constexpr uint32 ULDUAR_ALGALON_EXPLOSIVE_SHOT_FAMILY_FLAG_1 = 0x80000000;

inline constexpr float ULDUAR_ALGALON_ROOM_RADIUS = 47.0f;
inline constexpr float ULDUAR_ALGALON_MELEE_REACH = 11.83f;
inline constexpr float ULDUAR_ALGALON_ROOM_Z_MIN = 410.0f;
inline constexpr float ULDUAR_ALGALON_ROOM_Z_MAX = 425.0f;
inline constexpr float ULDUAR_ALGALON_SCAN_RADIUS = 70.0f;

inline constexpr float ULDUAR_ALGALON_SHELTER_RADIUS = 6.0f;
inline constexpr float ULDUAR_ALGALON_SHELTER_IN_RADIUS = ULDUAR_ALGALON_SHELTER_RADIUS - 1.5f;
inline constexpr int32 ULDUAR_ALGALON_FIELD_PULSE_MS = 1000;
inline constexpr float ULDUAR_ALGALON_HOLE_EXIT_RADIUS = 8.0f;
inline constexpr float ULDUAR_ALGALON_HOLE_PATH_CLEARANCE = 7.5f;
inline constexpr float ULDUAR_ALGALON_HOLE_DETOUR_RADIUS = 9.5f;
inline constexpr float ULDUAR_ALGALON_FIELD_PATH_CLEARANCE = 6.3f;
inline constexpr float ULDUAR_ALGALON_FIELD_STAND_CLEARANCE = 7.5f;

inline constexpr float ULDUAR_ALGALON_BLACK_HOLE_KITE_OFFSET = 8.0f;
inline constexpr float ULDUAR_ALGALON_KITE_MIN_OFFSET = 7.8f;
inline constexpr float ULDUAR_ALGALON_KITE_STAGE_OFFSET = 11.0f;
inline constexpr uint32 ULDUAR_ALGALON_KITE_WAIT_MS = 6000;
inline constexpr uint32 ULDUAR_ALGALON_KITE_RETRY_MS = 3000;
inline constexpr uint32 ULDUAR_ALGALON_KITE_STALL_MS = 12000;
inline constexpr float ULDUAR_ALGALON_KITE_BESIDE = 5.0f;
inline constexpr float ULDUAR_ALGALON_CONSTELLATION_EAT_RADIUS = 6.5f;
inline constexpr float ULDUAR_ALGALON_CONSTELLATION_LOW_Z = 7.5f;
inline constexpr float ULDUAR_ALGALON_KITE_SWING = static_cast<float>(M_PI) / 6.0f;
inline constexpr float ULDUAR_ALGALON_TAUNT_REACH = 28.0f;
inline constexpr float ULDUAR_ALGALON_TAUNT_SPOT_NEAR = 12.0f;
inline constexpr float ULDUAR_ALGALON_TAUNT_SPOT_FAR = 20.0f;
inline constexpr float ULDUAR_ALGALON_TAUNT_LINE_SLACK = 0.5f;
inline constexpr float ULDUAR_ALGALON_HANDLER_HOLE_CLEARANCE = 12.0f;
inline constexpr float ULDUAR_ALGALON_HANDLER_KEEP_MARGIN = 2.0f;
inline constexpr uint32 ULDUAR_ALGALON_TAUNT_COOLDOWN_MS = 8000;
inline constexpr uint32 ULDUAR_ALGALON_OFFTANK_TAUNT_SECONDS = 30;
inline constexpr float ULDUAR_ALGALON_TANK_SHELTER_REACH = 35.0f;
inline constexpr float ULDUAR_ALGALON_ROOM_CLAMP = 44.0f;

inline constexpr uint8 ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS = 4;
inline constexpr uint8 ULDUAR_ALGALON_EARLY_SWAP_STACKS = 3;
inline constexpr uint8 ULDUAR_ALGALON_EARLY_SWAP_PARTNER_STACKS = 1;
inline constexpr uint32 ULDUAR_ALGALON_EARLY_SWAP_SECONDS = 15;
inline constexpr uint8 ULDUAR_ALGALON_HANDOVER_PARTNER_STACKS = 2;

inline constexpr uint32 ULDUAR_ALGALON_FIRST_BIG_BANG_MS = 90000;
inline constexpr uint32 ULDUAR_ALGALON_BIG_BANG_INTERVAL_MS = 90500;
inline constexpr uint32 ULDUAR_ALGALON_BIG_BANG_CAST_MS = 8000;
inline constexpr uint32 ULDUAR_ALGALON_SHELTER_WINDOW_SECONDS = 30;
inline constexpr uint32 ULDUAR_ALGALON_KITE_RESERVE_SECONDS = 60;
inline constexpr uint8 ULDUAR_ALGALON_KITE_RESERVED_HOLES = 2;
inline constexpr uint32 ULDUAR_ALGALON_COOLDOWN_HOLD_SECONDS = 20;
inline constexpr uint32 ULDUAR_ALGALON_GATHER_LEAD_SECONDS = 10;
inline constexpr int32 ULDUAR_ALGALON_HIDE_MARGIN_MS = 2500;
inline constexpr int32 ULDUAR_ALGALON_HIDE_CAST_SLACK_MS = 250;
inline constexpr float ULDUAR_ALGALON_NITRO_MIN_WALK = 10.0f;
inline constexpr int32 ULDUAR_ALGALON_SOAK_DEFENSIVE_MS = 3000;
inline constexpr int32 ULDUAR_ALGALON_SOAK_DISPERSION_MS = 5400;
inline constexpr int32 ULDUAR_ALGALON_EXTERNAL_MIN_REMAINING_MS = 4000;
inline constexpr uint32 ULDUAR_ALGALON_RESCUE_WINDOW_MS = 4000;
inline constexpr float ULDUAR_ALGALON_RESCUE_HP_PCT = 60.0f;
inline constexpr float ULDUAR_ALGALON_RESCUE_LAY_ON_HANDS_PCT = 30.0f;
inline constexpr int32 ULDUAR_ALGALON_PREP_GCD_MS = 1500;
inline constexpr int32 ULDUAR_ALGALON_LAST_IN_MARGIN_MS = 1500;
inline constexpr int32 ULDUAR_ALGALON_HOLD_LEAVE_LEAD_MS = 500;
inline constexpr uint32 ULDUAR_ALGALON_BIG_BANG_STILL_MS = 3000;
inline constexpr float ULDUAR_ALGALON_TANK_DEFENSIVE_PCT = 60.0f;
inline constexpr float ULDUAR_ALGALON_TANK_LAY_ON_HANDS_PCT = 25.0f;
inline constexpr uint32 ULDUAR_ALGALON_TANK_WINDOW_LEAD_MS = 1000;
inline constexpr float ULDUAR_ALGALON_TANK_WINDOW_PCT = 80.0f;
inline constexpr uint32 ULDUAR_ALGALON_TANK_WINDOW_TAIL_MS = 5000;
inline constexpr uint32 ULDUAR_ALGALON_TANK_DEFENSIVE_CLAIM_MS = 1000;
inline constexpr float ULDUAR_ALGALON_SACRIFICE_MIN_PCT = 50.0f;
inline constexpr float ULDUAR_ALGALON_EXPLOSION_LOW_PCT = 3.0f;
inline constexpr uint32 ULDUAR_ALGALON_EXPLOSION_COVER_LEAD_MS = 3500;
inline constexpr float ULDUAR_ALGALON_EXPLOSION_COVER_HP_PCT = 5.0f;
inline constexpr uint32 ULDUAR_ALGALON_EXPLOSION_COVER_MS = 6000;
inline constexpr uint32 ULDUAR_ALGALON_EXPLOSION_COVER_MIN_UNDER = 2;
inline constexpr uint32 ULDUAR_ALGALON_EXPLOSION_PERSONAL_LEAD_MS = 1500;
inline constexpr float ULDUAR_ALGALON_COVERED_EXPLOSION_FACTOR = 0.8f;
inline constexpr uint32 ULDUAR_ALGALON_EXPLOSION_STACK_MS = 3000;
inline constexpr uint32 ULDUAR_ALGALON_EXPLOSION_STACK_MAX = 3;
inline constexpr uint32 ULDUAR_ALGALON_EXPLOSION_CONSUMABLE_HEAL = 3500;
inline constexpr float ULDUAR_ALGALON_BUTTON_HOLD_PCT = 35.0f;
inline constexpr float ULDUAR_ALGALON_AURA_RANGE = 40.0f;
inline constexpr uint8 ULDUAR_ALGALON_TANK_HEALERS_10 = 1;
inline constexpr uint8 ULDUAR_ALGALON_TANK_HEALERS_25 = 2;
inline constexpr float ULDUAR_ALGALON_TANK_CRITICAL_PCT = 50.0f;
inline constexpr float ULDUAR_ALGALON_HOLDER_URGENT_PCT = 70.0f;
inline constexpr float ULDUAR_ALGALON_TANK_HEALER_SELF_PCT = 50.0f;
inline constexpr float ULDUAR_ALGALON_HOLDER_TOPOFF_PCT = 95.0f;
inline constexpr float ULDUAR_ALGALON_OFFTANK_TOPOFF_PCT = 85.0f;
inline constexpr float ULDUAR_ALGALON_OFFTANK_MARGIN_PCT = 15.0f;
inline constexpr float ULDUAR_ALGALON_RUN_SPEED = 7.0f;
inline constexpr float ULDUAR_ALGALON_FAR_HOLE_YARDS = 35.0f;
inline constexpr float ULDUAR_ALGALON_NEAR_HOLE_STANDOFF = 30.0f;
inline constexpr uint32 ULDUAR_ALGALON_FAR_HOLE_SECONDS = 15;

inline constexpr int32 ULDUAR_ALGALON_UNDER_LEAD_MS = 4000;
inline constexpr int32 ULDUAR_ALGALON_UNDER_TAIL_MS = 600;
inline constexpr int32 ULDUAR_ALGALON_UNDER_HIDE_EXTRA_MS = 3000;
inline constexpr int32 ULDUAR_ALGALON_UNDER_HIDE_STRETCH_MS = 4000;
inline constexpr int32 ULDUAR_ALGALON_UNDER_DEATH_LEAD_MS = 1500;
inline constexpr int32 ULDUAR_ALGALON_UNDER_FINISH_MS = 5000;
inline constexpr int32 ULDUAR_ALGALON_UNDER_RELEASE_FROM_MS = 15000;
inline constexpr int32 ULDUAR_ALGALON_UNDER_RELEASE_TO_MS = 8000;
inline constexpr uint32 ULDUAR_ALGALON_UNDER_LATE_HOLD_MS = 1000;
inline constexpr uint32 ULDUAR_ALGALON_UNDER_SOAKER_GRACE_MS = 2000;
inline constexpr uint32 ULDUAR_ALGALON_UNDER_KEEP_MS = 1200;
inline constexpr int32 ULDUAR_ALGALON_UNDER_DYING_MS = 3000;
inline constexpr float ULDUAR_ALGALON_UNDER_TEAM_PCT_PER_SEC = 6.0f;
inline constexpr int32 ULDUAR_ALGALON_UNDER_SWITCH_MS = 3000;
inline constexpr int32 ULDUAR_ALGALON_UNDER_BULK_SLACK_MS = 35000;
inline constexpr uint32 ULDUAR_ALGALON_UNDER_FINISH_TICKS = 5;
inline constexpr float ULDUAR_ALGALON_UNDER_HOLE_REACH = 22.0f;

inline constexpr uint32 ULDUAR_ALGALON_STAR_PACING_GAP_MS = 8000;
inline constexpr uint32 ULDUAR_ALGALON_STAR_EXPLOSION_MAX_10 = 16913;
inline constexpr uint32 ULDUAR_ALGALON_STAR_EXPLOSION_MAX_25 = 21526;
inline constexpr uint32 ULDUAR_ALGALON_STAR_SERIAL_SECONDS = 16;
inline constexpr float ULDUAR_ALGALON_STAR_CLEAR_RADIUS = 7.0f;
inline constexpr float ULDUAR_ALGALON_STAR_TANK_CLEARANCE = 9.0f;
inline constexpr float ULDUAR_ALGALON_STAR_BOSS_CLEARANCE = 16.0f;
inline constexpr float ULDUAR_ALGALON_STAR_DYING_HP_PCT = 5.0f;
inline constexpr float ULDUAR_ALGALON_STAR_DYING_RADIUS = 8.0f;
inline constexpr float ULDUAR_ALGALON_STAR_DYING_HOLD_RADIUS = 12.0f;
inline constexpr float ULDUAR_ALGALON_STAR_DRAIN_SLACK_PCT = 3.0f;
inline constexpr float ULDUAR_ALGALON_STAR_TEAM_REACH = 28.0f;
inline constexpr float ULDUAR_ALGALON_STAR_TEAM_STANDOFF = 25.0f;
inline constexpr float ULDUAR_ALGALON_STAR_TEAM_HOLD_MIN = 12.0f;
inline constexpr uint8 ULDUAR_ALGALON_STAR_TEAM_10 = 2;
inline constexpr uint8 ULDUAR_ALGALON_STAR_TEAM_25 = 3;
inline constexpr uint8 ULDUAR_ALGALON_STAR_TEAM_EXTRA_25 = 2;
inline constexpr uint32 ULDUAR_ALGALON_STAR_TEAM_EXTRA_BIG_BANG = 2;
inline constexpr float ULDUAR_ALGALON_SPLASH_RADIUS = 15.0f;
inline constexpr float ULDUAR_ALGALON_SPLASH_PAST_REACH = 11.0f;
inline constexpr float ULDUAR_ALGALON_GROUND_SPLASH_RADIUS = 25.0f;

inline constexpr uint32 ULDUAR_ALGALON_STAR_LOG_MS = 4000;
inline constexpr uint32 ULDUAR_ALGALON_STAR_LOG_STEP_MS = 500;
inline constexpr uint32 ULDUAR_ALGALON_STAR_LOG_MIN_MS = 2000;
inline constexpr float ULDUAR_ALGALON_STAR_DRAIN_PCT_PER_SEC = 1.0f;
inline constexpr float ULDUAR_ALGALON_STAR_DROPPED_PCT_PER_SEC = 5.0f;
inline constexpr uint32 ULDUAR_ALGALON_STAR_DROP_CARRY_MS = 4000;
inline constexpr uint32 ULDUAR_ALGALON_STAR_KILL_START_MS = 2000;
inline constexpr float ULDUAR_ALGALON_STAR_KILL_SLOW_PCT_PER_SEC = 5.5f;
inline constexpr float ULDUAR_ALGALON_STAR_KILL_FAST_PCT_PER_SEC = 9.0f;
inline constexpr float ULDUAR_ALGALON_STAR_KILL_MEASURED_PCT_PER_SEC = 3.5f;
inline constexpr float ULDUAR_ALGALON_STAR_KILL_PEAK_PCT_PER_SEC = 12.0f;
inline constexpr uint32 ULDUAR_ALGALON_STAR_DUE_HORIZON_MS = 30000;
inline constexpr uint32 ULDUAR_ALGALON_STAR_KEEP_GAP_MS = 6000;
inline constexpr uint32 ULDUAR_ALGALON_STAR_BIG_BANG_LEAD_MS = 1000;
inline constexpr uint32 ULDUAR_ALGALON_STAR_BIG_BANG_TAIL_MS = 3000;
inline constexpr uint32 ULDUAR_ALGALON_STAR_VOID_RECOVERY_MS = 16000;
inline constexpr uint32 ULDUAR_ALGALON_COSMIC_SMASH_IMPACT_MS = 4800;
inline constexpr uint32 ULDUAR_ALGALON_STAR_IMPACT_LEAD_MS = 4000;
inline constexpr uint32 ULDUAR_ALGALON_STAR_IMPACT_TAIL_MS = 5000;
inline constexpr uint32 ULDUAR_ALGALON_STAR_SMASH_RECOVERY_MS = 10000;
inline constexpr uint32 ULDUAR_ALGALON_FIRST_SMASH_IMPACT_MS = 30900;
inline constexpr uint32 ULDUAR_ALGALON_COSMIC_SMASH_INTERVAL_MS = 25500;
inline constexpr uint8 ULDUAR_ALGALON_SMASH_FORECAST = 3;
inline constexpr uint32 ULDUAR_ALGALON_STAR_DYING_MS = 4000;

inline constexpr float ULDUAR_ALGALON_COSMIC_SMASH_TRIGGER_RADIUS = 15.0f;
inline constexpr float ULDUAR_ALGALON_COSMIC_SMASH_CLEARANCE = 15.0f;
inline constexpr float ULDUAR_ALGALON_COSMIC_SMASH_MIN_CLEARANCE = 10.0f;
inline constexpr int32 ULDUAR_ALGALON_COSMIC_SMASH_IMPACT_SPREAD_MS = 300;
inline constexpr float ULDUAR_ALGALON_SMASH_DETOUR_RADIUS = 12.0f;
inline constexpr int32 ULDUAR_ALGALON_SMASH_IN_LEAD_MS = 500;
inline constexpr float ULDUAR_ALGALON_COSMIC_SMASH_HOLD_RADIUS = 19.0f;
inline constexpr float ULDUAR_ALGALON_DODGE_SEARCH_RADIUS = 24.0f;
inline constexpr uint32 ULDUAR_ALGALON_MARKER_LIFETIME_MS = 5500;
inline constexpr float ULDUAR_ALGALON_SWEEP_STEP = 2.0f;
inline constexpr float ULDUAR_ALGALON_SWEEP_ANGLE_STEP = static_cast<float>(M_PI) / 8.0f;
inline constexpr uint8 ULDUAR_ALGALON_SWEEP_BEARINGS = 16;
inline constexpr float ULDUAR_ALGALON_AREA_TAUNT_RANGE = 10.0f;

inline constexpr uint8 ULDUAR_ALGALON_HEALER_SLOTS = 4;
inline constexpr uint8 ULDUAR_ALGALON_RANGED_INNER_SLOTS = 6;
inline constexpr uint8 ULDUAR_ALGALON_RANGED_OUTER_SLOTS = 8;
inline constexpr uint8 ULDUAR_ALGALON_TOTAL_SLOTS =
    ULDUAR_ALGALON_HEALER_SLOTS + ULDUAR_ALGALON_RANGED_INNER_SLOTS + ULDUAR_ALGALON_RANGED_OUTER_SLOTS;
inline constexpr float ULDUAR_ALGALON_SLOT_TOLERANCE = 2.0f;
inline constexpr float ULDUAR_ALGALON_FORMATION_CLEARANCE = 13.5f;
inline constexpr float ULDUAR_ALGALON_HOLDER_ON_SPOT_RADIUS = 3.0f;
inline constexpr float ULDUAR_ALGALON_OFFTANK_STEP = 7.0f;
inline constexpr uint8 ULDUAR_ALGALON_MELEE_STAGE_SPOTS = 10;
inline constexpr uint32 ULDUAR_ALGALON_MELEE_RELEASE_MS = 8000;
inline constexpr uint32 ULDUAR_ALGALON_MELEE_RELEASE_ANYWAY_MS = 20000;
inline constexpr float ULDUAR_ALGALON_MELEE_SPOT_INSET = 3.0f;
inline constexpr uint32 ULDUAR_ALGALON_OPENER_MS = 20000;
inline constexpr float ULDUAR_ALGALON_TRICKS_RANGE = 20.0f;
inline constexpr float ULDUAR_ALGALON_MISDIRECTION_RANGE = 100.0f;
inline constexpr uint32 ULDUAR_ALGALON_PULL_HOLD_MS = 3000;
inline constexpr uint32 ULDUAR_ALGALON_TANK_CALL_MS = 5000;
inline constexpr int32 ULDUAR_ALGALON_HIDE_CALL_MS = 3000;
inline constexpr int32 ULDUAR_ALGALON_HUMAN_SOAK_CALL_MS = 4000;
inline constexpr float ULDUAR_ALGALON_SLOT_DISPLACE_RADIUS = 12.0f;

inline constexpr uint32 ULDUAR_ALGALON_STATE_TICK_MS = 250;
inline constexpr uint32 ULDUAR_ALGALON_STALL_MS = 500;

extern const Position ULDUAR_ALGALON_ROOM_CENTER;
extern const Position ULDUAR_ALGALON_PICKUP_SLOT;
extern const Position ULDUAR_ALGALON_PULL_OFFTANK_SLOT;
extern const Position ULDUAR_ALGALON_TANK_SLOT;
extern const Position ULDUAR_ALGALON_HOLD_SPOT;
extern const Position ULDUAR_ALGALON_OFFTANK_SLOT;
extern const std::array<Position, ULDUAR_ALGALON_TOTAL_SLOTS> ULDUAR_ALGALON_RING_SLOTS;
extern const std::array<Position, ULDUAR_ALGALON_MELEE_STAGE_SPOTS> ULDUAR_ALGALON_MELEE_STAGE_SLOTS;

// algalon.phase writes these numbers and algalon.py's PHASE_NAMES maps them back, renumber both together.
enum class AlgalonPhase : uint32
{
    Idle = 0,
    Intro = 1,
    One = 2,
    Two = 3,
    Won = 4,
};

enum class AlgalonKiteStage : uint8
{
    Stage,
    Wait,
    Cross,
    Parked,
};

enum class AlgalonTauntLine : uint8
{
    Clear,
    Side,
    Blocked,
};

struct AlgalonKite
{
    AlgalonKiteStage stage = AlgalonKiteStage::Stage;
    uint32 sinceMs = 0;
    Position spot;
    bool hasSpot = false;
    ObjectGuid looseTarget;
};

struct AlgalonScanUnit
{
    ObjectGuid guid;
    Position position;
    ObjectGuid victim;
    float healthPct = 0.0f;
    bool active = false;
    uint32 firstSeenMs = 0;
    float lossPctPerSec = ULDUAR_ALGALON_STAR_DRAIN_PCT_PER_SEC;
    bool lossMeasured = false;
    uint32 health = 0;
    uint32 pendingHp = 0;
    uint32 tickHp = 0;
    int32 nextTickMs = 0;
    bool under = false;
    uint32 underLo = 0;
    uint32 underHi = 0;
    bool underDying = false;
};

struct AlgalonEncounterState
{
    RaidObs::ObsGuidMap<uint8> slotAssignments{"algalon.slot"};
    RaidObs::ObsGuidMap<ObjectGuid> shelterAssignments{"algalon.shelter"};
    RaidObs::ObsGuidMap<ObjectGuid> kiteHoleAssignments{"algalon.kitehole"};

    RaidObs::ObsValue<AlgalonPhase> phase{"algalon.phase"};
    RaidObs::ObsValue<uint32> bigBang{"algalon.bigbang"};
    RaidObs::ObsValue<ObjectGuid> bigBangSoaker{"algalon.soaker"};
    RaidObs::ObsValue<ObjectGuid> bigBangBackup{"algalon.backup"};
    RaidObs::ObsValue<ObjectGuid> disperser{"algalon.disperser"};
    RaidObs::ObsValue<ObjectGuid> shadowAuraPaladin{"algalon.shadowaura"};
    RaidObs::ObsValue<ObjectGuid> fireTotemShaman{"algalon.firetotem"};
    RaidObs::ObsValue<bool> urgent{"algalon.urgent"};
    RaidObs::ObsValue<uint32> holeCount{"algalon.holes"};
    RaidObs::ObsValue<ObjectGuid> focusStar{"algalon.focusstar"};
    RaidObs::ObsValue<ObjectGuid> finishStar{"algalon.finishstar"};
    RaidObs::ObsValue<ObjectGuid> finisher{"algalon.finisher"};
    RaidObs::ObsGuidSet starTeam{"algalon.starteam"};
    RaidObs::ObsValue<ObjectGuid> handler{"algalon.handler"};
    RaidObs::ObsGuidSet swapPair{"algalon.swappair"};
    RaidObs::ObsGuidSet tankHealers{"algalon.tankhealer"};
    RaidObs::ObsValue<uint32> tankHealerSlots{"algalon.tankhealslots"};

    ObjectGuid boss;
    bool engaged = false;
    bool engagedSeen = false;
    bool meleeReleased = false;
    uint32 introEndMs = 0;
    ObjectGuid firstHolder;
    uint32 firstHoldMs = 0;
    uint32 lastTankCallMs = 0;
    bool bigBangHideCalled = false;
    bool humanSoakCalled = false;
    uint32 nextBigBangMs = 0;
    uint32 bigBangCount = 0;
    bool bigBangCasting = false;
    uint32 lastBigBangEndMs = 0;
    ObjectGuid lastSoaker;
    uint32 handoverNoted = 0;
    std::unordered_map<ObjectGuid, uint8> hidersOut;
    uint32 tankCutClaimMs = 0;
    uint32 layOnHandsClaimMs = 0;
    ObjectGuid coverStar;
    uint32 coverClaimMs = 0;
    std::unordered_map<ObjectGuid, ObjectGuid> personalStars;
    std::unordered_map<ObjectGuid, ObjectGuid> personalBlocks;
    std::unordered_set<ObjectGuid> nitroRunners;
    std::unordered_set<ObjectGuid> underStars;
    uint32 underFromMs = 0;
    uint32 underToMs = 0;
    uint32 underPlanCycle = 0;
    std::unordered_set<ObjectGuid> underReleased;
    std::unordered_map<ObjectGuid, uint32> underLateMs;
    uint32 underNoSoakMs = 0;
    ObjectGuid bulkStar;
    ObjectGuid finishLatch;
    ObjectGuid finisherLatch;
    ObjectGuid shelterStar;
    std::unordered_set<ObjectGuid> starTeamExtras;
    float bulkPctPerSec = 0.0f;
    uint32 bulkTeam = 0;
    uint32 bulkMeasuredCycle = 0;
    uint32 focusSinceMs = 0;
    ObjectGuid bulkFrom;
    uint32 bulkFromMs = 0;
    float bulkFromPct = 0.0f;
    uint32 bulkFromTeam = 0;
    uint32 lastSmashImpactMs = 0;

    std::vector<AlgalonScanUnit> stars;
    std::vector<AlgalonScanUnit> holes;
    std::vector<AlgalonScanUnit> constellations;
    std::vector<AlgalonScanUnit> markers;
    std::vector<AlgalonScanUnit> darkMatter;
    std::unordered_map<ObjectGuid, uint32> markerSeenMs;
    std::unordered_map<ObjectGuid, uint32> starSeenMs;
    std::unordered_map<ObjectGuid, std::vector<std::pair<uint32, float>>> starHealthLog;
    std::unordered_map<ObjectGuid, std::pair<uint32, float>> starLeft;
    float teamKillPctPerSec = 0.0f;
    uint32 teamKillTeam = 0;
    uint32 nextSmashImpactMs = 0;
    std::string lastStarWindow;
    bool focusByDeadline = false;
    std::unordered_map<ObjectGuid, AlgalonKite> kites;
    std::unordered_set<ObjectGuid> kiteRefusedHoles;
    std::unordered_map<ObjectGuid, std::vector<uint32>> petSplashHeld;
    bool wormHolesSeen = false;

    uint32 lastStarDeathMs = 0;
    uint32 lastTickMs = 0;
};

Unit* GetAlgalon(PlayerbotAI* botAI);
bool AlgalonInRoom(Player* bot);
bool AlgalonPresent(PlayerbotAI* botAI);
bool AlgalonEngaged(PlayerbotAI* botAI);
bool AlgalonInPhaseOne(PlayerbotAI* botAI);
bool IsAlgalonPhased(Unit const* unit);
bool AlgalonInOpener(PlayerbotAI* botAI);
Player* GetAlgalonOpenerTank(PlayerbotAI* botAI);
char const* AlgalonThreatRedirectSpell(Player* bot);

void AlgalonTickEncounterState(PlayerbotAI* botAI);

bool AlgalonBigBangCasting(PlayerbotAI* botAI);
int32 AlgalonBigBangRemainingMs(PlayerbotAI* botAI);
bool AlgalonBigBangWithin(PlayerbotAI* botAI, uint32 seconds);
bool AlgalonBigBangLatched(Player* bot);
int32 AlgalonMsToBigBangHit(PlayerbotAI* botAI);
Player* GetAlgalonRescueTarget(PlayerbotAI* botAI);
char const* GetAlgalonSoakerExternal(PlayerbotAI* botAI, Player* soaker);
char const* GetAlgalonSoakerPrep(PlayerbotAI* botAI, Player* soaker);
char const* GetAlgalonRescueSpell(PlayerbotAI* botAI, Player* soaker);

bool IsAlgalonTankHealer(Player* bot);
struct AlgalonTankHealPick
{
    Player* target = nullptr;
    bool critical = false;
};
AlgalonTankHealPick GetAlgalonTankHealPick(PlayerbotAI* botAI);
char const* GetAlgalonTankHealSpell(PlayerbotAI* botAI, AlgalonTankHealPick const& pick);
Player* GetAlgalonBeaconTarget(PlayerbotAI* botAI);

bool AlgalonDispersionSoak(Player* bot);

struct AlgalonTankCut
{
    Player* target = nullptr;
    uint32 spellId = 0;
    char const* why = nullptr;
};
AlgalonTankCut GetAlgalonTankCut(PlayerbotAI* botAI);
void ClaimAlgalonTankCut(Player* bot, AlgalonTankCut const& cut);

char const* GetAlgalonExplosionCover(PlayerbotAI* botAI);
void ClaimAlgalonExplosionCover(Player* bot, char const* cover);
bool AlgalonUnderStarNear(Player* bot, float radius);
struct AlgalonPersonalPick
{
    uint32 spellId = 0;
    char const* consumable = nullptr;
    explicit operator bool() const { return spellId || consumable; }
};
AlgalonPersonalPick GetAlgalonExplosionPersonal(PlayerbotAI* botAI);
void ClaimAlgalonExplosionPersonal(Player* bot, AlgalonPersonalPick const& pick);
bool IsAlgalonShadowAuraPaladin(Player* bot);
bool IsAlgalonFireTotemShaman(Player* bot);

enum class AlgalonHideRole : uint8
{
    None,
    Soak,
    Backup,
    Run,
    In,
    Exit,
    NoShelter,
    Wait,
    Hold,
};
AlgalonHideRole GetAlgalonHideRole(Player* bot);
bool AlgalonShouldRunForShelter(Player* bot);
int32 AlgalonHideSlackMs(Player* bot);
void AlgalonBoostHideRun(Player* bot, float walk);
bool AlgalonStaysOut(Player* bot);
bool AlgalonTankCaughtOut(Player* bot);
bool AlgalonHoldsConstellation(Player* bot);
bool AlgalonCanDisperse(Player* bot);

Player* GetAlgalonBigBangSoaker(PlayerbotAI* botAI);
Player* GetAlgalonBigBangBackup(PlayerbotAI* botAI);
bool GetAlgalonShelter(Player* bot, Position& shelter);
bool AlgalonHideRoute(Player* bot, Position const& shelter, Position& next);
bool AlgalonHoleNear(Player* bot, float radius, Position* hole = nullptr);

Player* GetAlgalonBossTank(PlayerbotAI* botAI);
bool IsAlgalonSwapTank(Player* player);
uint8 GetAlgalonPhasePunchStacks(Player* player);
Player* GetAlgalonPickupTank(PlayerbotAI* botAI);
bool AlgalonShouldPickUp(Player* bot);
bool AlgalonShouldSwap(Player* bot);
bool AlgalonTakesBossNow(Player* bot);
bool AlgalonYieldsBoss(Player* bot);
Player* AlgalonLoneHolder(PlayerbotAI* botAI);
bool AlgalonOffTankWorking(Player* bot);

Player* GetAlgalonHandler(PlayerbotAI* botAI);
bool IsAlgalonHandler(Player* bot);
Unit* GetAlgalonHandlerConstellation(Player* bot);
Unit* GetAlgalonOffTankConstellation(Player* bot);
bool GetAlgalonKiteSpot(Player* bot, Unit* constellation, Position& spot, char const*& stage);
bool AlgalonKiteBehind(Player* bot, Unit* constellation, Position& spot);
bool AlgalonKitingBehind(Player* bot, Unit* constellation);
AlgalonTauntLine GetAlgalonTauntLine(Player* bot, Unit* constellation, Position& spot);
bool GetAlgalonKeepSpot(Player* bot, Position& spot);
Unit* GetAlgalonLooseDarkMatter(Player* bot);

bool IsAlgalonStarTeam(Player* bot);
Unit* GetAlgalonFocusStar(PlayerbotAI* botAI);
Unit* GetAlgalonStarTarget(PlayerbotAI* botAI);
uint8 AlgalonAliveStarCount(PlayerbotAI* botAI);

bool AlgalonCosmicSmashThreatens(Player* bot);
bool AlgalonDyingStarThreatens(Player* bot);
bool AlgalonCosmicSmashMarkerNear(PlayerbotAI* botAI, Position const& spot, float radius);
bool AlgalonSmashHoldsGround(Player* bot);
std::vector<EncounterHelpers::HazardCircle> GetAlgalonDodgeHazards(Player* bot, float markerClearance);
bool AlgalonSpotInRoom(float x, float y);
bool AlgalonPathCrossesHole(Player* bot, float x, float y, float clearance = ULDUAR_ALGALON_HOLE_PATH_CLEARANCE);
bool AlgalonWalkEntersField(Player* bot, float x, float y,
                            float standClearance = ULDUAR_ALGALON_FIELD_STAND_CLEARANCE);
bool AlgalonWalkAround(Player* bot, Position const& to, Position& next);
bool AlgalonPhasedInField(Player* bot);
bool AlgalonStarWithin(PlayerbotAI* botAI, Position const& spot, float radius, ObjectGuid const& except);
bool AlgalonHeadsForFarHole(Player* bot);

bool AlgalonTakesRingSlot(Player* bot);
bool AlgalonMeleeStaged(Player* bot);
bool TryGetAlgalonSlotPosition(uint8 slotIndex, Position& position);
bool TryGetAlgalonSlot(Player* bot, Position& position, bool* waypoint = nullptr);
bool GetAlgalonSlotAnchor(Player* bot, Position& slot);
bool GetAlgalonDodgeAnchor(Player* bot, Position& anchor);
bool AlgalonHiderInCast(Player* bot);

void AppendAlgalonTargetExclusions(PlayerbotAI* botAI, GuidSet& exclusions);
bool AlgalonMayDamage(Player* bot, Unit* target);

#endif
