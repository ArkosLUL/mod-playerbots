/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERXT002_H
#define PLAYERBOTS_ULDENCOUNTERXT002_H

#include <string>
#include <vector>

#include "Position.h"
#include "UldData.h"

class Player;
class PlayerbotAI;
class Unit;

// XT-002 Deconstructor.

enum UlduarXT002Ids
{
    // NPC_HEART_OF_DECONSTRUCTOR come from core ulduar.h via UldScripts.h.
    PB_NPC_XT002_PUMMELLER = 33344,    // aggressive add, wants an off-tank
    PB_NPC_XT002_BOOMBOT = 33346,      // explodes on reaching XT or at 50% health; melee must not touch it
    PB_NPC_XT002_LIFE_SPARK = 34004,   // hard mode only, spawned by an expiring Searing Light
    PB_NPC_XT002_VOID_ZONE = 34001,    // hard mode only, dropped by an expiring Gravity Bomb
    // 10-man ids. The core casts these and lets the difficulty table pick, so a 25-man raid carries
    // 65121, 64234 and 64193 instead: read them through sSpellMgr->GetSpellIdForDifficulty.
    SPELL_XT002_SEARING_LIGHT = 63018,
    SPELL_XT002_GRAVITY_BOMB = 63024,
    SPELL_XT002_HEARTBREAK = 65737,     // permanent hard-mode empower once the Heart dies
    SPELL_XT002_EXPOSED_HEART = 63849,  // channeled by the Heart while it is vulnerable
    SPELL_XT002_SUBMERGE = 37751,
};

// XT-002: how far a carrier walks from the raid. Twice the 12 yd splash, since only the carrier moves and a
// bomb landing at the edge still clips whoever drifted a yard.
inline constexpr float ULDUAR_XT002_DEBUFF_CLEAR_RADIUS = 25.0f;

// XT-002: Gravity Bomb's expiry pull (63025/64233 effect 1). The damage only reaches 12 yd, but the pull into
// a fresh Void Zone is what kills, so a stop-short drop point has to clear this.
inline constexpr float ULDUAR_XT002_GRAVITY_BOMB_PULL_RADIUS = 20.0f;

// XT-002: Boom is about 10 yd and a Boombot also blows at 50% health, so melee keep a margin.
inline constexpr float ULDUAR_XT002_BOOMBOT_AVOID_RADIUS = 12.0f;

// XT-002 hard mode: Consumption (64208) is 5 yd and doesn't grow, plus a 1 yd buffer.
inline constexpr float ULDUAR_XT002_VOID_ZONE_RADIUS = 6.0f;

// XT-002: drift allowed off a spot before walking back. Ranged and healers own a slot each, so their band
// only absorbs drift; widening it instead stacks them on one point.
inline constexpr float ULDUAR_XT002_MAINTANK_SPOT_TOLERANCE = 3.0f;
inline constexpr float ULDUAR_XT002_RANGED_SPOT_TOLERANCE = 5.0f;

// XT-002: ranged/healer formation as ellipse half-axes, wider north-south since east-west is the line to XT
// and the tank. Centred north of the anchor; the ranged lots mirror about that centre.
inline constexpr float ULDUAR_XT002_RANGED_RING_OFFSET_Y = 6.0f;
inline constexpr float ULDUAR_XT002_RANGED_RING_INNER_X = 6.0f;
inline constexpr float ULDUAR_XT002_RANGED_RING_INNER_Y = 8.0f;
inline constexpr float ULDUAR_XT002_RANGED_RING_OUTER_X = 9.0f;
inline constexpr float ULDUAR_XT002_RANGED_RING_OUTER_Y = 12.0f;
inline constexpr size_t ULDUAR_XT002_RANGED_RING_INNER_SLOTS = 6;

// XT-002: hunters get an arc round XT, not a ring slot. Shots fail inside 5 yd plus melee range, and the Heart's
// 22.5 yd combat reach puts that at 30.3 yd, past every ring slot. Radius adds the 5 yd slot tolerance.
inline constexpr float ULDUAR_XT002_HUNTER_ARC_RADIUS = 37.0f;

// XT-002: arc bearings off XT's spawn, west turning north. Further south the Searing Light spot is inside its
// 12 yd clearance, further north the northern bomb lot is inside Gravity Bomb's 20 yd pull. A fourth hunter
// starts the next row out.
inline constexpr float ULDUAR_XT002_HUNTER_ARC_FIRST_BEARING_DEG = 175.0f;
inline constexpr float ULDUAR_XT002_HUNTER_ARC_BEARING_STEP_DEG = 10.0f;
inline constexpr size_t ULDUAR_XT002_HUNTER_ARC_SLOTS = 3;
inline constexpr float ULDUAR_XT002_HUNTER_ARC_ROW_STEP = 6.0f;

// XT-002 hard mode: default Void Zone parking grid, walked +x from its origin and then away from the raid in
// y. Step is just over the Void Zone diameter so consecutive drops can't overlap.
inline constexpr float ULDUAR_XT002_BOMB_GRID_STEP = 6.0f;
inline constexpr int ULDUAR_XT002_BOMB_GRID_X_CELLS = 5;
inline constexpr int ULDUAR_XT002_BOMB_GRID_Y_CELLS = 4;

// XT-002 hard mode: melee lot columns. A lot is entered from the raid side, so a row of puddles seals
// everything behind it: wide beats deep.
inline constexpr int ULDUAR_XT002_BOMB_GRID_X_CELLS_MELEE = 7;

// XT-002 hard mode: arrival deadband for a parking cell and the drift allowed before re-issuing the move.
// Re-issuing every tick slides the bot in place so it can't cast.
inline constexpr float ULDUAR_XT002_BOMB_CELL_ARRIVED = 2.0f;
inline constexpr float ULDUAR_XT002_BOMB_CELL_REENGAGE = 5.0f;

// XT-002 hard mode: taken off the debuff before working out reach. Reaction delay, one tick, and a navmesh
// path longer than the straight line.
inline constexpr uint32 ULDUAR_XT002_BOMB_TRAVEL_MARGIN_MS = 1500;

// XT-002 hard mode: a carrier settling at the deadband edge is still outside Consumption. A preference,
// not a gate, or one puddle would run the lot out.
inline constexpr float ULDUAR_XT002_BOMB_CELL_PREFERRED_CLEARANCE =
    ULDUAR_XT002_VOID_ZONE_RADIUS + ULDUAR_XT002_BOMB_CELL_ARRIVED;

// XT-002 hard mode: the carrier outranks the hazard dodge, so walks to a cell rank by how close they pass a
// puddle, capped here. Not a gate: two taken cells in a row would read every cell behind them as blocked.
inline constexpr float ULDUAR_XT002_BOMB_APPROACH_CLEARANCE = 7.5f;

// XT-002: adds further than this from XT are still at their toy pile, and every add comes in by itself.
// Life Sparks are exempt: they spawn on the Searing Light carrier and chase players.
inline constexpr float ULDUAR_XT002_ADD_LEASH_RADIUS = 60.0f;

// XT-002: how far from itself a bot goes for an add. Ranged reach the Searing Light spot, where sparks spawn;
// melee barely move, since adds walk to XT or chase a player. Tanks are exempt.
inline constexpr float ULDUAR_XT002_RANGED_ENGAGE_RANGE = 35.0f;
inline constexpr float ULDUAR_XT002_MELEE_ENGAGE_RANGE = 15.0f;

// Taunt, Growl, Dark Command and Hand of Reckoning are all 30yd.
inline constexpr float ULDUAR_XT002_TAUNT_RANGE = 30.0f;

// XT-002: anchored on XT so one scan covers every lot. The far corner of the northern lot is 92 yd from XT's
// most easterly position, and a puddle outside this is a cell the ranking thinks is free.
inline constexpr float ULDUAR_XT002_VOID_ZONE_SEARCH_RADIUS = 110.0f;

// XT-002: ranked destinations a mover offers per tick. findSmoothPath refuses some walkable points here, the
// same ones every tick, so a lone winner never moves. Each try costs Playerbots.MaxMovementSearchTime builds.
inline constexpr size_t ULDUAR_XT002_MOVE_CANDIDATE_ATTEMPTS = 5;

// XT-002: radius of the Searing Light spot's alternates. Clears the Void Zone radius, so one puddle can't cover
// both the spot and the ring.
inline constexpr float ULDUAR_XT002_SEARING_LIGHT_DETOUR = 10.0f;

// XT-002: room a Searing Light destination keeps from every formation slot, the 8 yd splash plus margin.
inline constexpr float ULDUAR_XT002_SEARING_LIGHT_SLOT_CLEARANCE = 12.0f;

// XT-002 normal mode: bots stop damaging the exposed Heart here so an in-flight hit cannot kill it
// and flip the raid into hard mode by accident.
inline constexpr float ULDUAR_XT002_HEART_SAFE_HP_PCT = 15.0f;

// XT-002: the last Heart phase is over below this, so held burst cooldowns are free. In hard mode it means
// the Heart outlived every window and no Heartbreak is coming.
inline constexpr float ULDUAR_XT002_FINAL_PUSH_HP_PCT = 25.0f;

// XT-002: how long a new holder keeps XT before the healers' tank buffs follow him. At the pull XT's victim
// flips every 1-3 s, and each Beacon, Sacred Shield or Earth Shield moved costs a global and a chunk of mana.
inline constexpr uint32 ULDUAR_XT002_BUFF_TANK_SETTLE_MS = 5000;

// XT-002: nobody has asked for the buff tank this long, so the pull it belongs to is over. Healers ask every
// tick in combat, and a wipe's reset takes far longer.
inline constexpr uint32 ULDUAR_XT002_BUFF_TANK_STALE_MS = 10000;

// XT-002: DPS threat brake, as shares of his holder's threat. He's taunt-immune, so a dps bot that pulls him
// keeps him until a tank's threat passes its own. Area damage this close (his 15 yd reach plus a splash) hits him.
inline constexpr float ULDUAR_XT002_THREAT_CAP = 0.9f;
inline constexpr float ULDUAR_XT002_THREAT_RELEASE = 0.8f;
inline constexpr float ULDUAR_XT002_AREA_THREAT_REACH = 25.0f;

static_assert(ULDUAR_XT002_THREAT_RELEASE < ULDUAR_XT002_THREAT_CAP && ULDUAR_XT002_THREAT_CAP < 1.1f,
              "the brake holds short of the 110% that takes XT off his holder");

// XT-002: a bomb healer stops this far inside heal range of its carrier, who is still walking to a cell.
inline constexpr float ULDUAR_XT002_BOMB_HEAL_MARGIN = 5.0f;

// XT-002: sideways tries round the straight line to the carrier when a puddle or another carrier blocks it.
inline constexpr float ULDUAR_XT002_BOMB_HEAL_BEARING_STEP_DEG = 15.0f;
inline constexpr int ULDUAR_XT002_BOMB_HEAL_BEARING_STEPS = 3;

// XT-002: the sent healer heals its bomb carrier first until it's above this. Below it the bomb's hit plus the
// tantrum after it killed carriers from 36-86%.
inline constexpr float ULDUAR_XT002_BOMB_HEAL_TOP_PCT = 90.0f;

extern const Position ULDUAR_XT002_SPAWN;
extern const Position ULDUAR_XT002_MAINTANK_SPOT;
extern const Position ULDUAR_XT002_RANGED_SPOT;
extern const Position ULDUAR_XT002_SEARING_LIGHT_SPOT;
extern const Position ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_MELEE;
extern const Position ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_RANGED_SOUTH;
extern const Position ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_RANGED_NORTH;

// One Void Zone parking grid, walked +x and `yDirection` in y so the origin is the corner nearest the raid.
// Cell counts are per lot: only the melee one has open ground to the east.
struct XT002BombLot
{
    Position origin;
    float yDirection;
    int xCells = ULDUAR_XT002_BOMB_GRID_X_CELLS;
    int yCells = ULDUAR_XT002_BOMB_GRID_Y_CELLS;
};

// Read off "nearest npcs", not "find target": the Heart never attacks, so it's on no threat list. Each call
// rescans (grid visit plus a LOS ray per npc), so look XT up once per evaluation and pass him down.
Unit* GetXT002(PlayerbotAI* botAI);

// The Heart while it's vulnerable: alive, selectable and channeling Exposed Heart. Damage to it transfers
// to XT, which makes this window the encounter's damage multiplier.
Unit* GetXT002ExposedHeart(PlayerbotAI* botAI);

// XT is down in a Heart phase: not selectable and not attacking anyone. False without an XT.
bool IsXT002Submerged(Unit* xt002);

// Difficulty-mapped debuff ids (the 10- and 25-man versions are separate spells).
uint32 GetXT002SearingLightSpellId(Player* bot);
uint32 GetXT002GravityBombSpellId(Player* bot);
uint32 GetXT002HeartbreakSpellId(Player* bot);

// Tank duties follow whoever XT is on, not the group's main tank flag: he's taunt-immune, so the tank he
// turns to keeps him. Both are living tanks, null only when the group has none.
struct XT002TankRoles
{
    Player* holder = nullptr;     // keeps XT at the tank spot
    Player* pummeller = nullptr;  // takes the Pummellers, the holder itself once it's the last tank
};

// The holder is XT's victim, or in the Heart window the one his threat list will turn him back to. With XT on
// a non-tank it's the tank with most threat on him, and before the pull the flagged main tank.
XT002TankRoles GetXT002TankRoles(Player* bot, Unit* xt002);

// Taunt, target priority and the redirect all read GetXT002TankRoles, so they agree on who holds the add.
bool IsXT002PummellerTank(Player* bot, Unit* xt002);

// The tank the healers' tank buffs go on while XT is in combat: his holder, once that tank has kept him for the
// settle time, or at once when the tank before is dead. Latched per instance, so every healer agrees. Null out
// of combat or with no living tank.
Player* GetXT002BuffTank(Player* bot, Unit* xt002);

// The buff tank if this healer's `spell` is missing on him and castable now, else null. Trigger and action both
// ask, so they can't disagree.
Player* GetXT002BuffTarget(PlayerbotAI* botAI, std::string const& spell);

// True while this dps bot holds its damage on XT: its threat on him reached ULDUAR_XT002_THREAT_CAP of the
// holder's, and stays held until under ULDUAR_XT002_THREAT_RELEASE. A holder still at 0 threat holds every dps
// bot: an engage adds none, and neither does walking XT to the spot. Latched per instance, so the multiplier and
// the priority action agree. False for tanks and healers, out of XT's combat, or with no living tank.
bool IsXT002ThreatBraked(Player* bot, Unit* xt002);

// The Gravity Bomb carrier this healer bot walks out to, else null. A carrier out of every healer's heal range gets
// the nearest bot healer not carrying a debuff, latched until the bomb comes off, so one healer goes and the pick
// can't flip while both walk. Latched per instance, so every healer reads the same answer.
Player* GetXT002BombHealCarrier(Player* bot);

// The carrier this healer was sent to, while it's in heal range and not above ULDUAR_XT002_BOMB_HEAL_TOP_PCT, else
// null. Trigger and action both ask, so they can't disagree.
Player* GetXT002BombHealTarget(Player* bot);

// Every living Gravity Bomb carrier in this bot's group.
std::vector<Player*> GetXT002BombCarriers(Player* bot);

// Close enough to XT to engage; anything further is still at its toy pile. True without an XT, so the gate
// never strands a bot with nothing to hit.
bool IsXT002AddEngageable(Unit* xt002, Unit* unit);

// Nearest live add of `entry` inside the leash and within `botReach`. Nearest, not first found: an add stuck
// at its pile would mask the one hitting the raid.
Unit* GetXT002EngageableAdd(PlayerbotAI* botAI, Player* bot, uint32 entry, float botReach);

// Add that takes no damage: evading, or chasing a victim it can't path to. In a raid map the core never resets
// the second kind and a taunt misses it, so it stays immune until it has a victim it can reach.
bool IsXT002AddEvading(Unit* unit);

// This bot's formation slot. Every bot sorts the same roster, so none needs telling; healers take the centre
// and inner ring to keep the tank in range, hunters the arc. False for melee and tanks, or without a group.
bool GetXT002RangedSlot(Player* bot, Unit* xt002, Position& out);

// Every formation slot except this bot's own, as laid out before any Void Zone re-deal. Empty when the
// group is gone.
std::vector<Position> GetXT002OtherFormationSlots(Player* bot);

// The parking grids this bot's role may drop a Void Zone in. The mover and the "am I in a lot" test both read
// it, so they can't disagree.
std::vector<XT002BombLot> GetXT002BombLots(Player* bot);

#endif
