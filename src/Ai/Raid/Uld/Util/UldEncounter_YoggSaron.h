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
    GO_FLEE_TO_THE_SURFACE_PORTAL = 194625,
    // One per illusion, opened by the Brain the moment the last Influence Tentacle in that room dies.
    // The entries run in the same order as ACTION_ILLUSION_DRAGONS/ICECROWN/STORMWIND.
    GO_CHAMBER_ILLUSION_DOORS = 194635,
    GO_ICECROWN_ILLUSION_DOORS = 194636,
    GO_STORMWIND_ILLUSION_DOORS = 194637,
};

constexpr float ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT = 300.0f;
constexpr float ULDUAR_YOGG_SARON_BRAIN_ROOM_AXIS_Z_PATHING_ISSUE_DETECT = 200.0f;
constexpr float ULDUAR_YOGG_SARON_STORMWIND_KEEPER_RADIUS = 150.0f;
constexpr float ULDUAR_YOGG_SARON_ICECROWN_CITADEL_RADIUS = 150.0f;
constexpr float ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_RADIUS = 150.0f;
constexpr float ULDUAR_YOGG_SARON_BRAIN_ROOM_RADIUS = 50.0f;

// Yogg-Saron reduced-Keeper hard mode: a bot whose Sanity (63050, 100 stacks) is at or below this
// pulls behind Yogg and faces away to conserve it. With Freya absent there are no Sanity Wells, so the
// drain is one-way - kept low so only near-Insane bots pull out. Confirm in-game.
constexpr uint32 ULDUAR_YOGG_SARON_SANITY_CONSERVE_THRESHOLD = 15;

// Ominous Clouds orbit Sara at 11.5/21.3/31.2/41/50.8/60.8 yd, constant 3 yd/s. Summon reach is the
// script's 6 yd plus both bounding radii, because the check runs through IsWithinDistInMap: the
// innermost orbit is provably player-only and the nearest player to each of its summons sat at
// 4.6-8.4 yd. Ring gaps are ~9.8 yd, so no station ever clears every orbit and the dodge has to win
// on warning instead.
//
// The trigger has to cover the clear radius. The action's own early-out is "already outside every
// circle", so the clear radius is what decides when a bot moves; a trigger inside it leaves a band
// where the bot is in a hazard circle and the node is never asked. At 10 against a clear of 14 a bot
// first moved with the cloud 10 yd out, which is 0.5 s before it is in reach against the 1.2 s it
// needs to cover 8.5 yd.
constexpr float ULDUAR_YOGG_SARON_CLOUD_TRIGGER_RADIUS = 18.0f;
constexpr float ULDUAR_YOGG_SARON_CLOUD_CLEAR_RADIUS = 14.0f;
constexpr float ULDUAR_YOGG_SARON_CLOUD_SUMMON_RADIUS = 9.0f;

// Unlike a Death Ray, crossing a cloud is worse than standing in one: the far side costs a Guardian.
// The orbit is the other half - a sideways step is back under the same cloud within seconds - so a
// candidate is tested against where the cloud will be as well as where it is. It has to outlast the
// crossing: a cloud sweeps a bot holding the standoff for about 5.8 s, so a shorter lead puts the
// sidestep in its near future.
constexpr uint32 ULDUAR_YOGG_SARON_CLOUD_LEAD_MS = 6000;

// Shadow Nova, the Guardian's death explosion: DBC radius 15, plus both object sizes at apply time,
// which measured 16.2 at its furthest. Ranged and healers stay out of it; melee and tanks have to eat
// it to kill the thing at all. Unlike the cloud pair the trigger sits inside the clear radius, and
// that band is margin rather than exposure: a bot 18 yd out is not moved to 20, but 18 is already
// past the blast.
constexpr float ULDUAR_YOGG_SARON_SHADOW_NOVA_TRIGGER_RADIUS = 17.0f;
constexpr float ULDUAR_YOGG_SARON_SHADOW_NOVA_CLEAR_RADIUS = 20.0f;

// A Guardian at or under this is about to detonate. Running from every Guardian instead scatters the
// raid to the rim and leaves bots to be picked off one at a time.
constexpr float ULDUAR_YOGG_SARON_GUARDIAN_NOVA_HEALTH_PCT = 20.0f;

// Fervor gets a wider gate because the nova it doubles kills outright rather than hurts, so waiting
// for a Guardian to look nearly dead is waiting too long: under focus fire one is below 20% for about
// a second, which is 7 yd of travel against a 16 yd blast.
constexpr float ULDUAR_YOGG_SARON_FERVOR_NOVA_HEALTH_PCT = 50.0f;

// Melee and tanks are held near Sara because a Guardian walks to whoever holds threat, and its death
// nova only reaches her from 15 yd - a kill further out does nothing for the phase at all. Release is
// tighter than the leash: walked back to the boundary itself a bot is let go the moment it crosses
// and dragged straight out again by reach melee.
constexpr float ULDUAR_YOGG_SARON_P1_LEASH = 15.0f;
constexpr float ULDUAR_YOGG_SARON_P1_LEASH_RELEASE = 12.0f;

// The mirror of the leash, for everyone who does not have to be in the blast. Every Guardian dies on
// Sara, so her own spot is a standing nova hazard: ranged and healers sat inside it for 37% and 49%
// of one phase 1 and Shadow Nova was 65% of all damage the raid took. Released further out than it
// fires, or reach spell walks the bot straight back in and the two trade the tick.
//
// They stack rather than spread once out there. Against a point hazard that sweeps a circle and
// re-arms 10 s after each summon, a blob is passed once per orbit while a spread-out line hands it
// somebody in reach for most of one - 1.5 summons against 7.3. That inverts the usual rule and only
// holds because the nova cannot reach this far.
constexpr float ULDUAR_YOGG_SARON_P1_STANDOFF = 20.0f;
constexpr float ULDUAR_YOGG_SARON_P1_STANDOFF_RELEASE = 22.0f;

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

// How far out to look for a Guardian worth kicking. Wider than any interrupt's range on purpose - the
// action drops the ones it cannot reach, and a short list here would hide a cast from a bot who could.
constexpr float ULDUAR_YOGG_SARON_INTERRUPT_SEARCH_RADIUS = 40.0f;

// Everything the raid has to clear out of an illusion room before the Brain can be touched. The
// Influence Tentacle leads the list because it is the one that gates the Brain.
extern const std::vector<uint32> ULDUAR_YOGG_SARON_ILLUSION_MOBS;

extern const Position ULDUAR_YOGG_SARON_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE;
extern const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT;
extern const Position ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT;

// Yogg-Saron phase reads. Yogg is not reliably on a bot's threat list, so both scan for the creature
// instead of going through "find target".
// Phase 1. Sara lives on into P2/P3 at 1 health, so "Sara is alive" is not a phase test by itself.
bool YoggSaronInPhase1(PlayerbotAI* botAI);
bool YoggSaronInPhase2(PlayerbotAI* botAI);
bool YoggSaronInPhase3(PlayerbotAI* botAI);

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

// Whether the Brain is safe to approach and hit. Damaging it while any Influence Tentacle lives deals
// nothing and kills the attacker outright, so this gates both the walk down and the target pick.
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
