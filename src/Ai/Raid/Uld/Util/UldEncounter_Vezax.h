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
// so standing still is not an option either way.
constexpr float ULDUAR_VEZAX_SHADOW_CRASH_IMPACT_RADIUS = 10.0f;
constexpr float ULDUAR_VEZAX_SHADOW_CRASH_DODGE_CLEARANCE = 12.0f;
constexpr float ULDUAR_VEZAX_SHADOW_CRASH_DODGE_SEARCH_RADIUS = 25.0f;

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
// only real answer to Aura of Despair - so mana casters walk into it rather than out of it. Only the
// damage and healing halves are on 63277 itself; the cast speed and mana cost ride 65269, linked to
// it through spell_linked_spell, so reading the DBC row alone says the field does nothing for mana.
//
// Healers never travel to one: it also cuts healing done by 75%, so a healer in a field heals for
// 0.25x per cast and 0.83x per point of mana. They stand in whichever one lands on the camp because
// the camp is where crashes land, not because it is worth walking to.
// Capped travel, or every caster abandons its slot for one 8 yd circle and Shadow Crash catches the
// lot of them next cast.
constexpr float ULDUAR_VEZAX_SHADOW_CRASH_SOAK_MAX_TRAVEL = 15.0f;

// 63323 re-applies the puddle aura every 2s, and each re-apply deals 100 * 2^stacks and hands back
// half of it as mana. Leave once the next tick would take this share of current health - a fixed
// stack cap kills undergeared 10-man healers and leaves value on the table for geared 25-man ones.
constexpr float ULDUAR_VEZAX_VAPOR_SOAK_MAX_TICK_HP_PCT = 0.35f;

// Vezax formation. He spawns dead centre of his room facing north (o 1.658) and the raid comes down
// from the north: the trash pack sits at y 109-137, and the only door - 194750 at y 31.5 - is
// DOOR_TYPE_PASSAGE, so it opens when he dies, on the way to Yogg. Everything the raid stands on goes
// north of the anchor, leaving the southern half for the Mark of the Faceless spots. Every radius
// below is navprobe-verified on map 603: the floor is a WMO with no holes at any bearing inside
// 45 yd. It is flat at Z 342.378 out to about 24 yd north and then settles half a yard lower on WMO
// rubble, which UpdateAllowedPositionZ absorbs - the camp spans that seam and nothing there is off
// mesh.
constexpr float ULDUAR_VEZAX_ARC_ORIENTATION = 1.5708f;

// One camp for every healer and ranged bot, split into two groups that dodge in opposite directions.
// A group is two files of five: files are the tangential axis, rows the radial one, and both are
// measured from the boss so a ranged pull that leaves him off his spawn carries the whole camp with
// him.
//
// The files are what set the strafe distance. Everyone in a group moves by the same vector, so the
// member on the far side of the target has to cross the impact and ends up STRAFE minus the group's
// own width away from it. 15 - 3 = 12 yd against a 10 yd impact, the same 2 yd of margin the search
// dodge aimed for. Widen the files and the strafe has to grow with them.
//
// The near row is what the flight time binds, and only the near row: the missile covers boss to
// impact at 10 yd/s, so a front rank at 22 yd gives 2.2s. The worst bot is the inner-file one of the
// group that got hit, which has to cross past the impact and needs 13 yd - 1.9s at run speed - to be
// clear of it. That leaves about a third of a second, and every row behind the first has more.
// Pulling the camp inward is what spends it; the strafe distance does not, because a bot is out of
// the blast long before it finishes walking.
constexpr float ULDUAR_VEZAX_CAMP_RADIUS = 28.0f;
constexpr float ULDUAR_VEZAX_CAMP_ROW_SPACING = 3.0f;
constexpr float ULDUAR_VEZAX_CAMP_FILE_SPACING = 3.0f;
// Half the gap between the two groups' inner files. 4.0 puts them 5 yd apart, close enough that one
// set of heals and one Bloodlust covers the camp, far enough that they are not one clump.
constexpr float ULDUAR_VEZAX_CAMP_GROUP_OFFSET = 4.0f;
constexpr float ULDUAR_VEZAX_CAMP_STRAFE = 15.0f;
constexpr uint8 ULDUAR_VEZAX_CAMP_ROWS = 5;
constexpr uint8 ULDUAR_VEZAX_CAMP_FILES = 2;

// Slot index space, which a RaidObs trace writes as a bare number: [0,10) group L, [10,20) group R.
// The main tank is not in here - it has exactly one holder and comes straight off IsMainTank.
constexpr uint8 ULDUAR_VEZAX_GROUP_SLOTS = ULDUAR_VEZAX_CAMP_ROWS * ULDUAR_VEZAX_CAMP_FILES;
constexpr uint8 ULDUAR_VEZAX_TOTAL_SLOTS = 2 * ULDUAR_VEZAX_GROUP_SLOTS;

// The arrival deadband is twice the tolerance, so anything at or above 1.5 would be wider than the
// 3 yd gap between neighbours and let a bot settle on someone else's slot.
constexpr float ULDUAR_VEZAX_SLOT_TOLERANCE = 1.2f;
constexpr float ULDUAR_VEZAX_TANK_SLOT_TOLERANCE = 3.0f;

// The hall runs 70 yd north and west of the anchor, so this stops well short of any wall. It is not
// meant to reach the entrance: outside it the movement multiplier is inert, so generic movement
// carries a bot in and the gate opens on arrival. Widening it is what would put a bot on a path
// through a wall, which is why it stays where it is.
constexpr float ULDUAR_VEZAX_ARENA_RADIUS = 45.0f;
constexpr float ULDUAR_VEZAX_ARENA_HEIGHT = 10.0f;

// Melee and the tank hold the boss rather than take slots, so all they get is a nudge apart.
constexpr float ULDUAR_VEZAX_MELEE_DECLUMP_RADIUS = 4.0f;

// Mark of the Faceless drains 5000/s from every ally within 15 yd and heals Vezax for it, which
// makes it the one mechanic a single camp cannot absorb: the whole camp is inside 15 yd of anyone in
// it. The marked bot leaves for one of three fixed spots south of the boss, opposite the camp - a
// step outward along its own bearing is not enough, because 18 yd from the front row lands 6 yd short
// of the back row and drains it anyway.
constexpr float ULDUAR_VEZAX_MARK_SPOT_RADIUS = 26.0f;
constexpr float ULDUAR_VEZAX_MARK_SPOT_ARC_OFFSET = 2.3208f;  // pi/2 + 0.75, clear of the camp
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
// until its holder is gone. The index space is laid out above next to the radii: [0,10) group L,
// [10,20) group R, packed row-major inside each so the low bit is the file.
struct VezaxEncounterState
{
    RaidObs::ObsGuidMap<uint8> slotAssignments{"vezax.slot"};
    // Where a bot stands while its own slot is buried under a vapor puddle. Cleared as soon as the
    // real slot is clear again. A Shadow Crash field never puts anyone here - the camp stands in
    // those - so this is rare, and a bot dodges from its assigned slot rather than this one.
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
// mage's bolts - only the damage half is magic-only. Healers stay out of this list: they stand in
// whichever field lands on the camp, but the -75% healing means one is never worth walking to.
bool VezaxCanSoakShadowCrashField(Player* bot);

// Anything with a mana bar that is not already full. Everyone else takes the puddle's doubling
// damage for no return and should be out of it from the first tick.
bool VezaxWantsVaporPuddleMana(Player* bot);

// Who may stand in a vapor puddle at all. A vapor spawns at a random point 45 yd out from Vezax and
// wanders 4 yd from there on a NullCreatureAI, so it never comes to the raid and cannot be dragged:
// the puddle lands where it died and the raid walks to it. The handlers went and made it; anyone else
// has to be low enough to need it.
bool VezaxMayStandInVaporPuddle(Player* bot);

// The puddle deals 100 * 2^stacks every 2s. Leaving is a prediction about the next tick, not a
// stack count: the same cap kills a 10-man healer and wastes mana for a geared 25-man one.
bool VezaxShouldLeaveVaporPuddle(Player* bot);

// What this particular bot has to stay out of. Nobody avoids a Shadow Crash field - the camp is
// where they land and the raid stands in them - so this is vapor puddles, and only those the bot is
// not entitled to.
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
