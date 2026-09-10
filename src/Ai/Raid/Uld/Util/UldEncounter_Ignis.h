/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERIGNIS_H
#define PLAYERBOTS_ULDENCOUNTERIGNIS_H

#include "Position.h"
#include "UldData.h"

class Player;
class PlayerbotAI;
class Unit;
class WorldObject;

// Ignis the Furnace Master.
//
// The fight is about Iron Constructs. Ignis activates them, Flame Jets makes them Molten, and a
// Molten construct dragged within 18 yd of one of the room's two water pools turns Brittle - at
// which point a melee hit shatters it. Letting them live instead stacks Strength of the Creator on
// the boss, so the dragging is the damage race.
//
// Scorched Ground burns where Flame Jets landed and does not despawn, so the tank walks a fixed arc
// rather than standing still, and Slag Pot picks one player to cook for ten seconds.

enum UlduarIgnisIds
{
    // Ignis the Furnace Master
    NPC_IGNIS = 33118,
    NPC_IGNIS_IRON_CONSTRUCT = 33121,
    NPC_IGNIS_SCORCHED_GROUND = 33123,
    // Dormant constructs wear this alongside UNIT_FLAG_NOT_SELECTABLE; Activate Construct strips it.
    SPELL_IGNIS_CONSTRUCT_INACTIVE = 38757,
    // Self-buff on the boss for the 3 s he is rooted and rotation-locked; the patch spawns when it
    // falls off, from the orientation frozen at cast start.
    SPELL_IGNIS_SCORCH = 62546,
    SPELL_IGNIS_SCORCH_25 = 63474,
    SPELL_IGNIS_FLAME_JETS = 62680,
    SPELL_IGNIS_FLAME_JETS_25 = 63472,
    SPELL_IGNIS_MOLTEN = 62373,
    SPELL_IGNIS_BRITTLE_10 = 62382,
    SPELL_IGNIS_BRITTLE_25 = 67114,
    SPELL_IGNIS_SLAG_POT_10 = 62717,
    SPELL_IGNIS_SLAG_POT_25 = 63477,
    SPELL_IGNIS_STRENGTH_OF_THE_CREATOR = 64473,
};

// Ignis: a Molten construct turns Brittle once it is this close to one of the room's two water
// triggers (boss_ignis.cpp polls FindNearestCreature(NPC_WATER_TRIGGER, 18.0f) once a second).
constexpr float ULDUAR_IGNIS_WATER_BRITTLE_RADIUS = 18.0f;

// Everyone but the construct tank clears Scorch's burning patch by this much; the tank parks the
// tighter distance instead, so the construct walking into melee range ends up on it stacking Heat.
// 62548 triggers its damage in a 13 yd radius, so anything under that is not actually a dodge.
constexpr float ULDUAR_IGNIS_SCORCHED_GROUND_AVOID_RADIUS = 15.0f;
constexpr float ULDUAR_IGNIS_SCORCHED_GROUND_PARK_DISTANCE = 3.0f;

// A Scorched Ground creature that lands this close to a water trigger never gets lit
// (boss_ignis.cpp skips SPELL_SCORCHED_GROUND within 25 yd of the water), so it stacks no Heat and
// the tank must not park a construct on it.
constexpr float ULDUAR_IGNIS_SCORCHED_GROUND_INERT_WATER_RADIUS = 25.0f;

// Molten wipes the construct's threat table and adds a heavy fire aura, so everyone who is not the
// construct tank clears this much room. Sized against Shatter (62383, 18850 damage in 13 yd) rather
// than the Molten pulse itself, which only reaches ~7 yd - the blast is what actually kills a bot.
constexpr float ULDUAR_IGNIS_MOLTEN_AVOID_RADIUS = 15.0f;

// The construct spawns run x 543..631 / y 217..338 and Ignis starts at (586.5, 378.8), so a tank
// standing at a water pool is already ~120 yd from the boss and further still from the far wall.
// Everything Ignis-side searches the grid at this radius rather than going through "nearest npcs",
// which is capped at AiPlayerbot.SightDistance (100 yd) and drops anything out of line of sight.
constexpr float ULDUAR_IGNIS_ROOM_SEARCH_RADIUS = 200.0f;

// Scorch summons its patch at boss + 20 yd along the boss's facing, and the boss faces the main
// tank, so where the tank stands is what picks the landing spot for every patch in the fight.
constexpr float ULDUAR_IGNIS_SCORCH_SPAWN_RANGE = 20.0f;

// Ignis' combat reach is 8.0, which puts melee range at roughly 10.8 yd centre to centre - the tank
// radius is that with a little slack so a step off the spot does not drop him out of range.
constexpr float ULDUAR_IGNIS_TANK_RADIUS = 9.5f;
constexpr float ULDUAR_IGNIS_TANK_SPOT_TOLERANCE = 2.0f;

// The tank works three slots 60 degrees apart, advancing one every Scorch, so the patches land in a
// fan instead of on top of each other. A 60 degree step at this radius leaves him 17.3 yd from the
// patch he just dropped and 26.8 yd from the one before - both outside the 13 yd burn.
constexpr uint8 ULDUAR_IGNIS_TANK_ARC_SLOTS = 3;
constexpr float ULDUAR_IGNIS_TANK_ARC_STEP = static_cast<float>(M_PI) / 3.0f;

// Bearing from the anchor to the tank, and therefore the direction the patch fan points: south.
// Patches south, water pools east and west, raid north - the three cannot collide.
constexpr float ULDUAR_IGNIS_TANK_BEARING = -static_cast<float>(M_PI) / 2.0f;

// Melee are admitted to a Brittle construct only once its aura has less than this left, as a
// fallback for a raid with nobody ranged in position. Brittle itself runs 15 s.
constexpr uint32 ULDUAR_IGNIS_BRITTLE_MELEE_FALLBACK_MS = 7000;

// Slag Pot ticks for ten seconds and cannot be dispelled or moved out of, but it does not kill from
// full, so healers only pile onto the victim once the ticks have actually opened a gap.
constexpr float ULDUAR_IGNIS_SLAG_POT_HEAL_HP_PCT = 85.0f;

extern const Position ULDUAR_IGNIS_BOSS_ANCHOR;
extern const Position ULDUAR_IGNIS_WATER_POOL_WEST;
extern const Position ULDUAR_IGNIS_WATER_POOL_EAST;

// Ignis the Furnace Master. These search the grid rather than going through "find target": a bot
// parked on an Iron Construct never has Ignis on its threat list, and a dormant construct carries
// UNIT_FLAG_NOT_SELECTABLE, which drops it out of "possible targets" entirely. The room is also
// wider than SightDistance, so the cached "nearest npcs" list goes blind at the water pools.
Unit* GetIgnis(PlayerbotAI* botAI);

// Alive and actually fighting. Every Ignis node hangs off this: the room is 200 yd wide and the boss
// is visible from the whole of it, so proximity alone has bots dodging and kiting on the way in.
bool IsIgnisEngaged(PlayerbotAI* botAI);

// Activated = Ignis has cast Activate Construct on it: selectable, aggressive, and worth tanking.
bool IsIgnisConstructActivated(Unit const* construct);

// 10 Heat stacks from standing in Scorched Ground. Molten also resets the construct's threat.
bool IsIgnisConstructMolten(Unit const* construct);

// A Molten construct brought to the water. One hit of 5000 (10-man) / 3000 (25-man) shatters it.
bool IsIgnisConstructBrittle(Unit const* construct);

// Lowest GUID of the Brittle constructs, not the nearest one: every bot has to converge on the same
// construct without anything shared to agree through, and one hit ends it.
Unit* GetIgnisBrittleConstruct(PlayerbotAI* botAI);
Unit* GetIgnisNearestMoltenConstruct(PlayerbotAI* botAI, WorldObject const* from);

// The construct the tank is currently walking through the loop: nearest activated one that has not
// turned Brittle yet. Once it is Brittle the tank is done and the raid takes over.
//
// Sticky per tank: a construct Ignis activates closer to the tank must not steal the walk, because
// the one already picked up has had its threat wiped by Molten and would peel straight into the raid.
Unit* GetIgnisDrivenConstruct(PlayerbotAI* botAI, Player* tank);

// Scorched Ground the construct tank can actually use, i.e. one that is lit. Patches that land
// within ULDUAR_IGNIS_SCORCHED_GROUND_INERT_WATER_RADIUS of the water are skipped by the core and
// stack no Heat, so parking on one would stall the Heat -> Molten chain for good.
Unit* GetIgnisNearestScorchedGround(PlayerbotAI* botAI, WorldObject const* from);
Position const& GetIgnisNearestWaterPool(WorldObject const* from);

// The lit patch nearest this tank's own pool, so the two construct tanks work opposite sides of the
// fan instead of both walking to whichever patch happens to be closest.
Unit* GetIgnisAssignedScorchedGround(PlayerbotAI* botAI, WorldObject const* from, int8 tankIndex);

// 0 or 1 for the two assist tanks that kite constructs, -1 for everyone else. Assist tanks only, on
// purpose: a raid without one skips the kiting path entirely rather than pulling the boss around
// behind a construct or feeding a DPS to a Molten one.
int8 GetIgnisConstructTankIndex(PlayerbotAI* botAI, Player* bot);

// Fixed by tank index, not by distance. The anchor sits almost exactly between the two pools, so
// nearest-pool is a coin flip that always lands the same way and would stack both Molten pulses and
// both Shatters in one spot.
Position const& GetIgnisAssignedWaterPool(int8 tankIndex);

// Where the main tank stands: an arc slot around the room-centre anchor, advanced one step on each
// Scorch. Ignis is rooted for those 3 s, so the move costs nothing and does not disturb the patch.
Position GetIgnisMainTankPosition(PlayerbotAI* botAI, Player* bot);

// The 3 s window in which Ignis is rooted and rotation-locked. The patch lands when it ends.
bool IsIgnisScorchWindow(Unit* boss);

// Flame Jets is a 2.7 s observable cast, unlike most of what this raid throws, so bots can see it
// coming and stop feeding casts into the knockback lockout that follows.
bool IsIgnisFlameJetsCasting(Unit* boss);

Player* GetIgnisSlagPotVictim(PlayerbotAI* botAI);
bool IsIgnisSlagPotVictim(Player* bot);

#endif
