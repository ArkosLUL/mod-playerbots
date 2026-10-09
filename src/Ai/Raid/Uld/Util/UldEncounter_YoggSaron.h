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
    SPELL_DIMINISH_POWER = 64145,  // Crusher's 5 min channel, -21% damage done raid-wide
    SPELL_LUNATIC_GAZE_SKULL = 64168,  // 64167 on a Laughing Skull fires this every second, 30 yd
    SPELL_SHADOW_NOVA_SARA = 65719,  // only cast from a Guardian's JustDied, so the cast is its death
    SPELL_SHATTERED_ILLUSION = 64173,  // stun on every platform tentacle once a brain room is cleared
    SPELL_SHADOW_BEACON = 64465,  // 10 s mark on a Guardian, entry swapped to 36064 while it lasts
    // Landed by Yogg's missile right after the mark ends: 37,500 a second for 20 s (25), on every ally
    // within 20 yd of the marked Guardian when it lands. One application, not an area aura.
    SPELL_EMPOWERING_SHADOWS_10 = 64468,
    SPELL_EMPOWERING_SHADOWS_25 = 64486,
    SPELL_GUARDIAN_DRAIN_LIFE = 64159,  // 5 s channel below 85% health, heals the Guardian 10x the drain
    SPELL_GUARDIAN_DRAIN_LIFE_H = 64160,
    // +100% to the caster's own aura for 6 s (aura 108 on all three effects, 99 base points), so it is
    // worth something only on the paladin carrying Shadow Resistance Aura. With the caster's own
    // Concentration Aura up it also casts 64364, silence and interrupt immunity for 40 yd, but only on
    // raiders carrying that caster's copy of the aura.
    SPELL_AURA_MASTERY = 31821,
    SPELL_CONCENTRATION_AURA = 19746,
    // 25-man with fewer than 4 Keepers: 2.3 s cast, then 4 s of silence within 50 yd.
    SPELL_DEAFENING_ROAR = 64189,
    SPELL_DISTRACTING_SHOT = 20736,  // 6 s taunt on an 8 s cooldown, 35 yd
    SPELL_HEROISM_SHAMAN = 32182,
    SPELL_BLOODLUST_SHAMAN = 2825,
    SPELL_SATED = 57724,
    SPELL_EXHAUSTION = 57723,
    ITEM_VALANYR = 46017,  // Val'anyr, Hammer of Ancient Kings
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

// Under this a bot sits the next wave out and refills at a well. A trip down cost a median 22 Sanity, 64
// at worst, and there's no time to top up between waves: the next portals open ~22 s after the team is
// back, with a 35 yd walk to the nearest well and 65 yd from it to a portal spot.
constexpr uint32 ULDUAR_YOGG_SARON_BRAIN_TEAM_MIN_SANITY = 60;

// Nitro Boosts (boots enchant), on the walk from a cleared illusion room to the Brain. Shorter walks end
// before 5 s of +150% buys anything. Chamber only: 3 min cooldown against a Chamber every 240 s, so it's
// always ready for the longest walk out, where it used to go to whichever room came first.
constexpr float ULDUAR_YOGG_SARON_NITRO_MIN_WALK = 40.0f;

// A warrior's charge at the Brain lands on the floor ring at both combat reaches from it (31.5 yd), so
// only worth breaking the walk for when that ring is at least this much nearer the bot's brain spot.
constexpr float ULDUAR_YOGG_SARON_BRAIN_CHARGE_MIN_GAIN = 8.0f;

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

// How many points along a walk are checked against where the clouds will be at that point. Eight puts
// the longest walk in the room, the run in from the door at about 17 s, inside one lead per step.
constexpr uint32 ULDUAR_YOGG_SARON_WALK_CLOUD_STEPS = 8;

// Shadow Nova, the Guardian's death explosion: DBC radius 15, plus both object sizes at apply time,
// which measured 16.2 at its furthest. Ranged and healers stay out of it; melee and tanks have to eat
// it to kill the thing at all. The trigger sits inside the clear radius, and that band is margin
// rather than exposure: a bot 18 yd out is not moved to 20, but 18 is already past the blast.
constexpr float ULDUAR_YOGG_SARON_SHADOW_NOVA_TRIGGER_RADIUS = 17.0f;
constexpr float ULDUAR_YOGG_SARON_SHADOW_NOVA_CLEAR_RADIUS = 20.0f;

// How close a live nova keeps a Sara's Fervor holder from walking back in: the clear radius plus 2 yd of
// slack. With only the dodge, the station or the leash walked the bot straight back past 17 yd and 31
// of 44 runs were back inside the blast while still holding Fervor.
constexpr float ULDUAR_YOGG_SARON_FERVOR_HOLD_RADIUS = 22.0f;

// A Guardian at or under this is about to detonate. Running from every Guardian instead scatters the
// raid to the rim and leaves bots to be picked off one at a time.
constexpr float ULDUAR_YOGG_SARON_GUARDIAN_NOVA_HEALTH_PCT = 20.0f;

// Where a Sara's Fervor holder leaves, earlier than anyone else because the nova hits it twice as hard.
// The widest gate whose worst case still covers the walk out: over 98 novas exactly one warning under
// 50% came in under the ~2.5 s a 15 yd walk takes, against 30 of them under 20%.
constexpr float ULDUAR_YOGG_SARON_FERVOR_NOVA_HEALTH_PCT = 50.0f;

// What one nova does when it resists nothing, which is the only figure worth planning against:
// resistance is rolled per hit, so the median is survivable and the hits that kill are the ones that
// roll none. Over 138 hits the damage before resistance ran a 23,547 median to a 27,034 ordinary
// maximum, 9 of them resisted nothing, and a ret paladin died from a verified full 28,582 bar on a
// recorded 26,692 - 65209 has two SCHOOL_DAMAGE effects and the trace records one row per raider, so
// the real hit is higher than any row shows. Below this figure a bot at full health never looked at
// risk, so the ladder only ever fired after the hit.
//
// Still the unresisted figure, and what a bot carrying no shadow resistance takes. A raid that has the
// 130 never sees it: over 892 hits the resisted share ran a 21.5% median and never under 10.0%, so the
// worst such a bot can roll is the second figure. Planning them against the first flagged a melee bot
// at full health that nothing on the table could kill, and the raid emptied its externals on novas
// that killed nobody. The gate reads the resistance itself rather than the aura, because Prayer of
// Shadow Protection, the paladin aura and gear all land on the same exclusive aura 143 and only the
// highest of them counts.
constexpr uint32 ULDUAR_YOGG_SARON_NOVA_EXPECTED_DAMAGE = 28600;
constexpr uint32 ULDUAR_YOGG_SARON_NOVA_RESISTED_DAMAGE = 25740;
constexpr uint32 ULDUAR_YOGG_SARON_NOVA_RESISTED_AT = 130;

// Two gates. A Guardian first dropping under 20% detonated a median 4.8 s later and as late as 11.5,
// under 10% a median 2.4 s and never later than 3.7 over one pull's 11 novas. So a bot spends its own
// button and a healer its external only at 10%, where a 5 s window always covers the hit and a Guardian
// sits for far less time; Divine Sacrifice still goes at 20%, because one cast covers 25 raiders for
// 10 s and is worth starting early. Everything in the ladder is instant, so even 0.3 s is castable.
constexpr float ULDUAR_YOGG_SARON_NOVA_SOON_HEALTH_PCT = 20.0f;
constexpr float ULDUAR_YOGG_SARON_NOVA_NOW_HEALTH_PCT = 10.0f;
constexpr uint32 ULDUAR_YOGG_SARON_NOVA_SOON_WINDOW_MS = 12000;
constexpr uint32 ULDUAR_YOGG_SARON_NOVA_NOW_WINDOW_MS = 5000;

// Counting past two low Guardians buys nothing: a pair already covers every death measured.
constexpr uint32 ULDUAR_YOGG_SARON_NOVA_MAX_CHAIN = 2;

// Divine Sacrifice is the only raid-wide reduction that exists on this core, and one cast covers 25
// raiders, so it waits until the nova threatens this many of them. Its own 10 s is the claim window. The
// 20% comes from the Divine Guardian talent, which procs 70940 as a raid area aura with value -20 - the
// spell id appears nowhere in the core source, so only the DBC says so. Measured: it reached all 25 on
// every cast, and the damage the split put back on the paladin never took one below 62%.
constexpr uint32 ULDUAR_YOGG_SARON_NOVA_RAID_COUNT = 3;

// How long one claim on a Guardian lasts, for the cover and the external alike: enough to outlive the
// Guardian that took it, which detonates within 3.7 s of passing the tighter gate. Keying it on a
// stretch of wall clock instead is what left the second Guardian of a pair uncovered. They die 1.8-4.5
// s apart, one pull put 32 such pairs on a single bar and 8 of them took more than a whole bar, and
// the bot holding the window had already spent its only button while two Divine Sacrifices sat ready.
constexpr uint32 ULDUAR_YOGG_SARON_NOVA_WINDOW_MS = 10000;

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

// Past the outer orbit's reach no cloud can touch a bot, so phase 1 positioning waits until the bot is
// inside it. The core opens the pull from 90 yd and bots used to run in from there before anyone was
// fighting. Combat is no gate: every Guardian's Reset puts the whole instance in combat.
constexpr float ULDUAR_YOGG_SARON_P1_ROOM_RADIUS =
    ULDUAR_YOGG_SARON_CLOUD_ORBITS[5] + ULDUAR_YOGG_SARON_CLOUD_SUMMON_REACH;

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

// A melee bot that is not tanking gets a tighter one, because none of the above is about its own feet:
// the inner orbit is 11.6 yd out and a cloud wakes to anyone inside 8.5, so past this radius the pile
// summons a Guardian by standing still. It did: five melee at 3.8 yd woke one in the last pull and
// another at 5.2 yd woke the next, and each bot sat outside the radius 27-48% of the phase although
// the pile's median was 1.79 yd. The release has to clear it by more than a step or reach melee drags
// the bot back out and the two trade the tick.
constexpr float ULDUAR_YOGG_SARON_P1_PILE_LEASH =
    ULDUAR_YOGG_SARON_CLOUD_ORBITS[0] - ULDUAR_YOGG_SARON_CLOUD_SUMMON_REACH;
constexpr float ULDUAR_YOGG_SARON_P1_PILE_RELEASE = 2.0f;

// How far the fetch can reach: Taunt 355, Hand of Reckoning 62124, Dark Command 56222 and Growl 6795
// all share spell range index 4, so 30 yd whoever is tanking. Without it the pick is whatever is
// lowest on health anywhere in sight range, and a Guardian a pet is grinding down at the wall wins
// that forever - one pull aimed 83 s of taunts at one 48 yd away and landed none of them.
constexpr float ULDUAR_YOGG_SARON_P1_TAUNT_RANGE = 30.0f;

// The other of those two 15s: how far 65719 reaches Sara, so how far out a kill still counts. It is
// the wider one on purpose - the leash decides where a bot stands, this decides when the raid gives
// up on a Guardian, and keeping them apart is what stops a focus flipping on a yard of drift.
constexpr float ULDUAR_YOGG_SARON_P1_SARA_NOVA_RADIUS = 15.0f;

// A Guardian outside the leash is only worn down to here, then left for the tank to fetch. The fetch
// is ~12 s (8 s taunt cooldown, the walk in, a taunted Guardian that stood still for 2 s) and an
// untargeted Guardian near the station still loses up to ~3%/s to splash.
constexpr float ULDUAR_YOGG_SARON_P1_PARK_HEALTH_PCT = 35.0f;

// Minimum time between two Guardian novas. Melee stand in every one on the stack: three in 3.4 s took
// two of them from full, the pull with no phase 1 deaths never had two closer than 6 s.
constexpr uint32 ULDUAR_YOGG_SARON_P1_NOVA_GAP_MS = 6000;

// Below this a Guardian is close enough to dying that the gap has to protect it. Held Guardians on the
// stack still lost 4.2-7.9%/s to the tank, humans, DoTs and cleaves, so in practice it buys ~3 s, not 6.
constexpr float ULDUAR_YOGG_SARON_P1_NOVA_GAP_HEALTH_PCT = 30.0f;

// A Guardian's nova also hits every other Guardian within 15 yd for 25,000-27,501 (65209 effect 1, a
// conditions row on entry 33136), ~2.7% of one. The focus dying 13.8 yd from one at 0.9% killed both in
// the same tick, and two melee with them. 18 covers the 16.2 yd measured on players.
constexpr float ULDUAR_YOGG_SARON_P1_NOVA_CHAIN_RADIUS = 18.0f;

// Death and Decay and Explosive Trap reach 10, Blizzard and Consecration 8.
constexpr float ULDUAR_YOGG_SARON_P1_AOE_HOLD_RADIUS = 10.0f;

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

// How far from Yogg's middle the phase 3 pets look for leftover tentacles. Wave tentacles spawn up to
// ~50 yd out.
constexpr float ULDUAR_YOGG_SARON_PET_LEFTOVER_SEARCH_RADIUS = 70.0f;

constexpr float ULDUAR_YOGG_SARON_CRUSH_RANGE = 25.0f;       // DBC radius 23 plus both object sizes
constexpr float ULDUAR_YOGG_SARON_CRUSH_TRIGGER_ARC = 8.0f;  // degrees either side of the facing
constexpr float ULDUAR_YOGG_SARON_CRUSH_CLEAR_ARC = 14.0f;   // ~4 yd of lateral room at 20 yd

// The Crusher's own melee range on a player, centre to centre: CombatReach 8 (display 28814) + 1.5 +
// 4/3 = 10.83, no leeway since it never moves. Melee only get it for a stun about to lift. Inside it a
// caster's weapon swing or a hunter's Raptor Strike lands, any melee hit makes that bot the tentacle's
// victim, and the next Crush lands on it: 3 of 3 Crush kills in one pull, all ranged at 5.8-6.8 yd.
constexpr float ULDUAR_YOGG_SARON_CRUSHER_REACH_TRIGGER_RADIUS = 11.0f;
constexpr float ULDUAR_YOGG_SARON_CRUSHER_REACH_CLEAR_RADIUS = 13.0f;

// Shattered Illusion lifts at Induce Madness end, or the moment the Brain drops under 30% and phase 3
// starts. A Crusher swings as soon as it's free, at whoever hit it last, so melee need to be out of its
// reach by then. 13 yd is under 2 s at run speed, doubled for the tick and the path.
constexpr uint32 ULDUAR_YOGG_SARON_STUN_EXIT_LEAD_MS = 4000;
// Fastest the Brain went down in one pull was 5.34% in 5 s, so the lead above can cost ~4.3%. Not while
// the brain team holds it, see ULDUAR_YOGG_SARON_BRAIN_HOLD_PCT.
constexpr float ULDUAR_YOGG_SARON_STUN_BRAIN_FLOOR_PCT = 35.0f;

// The brain team stops short of 30% while anything lives on the platform, so the stun runs on instead of
// ending with phase 3: the last wave of two pulls hit 30% 43-45 s in and left 2.45M and 3.12M of
// tentacles for phase 3. DoTs stop earlier since they keep ticking. The finish starts with this much
// Induce Madness left, ~2% in ~2 s, before the exit lead. Under the guard a held Brain is going under
// anyway, a human or a pet still on it, so the stun is read as if nobody held.
constexpr float ULDUAR_YOGG_SARON_BRAIN_HOLD_PCT = 32.0f;
constexpr float ULDUAR_YOGG_SARON_BRAIN_DOT_STOP_PCT = 36.0f;
constexpr uint32 ULDUAR_YOGG_SARON_BRAIN_RESUME_MS = 8000;
constexpr float ULDUAR_YOGG_SARON_BRAIN_HOLD_GUARD_PCT = 31.0f;

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
// EVENT_SARA_P2_START. Measured 18.2 / 18.0 / 18.3 s over three pulls, though nothing here counts it
// down: the window is read off the Brain being absent.
//
// Melee and the tank are standing in the ring when it appears in every pull on record - 9 of 9 melee
// and the tank, against 0 of 10 ranged and 0 of 4 healers, who are already out on the 21.5 yd
// station. So the walk out belongs to melee and the tank alone, and they go the moment Sara dies.
// Leading it to the last few seconds left two and three of eleven still inside the ring when it lit,
// and what the lead bought, keeping a Guardian's nova off the back line while melee held the middle,
// the anchor's 25 yd of clearance buys anyway.
//
// How long the walk keeps claiming the bot after the ring has actually lit. Without it the
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

// Headings tried when turning a bot with no target off the skulls, 22.5 degrees apart.
constexpr uint32 ULDUAR_YOGG_SARON_SKULL_HEADINGS = 16;

// How far a bot with a target may turn off it to get a skull out of its front half: 40 degrees, under
// set facing's 45, so nothing turns it back. AttackAction and CastSpell allow 60.
constexpr float ULDUAR_YOGG_SARON_SKULL_TARGET_TURN = 0.6981f;
constexpr float ULDUAR_YOGG_SARON_SKULL_TARGET_STEP = 0.0873f;

// A skull straight behind the target can't be turned from, only walked round, and 15 yd is enough of a
// sidestep: one pull measured a median 4.3 yd from the tentacle against 22.4 yd to the skull, with the
// two inside 90 degrees of each other in 459 of 640 samples.
constexpr float ULDUAR_YOGG_SARON_ILLUSION_FACING_SEARCH_RADIUS = 15.0f;

// The sweep needs a circle to ring outward from, and a bot has no business standing under a skull
// anyway. Small on purpose: the constraint that matters is angular, not radial.
constexpr float ULDUAR_YOGG_SARON_SKULL_CLEAR_RADIUS = 5.0f;

// How far the brain team's healer may stray from its illusion room's middle before it walks back.
// Every tentacle sits within 43 yd of the middle, and a heal reach step toward a mate on the far side of
// the Chamber stays inside this.
constexpr float ULDUAR_YOGG_SARON_ILLUSION_HEALER_STATION_RADIUS = 10.0f;

// Close enough to the spot the Brain is hit from to stop walking. The melee spot has 2.7 yd of melee
// range in hand, so a bot parked this far off it still reaches.
constexpr float ULDUAR_YOGG_SARON_BRAIN_SPOT_ARRIVED_RADIUS = 3.0f;

// How early to leave the brain level before Induce Madness lands. It strips all 100 Sanity from
// anyone at or below z 300, and no Sanity means Insane, whose removal kills the player outright - so
// a mind control is always a death. The lead is taken out of the window the raid has to damage the
// Brain, so it is measured rather than flat: a bot standing on a portal needs the floor, one just in
// from the Chamber door is about 44 yd out. At 2x and 5 s bots left with a median 6.7 s and surfaced
// with 4.7 s; the walk out took a median 2.2 s, 5.4 s at worst with 2 s of it stood at the portal.
//
// Timed at base run speed. Illusion Room (63988) adds 30% down there, but bots made 4-6 yd/s on the way
// out once casts and refused moves counted, and the faster number said "safe" with no time left: five
// charmed in one pull, all with Sanity to spare.
constexpr uint32 ULDUAR_YOGG_SARON_EXIT_LEAD_FLOOR_MS = 3000;
constexpr float ULDUAR_YOGG_SARON_EXIT_LEAD_SAFETY = 1.5f;

// Phase 3 station. The radius is what the bot is allowed to drift inside, the leash is how far a tank
// may wander before it is walked back - wider because a tank chasing a guardian to the room's edge is
// doing its job.
constexpr float ULDUAR_YOGG_SARON_PHASE_3_STATION_RADIUS = 15.0f;
constexpr float ULDUAR_YOGG_SARON_PHASE_3_TANK_LEASH = 30.0f;

// How far the ranged station keeps from a live Crusher. Inside its 10.83 yd reach a ranged bot's first
// hit makes it the victim, and the Crush cone runs through everyone stacked behind it: seven dead in 2 s
// on a station 7.3 yd from a leftover one. 15.8 is a hunter's Auto Shot dead zone, plus 2 for where
// bots actually stop.
constexpr float ULDUAR_YOGG_SARON_PHASE_3_RANGED_CRUSHER_CLEARANCE = 18.0f;

// How close to the melee spot a Guardian must be for phase 3 melee to take it, during a gaze or once
// beacon-healed. Guardian control keeps the tank within 5 yd of the spot and a full-health Guardian
// swings from up to 14 yd.
constexpr float ULDUAR_YOGG_SARON_PHASE_3_MELEE_GUARDIAN_RANGE = 20.0f;

// Without Thorim a Guardian can't die, and a fresh or beacon-healed one is 425k: bringing every one to
// Weakened took 75% of a pull's damage and left Yogg 3%. So only down to the floor, where it hits about
// a third as hard as at full, and the rest goes on Yogg. Under the Weakened line it hits for ~2k.
constexpr float ULDUAR_YOGG_SARON_PHASE_3_GUARDIAN_FLOOR_PCT = 30.0f;
constexpr float ULDUAR_YOGG_SARON_GUARDIAN_WEAKENED_PCT = 10.0f;

// Phase 3 area spells need this many units worth hitting within the area trigger's 8 yd of the target:
// Yogg, a leftover tentacle, a Guardian over the floor that a tank holds. The trigger counts anything,
// and Yogg's 30 yd reach puts him in every count at the pile, so a lone Guardian got Blizzard and Seed
// of Corruption where a single-target spell takes it to the floor faster.
constexpr float ULDUAR_YOGG_SARON_PHASE_3_AREA_RADIUS = 8.0f;
constexpr uint32 ULDUAR_YOGG_SARON_PHASE_3_AREA_MIN_WORTH = 3;
constexpr float ULDUAR_YOGG_SARON_COMBAT_REACH = 30.0f;

// A non-tank starts on an Immortal Guardian only while its own threat on it is under the start share of
// the tank holding it, and lets go past the keep share. Taking one over needs 110% in melee and 130% at
// range. Without a gate one pull put 36 swings of 26-41k on non-tanks, 11 of them killing blows.
constexpr float ULDUAR_YOGG_SARON_GUARDIAN_THREAT_START_SHARE = 0.8f;
constexpr float ULDUAR_YOGG_SARON_GUARDIAN_THREAT_KEEP_SHARE = 0.95f;

// The tank leaves the Guardian it is hitting only once its lead there, over the highest non-tank, is past
// this, and only for one where the lead is thinner. 1.3 is what pulls one at range. Without switching the
// raid waits on the newest Guardian until the oldest reaches Weakened.
constexpr float ULDUAR_YOGG_SARON_TANK_THREAT_SWITCH_LEAD = 1.5f;

// Smallest angle between a bot's target and Yogg, seen from the bot, that still has a heading with the
// target inside set facing's 45 degrees and Yogg outside the gaze's 90. 65 degrees, which leaves 10 on
// each side.
constexpr float ULDUAR_YOGG_SARON_GAZE_SAFE_SEPARATION = 1.1345f;

// How long a forced walk may fail to close distance before the bot stops trying, and how close counts
// as having got there. The arrival radius is generous because MoveTo stops where its own tolerance
// leaves the bot, not on the point it was handed.
constexpr uint32 ULDUAR_YOGG_SARON_WALK_GIVE_UP_MS = 6000;
constexpr float ULDUAR_YOGG_SARON_WALK_ARRIVED_RADIUS = 5.0f;

// How long a given-up walk stays given up. Without it the give-up is permanent: the escape below it
// needs a gap in the asking and these nodes ask every tick, so six bots in one pull gave up the walk
// into the Chamber, stood still for 21 s and were charmed where they stopped. Standing still below the
// platform is a death, so a destination worth retrying beats a bot that has stopped for good.
constexpr uint32 ULDUAR_YOGG_SARON_WALK_RETRY_MS = 4000;

// How long a bot must have stood still before its unfinished MoveTo booking is dropped. A spline that
// is still running keeps isMoving() true, so this only ever fires on a walk that has already died.
constexpr uint32 ULDUAR_YOGG_SARON_WALK_STALL_MS = 500;

// Grim Reprisal sends 60% of every direct hit on a tentacle back at whoever landed it, DoT ticks
// excepted, so a damage dealer below the stop line stays off the tentacles until healed past the resume
// line. The worst single burst measured was 35% of max health in 0.2 s (a Windfury chain), and six
// melee died in one pull still swinging at 21-39%.
constexpr float ULDUAR_YOGG_SARON_REPRISAL_STOP_PCT = 40.0f;
constexpr float ULDUAR_YOGG_SARON_REPRISAL_RESUME_PCT = 70.0f;

// Induce Madness time left under which nobody holds back. An uncleared room has no way out, so a
// tentacle left alive at the end charms everyone in the room, held or not.
constexpr uint32 ULDUAR_YOGG_SARON_REPRISAL_ALL_IN_MS = 15000;

// A held bot further than this from its room's healer walks toward it, and stops about this short of it.
// Inside the 38.5 yd heal range with room for both to move: four of the six melee that died to
// reprisal in one pull had the healer 37-50 yd away.
constexpr float ULDUAR_YOGG_SARON_REPRISAL_REGROUP_RANGE = 25.0f;
constexpr float ULDUAR_YOGG_SARON_REPRISAL_REGROUP_STOP = 15.0f;

// Stormwind bots whose first tentacle sits inside this of the stage spot keep clearing once it dies,
// the rest wait at the door. Takes the two at 31 yd, the next ones are 53 out.
constexpr float ULDUAR_YOGG_SARON_STORMWIND_HELPER_RADIUS = 40.0f;

// Induce Madness time left under which the Icecrown and Stormwind shares lift and everyone clears
// whatever's alive. Slowest clear on record left about 29 s, and an uncleared room charms everyone in it.
constexpr uint32 ULDUAR_YOGG_SARON_SHARE_LIFT_MS = 25000;

// Close enough to the stage spot. Small because the spot is only ~3 yd inside either room circle.
constexpr float ULDUAR_YOGG_SARON_STAGE_ARRIVED_RADIUS = 1.5f;

// How often the per-bot exposure probes resample. They answer "where was this bot standing when the
// thing that killed it went off", so a second is fine and a tick is 25 bots of noise.
constexpr uint32 ULDUAR_YOGG_SARON_OBS_SCAN_INTERVAL_MS = 1000;

// How far out to look for a Guardian worth kicking. Wider than any interrupt's range on purpose - the
// action drops the ones it cannot reach, and a short list here would hide a cast from a bot who could.
constexpr float ULDUAR_YOGG_SARON_INTERRUPT_SEARCH_RADIUS = 40.0f;

// How long one bot's kick holds a cast for itself, so the next bot spends its cooldown elsewhere.
constexpr uint32 ULDUAR_YOGG_SARON_INTERRUPT_CLAIM_MS = 1500;

// Phase 3 without Thorim: how far from Yogg to look for Shadow Beacon marked Guardians, and how long a
// dragger holds one after the mark ends if the heal hasn't shown up. Yogg's missile goes out the moment
// the mark ends and flies at 30 yd/s, 0.3-1.1 s from the drag spots.
constexpr float ULDUAR_YOGG_SARON_BEACON_DRAG_SEARCH_RADIUS = 70.0f;
constexpr uint32 ULDUAR_YOGG_SARON_BEACON_HEAL_WAIT_MS = 3000;
constexpr float ULDUAR_YOGG_SARON_BEACON_DRAG_ARRIVED_RADIUS = 3.0f;

// A drag whose taunt hasn't turned its Guardian this long after the cast goes to the next bot. One pull's
// Hand of Reckoning missed and the dragger kept the claim on cooldown for 8 s. Every taunt has landed by
// the second line: Distracting Shot flies at 40 yd/s, 0.85 s at its 34 yd.
constexpr uint32 ULDUAR_YOGG_SARON_BEACON_TAUNT_CHECK_MS = 1500;
constexpr uint32 ULDUAR_YOGG_SARON_BEACON_TAUNT_LAND_MS = 1000;

// Shadow Beacon comes 45 s into phase 3, then 45 s after each one that lands, retried every 5 s. The
// draggers walk out to a staging spot this long before it's due and give up this long after.
constexpr uint32 ULDUAR_YOGG_SARON_BEACON_PERIOD_MS = 45000;
constexpr uint32 ULDUAR_YOGG_SARON_BEACON_STAGE_LEAD_MS = 5000;
constexpr uint32 ULDUAR_YOGG_SARON_BEACON_STAGE_TIMEOUT_MS = 8000;

// Kickers held back for the marks' Drain Life while a beacon lasts, ready within this long. A channelling
// Guardian doesn't move, so one channel during a drag keeps its mark in the pile, and in one pull every
// kick went on unmarked channels while two marks healed eleven Guardians.
constexpr uint32 ULDUAR_YOGG_SARON_MARK_KICKERS = 2;
constexpr uint32 ULDUAR_YOGG_SARON_MARK_KICKER_READY_MS = 2000;

// A beacon-healed Guardian climbs ~9% a second, and past this it hits a non-tank for ~17k and up. The
// 25-36k hits that killed both draggers in one pull came 5-8 s after the heal, with no tank taking it.
constexpr float ULDUAR_YOGG_SARON_BEACON_DANGER_HEALTH_PCT = 40.0f;
// How long a released dragger walks its healed Guardian back toward the tank, and how long one external
// holds its victim for itself.
constexpr uint32 ULDUAR_YOGG_SARON_BEACON_RETURN_MS = 8000;
constexpr uint32 ULDUAR_YOGG_SARON_BEACON_EXTERNAL_CLAIM_MS = 8000;
// A tank holding the pile presses its own button once this many Guardians on it carry another Guardian's
// beacon heal, at any health, and gets an external under the line. A 60% line was one full-power swing, so
// both tanks spent the 2 min button on a mild beacon and had none when a failed drag healed the pile.
constexpr uint32 ULDUAR_YOGG_SARON_BEACON_PILE_RESET_GUARDIANS = 2;
constexpr float ULDUAR_YOGG_SARON_BEACON_TANK_EXTERNAL_PCT = 50.0f;

// Both tanks stand on the melee spot, so Consecration keeps the whole pile on whoever has the most threat:
// one held 5-11 Guardians for 90 s while the other held 0-3, and the other's taunts came back within 4 s.
// So the other tank takes the pile once the holder is under the swap line with this many on it, and the
// holder stops its area threat and taunts for one Righteous Defense cooldown.
constexpr float ULDUAR_YOGG_SARON_TANK_SWAP_PCT = 50.0f;
constexpr float ULDUAR_YOGG_SARON_TANK_SWAP_TAKER_PCT = 70.0f;
constexpr uint32 ULDUAR_YOGG_SARON_TANK_SWAP_MIN_GUARDIANS = 3;
constexpr uint32 ULDUAR_YOGG_SARON_TANK_SWAP_MS = 8000;

// Lay on Hands is kept for a tank under this in phase 3, one paladin per tank per claim. Every pull spent
// all four on dps in phase 1, and a tank death is what ended phase 3.
constexpr float ULDUAR_YOGG_SARON_TANK_LOH_PCT = 20.0f;
constexpr uint32 ULDUAR_YOGG_SARON_TANK_LOH_CLAIM_MS = 3000;
constexpr float ULDUAR_YOGG_SARON_TANK_LOH_RANGE = 40.0f;

// Avenging Wrath locks Divine Protection for 30 s, so a phase 3 tank only presses it with Divine
// Protection that far from ready anyway.
constexpr uint32 ULDUAR_YOGG_SARON_TANK_WRATH_LOCK_MS = 30000;

// A Drain Life nobody has kicked by this point gets a mid-cast bot's own cast stopped so it can kick, but
// only when no free bot can and no ready one finishes its cast within the wait. 29 of 54 full channels in
// one pull had a ready kick in range on a bot busy casting. One nudge per channel.
constexpr uint32 ULDUAR_YOGG_SARON_KICK_LAST_RESORT_MS = 1500;
constexpr uint32 ULDUAR_YOGG_SARON_KICK_WAIT_MS = 1000;
constexpr uint32 ULDUAR_YOGG_SARON_KICK_NUDGE_MS = 2000;
// Hunter drags. Distracting Shot is a 6 s taunt against a 10 s mark, so it goes out with this much of the
// mark left and still covers the heal 0.3-1.1 s after it. Feign Death if the Guardian is still on the
// hunter this long after the release.
constexpr uint32 ULDUAR_YOGG_SARON_BEACON_SHOT_LEFT_MS = 5000;
constexpr uint32 ULDUAR_YOGG_SARON_BEACON_FEIGN_DELAY_MS = 1000;
constexpr float ULDUAR_YOGG_SARON_DISTRACTING_SHOT_RANGE = 34.0f;

// Deafening Roar comes 30 s into phase 3, then every 60 s. The paladin whose turn it is puts Concentration
// Aura up this early, since the immunity only reaches raiders carrying its own copy. Another paladin may
// take the roar this long after the cast starts.
constexpr uint32 ULDUAR_YOGG_SARON_KEEPER_COUNT = 4;
constexpr uint32 ULDUAR_YOGG_SARON_ROAR_FIRST_MS = 30000;
constexpr uint32 ULDUAR_YOGG_SARON_ROAR_PERIOD_MS = 60000;
constexpr uint32 ULDUAR_YOGG_SARON_ROAR_PRESWITCH_MS = 10000;
constexpr uint32 ULDUAR_YOGG_SARON_ROAR_HANDOFF_MS = 500;

// Heroism waits for the raid to be fighting in its phase 3 spots: this share of the living group at its
// station or standing in reach of its target, or the fallback after phase 3 starts. 80% came 16-20 s in
// on two pulls and 40 s in on a third, where ranged were still out on leftover tentacles.
constexpr float ULDUAR_YOGG_SARON_LUST_POSITION_SHARE = 0.8f;
constexpr uint32 ULDUAR_YOGG_SARON_LUST_FALLBACK_MS = 30000;

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

// Icecrown's nine tentacles stand in three tight groups of three, 33-58 yd apart: west, south, east.
// A tentacle belongs to the nearest centroid. creature_summon_groups summonerId 33890 group 2.
constexpr uint8 ULDUAR_YOGG_SARON_ICECROWN_CLUSTER_COUNT = 3;
extern const Position ULDUAR_YOGG_SARON_ICECROWN_CLUSTERS[ULDUAR_YOGG_SARON_ICECROWN_CLUSTER_COUNT];

// Where a bot that's done its share of the room waits for the door, on the navmesh path from the landing
// to the Brain. As close to the door as the room test allows: both doors sit inside the brain room's
// 50 yd circle (Icecrown 49.0, Stormwind 45.7), so a bot standing at one reads as already in there.
// 54.8 / 57.0 yd from their own room's middle, 53.7 / 53.0 from the brain room's, settled Z by navprobe.
extern const Position ULDUAR_YOGG_SARON_ICECROWN_STAGE_SPOT;
extern const Position ULDUAR_YOGG_SARON_STORMWIND_STAGE_SPOT;

// Where melee hit the Brain from: 12.3 yd west of it and behind it, since it faces east for the whole
// fight. 30.1 yd 3D against a 32.8 yd melee range (CombatReach 30), on the floor by navprobe and pathed
// from all three illusion entrances. Anywhere in front, set behind picks its spot at the 3D distance
// as a floor radius, walks the bot 15 yd out of range, and reach melee walks it back.
extern const Position ULDUAR_YOGG_SARON_BRAIN_MELEE_SPOT;

// Where ranged and healers hit the Brain from, one per illusion room: a few yards in from the exit
// portal on that room's side. Door to spot to portal is 53, 62 and 71 yd against 93-106 through the
// middle, and the exit lead drops to its floor from 10.6 s. All within 36.3 yd of the melee spot for
// heals, and on the floor by navprobe, PATHFIND_NORMAL from the door, to the portal and from the melee
// spot. West for Stormwind, south for Icecrown, north for the Chamber.
extern const Position ULDUAR_YOGG_SARON_BRAIN_WEST_SPOT;
extern const Position ULDUAR_YOGG_SARON_BRAIN_SOUTH_SPOT;
extern const Position ULDUAR_YOGG_SARON_BRAIN_NORTH_SPOT;
extern const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_ENTRANCE;
extern const Position ULDUAR_YOGG_SARON_P1_RANGED_SPOT;

// Where melee and the tank sit out the handover. On the station ring, so the only orbit that
// reaches it is the one the back line already wakes; the 15 yd ring the walk used before is inside
// the innermost orbit, which nothing but a player's feet can wake. 25 yd round from the station
// spot, the nova clear radius plus the back line's own blob, so a Guardian dying in the melee group
// cannot reach the raider nearest it. On the floor by navprobe, same Z as the station.
extern const Position ULDUAR_YOGG_SARON_HANDOVER_MELEE_SPOT;
extern const Position ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT;
extern const Position ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT;

// Where the ranged stand instead while a Crusher is too close to their spot, tried in this order: the
// ranged spot turned -15, +15, -30 and +30 degrees round Yogg's middle, same 39 yd out. On the floor by
// navprobe with straight paths from the ranged spot, 21.6-25.2 yd from the melee spot.
extern const std::vector<Position> ULDUAR_YOGG_SARON_PHASE_3_RANGED_STANDBY_SPOTS;

// Where a Shadow Beacon marked Guardian is walked to without Thorim, one spot per marked Guardian. The
// heal goes on every Guardian within 20 yd of the marked one, once, as it's cast, and holds it at full
// for 20 s. The tank and the Guardians on him drift up to ~13 yd off the melee spot, and from 27 yd out
// that left two pile Guardians 18-20 yd from the casters, both reset. So north and south are ~39 yd out:
// PATHFIND_NORMAL from the melee spot by navprobe, 39 yd from Yogg's middle, 48-50 from the ranged
// spot, 28-30 from the nearest Sanity Well, still in heal range of the melee spot. North-east is 33.7 yd
// from the melee spot, 48 from Yogg's middle, 24 from the ranged spot and 34 from north, PATHFIND_NORMAL.
extern const Position ULDUAR_YOGG_SARON_BEACON_DRAG_NORTH;
extern const Position ULDUAR_YOGG_SARON_BEACON_DRAG_SOUTH;
extern const Position ULDUAR_YOGG_SARON_BEACON_DRAG_NORTHEAST;

// Where a hunter stands to pull a marked Guardian with Distracting Shot, on the line to each drag spot
// but inside the shot's 35 yd. A Guardian that walks to the hunter stops ~29 yd off the pile. On the floor
// by navprobe, PATHFIND_NORMAL 33.2 / 33.1 yd from the melee spot and 42 / 44 from the ranged spot.
extern const Position ULDUAR_YOGG_SARON_BEACON_HUNTER_NORTH;
extern const Position ULDUAR_YOGG_SARON_BEACON_HUNTER_SOUTH;

// Where a non-hunter dragger waits for the next beacon, 20 yd along the PATHFIND_NORMAL line from the melee
// spot to each drag spot, on the floor by navprobe. A 30 yd taunt still reaches a mark anywhere in the
// pile, and the walk after it is ~19 yd instead of ~39.
extern const Position ULDUAR_YOGG_SARON_BEACON_STAGE_NORTH;
extern const Position ULDUAR_YOGG_SARON_BEACON_STAGE_SOUTH;
extern const Position ULDUAR_YOGG_SARON_BEACON_STAGE_NORTHEAST;

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

// FindNearestCreature(entry, 200, alive) for Sara, Yogg, the Brain and the Sanity Wells, which nearly
// every Yogg trigger asks again. Answered once per trigger pass (see EncounterTriggerPassId), live anywhere
// else and for any other entry.
Creature* YoggSaronNearestCreature(PlayerbotAI* botAI, uint32 entry);

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
    // Off the tentacles until healed, see YoggSaronReprisalHold. Has no target, so without its own
    // state it reads as walkingin and the walk-in node marches it back onto one.
    YOGG_SARON_ROOM_STATE_REPRISAL = 6,
    // Done with its share of an Icecrown or Stormwind room, waiting at the door for the rest.
    YOGG_SARON_ROOM_STATE_STAGED = 7,
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
// platform. The lead is measured against the walk the bot actually faces, and only once it is in the
// brain room.
bool YoggSaronShouldLeaveBrainLevel(PlayerbotAI* botAI);

// Whether the bot has already decided to leave for this Induce Madness. Latched until the cast ends, so
// a bot closing on the portal can't talk itself back to the Brain. Reads the latch only, no sweep.
bool YoggSaronLeavingBrainLevel(Player* bot);

// Sanity stacks (63050). 0 with no aura, which is Insane or out of the fight.
uint32 YoggSaronSanityOf(Player* player);

// The spot a bot hits the Brain from once its room is done: behind it for melee, the portal spot of the
// room it came up from for everyone else. The brain room middle when that spot can't see the Brain or
// the room isn't known yet. Probed as `yogg.brainspot`.
Position YoggSaronBrainSpot(Player* bot);

// A bot's phase 3 station. Healers take the melee spot with the tank: it is 18.5 yd straight behind
// Yogg, outside his knockback and within heal range of the ranged spot, and a Guardian chasing a healer
// walks into the tank there. Ranged move to a standby spot while a live Crusher is within
// ULDUAR_YOGG_SARON_PHASE_3_RANGED_CRUSHER_CLEARANCE of theirs, the same one for every bot.
Position const& YoggSaronPhaseThreeSpot(Player* bot);

// Laughing Skulls within gaze range that are in the bot's front 180 degrees, which is the exact test
// the spell uses to pick its targets. The distance is re-checked after the grid sweep, whose own range
// test is bounding-radius inclusive: 164 of 235 probe flips in one pull had no skull inside 30 yd.
std::vector<Unit*> GetYoggSaronSkullsInArc(PlayerbotAI* botAI);

// The same skulls without the arc test, for deciding where to stand rather than what is hitting the
// bot now. A candidate spot is judged on the heading it would force, so the bot's current facing has
// no bearing on which skulls matter.
std::vector<Position> GetYoggSaronSkullsInRange(PlayerbotAI* botAI);

// How far one heading is off another, 0 to pi either way round.
float YoggSaronHeadingGap(float from, float to);

// Skulls in the front half of a heading from (x, y): what Lunatic Gaze's HasInArc(pi) would hit.
uint32 YoggSaronSkullsAhead(std::vector<Position> const& skulls, float x, float y, float heading);

// The heading within ULDUAR_YOGG_SARON_SKULL_TARGET_TURN of `targetBearing` with the fewest skulls ahead,
// nearest the target on a tie, and how many that is.
uint32 YoggSaronAngledSkullHeading(std::vector<Position> const& skulls, float x, float y, float targetBearing,
                                   float& heading);

// Whether a bot at (x, y) attacking (targetX, targetY) has a heading it may keep that leaves every one
// of `skulls` outside its front 180 degrees, the exact filter spell_yogg_saron_lunatic_gaze uses.
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

// Phase 3: the leftover Corruptor or Constrictor every bot's pet goes for, or nullptr. A Constrictor
// holding somebody first, then lowest health, then GUID, so all pets pile on one. Skips any with a
// swinging Crusher within Crush range, the pet guard would only pull the pet back out.
Unit* YoggSaronPhase3PetLeftover(PlayerbotAI* botAI);

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
//
// Bots benched for low Sanity are skipped, and their portals go to whoever's next in that order. Too few
// left means empty portals, never a low bot sent down.
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

// A brain team bot already walking to, standing on, or late for its portal. Anything that would pull it
// elsewhere stands down: a room without its healer fell to 14% while Brain Link held that healer upstairs.
bool YoggSaronPortalWalkPending(PlayerbotAI* botAI);

// A brain team bot already on its portal spot waiting for the wave. Nothing else keeps it there: the
// portal node only walks on spreading or late, so reach would take it back to its target and the two
// would trade it for the whole lead. Slot and distance first, so the plan is only read on the spot.
bool YoggSaronHoldingPortalSpot(PlayerbotAI* botAI);

// Damage dealer in an illusion room off the tentacles until healed. Latched in the tick, see
// ULDUAR_YOGG_SARON_REPRISAL_STOP_PCT; this only reads it.
bool YoggSaronReprisalHold(PlayerbotAI* botAI);

// Nearest living group healer in the bot's own illusion room, or null.
Player* YoggSaronRoomHealer(PlayerbotAI* botAI);

// What the interrupt node kicks: phase 1 Guardians casting Dark Volley, and in phase 3 Immortal
// Guardians channelling Drain Life, beacon marks first. A kicker held back for the marks gets only those.
// Shared between trigger and action so the two cannot disagree about what is being kicked.
std::vector<Unit*> GetYoggSaronInterruptCasters(PlayerbotAI* botAI);

// A Guardian a beacon marks or a bot is dragging. A read of the drag claims, no sweep.
bool YoggSaronMarkCaster(Unit* caster);

// This bot is one of the kickers held back for the marks while a beacon lasts.
bool YoggSaronMarkKicker(Player* bot);

// Interrupts this bot can aim, in the order the action tries them, empty for a class with none.
// Avenger's Shield is left out - it picks its own target and cannot be pointed at a named Guardian.
std::vector<char const*> YoggSaronInterruptSpells(Player* bot);
bool YoggSaronCanInterrupt(Player* bot);

// The same list for one caster. An Immortal Guardian's immunity set takes silences but not interrupts,
// so Strangulate, Silencing Shot and Arcane Torrent would only burn their cooldowns on one.
std::vector<char const*> YoggSaronInterruptSpellsFor(Player* bot, Unit* caster);

// Whether this bot can kick the caster with this spell right now. CanCastSpell alone passes a pet's spell
// on cooldown and counts out of range as castable, which spent the node's tick on a cast that failed.
bool YoggSaronInterruptReady(PlayerbotAI* botAI, char const* spell, Unit* caster);

// Whether another bot's kick already holds this cast, and taking it for this bot. One kick per cast:
// Drain Life comes round every ~14.5 s per Guardian and the raid has about one kick a second.
bool YoggSaronInterruptClaimedByOther(Player* bot, Unit* caster);
void ClaimYoggSaronInterrupt(Player* bot, Unit* caster);

// Crushers mid Diminish Power channel, not in its 1.5 s cast. 64148 on the tentacle breaks the channel
// on a taken melee swing or melee-class spell, and only once it is channelling, so a Judgement during
// the cast burns a 10 s cooldown for nothing.
std::vector<Unit*> GetYoggSaronChannellingCrushers(PlayerbotAI* botAI);

// Judgements in the order the node tries them, empty for anything but a paladin. All three share one
// cooldown, and every Judgement damage spell is melee class, so any of them breaks the channel. Range
// is 10 yd plus both combat reaches, ~19.5 yd from the tentacle's centre, well outside its swing.
std::vector<char const*> YoggSaronJudgementSpells(Player* bot);

// Guardians whose death nova this bot should leave. Ranged and healers count only one that is both
// about to die and chasing them: at spell range nothing else can reach them. A Sara's Fervor holder
// counts every living Guardian in reach, because Fervor outlasts the gap between novas. Melee stand in
// a nova by design and count one about to detonate only when their pool is small enough that it kills
// them whatever reduction they own. Shared so the trigger and the action cannot disagree about who is
// running.
std::vector<Unit*> GetYoggSaronNovaThreats(PlayerbotAI* botAI, float radius);

// What the next Shadow Nova is worth against `target`: the unresisted figure per low Guardian within
// the blast, at most two of them, doubled while Sara's Fervor is on it. 0 when nothing in reach is close
// to detonating. Unlike GetYoggSaronNovaThreats this counts melee as well - that one answers who should
// run, this one answers who is about to be hit whether they can move or not.
uint32 YoggSaronNovaDamageOn(Unit* target);

// Whether that kills `target` outright. False for a bot the nova only bruises, so the shortest cooldown
// is not spent on the first of eleven novas.
bool YoggSaronNovaWouldHurt(Unit* target);

// How long a button has to last to still be up when the nova lands, or 0 when the bot does not need one.
// `now` asks at the 10% gate, which is where a bot spends its own button and a healer its external; the
// 20% gate is Divine Sacrifice's alone. Shared so the trigger and the action cannot pick different
// windows.
uint32 YoggSaronNovaNeedMs(Unit* target, bool now);

// Whether a living Guardian is close enough for its death nova to reach this bot. What the nova nodes
// gate on instead of the phase: the Guardians phase 1 leaves alive keep their death explosion and
// nothing in the core despawns them, so they have to be answered in phase 2 as well.
bool YoggSaronNovaNearby(PlayerbotAI* botAI);

// Whether the nova cooldowns are still being saved. All of phase 1, because every death measured in it
// was a nova and nothing else there is worth a three-minute cooldown, and after it only while a leftover
// Guardian is in reach - phase 2 has Crush and the Death Rays to spend them on.
bool YoggSaronNovaHold(PlayerbotAI* botAI);

// How many melee and tanks on the stack the next nova would take that far down, which is what decides
// whether the one raid-wide cooldown is worth spending.
uint32 YoggSaronNovaHurtCount(PlayerbotAI* botAI);

// The one paladin that carries Shadow Resistance Aura here, lowest GUID of the bot paladins that know
// Aura Mastery. Prayer of Shadow Protection already gives the same 130 on the same exclusive aura 143
// and the core takes the highest of those rather than the sum, so the aura slot buys the raid nothing
// unless the paladin holding it can double it to 260.
bool IsYoggSaronShadowAuraPaladin(Player* bot);

// Whether this bot should be left alone to hold that aura, which is what keeps its own buff strategy
// from trading the exclusive slot back on the next GCD.
bool YoggSaronShadowAuraHeld(PlayerbotAI* botAI);

// Whether some other paladin should keep off the aura and off Aura Mastery. Two copies of the same aura
// coexist per caster and only the strongest keeps its effects, so a second paladin running it costs the
// elected one half a second of effect hand-off right when the cover is pressed, and a press from the
// paladin whose copy is not the strongest does nothing at all. Only while there is an elected paladin,
// so a raid with no Aura Mastery anywhere still gets the aura from the generic nodes.
bool YoggSaronShadowAuraOtherHeld(PlayerbotAI* botAI);

// Which raid-wide cover this paladin can give the next nova, or null for none. Shared so the trigger
// and the action cannot pick different buttons.
char const* YoggSaronNovaRaidCover(PlayerbotAI* botAI);

// Whether a battle rez should wait for the handover. It strips every raid buff, so the bot comes back
// on a bar 13-17% smaller with no Prayer of Shadow Protection, and one nova then kills it from full
// health: that happened three times in one pull and the cooldown bought 14 s, 8 s and no life at all.
bool YoggSaronBattleRezHeld(PlayerbotAI* botAI);

// The raider a healer external belongs on: whoever the nova leaves furthest past dead, which puts a
// Sara's Fervor holder first and the smallest health pool next. Never the bot itself, and nullptr when
// nobody needs one.
Player* YoggSaronNovaVictim(PlayerbotAI* botAI);

// One external per victim, and one raid-wide cooldown per nova. True for the bot that takes the claim
// and false for everyone else until it lapses; the window claim stays true for its owner so it can
// retry across ticks.
bool ClaimYoggSaronNovaExternal(PlayerbotAI* botAI);
bool ClaimYoggSaronNovaWindow(Player* bot);

// The Guardian no dps should touch: the higher-health half of a low pair standing close enough for one
// nova to finish the other, which is how eight melee died in a single tick. The guid breaks an
// exact tie, so one of a pair is always free to die and the two can never wait on each other.
void AppendYoggSaronTargetExclusions(PlayerbotAI* botAI, GuidSet& exclusions);

// Which of two phase 1 Guardians the raid should be on. One inside the leash beats one outside it
// whatever their health, because a kill out there does nothing for Sara and its nova reaches the
// station instead of the melee pile; lowest health decides within each group, which is the focus fire
// that keeps a Guardian from spending 3.5 s at detonation range. Shared so the kill order and the
// tank's taunt cannot pick different Guardians - they run on different bots and never exchange state,
// so agreeing means computing the same answer from the same world.
bool YoggSaronPhase1GuardianPreferred(Unit* candidate, Unit* incumbent);

// The two radii that comment separates, as predicates, so nothing has to remember which 15 is which.
// A Guardian that counts is one whose death nova still reaches Sara; one on the stack also dies close
// enough that the blast stops at the melee pile instead of the 21.5 yd station.
bool YoggSaronGuardianCountsForSara(Unit* guardian);
bool YoggSaronGuardianOnTheStack(Unit* guardian);

// Inside ULDUAR_YOGG_SARON_P1_ROOM_RADIUS. Plain distance, no sweep, so it goes ahead of the phase read.
bool YoggSaronInPhase1Room(Player* bot);

// Record a Guardian's death, from the spell hook that sees its 65719 go out.
void YoggSaronNoteGuardianDeath(Unit* guardian);

// Whether any Guardian in the bot's instance died in the last `ms`.
bool YoggSaronGuardianDiedWithin(Player* bot, uint32 ms);

// Whether the raid may finish this Guardian where it stands: on the stack, or still above the park
// floor. Never below the nova gap floor while the last nova is under 6 s old, or while a lower Guardian
// stands inside nova chain range, which has to die first. The park floor is off without a living bot
// tank, since nobody would ever fetch a parked one. The other two never are.
bool YoggSaronPhase1GuardianKillable(PlayerbotAI* botAI, Unit* guardian);

// The one Guardian every non-tank is on in phase 1, shared per instance, or null while every Guardian
// is parked or waiting its turn. Per-bot holds let ranged and melee lock different Guardians, and the
// two came down in lockstep and died 17 ms apart. Held until it dies, stops being killable, or drifts
// past 15 yd while a killable one is on the stack.
Unit* YoggSaronPhase1Focus(PlayerbotAI* botAI);

// True while a Guardian other than the focus sits below the gap floor within 10 yd of the middle or of
// the focus, where AoE kills it on nobody's schedule. 4 of 13 deaths on one pull were that, three of
// them within 2 s of another nova. Reads the focus without re-picking it.
bool YoggSaronPhase1AoeHold(PlayerbotAI* botAI);

// The Guardians still alive when Shadow Barrier lands keep the nova gap and the chain rule: their nova
// still hits for ~25k, and finished 1-2 s apart at the handover spot they killed 3, 7 and 4 melee in
// three pulls. No park floor, there's no stack left to keep them off.
bool YoggSaronLeftoverGuardianKillable(PlayerbotAI* botAI, Unit* guardian);

// After phase 1, true while a Guardian other than the bot's target sits below the gap floor within
// 10 yd of the bot or its target, where AoE would kill it out of turn.
bool YoggSaronLeftoverAoeHold(PlayerbotAI* botAI);

// The Guardian the tank should pull in, or null while every one of them is already where it should be
// or already walking at a tank. The raid's focus first: the tank gets one taunt per Guardian death at
// best - 12 casts against 13 kills on the pull that measured it - and spending that on whoever last
// hit somebody picked the 7th-lowest Guardian of 9. Only fires for a Guardian outside the leash,
// because one already inside needs no fetching.
Unit* YoggSaronPhase1TauntTarget(PlayerbotAI* botAI);

// The Guardian a phase 1 tank should be swinging at, lowest health first, or null while none is
// already in reach. Nothing else gives a tank a target here - the dps resolver stands down for one
// and the generic picker keeps whatever is selected - so one pull held a stale selection 48 yd away
// and dealt nothing for 83 s with a Guardian inside its reach throughout.
Unit* YoggSaronPhase1StackTarget(PlayerbotAI* botAI);

// Whether the raid still has a living bot tank to fetch Guardians with. The reach guard below stands
// melee down on the promise that somebody else brings the target in, so it has to know that somebody
// is there - without this a dead tank strands the melee half of the raid out of combat.
bool YoggSaronBotTankAlive(PlayerbotAI* botAI);

// Where a cloud will be `leadMs` ahead, taken from its own facing and run speed.
Position YoggSaronCloudLead(Creature* cloud, uint32 leadMs = ULDUAR_YOGG_SARON_CLOUD_LEAD_MS);

// Whether a straight walk from the bot to (x, y) stays outside every cloud's summon radius.
bool YoggSaronRouteClearOfClouds(Player* bot, std::vector<Position> const& clouds, float x, float y);

// Whether a walk from the bot to `to` would cross a cloud now, halfway or on arrival, so it should wait
// for the cloud to pass (about 6 s). False while the bot already stands in one: leaving beats staying.
// Clouds within `exemptRadius` of `exempt` are ignored.
bool YoggSaronWalkCrossesCloud(Player* bot, Position const& to, Position const* exempt = nullptr,
                               float exemptRadius = 0.0f);

// The window between Sara dying and Yogg emerging. Yogg being here without SPELL_SHADOW_BARRIER is
// the whole of it, because ACTION_YOGG_SARON_APPEAR casts the barrier and the knockback in one call.
// SetVisible(false) does not hide him from a grid search, so the window is readable from its first
// tick.
struct YoggSaronHandover
{
    bool active = false;
    // Whether the handover owns where this bot stands, decided here rather than by each caller, so the
    // trigger, the action and the two multipliers cannot answer it differently. Melee and the tank
    // only, the same audience the positioning trigger opens with.
    //
    // It stays true for a hold after the ring has lit, because the window itself ends on that tick and
    // a bot released there walks straight back under the knock back.
    bool clearing = false;
};

// A read of the window the encounter's tick latches.
YoggSaronHandover YoggSaronHandoverState(PlayerbotAI* botAI);

// The encounter's tick: latches the phase 1 to 2 handover window, then writes the throttled trace notes.
void YoggSaronTick(PlayerbotAI* botAI);

// The body's knockback ring, which has no world object behind it and so cannot be swept for.
bool YoggSaronInBodyKnockback(Player* player);
bool YoggSaronRouteClearOfBody(Player* bot, float x, float y);

// Whether a straight walk from the bot to (x, y) stays out of every ray's clear radius, and inside one
// never gets closer than the bot already is.
bool YoggSaronRouteClearOfDeathRays(Player* bot, std::vector<Position> const& rays, float x, float y);

// A live Death Ray within its clear radius of a bot on the platform. A short sweep, no phase read.
bool YoggSaronDeathRayNear(PlayerbotAI* botAI);

// A point to cross the room through when the straight line would go over the body. One is enough:
// each leg halves the turn the next one has to make, so the bot walks the arc rather than the chord.
bool YoggSaronBodyDetour(Player* bot, Position const& destination, Position& waypoint);

// Where a walk to a fixed spot should aim this tick: the spot, or the detour waypoint when the straight
// line to it crosses the body. Latch a walk on the spot, never on this: the waypoint moves with the bot.
Position YoggSaronBodyRoute(Player* bot, Position const& spot);

// An Immortal Guardian still worth a taunt or a hit: alive, not Weakened and not being dragged off a
// Shadow Beacon. A Weakened one has no Empowered stacks left and only Titanic Storm can finish it.
bool IsYoggSaronHoldableGuardian(Unit* unit);

// Phase 3 without Thorim. The marked Guardian this bot drags clear of Yogg and the pile, or nullptr. With
// `claim` it also takes one: bot tanks first while another tank holds the pile, then ret paladins and
// dps Death Knights, then hunters, by GUID, never a bot some unmarked Guardian is hitting, since that one
// would follow it out. The claim lasts until the heal has landed, or is handed on when its taunt didn't
// take within ULDUAR_YOGG_SARON_BEACON_TAUNT_CHECK_MS.
Unit* YoggSaronBeaconDragGuardian(PlayerbotAI* botAI, bool claim);

// Stamps the taunt this bot just cast on the Guardian it drags.
void YoggSaronBeaconTaunted(Player* bot);

// Whether the bot holds a drag claim. A read of the claim map only, no sweep.
bool YoggSaronBeaconDragging(Player* bot);

// Where this bot waits for the next beacon, latched ULDUAR_YOGG_SARON_BEACON_STAGE_LEAD_MS before it's due,
// or nullptr. A hunter waits at its hunter spot. Only until the first mark shows up.
Position const* YoggSaronBeaconStageSpot(Player* bot);

// The drag spot of this bot's own drag, picked when it claimed, and the hunter spot on that side.
Position const& YoggSaronBeaconDragSpot(Player* bot);
Position const& YoggSaronBeaconHunterSpot(Player* bot);

// The healed Guardian this bot dragged, while it is still on the bot within ULDUAR_YOGG_SARON_BEACON_RETURN_MS
// of the release, or nullptr. The tank is ~39 yd away and a taunt reaches 30, so the dragger walks it back.
Unit* YoggSaronBeaconReturning(Player* bot);

// A Guardian the beacon heal is on, or still marked, that is hitting `victim` from the danger line up,
// or nullptr.
Unit* YoggSaronHealedGuardianOn(PlayerbotAI* botAI, Player* victim);

// Who the next external against a healed Guardian goes to: draggers first, then the lowest, humans too.
// A tank holding the pile only under ULDUAR_YOGG_SARON_BEACON_TANK_EXTERNAL_PCT. Skips a bot whose own
// button is ready or running and anyone another external already holds.
Player* YoggSaronBeaconExternalVictim(PlayerbotAI* botAI);
bool ClaimYoggSaronBeaconExternal(PlayerbotAI* botAI, Player* victim);

// Holdable Guardians whose victim is this tank, from its own attacker set.
uint32 YoggSaronGuardiansOn(Player* tank);

// Guardians on this tank carrying a beacon heal another Guardian cast, at any health. A returned mark
// carries only its own, so it takes a heal that reached the pile.
uint32 YoggSaronPileHealedOn(Player* tank);

// The tank this bot is taking the pile from, see ULDUAR_YOGG_SARON_TANK_SWAP_PCT, or nullptr. Starts a swap
// when the conditions hold and none is running, and ends one that no longer holds.
Player* YoggSaronTankSwapTake(PlayerbotAI* botAI);

// This bot is the holder of a running swap, so it keeps its area threat and taunts off the pile.
bool YoggSaronTankShedding(PlayerbotAI* botAI);

// A tank under the swap line while another is over the taker line leaves loose Guardians to that one. A
// soulstoned tank taunted straight back into the pile and died again 14 s later.
bool YoggSaronTankLowHold(Player* bot);

// A phase 3 paladin tank keeps Avenging Wrath while Divine Protection is ready within its 30 s lock.
bool YoggSaronTankWrathHeld(PlayerbotAI* botAI);

bool YoggSaronBeforePhase3(PlayerbotAI* botAI);

// The tank a bot paladin should Lay on Hands, under ULDUAR_YOGG_SARON_TANK_LOH_PCT and in range, lowest
// first, humans too and the paladin itself if it tanks. nullptr when nobody needs it or it can't go out.
Player* YoggSaronTankLayOnHandsTarget(PlayerbotAI* botAI);
bool ClaimYoggSaronTankLayOnHands(PlayerbotAI* botAI, Player* tank);

// A healer in combat during the encounter with no target. Phase 3 gives healers none, and a bot with no
// target always drops to the non-combat engine, which has no mana cooldowns or potions.
bool YoggSaronHealerTargetless(PlayerbotAI* botAI);

// A healed Guardian is on this bot, so an immunity would just hand it to somebody else.
bool YoggSaronBeaconImmunityHeld(PlayerbotAI* botAI);

// A hunter whose drag claim hasn't seen the heal yet. Any damage on its Guardian is threat that keeps it
// there once the taunt ends, and Feign Death or Deterrence would undo the pull.
bool YoggSaronBeaconHunterHold(PlayerbotAI* botAI);

// Deafening Roar is due this phase 3: 25-man, fewer than 4 Keepers. Latched by the tick.
bool YoggSaronRoarExpected(PlayerbotAI* botAI);
bool YoggSaronRoarCasting(PlayerbotAI* botAI);

// The bot paladin whose Aura Mastery covers the next roar: off cooldown by then, own Concentration Aura
// already up, lowest GUID. A dragger only if nobody else can, since from the drag spot its 40 yd reach
// covers almost nobody. Every bot reads the same world, so they agree.
Player* YoggSaronRoarPaladin(PlayerbotAI* botAI);

// This bot is the roar paladin and the roar is under ULDUAR_YOGG_SARON_ROAR_PRESWITCH_MS away or being
// cast, so it keeps Concentration Aura up.
bool YoggSaronRoarAuraHeld(PlayerbotAI* botAI);

// One Aura Mastery per roar. The roar paladin takes it at once, anyone else after the handoff delay, and
// the owner keeps it so a failed press can retry.
bool ClaimYoggSaronRoar(PlayerbotAI* botAI);

// The brain team holds off the Brain, see ULDUAR_YOGG_SARON_BRAIN_HOLD_PCT. Latched per bot in the tick.
// The DoT hold is true under a full hold too.
bool YoggSaronBrainHeld(PlayerbotAI* botAI);
bool YoggSaronBrainDotHeld(PlayerbotAI* botAI);

// Lunatic Gaze is up on Yogg (64163 is a 4 s aura on him, not a channel), and whether a target sits far
// enough round from him that the bot can hit it with Yogg out of its front arc.
bool YoggSaronGazeActive(PlayerbotAI* botAI);
bool YoggSaronOutsideGaze(Player* bot, Unit* yogg, Unit* target);

// Whether a shaman should cast Heroism or Bloodlust now: phase 3, and the raid at its stations or in
// reach of its targets, see ULDUAR_YOGG_SARON_LUST_POSITION_SHARE, or the fallback has run out.
bool YoggSaronPhase3LustReady(PlayerbotAI* botAI);
bool YoggSaronHoldableGuardianWithin(Player* bot, float radius);

// A holdable Guardian among the bot's nearest npcs whose victim is not a tank, so a taunt is owed.
bool YoggSaronGuardianLoose(PlayerbotAI* botAI);

// Whether a non-tank may hit this Guardian: a tank is its victim and leads the bot's threat on it by
// the start share, or the keep share for one the bot is already on. With no tank alive in the group
// nothing could ever pass, so everyone may.
bool YoggSaronGuardianThreatAllows(Player* bot, Unit* guardian, bool holding);

// This tank's threat on the Guardian over the highest non-tank's, pets included. Zero when nobody has
// any, since heal threat is about to land there; float max when only tanks do.
float YoggSaronTankThreatLead(Player* tank, Unit* guardian);

// A Constrictor with somebody squeezed in its seat.
bool YoggSaronConstrictorHolding(Unit* unit);

// Any live Influence Tentacle within `radius` of `centre`, the bot's own position by default, disguise
// included. A tentacle is re-stamped as a Suit
// of Armor, a Deathsworn Zealot or a Consort the instant it spawns and only reverts to entry 33943
// when something damages it, so a sweep for 33943 alone reports a room full of them as empty. One
// grid visit for the whole list.
Unit* YoggSaronLiveIllusionMob(PlayerbotAI* botAI, float radius, Position const* centre = nullptr);

// Every live illusion mob in the bot's own room, nearest to the bot first. Same sweep as
// YoggSaronLiveIllusionMob, which answers "is this room clear" and so returns whichever one the grid
// visit happened to hand back; this one is for walking somewhere, where which one matters.
std::vector<Unit*> GetYoggSaronRoomIllusionMobs(PlayerbotAI* botAI);

// Whether this bot may take this tentacle. At door-open everybody stood 106-130 yd from the Brain, deep
// in the room, so a bot that's done its share waits at the door instead of joining the last tentacle.
// Icecrown: its own group only, and another one only once its own is clear and nobody of that group is
// left in the room. Stormwind: a bot whose first tentacle was one of the two by the door keeps clearing,
// the rest only keep the one they're on or take one nobody else is on, so a 10-man never leaves one
// uncovered. No limit in the Chamber, for healers, or under ULDUAR_YOGG_SARON_SHARE_LIFT_MS.
bool YoggSaronIllusionTentacleAllowed(PlayerbotAI* botAI, Unit* tentacle);

// The room isn't clear but nothing left in it is this bot's to take, so it waits at the door.
bool YoggSaronIllusionStaged(PlayerbotAI* botAI);

// Where a staged bot waits. False outside Icecrown and Stormwind.
bool YoggSaronIllusionStageSpot(Player* bot, Position& spot);

// Where the brain team's healer stands in an illusion room: the middle of its live tentacles, which is
// where the melee on them are. The room's own middle is the centroid of the whole summon group, so the
// tentacles ring it - 36-44 yd out in the Chamber against a 38.5 yd heal range, which put its own room
// out of reach and held one wave to 17 landed heals against 137 and 172 in the other two rooms. Falls
// back to the room middle with nothing alive to centre on. False outside an illusion room.
bool YoggSaronIllusionHealerStation(PlayerbotAI* botAI, Position& station);

// Whether the Brain is safe to hit: every Influence Tentacle in this room dead. Scoped to the room
// rather than swept at 200 yd, which is one yard short of reaching the next room's tentacles, and
// measured from that room's middle so a bot in its doorway does not read the far half as empty.
bool YoggSaronInfluenceTentaclesCleared(PlayerbotAI* botAI);

// Nothing on the platform to kill: no Guardian, Crusher, Constrictor or Corruptor alive. Phase 2 had
// 66 s of this in one pull with most of the raid below 100 Sanity.
bool YoggSaronPlatformIdle(PlayerbotAI* botAI);

// Live Crusher Tentacles to angle away from, each as its position plus the facing its Crush cone
// follows. The one currently hitting the bot is left out: inside its reach that bot is hit wherever it
// stands, and the way out is leaving the reach, which GetYoggSaronCrusherReaches covers.
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

// Every live Crusher for ranged and healers, with no victim exemption and no swing gate: an idle one
// still takes a bot that walks in and hits it, and for its victim, walking out is what stops the
// swings. Melee only get a stunned one whose stun is about to lift, since that's the only Crusher they
// are ever let near.
std::vector<Position> GetYoggSaronCrusherReaches(PlayerbotAI* botAI, float searchRadius);
bool YoggSaronInCrusherReach(PlayerbotAI* botAI, float radius);

// A Crusher under Shattered Illusion can't swing or channel, so nothing it does can hurt a bot standing
// in its reach. True only while that holds for at least ULDUAR_YOGG_SARON_STUN_EXIT_LEAD_MS more: the
// Brain is still casting Induce Madness with more than the lead left, and is above
// ULDUAR_YOGG_SARON_STUN_BRAIN_FLOOR_PCT. While the brain team holds, the floor is the hold guard and the
// lead runs to the finish instead. Creatures take no cast pushback, so the cast bar is exact.
bool YoggSaronCrusherStunHolds(PlayerbotAI* botAI, Unit* crusher);

// Whether a forced walk is still closing on where it was sent. MoveTo's `ok` says a command was
// issued, never that a route exists: the core falls back to a straight-line spline, which once carried
// a bot 91 yd vertically at a constant speed with ok=1. Closing distance is the only evidence of a
// real path. Records are kept per node and destination, so testing several destinations in one tick
// does not wipe what the others learned.
bool YoggSaronWalkMakingProgress(PlayerbotAI* botAI, char const* node, Position const& destination);

// Drops an unfinished MoveTo booking once the bot has stood still past ULDUAR_YOGG_SARON_WALK_STALL_MS.
// MoveTo refuses the point it already issued for MaxWaitForMove (5 s) whether the bot is still walking
// or not, so a walk something else stopped stood out that 5 s: one healer covered 24 of its 56 yd in
// 54 s at 0.45 yd/s. Below the platform the stops were a moving bot's potion use and the skull turn's
// SetFacingTo, both fixed at the source. This stays for whatever stops a walk next, and `yogg.walkstep`
// counts how often it has to.
void YoggSaronReleaseStalledWalk(PlayerbotAI* botAI, char const* node, Position const& destination);

// Window in which a counterable fear can land, for the shared anti-fear component. The only fear is
// Malady of the Mind, cast in P2 but re-cast from the feared player on removal, so it can outlive P2.
// Deafening Roar in P3 is a silence, not a fear.
bool YoggSaronFearWindowActive(PlayerbotAI* botAI);

#endif
