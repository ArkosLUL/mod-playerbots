/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERXT002_H
#define PLAYERBOTS_ULDENCOUNTERXT002_H

#include <vector>

#include "Position.h"
#include "UldData.h"

class Player;
class PlayerbotAI;
class Unit;

// XT-002 Deconstructor.
//
// XT and his Heart both spend part of the fight carrying UNIT_FLAG_NOT_SELECTABLE, which drops them
// out of "possible targets" entirely, so the encounter is resolved off the raw nearby-npc list
// instead - see GetXT002 below.
//
// The two debuffs pull in opposite directions and drive the formation. Searing Light needs its
// carrier away from everyone; Gravity Bomb's expiry yanks raiders towards the carrier and leaves a
// Void Zone, so bomb carriers walk to fixed grid origins rather than improvising. Heartbreak (hard
// mode) changes what the ranged ring is allowed to look like.

enum UlduarXT002Ids
{
    // NPC_HEART_OF_DECONSTRUCTOR come from core ulduar.h via UldScripts.h.
    PB_NPC_XT002_PUMMELLER = 33344,    // aggressive add, wants an off-tank
    PB_NPC_XT002_BOOMBOT = 33346,      // explodes on reaching XT or at 50% health; melee must not touch it
    PB_NPC_XT002_LIFE_SPARK = 34004,   // hard mode only, spawned by an expiring Searing Light
    PB_NPC_XT002_VOID_ZONE = 34001,    // hard mode only, dropped by an expiring Gravity Bomb
    SPELL_XT002_SEARING_LIGHT_10 = 63018,
    SPELL_XT002_SEARING_LIGHT_25 = 65121,
    SPELL_XT002_GRAVITY_BOMB_10 = 63024,
    SPELL_XT002_GRAVITY_BOMB_25 = 64234,
    SPELL_XT002_EXPOSED_HEART = 63849,  // channeled by the Heart while it is vulnerable
    // Permanent hard-mode empower once the Heart dies. The core casts 65737 and lets the difficulty
    // conversion pick, so 25-man raids carry 64193 and a check against 65737 alone never fires there.
    SPELL_XT002_HEARTBREAK_10 = 65737,
    SPELL_XT002_HEARTBREAK_25 = 64193,
    SPELL_XT002_SUBMERGE = 37751,
};

// XT-002: how far a carrier actually walks. Twice the 12yd splash, because the raid does not step aside
// for it - the carrier is the only one that moves, and a bomb that lands on the edge of the radius
// still clips whoever drifted a yard the wrong way.
constexpr float ULDUAR_XT002_DEBUFF_CLEAR_RADIUS = 25.0f;

// XT-002: how far Gravity Bomb's expiry burst yanks raiders towards the carrier (63025/64233, effect
// 1). The damage half only reaches 12 yd, but being pulled into a fresh Void Zone is what kills, so
// this is the number a drop point has to beat when the carrier cannot make it to the lot.
constexpr float ULDUAR_XT002_GRAVITY_BOMB_PULL_RADIUS = 20.0f;

// XT-002: Boom is roughly 10 yd, and a Boombot also detonates at 50% health, so melee leave margin
// rather than trading the hit for a few swings.
constexpr float ULDUAR_XT002_BOOMBOT_AVOID_RADIUS = 12.0f;

// XT-002 hard mode: Void Zone's Consumption pool. The damage half (64208) is 5 yd and does not grow,
// so this is a 1 yd buffer on top.
constexpr float ULDUAR_XT002_VOID_ZONE_RADIUS = 6.0f;

// XT-002: how far off its spot a bot is allowed to sit before it walks back. The tank's is loose
// enough to survive XT drifting a step. Ranged and healers each own a slot rather than the anchor
// itself, so their band only has to absorb drift - widening it instead of spreading them is what put
// thirteen bots inside 3 yd of one coordinate and handed Searing Light the whole group.
constexpr float ULDUAR_XT002_MAINTANK_SPOT_TOLERANCE = 3.0f;
constexpr float ULDUAR_XT002_RANGED_SPOT_TOLERANCE = 5.0f;

// XT-002: the ranged/healer formation, as half-axes of an ellipse. It is wider north to south than
// east to west on purpose: east-west is the line to XT and to the tank spot, so spreading along it
// costs spell and heal range where spreading across it costs nothing. Every slot stays inside 30 yd of
// XT and 39 yd of the tank. Searing Light is 8 yd, which the outer ring beats and the inner one does
// not quite - 14 bots cannot all sit 8 yd apart and stay in range, so the aim is to cost one splash
// two or three bots instead of the whole group.
//
// The centre sits north of the ranged anchor rather than on it, and the ranged parking lots are
// mirrored about that centre, so the offset now sets the clearance on both sides at once. Centred on
// the anchor instead, the southern slots sit 14 yd from the nearest cell - inside Gravity Bomb's 20 yd
// pull, so an expiring puddle would drag those bots into it. Offset, both edges land at 23 yd.
constexpr float ULDUAR_XT002_RANGED_RING_OFFSET_Y = 6.0f;
constexpr float ULDUAR_XT002_RANGED_RING_INNER_X = 6.0f;
constexpr float ULDUAR_XT002_RANGED_RING_INNER_Y = 8.0f;
constexpr float ULDUAR_XT002_RANGED_RING_OUTER_X = 9.0f;
constexpr float ULDUAR_XT002_RANGED_RING_OUTER_Y = 12.0f;
constexpr size_t ULDUAR_XT002_RANGED_RING_INNER_SLOTS = 6;

// XT-002 hard mode: Void Zone parking grid, walked +x from a Gravity Bomb origin and then away from
// the raid in y. The origin is the corner nearest the raid, so the cells that only get used once the
// lot fills are the far ones. Step is just over the Void Zone diameter so consecutive drops cannot
// overlap.
constexpr float ULDUAR_XT002_BOMB_GRID_STEP = 6.0f;
constexpr int ULDUAR_XT002_BOMB_GRID_X_CELLS = 5;
constexpr int ULDUAR_XT002_BOMB_GRID_Y_CELLS = 4;

// XT-002 hard mode: arrival deadband for a parking cell, and the drift tolerated before the carrier
// re-issues a move. A bot that re-issues every tick slides in place and cannot cast, and Gravity Bomb
// only lasts 9s.
constexpr float ULDUAR_XT002_BOMB_CELL_ARRIVED = 2.0f;
constexpr float ULDUAR_XT002_BOMB_CELL_REENGAGE = 5.0f;

// XT-002 hard mode: taken off the debuff before working out how far a carrier can still walk. Covers
// the reaction delay and one engine tick, plus a little for the navmesh path being longer than the
// straight line the reach is measured along.
constexpr uint32 ULDUAR_XT002_BOMB_TRAVEL_MARGIN_MS = 1500;

// XT-002 hard mode: clearance a parking cell is preferred to have, so a carrier that settles at the
// edge of the arrival deadband is still outside Consumption. A preference and not a gate: as a gate
// one puddle would block five cells of twenty and the lot would run out mid-fight.
constexpr float ULDUAR_XT002_BOMB_CELL_PREFERRED_CLEARANCE =
    ULDUAR_XT002_VOID_ZONE_RADIUS + ULDUAR_XT002_BOMB_CELL_ARRIVED;

// XT-002 hard mode: clearance the walk to a parking cell keeps from puddles already down. The carrier
// action outranks "xt002 avoid hazard action", so nothing else protects a carrier on the way in.
constexpr float ULDUAR_XT002_BOMB_APPROACH_CLEARANCE = 7.5f;

// XT-002: adds spawn at toy piles 78-125yd out and walk in, and one that never paths away from its
// pile sits there for the rest of the fight. Nothing out there needs fetching - Scrapbots and Boombots
// come to XT, Pummellers chase whoever they aggro - so anything this far from the boss is left alone.
// Life Sparks are exempt: they spawn on the Searing Light carrier and chase players, not XT.
constexpr float ULDUAR_XT002_ADD_LEASH_RADIUS = 60.0f;

// XT-002: how far a bot travels for an add, measured from itself. Ranged reach the Life Spark spot
// without leaving their anchor; melee never need to move, since every add either walks to XT or chases
// a player home. Tanks are exempt - going and getting the Pummeller is the off-tank's job.
constexpr float ULDUAR_XT002_RANGED_ENGAGE_RANGE = 35.0f;
constexpr float ULDUAR_XT002_MELEE_ENGAGE_RANGE = 15.0f;

// Taunt, Growl, Dark Command and Hand of Reckoning are all 30yd.
constexpr float ULDUAR_XT002_TAUNT_RANGE = 30.0f;

// Anchored on XT rather than the carrier, so one lookup covers every parking lot wherever the carrier
// happens to be standing when the debuff lands. The far corner of the northern lot sits 92yd from the
// most easterly position XT has been measured at, and a puddle outside this radius is a cell the
// ranking believes is free.
constexpr float ULDUAR_XT002_VOID_ZONE_SEARCH_RADIUS = 110.0f;

// XT-002: how many ranked destinations a mover will offer the pathfinder before giving up for the
// tick. findSmoothPath refuses points in this room that are plainly walkable - it returns
// PATHFIND_NOPATH for the Searing Light spot from most of the melee stack while the mesh is intact
// underneath - and the refusal is deterministic, so a mover that re-offers its one winner never moves
// the bot at all. Which points it refuses does not follow the geometry: probing the ring below from
// the melee stack, four of the eight path normally and four do not, in no particular arrangement. Each
// attempt costs AiPlayerbot.MaxMovementSearchTime path builds, and only carriers run this loop.
constexpr size_t ULDUAR_XT002_MOVE_CANDIDATE_ATTEMPTS = 5;

// XT-002: how far off the Searing Light spot its alternates sit. Clears the Void Zone radius, so a
// puddle covering the spot cannot cover the ring as well - one parked itself 2 yd from the spot and
// stayed for the last 133s of a fight, and every carrier sent there afterwards stood in Consumption.
constexpr float ULDUAR_XT002_SEARING_LIGHT_DETOUR = 10.0f;

// How much room a Searing Light destination keeps from a formation slot. The 8yd splash plus margin.
// Part of the ring always points back at the raid, and those headings are the nearest ones, so a
// carrier picked them 132 times in one pull with 4 to 6 raiders inside the splash every time.
constexpr float ULDUAR_XT002_SEARING_LIGHT_SLOT_CLEARANCE = 12.0f;

// XT-002 normal mode: bots stop damaging the exposed Heart here so an in-flight hit cannot kill it
// and flip the raid into hard mode by accident.
constexpr float ULDUAR_XT002_HEART_SAFE_HP_PCT = 15.0f;

// XT-002 normal mode: the last Heart phase is over below this, so the held burst cooldowns are free.
constexpr float ULDUAR_XT002_FINAL_PUSH_HP_PCT = 25.0f;

extern const Position ULDUAR_XT002_MAINTANK_SPOT;
extern const Position ULDUAR_XT002_RANGED_SPOT;
extern const Position ULDUAR_XT002_SEARING_LIGHT_SPOT;
extern const Position ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_MELEE;
extern const Position ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_RANGED_SOUTH;
extern const Position ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_RANGED_NORTH;

// One Void Zone parking grid. The grid walks +x from the origin and `yDirection` in y, so the origin
// is always the corner nearest the raid whichever side of the formation the lot is on.
struct XT002BombLot
{
    Position origin;
    float yDirection;
};

// XT-002 Deconstructor. These use GetFirstAliveUnitByEntry rather than "find target": the Heart
// never attacks anyone, so it never lands on a bot's threat list and "find target" cannot resolve it.
// Each call is a fresh scan ("nearest npcs" recomputes on every read: 100yd grid visit plus a LOS ray
// per npc), so look XT up once per evaluation and pass him down.
Unit* GetXT002(PlayerbotAI* botAI);

// The Heart while it is actually vulnerable - alive, selectable and channeling Exposed Heart.
// Damage dealt to it transfers to XT, which makes this window the encounter's damage multiplier.
Unit* GetXT002ExposedHeart(PlayerbotAI* botAI);

// XT is down in a Heart phase: not selectable and not attacking anyone. False without an XT.
bool IsXT002Submerged(Unit* xt002);

// Difficulty-mapped debuff ids (the 10- and 25-man versions are separate spells).
uint32 GetXT002SearingLightSpellId(Player* bot);
uint32 GetXT002GravityBombSpellId(Player* bot);
uint32 GetXT002HeartbreakSpellId(Player* bot);

// This bot owns the Pummeller: the first assist tank, falling back to the main tank when the raid has
// no second tank left. Both the taunt and the tank's target priority read it, so they cannot disagree
// about who is holding the add.
bool IsXT002PummellerTank(PlayerbotAI* botAI, Player* bot);

// Close enough to XT to be worth engaging. Anything further out is still sitting at its toy pile.
// True when XT cannot be found, so the gate can never strand a bot with nothing to hit.
bool IsXT002AddEngageable(Unit* xt002, Unit* unit);

// Nearest live add of `entry` inside the leash around XT and within `botReach` of the bot. Nearest
// rather than first-found: GetFirstAliveUnitByEntry lets an add stuck at a pile mask the one actually
// hitting the raid. The leash applies whatever `botReach` is passed.
Unit* GetXT002EngageableAdd(PlayerbotAI* botAI, Player* bot, uint32 entry, float botReach);

// This bot's own spot in the ranged/healer formation, laid out just north of ULDUAR_XT002_RANGED_SPOT.
// Every bot sorts the same roster the same way and reads its own index out of it, so the layout needs
// no communication and survives a death mid-fight. Healers take the centre and the inner ring because
// they are the ones who need the tank in range; ranged dps fill the outer. A slot sitting in a Void
// Zone is dealt out. False when the bot is neither ranged dps nor a healer, or the group is gone.
bool GetXT002RangedSlot(Player* bot, Unit* xt002, Position& out);

// Every formation slot except this bot's own, as laid out before any Void Zone re-deal. Empty when the
// group is gone.
std::vector<Position> GetXT002OtherFormationSlots(Player* bot);

// The parking grids this bot's role may drop a Void Zone in. Melee get the one under their stack;
// ranged and healers get one on each side of the formation, so a carrier parks on whichever side it
// is already standing. Everything that reads a lot goes through here, so the mover and the
// "am I in a lot" test cannot disagree about which grid a bot owns.
std::vector<XT002BombLot> GetXT002BombLots(PlayerbotAI* botAI, Player* bot);

#endif
