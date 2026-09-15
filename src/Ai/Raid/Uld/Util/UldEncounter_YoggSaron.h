/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERYOGGSARON_H
#define PLAYERBOTS_ULDENCOUNTERYOGGSARON_H

#include "Position.h"
#include "UldData.h"

#include <vector>

class PlayerbotAI;
class Creature;
class Player;
class Unit;

// Yogg-Saron.
//
// Three phases in three different rooms. P1 is tentacles and Ominous Clouds around the central
// platform; P2 opens the brain room below, where the raid splits between killing tentacles above and
// clearing illusions - Stormwind, Icecrown and the Chamber of Aspects - inside the brain; P3 is the
// boss himself on the platform once the Shadow Barrier drops and the last Guardian is down.
//
// Sanity (63050) is the currency underneath all of it: it drains on gaze and illusion exposure, and
// at zero the bot is Insane and lost. That is why the geometry below is per-room rather than one
// arena, and why phase is read off the boss's aura rather than a timer.

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
    SPELL_SARAS_FERVOR = 63138,  // +20% damage done and +100% damage taken, 15s
    SPELL_BRAIN_LINK = 63802,
    SPELL_MALADY_OF_THE_MIND = 63830,
    SPELL_SHADOW_BARRIER = 63894,
    SPELL_TELEPORT_TO_CHAMBER = 63997,
    SPELL_TELEPORT_TO_ICECROWN = 63998,
    SPELL_TELEPORT_TO_STORMWIND = 63989,
    SPELL_TELEPORT_BACK = 63992,
    SPELL_CANCEL_ILLUSION_AURA = 63993,
    SPELL_INDUCE_MADNESS = 64059,
    SPELL_LUNATIC_GAZE_YS = 64163,
    SPELL_DARK_VOLLEY = 63038,  // Guardian's 1.5s cast, 35 yd - distance is no answer, only a kick
    SPELL_DARK_VOLLEY_H = 65330,  // 25 normal casts 63038, so the split is not 10/25: test both
    SPELL_SQUEEZE = 64125,  // Constrictor Tentacle's grip; base id, difficulty-mapped at runtime
    SPELL_WEAKENED = 64162,  // Immortal Guardian's killable window; Thorim's Titanic Storm executes it
    SPELL_KNOCK_BACK_TRIGGERED = 64020,  // what 64022 fires every second off Yogg's body, 14 yd
    SPELL_CRUSH_CONE = 64147,  // the cone 64146 procs down the Crusher Tentacle's facing
    SPELL_LUNATIC_GAZE_SKULL = 64168,  // 64167 on a Laughing Skull fires this every second, 30 yd
    GO_FLEE_TO_THE_SURFACE_PORTAL = 194625,
    // One per illusion, opened by the Brain the moment the last Influence Tentacle in that room dies.
    // The entries run in the same order as ACTION_ILLUSION_DRAGONS/ICECROWN/STORMWIND.
    GO_CHAMBER_ILLUSION_DOORS = 194635,
    GO_ICECROWN_ILLUSION_DOORS = 194636,
    GO_STORMWIND_ILLUSION_DOORS = 194637,
};

constexpr float ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT = 300.0f;
constexpr float ULDUAR_YOGG_SARON_BRAIN_ROOM_AXIS_Z_PATHING_ISSUE_DETECT = 200.0f;
// The three illusion rooms and the brain room share one floor 108-124 yd apart, so a radius wide
// enough to overlap makes the first test win everywhere: at 150 the brain room read as Stormwind on
// every sample and 14-21% of illusion-room samples took the wrong room's name. 60 covers each room's
// landing spot (48.9 / 54.0 / 55.9 yd from its own middle) and every Laughing Skull spawn (52.8 at
// the furthest), while the nearest rival middle is 190 yd away.
constexpr float ULDUAR_YOGG_SARON_STORMWIND_KEEPER_RADIUS = 60.0f;
constexpr float ULDUAR_YOGG_SARON_ICECROWN_CITADEL_RADIUS = 60.0f;
constexpr float ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_RADIUS = 60.0f;
constexpr float ULDUAR_YOGG_SARON_BRAIN_ROOM_RADIUS = 50.0f;

// Floor of the brain level. The three illusion rooms and the brain room all sit on it; the boss
// platform is 85 yd above.
constexpr float ULDUAR_YOGG_SARON_BRAIN_LEVEL_MIN_Z = 230.0f;
constexpr float ULDUAR_YOGG_SARON_BRAIN_LEVEL_MAX_Z = 250.0f;

// Yogg-Saron reduced-Keeper hard mode: a bot whose Sanity (63050, 100 stacks) is at or below this
// pulls behind Yogg and faces away to conserve it. With Freya absent there are no Sanity Wells, so the
// drain is one-way - kept low so only near-Insane bots pull out. Confirm in-game.
constexpr uint32 ULDUAR_YOGG_SARON_SANITY_CONSERVE_THRESHOLD = 15;

// A cloud summons a Guardian on any player within 8.5 yd. SelectNearbyTarget's 6 yd goes through
// _IsWithinDist, which adds both GetObjectSize() values, and that returns UNIT_FIELD_COMBATREACH
// rather than the bounding radius: 1.0 for the cloud, and 1.5 for every player whatever their race,
// because Player::SetObjectScale hands out DEFAULT_COMBAT_REACH flat.
//
// Nobody dodges that. The orbits are fixed circles, the summon aura re-arms 10 s after every kill,
// and the ring gaps are under the 17 yd a cloud sweeps, so no radius clears every orbit and no
// sidestep outruns one. Bots stand where the geometry cannot reach them instead - see the cloud-free
// radius and the ranged station below - and this is only the circle a bot moving for some other
// reason must not land in or walk through. Widening it back into a dodge cost one pull 4,225 moves,
// a quarter of the raid's damage and the phase.
constexpr float ULDUAR_YOGG_SARON_CLOUD_AVOID_RADIUS = 9.5f;

// Crossing a cloud is worse than standing next to one: the far side costs a Guardian. The orbit is
// the other half - a spot clear of a cloud now is under it seconds later - so a candidate is tested
// against where the cloud will be as well as where it is. It has to outlast the crossing: a cloud
// sweeps a stationary bot for about 5.7 s, so a shorter lead puts the destination in its near future.
constexpr uint32 ULDUAR_YOGG_SARON_CLOUD_LEAD_MS = 6000;

// Shadow Nova, the Guardian's death explosion: DBC radius 15, plus both object sizes at apply time,
// which measured 16.2 at its furthest. Ranged and healers stay out of it; melee and tanks have to eat
// it to kill the thing at all. The trigger sits inside the clear radius, and that band is margin
// rather than exposure: a bot 18 yd out is not moved to 20, but 18 is already past the blast.
constexpr float ULDUAR_YOGG_SARON_SHADOW_NOVA_TRIGGER_RADIUS = 17.0f;
constexpr float ULDUAR_YOGG_SARON_SHADOW_NOVA_CLEAR_RADIUS = 20.0f;

// A Guardian at or under this is about to detonate. Running from every Guardian instead scatters the
// raid to the rim and leaves bots to be picked off one at a time.
constexpr float ULDUAR_YOGG_SARON_GUARDIAN_NOVA_HEALTH_PCT = 20.0f;

// Fervor gets a wider gate because the nova it doubles kills outright rather than hurts, so waiting
// for a Guardian to look nearly dead is waiting too long: under focus fire one is below 20% for about
// a second, which is 7 yd of travel against a 16 yd blast.
constexpr float ULDUAR_YOGG_SARON_FERVOR_NOVA_HEALTH_PCT = 50.0f;

// The two places in the room a cloud cannot reach, both fixed by the orbits. Measured over four pulls
// the six sit at 11.39-11.86 / 21.25-21.52 / 31.13-31.31 / 40.93-41.07 / 50.81-50.92 / 60.74-60.84 yd
// and never drift, so against an 8.5 yd reach:
//
//   - inside 11.39 - 8.5 = 2.89 yd of Sara nothing reaches at all, which is where melee already stand
//   - between 11.86 + 8.5 and 31.13 - 8.5 only the second orbit reaches, and the midpoint of that band
//     is 21.5 - the second orbit itself, because standing on a ring is what buys the most room from
//     its neighbours. That leaves 1.13 yd either way, which is the whole band tolerance.
//
// No radius is clear of every orbit: the gaps are 9.4-9.9 yd against the 17 yd a cloud sweeps. The
// third orbit would be the cheaper station - one Guardian per 65 s against 45 - but it is 31.2 yd out
// against a 28.5 yd spellDistance, so a bot posted there walks itself back in.
constexpr float ULDUAR_YOGG_SARON_P1_CLOUD_FREE_RADIUS = 2.8f;
constexpr float ULDUAR_YOGG_SARON_P1_RANGED_STATION_RADIUS = 21.5f;
constexpr float ULDUAR_YOGG_SARON_P1_RANGED_BAND_TOLERANCE = 1.0f;

// The six orbits themselves, midpoints of the four-pull spread above. Not used to steer anything -
// the station is a spot, not a ring - only to say which orbits could reach a bot that ended up
// somewhere else, which is the one number the whole phase 1 design turns on.
constexpr float ULDUAR_YOGG_SARON_CLOUD_ORBITS[] = {11.6f, 21.4f, 31.2f, 41.0f, 50.9f, 60.8f};
constexpr float ULDUAR_YOGG_SARON_CLOUD_SUMMON_REACH = 8.5f;

// Melee and tanks are held near Sara because a Guardian walks to whoever holds threat, and its death
// nova only reaches her from 15 yd - a kill further out does nothing for the phase at all. The
// release is the cloud-free radius rather than a boundary a step inside the leash: anything between
// the two lets go of the bot somewhere the innermost orbit sweeps, and 12 let go of it on the orbit.
constexpr float ULDUAR_YOGG_SARON_P1_LEASH = 15.0f;
constexpr float ULDUAR_YOGG_SARON_P1_LEASH_RELEASE = ULDUAR_YOGG_SARON_P1_CLOUD_FREE_RADIUS;

// Ranged and healers stack rather than spread, which inverts the usual rule and only holds because the
// nova cannot reach the station. A cloud is in contact for (8.5 + blob + 8.5) / 3 seconds and re-arms
// 10 s after each summon, so a blob under 13 yd across costs exactly one Guardian per pass: at 5 yd
// that is 9 s of contact and one Guardian per 45 s orbit for the whole back line. Spread over an arc
// instead, one pull handed the cloud somebody in reach for most of every orbit.
constexpr float ULDUAR_YOGG_SARON_P1_RANGED_STACK_RADIUS = 5.0f;

// The second cap is the load-bearing one: a bot dodging outward otherwise walks out of spell range
// and stops contributing for the rest of the phase.
constexpr float ULDUAR_YOGG_SARON_SPACING_SEARCH_RADIUS = 25.0f;
constexpr float ULDUAR_YOGG_SARON_SPACING_MAX_FROM_MIDDLE = 35.0f;

// Backstop only - "is the held spot still clear" normally invalidates first.
constexpr uint32 ULDUAR_YOGG_SARON_SPACING_HOLD_MS = 3000;

// Crush, the Crusher Tentacle's 100% proc on its own white swings: a 23 yd physical cone, +-5 degrees
// off its current facing, and the facing tracks whoever it is hitting. So the danger is standing
// collinear with the tentacle and its victim, not standing close - a four-yard sidestep at 20 yd
// clears it. Melee range is no exemption either: the cone's proximity bypass is 2.0 yd against a
// ~10.8 yd melee reach here.
constexpr float ULDUAR_YOGG_SARON_CRUSH_RANGE = 25.0f;       // DBC radius 23 plus both object sizes
constexpr float ULDUAR_YOGG_SARON_CRUSH_TRIGGER_ARC = 8.0f;  // degrees either side of the facing
constexpr float ULDUAR_YOGG_SARON_CRUSH_CLEAR_ARC = 14.0f;   // ~4 yd of lateral room at 20 yd

// Death Rays walk 9 yd legs every 1625 ms along a re-rolled cardinal axis, so the gap between these
// two is about a second of travel. The Death Orb that drops them is a marker 27 yd overhead and never
// reaches anybody.
constexpr float ULDUAR_YOGG_SARON_DEATH_RAY_TRIGGER_RADIUS = 9.0f;  // DBC 3 yd plus room to react
constexpr float ULDUAR_YOGG_SARON_DEATH_RAY_CLEAR_RADIUS = 14.0f;

// Wider than phase 1's: a Crush wedge can only be left sideways, and at 25 yd out that is a long walk.
constexpr float ULDUAR_YOGG_SARON_P2_SPACING_SEARCH_RADIUS = 35.0f;

// Phase 2 needs its own cap. Sharing phase 1's 35 yd put the box inside where the raid stands: melee
// were already beyond it in 37-50% of samples and tanks in 64-81%, so every outward candidate was
// rejected and a bot needing a three-yard sidestep had to walk inward along a 25 yd Crush wedge. The
// tentacle ring sits 41-48 yd out and the room ends at the outermost cloud orbit, 60.8 yd.
constexpr float ULDUAR_YOGG_SARON_P2_SPACING_MAX_FROM_MIDDLE = 55.0f;

// Yogg's body knocks players away once a second for the rest of the fight. 64022 is cast on himself
// at ACTION_YOGG_SARON_APPEAR and never removed: an infinite self aura triggering 64020 every 1000 ms,
// radius 14 yd, with no conditions row and no script filter. His model sits 4.5 yd above the floor,
// so sqrt(14^2 - 4.5^2) = 13.26 yd of horizontal ring. Nothing in the world can be swept for it.
//
// This costs melee nothing: Yogg's CombatReach is 30, so melee range on him is about 34 yd.
constexpr float ULDUAR_YOGG_SARON_BODY_KNOCKBACK_RADIUS = 13.3f;
constexpr float ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS = 15.0f;

// Where a bot crossing the room is sent instead of straight through the body. Wide enough that both
// legs of the detour keep their distance: the worst case is a half-turn, whose chord passes
// 24 * cos(45 deg) = 17.0 yd from the middle.
constexpr float ULDUAR_YOGG_SARON_BODY_DETOUR_RADIUS = 24.0f;

// The portal wave clock. EVENT_SARA_P2_OPEN_PORTALS fires 60 s after phase 2 starts and repeats
// every 80 s; clearing a room delays Sara's other events but explicitly reschedules this one, so the
// cadence holds all fight. Each portal is one use and despawns after 25 s, so a bot that starts
// walking when it sees one has already lost the wave - the spread has to happen before it opens.
//
// The debounce is what separates two waves from one wave seen twice, and sits above the 25 s despawn.
constexpr uint32 ULDUAR_YOGG_SARON_PORTAL_FIRST_WAVE_MS = 60000;
constexpr uint32 ULDUAR_YOGG_SARON_PORTAL_WAVE_PERIOD_MS = 80000;
constexpr uint32 ULDUAR_YOGG_SARON_PORTAL_WAVE_DEBOUNCE_MS = 30000;

// Same adaptive shape as the exit lead below: every millisecond spent standing on a portal spot is a
// millisecond not spent killing tentacles, so the lead is measured against the walk the bot faces.
constexpr uint32 ULDUAR_YOGG_SARON_PORTAL_SPREAD_LEAD_FLOOR_MS = 8000;
constexpr float ULDUAR_YOGG_SARON_PORTAL_SPREAD_LEAD_SAFETY = 2.0f;
constexpr float ULDUAR_YOGG_SARON_PORTAL_SEARCH_RADIUS = 100.0f;
constexpr float ULDUAR_YOGG_SARON_PORTAL_ARRIVED_RADIUS = 3.0f;

// AddPortals spawns RAID_MODE(4, 10), which is the first four table entries in 10-man and all ten in
// 25-man, so the brain team is capped by how many portals actually exist.
constexpr uint32 ULDUAR_YOGG_SARON_PORTAL_SPOTS_10MAN = 4;
constexpr uint32 ULDUAR_YOGG_SARON_PORTAL_SPOTS_25MAN = 10;

// Lunatic Gaze off a Laughing Skull: 64167 on the skull triggers 64168 every 1000 ms for 1750 shadow
// damage and -2 Sanity at 30 yd. The skull is UNIT_FLAG_NOT_SELECTABLE and cannot be killed, and the
// spell picks its targets with HasInArc(M_PI, caster), so facing away is the only defence there is.
constexpr float ULDUAR_YOGG_SARON_LAUGHING_SKULL_RADIUS = 30.0f;

// How early to leave the brain level before Induce Madness lands. It strips all 100 Sanity from
// anyone at or below z 300, and no Sanity means Insane, whose removal kills the player outright - so
// a mind control is always a death. The lead is taken out of the window the raid has to damage the
// Brain, so it is measured rather than flat: a bot standing on a portal needs the floor, one deep in
// an illusion room can be ~120 yd out.
constexpr uint32 ULDUAR_YOGG_SARON_EXIT_LEAD_FLOOR_MS = 10000;
constexpr float ULDUAR_YOGG_SARON_EXIT_LEAD_SAFETY = 2.0f;

// Phase 3 station. The radius is what the bot is allowed to drift inside, the leash is how far a tank
// may wander before it is walked back - wider because a tank chasing a guardian to the room's edge is
// doing its job.
constexpr float ULDUAR_YOGG_SARON_PHASE_3_STATION_RADIUS = 15.0f;
constexpr float ULDUAR_YOGG_SARON_PHASE_3_TANK_LEASH = 30.0f;

// How long a forced walk may fail to close distance before the bot stops trying, and how close counts
// as having got there. The arrival radius is generous because MoveTo stops where its own tolerance
// leaves the bot, not on the point it was handed.
constexpr uint32 ULDUAR_YOGG_SARON_WALK_GIVE_UP_MS = 6000;
constexpr float ULDUAR_YOGG_SARON_WALK_ARRIVED_RADIUS = 5.0f;

// How often the per-bot exposure probes resample. They answer "where was this bot standing when the
// thing that killed it went off", so a second is fine and a tick is 25 bots of noise.
constexpr uint32 ULDUAR_YOGG_SARON_OBS_SCAN_INTERVAL_MS = 1000;

// How far out to look for a Guardian worth kicking. Wider than any interrupt's range on purpose - the
// action drops the ones it cannot reach, and a short list here would hide a cast from a bot who could.
constexpr float ULDUAR_YOGG_SARON_INTERRUPT_SEARCH_RADIUS = 40.0f;

// Everything the raid has to clear out of an illusion room before the Brain can be touched: the
// Influence Tentacle and the six entries Creature::UpdateEntry disguises it as. Nothing else in the
// rooms is a target - the dragons, Garona, King Llane and the Immolated Champion are faction 35 and
// unattackable, and The Lich King is hostile but carries 11.1 M health, never attacks and cannot be
// killed. Listing any of them outranks the Brain in the kill order and blocks it forever.
//
// A disguise shows the entry's own health percentage while carrying the tentacle's real 8,000 (10) /
// 40,000 (25), because UpdateEntry keeps current health. That is not a damaged mob.
extern const std::vector<uint32> ULDUAR_YOGG_SARON_ILLUSION_MOBS;

// The ten portal spots, straight from the core's yoggPortalLoc. Kept as a table rather than read off
// the live creatures because the brain team has to be standing on its spot before any portal exists.
// All ten are navprobe-clean on the floor and 20.1-23.2 yd from the body, outside the knockback ring.
extern const Position ULDUAR_YOGG_SARON_PORTAL_SPOTS[ULDUAR_YOGG_SARON_PORTAL_SPOTS_25MAN];

extern const Position ULDUAR_YOGG_SARON_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_P1_RANGED_SPOT;
extern const Position ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT;
extern const Position ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT;

// Yogg-Saron phase read, and the one place the encounter is recognised at all. Yogg is not reliably
// on a bot's threat list, so it scans for the creature instead of going through "find target", and
// every node routes through here so trigger and action cannot answer differently.
//
// Combat is part of the read. Sara is a static friendly spawn that LoadAllGrids makes findable from
// instance creation and that lives on at 1 health into phases 2 and 3, so her being there says
// nothing; her JustEngagedWith calls SetInCombatWithZone, which does. Without that gate the whole
// raid walked its phase 1 stations out of combat, crossing all six cloud orbits at a run and handing
// the pull four of its twelve Guardians before the first point of damage.
uint32 YoggSaronPhase(PlayerbotAI* botAI);
bool YoggSaronEncounterActive(PlayerbotAI* botAI);
bool YoggSaronInPhase1(PlayerbotAI* botAI);
bool YoggSaronInPhase2(PlayerbotAI* botAI);
bool YoggSaronInPhase3(PlayerbotAI* botAI);

// Which room on the brain level a position is in, or the boss platform above it.
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

// What the bot is doing about the room it landed in. The brain room was never reached in either
// measured attempt because nothing walked a bot in: 71% of Stormwind samples sat within 6 yd of the
// landing spot with the tentacles alive 28-82 yd further in, and 65% of brain-level samples had no
// target at all, because the dps resolver needs line of sight and a doorway breaks it.
enum YoggSaronRoomState : uint32
{
    YOGG_SARON_ROOM_STATE_NONE = 0,
    YOGG_SARON_ROOM_STATE_WALKING_IN = 1,
    YOGG_SARON_ROOM_STATE_FIGHTING = 2,
    YOGG_SARON_ROOM_STATE_DOOR_SHUT = 3,
    YOGG_SARON_ROOM_STATE_TO_BRAIN = 4,
    YOGG_SARON_ROOM_STATE_AT_BRAIN = 5,
};

YoggSaronRoomState YoggSaronRoomStateOf(PlayerbotAI* botAI);

// The middle of the room the bot is in, which is the centroid of that room's Influence Tentacle
// summon group - walk there and the tentacles come into line of sight. False outside the three rooms.
bool YoggSaronRoomMiddle(Player* player, Position& middle);

// Whether the Brain is safe to walk to and hit: every Influence Tentacle in this room dead, and the
// room's door open. Damaging the Brain while one lives is Unit::Kill(who, who) on the attacker, so
// the door is the second read of the one server fact rather than a nicety.
bool YoggSaronBrainRoomApproachable(PlayerbotAI* botAI);

// Whether Induce Madness is close enough that the bot has to start walking for an exit portal. It
// strips all 100 Sanity from anyone below z 300 and Insane's removal kills outright, so a mind
// control is always a death - and no Sanity Well reaches the brain level, they are all on the
// platform. The lead is measured against the walk the bot actually faces.
bool YoggSaronShouldLeaveBrainLevel(PlayerbotAI* botAI);

// Laughing Skulls within gaze range that are in the bot's front 180 degrees, which is the exact test
// the spell uses to pick its targets.
std::vector<Unit*> GetYoggSaronSkullsInArc(PlayerbotAI* botAI);

// Where the portal wave clock stands. ordinal counts waves actually seen, msToNextWave is the
// prediction the spread runs on, and portalsUp is this bot's own sight of the ring - a bot
// underground sees nothing and its silence must not be read as a wave ending.
struct YoggSaronPortalWave
{
    bool active = false;
    uint32 ordinal = 0;
    uint32 msToNextWave = 0;
    bool portalsUp = false;
};

YoggSaronPortalWave YoggSaronPortalWaveState(PlayerbotAI* botAI);

// Who goes down the portals, in an order every bot derives identically: melee dps first, then exactly
// one healer, then ranged to fill, each band by GUID, tanks never. Melee because the room is a 60 s
// race on foot, and because it takes them out of the Crush ring for the length of every window - they
// stand inside an 8 degree wedge in 6.5% of samples against 1.3% for ranged.
std::vector<Player*> GetYoggSaronBrainTeam(PlayerbotAI* botAI);

// What this bot should be doing about the next portal wave.
enum YoggSaronPortalIntent : uint32
{
    YOGG_SARON_PORTAL_NOT_TEAM = 0,
    YOGG_SARON_PORTAL_WAITING = 1,
    YOGG_SARON_PORTAL_SPREADING = 2,
    YOGG_SARON_PORTAL_HOLDING = 3,
    YOGG_SARON_PORTAL_LATE = 4,
};

// Nearest-first, latched for the wave so it does not churn as bots move. By group index instead, the
// assigned walk ran a median of 41-44 yd against the 12-15 yd of the nearest live portal.
YoggSaronPortalIntent YoggSaronPortalPlan(PlayerbotAI* botAI, Position& spot);

// Guardians casting Dark Volley right now, for the interrupt node. Shared between trigger and action
// so the two cannot disagree about what is being kicked.
std::vector<Unit*> GetYoggSaronDarkVolleyCasters(PlayerbotAI* botAI);

// Interrupts this bot can aim, in the order the action tries them, empty for a class with none.
// Avenger's Shield is left out - it picks its own target and cannot be pointed at a named Guardian.
std::vector<char const*> YoggSaronInterruptSpells(Player* bot);
bool YoggSaronCanInterrupt(Player* bot);

// Guardians whose death nova this bot should leave. Ranged and healers count only one that is both
// about to die and chasing them: at spell range nothing else can reach them. Melee stand in a nova by
// design and count one only while Sara's Fervor is doubling it. Shared so the trigger and the action
// cannot disagree about who is running.
std::vector<Unit*> GetYoggSaronNovaThreats(PlayerbotAI* botAI, float radius);

// Where a cloud will be one lead ahead, taken from its own facing and run speed.
Position YoggSaronCloudLead(Creature* cloud);

// Whether a straight walk from the bot to (x, y) stays outside every cloud's summon radius.
bool YoggSaronRouteClearOfClouds(Player* bot, std::vector<Position> const& clouds, float x, float y);

// The body's knockback ring, which has no world object behind it and so cannot be swept for.
bool YoggSaronInBodyKnockback(Player* player);
bool YoggSaronRouteClearOfBody(Player* bot, float x, float y);

// A point to cross the room through when the straight line would go over the body. One is enough:
// each leg halves the turn the next one has to make, so the bot walks the arc rather than the chord.
bool YoggSaronBodyDetour(Player* bot, Position const& destination, Position& waypoint);

// Whether the Brain is safe to hit: every Influence Tentacle in this room dead. Scoped to the room
// rather than swept at 200 yd, which is one yard short of reaching the next room's tentacles.
bool YoggSaronInfluenceTentaclesCleared(PlayerbotAI* botAI);

// Live Crusher Tentacles to angle away from, each as its position plus the facing its Crush cone
// follows. The one currently hitting the bot is left out: that bot is hit wherever it stands, and
// moving only drags the cone around behind it.
//
// Shared between the spacing trigger and its action so the two cannot disagree about what a wedge is.
// The trigger asks at the tight arc and the action at the wide one, which is what keeps a tentacle
// re-facing a yard from restarting the dance.
std::vector<Position> GetYoggSaronCrushWedges(PlayerbotAI* botAI, float searchRadius);
bool InYoggSaronCrushWedge(std::vector<Position> const& wedges, float x, float y, float arcDegrees);

// Whether a forced walk is still closing on where it was sent. MoveTo's `ok` says a command was
// issued, never that a route exists: the core falls back to a straight-line spline, which once carried
// a bot 91 yd vertically at a constant speed with ok=1. Closing distance is the only evidence of a
// real path. Records are kept per node and destination, so testing several destinations in one tick
// does not wipe what the others learned.
bool YoggSaronWalkMakingProgress(PlayerbotAI* botAI, char const* node, Position const& destination);

// Window in which a counterable fear can land, for the shared anti-fear component. Yogg-Saron fears
// in P2 (Malady of the Mind, which re-casts on removal) and again in P3 (Deafening Roar).
bool YoggSaronFearWindowActive(PlayerbotAI* botAI);

#endif
