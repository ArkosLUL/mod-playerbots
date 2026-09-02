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
    // Starlight is an 8 yd zone centred on whichever druid helper this raid's faction and size got.
    NPC_HODIR_DRUID_ALLIANCE_10 = 32901,
    NPC_HODIR_DRUID_ALLIANCE_25 = 33325,
    NPC_HODIR_DRUID_HORDE_10 = 32941,
    NPC_HODIR_DRUID_HORDE_25 = 33333,
    SPELL_FLASH_FREEZE = 61968,
    SPELL_BITING_COLD_PLAYER_AURA = 62039,
    SPELL_HODIR_FLASH_FREEZE_TRAPPED = 61969,
    SPELL_HODIR_STARLIGHT = 62807,
    SPELL_HODIR_TOASTY_FIRE_AURA = 62821,
    SPELL_HODIR_FROZEN_BLOWS = 62478,  // base id, difficulty-mapped at runtime
    SPELL_HODIR_STORM_CLOUD = 65123,   // base id, difficulty-mapped at runtime
    SPELL_HODIR_STORM_POWER = 63711,   // what the carrier hands out; also difficulty-mapped
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

// Storm Power lands on allies within 3 yd of the carrier, and the carrier only has 4 (10man) / 6
// (25man) one-second ticks to spend, so it tours the ring rather than searching for a cluster.
constexpr float ULDUAR_HODIR_STORM_CLOUD_STACK_RADIUS = 3.0f;

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

// Slots are laid out concentrically at a minimum separation of 4.5 yd, because 62457 splashes 4, so
// one icicle catches one bot instead of five. The ring is sized to fit inside a Toasty Fire - a slot
// outside one is a bot walking a Biting Cold shuttle instead of standing still and casting - but it
// only ever sits in a fire that lands inside the caster band, and this mage drops them 33 to 43 yd
// from Hodir, so in practice the ring forms on the fixed anchor and pays the shuttle.
//
// The outer ring sits 4.5 yd beyond the inner one so a bot shedding Biting Cold can step outward
// without closing on its neighbours.
constexpr float ULDUAR_HODIR_RAID_RING_INNER = 4.5f;
constexpr float ULDUAR_HODIR_RAID_RING_OUTER = 9.0f;
constexpr uint32 ULDUAR_HODIR_RAID_RING_INNER_SLOTS = 6;

// Arrival tolerance doubles as the re-anchor threshold.
constexpr float ULDUAR_HODIR_RING_SPOT_TOLERANCE = 2.0f;
constexpr float ULDUAR_HODIR_MAINTANK_SPOT_TOLERANCE = 3.0f;
// Exact at 9 + 2 = 11, so a bot sitting at the far edge of its tolerance is on the fire's boundary.
static_assert(ULDUAR_HODIR_RAID_RING_OUTER + ULDUAR_HODIR_RING_SPOT_TOLERANCE <=
                  ULDUAR_HODIR_TOASTY_FIRE_RADIUS,
              "the outer ring plus its arrival tolerance has to stay inside a Toasty Fire");
static_assert(ULDUAR_HODIR_RAID_RING_OUTER - ULDUAR_HODIR_RAID_RING_INNER > ULDUAR_HODIR_ICE_SHARDS_RADIUS,
              "the two rings have to sit more than one Ice Shards radius apart");

// Measured against Hodir himself, not the tank spot, which he leaves: he drifted 10-25 yd off it and
// a fixed-point gate let the centre land 6.8 yd from him with a 4.5 yd inner ring. The gap does not
// have to clear the whole ring - a slot that still lands close to him simply never gets walked to,
// because the position trigger checks that the slot is clear before it fires.
constexpr float ULDUAR_HODIR_CENTRE_MIN_BOSS_GAP = 15.0f;

// The far end of the same band: how far from Hodir a caster may stand and still reach him. A Shadow
// Bolt is 30 and that is the shortest range in the raid. Both the fire and the Starlight zone are
// tested against this, because both are reasons to stand somewhere other than the ring slot, and a
// spot that cannot reach the boss is worth nothing whatever else it gives.
//
// Picking fires off the fixed anchor instead let the centre drift to a p75 of 29 yd from him and a
// max of 52, with the ring's far side 45 out and the casters walking a reach spell back in.
constexpr float ULDUAR_HODIR_CASTER_MAX_BOSS_GAP = 30.0f;
static_assert(ULDUAR_HODIR_CENTRE_MIN_BOSS_GAP < ULDUAR_HODIR_CASTER_MAX_BOSS_GAP,
              "the caster band has to have room between its ends");

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

// Ranged and healers hold at least this far from Hodir. His combat reach plus a raider's is roughly
// 13 yd, and Frozen Blows turns one of his swings into 20000-30000, so a caster inside this is one
// swing from dead whether or not it has aggro.
constexpr float ULDUAR_HODIR_RANGED_MIN_BOSS_GAP = 15.0f;

// How far a bot may drift from its slot before it is walked home regardless of anything else. The
// slot is not a restoring force any more, so without a hard leash a bot that stepped out for one
// dodge after another ends up out of heal range with nothing pulling it back.
constexpr float ULDUAR_HODIR_RETURN_LEASH = 20.0f;

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
// Arming at 2 stacks costs ~33% movement duty and ~600/s; arming at 1 would cost half the raid's cast
// uptime for 200/s less.
constexpr float ULDUAR_HODIR_SHUTTLE_HALF_LEG = 3.0f;
constexpr float ULDUAR_HODIR_SHUTTLE_BEARING = -0.785398f;  // -pi/4, parallel to the SW bevel
constexpr uint32 ULDUAR_HODIR_BITING_COLD_SHED_STACKS = 2;

// Standing in Starlight is worth a stack: +50% to every cast and swing against 1600 a tick. Only 36
// of 3298 stack applications in a six minute kill ever reached 3, so this mostly just stops the shed
// interrupting a zone rather than actually letting stacks run.
constexpr uint32 ULDUAR_HODIR_BITING_COLD_SHED_STACKS_IN_STARLIGHT = 3;

// A trapped raider dies to the next Flash Freeze 48s later, so freeing them outranks the boss - but
// the block has little health, so only the nearest few bots leave what they were doing.
constexpr float ULDUAR_HODIR_TRAPPED_ALLY_RANGE = 45.0f;
constexpr uint32 ULDUAR_HODIR_TRAPPED_ALLY_BREAKERS = 5;

// A helper block is the same creature but a very different problem: one Flash Freeze freezes every
// helper at once - measured at 8 blocks a cycle, 11 cycles out of 11 - so a per-block cap multiplies
// by that. 8 x 5 slots over 18 eligible bots put every dps on ice, up to 11 of them on one block a
// median 21 yd from the boss. This is the ceiling across every block that is up, one breaker each.
constexpr uint32 ULDUAR_HODIR_HELPER_BLOCK_BREAKERS = 8;

// Never empty the ranged group for ice. Sized off who is actually there rather than off raid size,
// so eight blocks against a 10-man's three ranged still leaves someone on the boss.
constexpr uint32 ULDUAR_HODIR_HELPER_BLOCK_MIN_FREE = 2;

// Frozen Blows' melee add-on lands 12645-28929 on a 45287 hp tank in 25man - a max roll is 64% of
// the pool - and two arrive about 2.4s apart. Below this, taunting into an open window is a death.
constexpr float ULDUAR_HODIR_TAUNT_HEALTH_FLOOR = 50.0f;

constexpr float ULDUAR_HODIR_ROOM_SEARCH_RADIUS = 100.0f;

extern const Position ULDUAR_HODIR_MAINTANK_SPOT;
extern const Position ULDUAR_HODIR_OFFTANK_SPOT;
extern const Position ULDUAR_HODIR_RAID_ANCHOR;

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

// True while Hodir carries Frozen Blows. Shared so the swap trigger and the taunt guard cannot
// disagree about whether the window is open. The server's spelldifficulty_dbc maps 62478 -> 63512
// for 25man, which the client DBC does not, so the difficulty lookup has to stay.
bool HodirFrozenBlowsActive(PlayerbotAI* botAI, Player* bot);

// True when taunting Hodir right now would kill this bot: Frozen Blows is up, the bot is under
// ULDUAR_HODIR_TAUNT_HEALTH_FLOOR, and somebody else is already holding him. That last clause is the
// rescue valve - with nobody on him the taunt is the save and has to survive.
bool HodirTauntWouldBeSuicide(PlayerbotAI* botAI, Player* bot);

// The Snowpacked Icicle Target the whole raid shelters at during Flash Freeze.
Creature* GetHodirSharedShelter(PlayerbotAI* botAI, Player* bot);

// The Toasty Fire the ranged formation forms on, or nullptr. Picked nearest Hodir rather than nearest
// the bot, for the same reason the shelter is: two derivations of "which fire" disagree and the
// formation oscillates between them. Only a fire that leaves the whole ring inside the caster band
// qualifies, so on most pulls there is none and this returns nullptr all fight.
Creature* GetHodirRaidFire(PlayerbotAI* botAI, Player* bot);

// Where the ranged formation is centred: a Toasty Fire when one sits inside the caster band,
// otherwise ULDUAR_HODIR_RAID_ANCHOR. The fire is what the ring wants to be inside - it stops Biting
// Cold, which is the difference between standing still and walking a shuttle. Deriving it fresh on
// every call is deliberate: a cached centre shared across the raid but validated against one bot's
// own state gets rewritten by whichever bot has stepped out, and the formation thrashes.
//
// onFire says whether the centre is a fire rather than the anchor. Callers need it because the rules
// that only hold inside a fire cannot be told from the position alone, and asking again would cost a
// second grid sweep.
Position GetHodirRingCentre(PlayerbotAI* botAI, Player* bot, bool* onFire = nullptr);

// Where this bot belongs and how far it may stray. Tanks get their fixed corner spots; ranged and
// healers get a formation slot, swapped for a spot inside a Starlight zone whenever one has landed
// within reach of that slot. Melee are unanchored and get false. Trigger and action both go through
// here so they cannot disagree.
bool GetHodirAnchor(PlayerbotAI* botAI, Player* bot, Position& out, float& tolerance);

// This bot's formation slot: centre, then an inner ring, then an outer ring, all inside the fire.
// Ranged dps are ranked ahead of healers and ties break on guid, so every bot derives the same layout
// without sharing state. The raw point is validated against the ground and the collision mesh before
// it is returned - MoveTo rejects an off-mesh destination silently.
bool GetHodirRingSlot(PlayerbotAI* botAI, Player* bot, Position const& centre, Position& out);

// The Starlight zone this bot is standing in, or false. The centre, not the bot's own spot, so the
// shuttle can be laid out around it.
bool GetHodirStarlightZoneAt(PlayerbotAI* botAI, Player* bot, Position& out);

// Where this bot walks to shed Biting Cold when jumping on the spot has not shed it. Tanks alternate
// between two fixed points beside their spot so the boss cannot be walked out of the corner; a bot in
// a Starlight zone shuttles across the zone so it keeps the aura; everyone else takes the nearest
// point clear of the rest of the raid, of every live icicle, and - if it has to shoot from range - of
// Hodir. Legs are long enough to cover two aura ticks.
bool GetHodirShuttleLeg(PlayerbotAI* botAI, Player* bot, Position& out);

// True when the shed would start walking a bot that is currently standing still: it holds at least
// the stacks the shuttle arms at, and it is not in a fire that sheds them for free. Says nothing
// about a shed already running - the action latches that itself and carries it down to zero, which
// is why HodirBitingColdTrigger still fires on any stack.
//
// Anything that steps aside for the shed has to ask this rather than the trigger. 87% of the time a
// bot holds Biting Cold it holds exactly one stack, 32.8% of the fight each, and in that window the
// shuttle does nothing - so a node that stood down for the trigger stood down for nothing.
bool IsHodirBitingColdShedArmed(Player* bot);

// False once an icicle has already detonated. It lingers another 3.3s after the blast, and treating
// that corpse as live both inflates the hazard set past what any dodge can clear and keeps bots
// walking back and forth over a spot that is already safe.
bool IsHodirIcicleLethal(Creature* icicle);

// Where every icicle that has not detonated yet is standing, each carrying the distance its own pool
// needs. The drift entry is included because it is lethal on the way down; once it lands it marks the
// shelter and IsHodirIcicleLethal stops reporting it. The two clears are passed in because the dodge
// sweeps twice: once for real margin, once tightened to the radius that actually kills.
std::vector<EncounterHelpers::HazardCircle> CollectHodirIcicleHazards(Player* bot, float radius, float smallClear,
                                                                     float bigClear);

// Which of several equally short spots a bot stepping aside would rather have. Melee want the boss;
// ranged and healers want the ring slot they were pulled off. A bot with no anchor gets no preference,
// which is the old behaviour.
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
Unit* GetHodirAssignedHelperBlock(PlayerbotAI* botAI, Player* bot);

#endif
