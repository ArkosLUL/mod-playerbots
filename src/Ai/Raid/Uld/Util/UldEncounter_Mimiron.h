/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERMIMIRON_H
#define PLAYERBOTS_ULDENCOUNTERMIMIRON_H

#include "Position.h"
#include "UldData.h"

#include <string>
#include <vector>

class Player;
class PlayerbotAI;
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
    SPELL_MIMIRON_NAPALM_SHELL = 63666,
    SPELL_MIMIRON_MAGNETIC_FIELD = 64668,
    ITEM_MIMIRON_MAGNETIC_CORE = 46029,  // 100% drop from the Assault Bot; grounds the ACU
    SPELL_MIMIRON_MAGNETIC_CORE_AURA = 64436,  // the 20s grounding itself, not the field 64668

    // Mimiron hard mode ("Firefighter", Big Red Button pressed): mechs empowered, two extra hazards.
    // NPC_MIMIRON (the boss; sits in his pod, never a bot attack target) comes from core ulduar.h via UldScripts.h.
    NPC_FLAMES_INITIAL = 34363,    // fire seed dropped on players, spawns a spreading node (non-selectable)
    NPC_FLAMES_SPREAD = 34121,     // persistent spreading ground-fire node (non-selectable)
    NPC_FROST_BOMB = 34149,        // VX-001's Frost Bomb; detonates in a large AoE
    NPC_EMERGENCY_FIRE_BOT = 34147  // puts the flames out; three spawn every 45s
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
constexpr float ULDUAR_MIMIRON_PHASE3_SPACING = 6.0f;
constexpr float ULDUAR_MIMIRON_PHASE3_MIN_RADIUS = 18.0f;

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

// Where melee and tanks wait out a phase handover. Eight yards puts them inside melee range of a
// combat-reach 8 mech the moment it goes live. Staging only - see GetMimironSpreadSlot for why melee
// never get a slot during a live phase.
constexpr float ULDUAR_MIMIRON_STAGING_MELEE_RADIUS = 8.0f;

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

// Same idea for anywhere a bot is asked to stand rather than flee to: mines plus any Rocket Strike
// marker still burning its fuse. Positioning that ignores markers walks a bot that just dodged one
// straight back onto it.
bool IsMimironSpotSafe(Player* bot, Position const& dest);

// The main tank's slot in phases 1 and 4 is a boss-holding spot, not somewhere it is free to refuse:
// the MK II parks on top of the mine field it just laid, so a tank that will not stand in one never
// brings the boss back.
bool IsMimironTankAnchorSlot(PlayerbotAI* botAI, Player* bot);

// The 20 s a Magnetic Core buys. The Aerial Command Unit is on the floor, passive, and taking +50%
// damage, and its own UpdateAI is short-circuited for the whole aura so nothing new spawns for 25 s.
// It is the only stretch of phase 3 in which the boss can be killed at all.
bool IsMimironAcuGrounded(PlayerbotAI* botAI);

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

// The mech to form up on when none of them is attackable yet. A defeated mech keeps
// UNIT_FLAG_NOT_SELECTABLE and the next one carries it until its phase starts, so "possible targets no
// los" is blind for the whole handover - 47.75 s from phase 1 to 2, 24 s to phase 3, 31.8 s to phase 4.
// Nothing else fires either, so the engine falls through to follow at relevance 1.0 and the raid trails
// its master. A grid scan does see them, which is enough to walk everyone to the next phase in advance.
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
// around VX-001, whose facing swings to whoever it last Rapid Burst; Rapid Burst and Hand Pulse are
// both 104 degree cones, so covering more bearings than one cone can hold is the only thing that
// helps. Returns false for roles this does not place. Trigger and action must both call this or the
// two disagree about where the bot belongs.
bool GetMimironSpreadSlot(PlayerbotAI* botAI, Player* bot, Position& out);

// Mimiron hard mode: bots flee a persistent fire node when this close (cells are small), and clear
// the Frost Bomb's larger explosion. Exact radii are DBC, so these are conservative defaults to
// confirm in-game.
constexpr float ULDUAR_MIMIRON_FLAMES_RADIUS = 5.0f;
constexpr float ULDUAR_MIMIRON_FROST_BOMB_RADIUS = 12.0f;

// VX-001 fights here and the Aerial Command Unit is summoned overhead, so a ring anchored to this
// point holds still while the mechs turn and charge about.
extern const Position ULDUAR_MIMIRON_ROOM_CENTER;
// Phase 3 staging, 18 yd east of the room centre. The add summon pads sit on three arms - west,
// north-east and south-east - so the east wedge is the one stretch of floor nothing walks down.
// Grouping there funnels every Junk and Assault Bot into the melee instead of into a lone ranged bot.
// navprobe: this point and a 12 yd fan around it are 16/16 on mesh, flat at Z 364.31.
extern const Position ULDUAR_MIMIRON_PHASE3_STAGE;
extern const Position ULDUAR_MIMIRON_PHASE4_TANK_SPOT;

#endif
