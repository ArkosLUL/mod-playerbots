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

// The phase 4 main tank orbits inside the chassis's chase range instead, so the MK II stays put and
// the cone apex with it. It cannot simply hold its spot: 20000 per 250 ms tick kills it outright.
//
// 1.5 is chosen against Unit::GetMeleeRange - reach 8 plus a player's 1.5 plus 4/3, so 10.83 yd. A
// tank orbiting at 9.5 never leaves that, so the chassis never chases and the apex holds still.
// This is load-bearing for every other bot: simulation clears every bearing, radius and offset
// against a stationary apex, and only ever fails once the apex is dragged around under the raid.
constexpr float ULDUAR_MIMIRON_BARRAGE_TANK_RING_MARGIN = 1.5f;

// A bot turns around VX-001 at (7.0 yd/s / radius) against a 10.6 deg/s sweep. Holding the raid
// inside this radius makes the worst-case 52 degree rotation fit the 4 s Spinning Up warning and
// still leaves 1.6x speed margin; break-even is 38 yd, where a bot can never out-turn the cone.
constexpr float ULDUAR_MIMIRON_SPREAD_RADIUS_MAX = 24.0f;

// Ranged ring, kept inside ULDUAR_MIMIRON_SPREAD_RADIUS_MAX so a barrage rotation from it still
// fits the Spinning Up window. The tolerance is what stops bots pacing over a yard of drift.
constexpr float ULDUAR_MIMIRON_SPREAD_RADIUS = 22.0f;
constexpr float ULDUAR_MIMIRON_SPREAD_TOLERANCE = 5.0f;

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
};

MimironFirefighterHazards GetMimironFirefighterHazards(PlayerbotAI* botAI);

// Whether `dest` clears every gathered hazard of that kind. Two calls rather than one because the
// flee fan counts the two refusals apart, and which filter emptied a fan is the thing the trace has
// to be able to name.
bool IsMimironSpotFireSafe(MimironFirefighterHazards const& hazards, Position const& dest);
bool IsMimironSpotBombSafe(MimironFirefighterHazards const& hazards, Position const& dest);

// Out of every Emergency Fire Bot's Water Spray line, and for a caster or healer in 25-man also out of
// its silence aura. Takes the bot because only the second half depends on who is asking.
bool IsMimironSpotFireBotSafe(Player* bot, MimironFirefighterHazards const& hazards, Position const& dest);

// The fire bots the raid leaves alone for now, so they keep putting the fire out: the
// ULDUAR_MIMIRON_FIREBOT_KEEP oldest, in phase 3, until the Aerial Command Unit is low enough that
// the cleanup has to start. Empty otherwise. Raid-wide, so every bot spares the same ones.
std::vector<ObjectGuid> GetMimironKeptFireBots(PlayerbotAI* botAI, Player* bot);
bool IsMimironFireBotProtected(PlayerbotAI* botAI, Player* bot, Unit* fireBot);

// Whether `dest` is outside a Shock Blast the MK II is casting right now. The escape runs a bot out to
// ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST and then hands the tick back, so anything else that moves a
// bot during the 4 s cast has to refuse the circle too, or the formation walks it back in and a fire
// dodge can carry one in from outside.
bool IsMimironSpotShockSafe(PlayerbotAI* botAI, Position const& dest);

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
// Empty as well when the bot already stands on clear ground within the substitute radius.
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

// Whether a core may go down now: the unit is in the air and no earlier core is still live.
bool IsMimironCoreUseReady(PlayerbotAI* botAI, Player* bot);

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

// Any of the three constructs actually fighting. Presence says nothing here: Leviathan MK II is a DB
// spawn that sits in the room unselectable until the button is pushed, and GetFirstAliveUnitByEntry
// does not filter selectability.
bool IsMimironEngaged(PlayerbotAI* botAI);

// Phase 4, start to finish. Keyed on VX-001 riding the chassis rather than on all three being
// attackable: a part pushed under 15000 sets UNIT_FLAG_NON_ATTACKABLE and drops out of the target list,
// and the phase is at its most time-critical after that, not over.
bool IsMimironPhase4(Player* bot);

// What this bot should be hitting in phase 4. nullptr means hold - everything it is allowed to touch is
// already at ULDUAR_MIMIRON_PHASE4_HOLD_PCT, and pushing a part under early costs the whole rendezvous.
// `melee` is a parameter rather than derived from the bot so the pet node can ask for a melee answer on
// behalf of a hunter.
Unit* GetMimironPhase4Focus(PlayerbotAI* botAI, Player* bot, bool melee);

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
//
// Takes the window rather than reading it, because GetMimironBarrageWindow runs a grid scan for the
// DB Target and callers test a fan of a dozen bearings against the same cast.
bool IsMimironSpotBarrageSafe(Unit* vx001, MimironBarrageWindow const& window, Position const& dest,
                              float travelSeconds);

// Where this bot stands between barrages. Ranged fan out over a full ring round the room rather than
// around VX-001, whose facing swings to whoever it last Rapid Burst. Returns false for roles this does
// not place, and for everyone during a handover - the raid follows its master between phases, and only
// the phase 4 main tank has a spot to hold. Trigger and action must both call this or the two disagree
// about where the bot belongs.
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
    float offset = 0.0f;      // this bot's own bearing off it, signed, radians
    float escape = 0.0f;      // arc this bot must cover to clear the cone, yards; 0 when already out
};

MimironRapidBurstWindow GetMimironRapidBurstWindow(PlayerbotAI* botAI, Player* bot, Unit* vx001);

// Whether `dest` is outside the cone as it is pointing now. The true 60 degrees only, with no margin:
// the margin belongs to whoever is choosing somewhere to stand, and folding it in here would refuse a
// step aimed at exactly the edge it was told to clear.
bool IsMimironSpotRapidBurstSafe(Unit* vx001, MimironRapidBurstWindow const& window,
                                 Position const& dest);

// Firefighter ground fire. A node's damage aura 64566 reaches 3 yd, so 5 covers the node footprint
// and pathing slop. Chains grow in 7 yd steps and 50 to 60 nodes are alive by the middle of the
// fight, so a destination has to clear every node it knows about rather than just the nearest one -
// a hop shorter than the step lands on the next node along.
constexpr float ULDUAR_MIMIRON_FLAMES_RADIUS = 5.0f;

// How far from a slot that is not clear a bot may stand in for it. Further out the slot's spacing and
// range are gone, and holding where it is does as well.
constexpr float ULDUAR_MIMIRON_SLOT_SUBSTITUTE_RADIUS = 6.0f;

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
// Where to stand rather than what the blast reaches: the extra clears the knockback and the yard or
// two a leg overshoots by.
constexpr float ULDUAR_MIMIRON_FROST_BOMB_CLEARANCE = 34.0f;
// How far past the clearance reach moves stay held while a bomb is live. The flee stops at the
// clearance and its trigger lets go just inside it, so without a band past both, "reach spell" or a
// heal reach walks the bot straight back in for the rest of the fuse.
constexpr float ULDUAR_MIMIRON_FROST_BOMB_HOLD_MARGIN = 4.0f;

// Emergency Fire Bots are kept alive through phase 3 to put the fire out. They never attack anyone:
// each walks to the nearest Flames (Spread) and hits it with Water Spray 64619. Two stay and any more
// are culled, which keeps the silence and the spray line rare and the cleanup before phase 4 short.
constexpr uint32 ULDUAR_MIMIRON_FIREBOT_KEEP = 2;
// Aerial Command Unit health at which the kept ones go on the kill list too. None may reach phase 4,
// where they spray straight into the rendezvous.
constexpr float ULDUAR_MIMIRON_FIREBOT_CLEANUP_PCT = 15.0f;
// Water Spray is SPELL_ATTR0_CU_CONE_LINE: a line 15 yd ahead of the bot, as wide as both object
// sizes, about 2.3 yd each side. 18850 to 21150 frost plus Emergency Mode's 25 % and a knockback,
// most of a bot's pool. The extra covers a step's overshoot and the bot turning to its next flame.
constexpr float ULDUAR_MIMIRON_FIREBOT_SPRAY_LENGTH = 16.0f;
constexpr float ULDUAR_MIMIRON_FIREBOT_SPRAY_HALF_WIDTH = 3.5f;
// Deafening Siren 64616 is a 10 yd area silence, on the 25-man bot only (creature_template_addon), and
// area auras add both object sizes to the radius.
constexpr float ULDUAR_MIMIRON_FIREBOT_SIREN_CLEARANCE = 13.0f;
// How close a kept fire bot may get to the bot, or to what it is hitting, before damage AoE is held.
// Bots cannot aim AoE away from one, and the kept ones walk into the raid after the fire.
constexpr float ULDUAR_MIMIRON_FIREBOT_AOE_CLEARANCE = 30.0f;

// Health below which a bot is willing to pay for a fire dodge. Above it the fire is cheaper than the
// trip: a node ticks about 3100 against a 22000 to 24000 pool, and the round trip out and back is
// 24 yd of a melee bot's uptime or a ranged bot's cast. Below it the arithmetic flips, because the
// bot no longer has the seven ticks of margin that made standing still affordable.
constexpr float ULDUAR_MIMIRON_FLAMES_DODGE_HEALTH_PCT = 60.0f;

// How many nodes overrule the health gate. One node is about 8 s of margin from full; two is 4, and
// four seconds is not enough to notice a health bar moving and then walk 12 yd.
constexpr uint32 ULDUAR_MIMIRON_FLAMES_DODGE_NODE_OVERRIDE = 2;

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

// Rapid Burst 64531/64532 is a 60 degree cone, so plus or minus 30 off VX-001's facing, 100 yd deep.
// acore_world.spell_cone says so and Spell::SelectImplicitConeTargets reads it before falling back to
// the DBC's TARGET_UNIT_CONE_ENEMY_104 default - but that table is not trusted on its own here, since
// its row for the Laser Barrage does not match observed behaviour. This one is measured: bucket every
// bot past 14 yd by its bearing at the tick before a hit and the hit rate holds around 80 % out to 30
// degrees, then drops to 9 % at 30-40 and about 1 % beyond. The margin is the yard or two a leg
// overshoots by plus the boss's own turn between the aura landing and the bot arriving.
constexpr float ULDUAR_MIMIRON_RAPID_BURST_HALF_ANGLE = 30.0f * static_cast<float>(M_PI) / 180.0f;
constexpr float ULDUAR_MIMIRON_RAPID_BURST_MARGIN = 8.0f * static_cast<float>(M_PI) / 180.0f;

// Longest arc worth walking to leave the cone. The window is 3 s and ticks twice a second, so a step
// only pays while it finishes inside it; measured, half the victims needed under 6 yd and two thirds
// under 9, and past that the boss has re-aimed at somebody else before the bot arrives. Above this a
// bot stands still and eats it, which is the honest answer rather than a walk that buys nothing.
constexpr float ULDUAR_MIMIRON_RAPID_BURST_MAX_STEP = 9.0f;

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
