/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ONYDATA_H
#define PLAYERBOTS_ONYDATA_H

#include <array>

#include "Define.h"
#include "Position.h"

// Ids, coordinates and tuning constants for Onyxia. No behaviour lives here.
//
// Coordinates: +X is north, +Y is west, the entrance tunnel is at the south end.
namespace OnyHelpers
{
constexpr uint32 ONYXIA_MAP_ID = 249;

// DATA_ONYXIA, the instance script's only boss index.
constexpr uint32 ONYXIA_BOSS_ONYXIA = 0;

// The 25-man creatures keep these entries, so they match both sizes.
namespace NpcId
{
constexpr uint32 Onyxia = 10184;
constexpr uint32 Whelp = 11262;
constexpr uint32 LairGuard = 36561;
}  // namespace NpcId

namespace GoId
{
constexpr uint32 Egg = 176511;
}

// 10-man id, then the 25-man one where the spell has a difficulty variant.
namespace SpellId
{
constexpr uint32 Fireball = 18392;
constexpr uint32 FireballH = 68926;
constexpr uint32 FlameBreath = 18435;
constexpr uint32 FlameBreathH = 68970;
constexpr uint32 WingBuffet = 18500;
constexpr uint32 WingBuffetH = 69293;
constexpr uint32 TailSweep = 68867;
constexpr uint32 TailSweepH = 69286;
constexpr uint32 Eruption = 17731;
constexpr uint32 EruptionH = 69294;
constexpr uint32 IgniteWeapon = 68959;
constexpr uint32 IgniteWeaponH = 68964;
constexpr uint32 Cleave = 68868;
constexpr uint32 BellowingRoar = 18431;
constexpr uint32 BlastNova = 68958;
// The egg's own trap, which hatches it. Only used to label the egg hazards in a trace.
constexpr uint32 EggTrap = 17647;
// Tricks of the Trade on the rogue, waiting and running: what he generates lands on the tank, so he
// is free to hit her.
constexpr uint32 TricksOfTheTrade = 57934;
constexpr uint32 TricksOfTheTradeActive = 59628;
}  // namespace SpellId

// One Deep Breath per phase 2 waypoint. `hover` is where she casts it from, `from`/`to` the first and
// last hop of the fire it walks across the floor. Indexed by waypoint id minus one.
struct BreathLane
{
    uint8 waypoint;
    uint32 spellId;
    Position hover;
    Position from;
    Position to;
};

inline std::array<BreathLane, 8> const BREATH_LANES = {{
    {1, 18351, {-64.496f, -214.906f, -60.0f}, {-68.8f, -215.0f, -86.0f}, {20.7f, -215.2f, -86.0f}},
    {2, 18596, {-59.809f, -190.758f, -60.0f}, {-58.3f, -189.0f, -86.0f}, {12.1f, -243.4f, -86.0f}},
    {3, 18609, {-29.450f, -180.600f, -60.0f}, {-37.7f, -188.6f, -86.0f}, {-37.7f, -243.7f, -86.0f}},
    {4, 18584, {6.895f, -180.246f, -60.0f}, {6.0f, -181.3f, -86.0f}, {-56.6f, -241.2f, -86.0f}},
    {5, 17086, {22.876f, -217.152f, -60.0f}, {20.7f, -215.2f, -86.0f}, {-130.8f, -213.4f, -86.0f}},
    {6, 18617, {10.2191f, -247.912f, -60.0f}, {12.1f, -243.4f, -86.0f}, {-58.3f, -189.0f, -86.0f}},
    {7, 18576, {-31.496f, -250.123f, -60.0f}, {-37.7f, -243.7f, -86.0f}, {-37.7f, -188.6f, -86.0f}},
    {8, 18564, {-63.5156f, -240.096f, -60.0f}, {-56.6f, -241.2f, -86.0f}, {6.0f, -181.3f, -86.0f}},
}};

// Where she takes off from and flies to first in phase 2.
constexpr uint8 ONYXIA_FIRST_HOVER_WAYPOINT = 5;

// Every Lava Fissure. Eruption bursts round all of them on each Bellowing Roar in phase 3.
inline std::array<Position, 52> const LAVA_FISSURES = {{
    {24.23f, -235.51f},  {14.42f, -233.45f},  {17.39f, -245.04f},  {16.84f, -225.11f},  {-28.1f, -190.67f},
    {-26.4f, -199.62f},  {8.24f, -253.58f},   {7.07f, -243.99f},   {-3.98f, -248.95f},  {-0.64f, -239.84f},
    {-15.39f, -232.51f}, {-12.89f, -245.33f}, {-20.98f, -254.7f},  {3.2f, -230.38f},    {-37.41f, -244.92f},
    {-26.09f, -229.62f}, {-28.12f, -238.21f}, {29.19f, -206.77f},  {31.17f, -218.99f},  {13.19f, -214.44f},
    {-37.38f, -184.48f}, {-1.67f, -217.14f},  {-9.81f, -212.03f},  {-19.4f, -219.85f},  {-19.7f, -209.64f},
    {24.05f, -193.9f},   {14.3f, -195.44f},   {17.0f, -204.31f},   {17.64f, -184.03f},  {2.95f, -198.74f},
    {-0.4f, -189.26f},   {6.95f, -185.41f},   {8.01f, -175.79f},   {-3.72f, -180.01f},  {-12.91f, -183.54f},
    {-21.13f, -174.71f}, {-15.67f, -196.47f}, {-47.95f, -193.89f}, {-54.97f, -199.12f}, {-64.16f, -196.12f},
    {-54.8f, -187.0f},   {-62.88f, -213.24f}, {-51.66f, -214.44f}, {-45.12f, -220.16f}, {-39.11f, -212.13f},
    {-78.55f, -210.42f}, {-72.94f, -201.88f}, {-75.14f, -224.65f}, {-54.69f, -229.82f}, {-48.09f, -234.86f},
    {-55.08f, -241.78f}, {-63.86f, -232.92f},
}};

// The main tank stands here with his back to the north wall, which stops Wing Buffet's knockback.
// navprobe: on the floor, PATHFIND_NORMAL from her home and from the phase 3 landing, 7.1 and 7.5yd
// from the two nearest fissures.
Position const ONYXIA_TANK_SPOT(34.0f, -212.5f, -83.46f);
// Where she settles in melee range of the tank spot, and what the raid lays out around while she is
// still on her way. Median of six holds with the tank on his spot; navprobe settles it at this z.
Position const ONYXIA_BOSS_HOLD_SPOT(18.9f, -213.5f, -85.42f);
// The main tank waits here while she comes down, in taunt range of the landing and clear of fissures
// and eggs.
Position const ONYXIA_P3_PICKUP(-32.0f, -205.0f, -87.84f);
Position const ONYXIA_LAND_SPOT(-63.496f, -214.906f, -84.4f);
Position const ONYXIA_GUARD_SPAWN(-101.654f, -214.491f, -80.70f);
// A Lair Guard she summons on the pull. It idles in the tunnel unless someone attacks it.
Position const ONYXIA_TUNNEL_GUARD(-167.84f, -200.55f, -66.34f);
Position const ONYXIA_CAVE_WEST(-32.535f, -170.19f, -89.0f);
Position const ONYXIA_CAVE_EAST(-33.18f, -258.80f, -89.0f);

constexpr float ONYXIA_PHASE_3_HEALTH_PCT = 40.0f;
// She hovers at z -60 in phase 2 and walks at -84 to -88, so anything above this is in the air.
constexpr float ONYXIA_AIR_Z = -75.0f;
constexpr float ONYXIA_COMBAT_REACH = 18.0f;
// Lair Guards west of this are the passive tunnel one.
constexpr float ONYXIA_TUNNEL_GUARD_MAX_X = -130.0f;

// An egg hatches on a player within about 6.9yd in 3D: its 5yd trap plus the egg and the player.
constexpr float ONYXIA_EGG_DANGER = 8.0f;
constexpr float ONYXIA_EGG_CLEARANCE = 9.5f;
// Whelps this close to an egg are left to walk out: chasing one in hatches more.
constexpr float ONYXIA_WHELP_EGG_EXCLUSION = 12.0f;
constexpr float ONYXIA_EGG_SEARCH_RADIUS = 200.0f;

constexpr float ONYXIA_BREATH_TRIGGER = 20.0f;
constexpr float ONYXIA_BREATH_CLEARANCE = 22.0f;
// From the cast starting to the last hop landing: 8.25s scripted, plus the hops' travel.
constexpr uint32 ONYXIA_BREATH_LIVE_MS = 9500;

// Fireball splashes 8yd round its target.
constexpr float ONYXIA_FIREBALL_SPACING = 9.5f;
// Blast Nova is 15yd round the guard, 5s cast. Non-tanks leave in the last 2s, after the interrupts.
constexpr float ONYXIA_BLAST_NOVA_CLEARANCE = 17.0f;
constexpr uint32 ONYXIA_BLAST_NOVA_LEAVE_MS = 2000;
// Eruption is 5yd round each fissure.
constexpr float ONYXIA_FISSURE_CLEARANCE = 6.9f;
constexpr float ONYXIA_DODGE_MAX_RADIUS = 45.0f;

// Main tank.
constexpr float ONYXIA_TANK_SPOT_TOLERANCE = 3.0f;
constexpr uint32 ONYXIA_TANK_WALK_REISSUE_MS = 500;
// He tanks her where he picked her up until he has held her this long with this lead over the next
// highest threat, then walks. A human tank out-threating him can't pin him past the max.
constexpr uint32 ONYXIA_SECURE_DWELL_MS = 5000;
constexpr float ONYXIA_SECURE_LEAD = 1.25f;
constexpr uint32 ONYXIA_SECURE_MAX_MS = 10000;

// Threat: everyone but the main tank and the healers waits this long after he takes her, then stays
// under the hold ratio of his threat until it drops to the release ratio.
constexpr uint32 ONYXIA_THREAT_DWELL_MS = 3000;
constexpr float ONYXIA_THREAT_HOLD_RATIO = 0.90f;
constexpr float ONYXIA_THREAT_RELEASE_RATIO = 0.80f;

// Burst waits until the main tank has held her this long. Bloodlust waits for phase 3, and with a
// human main tank, whose taunt the bots cannot see coming, for this long after she touches down.
constexpr uint32 ONYXIA_BURST_DWELL_MS = 5000;
constexpr uint32 ONYXIA_HUMAN_TANK_LUST_MS = 15000;
constexpr uint32 ONYXIA_REDIRECT_WINDOW_MS = 10000;

// Ground layout: ranged and healers on the flanks at these bearings off her facing, both sides.
constexpr float ONYXIA_SLOT_BEARING_FIRST = 102.0f;
constexpr float ONYXIA_SLOT_BEARING_STEP = 12.0f;
constexpr uint8 ONYXIA_SLOTS_PER_RING = 6;
constexpr float ONYXIA_SLOT_RADIUS_HEAL = 24.0f;
constexpr float ONYXIA_SLOT_RADIUS_CASTER = 30.0f;
constexpr float ONYXIA_SLOT_RADIUS_HUNTER = 36.0f;
constexpr float ONYXIA_SLOT_RADIUS_OVERFLOW = 42.0f;
constexpr float ONYXIA_SLOT_ARRIVAL = 0.5f;
constexpr float ONYXIA_SLOT_RETRIGGER = 3.5f;
// A slot pulled further than this by the floor check is not the slot any more.
constexpr float ONYXIA_SLOT_MAX_CLIP = 3.0f;
// Her live anchor moves the layout only once she has moved this far or turned this much.
constexpr float ONYXIA_ANCHOR_MOVE = 6.0f;
constexpr float ONYXIA_ANCHOR_TURN_DEG = 20.0f;

// Melee: her rear flank, clear of Cleave's front half and Tail Sweep's rear cone.
constexpr float ONYXIA_FLANK_MIN_DEG = 100.0f;
constexpr float ONYXIA_FLANK_MAX_DEG = 120.0f;
constexpr float ONYXIA_FLANK_SLACK_DEG = 5.0f;
constexpr float ONYXIA_FLANK_DEPTH = 0.6f;
// A melee bot this close past her tail keeps the side it had.
constexpr float ONYXIA_FLANK_SIDE_SWITCH_DEG = 20.0f;
// Re-aim a flank walk in flight once its point has moved this far.
constexpr float ONYXIA_FLANK_REAIM = 3.0f;

// Phase 2 cells round each breath lane for ranged and healers.
constexpr float ONYXIA_AIR_STEP_ALONG = 9.0f;
constexpr std::array<float, 3> ONYXIA_AIR_OFFSETS = {13.5f, 22.5f, 31.5f};
constexpr float ONYXIA_AIR_FLOOR_MIN_Z = -90.5f;
constexpr float ONYXIA_AIR_FLOOR_MAX_Z = -82.5f;
constexpr float ONYXIA_AIR_GUARD_CLEARANCE = 20.0f;
// 30yd spell range plus her combat reach plus the caster's own.
constexpr float ONYXIA_AIR_SPELL_RANGE = 49.5f;
constexpr float ONYXIA_AIR_WAYPOINT_SNAP = 3.0f;
}  // namespace OnyHelpers

#endif
