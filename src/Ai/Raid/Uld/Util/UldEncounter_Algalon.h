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

// Algalon the Observer.

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
    // Externals put on the Big Bang soaker, matched by aura so two healers don't double up.
    SPELL_ALGALON_PAIN_SUPPRESSION = 33206,
    SPELL_ALGALON_GUARDIAN_SPIRIT = 47788,
    SPELL_ALGALON_HAND_OF_SACRIFICE = 6940,
    SPELL_ALGALON_DISPERSION = 47585,
    SPELL_ALGALON_FORBEARANCE = 25771,
    SPELL_ALGALON_WEAKENED_SOUL = 6788,
    SPELL_ALGALON_ICE_BLOCK = 45438,
};

// He casts Ascend the moment he leaves a 47 yd disc around home or drops under Z 410.
constexpr float ULDUAR_ALGALON_ROOM_RADIUS = 47.0f;
// His combat reach is 9 (model 28641), so his swing lands at 9 + 1.5 + 4/3 on a player.
constexpr float ULDUAR_ALGALON_MELEE_REACH = 11.83f;
constexpr float ULDUAR_ALGALON_ROOM_Z_MIN = 410.0f;
constexpr float ULDUAR_ALGALON_ROOM_Z_MAX = 425.0f;
// Constellations spawn up to 67 yd out from home, well past the room edge.
constexpr float ULDUAR_ALGALON_SCAN_RADIUS = 70.0f;

// Black Hole (62168) and Worm Hole (65250) phase anyone inside 6 yd for 10s, no target cap.
constexpr float ULDUAR_ALGALON_SHELTER_RADIUS = 6.0f;
// Big Bang's 64445 strips every phase a second after the hit and the field pulses every second, so a
// bot still inside it then is phased again for 10s. Exits clear it with room to spare.
constexpr float ULDUAR_ALGALON_HOLE_EXIT_RADIUS = 8.0f;
// A walk whose straight line passes this close to a hole clips its field and phases the walker into
// the void Dark Matter. A blocked walk detours via a point this far round the hole.
constexpr float ULDUAR_ALGALON_HOLE_PATH_CLEARANCE = 7.5f;
constexpr float ULDUAR_ALGALON_HOLE_DETOUR_RADIUS = 9.5f;
// Just outside the 6 yd field: the last path rule before a dodge or exit gives up on paths altogether,
// and what the walk guard holds a generic mover to.
constexpr float ULDUAR_ALGALON_FIELD_PATH_CLEARANCE = 6.3f;
// A generic mover may not stop closer than this to a hole: the bot would stand on the field's edge.
constexpr float ULDUAR_ALGALON_FIELD_STAND_CLEARANCE = 7.5f;

// The handler parks this far past the hole on the far side, so the chase drags the constellation
// through the field while the handler stays outside it.
constexpr float ULDUAR_ALGALON_BLACK_HOLE_KITE_OFFSET = 9.0f;
// Park points and dodges stay this far inside the room edge.
constexpr float ULDUAR_ALGALON_ROOM_CLAMP = 44.0f;

// Phase Punch is a 45s aura refreshed every 15.5s and the 5th stack phases the tank out. Swapping at
// 4 leaves ~17s of margin before either tank is stuck unable to take him back.
constexpr uint8 ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS = 4;
// The Phase Punch queued behind the cast lands on the soaker while everyone else is still phased, and a
// 5th stack then leaves nobody unphased, which resets him. So a holder deep in stacks hands him over first.
constexpr uint8 ULDUAR_ALGALON_EARLY_SWAP_STACKS = 3;
constexpr uint8 ULDUAR_ALGALON_EARLY_SWAP_PARTNER_STACKS = 1;
constexpr uint32 ULDUAR_ALGALON_EARLY_SWAP_SECONDS = 15;
// A holder with no physical defensive left hands him over before the cast to a partner who has one, as long
// as the partner can still take the Phase Punch behind it.
constexpr uint8 ULDUAR_ALGALON_HANDOVER_PARTNER_STACKS = 2;

// First cast 90s after the intro ends, then every 90.5s. The prediction is re-latched on every cast.
constexpr uint32 ULDUAR_ALGALON_FIRST_BIG_BANG_MS = 90000;
constexpr uint32 ULDUAR_ALGALON_BIG_BANG_INTERVAL_MS = 90500;
constexpr uint32 ULDUAR_ALGALON_BIG_BANG_CAST_MS = 8000;
// How early the raid needs a hole standing, and keeps the last one out of the kite.
constexpr uint32 ULDUAR_ALGALON_SHELTER_WINDOW_SECONDS = 30;
// Ice Block stays unspent this long before a cast: it pins the mage for 10 s, and Big Bang goes through it.
constexpr uint32 ULDUAR_ALGALON_COOLDOWN_HOLD_SECONDS = 20;
// Hiders keep fighting until their walk plus this margin is all that's left of the cast. The phase only
// has to cover the hit, and 64445 brings everyone back a second after it, so hiding early buys nothing.
constexpr int32 ULDUAR_ALGALON_HIDE_MARGIN_MS = 2500;
// Cast remaining when the soaker's defensive goes up, so it also covers his swings until the raid is back.
constexpr int32 ULDUAR_ALGALON_SOAK_DEFENSIVE_MS = 3000;
// Dispersion only lasts 6s.
constexpr int32 ULDUAR_ALGALON_SOAK_DISPERSION_MS = 5000;
// An external needs the priest to still reach a hole after the GCD.
constexpr int32 ULDUAR_ALGALON_EXTERNAL_MIN_REMAINING_MS = 4000;
// The hit takes the soaker to ~20% and the raid is back a second later, so the healers' first cast
// after it decides whether he lives. They get this long to land it.
constexpr uint32 ULDUAR_ALGALON_RESCUE_WINDOW_MS = 4000;
constexpr float ULDUAR_ALGALON_RESCUE_HP_PCT = 60.0f;
constexpr float ULDUAR_ALGALON_RESCUE_LAY_ON_HANDS_PCT = 30.0f;
// A healer still has one global cooldown to spend on the soaker before its walk to the hole.
constexpr int32 ULDUAR_ALGALON_PREP_GCD_MS = 1500;
// Run speed for walk estimates made for someone else, without their speed to hand.
constexpr float ULDUAR_ALGALON_RUN_SPEED = 7.0f;
// A hole further than this can't be reached in the cast, so the bot walks towards it beforehand, to this
// far from it, from this long before the cast.
constexpr float ULDUAR_ALGALON_FAR_HOLE_YARDS = 35.0f;
constexpr float ULDUAR_ALGALON_NEAR_HOLE_STANDOFF = 30.0f;
constexpr uint32 ULDUAR_ALGALON_FAR_HOLE_SECONDS = 15;

// Collapsing Star pacing. Each death is Black Hole Explosion on everyone unphased (64122 up to 16,912,
// 65108 up to 21,525), and Collapse drains 1% max health a second, so four ignored stars explode within
// seconds of each other.
constexpr uint32 ULDUAR_ALGALON_STAR_PACING_GAP_MS = 8000;
constexpr uint32 ULDUAR_ALGALON_STAR_EXPLOSION_MAX_10 = 16913;
constexpr uint32 ULDUAR_ALGALON_STAR_EXPLOSION_MAX_25 = 21526;
// One gap plus a kill, per star still to go off.
constexpr uint32 ULDUAR_ALGALON_STAR_SERIAL_SECONDS = 16;
// The black hole lands where the star dies and phases everyone in its field.
constexpr float ULDUAR_ALGALON_STAR_CLEAR_RADIUS = 7.0f;
// A hole under a swap tank phases it and sets him loose; one inside his melee ring phases the melee.
constexpr float ULDUAR_ALGALON_STAR_TANK_CLEARANCE = 9.0f;
constexpr float ULDUAR_ALGALON_STAR_BOSS_CLEARANCE = 16.0f;
// A star this low dies within seconds whoever is on it, and everyone in its field is phased.
constexpr float ULDUAR_ALGALON_STAR_DYING_HP_PCT = 5.0f;
constexpr float ULDUAR_ALGALON_STAR_DYING_RADIUS = 8.0f;
// Inside this the generic movers are held until it dies, or reach walks a bot that stepped off straight back.
constexpr float ULDUAR_ALGALON_STAR_DYING_HOLD_RADIUS = 12.0f;
// A star dead within this much of its drain-only health drained out; anything lower was hit.
constexpr float ULDUAR_ALGALON_STAR_DRAIN_SLACK_PCT = 3.0f;
// A ranged team member further than this from the focus star walks to this far from it.
constexpr float ULDUAR_ALGALON_STAR_TEAM_REACH = 28.0f;
constexpr float ULDUAR_ALGALON_STAR_TEAM_STANDOFF = 25.0f;
constexpr uint8 ULDUAR_ALGALON_STAR_TEAM_10 = 2;
constexpr uint8 ULDUAR_ALGALON_STAR_TEAM_25 = 3;
// Splash that would also land on a star this close to the bot or its target is held. 15 is glyphed
// Pestilence; diseases landed on a star 12.5 yd out.
constexpr float ULDUAR_ALGALON_SPLASH_RADIUS = 15.0f;
// Ground AoE sits there 8-10 s and stars wander ~1.7 yd/s, so it's held while one is this close to where it lands.
constexpr float ULDUAR_ALGALON_GROUND_SPLASH_RADIUS = 25.0f;

// Star death forecast. Splash near him takes 1-2.4%/s on top of the drain, so a star's health isn't its
// seconds left; the slope of its health log is.
constexpr uint32 ULDUAR_ALGALON_STAR_LOG_MS = 4000;
constexpr uint32 ULDUAR_ALGALON_STAR_LOG_STEP_MS = 500;
// Less log than this reads as the drain: a slope over one or two samples is noise.
constexpr uint32 ULDUAR_ALGALON_STAR_LOG_MIN_MS = 2000;
constexpr float ULDUAR_ALGALON_STAR_DRAIN_PCT_PER_SEC = 1.0f;
// A star the team drops part-killed keeps dying from casts in flight, channels and DoTs (5.8%/s measured).
constexpr float ULDUAR_ALGALON_STAR_DROPPED_PCT_PER_SEC = 5.0f;
// The team's kill speed over a whole kill, slowest and fastest (6.2-7.8%/s measured on 25-man).
constexpr float ULDUAR_ALGALON_STAR_KILL_SLOW_PCT_PER_SEC = 5.5f;
constexpr float ULDUAR_ALGALON_STAR_KILL_FAST_PCT_PER_SEC = 9.0f;
// A focus losing less than this is still the walk in, not the kill.
constexpr float ULDUAR_ALGALON_STAR_KILL_MEASURED_PCT_PER_SEC = 3.5f;
// The fastest 4 s slope worth planning for: the last 15% has gone at 16%/s in a second.
constexpr float ULDUAR_ALGALON_STAR_KILL_PEAK_PCT_PER_SEC = 12.0f;
// A star whose own death would land badly is only acted on this close to it: a fresh wave's four all die
// together and would read as due at the spawn, and the serial deadline already paces them.
constexpr uint32 ULDUAR_ALGALON_STAR_DUE_HORIZON_MS = 30000;
// A kill already running only stops for an explosion this close, so it doesn't flip on a tick of noise.
constexpr uint32 ULDUAR_ALGALON_STAR_KEEP_GAP_MS = 6000;
// No explosion from just before the cast until the raid is back out of the void.
constexpr uint32 ULDUAR_ALGALON_STAR_BIG_BANG_LEAD_MS = 1000;
constexpr uint32 ULDUAR_ALGALON_STAR_BIG_BANG_TAIL_MS = 3000;
// Nor around a meteor's impact: it takes 6-11k off everyone, and the healers need seconds to undo it.
constexpr uint32 ULDUAR_ALGALON_COSMIC_SMASH_IMPACT_MS = 4800;
constexpr uint32 ULDUAR_ALGALON_STAR_IMPACT_LEAD_MS = 4000;
constexpr uint32 ULDUAR_ALGALON_STAR_IMPACT_TAIL_MS = 5000;
// Meteors fall on a fixed clock (Repeat(25s + 500ms)): the first lands 30.9 s after the intro ends.
constexpr uint32 ULDUAR_ALGALON_FIRST_SMASH_IMPACT_MS = 30900;
constexpr uint32 ULDUAR_ALGALON_COSMIC_SMASH_INTERVAL_MS = 25500;
// Predicted impacts the star plan looks ahead to.
constexpr uint8 ULDUAR_ALGALON_SMASH_FORECAST = 3;
// Forecast to die this soon, a star is a hazard whatever its health: at 3.4%/s the 5% line is 1.5 s.
constexpr uint32 ULDUAR_ALGALON_STAR_DYING_MS = 4000;

// Cosmic Smash: full damage inside 6 yd, doubled dmg/dist to 10, dmg/dist past that. Impact is ~4.8s
// after the marker lands, one marker on 10-man, three on 25-man.
constexpr float ULDUAR_ALGALON_COSMIC_SMASH_TRIGGER_RADIUS = 15.0f;
constexpr float ULDUAR_ALGALON_COSMIC_SMASH_CLEARANCE = 15.0f;
constexpr float ULDUAR_ALGALON_COSMIC_SMASH_MIN_CLEARANCE = 10.0f;
// Inside this the generic movers are held until the marker expires.
constexpr float ULDUAR_ALGALON_COSMIC_SMASH_HOLD_RADIUS = 19.0f;
constexpr float ULDUAR_ALGALON_DODGE_SEARCH_RADIUS = 24.0f;
// The marker creatures live 10s but the meteor lands ~4.8s in; past this they mark nothing.
constexpr uint32 ULDUAR_ALGALON_MARKER_LIFETIME_MS = 5500;

// Algalon formation. He lands in the middle and is walked out to the edge on the -Y side, between the two
// star spawns there, so the stars wander past the raid less. A chase stops 1.5-12 yd short of his tank,
// so every ring slot and melee waiting spot stays 13.5 yd from the whole line from his landing spot to the
// tank slot: at the end of the intro he swings at a random player, whenever he loses his tank he swings
// at whoever is next, and anyone in reach is one hit.
//
// Healers 0-3 at 19 yd from the hold, inner ranged 4-9 at 25 yd, outer ranged 10-17 at 31 yd, each ring
// ordered to alternate sides so a short raid still spreads. Every slot is at least 8 yd from the four worm
// hole spots, and healers are within 22 yd of the tank slot.
constexpr uint8 ULDUAR_ALGALON_HEALER_SLOTS = 4;
constexpr uint8 ULDUAR_ALGALON_RANGED_INNER_SLOTS = 6;
constexpr uint8 ULDUAR_ALGALON_RANGED_OUTER_SLOTS = 8;
constexpr uint8 ULDUAR_ALGALON_TOTAL_SLOTS =
    ULDUAR_ALGALON_HEALER_SLOTS + ULDUAR_ALGALON_RANGED_INNER_SLOTS + ULDUAR_ALGALON_RANGED_OUTER_SLOTS;
constexpr float ULDUAR_ALGALON_SLOT_TOLERANCE = 2.0f;
// The spots the walk out never puts in his reach; a spot made up during the fight keeps this from him.
constexpr float ULDUAR_ALGALON_FORMATION_CLEARANCE = 13.5f;
// The holder this close to the tank slot has him where he belongs.
constexpr float ULDUAR_ALGALON_HOLDER_ON_SPOT_RADIUS = 3.0f;
// With him anywhere else, the other swap tank waits this far from him towards the tank slot.
constexpr float ULDUAR_ALGALON_OFFTANK_STEP = 7.0f;
constexpr uint8 ULDUAR_ALGALON_MELEE_STAGE_SPOTS = 10;
// Melee go in once a swap tank holds him on the tank slot, or this long after the intro if he holds him
// somewhere else, and after the second wait whoever holds him, so a pull nobody picks up doesn't idle them.
constexpr uint32 ULDUAR_ALGALON_MELEE_RELEASE_MS = 8000;
constexpr uint32 ULDUAR_ALGALON_MELEE_RELEASE_ANYWAY_MS = 20000;
// Threat redirects go on the tank from the intro until this long after it. Tricks reaches 20 yd and
// Misdirection 100; CanCastSpell lets an out of range cast through.
constexpr uint32 ULDUAR_ALGALON_OPENER_MS = 20000;
constexpr float ULDUAR_ALGALON_TRICKS_RANGE = 20.0f;
constexpr float ULDUAR_ALGALON_MISDIRECTION_RANGE = 100.0f;
// The pull's first holder keeps him on the pickup slot this long, facing him, before the walk out.
constexpr uint32 ULDUAR_ALGALON_PULL_HOLD_MS = 3000;
// A human tank is told to taunt this often while a swap or a pickup is theirs to make.
constexpr uint32 ULDUAR_ALGALON_TANK_CALL_MS = 5000;
// Humans are told to step into a hole with this much of the Big Bang cast left.
constexpr int32 ULDUAR_ALGALON_HIDE_CALL_MS = 3000;
// How far a slot buried under a hole may be stepped aside before the formation gives up on it.
constexpr float ULDUAR_ALGALON_SLOT_DISPLACE_RADIUS = 12.0f;

// Per instance, not per bot: one room scan serves the whole raid.
constexpr uint32 ULDUAR_ALGALON_STATE_TICK_MS = 250;
// A walk standing still this long was stopped short and has to be issued again.
constexpr uint32 ULDUAR_ALGALON_STALL_MS = 500;

// His home and landing spot.
extern const Position ULDUAR_ALGALON_ROOM_CENTER;
// Both swap tanks wait 8 yd from his landing spot, inside his reach, so the pickup taunt lands the moment
// he turns selectable.
extern const Position ULDUAR_ALGALON_PICKUP_SLOT;
extern const Position ULDUAR_ALGALON_PULL_OFFTANK_SLOT;
// The holder stands on the tank slot and he stops short of it, about on the hold spot; the other swap
// tank's slot is in his reach wherever he stops.
extern const Position ULDUAR_ALGALON_TANK_SLOT;
extern const Position ULDUAR_ALGALON_HOLD_SPOT;
extern const Position ULDUAR_ALGALON_OFFTANK_SLOT;
extern const std::array<Position, ULDUAR_ALGALON_TOTAL_SLOTS> ULDUAR_ALGALON_RING_SLOTS;
extern const std::array<Position, ULDUAR_ALGALON_MELEE_STAGE_SPOTS> ULDUAR_ALGALON_MELEE_STAGE_SLOTS;

// Written as its number into algalon.phase.
enum class AlgalonPhase : uint32
{
    Idle = 0,
    Intro = 1,
    One = 2,
    Two = 3,
    Won = 4,
};

// One creature from the room scan. Positions stay valid for phased bots, which can't see the room.
struct AlgalonScanUnit
{
    ObjectGuid guid;
    Position position;
    ObjectGuid victim;
    float healthPct = 0.0f;
    bool active = false;
    // Stars only: Collapse drains 1% a second from here, so health against age says whether anything hit it.
    uint32 firstSeenMs = 0;
    // Stars only: health lost per second over the log, never under the drain. `lossMeasured` once the log
    // covers ULDUAR_ALGALON_STAR_LOG_MIN_MS.
    float lossPctPerSec = ULDUAR_ALGALON_STAR_DRAIN_PCT_PER_SEC;
    bool lossMeasured = false;
};

struct AlgalonEncounterState
{
    // Slots are held, not re-derived, so one death doesn't renumber everyone behind the corpse.
    RaidObs::ObsGuidMap<uint8> slotAssignments{"algalon.slot"};
    // Latched per cast so a hole appearing closer can't turn a bot around mid run.
    RaidObs::ObsGuidMap<ObjectGuid> shelterAssignments{"algalon.shelter"};
    RaidObs::ObsGuidMap<ObjectGuid> kiteHoleAssignments{"algalon.kitehole"};

    RaidObs::ObsValue<AlgalonPhase> phase{"algalon.phase"};
    RaidObs::ObsValue<uint32> bigBang{"algalon.bigbang"};
    RaidObs::ObsValue<ObjectGuid> bigBangSoaker{"algalon.soaker"};
    RaidObs::ObsValue<ObjectGuid> bigBangBackup{"algalon.backup"};
    RaidObs::ObsValue<bool> urgent{"algalon.urgent"};
    RaidObs::ObsValue<uint32> holeCount{"algalon.holes"};
    RaidObs::ObsValue<ObjectGuid> focusStar{"algalon.focusstar"};
    RaidObs::ObsGuidSet starTeam{"algalon.starteam"};
    RaidObs::ObsValue<ObjectGuid> handler{"algalon.handler"};
    // The two tanks that trade him on Phase Punch, latched for the pull: two bot tanks when the raid has
    // them, else the group's main and first assist tank, one of whom may be a human.
    RaidObs::ObsGuidSet swapPair{"algalon.swappair"};

    ObjectGuid boss;
    bool engaged = false;
    bool engagedSeen = false;
    bool meleeReleased = false;
    uint32 introEndMs = 0;
    // The pull's first swap tank to hold him, and when.
    ObjectGuid firstHolder;
    uint32 firstHoldMs = 0;
    uint32 lastTankCallMs = 0;
    bool bigBangHideCalled = false;
    uint32 nextBigBangMs = 0;
    uint32 bigBangCount = 0;
    bool bigBangCasting = false;
    // The soaker is cleared with the cast, and the second after the hit is when he needs the raid.
    uint32 lastBigBangEndMs = 0;
    ObjectGuid lastSoaker;
    // The Big Bang cycle the defensive hand-over was last traced for.
    uint32 handoverNoted = 0;

    std::vector<AlgalonScanUnit> stars;
    std::vector<AlgalonScanUnit> holes;
    std::vector<AlgalonScanUnit> constellations;
    std::vector<AlgalonScanUnit> markers;
    std::vector<AlgalonScanUnit> darkMatter;
    std::unordered_map<ObjectGuid, uint32> markerSeenMs;
    std::unordered_map<ObjectGuid, uint32> starSeenMs;
    // (ms, health %) per star, one sample per ULDUAR_ALGALON_STAR_LOG_STEP_MS.
    std::unordered_map<ObjectGuid, std::vector<std::pair<uint32, float>>> starHealthLog;
    // Dropped part-killed: the log restarts, and until it fills the star reads as dying at the dropped rate,
    // or the old slope still carries the team's damage and makes it due again next tick.
    std::unordered_set<ObjectGuid> droppedStars;
    // The speed the team last showed on its focus, capped at the peak.
    float teamKillPctPerSec = 0.0f;
    // The next meteor not yet marked.
    uint32 nextSmashImpactMs = 0;
    // The window last traced, so the plan behind it is noted once per change.
    std::string lastStarWindow;
    // Opened at a deadline, the kill keeps the deadline's looser position test.
    bool focusByDeadline = false;
    // Far-side bearing per handler, latched with its kite hole, or the park spot turns as it walks round.
    std::unordered_map<ObjectGuid, float> kiteBearings;
    bool wormHolesSeen = false;

    uint32 lastStarDeathMs = 0;
    uint32 lastTickMs = 0;
};

// Instance lookup, never "find target" or "nearest npcs": both are phase filtered, and a bot inside a
// hole would lose him and every node with him.
Unit* GetAlgalon(PlayerbotAI* botAI);
bool AlgalonInRoom(Player* bot);
// Alive and the bot is in his room, pull or not. The formation settles on this before the pull.
bool AlgalonPresent(PlayerbotAI* botAI);
// In combat and not beaten.
bool AlgalonEngaged(PlayerbotAI* botAI);
bool AlgalonInPhaseOne(PlayerbotAI* botAI);
bool IsAlgalonPhased(Unit const* unit);
// From the intro until ULDUAR_ALGALON_OPENER_MS after it.
bool AlgalonInOpener(PlayerbotAI* botAI);
// The swap tank holding him, or the one about to take him while the intro runs.
Player* GetAlgalonOpenerTank(PlayerbotAI* botAI);
// "misdirection" or "tricks of the trade" for the classes that redirect threat, else nullptr.
char const* AlgalonThreatRedirectSpell(Player* bot);

// Runs from the definition tick, at most once per ULDUAR_ALGALON_STATE_TICK_MS per instance.
void AlgalonTickEncounterState(PlayerbotAI* botAI);

bool AlgalonBigBangCasting(PlayerbotAI* botAI);
// -1 when he isn't casting it.
int32 AlgalonBigBangRemainingMs(PlayerbotAI* botAI);
// Off the predicted clock; false while casting.
bool AlgalonBigBangWithin(PlayerbotAI* botAI, uint32 seconds);
// The cast the instance tick has latched a soaker for. Roles read this, never the live cast: in the
// tick's first 250ms of a cast nobody is the soaker yet, and every bot would run for a hole.
bool AlgalonBigBangLatched(Player* bot);
// Until the next predicted hit, cast included; 0 while he casts it. INT32_MAX until the intro ends, when
// nothing is predicted and nothing should be held for it.
int32 AlgalonMsToBigBangHit(PlayerbotAI* botAI);
// The last cast's soaker, alive, back in the room and still low, while the healers' window to reach him
// after the hit is open; else nullptr.
Player* GetAlgalonRescueTarget(PlayerbotAI* botAI);
// What this bot can put on the soaker: an external before the hit, a heal over time or shield before it
// hides, an instant heal once it is back. nullptr when it has nothing castable or already put it up.
char const* GetAlgalonSoakerExternal(PlayerbotAI* botAI, Player* soaker);
char const* GetAlgalonSoakerPrep(PlayerbotAI* botAI, Player* soaker);
char const* GetAlgalonRescueSpell(PlayerbotAI* botAI, Player* soaker);

// What this bot does about the current Big Bang. Traced as algalon.hide.
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
// The Run branch alone, untraced and without latching a shelter, for rule predicates.
bool AlgalonShouldRunForShelter(Player* bot);
// How long this bot can still fight before it has to run for its hole; INT32_MAX when it has no hole to run
// to or doesn't hide.
int32 AlgalonHideSlackMs(Player* bot);
// The soaker, the backup, and a handler holding constellations: one follows its victim into a hole
// and closes it on the raid.
bool AlgalonStaysOut(Player* bot);
bool AlgalonHoldsConstellation(Player* bot);
// By spell id: CanCastSpell refuses during a channel, and a shadow priest is nearly always in Mind Flay.
bool AlgalonCanDisperse(Player* bot);

Player* GetAlgalonBigBangSoaker(PlayerbotAI* botAI);
Player* GetAlgalonBigBangBackup(PlayerbotAI* botAI);
// The hole this bot runs to for the current cast, latched.
bool GetAlgalonShelter(Player* bot, Position& shelter);
// Nearest cached hole within radius, readable while phased.
bool AlgalonHoleNear(Player* bot, float radius, Position* hole = nullptr);

// Whoever Algalon is swinging at. Alternates between the two tanks with the Phase Punch swap.
Player* GetAlgalonBossTank(PlayerbotAI* botAI);
bool IsAlgalonSwapTank(Player* player);
uint8 GetAlgalonPhasePunchStacks(Player* player);
// The swap tank that takes him when his target is nobody who should have him, a bot before a human.
// Before the pull it is the one waiting on the tank slot for the end of the intro.
Player* GetAlgalonPickupTank(PlayerbotAI* botAI);
// Due to taunt him now: he is loose, or the holder is deep enough in Phase Punch to hand him over.
bool AlgalonShouldPickUp(Player* bot);
bool AlgalonShouldSwap(Player* bot);
// A swap tank or the handler not holding him stays off him while a swap tank does. After a taunt both
// tanks build threat at the same rate, so the one carrying the stacks takes him straight back, and a
// tank outside the pair would take him from both.
bool AlgalonYieldsBoss(Player* bot);
// The swap tank holding him when the other is dead or phased. Not stacks: the old holder keeps its 4 for the
// aura's 45 s after every swap, so that would be most of the fight.
Player* AlgalonLoneHolder(PlayerbotAI* botAI);
// The swap tank not holding him, busy with an add rather than idle on its slot.
bool AlgalonOffTankWorking(Player* bot);

// Constellations in phase 1, Unleashed Dark Matter in phase 2: a third tank, else the swap tank not
// holding Algalon.
Player* GetAlgalonHandler(PlayerbotAI* botAI);
// A loose constellation to taunt first, else one already on the handler while a hole is spare to drag it
// through. Traced as algalon.kite.
Unit* GetAlgalonHandlerConstellation(Player* bot);
// Where the handler parks to drag its constellation through a spare hole.
bool GetAlgalonKiteSpot(Player* bot, Unit* constellation, Position& spot);
// A Dark Matter on anyone but the handler, the one hurting the most fragile victim first.
Unit* GetAlgalonLooseDarkMatter(Player* bot);

// Ranged DPS assigned to the stars; every ranged DPS joins while no hole stands with a Big Bang due.
bool IsAlgalonStarTeam(Player* bot);
// The star the team may kill right now, or nullptr while the pacing window is shut.
Unit* GetAlgalonFocusStar(PlayerbotAI* botAI);
uint8 AlgalonAliveStarCount(PlayerbotAI* botAI);

bool AlgalonCosmicSmashThreatens(Player* bot);
// The bot stands where a star about to die will put its hole.
bool AlgalonDyingStarThreatens(Player* bot);
bool AlgalonCosmicSmashMarkerNear(PlayerbotAI* botAI, Position const& spot, float radius);
// Close enough to a live marker or a dying star that the generic movers are held until it is gone.
bool AlgalonSmashHoldsGround(Player* bot);
// Every marker, hole and dying star a dodge or an exit must stay clear of.
std::vector<EncounterHelpers::HazardCircle> GetAlgalonDodgeHazards(Player* bot, float markerClearance);
bool AlgalonSpotInRoom(float x, float y);
// A straight walk from the bot to (x, y) would pass within `clearance` of a hole. The hole it is already
// leaving doesn't count.
bool AlgalonPathCrossesHole(Player* bot, float x, float y, float clearance = ULDUAR_ALGALON_HOLE_PATH_CLEARANCE);
// A generic mover stopping at (x, y) would leave the bot standing at a field's edge, or walk it through one.
// A phased bot passes the exit radius: still inside a field when the phase drops, it is phased again.
bool AlgalonWalkEntersField(Player* bot, float x, float y,
                            float standClearance = ULDUAR_ALGALON_FIELD_STAND_CLEARANCE);
// Where to walk next on the way to `to`: `to` itself, or a point round a hole the straight walk would clip.
// False when the way is blocked and nothing goes round.
bool AlgalonWalkAround(Player* bot, Position const& to, Position& next);
// Phased with a hole within its exit radius: walking out is all that counts until it is outside.
bool AlgalonPhasedInField(Player* bot);
// A star other than `except` within `radius` of the spot.
bool AlgalonStarWithin(PlayerbotAI* botAI, Position const& spot, float radius, ObjectGuid const& except);
// Its hole is too far to reach in the cast, so it is walking towards it before Big Bang.
bool AlgalonHeadsForFarHole(Player* bot);

// Ranged and healers ring the hold spot.
bool AlgalonTakesRingSlot(Player* bot);
// A melee bot kept outside his reach until a swap tank holds him.
bool AlgalonMeleeStaged(Player* bot);
bool TryGetAlgalonSlotPosition(uint8 slotIndex, Position& position);
// The spot this bot should stand on, stepped aside when a hole buried it, or a waypoint round a hole in
// the way. Traced as algalon.spot.
bool TryGetAlgalonSlot(Player* bot, Position& position, bool* waypoint = nullptr);
// The undisplaced slot, untraced, for dodges that prefer to land near it.
bool GetAlgalonSlotAnchor(Player* bot, Position& slot);

// Stars, constellations and Dark Matter the bot's own job doesn't cover, and Algalon himself for the
// swap tank yielding him.
void AppendAlgalonTargetExclusions(PlayerbotAI* botAI, GuidSet& exclusions);
// Same rule for damage already on its way: false vetoes a hit on this unit.
bool AlgalonMayDamage(Player* bot, Unit* target);

#endif
