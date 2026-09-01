/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERVEZAX_H
#define PLAYERBOTS_ULDENCOUNTERVEZAX_H

#include "Position.h"
#include "RaidObs.h"
#include "UldData.h"

#include "ObjectGuid.h"

#include <unordered_map>
#include <vector>

class Player;
class PlayerbotAI;
class Unit;

// General Vezax.
//
// The encounter has two ground effects and they pull in opposite directions. The Shadow Crash field
// (63277) is a buff worth standing in for anything that casts with mana; the puddle a killed
// Saronite Vapor drops (63322) trades health for mana on a doubling curve. Neither is visible to
// AvoidAoeAction, so both are found here: the field is a persistent area aura and therefore a
// DynamicObject, the puddle rides the vapor's corpse.

enum UlduarVezaxIds
{
    // General Vezax
    SPELL_MARK_OF_THE_FACELESS = 63276,
    // 62660 is the cast, 62659 the 10 yd impact, and 63277 the 8 yd field it leaves behind for 20s.
    // Only the field is reactable - the impact resolves the instant the missile lands.
    SPELL_VEZAX_SHADOW_CRASH_CAST = 62660,
    SPELL_VEZAX_SHADOW_CRASH_DMG = 62659,
    SPELL_VEZAX_SHADOW_CRASH_FIELD = 63277,
    SPELL_VEZAX_SEARING_FLAMES = 62661,
    SPELL_VEZAX_SURGE_OF_DARKNESS = 62662,
    SPELL_VEZAX_SARONITE_VAPORS_PUDDLE = 63322,
    // Cast by a dying vapor on itself, so it fires exactly once at the moment the puddle appears.
    SPELL_VEZAX_SARONITE_VAPORS_SPAWN = 63323,
    SPELL_VEZAX_SARONITE_BARRIER = 63364,

    // General Vezax
    NPC_VEZAX = 33271,
    NPC_VEZAX_SARONITE_VAPORS = 33488,
    NPC_VEZAX_SARONITE_ANIMUS = 33524,
};

// Shadow Crash lands as a missile: 62660 is instant with Speed 10, so its destination is fixed at
// cast time and a bot 26 yd out has ~2.6s to leave it. The impact (62659) is 10 yd and knocks back,
// so standing still is not an option either way. 12 yd of travel is about 1.7s and leaves time to
// walk back into the field the missile drops.
constexpr float ULDUAR_VEZAX_SHADOW_CRASH_IMPACT_RADIUS = 10.0f;
constexpr float ULDUAR_VEZAX_SHADOW_CRASH_DODGE_CLEARANCE = 12.0f;
constexpr float ULDUAR_VEZAX_SHADOW_CRASH_DODGE_SEARCH_RADIUS = 25.0f;

// FindNearestPositionClearOfHazards answers with the nearest clear spot, which for a caster standing
// on the impact is as likely to point inward as outward. Ranged keep to the band their blocks live
// in, so a dodge never dumps one in the melee ball. Healers get their own ceiling instead of that
// floor: stepping past 12.5 yd from the boss puts them back in the Shadow Crash target pool, which is
// the whole reason they stand where they do.
constexpr float ULDUAR_VEZAX_DODGE_BAND_MIN = 16.0f;
constexpr float ULDUAR_VEZAX_DODGE_BAND_MAX = 36.0f;
constexpr float ULDUAR_VEZAX_HEALER_DODGE_BAND_MAX = 12.0f;

// Both Vezax ground hazards are 8 yd: the Shadow Crash field (63277) and the puddle a killed
// Saronite Vapor leaves on its corpse (63322). Plus a yard of slack, since a bot that stops exactly
// on the boundary is still taking ticks.
constexpr float ULDUAR_VEZAX_HAZARD_RADIUS = 8.0f;
constexpr float ULDUAR_VEZAX_HAZARD_CLEARANCE = 9.0f;
// Wide enough to cover the whole ranged formation. Callers that only care about what is under
// their own feet pass a tighter radius - the sweep is two grid searches and the raid runs it often.
constexpr float ULDUAR_VEZAX_HAZARD_SEARCH_RADIUS = 60.0f;
constexpr float ULDUAR_VEZAX_HAZARD_LOCAL_SEARCH_RADIUS = 25.0f;

// The field is worth +100% magic damage, +100% cast speed and -70% mana cost for 20s, which is the
// only real answer to Aura of Despair - so mana casters walk into it rather than out of it. Healers
// never do: it also cuts healing done by 75%, which halves their throughput outright.
// Capped travel, or every caster abandons its slot for one 8 yd circle and Shadow Crash catches the
// lot of them next cast.
constexpr float ULDUAR_VEZAX_SHADOW_CRASH_SOAK_MAX_TRAVEL = 15.0f;

// The puddle deals 100 * 2^stacks every 4s and hands back half as mana. Leave once the next tick
// would take this share of current health - a fixed stack cap kills undergeared 10-man healers and
// leaves value on the table for geared 25-man ones.
constexpr float ULDUAR_VEZAX_VAPOR_SOAK_MAX_TICK_HP_PCT = 0.35f;

// Vezax formation. He spawns dead centre of his room facing north (o 1.658) and the raid comes down
// from the north: the trash pack sits at y 109-137, and the only door - 194750 at y 31.5 - is
// DOOR_TYPE_PASSAGE, so it opens when he dies, on the way to Yogg. Everything the raid stands on goes
// north of the anchor, leaving the southern half for the Mark of the Faceless spots. Every radius
// below is navprobe-verified on map 603: the floor is a WMO, flat at Z 342.378, with no holes at any
// bearing inside 45 yd.
constexpr float ULDUAR_VEZAX_ARC_ORIENTATION = 1.5708f;

// Three blocks, not three arcs. A block is a centre bearing, two rows, and three slots per row at a
// fixed chord spacing; each row's arc width is derived from that spacing, so the shape holds at any
// radius.
//
// Ranged take two of them because the field is 8 yd and the impact is 10. At 3.7 yd spacing a group's
// furthest pair sits 7.97 yd apart, so a field landing on any slot covers the other five - and two
// groups 43.8 yd apart cannot both be caught by one impact.
//
// Healers sit at 11.25 because SelectTarget skips anything within 3 yd of Vezax plus *both* combat
// reaches - 3 + 8 + 1.5 = 12.5 - so a healer inside that line is never a Shadow Crash target, and no
// crash then lands nearer the boss than 14.5 yd. Their ring is measured from the boss, not the
// anchor: the exclusion is his, and a ranged pull can leave him yards off his spawn.
constexpr float ULDUAR_VEZAX_RANGED_GROUP_OFFSET = 1.0f;
constexpr float ULDUAR_VEZAX_HEALER_RADIUS = 11.25f;
constexpr float ULDUAR_VEZAX_HEALER_SPACING = 4.5f;
constexpr float ULDUAR_VEZAX_RANGED_NEAR_RADIUS = 24.5f;
constexpr float ULDUAR_VEZAX_RANGED_FAR_RADIUS = 27.5f;
// A thirteenth ranged bot would otherwise get no slot and fall through to the melee de-clump, which
// walks it onto the boss. This row is deliberately 9 yd off the near one, outside the one-field
// guarantee: overflow is somewhere to stand, not somewhere to soak.
constexpr float ULDUAR_VEZAX_RANGED_OVERFLOW_RADIUS = 33.5f;
constexpr float ULDUAR_VEZAX_RANGED_SPACING = 3.7f;
constexpr uint8 ULDUAR_VEZAX_BLOCK_ROW_SLOTS = 3;

// Slot index space, which a RaidObs trace writes as a bare number: [0,6) healers, [6,12) group L,
// [12,18) group R, [18,21) L overflow, [21,24) R overflow. The main tank is not in here - it has
// exactly one holder and comes straight off IsMainTank.
constexpr uint8 ULDUAR_VEZAX_HEALER_SLOTS = 6;
constexpr uint8 ULDUAR_VEZAX_RANGED_GROUP_SLOTS = 6;
constexpr uint8 ULDUAR_VEZAX_RANGED_OVERFLOW_SLOTS = 3;
constexpr uint8 ULDUAR_VEZAX_RANGED_SLOTS =
    2 * (ULDUAR_VEZAX_RANGED_GROUP_SLOTS + ULDUAR_VEZAX_RANGED_OVERFLOW_SLOTS);
constexpr uint8 ULDUAR_VEZAX_TOTAL_SLOTS = ULDUAR_VEZAX_HEALER_SLOTS + ULDUAR_VEZAX_RANGED_SLOTS;

// Per band, because the packing differs. The arrival deadband is twice the tolerance, so the old 2.0
// was wider than the 3.7 yd gap between ranged neighbours. Healers have 2.25 yd of room between the
// melee ring at 10.25 and the target-exclusion line at 12.5, and 0.8 is what keeps them inside both.
constexpr float ULDUAR_VEZAX_SLOT_TOLERANCE = 1.2f;
constexpr float ULDUAR_VEZAX_HEALER_SLOT_TOLERANCE = 0.8f;
constexpr float ULDUAR_VEZAX_TANK_SLOT_TOLERANCE = 3.0f;

// The hall runs 70 yd north and west of the anchor, so this stops well short of any wall. It is not
// meant to reach the entrance: outside it the movement multiplier is inert, so generic movement
// carries a bot in and the gate opens on arrival. Widening it is what would put a bot on a path
// through a wall, which is why it stays where it is.
constexpr float ULDUAR_VEZAX_ARENA_RADIUS = 45.0f;
constexpr float ULDUAR_VEZAX_ARENA_HEIGHT = 10.0f;

// Melee and the tank hold the boss rather than take slots, so all they get is a nudge apart.
constexpr float ULDUAR_VEZAX_MELEE_DECLUMP_RADIUS = 4.0f;

// Mark of the Faceless drains 5000/s from every ally within 15 yd and heals Vezax for it. Ranged step
// straight outward along their own bearing, an 18 yd walk rather than the 40-odd it takes to reach
// the far side of the room - the debuff lasts 10s and travel is the whole cost of the mechanic.
// Capped inside the arena bubble: past it the formation gate goes false, the movement multiplier
// hands the generic movers back, and the bot wanders instead of coming home.
//
// Everyone else keeps the three fixed spots behind the boss. The core only marks someone inside 15 yd
// when fewer than 9 (25m) / 4 (10m) players are further out, and the twelve ranged always clear that
// bar, so that path is the corner case rather than the common one.
constexpr float ULDUAR_VEZAX_MARK_SEPARATION = 18.0f;
constexpr float ULDUAR_VEZAX_MARK_MAX_RADIUS = 44.0f;
constexpr float ULDUAR_VEZAX_MARK_SPOT_RADIUS = 26.0f;
constexpr float ULDUAR_VEZAX_MARK_SPOT_ARC_OFFSET = 2.3208f;  // pi/2 + 0.75, clear of the blocks
constexpr float ULDUAR_VEZAX_MARK_SPOT_TOLERANCE = 3.0f;
constexpr uint8 ULDUAR_VEZAX_MARK_SPOT_COUNT = 3;

// The handler is whoever the mana actually helps. The puddle trades health on a 100 * 2^stacks curve
// for half of it back as mana, which is worth paying at 10% and not at 80% - so nobody low means
// nobody kills, and the vapor despawns on its own. Healers rank first among those who qualify.
constexpr uint8 ULDUAR_VEZAX_VAPOR_HANDLERS = 2;
constexpr uint8 ULDUAR_VEZAX_VAPOR_HANDLER_MANA_PCT = 10;
// Close to the vapor before killing it, or the puddle drops wherever the bot happened to be standing
// and reaching it costs the walk below instead of nothing.
constexpr float ULDUAR_VEZAX_VAPOR_KILL_RANGE = 5.0f;
constexpr float ULDUAR_VEZAX_VAPOR_SOAK_MAX_TRAVEL = 25.0f;

// Vezax' own spawn point, and the point the Saronite Vapors charge to when they merge.
extern const Position ULDUAR_VEZAX_ANCHOR;

struct VezaxHazard
{
    Position position;
    float radius = ULDUAR_VEZAX_HAZARD_RADIUS;
    bool isShadowCrashField = false;
};

struct VezaxEncounterTargets
{
    Unit* vezax = nullptr;
    Unit* animus = nullptr;
    std::vector<Unit*> liveVapors;
};

// Slots are held, not re-derived. Ranking the raid by guid every tick means one death renumbers
// everyone behind the corpse and the whole formation shuffles mid-fight, so an assignment is kept
// until its holder is gone. The index space is laid out above next to the radii:
// [0,6) healers, [6,12) group L, [12,18) group R, [18,21) L overflow, [21,24) R overflow.
struct VezaxEncounterState
{
    RaidObs::ObsGuidMap<uint8> slotAssignments{"vezax.slot"};
    // Where a bot stands while its own slot is buried under a hazard. Cleared as soon as the real
    // slot is clear again.
    RaidObs::ObsGuidMap<uint8> displacedAssignments{"vezax.displaced"};
};

extern std::unordered_map<uint32 /*instanceId*/, VezaxEncounterState> vezaxEncounterStates;

// From the instance script rather than a target sweep. "find target" walks only the bot's own threat
// list, so a bot that switched to the Animus or a vapor would stop seeing the boss; the entry sweep
// that replaced it recalculates a 100 yd search on every call, and the movement multiplier asks once
// per action per pass.
Unit* GetVezax(PlayerbotAI* botAI);
bool VezaxEncounterActive(PlayerbotAI* botAI);

// Gate for the formation and the movement multiplier. Presence alone is not enough on this boss:
// he is visible from outside his hall, so a presence gate sends bots walking into walls before the
// pull and takes their generic movers away while they do it.
bool VezaxFormationActive(PlayerbotAI* botAI);

void GatherVezaxEncounterTargets(PlayerbotAI* botAI, VezaxEncounterTargets& targets);

// One sweep per action execution - GetDynamicObjectPositions is a grid search, and repeating it per
// hazard test across a 25-man raid is the per-tick cost the raid-mechanics notes warn about. Callers
// gather once and pass the vector to every check below.
void GatherVezaxHazards(Player* bot, std::vector<VezaxHazard>& hazards,
                        float searchRadius = ULDUAR_VEZAX_HAZARD_SEARCH_RADIUS);

bool TryGetVezaxNearestHazard(Player* bot, std::vector<VezaxHazard> const& hazards,
                              bool wantShadowCrashField, VezaxHazard& hazard);
// Takes the already role-filtered avoid list from VezaxBuildAvoidPositions, not the raw hazards:
// what counts as dangerous depends on who is asking.
bool IsVezaxSpotSafe(Position const& spot, std::vector<Position> const& avoid, float clearance);

// Anything with a mana bar that is not a healer. The field's -70% cost is MOD_POWER_COST_SCHOOL_PCT
// with a school mask of 127, so it covers physical too and a hunter's shots get it as readily as a
// mage's bolts - only the damage half is magic-only. Healers stay out: it cuts healing done by 75%.
bool VezaxCanSoakShadowCrashField(Player* bot);

// Only healers are actually hurt by the field. Melee and tanks gain nothing from it either, but
// nothing in it damages them, so walking them out would spend uptime to avoid a buff they cannot use.
bool VezaxMustLeaveShadowCrashField(Player* bot);

// Anything with a mana bar that is not already full. Everyone else takes the puddle's doubling
// damage for no return and should be out of it from the first tick.
bool VezaxWantsVaporPuddleMana(Player* bot);

// Who may stand in a vapor puddle at all. Vapors spawn on the boss and wander 4 yd, so an 8 yd puddle
// covers the tank, the melee and most of the healer ring: ungated, one puddle puts 100 * 2^stacks on
// thirteen bots. The handlers went and made it; anyone else has to be low enough to need it.
bool VezaxMayStandInVaporPuddle(Player* bot);

// The puddle deals 100 * 2^stacks every 4s. Leaving is a prediction about the next tick, not a
// stack count: the same cap kills a 10-man healer and wastes mana for a geared 25-man one.
bool VezaxShouldLeaveVaporPuddle(Player* bot);

// What this particular bot has to stay out of. A mana caster wants to be standing in a Shadow Crash
// field, so for it only the vapor puddles count - and only those it is not entitled to.
void VezaxBuildAvoidPositions(Player* bot, std::vector<VezaxHazard> const& hazards,
                              std::vector<Position>& avoid);

// Takes the bot because the healer ring is centred on the boss, not the anchor.
bool TryGetVezaxSlotPosition(Player* bot, uint8 slotIndex, Position& position);
// Ranged pack at 3.7 yd, healers hold a 2.25 yd band and the tank only has to be on the anchor, so
// one tolerance cannot serve all three.
float VezaxSlotTolerance(Player* bot);
void EnsureVezaxSlotAssignments(Player* bot);

// The spot this bot should be standing on, hazards taken into account. False for melee, who hold the
// boss instead; the main tank is answered here too, from the anchor, because a tank standing on
// Vezax's spawn is what keeps him there for everyone else's radii.
bool TryGetVezaxSlot(Player* bot, std::vector<VezaxHazard> const& hazards, Position& position);

bool TryGetVezaxMarkSpot(Player* bot, Position& position);

// Where the missile now in flight will land, or false when none is. Instant cast plus Speed 10 means
// the boss is never in UNIT_STATE_CASTING for it - the delayed spell is what stays current, and its
// destination was frozen when it went out, which is what makes the thing dodgeable at all.
bool TryGetVezaxShadowCrashImpact(PlayerbotAI* botAI, Position& impact);

// Somewhere clear of that impact and still inside the band this bot's role is allowed to stand in.
bool TryGetVezaxDodgeSpot(Player* bot, Position const& impact, Position& spot);

// Drop this instance's assignments once Vezax is gone, or they survive into the next pull and bots
// walk to slots nobody is standing in.
void ResetVezaxEncounterState(Player* bot, bool clearInstance);

// The interrupt this bot could land on target right now, or nullptr. Stuns are deliberately absent:
// Vezax is a boss and immune to them, so bash and hammer of justice would never connect.
char const* VezaxReadyInterrupt(Player* bot, Unit* target);

// One kick per Searing Flames. The cast is 2s on an 8s cadence in 25-man, so three bots answering the
// same one leaves the next unanswered - and that one is 13875-16125 to the whole raid plus 75% of the
// tank's armour. Lowest guid among the bots whose interrupt is actually off cooldown wins, which
// rotates the duty for free as cooldowns come and go.
bool VezaxIsSearingFlamesInterrupter(Player* bot, Unit* boss);

// Whoever the puddle's mana would actually help: non-tanks under 10%, healers ranked first, two of
// them. Nobody low means nobody kills, and the vapor despawns on its own - which is the right answer,
// because the puddle is paid for in health.
bool VezaxIsVaporHandler(Player* bot);

#endif
