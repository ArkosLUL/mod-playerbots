/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERMIMIRON_H
#define PLAYERBOTS_ULDENCOUNTERMIMIRON_H

#include "ObjectGuid.h"
#include "Position.h"
#include "UldData.h"

#include <string>
#include <vector>

class Creature;
class Player;
class PlayerbotAI;
class Spell;
class Unit;

// Mimiron.
//
// Four phases against three mechs - Leviathan MK II, VX-001, the Aerial Command Unit, then all
// three at once - fought in one round room. Almost everything derived here is a place to stand:
// Proximity Mines, Rocket Strikes and Bomb Bots are non-selectable, so pathing knows nothing about
// them and every destination is filtered through the spot-safety helpers below.
//
// The P3Wx2 Laser Barrage is the exception that shapes the rest. It is a 104-degree cone with
// effectively unlimited range, so distance buys nothing and only bearing matters - which is why the
// raid orbits VX-001 rather than spreading, and why the barrage window is predicted rather than
// reacted to.

enum UlduarMimironIds
{
    NPC_LEVIATHAN_MKII = 33432,
    NPC_LEVIATHAN_MKII_CANNON = 34071,  // rides the MK II and does its Plasma Blast / Napalm casting
    NPC_VX001 = 33651,
    NPC_AERIAL_COMMAND_UNIT = 33670,
    NPC_BOMB_BOT = 33836,
    NPC_ROCKET_STRIKE_N = 34047,
    NPC_ASSAULT_BOT = 34057,
    NPC_PROXIMITY_MINE = 34362,
    NPC_MIMIRON_DB_TARGET = 33576,  // the barrage beams aim at this; it laps the room clockwise
    NPC_JUNK_BOT = 33855,
    NPC_MAGNETIC_CORE = 34068,
    SPELL_P3WX2_LASER_BARRAGE_1 = 63293,  // the only one that damages: a 104 degree cone, 50000 yd
    SPELL_P3WX2_LASER_BARRAGE_2 = 63297,  // SPELL_EFFECT_DUMMY, places a beam visual, harmless
    SPELL_SPINNING_UP = 63414,
    SPELL_SHOCK_BLAST = 63631,
    SPELL_P3WX2_LASER_BARRAGE_3 = 64042,  // SPELL_EFFECT_DUMMY, places a beam visual, harmless
    SPELL_P3WX2_LASER_BARRAGE_AURA_1 = 63274,
    SPELL_P3WX2_LASER_BARRAGE_AURA_2 = 63300,
    SPELL_MIMIRON_PLASMA_BLAST = 62997,
    SPELL_MIMIRON_PLASMA_BLAST_25 = 64529,
    SPELL_MIMIRON_NAPALM_SHELL = 63666,
    SPELL_MIMIRON_MAGNETIC_FIELD = 64668,
    ITEM_MIMIRON_MAGNETIC_CORE = 46029,  // 100% drop from the Assault Bot; grounds the ACU
    SPELL_MIMIRON_MAGNETIC_CORE_AURA = 64436,  // the 20s grounding itself, not the field 64668

    // Mimiron hard mode ("Firefighter", Big Red Button pressed): mechs empowered, two extra hazards.
    // NPC_MIMIRON (the boss; sits in his pod, never a bot attack target) comes from core ulduar.h via UldScripts.h.
    NPC_FLAMES_INITIAL = 34363,    // fire seed dropped on players, spawns a spreading node (non-selectable)
    NPC_FLAMES_SPREAD = 34121,     // persistent spreading ground-fire node (non-selectable)
    NPC_FROST_BOMB = 34149,        // VX-001's Frost Bomb; detonates in a large AoE
    NPC_EMERGENCY_FIRE_BOT = 34147,  // puts the flames out; three spawn every 45s

    // Rapid Burst rides the player it was aimed at, not VX-001. 63382 is a 3000 ms aura with a
    // 500 ms periodic dummy, and each of its six ticks fires the cone from VX-001 along the facing
    // it was pointed in when the aura landed. Reading the carrier is what makes the centreline exact.
    SPELL_MIMIRON_RAPID_BURST = 63382
};

// Mimiron P3Wx2 Laser Barrage. The damage is 63293, a TARGET_UNIT_CONE_ENEMY_104 cone: 104 degrees
// wide with a 50000 yd radius, so distance from VX-001 buys nothing and only bearing matters.
constexpr float ULDUAR_MIMIRON_BARRAGE_HALF_ANGLE = 52.0f * static_cast<float>(M_PI) / 180.0f;
// 15 rather than 12: the cone lands damage every 250 ms and turns 2.7 degrees in that time, so 12 was
// about one bot reaction tick with nothing to spare. This still leaves a 120 degree safe wedge.
constexpr float ULDUAR_MIMIRON_BARRAGE_MARGIN = 15.0f * static_cast<float>(M_PI) / 180.0f;

// Inside this the beams hit whatever the bearing. The cone check skips bearing for anything within
// IsWithinBoundaryRadius of VX-001, max(bounding radius, MIN_MELEE_REACH 2.0) centre to centre, so
// 2.0 plus a yard. A fire dodge can park a melee bot under the model, and the bearing test calls
// that spot clear.
constexpr float ULDUAR_MIMIRON_BARRAGE_BOUNDARY = 3.0f;

// Waypoint velocity on path 13395. NPC 33576 laps a 707 yd polygon that a circle of radius 113.2
// centred 2.7 yd from ULDUAR_MIMIRON_ROOM_CENTER fits, so predicting its position by rotating it
// about the room centre lands within a couple of degrees of bearing over a whole barrage. Clockwise.
constexpr float ULDUAR_MIMIRON_DB_TARGET_SPEED = 20.8988f;

// 63274 runs 10 s once 63414's single tick starts it. Only used while Spinning Up, when the barrage
// aura does not exist yet to be read; from ignition on the live duration is preferred.
constexpr float ULDUAR_MIMIRON_BARRAGE_FIRE_SECONDS = 10.0f;

// Rotate in bounded steps rather than aiming one move at the far side of the ring: creatures are not
// in the navmesh, so a long chord walks straight through VX-001, and crossing the apex crosses every
// bearing the cone covers. A 40 degree chord stays within 6% of the ring radius.
constexpr float ULDUAR_MIMIRON_BARRAGE_STEP = 40.0f * static_cast<float>(M_PI) / 180.0f;

// Added to VX-001's combat reach (8) to get the smallest ring a bot may orbit on. Melee sit inside
// that, and an orbit at their own radius runs through the model.
constexpr float ULDUAR_MIMIRON_BARRAGE_RING_MARGIN = 6.0f;

// Melee, the phase 4 main tank included, orbit on a tight ring instead, so they keep swinging
// through the whole window. Two limits it has to sit inside: Unit::GetMeleeRange, reach 8 plus a
// player's 1.5 plus 4/3, so 10.83 yd, or the chassis chases the tank and drags the cone apex with
// it; and the bot's own "reach melee" test, IsWithinCombatRange at MeleeDistance 0.75, so 10.25 yd,
// or that node fires and the charge guard vetoes it every tick. 9.0 leaves a yard of spline slop
// under both, and stays well past the 2.0 yd boundary radius where the cone check stops testing
// bearing at all. A stationary apex is load-bearing for every other bot: every bearing, radius and
// offset here is cleared against one, and only fails once the apex is dragged under the raid.
constexpr float ULDUAR_MIMIRON_BARRAGE_MELEE_RING_MARGIN = 1.0f;

// How far in the fire shift may pull a melee bot off that ring. Inside 5 the orbit runs through
// VX-001's model, and a 40 degree step on a 5 yd ring is 3.5 yd of travel, which is still a move.
constexpr float ULDUAR_MIMIRON_BARRAGE_MELEE_RING_MIN = 5.0f;

// Radius shifts off the orbit, nearest first, for stepping round fire at a given bearing.
constexpr float ULDUAR_MIMIRON_BARRAGE_RADIUS_SHIFTS[] = {0.0f, 2.0f, -2.0f, 4.0f, -4.0f, 6.0f, -6.0f, 8.0f, -8.0f};

// Spare time a bot inside the ignition cone needs to walk all the way out the trailing side before
// the beams light: two beam ticks for the leg to start and land. Out the leading side means
// outrunning the sweep round the whole band to its far edge, and whatever fire is sitting there.
constexpr float ULDUAR_MIMIRON_BARRAGE_IGNITION_BUFFER = 0.5f;

// Bearing step when looking inside the safe sector for ground that isn't burning. Under 2 yd at the
// ranged ring, against a 5 yd flame radius.
constexpr float ULDUAR_MIMIRON_BARRAGE_REFUGE_PROBE = 5.0f * static_cast<float>(M_PI) / 180.0f;

// A bot turns around VX-001 at (7.0 yd/s / radius) against a 10.6 deg/s sweep. Holding the raid
// inside this radius makes the worst-case 52 degree rotation fit the 4 s Spinning Up warning and
// still leaves 1.6x speed margin; break-even is 38 yd, where a bot can never out-turn the cone.
constexpr float ULDUAR_MIMIRON_SPREAD_RADIUS_MAX = 24.0f;

// Ranged ring, kept inside ULDUAR_MIMIRON_SPREAD_RADIUS_MAX so a barrage rotation from it still
// fits the Spinning Up window. The tolerance is what stops bots pacing over a yard of drift.
constexpr float ULDUAR_MIMIRON_SPREAD_RADIUS = 22.0f;
constexpr float ULDUAR_MIMIRON_SPREAD_TOLERANCE = 5.0f;

// How long a re-aimed Firefighter wedge holds its new centreline before another barrage may move it
// again. One barrage is one re-aim: the sector is worked out from the ignition cone, and a second
// look during the same Spinning Up would walk the raid twice for one cast.
constexpr uint32 ULDUAR_MIMIRON_WEDGE_REAIM_MS = 20000;

// Step size of the phase 3 wedge's slide toward the Aerial Command Unit. The wedge holds a slide
// until a slot leaves casting range or the slide it needs has moved this far, instead of following
// every yard the unit drifts. Under the tolerance, so one step never walks a bot off its own slot.
constexpr float ULDUAR_MIMIRON_SHIFT_SLACK = 4.0f;

// How far inside the bot's own spell range the outermost wedge row is allowed to sit, which is what
// caps the row count. Bots cast out to AiPlayerbot.SpellDistance, 28.5 by default, and a slot past
// that does not self-correct: "reach spell" is ACTION_HIGH and the formation is ACTION_RAID, so the
// formation wins every tick and walks the bot back out.
constexpr float ULDUAR_MIMIRON_SPREAD_RANGE_MARGIN = 4.0f;

// Everything inside this of the room centre is walkable and flat at Z 364.31 (navprobe, 16 headings).
// Past it a boss-anchored formation hangs over the edge once the MK II has been dragged to the wall.
constexpr float ULDUAR_MIMIRON_ROOM_RADIUS = 40.0f;

// Shock Blast 63631 is TARGET_SRC_CASTER with a 15 yd radius on a 4 s cast, so 18 clears it with
// margin. Centre to centre: the flee used to be built out of GetDistance2d, which had already taken
// off the MK II's combat reach of 8 and the bot's own 1.5, and so ran everyone out to 29.5 yd.
constexpr float ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST = 18.0f;

// Phase 3 staging fan, ranged and healers only. Bomb Bots blast 5 yd, so no two bots may share one
// and 6 keeps a detonation to a single victim.
//
// Invariant, and it is load-bearing: this must stay above whatever the phase 1 node writes as the
// disperse distance. Equal to it, every bot that reaches its slot is judged too close by the generic
// unstacker and shoved off it, and the formation walks it straight back - which cost ranged 13 to
// 17 % of their output and added 15 s to phase 1.
constexpr float ULDUAR_MIMIRON_PHASE3_SPACING = 6.0f;
constexpr float ULDUAR_MIMIRON_PHASE3_MIN_RADIUS = 18.0f;

// What the generic unstacker is told to keep between ranged bots while the MK II is up, which is the
// only phase Napalm Shell exists in. It splashes 5 yd, so this clears it; it also has to stay under
// ULDUAR_MIMIRON_PHASE3_SPACING or the formation and the unstacker fight over every bot that reaches
// its slot. Melee never set it - the phase 1 node is ranged-only - so they run on the
// DisperseDistanceValue default of -1, which the unstacker rejects outright.
constexpr float ULDUAR_MIMIRON_DISPERSE_DISTANCE = 5.5f;

// Firefighter phase 1 camp: a wedge of per-bot slots around the tank spot. One camp, because chains
// grow toward whoever is nearest their head and a raid held together makes them converge, but never
// one point: Napalm Shell 65026 blasts exactly 5 yd for about 57k, so bots closer than that die in
// pairs. Rows 6 apart, starting at 21 off the tank spot because the MK II stands about 5 yd off it
// toward the camp: that keeps the inner row mostly clear of Shock Blast's 15 yd and the mines scattered
// inside it. The outer rows sit past Napalm's own floor - 15 yd edge to edge, raw 24.5 with both
// combat reaches - so its target pool is never empty and it never falls back to a random threat pick.
constexpr float ULDUAR_MIMIRON_PHASE1_CAMP_FIRST_ROW = 21.0f;
constexpr float ULDUAR_MIMIRON_PHASE1_CAMP_SPACING = 6.0f;
constexpr uint32 ULDUAR_MIMIRON_PHASE1_CAMP_ROWS = 3;

// Sixteen slots in three rows, never closer than 5.5 yd even full. navprobe --nav 0x09 at 1 degree:
// every bearing from 288 to 74 off the tank spot is on mesh at 21, 27 and 33 yd, so a centreline from
// 326 to 36 keeps the whole wedge on the floor.
constexpr float ULDUAR_MIMIRON_PHASE1_CAMP_HALF_ANGLE = 38.0f * static_cast<float>(M_PI) / 180.0f;

// Sight turn for the camp. The tank spot sits about 3 yd inside the doorway alcove and the MK II drifts
// further in, so the alcove walls can hide it from the wedge's edge slots. A bot that loses sight of its
// target drops it, falls back to the non combat engine and walks to its master. So the whole camp turns
// toward the room in these steps until every slot sees the MK II. Fixed eye height so every bot builds
// the same camp; 1.2 and 2.5 gave the same answers against the alcove walls.
constexpr float ULDUAR_MIMIRON_CAMP_SIGHT_STEP = 4.0f * static_cast<float>(M_PI) / 180.0f;
constexpr float ULDUAR_MIMIRON_CAMP_SIGHT_MAX = 16.0f * static_cast<float>(M_PI) / 180.0f;
constexpr float ULDUAR_MIMIRON_CAMP_SIGHT_EYE = 2.0f;
// Turning back waits this long after the last raise, or the camp swings with every MK II step.
constexpr uint32 ULDUAR_MIMIRON_CAMP_SIGHT_HOLD_MS = 15000;

// How far ahead of the highest non-tank threat the main tank has to be before he may start walking
// the MK II west. He used to leave with about three seconds of it: the boss switched to a melee dps
// within 2 to 3 s, stopped following, and parked 15 yd off the room centre while the tank finished
// the walk alone 48 yd away. Time on the tank across three pulls was 65 %, 26 % and 20 %.
constexpr float ULDUAR_MIMIRON_TANK_THREAT_LEAD = 1.3f;

// Escape hatch for that hold. A threat table that never resolves - a human holding the boss, or a
// dead tank - must not pin the fight at the pull spot for the rest of the phase.
constexpr uint32 ULDUAR_MIMIRON_TANK_HOLD_MAX_MS = 10000;

// Fire nodes this close to a stack anchor count against it. The clump is what the field converges
// on, because chains grow toward whoever is nearest their head, so its own anchor burns first: one
// pull took 538k flame damage in phase 1 with the median victim 3.4 yd from the anchor.
constexpr float ULDUAR_MIMIRON_STACK_FIRE_RADIUS = 10.0f;

// Move once the live anchor carries more than the limit, and only somewhere cleaner by the margin,
// then sit still for the hold. Hysteresis on both counts - chains grow 1.22 yd/s, so a bare "stand
// on the cleanest" walks the raid back and forth across the arc for the whole phase.
constexpr uint32 ULDUAR_MIMIRON_STACK_FIRE_LIMIT = 2;
constexpr uint32 ULDUAR_MIMIRON_STACK_FIRE_MARGIN = 2;
constexpr uint32 ULDUAR_MIMIRON_STACK_HOLD_MS = 15000;

// How long one Plasma Blast counts as the same window for the purpose of claiming it. About 4 s of
// cast then six ticks a second apart, so ~9 s from cast start to the last tick, and the next cast is
// 22 s behind. Anything shorter than the 9 lets a second button in on the same window's tail.
constexpr uint32 ULDUAR_MIMIRON_PLASMA_WINDOW_MS = 12000;

// Half-width of the staging wedge. The north-east and south-east arms leave the room centre at 59
// degrees, so only a crowded outer row reaches a bearing anything walks down, and the west arm is
// excluded outright. Narrower than this and a 25-man ranged group will not fit inside casting range.
constexpr float ULDUAR_MIMIRON_PHASE3_WEDGE_HALF_ANGLE = 60.0f * static_cast<float>(M_PI) / 180.0f;

// Loot range for an Assault Bot corpse, and how close the Magnetic Core has to be used: 64444 places
// its summon by nearest entry, so the bot has to be standing under the Aerial Command Unit.
constexpr float ULDUAR_MIMIRON_CORE_LOOT_RANGE = 5.0f;
constexpr float ULDUAR_MIMIRON_CORE_USE_RANGE = 12.0f;

// How many cores the carrier banks before spending any, then spends back to back for one unbroken
// 40 s on the floor. Not fewer add holes: DO_DISABLE_AERIAL delays the unit's event map 25 s on every
// landing and its UpdateAI returns for the whole 20 s aura, so each landing is 45 s with no Assault
// Bot, chained or not.
constexpr uint32 ULDUAR_MIMIRON_CORE_BANK = 2;

// Escape hatches out of that hold. item_template 46029 has duration 60, so a banked core expires;
// the observed wait for a second was 32 s and 52 s, which is what this margin has to survive. And
// below the health release the phase ends before another core could be spent at all.
constexpr uint32 ULDUAR_MIMIRON_CORE_HOLD_EXPIRY_MS = 10000;
constexpr float ULDUAR_MIMIRON_CORE_HOLD_RELEASE_PCT = 25.0f;

// How far the carrier will go looking for an Assault Bot corpse. They die wherever the raid stopped
// them, and the corpse only lasts 25 s, so the node has to start walking rather than wait for the bot
// to happen to be standing on one.
constexpr float ULDUAR_MIMIRON_CORE_SEARCH_RANGE = 60.0f;

// How far to look for a Magnetic Core that is already down. 34068 casts 64436 on itself 3 s after the
// use and despawns at 25 s, so a live one covers the arming, the 20 s landing and the 2 s climb back.
// Using another inside that is what breaks the fight: a second 64436 replaces the first, the script
// runs its lift-off and its landing in the same tick so the unit climbs with the aura still on, and
// every landing pushes all the add timers back another 25 s with no cap.
constexpr float ULDUAR_MIMIRON_CORE_PENDING_RANGE = 100.0f;

// A core the carrier just used is invisible to that check for its whole arming: 64444 is instant, but
// the summon is not there to be found on the next tick and its 64436 only reaches the unit 3.8 to
// 4.4 s later. Two cores 0.2 s apart both went in that way, and the unit spent half a second on the
// floor instead of 19, so the carrier has to remember its own use for this long.
constexpr uint32 ULDUAR_MIMIRON_CORE_ARM_MS = 6000;

// Phase 4 only ends when all three parts are channelling Self Repair at once, and that cast is 15 s,
// so they have to come down level rather than one at a time. Percent, not raw health: the Aerial
// Command Unit's HealthModifier is 200 against 300, so ordering on raw health ranked it last every
// tick and it never kept pace. Bots stop at 10 % and wait for the other two, because all three sit on
// the same point server-side and melee cleave splashes every one of them.
constexpr float ULDUAR_MIMIRON_PHASE4_HOLD_PCT = 10.0f;

// Health band the phase 4 focus is ranked on. Twenty-five bots burn two parts down within a tenth of a
// percent of each other, so comparing raw percent hands every one of them a new target on each
// crossing - a kill traced 1380 of 1564 target notes as a VX-001/MK II flip, roughly five a second per
// bot, each resetting a swing or a cast. Two percent is about 2.4 s of raid damage, and five bands
// still fit inside ULDUAR_MIMIRON_PHASE4_HOLD_PCT.
constexpr float ULDUAR_MIMIRON_PHASE4_FOCUS_BAND_PCT = 2.0f;

// How far below the part furthest from death the others may be pushed before they wait. The
// Aerial Command Unit is the one that lags: it is mostly the ranged's, and all three sit on one
// point so most of their AoE lands on the other two instead. A fixed floor does not know that -
// the ground pair reached 10 % with the unit still at 20 % and was driven under long before it
// caught up, which cost a whole second phase 4. Two focus bands, so the hold releases when the
// unit catches up rather than on every tick of splash. Melee waiting at the floor hit the unit
// until the pair has drifted twice this far under it.
constexpr float ULDUAR_MIMIRON_PHASE4_CONVERGE_PCT = 4.0f;

// Phase 4 tank spot under Firefighter: candidates on these rings round the room centre, and the
// centre itself. 24 yd plus the melee ring round the chassis stays inside ULDUAR_MIMIRON_ROOM_RADIUS.
constexpr float ULDUAR_MIMIRON_PHASE4_TANK_RINGS[] = {8.0f, 16.0f, 24.0f};
constexpr uint32 ULDUAR_MIMIRON_PHASE4_TANK_BEARINGS = 12;

// Fire nodes this close to a candidate count against it: the melee ring round the chassis is ~9 yd
// out and a node burns 5 yd.
constexpr float ULDUAR_MIMIRON_PHASE4_TANK_CLEAR_RADIUS = 14.0f;

// Drag once the live spot carries more than the limit, only somewhere cleaner by the margin, then
// sit for the hold. Every drag walks the chassis, the melee on it and the barrage apex.
constexpr uint32 ULDUAR_MIMIRON_PHASE4_TANK_FIRE_LIMIT = 1;
constexpr uint32 ULDUAR_MIMIRON_PHASE4_TANK_FIRE_MARGIN = 2;
constexpr uint32 ULDUAR_MIMIRON_PHASE4_TANK_HOLD_MS = 20000;

// How far a grid scan looks for a mech that is not attackable yet. The MK II parks 58 yd off centre
// between phases and a ranged bot can be another 40 out on top of that.
constexpr float ULDUAR_MIMIRON_STAGING_SEARCH_RANGE = 200.0f;

// Proximity Mines fire on anyone inside 1.9 yd and blast for 3 yd (66351). They are non-attackable,
// so the only handling is refusing to walk a bot into one. Ten land inside 15 yd after every Shock
// Blast, so a wide avoid radius leaves no clear ground at all and the raid just paces; these are
// deliberately tight, and eating the odd 3 yd blast is cheaper than losing a dodge to it.
constexpr float ULDUAR_MIMIRON_MINE_TRIGGER_RADIUS = 3.0f;
constexpr float ULDUAR_MIMIRON_MINE_CLEARANCE = 3.5f;
constexpr float ULDUAR_MIMIRON_MINE_MAX_STEP = 5.0f;

// Rocket Strike markers burn a 5 s fuse and blast 3 yd, and the rocket picks its target from players
// beyond 15 yd - that is the ranged ring. Destinations near a live marker have to be refused for as
// long as it lives, or a bot that dodged walks straight back onto it.
constexpr float ULDUAR_MIMIRON_ROCKET_CLEARANCE = 8.0f;

// Where the dodge actually fires. 63041 is EffectRadiusIndex 15, a flat 3.0 yd, and a creature
// caster adds no combat reach in WorldObjectSpellAreaTargetCheck, so 5 covers the blast with slop
// while the 8 above stays the stand-off radius. Run/stand hysteresis, like the bomb and the siren:
// one marker used to move five to eleven bots off the ring for a blast that hit one of them.
constexpr float ULDUAR_MIMIRON_ROCKET_RUN_RADIUS = 5.0f;

// Napalm Shell splashes 5 yd around its target. Bomb Bots blast 5 yd on melee contact (63801) and
// match player run speed, so the extra yard here only buys time for ranged to kill them.
constexpr float ULDUAR_MIMIRON_NAPALM_RADIUS = 6.0f;
constexpr float ULDUAR_MIMIRON_BOMB_BOT_RADIUS = 8.0f;

// How much ground a Bomb Bot must still have to cover before a snare is worth a global. It runs
// 8.0 yd/s and dies in about three casts, so snaring one already inside this costs more than it buys.
constexpr float ULDUAR_MIMIRON_BOMB_BOT_SNARE_MIN_APPROACH = 15.0f;

// How often the raid-wide observability pass folds its answers, and the lifetime it stamps on a
// barrage hazard row. 250 matches the default Obs.SnapshotIntervalMs, so every snapshot has a cone
// row beside it, and it is also the barrage's own damage tick.
constexpr uint32 ULDUAR_MIMIRON_OBS_SCAN_INTERVAL_MS = 250;

// Proximity Mines are non-selectable, so they never reach "possible targets" and pathing knows
// nothing about them. Movement actions check their intended destination through this first.
bool IsMimironSpotMineSafe(Player* bot, Position const& dest,
                           float clearance = ULDUAR_MIMIRON_MINE_CLEARANCE);

// The Firefighter hazards a bot can see, gathered in one pass, empty whenever hard mode is off. The
// flee fan tests up to eleven bearings and "nearest npcs" recalculates on demand, so screening a
// bearing at a time would walk a 50 to 60 node fire field eleven times for one dodge.
struct MimironFirefighterHazards
{
    std::vector<Position> flames;
    std::vector<Position> bombs;
    std::vector<Position> fireBots;  // orientation is where the Water Spray line points
    // Origin and facing of every line a spray can take next: each bot's live line, plus the one toward
    // its nearest spread flame, which is where the script aims the next spray.
    std::vector<Position> sprayLanes;
};

MimironFirefighterHazards GetMimironFirefighterHazards(PlayerbotAI* botAI);

// Whether `dest` clears every gathered hazard of that kind. Two calls rather than one because the
// flee fan counts the two refusals apart, and which filter emptied a fan is the thing the trace has
// to be able to name. The bomb test is a destination test: it screens at
// ULDUAR_MIMIRON_FROST_BOMB_STAND_RADIUS, past where the flee trigger lets go.
bool IsMimironSpotFireSafe(MimironFirefighterHazards const& hazards, Position const& dest);
bool IsMimironSpotBombSafe(MimironFirefighterHazards const& hazards, Position const& dest);

// Out of every Emergency Fire Bot's Water Spray lane, and for a caster or healer in 25-man also more
// than `sirenRadius` from it. Takes the bot because only the second half depends on who is asking.
// The bot's own spot is judged at ULDUAR_MIMIRON_FIREBOT_SIREN_CLEARANCE and
// ULDUAR_MIMIRON_FIREBOT_SPRAY_HALF_WIDTH, a destination at ULDUAR_MIMIRON_FIREBOT_SIREN_STAND and
// ULDUAR_MIMIRON_FIREBOT_SPRAY_STAND_HALF_WIDTH.
bool IsMimironSpotFireBotSafe(Player* bot, MimironFirefighterHazards const& hazards, Position const& dest,
                              float sirenRadius, float sprayHalfWidth);
bool IsMimironSpotInFireBotSpray(MimironFirefighterHazards const& hazards, Position const& dest,
                                 float halfWidth);

// The fire bots the raid leaves alone for now, so they keep putting the fire out: every living one,
// in phase 3, until the Aerial Command Unit is low enough that the cleanup has to start. Empty
// otherwise. Raid-wide, so every bot spares the same ones.
std::vector<ObjectGuid> GetMimironKeptFireBots(PlayerbotAI* botAI, Player* bot);
bool IsMimironFireBotProtected(PlayerbotAI* botAI, Player* bot, Unit* fireBot);

// Whether `dest` is outside a Shock Blast the MK II is casting right now. The escape runs a bot out to
// ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST and then hands the tick back, so anything else that moves a
// bot during the 4 s cast has to refuse the circle too, or the formation walks it back in and a fire
// dodge can carry one in from outside.
bool IsMimironSpotShockSafe(PlayerbotAI* botAI, Position const& dest);

// Whether `dest` is ULDUAR_MIMIRON_ROCKET_CLEARANCE clear of every live Rocket Strike marker. For
// moves that screen the fire themselves and still have to miss the 5000000 blast.
bool IsMimironSpotRocketSafe(PlayerbotAI* botAI, Position const& dest);

// Same idea for anywhere a bot is asked to stand rather than flee to: mines, any Rocket Strike marker
// still burning its fuse and a Shock Blast being cast, and under hard mode the ground fire, the
// Frost Bomb and the fire bots. Positioning that ignores markers walks a bot that just dodged one
// straight back onto it, and one that ignores the bomb walks the raid back into the blast for the
// whole ten second fuse.
bool IsMimironSpotSafe(Player* bot, Position const& dest);

// Whether the straight walk from the bot to `dest` stays out of the fire. A slot can be clean while
// the ground between it and the bot burns, and then the fire dodge fires halfway there and throws the
// bot out the far side, up to 23 yd, over and over. Nodes the bot already stands in are left out,
// since walking out of those is the point.
bool IsMimironWalkFireSafe(Player* bot, MimironFirefighterHazards const& hazards, Position const& dest);

// One leg the formation can send a bot on toward its slot. `how` is "direct", "substitute" or
// "detour", and `turn` is a detour's signed turn off the direct bearing, radians.
struct MimironApproach
{
    Position dest;
    char const* how;
    float turn;
};

// Where the formation sends this bot next, best first, and empty when nothing gets there. Once the
// fire covers most of the floor, refusing every slot that burns and every walk that crosses fire
// left ranged a median 15 to 24 yd off their slots from phase 2 on. So:
// - a slot that is not clear gives way, outside phase 1, to the nearest clear point within
//   ULDUAR_MIMIRON_SLOT_SUBSTITUTE_RADIUS that keeps ULDUAR_MIMIRON_DISPERSE_DISTANCE off everyone;
// - a walk through fire goes via one waypoint whose two legs both miss it.
// Empty as well when the bot already stands on clear ground within the substitute radius. Every
// candidate is screened against the Laser Barrage cone with its own travel time, like a flee
// bearing: this is the largest mover in the fight and its legs outlive the sweep that kills.
std::vector<MimironApproach> GetMimironSlotApproaches(PlayerbotAI* botAI, Player* bot, Position const& slot,
                                                      MimironFirefighterHazards const& hazards);

// The main tank's slot in phases 1 and 4 is a boss-holding spot, not somewhere it is free to refuse:
// the MK II parks on top of the mine field it just laid, so a tank that will not stand in one never
// brings the boss back.
bool IsMimironTankAnchorSlot(PlayerbotAI* botAI, Player* bot);

// Whether this bot owns the Plasma Blast window that is casting now. First caller takes it and
// everyone else stands down, which is what keeps a Shield Wall and a Pain Suppression off the same
// five seconds - either one alone carries the window, and the second is wasted.
bool ClaimMimironPlasmaWindow(Player* bot);

// Whether the main tank has enough of a threat lead on the MK II to start the walk west. Latches
// once per pull: a mid-phase dip must not send him back and restart the drag with the raid already
// spread out behind him.
bool IsMimironTankDragReady(PlayerbotAI* botAI, Player* bot);

// Which of ULDUAR_MIMIRON_PHASE1_STACK_SPOTS the camp points at. Decided once per instance and not
// per bot - twelve bots each picking their own cleanest anchor is twelve camps, and one camp is the
// whole point of the shape.
Position const& GetMimironPhase1StackAnchor(PlayerbotAI* botAI, Player* bot);

// Drops the phase 1 latches. Called off the constructs rather than any one bot's combat state, so a
// wipe re-arms the tank hold and the stack goes back to its default anchor for the next pull.
void ResetMimironFightState(Player* bot);

// The fire dodge is the only node that knows where the fire is, and it only moves the bot once.
// Stamping the escape lets everything that does not know hold still for ULDUAR_MIMIRON_FLAMES_HOLD_MS
// rather than undo it.
void NoteMimironFireDodge(Player* bot);
bool IsMimironFireHoldActive(Player* bot);

// A spot within `range` of `target` that is not burning, for a bot that has to close on an add
// standing in the fire. False when the bot is already in range, when the straight approach is
// clear anyway - "reach melee" and "reach spell" do that themselves, and better - or when no
// bearing round the target is clear. Only the destination is screened: walking through a node
// costs one tick of it, standing in one costs the phase.
bool GetMimironTargetApproach(PlayerbotAI* botAI, Player* bot, Unit* target, float range,
                              Position& out);

// The range the bot's own reach node would stop at, so the two agree on what "in range" means.
float GetMimironApproachRange(PlayerbotAI* botAI, Player* bot);

// What the phase 1 node writes as the generic unstacker's minimum spacing, and what its trigger
// latches against. One accessor because the two have to agree: that trigger re-arms until the value
// it reads back matches the one the action wrote, so a literal on either side leaves it permanently
// active and the action then wins every tick at ACTION_RAID.
float GetMimironPhase1DisperseDistance(PlayerbotAI* botAI);

// The 20 s a Magnetic Core buys. The Aerial Command Unit is on the floor, passive, and taking +50%
// damage. Its UpdateAI returns before the event map for the whole aura and the landing delays every
// event 25 s on top, so the next add comes about 45 s after the core. It is the only stretch of
// phase 3 in which the boss can be killed at all.
bool IsMimironAcuGrounded(PlayerbotAI* botAI);

// Phase 3 with the unit in the air. Nothing melee can reach it then, and walking toward it only drags
// it, since it holds 30 yd from whoever it is on.
bool IsMimironAcuAirborne(PlayerbotAI* botAI, Player* bot);

// An Assault Bot corpse that still has its Magnetic Core: one core per corpse, taken off the corpse's
// own loot when the loot was filled, so a player who looted it first leaves nothing for the bot.
Creature* GetMimironCoreCorpse(Player* bot);

// Moves that corpse's core into the bot's bags and marks the corpse spent. False on full bags.
bool TakeMimironCore(Player* bot, Creature* corpse);

// Whether a core may go down now: the unit is in the air and no earlier core is still live or arming.
bool IsMimironCoreUseReady(PlayerbotAI* botAI, Player* bot);

// Starts that arming window. Held per instance, so a new carrier picking the next core up still sees
// the one its predecessor sent down.
void NoteMimironCoreSpent(Player* bot);

// The ranged snare this bot can put on a Bomb Bot, or empty for a class that has none. Roots are
// deliberately absent, but not because they break: neither Entangling Roots nor Frost Nova carries
// AURA_INTERRUPT_FLAG_TAKE_DAMAGE in 3.3.5. They are absent because a Bomb Bot has to be stopped
// while it is still approaching, and both of those land only at or around the caster.
// Trigger and action both read this, or the two disagree about who is covered.
std::string GetMimironBombBotSnare(Player* bot);

// How far a Bomb Bot still has to run before it reaches whoever it is chasing. Measured from its own
// victim rather than from the caster: a hunter 25 yd away would otherwise spend a global snaring one
// that is already two yards from a healer. Falls back to the caster's own distance before it picks
// a victim.
float GetMimironBombBotApproach(Player* bot, Unit* bombBot);

// The Bomb Bot chasing this bot, if any. It runs 8.0 yd/s against a player's 7.0 and detonates on
// contact, so the one it is after cannot leave and has to shoot it down, while anyone else inside the
// blast can step out and should. The DPS list and the sidestep both read this because each used to
// defer to the other - one dropped the Bomb Bot below the blast radius, the other stood down whenever
// one was in spell range - and a ranged bot inside 8 yd therefore did neither.
Unit* GetMimironBombBotChasing(PlayerbotAI* botAI, Player* bot);

// The mech the ranged formation is shaped around. Phase order is MK II, VX-001, Aerial Command Unit,
// then all three together, and VX-001 is the one that stays parked once they reassemble.
Unit* GetMimironRingFocus(PlayerbotAI* botAI);

// Which mech is coming next while none of them is attackable yet. A defeated mech keeps
// UNIT_FLAG_NOT_SELECTABLE and the next one carries it until its phase starts, so "possible targets no
// los" is blind for the whole handover - 47.75 s from phase 1 to 2, 24 s to phase 3, 31.8 s to phase 4
// - and a grid scan is the only thing that sees them. Read to tell a phase 4 handover from the other
// two, which is what puts the main tank on the chassis spot before VX-001 goes live.
Unit* GetMimironStagingFocus(Player* bot);

// Cheap screen in front of a grid scan or a slot derivation: the hard mode switch is a config
// read that holds all over Ulduar, so a node gated on it alone runs in every fight in the
// instance. Bounded by the staging search range rather than the room, so it never cuts a bot
// out of the fight.
bool IsNearMimironRoom(Player* bot);

// The encounter's tick: the trace's phase, core and barrage rows, and the fight state reset after a
// wipe. Throttled per instance, so every bot may call it every tick.
void MimironTick(PlayerbotAI* botAI);

// Any of the three constructs actually fighting. Presence says nothing here: Leviathan MK II is a DB
// spawn that sits in the room unselectable until the button is pushed, and GetFirstAliveUnitByEntry
// does not filter selectability.
bool IsMimironEngaged(PlayerbotAI* botAI);

// Phase 4, start to finish. Keyed on VX-001 riding the chassis rather than on all three being
// attackable: a part pushed under 15000 sets UNIT_FLAG_NON_ATTACKABLE and drops out of the target list,
// and the phase is at its most time-critical after that, not over.
bool IsMimironPhase4(Player* bot);

// Phase 1: the MK II is up and neither later construct is. Phase 4 excluded by the vehicle seat,
// since a part self-repairing drops off the target list and leaves the MK II looking alone.
bool MimironPhase1Active(PlayerbotAI* botAI);

// Phase 2: VX-001 on the floor on its own. Riding the chassis is phase 4.
bool IsMimironPhase2(PlayerbotAI* botAI);

// Raid members within heal distance at or under LowHealth before Divine Sacrifice, Divine Hymn and
// Power Infusion may go out in phase 2. Their shared trigger wants 6 at 65 %, which the walk-in Rapid
// Burst meets in the first seconds, and all three are then on cooldown for the Heat Wave, bomb and
// bursts that land together after the first barrage. Before that barrage the count at 45 % peaked at
// 4, after it at 11 to 17.
constexpr uint32 ULDUAR_MIMIRON_STORM_COOLDOWN_LOW_COUNT = 6;

// Phase 3: the Aerial Command Unit up on its own. "possible targets no los" drops unselectable units,
// so the idle MK II and VX-001 are not in it and the unit only is once it can be attacked.
bool IsMimironPhase3(PlayerbotAI* botAI);

// The paladin that swaps to Frost Resistance Aura for phase 3 of hard mode, or nullptr. Water Spray
// is frost and takes partial resists, and a topped bot survives it at 20 % resisted: the aura puts
// every spray at 10 % or more, where Gift of the Wild's 54 alone leaves 28 % unresisted. The two
// share an exclusive aura type, so only the higher counts. Never the fire carrier, that aura holds
// a fifth of the fire off everyone; a tank first, since it stands central (24 of 26 spray hits
// landed within its 40 yd), then a non-healer, then a healer. A lone paladin keeps fire.
Player* GetMimironFrostResistancePaladin(PlayerbotAI* botAI, Player* bot);

// What this bot should be hitting in phase 4. nullptr means hold - everything it is allowed to touch is
// already at the floor, and pushing a part under early costs the whole rendezvous. A melee answer is the
// Aerial Command Unit while both ground parts wait on it.
// `melee` is a parameter rather than derived from the bot so the pet node can ask for a melee answer on
// behalf of a hunter.
Unit* GetMimironPhase4Focus(PlayerbotAI* botAI, Player* bot, bool melee);

// The main tank's phase 4 target: the MK II, the one part that swings and walks after its victim, so
// "lose aggro" can see it. The shared focus only while the MK II is self-repairing. Never a hold.
Unit* GetMimironPhase4TankFocus(PlayerbotAI* botAI, Player* bot);

// Who fetches the Magnetic Core. Group order so every bot computes the same answer, but melee first:
// they are already standing on the Assault Bot when it dies, whereas the plain first-bot-in-group pick
// is usually a ranged bot 22 yd out that never comes within loot range of anything.
Player* GetMimironCoreCarrier(PlayerbotAI* botAI);

// Seconds until the Laser Barrage ignites, or -1 when VX-001 is not spinning up. Spinning Up is a 4 s
// channel on VX-001 and leaves no aura on it: 63414 sends effect 0 to the DB Target and effect 1 to the
// MK II, and its third effect does nothing, so HasAura never answers true and the whole telegraph reads
// as nothing at all. FindCurrentSpellBySpellId is what the encounter script itself polls.
float GetMimironSpinningUpSeconds(Unit* vx001);

// The P3Wx2 Laser Barrage cone as it will actually be, worked out live rather than latched. The beams
// follow VX-001's facing, which the core repoints at NPC 33576 on every tick of the barrage aura, so
// the bearing to that NPC is the centreline - but only from the moment the barrage lands. Spinning Up
// aims once and then holds for four seconds, during which 33576 travels another 42.6 degrees, so the
// cone ignites well clockwise of where the boss is visibly pointing.
struct MimironBarrageWindow
{
    bool valid = false;
    float lead = 0.0f;       // world bearing of the centreline at ignition, or right now if firing
    float sweep = 0.0f;      // clockwise radians still to come, below one full turn
    float rate = 0.0f;       // radians per second, 0 while still spinning up
    float untilLive = 0.0f;  // seconds until the first damage tick, 0 once firing
};

// Live every tick, so a moving apex, a rotating chassis and the bearing rate swinging 8.3-14.7 deg/s
// off centre all fall out for free, and every bot derives the same cone without coordinating.
MimironBarrageWindow GetMimironBarrageWindow(Player* bot, Unit* vx001);

// Whether `dest` is still clear of everywhere the beams will sweep, `travelSeconds` from now.
// Predictive on purpose: a MOVEMENT_FORCED leg holds the movement lock for its whole duration - up to
// 2.6 s for an 18 yd Shock Blast flee - and the cone turns about 10.6 degrees a second, so "clear when
// the move was issued" is the wrong question to ask. Measured clockwise from the ignition centreline
// and never folded to a signed angle: the band is 240 degrees wide at the room centre, wider off it.
// The ULDUAR_MIMIRON_BARRAGE_BOUNDARY circle is refused whatever the bearing.
//
// Takes the window rather than reading it, because GetMimironBarrageWindow runs a grid scan for the
// DB Target and callers test a fan of a dozen bearings against the same cast.
bool IsMimironSpotBarrageSafe(Unit* vx001, MimironBarrageWindow const& window, Position const& dest,
                              float travelSeconds);

// Where this bot stands between barrages. Ranged fan out over a full ring round the room rather than
// around VX-001, whose facing swings to whoever it last Rapid Burst. Returns false for roles this does
// not place, and for everyone during a handover - the raid follows its master between phases, and only
// the phase 4 main tank has a spot to hold. Trigger and action must both call this or the two disagree
// about where the bot belongs. Hard-mode phase 2 melee get the sector opposite the ranged wedge.
bool GetMimironSpreadSlot(PlayerbotAI* botAI, Player* bot, Position& out);

// The cannon's Plasma Blast cast, or nullptr when it is between casts. Matches both ids:
// spelldifficulty_dbc in the world DB remaps 62997 to 64529 on 25-man, so the id the boss
// script names is never the one a 25-man cast carries.
Spell* GetMimironPlasmaBlastCast(Unit* cannon);

// The live Rapid Burst cone, or a window that is not valid when nothing is firing. The centreline is
// taken from the raid member carrying 63382 rather than from VX-001's orientation: the boss is
// pointed at that player once, when the aura lands, and holds it for the whole 3 s, so the carrier is
// the exact bearing where the orientation is only the last thing the server happened to write.
struct MimironRapidBurstWindow
{
    bool valid = false;
    float centreline = 0.0f;  // world bearing from VX-001 to the aura carrier
};

MimironRapidBurstWindow GetMimironRapidBurstWindow(PlayerbotAI* botAI, Player* bot, Unit* vx001);

// Whether `dest` is outside the cone as it is pointing now, the true 60 degrees with no margin.
bool IsMimironSpotRapidBurstSafe(Unit* vx001, MimironRapidBurstWindow const& window,
                                 Position const& dest);

// Hard-mode phase 2: the bearings off VX-001 where this bot would share every Rapid Burst aimed at
// the other group. For melee that's the ranged wedge, for ranged the melee sector, each widened by
// the cone's half-width. Not valid anywhere else.
struct MimironBurstSector
{
    bool valid = false;
    float centre = 0.0f;  // world bearing from VX-001
    float halfWidth = 0.0f;
};

MimironBurstSector GetMimironOtherGroupSector(PlayerbotAI* botAI, Player* bot, Unit* vx001);

bool IsMimironSpotInSector(Unit* vx001, MimironBurstSector const& sector, Position const& dest);

// Firefighter ground fire. A node's damage aura 64566 reaches 3 yd, so 5 covers the node footprint
// and pathing slop. Chains grow in 7 yd steps and 50 to 60 nodes are alive by the middle of the
// fight, so a destination has to clear every node it knows about rather than just the nearest one -
// a hop shorter than the step lands on the next node along.
constexpr float ULDUAR_MIMIRON_FLAMES_RADIUS = 5.0f;

// How far from a slot that is not clear a bot may stand in for it in phase 1, and how close it has to
// be already to stay put rather than look at all. The camp's rows are 6 yd apart, and a stand-in
// squeezed between them is how two bots end up under one Napalm Shell.
constexpr float ULDUAR_MIMIRON_SLOT_SUBSTITUTE_RADIUS = 6.0f;

// The same search outside phase 1, where the fire covers the floor and a 6 yd ring round a burning
// slot burns too - 30 % of a Firefighter phase 3 was ranged standing still off their slots because
// nothing on the ring was clear. The phase 3 wedge orbits 17 to 27 yd out against a 35 yd cast
// range, so 12 still leaves the bot in range, and every candidate is screened against
// ULDUAR_MIMIRON_DISPERSE_DISTANCE anyway, so widening cannot clump the raid.
constexpr float ULDUAR_MIMIRON_SLOT_SUBSTITUTE_RADIUS_WIDE = 12.0f;

// Detour waypoint turns off the direct bearing, degrees, tried in order. The waypoint sits over the
// walk's midpoint, so 65 makes the walk 2.4 times as long. Past that it is walking away.
constexpr float ULDUAR_MIMIRON_DETOUR_TURNS_DEG[] = {20.0f, 35.0f, 50.0f, 65.0f};

// How far the fire has to be before a bot may sit down to eat or drink. DrinkAction and EatAction push
// the bot's next AI check back 12 to 18 s, and a chain grows 1.22 yd/s toward the nearest player,
// which a bot sitting still usually is.
constexpr float ULDUAR_MIMIRON_DRINK_FIRE_CLEARANCE = 15.0f;

// Frost Bomb Explosion 65333: 30 yd, 47124 base, plus a knockback. That is about twice a bot's
// health pool, so this is a positional check and no amount of healing answers it. The bomb summons
// on a burning flame node - 64623's condition rows require entry 34121 carrying aura 64561 - and its
// SmartAI detonates 10 s after the spawn, which is the whole warning.
constexpr float ULDUAR_MIMIRON_FROST_BOMB_RADIUS = 30.0f;
// Where a destination has to be. The flee trigger's FindNearestCreature adds both bounding radii to
// the 30 and lets go near 31, so a spot picked at 30.5 flees again on the next tick. Two pulls
// had 136 of 211 accepted escapes land short of the 34 they aimed for, 19 of them under 32.
constexpr float ULDUAR_MIMIRON_FROST_BOMB_STAND_RADIUS = 32.0f;
// Where to stand rather than what the blast reaches: the extra clears the knockback and the yard or
// two a leg overshoots by.
constexpr float ULDUAR_MIMIRON_FROST_BOMB_CLEARANCE = 34.0f;
// How far past the clearance reach moves stay held while a bomb is live. The flee stops at the
// clearance and its trigger lets go just inside it, so without a band past both, "reach spell" or a
// heal reach walks the bot straight back in for the rest of the fuse.
constexpr float ULDUAR_MIMIRON_FROST_BOMB_HOLD_MARGIN = 4.0f;

// Emergency Fire Bots are kept alive through phase 3 to put the fire out. They never attack anyone:
// each walks to the nearest Flames (Spread) and hits it with Water Spray 64619. Every one of them is
// left alone until the cleanup: three spawn every 45 s, the fire is 41 to 46 % of all phase 3 intake
// with 23 to 26 nodes alive at a time, and culling down to a pair held the field at that size while
// the raid spent 6 % of its phase 3 shooting the fire brigade.
//
// Aerial Command Unit health at which they all go on the kill list. None may reach phase 4, where
// they spray straight into the rendezvous.
constexpr float ULDUAR_MIMIRON_FIREBOT_CLEANUP_PCT = 15.0f;
// How many Junk Bots have to be up before the melee leave the Assault Bot for them. One arrives every
// 10 s with 251k health and nothing else kills them: they reached nine alive, lived 63 s each and took
// 627k off the raid, more than any other source in phase 3. Melee pay almost nothing to switch, since
// the nearest is 3.4 yd away with four inside a cleave, while the Assault Bot stands 7.7 yd off the
// pile. Ranged and healers keep it, because it is the only Magnetic Core source.
constexpr uint32 ULDUAR_MIMIRON_JUNK_BOT_PILE = 4;

// Water Spray is SPELL_ATTR0_CU_CONE_LINE: a line 15 yd ahead of the bot, as wide as both object
// sizes, about 2.3 yd each side. 18850 to 21150 frost plus Emergency Mode's 25 % and a knockback,
// most of a bot's pool. The extra covers a step's overshoot and the bot turning to its next flame.
constexpr float ULDUAR_MIMIRON_FIREBOT_SPRAY_LENGTH = 16.0f;
constexpr float ULDUAR_MIMIRON_FIREBOT_SPRAY_HALF_WIDTH = 3.5f;
// Where a destination has to stay off a lane. The sidestep lands at the run width plus 1.5, past
// this, and a lane jumps whenever a spray puts out the flame it points at, so without the gap a
// formation slot just outside the run width flips with the dodge.
constexpr float ULDUAR_MIMIRON_FIREBOT_SPRAY_STAND_HALF_WIDTH = 4.5f;
// The script's reach: a fire bot with its nearest spread flame this close sprays at once, else it
// walks to this far short of the flame and sprays on arrival, facing the way it walked. So the next
// spray is predictable. A stationary bot turns and fires in the same tick, and a walking bot's line
// starts where it stops, which is why the live line alone warned one victim in 14; the lane toward
// the nearest flame, read 2 s ahead, held 13 of the 14.
constexpr float ULDUAR_MIMIRON_FIREBOT_SPRAY_REACH = 5.0f;
// Deafening Siren 64616 is a 10 yd area silence, on the 25-man bot only (creature_template_addon), and
// area auras add both object sizes to the radius.
constexpr float ULDUAR_MIMIRON_FIREBOT_SIREN_CLEARANCE = 13.0f;
// Where a caster's destination has to be, and where the siren dodge aims. The kept bots walk to the
// next flame, which is usually in the raid: formation spots picked just past 13 (median 15.7) had
// the bot walked back into at 13.2 a moment later, 212 times in one phase 3.
constexpr float ULDUAR_MIMIRON_FIREBOT_SIREN_STAND = 17.0f;
constexpr float ULDUAR_MIMIRON_FIREBOT_SIREN_FLEE = 19.0f;
// How close a kept fire bot may get to the bot, or to what it is hitting, before damage AoE is held.
// Bots cannot aim AoE away from one, and the kept ones walk into the raid after the fire. It was 30,
// which is affordable for a pair and switches the raid's AoE off for all of phase 3 once nine to
// fifteen are alive - and phase 3 is where a Junk Bot arrives every 10 s. The widest raid AoE in
// play is about 10 yd (Death and Decay, Hurricane, Blizzard), so 12 covers the splash that reaches
// one of them.
constexpr float ULDUAR_MIMIRON_FIREBOT_AOE_CLEARANCE = 12.0f;

// Health below which a bot is willing to pay for a fire dodge. Above it the fire is cheaper than the
// trip: a node ticks about 3100 against a 22000 to 24000 pool, and the round trip out and back is
// 24 yd of a melee bot's uptime or a ranged bot's cast. Below it the arithmetic flips, because the
// bot no longer has the seven ticks of margin that made standing still affordable.
constexpr float ULDUAR_MIMIRON_FLAMES_DODGE_HEALTH_PCT = 60.0f;

// How many nodes overrule the health gate. One node is about 8 s of margin from full; two is 4, and
// four seconds is not enough to notice a health bar moving and then walk 12 yd.
constexpr uint32 ULDUAR_MIMIRON_FLAMES_DODGE_NODE_OVERRIDE = 2;

// How long after a fire dodge the generic movers stay out of it. They pick a destination with no
// idea the floor is burning - two thirds of every "reach melee" leg in a Firefighter phase 3
// aimed inside a node, and 97% of the follow legs in a handover - so the bot pays for a dodge and
// is walked back in on the next tick, once every 3.1 s. Long enough to outlast that, short enough
// that nobody is stranded while the fire covers a third of the floor.
constexpr uint32 ULDUAR_MIMIRON_FLAMES_HOLD_MS = 2500;

// How far inside its own stop distance a bot closing on a target parks. Landing exactly on the
// edge leaves it one step short the moment the target shuffles, and the step back out is another
// walk through the fire.
constexpr float ULDUAR_MIMIRON_APPROACH_INSET = 2.0f;

// The distance ladder a fire dodge tries, each added to the burning cluster's own edge. A chain adds
// a node every 5.75 s exactly 7 yd along, so a hop shorter than that can land on the next node - but
// one fixed 7 yd hop found no clean bearing four times out of five and fell through to an unscreened
// MoveAway anyway. Screening every rung against the whole field is what makes the short ones safe,
// and landing on one is the half of the dodge's cost that comes back as uptime.
constexpr float ULDUAR_MIMIRON_FLAMES_STEP_LADDER[] = {3.0f, 5.0f, 7.0f, 10.0f};

// Longest leg a fire dodge may ask for, whatever the ladder and the burning cluster's own width come
// to. The dodge issues at MOVEMENT_FORCED, so its leg holds the movement lock against the Rapid
// Burst and Frost Bomb dodges, and Rapid Burst lands with no telegraph to stand down for - 12 yd is
// about 1.7 s at run speed, which is what a fire hop can then cost one of those. This trades reach
// and not safety: a shortened hop that still lands in fire is refused by the fan like any other.
constexpr float ULDUAR_MIMIRON_FLAMES_MAX_HOP = 12.0f;

// Shortest step a swept bearing may actually cover, as a share of the hop it asked for.
// CheckCollisionAndGetValidCoords rewrites the destination to the raycast's last point and
// still reports success on a PATHFIND_INCOMPLETE result, so a bearing that leaves the mesh
// comes back as a destination on the bot's own feet - and the fan's back-tracking test only
// asks for further from the hazard, which 7 mm satisfies. Three of those in a row read as
// three escapes and left a bot 0.3 yd from where a Shock Blast was about to land.
constexpr float ULDUAR_MIMIRON_FLEE_MIN_PROGRESS_PCT = 0.5f;

// Rapid Burst 64531/64532 is a 60 degree cone, so plus or minus 30 off VX-001's facing, 100 yd deep.
// acore_world.spell_cone says so and Spell::SelectImplicitConeTargets reads it before falling back to
// the DBC's TARGET_UNIT_CONE_ENEMY_104 default - but that table is not trusted on its own here, since
// its row for the Laser Barrage does not match observed behaviour. This one is measured: bucket every
// bot past 14 yd by its bearing at the tick before a hit and the hit rate holds around 80 % out to 30
// degrees, then drops to 9 % at 30-40 and about 1 % beyond.
constexpr float ULDUAR_MIMIRON_RAPID_BURST_HALF_ANGLE = 30.0f * static_cast<float>(M_PI) / 180.0f;

// Hard-mode phase 2 melee sector, centred opposite the ranged wedge. The 5 yd slot tolerance on the
// 9 yd ring is ~32 degrees of slack, so a melee bot can sit 77 off centre and still be past the 90
// where the wedge's 60 plus the Rapid Burst cone's 30 ends.
constexpr float ULDUAR_MIMIRON_PHASE2_MELEE_HALF_ANGLE = 45.0f * static_cast<float>(M_PI) / 180.0f;

// Added to VX-001's combat reach for the furthest a melee stand-in may sit: 10.0, inside the 10.25
// where "reach melee" fires and walks the bot straight at the boss off its sector.
constexpr float ULDUAR_MIMIRON_PHASE2_MELEE_REACH_MARGIN = 2.0f;

// VX-001 fights here and the Aerial Command Unit is summoned overhead, so a ring anchored to this
// point holds still while the mechs turn and charge about.
extern const Position ULDUAR_MIMIRON_ROOM_CENTER;
// Phase 3 staging, 18 yd east of the room centre. The add summon pads sit on three arms - west,
// north-east and south-east - so the east wedge is the one stretch of floor nothing walks down.
// Grouping there funnels every Junk and Assault Bot into the melee instead of into a lone ranged bot.
// navprobe: this point and a 12 yd fan around it are 16/16 on mesh, flat at Z 364.31.
extern const Position ULDUAR_MIMIRON_PHASE3_STAGE;
extern const Position ULDUAR_MIMIRON_PHASE4_TANK_SPOT;

// Firefighter phase 1, 53 yd west of the room centre. Mimiron seeds fire 5 yd from three random raid
// members every 30 s and each chain then crawls toward whoever is nearest it, so the fire ends up
// wherever the raid stood - and the raid stands on its tank. Holding the MK II out here keeps every
// batch off the ground VX-001 is summoned onto and phases 2 to 4 are fought on.
//
// navprobe map 603: 0.223 yd to the nearest poly, settles flat at Z 364.314, and rings around it are
// 16/16 on mesh at 12 yd and 14/16 at 18 and 24, the failures all west in the raised doorway alcove.
// 51.5 yd from Mimiron's own spawn, and he evades past 80 yd from it on every tick - that check is
// the only leash in the encounter, the MK II has none of its own.
extern const Position ULDUAR_MIMIRON_PHASE1_TANK_SPOT;

// Where the Firefighter phase 1 camp points: each is the wedge's middle-row centre, 27 yd from the
// tank spot on bearings 25 and 345. Two rather than more, because the camp is what the fire
// converges on and switching has to actually walk it off its own anchor. These two are 18.5 yd apart,
// still more than a 5 yd node cluster plus a 7 yd chain step. Index 0 is the default.
//
// Bearings further out put the wedge edges on the alcove walls' shadow line: over 323 traced MK II
// positions, 30 and 330 hid the worst edge slot 21 % and 38 % of the time, these 1.5 % and 1.2 %. The
// south shadow is the wider one because the MK II drifts south-west.
//
// navprobe map 603: every bearing from 288 to 74 off the tank spot is on mesh at 21, 27 and 33 yd and
// settles at Z 364.314. Westward bearings are out - 105 to 255 degrees is off mesh against the wall or
// up in the raised doorway alcove - and 90 and 270 sit on holes in the mesh.
constexpr uint8 ULDUAR_MIMIRON_PHASE1_STACK_COUNT = 2;
extern const Position ULDUAR_MIMIRON_PHASE1_STACK_SPOTS[ULDUAR_MIMIRON_PHASE1_STACK_COUNT];

#endif
