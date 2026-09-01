/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERFLAMELEVIATHAN_H
#define PLAYERBOTS_ULDENCOUNTERFLAMELEVIATHAN_H

#include "Position.h"
#include "UldData.h"

#include <vector>

class Player;
class PlayerbotAI;
class Unit;

// Flame Leviathan.
//
// The whole fight is fought from vehicles, so nothing here reads a bot's own position or spells -
// it reads the vehicle it is riding. A trigger and its action must call the same helper below or
// the two derivations disagree about who is doing what.
//
// Pursued picks a vehicle every 31s and the boss drives at it, dropping Battering Ram; the pursued
// vehicle kites the wall ring while choppers lay tar ahead of him and siege engines interrupt his
// Flame Vents channel. Each tower left standing empowers him and adds its own ground hazard.

enum UlduarFlameLeviathanIds
{
    // spawns that tower's periodic ground hazard).
    NPC_FL_THORIM_HAMMER_TARGET = 33364,     // Storm: static lightning-strike marks
    NPC_FL_MIMIRONS_INFERNO_TARGET = 33369,  // Flame: moving fire trail
    // Frost: walks to a target, roots itself on arrival, then fires 5s later where it stopped. The
    // strike carries a 60s stun (62297) with no mechanic and no dispel type, so a vehicle that eats
    // one is out of the fight for a minute and nothing can shorten it.
    NPC_FL_HODIRS_FURY_TARGET = 33108,

    // Flame Leviathan. Vehicle entries come from core ulduar.h via UldScripts.h; these are the
    // boss's own spells and the units the vehicles interact with.
    SPELL_FL_PURSUED = 62374,          // random vehicle every 31s, 35s; the boss then drives at it
    SPELL_FL_GATHERING_SPEED = 62375,  // +5% speed, stacks to 20, re-applied every 15s
    SPELL_FL_BATTERING_RAM = 62376,    // used on the pursued target inside 15 yd
    SPELL_FL_FLAME_VENTS = 62396,      // 10s channel; Electroshock interrupts it
    SPELL_FL_MISSILE_BARRAGE = 62400,
    NPC_FL_TURRET = 33139,             // boss-mounted turrets; never worth a vehicle's cast
    NPC_FL_DEFENSE_TURRET = 33142,
    NPC_FL_POOL_OF_TAR = 33090,        // faction 1965, i.e. the boss's; harmless to the raid
    NPC_FL_PYRITE_CONTAINER = 33189,   // grabbable crate, +25 energy to a demolisher
    NPC_FL_MECHANOLIFT = 33214,        // shot down with Anti-Air Rocket to drop a crate

    // Salvaged Siege Engine (33060) driver seat.
    SPELL_FL_RAM = 62345,
    SPELL_FL_ELECTROSHOCK = 62522,
    SPELL_FL_STEAM_RUSH = 62346,       // CHARGE_DEST along our own facing, not toward a target

    // Salvaged Siege Turret (33067), the siege engine's gunner.
    SPELL_FL_FIRE_CANNON = 62358,      // physical school despite the name; never ignites tar
    SPELL_FL_ANTI_AIR_ROCKET_SIEGE = 62359,
    SPELL_FL_SHIELD_GENERATOR = 64677,

    // Salvaged Chopper (33062).
    SPELL_FL_SONIC_HORN = 62974,
    SPELL_FL_TAR = 62286,              // pool spawns 9 yd BEHIND the chopper
    SPELL_FL_SPEED_BOOST = 62299,

    // Hodir's Fury's 60s stun, on the vehicle as well as the crew. Cleared by fire, see
    // ULDUAR_FL_HURL_BOULDER_MAX_RANGE.
    SPELL_FL_HODIRS_FURY_STUN = 62297,

    // Salvaged Demolisher (33109) driver seat.
    // Hurl Boulder triggers Boulder 62307, whose third effect triggers Flames 65045; Mortar 62634
    // triggers 62635, whose third triggers Flames 65044. spell_linked_spell maps both Flames to
    // -62297 ("Flames remove ice"), so either one thaws a frozen vehicle. Free, and the boulder's
    // own damage is enemy-only, so aiming one at a frozen ally costs nothing.
    SPELL_FL_HURL_BOULDER = 62306,
    SPELL_FL_HURL_PYRITE_BARREL = 62490,
    SPELL_FL_DEMOLISHER_RAM = 62308,
    SPELL_FL_BLUE_PYRITE_DOT = 68605,  // stacking DoT the barrel leaves on the boss, 10s, 10 stacks

    // Salvaged Demolisher Mechanic Seat (33167), the demolisher's gunner.
    SPELL_FL_MORTAR = 62634,
    SPELL_FL_ANTI_AIR_ROCKET = 64979,
    SPELL_FL_GRAB_CRATE = 62479,
    SPELL_FL_INCREASED_SPEED = 62471,

};

// Flame Leviathan hard-mode tower bitmask, used to pick which ground hazards to dodge.
enum FlameLeviathanTowerFlags
{
    FL_TOWER_STORM = 0x1,
    FL_TOWER_FLAMES = 0x2,
    FL_TOWER_FROST = 0x4,
    FL_TOWER_LIFE = 0x8,
    FL_TOWER_ALL = 0xF
};

// Vehicle keeps this clear of any active-tower ground hazard (strike / fire / frost).
constexpr float ULDUAR_FL_TOWER_HAZARD_RADIUS = 18.0f;

// What the strike itself actually covers, which is smaller than the band above: Hodir's Fury 10 yd
// (62297), Mimiron's Inferno 9 (62910), Thorim's Hammer 7 (62912). The scan radius is the warning;
// this is the circle a vehicle has to be out of.
constexpr float ULDUAR_FL_TOWER_BLAST_RADIUS = 10.0f;

// Flame Leviathan arena corners, taken from the four NPC_FREYA_WARD_TARGET spawn points in
// boss_flame_leviathan.cpp's SummonTowerHelpers. The kite ring and every "is this inside the
// arena" test are derived from these, so nothing else hardcodes arena geometry.
extern std::vector<Position> const ULDUAR_FL_ARENA_CORNERS;

// Where each vehicle parks relative to the boss. Measured surface-to-surface (added to his combat
// reach), because he has a large model and a raw centre distance would put melee inside him.
// Ram's 18 yd cone is measured with the boss's 15 yd combat reach added, so 8 here is comfortably
// inside it. Distance was never what made Ram miss - facing was.
constexpr float ULDUAR_FL_SIEGE_STAND_DIST = 8.0f;

// Sonic Horn is a 35 yd cone and his reach adds another 15, so a chopper has no reason to sit on top
// of him - 6 yd put it 21 yd from his centre, permanently inside Battering Ram's 25. Out here it
// keeps every shot and is only in the blast while it chooses to be, i.e. on the tar lead.
constexpr float ULDUAR_FL_CHOPPER_STAND_DIST = 20.0f;
constexpr float ULDUAR_FL_DEMOLISHER_BAND = 50.0f;    // inside the 10-70 yd hurl band, outside Battering Ram
constexpr float ULDUAR_FL_TAR_LEAD_DIST = 30.0f;      // how far ahead of him the lead chopper parks

// Re-facing costs a spline, so only correct a facing that has really drifted. Roughly 6 degrees.
constexpr float ULDUAR_FL_FACING_TOLERANCE = 0.1f;

// A loose arrival deadband on purpose: it is what stops a group of vehicles piling onto one
// coordinate, and a tight one makes a bot slide in place instead of ever holding still.
constexpr float ULDUAR_FL_ARRIVE_TOLERANCE = 8.0f;

// Only re-issue a MoveTo once the goal has drifted this far. Re-stamping the same destination
// restarts the spline every tick and the vehicle crawls instead of arriving.
constexpr float ULDUAR_FL_REPOSITION_EPSILON = 6.0f;

// Energy costs the strategy gates on. Only the ones used as thresholds are listed; the rest of the
// spellbook is free. Siege engines, turrets and choppers regenerate 20 per 2s against a cap of 100.
constexpr uint32 ULDUAR_FL_ELECTROSHOCK_COST = 20;
constexpr uint32 ULDUAR_FL_RAM_COST = 40;
constexpr uint32 ULDUAR_FL_STEAM_RUSH_COST = 40;
constexpr uint32 ULDUAR_FL_SONIC_HORN_COST = 20;
constexpr uint32 ULDUAR_FL_FIRE_CANNON_COST = 20;
constexpr uint32 ULDUAR_FL_SPEED_BOOST_COST = 50;
constexpr uint32 ULDUAR_FL_INCREASED_SPEED_COST = 25;
constexpr uint32 ULDUAR_FL_PYRITE_BARREL_COST = 5;

// Ram, Electroshock and Sonic Horn are TARGET_UNIT_CONE_ENEMY_104: a frontal cone whose reach is
// the effect radius, not the spell range. Spell::CheckRange short-circuits on RangeEntry ID 1 and
// waves them all through, so these radii are the real limit and nothing else enforces them.
constexpr float ULDUAR_FL_ELECTROSHOCK_CONE_RADIUS = 25.0f;
constexpr float ULDUAR_FL_RAM_CONE_RADIUS = 18.0f;
constexpr float ULDUAR_FL_SONIC_HORN_CONE_RADIUS = 35.0f;

// Each cone also has its own width, from world.spell_cone; Spell::SelectImplicitConeTargets falls
// back to 60 degrees for a spell with no row. These are half-widths already, because HasInArc
// splits the arc it is handed. Nothing widens them at cast time: isInFront passes no target radius,
// so the boss's 15 yd combat reach buys no slack, and CAST_ANGLE_IN_FRONT (120 degrees) is wider
// than all three - which means CastVehicleSpell will not turn the vehicle for these on its own.
// A siege engine 45 degrees off him passes that gate, casts, and Electroshock's 30 degrees finds
// nothing: 71 casts landed once over five pulls.
constexpr float ULDUAR_FL_ELECTROSHOCK_CONE_HALF_ANGLE = 30.0f * float(M_PI) / 180.0f;
constexpr float ULDUAR_FL_RAM_CONE_HALF_ANGLE = 50.0f * float(M_PI) / 180.0f;
constexpr float ULDUAR_FL_SONIC_HORN_CONE_HALF_ANGLE = 25.0f * float(M_PI) / 180.0f;

// Cooldowns the module keeps itself, because Spell::SendSpellCooldown returns early for a creature
// caster and nothing enforces Electroshock's 10 s otherwise. The short one is for a shot that went
// out and did not stop the channel: retry, but not at tick rate - each attempt costs 20 energy.
constexpr uint32 ULDUAR_FL_ELECTROSHOCK_COOLDOWN_MS = 10000;
constexpr uint32 ULDUAR_FL_ELECTROSHOCK_RETRY_MS = 3000;

// Battering Ram is TARGET_DEST_TARGET_ENEMY at radius index 20: a 25 yd sphere centred on whoever he
// is pursuing, not a cone off his front. Distance to him tells you nothing; distance to the Pursued
// vehicle is the whole answer.
constexpr float ULDUAR_FL_BATTERING_RAM_RADIUS = 25.0f;

// He only casts it inside IsWithinCombatRange(victim, 15.0f), which adds both combat reaches on top.
constexpr float ULDUAR_FL_BATTERING_RAM_CAST_RANGE = 15.0f;

// Hurl Boulder is a lobbed shot with a real minimum range (RangeIndex 164), so a demolisher parked
// on top of a frozen ally cannot thaw it and has to back off first. Mortar (RangeIndex 37) has no
// minimum but only reaches 50.
constexpr float ULDUAR_FL_HURL_BOULDER_MIN_RANGE = 10.0f;
constexpr float ULDUAR_FL_HURL_BOULDER_MAX_RANGE = 70.0f;
constexpr float ULDUAR_FL_MORTAR_MAX_RANGE = 50.0f;

// Gap to hold between vehicles of one class: Hodir's Fury's 10 yd blast plus enough that a vehicle
// drifting inside its arrival deadband does not close it. Half the fight ran with four or more
// vehicles inside one such circle, so a single reticle could freeze a whole class for 60 s.
constexpr float ULDUAR_FL_STATION_SPACING = 12.0f;

// The widest the fan may open. Past this the outer slots stop being "behind him" at all.
constexpr float ULDUAR_FL_STATION_MAX_ARC = 2.0f * float(M_PI) / 3.0f;

// A pyrite crate energizes for 25, so grabbing one above this wastes part of it.
constexpr uint32 ULDUAR_FL_CRATE_GRAB_CEILING = 75;

// Demolishers do not regenerate energy (no UNIT_FLAG2_REGENERATE_POWER), so a full tank is 20
// barrels and pyrite crates are the only refill. Barrel above the reserve, boulder below it; the
// reserve is what keeps Increased Speed (25) affordable when Pursued lands.
constexpr uint32 ULDUAR_FL_PYRITE_RESERVE = 30;
constexpr float ULDUAR_FL_CRATE_DETOUR_RADIUS = 60.0f;  // how far a starved demolisher leaves its band

// Wall-hugging kite ring. The corners are inset off the walls so MoveTo has mesh to land on, and
// each 90-degree corner is chamfered into two nodes so a kiting vehicle rounds it instead of
// driving into it while the boss cuts the diagonal.
constexpr float ULDUAR_FL_KITE_WALL_INSET = 15.0f;
constexpr float ULDUAR_FL_KITE_CORNER_CHAMFER = 35.0f;
constexpr float ULDUAR_FL_KITE_ADVANCE_DIST = 30.0f;    // switch nodes on approach, never on arrival
constexpr float ULDUAR_FL_KITE_BOSS_CLEARANCE = 50.0f;  // a node this close to him is not a destination

// Flame Leviathan. The whole fight is from vehicles, and a trigger and its action must call the
// same helper here or the two derivations disagree about who is doing what.

// Resolved by entry, never through "find target" or "attackers": threat on this fight belongs to
// the vehicle creature rather than the bot player, so the boss is usually absent from both.
Unit* FlameLeviathanBoss(PlayerbotAI* botAI);

// Everything else stays inert until this is true, so the raid can still drive into the arena and
// pull normally instead of being dragged at the boss the moment it seats.
bool FlameLeviathanEngaged(PlayerbotAI* botAI);

// The vehicle a bot is actually riding into battle. A gunner sits in seat 0 of a turret creature
// that is itself bolted into the siege engine / demolisher, so this walks one link up for them.
Unit* FlameLeviathanRiddenVehicle(Player* bot);

// Seat 0 of the turret and mechanic seat both carry CAN_CONTROL, so IsInVehicle(true) is true for
// a gunner and MoveTo would happily steer the bolted-on turret model. Gate movement on this.
bool FlameLeviathanIsDriver(Player* bot);

// Pursued lands on whichever unit the boss's spell picked, which may be the ridden vehicle or the
// parent underneath it, so both links are checked.
bool FlameLeviathanIsPursued(Player* bot);

bool FlameLeviathanIsVentChanneling(Unit* boss);

// True for exactly one siege engine per channel: highest energy, guid breaking ties, and then the
// channel is claimed so nobody else fires into it. Ranking alone is not enough - casting spends 20
// energy, which promotes the next engine in the same tick, and four of them emptied 80 energy into
// one channel 30 ms apart. The Ram energy reserve reads this too, so the fuel travels with the duty.
bool FlameLeviathanIsVentInterrupter(PlayerbotAI* botAI, Player* bot);

// Claims the channel now being cast at, so the ranking above stops handing it to the next engine.
// Called once the interrupt has actually gone out.
void FlameLeviathanClaimVentChannel(Player* bot);

// A cone spell lands nothing unless the vehicle is genuinely pointed at him, and CastVehicleSpell
// only turns for targets outside 120 degrees - wider than any of these cones. Returns true when the
// shot is on; otherwise starts the turn and leaves the cast for a later tick.
bool FlameLeviathanFaceForCone(Unit* vehicleBase, Unit* target, float halfAngle, float radius);

// Alive, mounted, and neither the hull nor the driver frozen. Any role handed out by election has to
// ask this, or a 60 s Hodir's Fury stun takes the role with the vehicle.
bool FlameLeviathanCrewUsable(Player* member);

// Nearest crewed vehicle held by Hodir's Fury's stun, inside the casting band of whatever will thaw
// it. `from` is excluded, so a driver never aims at the hull it is sitting in - it could not cast
// anyway. A gunner passes its turret instead, which deliberately leaves its own demolisher eligible:
// the seat is not stunned when only the hull is, and Mortar has no minimum range.
Unit* FlameLeviathanFrozenVehicle(Player* bot, Unit* from, float minRange, float maxRange);

// The vehicle currently wearing Pursued, or null. Everything Battering Ram is measured against.
Unit* FlameLeviathanPursuedVehicle(PlayerbotAI* botAI, Player* bot);

// True while this vehicle is inside the 25 yd sphere Battering Ram drops on the Pursued vehicle.
bool FlameLeviathanInBatteringRamBlast(Unit* vehicleBase, Unit* pursued);

// The whole "get out of the blast" test in one place, because the urgent-drive trigger and the drive
// action both ask it and a trigger that fires wider than its action just demotes the cast node for
// nothing. True when this vehicle shares the blast with the Pursued one and he is close enough to
// fire. No switch prediction: the rule re-aims itself the moment the Pursued guid changes.
bool FlameLeviathanShouldClearBatteringRam(PlayerbotAI* botAI, Player* bot);

// Lowest-guid chopper driver that is neither Pursued nor frozen. It runs ahead of the boss dropping
// tar in his path; a Pursued chopper is already driving away from him and drops tar for free.
bool FlameLeviathanIsTarLead(PlayerbotAI* botAI, Player* bot);

bool FlameLeviathanInArena(Position const& pos, float margin = 0.0f);

Position FlameLeviathanRearPoint(Unit* boss, float standDist, float bearingOffset = 0.0f);

// Where this vehicle sits in its class's fan, as an angle off the class bearing. One station point
// per class stacked the whole class on one spot, and a single Hodir's Fury then froze all of it.
float FlameLeviathanStationBearingOffset(Player* bot, Unit* vehicleBase, float radius);
Position FlameLeviathanLeadPoint(Unit* boss, float standDist);

// How far ahead of him the lead chopper can sit without parking inside Battering Ram's sphere.
// Zero when there is no room at all, which means the slot has to be given up for now.
float FlameLeviathanTarLeadDistance(Unit* boss, Unit* pursued, float size);

// Eight nodes hugging the arena walls, corners chamfered. Built once from ULDUAR_FL_ARENA_CORNERS.
std::vector<Position> const& FlameLeviathanKiteRing();

#endif
