/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERHODIR_H
#define PLAYERBOTS_ULDENCOUNTERHODIR_H

#include "EncounterHelpers.h"
#include "Position.h"
#include "UldData.h"

#include <vector>

class Creature;
class Player;
class PlayerbotAI;
class Unit;

// Hodir.
//
// A soft enrage on a hard timer, so the fight is about throughput rather than survival. Starlight
// and the helper NPCs' Toasty Fires are the two buffs worth standing in, and Biting Cold punishes
// standing still, so the formation shuttles between zones rather than holding one spot.
//
// Flash Freeze encases anyone not under shelter and drops ice blocks that trap allies; freeing them
// is a damage job handed out per bot below. Icicles fall continuously and have to be dodged, which
// is why almost every point derived here is snapped to the floor before it is used.

enum UlduarHodirIds
{
    // the small one that lands every 2s and must be dodged. 33173 "Snowpacked Icicle" is the Flash
    // Freeze drift; it must be dodged only while it is still falling, and it leaves a 7 yd Ice
    // Shards pool where it lands. 33174 "Snowpacked Icicle Target" is the invisible dummy 33173
    // spawns beside itself - non-attackable, and the thing that carries the Safe Area aura Flash
    // Freeze checks for. Only 33174 is a shelter; 33173 is a hazard that happens to mark one.
    NPC_HODIR_ICICLE_SMALL = 33169,
    NPC_HODIR_ICICLE_DRIFT = 33173,
    NPC_SNOWPACKED_ICICLE = 33174,
    NPC_TOASTY_FIRE = 33342,
    NPC_HODIR_FLASH_FREEZE_BLOCK = 32938,   // ice block encasing a frozen helper; kill it to free them
    NPC_HODIR_FLASH_FREEZE_PLAYER = 32926,  // same, on a raider - the next Flash Freeze instakills them
    // The helpers, by faction. The _10 row spawns in both sizes, the _25 row only in 25man. Each block
    // is summoned by the helper inside it, so the summoner's entry says who a block holds.
    NPC_HODIR_MAGE_ALLIANCE_10 = 32893,
    NPC_HODIR_MAGE_ALLIANCE_25 = 33327,
    NPC_HODIR_MAGE_HORDE_10 = 32946,
    NPC_HODIR_MAGE_HORDE_25 = 33331,
    NPC_HODIR_DRUID_ALLIANCE_10 = 32901,
    NPC_HODIR_DRUID_ALLIANCE_25 = 33325,
    NPC_HODIR_DRUID_HORDE_10 = 32941,
    NPC_HODIR_DRUID_HORDE_25 = 33333,
    NPC_HODIR_SHAMAN_ALLIANCE_10 = 32900,
    NPC_HODIR_SHAMAN_ALLIANCE_25 = 33328,
    NPC_HODIR_SHAMAN_HORDE_10 = 32950,
    NPC_HODIR_SHAMAN_HORDE_25 = 33332,
    NPC_HODIR_PRIEST_ALLIANCE_10 = 32897,
    NPC_HODIR_PRIEST_ALLIANCE_25 = 33326,
    NPC_HODIR_PRIEST_HORDE_10 = 32948,
    NPC_HODIR_PRIEST_HORDE_25 = 33330,
    SPELL_FLASH_FREEZE = 61968,
    SPELL_BITING_COLD_PLAYER_AURA = 62039,
    SPELL_HODIR_FLASH_FREEZE_TRAPPED = 61969,
    SPELL_HODIR_STARLIGHT = 62807,
    SPELL_HODIR_TOASTY_FIRE_AURA = 62821,
    SPELL_HODIR_FROZEN_BLOWS = 62478,  // base id, difficulty-mapped at runtime
    SPELL_HODIR_STORM_CLOUD = 65123,   // base id, difficulty-mapped at runtime
    SPELL_HODIR_STORM_POWER = 63711,   // what the carrier hands out; also difficulty-mapped
    SPELL_HODIR_SHATTER_CHEST_TIMER = 65272,  // on himself from the pull for 180 s, so its age is the pull clock
};

// Hodir.
//
// Starlight (62807) is the fight's biggest throughput lever: aura 193 runs through
// HandleModCombatSpeedPct, which applies to cast time as well as all three attack timers, for +50%.
// Its DBC row says 8, but it does not behave like 8: across two traces, bots holding the aura sit at
// a median 2.0 yd from the zone and p90 3.3, while bots without it are already at 7.6 by the tenth
// percentile. 4 is what it reaches, and that is small enough that a zone holds one bot at the 4.5 yd
// spacing icicles force - so it is a per-bot opportunity, never something to build a formation on.
//
// Toasty Fire (62821) measures true to its 11 (with-aura p90 11.9) and is the one worth standing in:
// it stops Biting Cold, which is otherwise a tenth of the raid's time spent walking. It grants no
// Flash Freeze exemption, whatever the old comment here claimed - only the Snowpacked Icicle Target
// does that, through 65705 -> 62464.
//
// 3, not the 8 the DBC row carries and not the 4 two traces of medians suggested. Binned by distance,
// the hold rate is 94/90/78% across the first three yards and falls off a cliff to 21% in the fourth,
// so the edge is at 3 and the 4th yard was sampling blur - bots cover 1.75 yd between snapshots.
constexpr float ULDUAR_HODIR_STARLIGHT_RADIUS = 3.0f;
constexpr float ULDUAR_HODIR_TOASTY_FIRE_RADIUS = 11.0f;
constexpr float ULDUAR_HODIR_SAFE_AREA_RADIUS = 9.0f;
// The run parks at TOLERANCE and only releases at RELEASE. MoveInside lands the bot at exactly
// TOLERANCE from the centre, so testing the same number at both ends means arriving in the shelter
// releases the bot the same tick and the ring anchor walks it straight back out - measured at 18-22
// yd out with the freeze 2 s away. Both rings stay inside the 9 yd Safe Area (62464, radius index 40).
constexpr float ULDUAR_HODIR_SAFE_AREA_TOLERANCE = 6.0f;
constexpr float ULDUAR_HODIR_SAFE_AREA_RELEASE = 8.0f;
static_assert(ULDUAR_HODIR_SAFE_AREA_RELEASE < ULDUAR_HODIR_SAFE_AREA_RADIUS,
              "the release ring has to stay inside what Safe Area actually covers");
static_assert(ULDUAR_HODIR_SAFE_AREA_TOLERANCE < ULDUAR_HODIR_SAFE_AREA_RELEASE,
              "the park ring has to sit inside the release ring or arriving releases the bot");

// Storm Power (65134) is an area pulse cast at the carrier's own feet - boss_hodir.cpp casts it on a
// null target - so how many it hits is bounded only by who is standing inside 3 yd. The carrier holds
// 4 (10man) / 6 (25man) charges.
//
// Touring the ring spends them one bot at a time, because the formation's own spacing is wider than
// the pulse: raiders sat inside it on 4.4% of samples during a live carry, p50 0-1 of them, while
// coverage over 50% was worth 198857 dps against 123105 under it. So the raid gathers on the carrier
// instead, and the carrier holds still.
constexpr float ULDUAR_HODIR_STORM_CLOUD_STACK_RADIUS = 3.0f;

// Where a receiver parks and how far it may drift before it is sent back, same 2 yd of hysteresis the
// shelter run uses - testing one number at both ends releases the bot the tick it arrives. 2 sits a
// yard inside the pulse, which is the blur a bot covers between two snapshots.
constexpr float ULDUAR_HODIR_STORM_CLOUD_COLLECT_PARK = 2.0f;
constexpr float ULDUAR_HODIR_STORM_CLOUD_COLLECT_RELEASE = 4.0f;
static_assert(ULDUAR_HODIR_STORM_CLOUD_COLLECT_PARK < ULDUAR_HODIR_STORM_CLOUD_STACK_RADIUS,
              "a parked receiver has to be inside the pulse, not on its edge");

// How far a bot will walk to collect. Short on purpose: a melee carrier then gathers the melee already
// on the boss and a ranged carrier gathers the formation, instead of either dragging the other half of
// the raid across the room for six seconds of buff.
constexpr float ULDUAR_HODIR_STORM_CLOUD_COLLECT_LEASH = 15.0f;

// Two different pools, two different radii, both 13000-14000 a hit. Icicle 33169 leaves Ice Shards
// 62457 in 4 yd; Snowpacked Icicle 33173 leaves Ice Shards 65370 in 7 yd. Clearing everything to 6
// stepped bots to the edge of the big one and killed four of them in one pull. Bots step past the
// edge rather than onto it, so each clear carries 2 yd of margin over its own radius.
constexpr float ULDUAR_HODIR_ICE_SHARDS_RADIUS = 4.0f;
constexpr float ULDUAR_HODIR_ICE_SHARDS_CLEAR = 6.0f;
constexpr float ULDUAR_HODIR_BIG_SHARDS_RADIUS = 7.0f;
constexpr float ULDUAR_HODIR_BIG_SHARDS_CLEAR = 9.0f;

// An icicle summon lives 7000ms (62234/62462, DurationIndex 165) but detonates at 3700ms: its AI
// casts the fall effect at 2000ms and that aura's single 1700ms tick triggers the blast. The last
// 3300ms are inert, and at one icicle every 2s roughly half of those alive have already blown.
constexpr uint32 ULDUAR_HODIR_ICICLE_SPENT_MS = 3300;

// Arrival tolerance for the tank hold spot, which doubles as the re-anchor threshold.
constexpr float ULDUAR_HODIR_TANK_HOLD_TOLERANCE = 3.0f;

// How far from Hodir a caster may stand and still reach him. The Starlight stand is tested against
// it, because a spot that cannot reach the boss is worth nothing whatever haste it carries.
//
// 35, not the 30 a Shadow Bolt's book range says. Spell range is measured to the target's bounding
// radius and Hodir is a giant, so the book number understates him: binned by distance, ranged bots
// dealt 7693 dps each from 30-35 yd against 7844 from 20-25, and casts aimed at him were started out
// to 40.1 (p99 36.2). 35-40 is where it falls off, to 4854, so the band ends there and not before.
constexpr float ULDUAR_HODIR_CASTER_MAX_BOSS_GAP = 35.0f;

constexpr float ULDUAR_HODIR_DODGE_LEASH = 12.0f;
constexpr float ULDUAR_HODIR_DECLUMP_RADIUS = 4.5f;

// The dodge decides to leave on the radius that actually kills, and lands on the clear that carries
// margin. Testing the clear at both ends is what had bots stepping out of pools they were never in:
// the small one triggers over 2.25x the area it kills in.
constexpr float ULDUAR_HODIR_DODGE_TRIGGER_MARGIN = 0.5f;

// The dodge holds one destination rather than deriving a new one every tick. Inside ARRIVE it has got
// there and stops issuing; slip further than SLIP back from its closest approach and something else is
// steering the bot, so it re-issues from where the bot actually is and takes the movement slot back.
//
// ARRIVE has to stay well short of the sweep's own step. The sweep rings outward in 2 yd hops, so at
// 1.5 a bot counted as arrived a fifth of the way into the leg, still inside the radius that re-arms
// the trigger, and dodged again immediately.
constexpr float ULDUAR_HODIR_DODGE_ARRIVE = 0.8f;
constexpr float ULDUAR_HODIR_DODGE_SLIP = 1.0f;

// How far the Starlight step looks for zones. Deliberately short of the room radius: it runs per bot
// per tick and sweeps every world object in range, and a zone beyond this is out of reach of any slot
// the bot could be on.
constexpr float ULDUAR_HODIR_STARLIGHT_SEARCH_RADIUS = 30.0f;

// Where a bot stands once it has stepped into a Starlight zone: this far from the zone centre, on the
// bearing of the slot it came from, so several bots in one zone spread around it rather than piling
// on a point. Any number may share a zone - one Ice Shards hit is 41% of a health pool (p90 54%), so
// an icicle catching two of them is two heals, and +50% to every cast and swing is worth that.
//
// Stand radius plus arrival tolerance has to stay inside what Starlight reaches, with room to spare.
// At 2 + 1 the sum landed exactly on the 3 yd edge, so a bot that reported arrived was standing where
// the aura holds one tick in five and any nudge dropped it.
//
// The margin comes out of the stand radius, never the tolerance. Tolerance is also what stands the
// position trigger down, so trimming it would just move the churn from the dodge to the anchor.
constexpr float ULDUAR_HODIR_STARLIGHT_STAND_RADIUS = 1.5f;
constexpr float ULDUAR_HODIR_STARLIGHT_STAND_TOLERANCE = 1.0f;

// How close two sweeps have to agree before the second is the same zone as the first. Starlight zones
// are static dynamic objects, so this only has to absorb float noise, not movement.
constexpr float ULDUAR_HODIR_STARLIGHT_ZONE_MATCH = 1.0f;
static_assert(ULDUAR_HODIR_STARLIGHT_STAND_RADIUS + ULDUAR_HODIR_STARLIGHT_STAND_TOLERANCE <=
                  ULDUAR_HODIR_STARLIGHT_RADIUS,
              "a bot at the edge of its tolerance has to still be inside Starlight");

// How long a latched Starlight stand may sit outside the band before the bot gives the zone up. A yard
// of wobble at the band edge clears inside this, Hodir moving between the centre and a fire doesn't.
// Over three pulls 84-96% of the time held out of the band came in runs of 10 s or more, 0-1% under 3.
constexpr uint32 ULDUAR_HODIR_STARLIGHT_REJECT_DROP_MS = 3000;

// Where the two ends of the shed shuttle sit when the bot is standing in Starlight. Both ends and the
// straight line between them stay inside the zone, so the aura survives the shuttle that would
// otherwise walk the bot out of it. A yard inside the 3 yd edge at both ends, because an end sitting
// on it sheds the buff the shuttle is being routed this way to keep.
//
// 4 yd end to end is short of the declump leg, which is the shortest single move that spans two aura
// ticks - but the shuttle chains, alternating ends until the aura is gone, so the bot never stops and
// it is the chain rather than the leg that covers the ticks.
constexpr float ULDUAR_HODIR_STARLIGHT_SHED_RADIUS = 2.0f;
static_assert(ULDUAR_HODIR_STARLIGHT_SHED_RADIUS < ULDUAR_HODIR_STARLIGHT_RADIUS,
              "both ends of the shed shuttle have to stay inside Starlight");

// Where a bot stands in a Toasty Fire and how far it may drift before it is sent back: 8 and 10
// against the aura's 11, the same 2 yd of hysteresis every other run here carries. A fire is worth
// walking to for two things - it sheds Biting Cold on every tick exactly as moving does, and a ranged
// attack or a magic spell cast from inside one procs Singed on Hodir, +2% magic damage taken a stack
// up to 25. The aura itself is worth +4 Spirit and nothing else.
constexpr float ULDUAR_HODIR_FIRE_STAND_RADIUS = 8.0f;
constexpr float ULDUAR_HODIR_FIRE_STAND_TOLERANCE = 2.0f;
static_assert(ULDUAR_HODIR_FIRE_STAND_RADIUS + ULDUAR_HODIR_FIRE_STAND_TOLERANCE <
                  ULDUAR_HODIR_TOASTY_FIRE_RADIUS,
              "a bot at the edge of its tolerance has to still be inside the fire");

// How far ranged and healers walk for a buff, and why there are two of these. A bot already in
// Starlight or a fire gives up one it holds to chase another, so it keeps the short leash. A bot with
// neither gives up only the walk: 30 yd is about 4s at run speed, against a Starlight zone that lives
// 51s and carries +50% haste. The long one has to be generous because Hodir is parked on a fire, and
// every zone a caster may legally stand in is therefore out on the helper arc: 15 yd reaches 6.7% of
// the legal stands that are up, 30 reaches 30.5%.
constexpr float ULDUAR_HODIR_BUFF_WALK = 15.0f;
constexpr float ULDUAR_HODIR_BUFF_WALK_UNBUFFED = 30.0f;
static_assert(ULDUAR_HODIR_BUFF_WALK_UNBUFFED <= ULDUAR_HODIR_STARLIGHT_SEARCH_RADIUS,
              "walking further than the zone sweep looks would find nothing to walk to");

// Ranged and healers hold at least this far from Hodir. Not for his swings, which only ever land on
// his victim, and nothing else he casts cares how close a raider stands. It keeps them off the melee,
// who stand within ~6.5 yd of him, so an icicle or a Freeze on one doesn't catch the other, and out of
// his melee range, where a raider pulls aggro at 110% of the tank's threat rather than 130%.
constexpr float ULDUAR_HODIR_RANGED_MIN_BOSS_GAP = 15.0f;
static_assert(ULDUAR_HODIR_RANGED_MIN_BOSS_GAP < ULDUAR_HODIR_CASTER_MAX_BOSS_GAP,
              "the caster band has to have room between its ends");

// A Starlight stand may come this close, since +50% haste is worth standing near the melee for. The
// stand itself stays out of his melee range: combat reach 5, a raider's 1.5 and 4/3 make 7.8. The
// druids cast where they stand, next to him, so 46% of zone samples sat inside 15 yd of him, and a
// stand within walking reach went from 33% of caster samples at 15 to 48% here.
constexpr float ULDUAR_HODIR_STARLIGHT_MIN_BOSS_GAP = 10.0f;
static_assert(ULDUAR_HODIR_STARLIGHT_MIN_BOSS_GAP <= ULDUAR_HODIR_RANGED_MIN_BOSS_GAP,
              "Starlight is the one stand allowed inside the caster gap, never one kept further out");

// How far past the hold point a tank stands. Hodir stops a median 5.0 yd from whoever holds him
// (p10 3.9, p90 7.0) and 3.6-4.2 yd back along the way he was dragged, so this lands him on the point.
// With the point on a fire, melee sit p50 1.2 yd behind his centre and wrap round him, so him landing
// there puts 98% of melee and pet samples inside its 11 yd.
constexpr float ULDUAR_HODIR_HOLD_TANK_OFFSET = 4.5f;

// How far from ULDUAR_HODIR_CENTRE a fire may sit and still be worth dragging him onto. Measured from
// the centre, not from him, so he never ends up further than this from the middle of the room and
// every tank spot stays 40+ yd inside the y band he evades outside. It does not pin the helpers: they
// drift over a pull (one mage's block went from 6 to 24 yd off the centre), so fires land outside it.
//
// 25, because he is held on the fire itself now rather than at a point near it. Some fire was inside
// this for 78/48/51% of 0-3:00 across three pulls, against 27/33/51% actually held under the old 12.
constexpr float ULDUAR_HODIR_HOLD_FIRE_LEASH = 25.0f;

// Closer than this and a bearing from Hodir to the point means nothing, so the tank takes the fixed
// outward one instead.
constexpr float ULDUAR_HODIR_HOLD_BEARING_MIN_GAP = 3.0f;

// He only walks toward whoever holds him. Shoved past the tank spot (a shelter run, a knockback) he
// parks there with the spot already in reach, 7.5-9 yd off the point for 113s of 186 in one pull. Still
// and this far off for this long, the bearing is re-aimed from where he stands. He normally stops within
// 1-2 yd of the point, so 5 only catches the parked case.
constexpr float ULDUAR_HODIR_HOLD_REAIM_GAP = 5.0f;
constexpr uint32 ULDUAR_HODIR_HOLD_REAIM_MS = 2000;

// Biting Cold ticks every second (measured 1005ms) for 200*2^stacks. The server takes a stack off on
// the second consecutive tick that reads target->isMoving(), and any stationary tick in between
// resets that progress - spell_hodir_biting_cold_player_aura in boss_hodir.cpp. So shedding needs
// more than a second of unbroken travel, which is why legs are 6 yd and chain until the aura is gone.
//
// isMoving() is the raw MOVEMENTFLAG_MASK_MOVING and does include the flags a jump raises, but an
// in-place jump cannot use that: MotionMaster::MoveJump splines to the point and takes its duration
// from length/speedXY, so a 0.01 yd hop is airborne for about 1.4ms and never covers a tick, let
// alone two. A jump long enough to span two ticks is a 7 yd walk, which is what this already does.
//
// Arming at 2 made the shuttle 32.4% of accepted moves; arming at 5 let a bot stand still through 3-5
// stacks, and with ranged no longer standing in fires that was 739 bot-seconds over 0-3:00, 66% of
// Biting Cold damage and two of three deaths in one pull. 4 is the middle.
//
// A bot in a fire never gets here: the check below short-circuits on the aura, since a fire sheds on
// every tick exactly as moving does.
constexpr float ULDUAR_HODIR_SHUTTLE_HALF_LEG = 3.0f;
constexpr uint32 ULDUAR_HODIR_BITING_COLD_SHED_STACKS = 4;

// Standing in Starlight is worth a stack: +50% to every cast and swing against what the tick costs.
constexpr uint32 ULDUAR_HODIR_BITING_COLD_SHED_STACKS_IN_STARLIGHT = 5;
static_assert(ULDUAR_HODIR_BITING_COLD_SHED_STACKS_IN_STARLIGHT <= 6,
              "62039 caps at 8 stacks and 200*2^7 is 25600 a tick, so past 6 the shed arms too late to matter");

// Where a shed stops. The last stack ticks 400 and takes 2s of walking to drop, and standing 4s puts
// it straight back on.
constexpr uint32 ULDUAR_HODIR_BITING_COLD_SHED_FLOOR = 1;
static_assert(ULDUAR_HODIR_BITING_COLD_SHED_FLOOR < ULDUAR_HODIR_BITING_COLD_SHED_STACKS,
              "a shed has to have somewhere to go");

// Legs for a bot shedding inside a landed shelter while Flash Freeze is being cast. Both ends stay
// inside the park ring, so the shelter run never fires on them.
constexpr float ULDUAR_HODIR_SHELTER_SHED_RADIUS = 3.0f;
static_assert(ULDUAR_HODIR_SHELTER_SHED_RADIUS < ULDUAR_HODIR_SAFE_AREA_TOLERANCE,
              "the shelter shuttle has to stay inside the park ring");

// How long a bot may stand still short of a walk it was sent on before the walk is issued again.
// MoveTo refuses the same point as a duplicate for MaxWaitForMove (5s) whether or not the bot is
// still moving, so an item use (StopMoving) or a Disengage leaves it standing where it stopped.
constexpr uint32 ULDUAR_HODIR_STALL_MS = 500;

// A trapped raider dies to the next Flash Freeze 48s later, so freeing them outranks the boss - but
// the block has little health, so only the nearest few bots leave what they were doing.
constexpr float ULDUAR_HODIR_TRAPPED_ALLY_RANGE = 45.0f;
constexpr uint32 ULDUAR_HODIR_TRAPPED_ALLY_BREAKERS = 5;

// A helper block is the same creature but a very different problem: one Flash Freeze freezes every
// helper at once - measured at 8 blocks a cycle, 11 cycles out of 11 - so a per-block cap multiplies
// by that. 8 x 5 slots over 18 eligible bots put every dps on ice, up to 11 of them on one block a
// median 21 yd from the boss. This is the ceiling across every block that is up.
constexpr uint32 ULDUAR_HODIR_HELPER_BLOCK_BREAKERS = 8;

// Breakers per mage block; every other block gets one. A mage casts its first Toasty Fire 6s after
// it is freed, and with one breaker on a 110675 hp block the next fire took 17.9-40.8s (28.5 on
// average) after each freeze. That is longer than Singed's 25s, so Hodir lost all 25 stacks every
// cycle, and the raid ran 238k dps with a fire up against 127k without. Three breakers free it in
// 4.4-11.0s, and the block carries no mechanic, so that time scales with how many are shooting it.
constexpr uint32 ULDUAR_HODIR_MAGE_BLOCK_BREAKERS = 5;

// The ceiling once no mage block is left. Nothing on ice is holding up the next fire then, and the
// druid, shaman and priest behind it are not worth the whole ranged group: 170-176 bot-seconds a pull
// go on ice after the last mage is already free, close to a tenth of all the time the ranged group
// has, and the fire is what the wave was for.
constexpr uint32 ULDUAR_HODIR_LATE_BLOCK_BREAKERS = 3;
static_assert(ULDUAR_HODIR_LATE_BLOCK_BREAKERS <= ULDUAR_HODIR_HELPER_BLOCK_BREAKERS,
              "the wave standing down cannot pull in more bots than it had while a mage was iced");

// Never empty the ranged group for ice. Sized off who is actually there rather than off raid size,
// so eight blocks against a 10-man's three ranged still leaves someone on the boss.
constexpr uint32 ULDUAR_HODIR_HELPER_BLOCK_MIN_FREE = 2;

// Frozen Blows' melee add-on lands 12645-28929 on a 45287 hp tank in 25man - a max roll is 64% of
// the pool - and two arrive about 2.4s apart. Below this, taunting into an open window is a death.
constexpr float ULDUAR_HODIR_TAUNT_HEALTH_FLOOR = 50.0f;

// Lust waits for the first Toasty Fire, but no longer than this into the pull. First fires have come
// 17-26 s in, and a pull with none by 30 s shouldn't sit on a 10 minute cooldown.
constexpr uint32 ULDUAR_HODIR_LUST_FALLBACK_MS = 30000;

constexpr float ULDUAR_HODIR_ROOM_SEARCH_RADIUS = 100.0f;

extern const Position ULDUAR_HODIR_CENTRE;

// Hodir. By entry, not "find target": that value walks only the bot's own threat list, so any bot
// parked on an ice block would stop seeing the boss and silently lose its Flash Freeze shelter.
Unit* GetHodir(PlayerbotAI* botAI);

// Anything that pulls - targeting, the anchors - waits for this instead of mere presence. Hodir
// never calls SetInCombatWithZone, so his flag flips exactly when someone engages him.
bool IsHodirEngaged(PlayerbotAI* botAI);

// True while Hodir is casting Flash Freeze. 61968 is a 9 s cast and the trace measures cast-to-land at
// 9.03 s, so this is the whole window and nothing but it: it opens 3.8 s before the drift lands and
// leaves the shelter behind, and closes exactly when the freeze resolves.
bool IsHodirFlashFreezeIncoming(PlayerbotAI* botAI);

// True while Heroism and Bloodlust wait: he's engaged, no Toasty Fire is up yet, and the pull is under
// ULDUAR_HODIR_LUST_FALLBACK_MS old.
bool IsHodirLustHeld(PlayerbotAI* botAI);

// True while Hodir carries Frozen Blows. Shared so the swap trigger and the taunt guard cannot
// disagree about whether the window is open. The server's spelldifficulty_dbc maps 62478 -> 63512
// for 25man, which the client DBC does not, so the difficulty lookup has to stay.
bool HodirFrozenBlowsActive(PlayerbotAI* botAI, Player* bot);

// True when taunting Hodir right now would kill this bot: Frozen Blows is up, the bot is under
// ULDUAR_HODIR_TAUNT_HEALTH_FLOOR, and somebody else is already holding him. That last clause is the
// rescue valve - with nobody on him the taunt is the save and has to survive.
bool HodirTauntWouldBeSuicide(PlayerbotAI* botAI, Player* bot);

// Where this bot shelters from Flash Freeze: the nearest Snowpacked Icicle Target, or while the cast
// is up the nearest drift that has not landed yet, since the target appears at the drift's own spot.
// Latched per bot until the pick leaves the floor. Nothing here is raid-wide - three drifts land 5-31
// yd apart and every target grants the Safe Area, so sending the raid to one of them bought nothing
// and cost a median 8 yd each way per bot per freeze.
Creature* GetHodirShelter(PlayerbotAI* botAI, Player* bot);

// How close the shelter run parks, and how far the bot may then drift before it is sent back. A drift
// still falling is a 14000 damage blast rather than a shelter, so it is parked on the edge of its
// clear and only stepped into once it has landed. Both pairs keep the same 2 yd of hysteresis, or
// arriving releases the bot on the same tick and the ring anchor walks it straight back out.
float GetHodirShelterPark(Creature* shelter);
float GetHodirShelterRelease(Creature* shelter);

// True when the bot's shelter is a landed Snowpacked Icicle Target, not a drift still falling, and the
// bot stands inside its park ring.
bool IsHodirInLandedShelter(PlayerbotAI* botAI, Player* bot);

// The bot carrying Storm Cloud right now, or nullptr. Swept off the group rather than the grid so a
// carrier standing outside anyone's sweep is still found, and taken as the first carrier in roster
// order so every bot answers the same when the boss has stacked two.
Player* GetHodirStormCloudCarrier(PlayerbotAI* botAI, Player* bot);

// The one point a carry gathers on, shared by the carrier and everyone collecting from it. Storm Power
// is a 3 yd pulse at the carrier's feet with 6 charges, so the raid has to come to the carrier and the
// carrier has to stop moving - two derivations that disagree walk past each other.
//
// Latched per carry on the carrier's aura apply time, because the carrier's own position is the seed
// and it moves. Keyed per instance rather than per bot so a receiver joining late reads the same point
// the carrier already committed to.
bool GetHodirStormCloudRally(PlayerbotAI* botAI, Player* bot, Player* carrier, Position& out);

// Where this bot belongs and how far it may stray. Tanks stand just past the hold point (the centre,
// or a fire within the leash of it) so Hodir stops on it. Ranged and healers get a buff stand -
// Starlight first, a fire otherwise - and nothing at all when neither is inside the walk the bot will
// make for one: no boss mechanic asks them to stand anywhere in particular, and a home spot they get
// walked back to is a home spot they get walked out of the buff for. Melee are unanchored and get
// false. Trigger and action both go through here so they cannot disagree.
bool GetHodirAnchor(PlayerbotAI* botAI, Player* bot, Position& out, float& tolerance);

// The Starlight zone this bot is standing in, or false. The centre, not the bot's own spot, so the
// shuttle can be laid out around it.
bool GetHodirStarlightZoneAt(PlayerbotAI* botAI, Player* bot, Position& out);

// Where this bot walks to shed Biting Cold. Tanks alternate between two points either side of their
// hold spot, across the line he is held on so neither end drags him off it. A bot in a landed shelter while
// Flash Freeze is cast, or in a Starlight zone, shuttles across it so it keeps the cover or the buff.
// Everyone else takes the nearest point clear of the rest of the raid, of every live icicle, and (if
// it shoots from range) of Hodir. Legs are long enough to cover two aura ticks.
bool GetHodirShuttleLeg(PlayerbotAI* botAI, Player* bot, Position& out);

// True when the shed would start walking a bot that is currently standing still: it holds at least
// the stacks the shuttle arms at, and it is not in a fire that sheds them for free. Says nothing
// about a shed already running: the action latches that itself and carries it down to
// ULDUAR_HODIR_BITING_COLD_SHED_FLOOR, which is why HodirBitingColdTrigger still fires on any stack.
//
// Anything that steps aside for the shed has to ask this rather than the trigger. 87% of the time a
// bot holds Biting Cold it holds exactly one stack, 32.8% of the fight each, and in that window the
// shuttle does nothing - so a node that stood down for the trigger stood down for nothing.
bool IsHodirBitingColdShedArmed(Player* bot);

// False once an icicle has already detonated. It lingers another 3.3s after the blast, and treating
// that corpse as live both inflates the hazard set past what any dodge can clear and keeps bots
// walking back and forth over a spot that is already safe.
bool IsHodirIcicleLethal(Creature* icicle);

// True when the straight walk from the bot to dest, both ends included, passes within smallRadius of a
// small icicle or bigRadius of a drift that has not detonated yet. The dodge only steps a bot out of the
// pool; anything that walks it straight back in re-arms the dodge, so movers ask this first.
bool IsHodirWalkThroughLiveIcicle(Player* bot, Position const& dest, float smallRadius, float bigRadius);

// Where every icicle that has not detonated yet is standing, each carrying the distance its own pool
// needs. The drift entry is included because it is lethal on the way down; once it lands it marks the
// shelter and IsHodirIcicleLethal stops reporting it. The two clears are passed in because the dodge
// sweeps twice: once for real margin, once tightened to the radius that actually kills.
std::vector<EncounterHelpers::HazardCircle> CollectHodirIcicleHazards(Player* bot, float radius, float smallClear,
                                                                     float bigClear);

// Which of several equally short spots a bot stepping aside would rather have. Melee want the boss;
// ranged and healers want the buff stand they were pulled off, so a dodge does not throw them out of
// the zone they just walked to. A bot with no anchor gets no preference.
bool GetHodirDodgePreference(PlayerbotAI* botAI, Player* bot, Position& out);

// The paladin that carries Frost Resistance Aura for this fight, or nullptr. Every point of the
// damage that kills this raid is frost, so the aura is worth a paladin's slot - but the slot is
// exclusive, so whoever holds it gives up their own. Prefers a paladin that is neither tanking nor
// healing, because retribution gives up the least; falls back to any paladin that knows the spell.
Player* GetHodirResistancePaladin(PlayerbotAI* botAI, Player* bot);

// True when this bot is one of the ULDUAR_HODIR_TRAPPED_ALLY_BREAKERS non-healers in range of the
// block, ranked by guid. Ranking by distance instead would re-shuffle the set every tick and bots
// would flick between the block and the boss.
bool IsHodirTrappedAllyBreaker(PlayerbotAI* botAI, Player* bot, Unit* block);

// The flash-frozen helper this bot should break, or nullptr. Budgeted across every block that is up
// rather than per block, and drawn from the ranged only - a melee that leaves makes a 21 yd round trip
// for a block a ranged bot can shoot from nearer where it already stands. Asked once per bot rather
// than once per block, because the sweep it needs is not cheap and one Flash Freeze puts up eight.
// Mages first with ULDUAR_HODIR_MAGE_BLOCK_BREAKERS each, then druids, shamans and priests.
Unit* GetHodirAssignedHelperBlock(PlayerbotAI* botAI, Player* bot);

// "mage", "druid", "shaman" or "priest" for a helper's ice block, nullptr for anything else.
char const* GetHodirHelperBlockKind(Unit* block);

#endif
