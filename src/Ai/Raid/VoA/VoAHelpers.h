/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_VOAHELPERS_H
#define PLAYERBOTS_VOAHELPERS_H

#include "PlayerbotAI.h"
#include "Position.h"

#include <vector>

class Player;
class Unit;
class WorldObject;

enum VoAIDs
{
    // Emalon the Storm Watcher. Both difficulty entries are listed because the searches below match
    // on entry, not on name.
    NPC_EMALON = 33993,
    NPC_EMALON_HEROIC = 33994,
    NPC_TEMPEST_MINION = 33998,
    NPC_TEMPEST_MINION_HEROIC = 34200,
    AURA_OVERCHARGE = 64217,
    SPELL_LIGHTNING_NOVA_10_MAN = 64216,
    SPELL_LIGHTNING_NOVA_25_MAN = 65279,

    // Archavon the Stone Watcher
    SPELL_ROCK_SHARDS = 58678,

    // Koralon the Flame Watcher
    SPELL_BURNING_BREATH = 66665,

    // Toravon the Ice Watcher
    NPC_FROZEN_ORB = 38456,
    SPELL_FREEZING_GROUND = 72090,
};

namespace VoaHelpers
{

constexpr uint32 VOA_MAP_ID = 624;

// Emalon's chamber was swept off the map-624 navmesh (tile 6243232.mmtile, where every poly is
// NAV_GROUND). The floor sits at z 91.5-92.8 and is a hexagon: a wide waist at Y -292..-286 tapering
// to about 45yd at both ends. Beyond Y -318 it splits into two alcoves with a wall between them, and
// past Y -256 it climbs into the entrance ramp at z 96.8.
constexpr float CHAMBER_CENTER_X = -218.75f;
constexpr float CHAMBER_MIN_Y = -316.0f;
constexpr float CHAMBER_MAX_Y = -260.0f;

// Emalon's CombatReach is 7.5, so GetMeleeRange against a player is 7.5 + 1.5 + 4/3 = 10.33yd - he
// stops chasing that far short of whoever holds him. Standing here therefore parks him at Y -300,
// which is what puts the ring's flanks on the 80yd waist instead of against the tapered ends.
constexpr float MAIN_TANK_ANCHOR_Y = -310.3f;
constexpr float MAIN_TANK_ARRIVAL_TOLERANCE = 2.0f;

// Both ring radii are pinned between two hard walls: outside the 20yd Lightning Nova, and inside the
// 38.5yd heal range to a main tank standing 10.33yd beyond the boss, which caps them at 28.2yd. That
// leaves 22-28yd for both rings, and is the reason healers sit *inside* ranged here.
constexpr float HEALER_RING_RADIUS = 23.0f;
constexpr float RANGED_RING_RADIUS = 27.0f;
// Swept against the navmesh: at 26yd the full +-90 degrees is floor and at 30yd it is +-80, so both
// arcs clear their whole span with margin.
constexpr float HEALER_RING_ARC_DEGREES = 120.0f;
constexpr float RANGED_RING_ARC_DEGREES = 150.0f;
// A recomputed slot closer than this to the one already walked to leaves the bot alone. Without it
// the whole ring trails the boss yard for yard while the pull drag is still running.
constexpr float RING_ANCHOR_DRIFT = 8.0f;
constexpr float RING_ARRIVAL_TOLERANCE = 4.0f;

// The waist's +X point: 33.9yd from the settled boss, so the off-tank never eats the nova, and
// 18-19yd from the two near minion spawn corners, inside the 30yd range every taunt in the game has.
extern Position const OFFTANK_CAMP;
constexpr float OFFTANK_ARRIVAL_TOLERANCE = 3.0f;
// Past this the off-tank walks at a minion instead of taunting it from the camp. The two far spawn
// corners are 47yd out, which no taunt reaches.
constexpr float MINION_TAUNT_RANGE = 28.0f;

// Lightning Nova is a hard 20yd on 10-man. 25-man has no radius worth escaping (100yd) but scales the
// damage by (70 - dist)/70, so the same step out is worth taking in both modes.
constexpr float LIGHTNING_NOVA_CLEAR_RADIUS = 22.0f;

// Nothing in this fight is further away than the room itself.
constexpr float ROOM_SEARCH_RADIUS = 200.0f;

// Deliberately looser than the destination clamp: this one answers "is that unit in Emalon's
// chamber", so it has to keep out Toravon's room at X -43 and the approach corridor's Tempest
// Warders at Y -229 and -196 while still counting a minion standing on the rim.
constexpr float ROOM_MIN_X = -265.0f;
constexpr float ROOM_MAX_X = -175.0f;
constexpr float ROOM_MIN_Y = -330.0f;
constexpr float ROOM_MAX_Y = -245.0f;
constexpr float ROOM_MAX_Z = 105.0f;

// Resolved by entry, never through AI_VALUE2("find target", ...): boss_emalon calls DoZoneInCombat
// for its summons but never SetInCombatWithZone for itself, so every bot that does not attack him -
// the off-tank and the whole ranged ring - is absent from his threat list and cannot find him by name.
Unit* GetEmalon(Player* bot);
bool EmalonEncounterActive(Player* bot);

bool IsTempestMinion(Unit const* unit);
std::vector<Unit*> LivingMinions(Player* bot);
Unit* OverchargedMinion(Player* bot);
// What the off-tank should be holding right now: the overcharged minion first, then anything not
// already beating on him, then the nearest.
Unit* MinionToPickUp(Player* bot);

bool InsideChamber(WorldObject const* object);

Player* GetOffTank(PlayerbotAI* botAI, Player* bot);
bool IsOffTank(Player* bot);
// Logs one warning per raid that has no assist tank, then returns false so every off-tank behaviour
// stays inert. A half-working single-tank fallback is harder to diagnose than a clean bail.
bool RequireOffTank(PlayerbotAI* botAI, Player* bot);
Player* RedirectTarget(PlayerbotAI* botAI, Player* bot);

// Every MoveTo destination in this strategy goes through here.
void ClampToChamber(float& x, float& y);

// Ring slot for one bot, already clamped. False for anyone who does not hold one - tanks and melee.
bool RingSlotFor(PlayerbotAI* botAI, Player* bot, Unit* boss, float& x, float& y);

}

#endif
