/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_MARROWGAR_H
#define PLAYERBOTS_ICCHELPERS_MARROWGAR_H

#include <vector>

#include "Define.h"
#include "ICCData.h"
#include "Position.h"

class Creature;
class Player;
class PlayerbotAI;
class Unit;

// icecrown_citadel.h CreaturesIds
constexpr uint32 NPC_LORD_MARROWGAR = 36612;
// boss_lord_marrowgar.cpp: the ground patch each Coldflame head lays, 69146 on 10N, remapped at the call.
// The Coldflame casts and Bone Storm have no difficulty row.
constexpr uint32 SPELL_LM_COLDFLAME_PATCH = 69146;
constexpr uint32 SPELL_LM_COLDFLAME_STORM = 72705;

// His combat reach. Coldflame picks among players further than reach + 1.5 + 1 from his centre, and each
// line starts half his reach out from him.
constexpr float LM_COMBAT_REACH = 20.25f;
// A patch is 3 yd (SpellInfoCorrections) and takes in a player's 1.5 reach. A head lays one where it
// stands, then steps 5 yd every 450 ms for 12 s or until a wall stops it.
constexpr float LM_COLDFLAME_REACH = 4.5f;
constexpr float LM_COLDFLAME_STEP = 5.0f;
constexpr float LM_COLDFLAME_FIRST = LM_COMBAT_REACH / 2.0f;
// Inner slots sit this close to his centre, so a line starting at LM_COLDFLAME_FIRST never reaches them.
// They spread over this arc of his back, out of his parry arc and as far from Bone Slice as his centre.
constexpr float LM_INNER_RADIUS = 2.0f;
constexpr float LM_INNER_ARC_DEG = 160.0f;
// Bait ring: outside the Coldflame exclusion and past a hunter's ~28 yd dead zone on a 20 yd boss,
// while healers at his centre keep it in their 38.5 yd range. Neighbours are 10.8 yd apart, past a line.
constexpr float LM_BAIT_RADIUS = 31.0f;
constexpr float LM_BAIT_SPACING_DEG = 20.0f;
constexpr uint32 LM_BAIT_SLOTS = 10;
static_assert(LM_INNER_RADIUS + 1.0f < LM_COLDFLAME_FIRST - LM_COLDFLAME_REACH,
              "a parked inner bot stays clear of every line");
// Spell::CheckRange adds the melee range to a ranged spell's 5 yd minimum
static_assert(LM_BAIT_RADIUS - 1.0f > LM_COMBAT_REACH + 1.5f + 4.0f / 3.0f + 5.0f, "a parked hunter bait can shoot");

// Bone Slice hits up to 2 (10-man) or 3 (25-man) players within 5 yd of his victim (SpellInfoCorrections)
// and splits its damage among them. Off-tanks stand this far beside the main tank; nobody else comes
// within LM_SLICE_KEEPOUT of it.
constexpr float LM_TANK_STACK_OFFSET = 2.0f;
constexpr float LM_SLICE_KEEPOUT = 8.0f;

// A melee dps takes a spike this close, unless it stands inside LM_SLICE_KEEPOUT of the main tank
constexpr float LM_MELEE_SPIKE_REACH = 10.0f;

constexpr float LM_STACK_SLOT_RADIUS = 1.5f;

// A stand parks within LM_STAND_PARK and walks back once pushed past LM_STAND_UNPARK
constexpr float LM_STAND_PARK = 1.0f;
constexpr float LM_STAND_UNPARK = 2.5f;
static_assert(LM_STAND_PARK < LM_STAND_UNPARK, "a walk has to end inside its own trigger");

// Dodge: fires inside a patch's reach plus margin, lands past it with the park tolerance on top
constexpr float LM_DODGE_TRIGGER = LM_COLDFLAME_REACH + 0.5f;
constexpr float LM_DODGE_CLEARANCE = LM_COLDFLAME_REACH + LM_STAND_PARK + 1.5f;
constexpr float LM_DODGE_TIGHT_CLEARANCE = LM_DODGE_TRIGGER + 0.5f;
static_assert(LM_DODGE_TRIGGER < LM_DODGE_TIGHT_CLEARANCE && LM_DODGE_TIGHT_CLEARANCE < LM_DODGE_CLEARANCE);

// navprobe-clean (--nav 0x09, settled z 41.4-42.4), and so are the bait ring and inner slots round a boss
// up to 6 yd off his spot and the re-centre pull points. All of the floor is inside his evade boundary.
// He attacks and charges nobody past x -337, so a dodge keeps x below LM_GATE_X_LIMIT.
inline Position const ICC_LM_BOSS_SPOT(-401.37f, 2211.14f, 41.99f);
inline Position const ICC_LM_TANK_SPOT(-415.37f, 2211.14f, 41.99f);
inline Position const ICC_LM_STORM_STACK(-384.0f, 2211.0f, 41.99f);
// 34 yd from the stack at 180, 60 and 300 degrees: a healer there reaches them all. 180 sits 2.6 yd from
// the tank spot and each side one 14-19 yd from a bait slot, so all three are held when his first charge
// picks the farthest players 3 s into the cast. From the side ones his cross passes the stack 8.8 yd off.
inline Position const ICC_LM_STORM_ANCHOR_180(-418.0f, 2211.0f, 41.99f);
inline Position const ICC_LM_STORM_ANCHOR_60(-367.0f, 2240.44f, 41.76f);
inline Position const ICC_LM_STORM_ANCHOR_300(-367.0f, 2181.56f, 41.76f);
constexpr float LM_GATE_X_LIMIT = -340.0f;

enum class IccLmPhase : uint8
{
    Normal,
    StormCast,  // the 3 s Bone Storm cast, so the storm stands start before he spins
    Storm
};

// Stored in lm.duty
enum class IccLmDuty : uint8
{
    None = 0,
    MainTank = 1,
    OffTank = 2,
    Inner = 3,
    Bait = 4
};

bool IsLmLive(PlayerbotAI* botAI);
// Alive and in combat, from a per-instance guid cache rescanned at most once a second
Creature* GetLmBoss(PlayerbotAI* botAI);
// Notes lm.phase
IccLmPhase GetLmPhase(Player* bot, Creature* boss);

// DPS only: a player's skull on him or a spike, then a spike in a melee's reach or one no melee reaches
// for ranged, else him. Notes lm.spike.
Unit* GetLmTarget(PlayerbotAI* botAI);

bool IccLmHoldsStand(PlayerbotAI* botAI);
bool IccLmHoldsNormalStand(PlayerbotAI* botAI);
bool IccLmHoldsStormStand(PlayerbotAI* botAI);
bool GetLmStandSpot(PlayerbotAI* botAI, Position& spot);
// The stand wants a walk and neither its spot nor the way there is under Coldflame
bool GetLmStandWalk(PlayerbotAI* botAI, Position& spot);

// Coldflame patches on the ground, the next ones each live head lays, and the cross a Bone Storm charge is
// about to drop where it ends. Cached per instance for 100 ms.
void GetLmColdflame(PlayerbotAI* botAI, std::vector<Position>& circles);
// Alive, free to move and inside LM_DODGE_TRIGGER of one of those. Notes lm.dodge clear once out.
bool IsLmInColdflame(PlayerbotAI* botAI);

// The phase note, the duty roster, the stand latches, the reset after a wipe, a bot's saved rti once
void TickLmEncounter(PlayerbotAI* botAI);

// The opener hook's boss list
std::vector<uint32> const& GetLmOpenerBosses(PlayerbotAI* botAI);

#endif
