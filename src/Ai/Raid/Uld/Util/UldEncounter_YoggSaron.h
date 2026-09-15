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
    // The two the aura casts on its partner every second, apart and together. Neither leaves an aura,
    // so the cast is the only place the pair is visible from outside the aura script.
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
// nova only reaches her from 15 yd - a kill further out does nothing for the phase at all.
//
// The leash is the tighter of two radii, though, not that one. Shadow Nova is also 15 yd (65209 and
// 65719 both carry radius index 18) and the back line stands at 21.5, so a Guardian dying more than
// 21.5 - 15 = 6.5 yd out catches the whole raid instead of the melee pile. Measured over three pulls:
// novas at 2.2-4.0 yd hit 9-10 players, novas at 8.5-14.8 yd hit 22-24.
//
// The release is the cloud-free radius rather than a boundary a step inside the leash: anything
// between the two lets go of the bot somewhere the innermost orbit sweeps, and 12 let go of it on the
// orbit.
constexpr float ULDUAR_YOGG_SARON_P1_LEASH = 6.5f;
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
// How far a pet pulled off a Crusher will look for something else to hit. It has to find something:
// an idle pet is one PetAI update away from re-acquiring, and its first two picks are whoever is
// hitting it and whoever its owner is hitting - both the Crusher the guard just pulled it from.
constexpr float ULDUAR_YOGG_SARON_PET_TARGET_RADIUS = 40.0f;

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

// Sara hits 0 and Yogg is summoned invisible in the same tick, then ACTION_YOGG_SARON_APPEAR lights
// the ring at the end of the transformation dialogue: 4 + 5 + 4.5 + 4 s of it plus the 500 ms
// EVENT_SARA_P2_START. Measured 18.2 / 18.0 / 18.3 s over three pulls.
//
// Melee and the tank are standing in the ring when it appears in every pull on record - 9 of 9 melee
// and the tank, against 0 of 10 ranged and 0 of 4 healers, who are already out on the 21.5 yd
// station. So the walk out belongs to melee and the tank alone.
//
// It is led rather than immediate. Leftover Guardians stop counting for Sara the moment she dies but
// their novas still land for 25k, and one dying at the clearance radius reaches the back line where
// one dying on the leash cannot - so the raid holds the middle until the last few seconds.
constexpr uint32 ULDUAR_YOGG_SARON_HANDOVER_MS = 18000;
constexpr uint32 ULDUAR_YOGG_SARON_HANDOVER_LEAD_FLOOR_MS = 4000;
constexpr float ULDUAR_YOGG_SARON_HANDOVER_LEAD_SAFETY = 2.0f;

// And how long the walk out keeps claiming the bot after the ring has actually lit. Without it the
// window closes on the same tick the barrier lands: three of nine melee were back inside 13.3 yd
// within 600 ms of the knock back starting, because reach melee took the very next tick.
constexpr uint32 ULDUAR_YOGG_SARON_HANDOVER_HOLD_MS = 6000;

// Where a bot crossing the room is sent instead of straight through the body. Wide enough that both
// legs of the detour keep their distance: the worst case is a half-turn, whose chord passes
// 24 * cos(45 deg) = 17.0 yd from the middle.
constexpr float ULDUAR_YOGG_SARON_BODY_DETOUR_RADIUS = 24.0f;

// Brain Link ties two raiders together for 30 s. Past 20 yd apart both take 63803 and lose 2 Sanity a
// second; inside it neither takes anything. The bot closes to a margin under the threshold so ordinary
// drift does not re-break a link it has just mended.
//
// The aura script keeps the partner's GUID to itself, but it casts on that partner once a second
// either way - 63803 while the two are apart, 63804 while they are together - so the pair is readable
// from the cast even though it is not readable from any aura. The TTL is three missed ticks.
// Guessing instead costs: one pull had nine of ten links sitting past 20 yd for their full 30 s,
// 477,727 damage, with the owner closing on whoever was nearest rather than on its partner.
constexpr float ULDUAR_YOGG_SARON_BRAIN_LINK_RANGE = 20.0f;
constexpr float ULDUAR_YOGG_SARON_BRAIN_LINK_CLOSE = 15.0f;
constexpr uint32 ULDUAR_YOGG_SARON_BRAIN_LINK_PAIR_TTL_MS = 3000;

// Hand of Protection frees a Squeeze victim outright. It grants physical school immunity and carries
// SPELL_ATTR1_IMMUNITY_PURGES_EFFECT, Squeeze is physical and dispellable, and the Squeeze aura script
// kills the tentacle on any removal - which is what unseats the passenger. 30 yd is the spell's own
// range, and a bot outside it has nothing to offer.
//
// The claim is keyed per victim, not per window: grabs overlap, one pull held two raiders at once for
// 20 s, and three paladins spending three two-minute cooldowns on one tentacle helps nobody.
constexpr float ULDUAR_YOGG_SARON_HAND_OF_PROTECTION_RANGE = 30.0f;
constexpr uint32 ULDUAR_YOGG_SARON_SQUEEZE_CLAIM_MS = 3000;

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

// One number for "I have arrived" and for "I can click this", because two disagreed: a bot jittering
// between 2 and 3 yd read as holding while the click test found nothing, and flipped holding to late
// and back every 0.7 s for a whole 25 s window without ever taking the portal.
constexpr float ULDUAR_YOGG_SARON_PORTAL_CLICK_RADIUS = 2.0f;

// AddPortals spawns RAID_MODE(4, 10), which is the first four table entries in 10-man and all ten in
// 25-man, so the brain team is capped by how many portals actually exist.
constexpr uint32 ULDUAR_YOGG_SARON_PORTAL_SPOTS_10MAN = 4;
constexpr uint32 ULDUAR_YOGG_SARON_PORTAL_SPOTS_25MAN = 10;

// Lunatic Gaze off a Laughing Skull: 64167 on the skull triggers 64168 every 1000 ms for 1750 shadow
// damage and -2 Sanity at 30 yd. The skull is UNIT_FLAG_NOT_SELECTABLE and cannot be killed, and the
// spell picks its targets with HasInArc(M_PI, caster), so facing away is the only defence there is.
constexpr float ULDUAR_YOGG_SARON_LAUGHING_SKULL_RADIUS = 30.0f;

// How far a heading may drift before it is worth correcting, about 6 degrees. Re-facing costs a
// spline, and a node that re-faces on every tick fights the engine's own target facing instead of the
// skull: one pull had a bot flip between two orientations once a second for 48 seconds.
constexpr float ULDUAR_YOGG_SARON_FACING_TOLERANCE = 0.1f;

// Facing a skull is decided by where the bot stands, not by what it points at: set facing,
// AttackAction and PlayerbotAI::CastSpell each turn it back at its target inside the same tick. So the
// answer is a sidestep around the tentacle, and 15 yd is enough of one - one pull measured a median
// 4.3 yd from the tentacle against 22.4 yd to the skull, with the two inside 90 degrees of each other
// in 459 of 640 samples.
constexpr float ULDUAR_YOGG_SARON_ILLUSION_FACING_SEARCH_RADIUS = 15.0f;

// The sweep needs a circle to ring outward from, and a bot has no business standing under a skull
// anyway. Small on purpose: the constraint that matters is angular, not radial.
constexpr float ULDUAR_YOGG_SARON_SKULL_CLEAR_RADIUS = 5.0f;

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
// The instance boss state is part of the read. Sara is a static friendly spawn that LoadAllGrids
// makes findable from instance creation and that lives on at 1 health into phases 2 and 3, so her
// being there says nothing on its own.
//
// Her combat flag says nothing either, which is what one build shipped and cost 24 s of every pull.
// She is FACTION_FRIENDLY for all of phase 1 and CombatManager::CanBeginCombat refuses a combat
// reference while either side is friendly and neither is hostile, so InitFight's SetInCombatWithZone
// puts her summons in combat and leaves her out of it. Her flag only comes up when her own phase 1
// casting first lands on somebody: 15 s of EVENT_SARA_P1_DOORS_CLOSE plus a 4 s cast at the very
// earliest, and 24.2 s on the pull that found it. GetBossState is IN_PROGRESS from InitFight itself.
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
// the spell uses to pick its targets. The distance is re-checked after the grid sweep, whose own range
// test is bounding-radius inclusive: 164 of 235 probe flips in one pull had no skull inside 30 yd.
std::vector<Unit*> GetYoggSaronSkullsInArc(PlayerbotAI* botAI);

// The same skulls without the arc test, for deciding where to stand rather than what is hitting the
// bot now. A candidate spot is judged on the heading it would force, so the bot's current facing has
// no bearing on which skulls matter.
std::vector<Position> GetYoggSaronSkullsInRange(PlayerbotAI* botAI);

// Whether standing at (x, y) and facing (targetX, targetY) leaves every one of `skulls` outside the
// front 180 degrees, which is the exact filter spell_yogg_saron_lunatic_gaze picks its targets with.
bool YoggSaronFacingClearOfSkulls(std::vector<Position> const& skulls, float x, float y, float targetX,
                                  float targetY);

// The raider a Squeeze rescue should be spent on: lowest health first, nearest as the tie-break, in
// Hand of Protection's range, and without Forbearance. Never the bot itself - a paladin who is held
// bubbles instead. nullptr when there is nobody worth the cooldown.
Player* YoggSaronSqueezeVictim(PlayerbotAI* botAI);

// One rescuer per victim. True for the bot that takes the claim and false for everyone else until it
// lapses.
bool ClaimYoggSaronSqueezeRescue(PlayerbotAI* botAI, Player* victim);

// Somewhere other than a Crusher Tentacle for a pet to be, nearest first. nullptr when the pet has
// nothing else within reach, which is the only case worth silencing it for.
Unit* YoggSaronPetFallbackTarget(PlayerbotAI* botAI, Creature* pet);

// Record a Brain Link pair, from the spell hook that sees 63803 or 63804 go out. Both ends read the
// same record, so both walk.
void YoggSaronNoteBrainLinkPair(Unit* owner, Unit* partner);

// Who a Brain Linked bot should close on, whichever end of the pair it is, or nullptr when it is not
// linked, is already close enough, or has gone underground - the script drops the link outright past
// 10 yd of vertical separation.
Player* YoggSaronBrainLinkTarget(PlayerbotAI* botAI);

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

// The window between Sara dying and Yogg emerging, and how long is left of it. Yogg being here
// without SPELL_SHADOW_BARRIER is the whole of it, because ACTION_YOGG_SARON_APPEAR casts the barrier
// and the knockback in one call. SetVisible(false) does not hide him from a grid search, so the
// window is readable from its first tick. msToRing is predicted off the first sighting the way the
// portal clock predicts its first wave, since nothing in the world counts it down.
struct YoggSaronHandover
{
    bool active = false;
    // Whether it is time for this bot to walk out, which is the lead decided here rather than by each
    // caller, so the trigger and the action cannot answer it differently. Never true for a bot that is
    // already outside the clearance radius, which is the whole back line.
    //
    // It stays true for a hold after the ring has lit, because the window itself ends on that tick and
    // a bot released there walks straight back under the knock back.
    bool clearing = false;
    uint32 msToRing = 0;
};

YoggSaronHandover YoggSaronHandoverState(PlayerbotAI* botAI);

// The body's knockback ring, which has no world object behind it and so cannot be swept for.
bool YoggSaronInBodyKnockback(Player* player);
bool YoggSaronRouteClearOfBody(Player* bot, float x, float y);

// A point to cross the room through when the straight line would go over the body. One is enough:
// each leg halves the turn the next one has to make, so the bot walks the arc rather than the chord.
bool YoggSaronBodyDetour(Player* bot, Position const& destination, Position& waypoint);

// Any live Influence Tentacle within `radius`, disguise included. A tentacle is re-stamped as a Suit
// of Armor, a Deathsworn Zealot or a Consort the instant it spawns and only reverts to entry 33943
// when something damages it, so a sweep for 33943 alone reports a room full of them as empty. One
// grid visit for the whole list.
Unit* YoggSaronLiveIllusionMob(PlayerbotAI* botAI, float radius);

// Whether the Brain is safe to hit: every Influence Tentacle in this room dead. Scoped to the room
// rather than swept at 200 yd, which is one yard short of reaching the next room's tentacles.
bool YoggSaronInfluenceTentaclesCleared(PlayerbotAI* botAI);

// Live Crusher Tentacles to angle away from, each as its position plus the facing its Crush cone
// follows. The one currently hitting the bot is left out: that bot is hit wherever it stands, and
// moving only drags the cone around behind it.
//
// A Crusher with nothing inside its melee range is left out too. Crush is a 100% proc on the
// tentacle's own white swing and UpdateAI will not swing at a victim out of melee range, so an
// unoccupied Crusher cannot produce a cone at all: one pull spent 251 hazard rows routing 25 bots
// around empty floor. Every one of its six cones fired with no player inside 12 yd and a pet at 5.5.
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
