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
// The Shadow Crash field (63277) is a buff worth standing in for anything that casts with mana, and
// AvoidAoeAction cannot see it, so it is found here: a persistent area aura, and therefore a
// DynamicObject.
//
// Saronite Vapors are deliberately ignored. A puddle only ever drops from `JustDied`, never from the
// script's own despawn, so a raid that does not shoot them never makes one - and killing one calls
// DoAction(1) on the boss, which destroys hard mode.

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
    SPELL_VEZAX_SARONITE_BARRIER = 63364,

    // General Vezax
    NPC_VEZAX = 33271,
    NPC_VEZAX_SARONITE_ANIMUS = 33524,
};

// Shadow Crash lands as a missile: 62660 is instant with Speed 10, so its destination is fixed at
// cast time and a bot 26 yd out has ~2.6s to leave it. The impact (62659) is 10 yd and knocks back,
// so standing still is not an option either way.
constexpr float ULDUAR_VEZAX_SHADOW_CRASH_IMPACT_RADIUS = 10.0f;
constexpr float ULDUAR_VEZAX_SHADOW_CRASH_DODGE_CLEARANCE = 12.0f;
constexpr float ULDUAR_VEZAX_SHADOW_CRASH_DODGE_SEARCH_RADIUS = 25.0f;

// The Shadow Crash field (63277) is 8 yd, plus a yard of slack since a bot that stops exactly on the
// boundary is still inside it.
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
// Two limits bracket the radii, and neither is visible from here without the numbers behind it:
//
//   Floor, 24.5 yd. Mark of the Faceless picks at random from everyone the boss measures beyond
//   15 yd, and falls back to everyone inside it when fewer than 9 qualify (4 in 10-man).
//   WorldObject::GetDistance subtracts both combat reaches and Vezax's is 8.0, so his "15 yd" is
//   24.5 yd of real distance. A camp inside that shares a pool with the melee ball and hands the
//   mark to the tank, whose 9 neighbours then leech 5000 a second each into the boss at 20x.
//
//   Ceiling, 38 yd. ReachTargetAction tests IsWithinCombatRange(target, spellDistance), which adds
//   the same two reaches to AiPlayerbot.SpellDistance of 28.5. Past that "reach spell" fires at
//   ACTION_HIGH against this formation at ACTION_RAID and the two deadlock.
//
// Rows 27 to 36 sit in the middle of that band with ~2 yd of slack at each end for the boss drifting
// off his spawn.
//
// The files set the strafe distance. Everyone in a group moves by the same vector, so the member on
// the far side of the target has to cross the impact and ends up STRAFE minus the group's own width
// away from it: 15 - 3 = 12 yd against a 10 yd impact. Widen the files and the strafe has to grow
// with them. Flight time is no longer the binding constraint it was at 22 yd - the missile covers
// boss to impact at 10 yd/s, so the front rank now gets 2.7s for a walk that needs 1.9s.
constexpr float ULDUAR_VEZAX_CAMP_RADIUS = 31.5f;
constexpr float ULDUAR_VEZAX_CAMP_ROW_SPACING = 2.25f;
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

// Mark of the Faceless ticks ten times over 10s on a 40s cadence. Every tick the boss casts 63278 at
// the marked bot and leeches 5000 from everyone else inside the radius, healing himself 20x what it
// takes - EffectMultipleValue on the HEALTH_LEECH effect, applied in Spell.cpp. It is the largest
// single thing in the fight: one untreated window put 8% of a 27.6M boss back.
//
// The marked bot is the one person its own leech skips, so nobody has to outrun their own mark -
// they have to leave everyone else. The radius reads 15 in the DBC but the area test is
// IsWithinDist3d, which adds the target's own combat reach, so the real figure is about 16.5.
constexpr float ULDUAR_VEZAX_MARK_LEECH_RADIUS = 15.0f;
constexpr float ULDUAR_VEZAX_MARK_BREAK_DISTANCE = 18.0f;

// A marked camp member steps sideways on its own group's side, never across the boss: the southern
// spots are a diameter away and the walk there drags the leech through the melee ball. 24 is a floor
// rather than a preference - the nearest slot on that side sits at tangential 5.5, so anything under
// 22 leaves a neighbour inside the leech. The camp's own width sets it.
constexpr float ULDUAR_VEZAX_MARK_SIDE_OFFSET = 24.0f;

// A marked melee has no camp side to step to, so it takes one of three spots south of the boss,
// opposite the camp and 50+ yd from its near row.
constexpr float ULDUAR_VEZAX_MARK_SPOT_RADIUS = 26.0f;
constexpr float ULDUAR_VEZAX_MARK_SPOT_ARC_OFFSET = 2.3208f;  // pi/2 + 0.75, clear of the camp
constexpr float ULDUAR_VEZAX_MARK_SPOT_TOLERANCE = 3.0f;
constexpr uint8 ULDUAR_VEZAX_MARK_SPOT_COUNT = 3;

// Aura of Despair stops mana regeneration, so a warlock here really does need Life Tap - but only
// when it has something to buy. Left alone it taps on cooldown at full mana: one traced pull had both
// locks casting it every 1.25s for the whole fight, paying 2000 health a time for mana they never
// spent, and both died of it having dealt a tenth of what the comparable caster did.
constexpr uint8 ULDUAR_VEZAX_LIFE_TAP_MANA_PCT = 60;

// Vezax' own spawn point, and the point the Saronite Vapors charge to when they merge.
extern const Position ULDUAR_VEZAX_ANCHOR;

struct VezaxHazard
{
    Position position;
    float radius = ULDUAR_VEZAX_HAZARD_RADIUS;
};

// Slots are held, not re-derived. Ranking the raid by guid every tick means one death renumbers
// everyone behind the corpse and the whole formation shuffles mid-fight, so an assignment is kept
// until its holder is gone. The index space is laid out above next to the radii: [0,10) group L,
// [10,20) group R, packed row-major inside each so the low bit is the file.
struct VezaxEncounterState
{
    RaidObs::ObsGuidMap<uint8> slotAssignments{"vezax.slot"};
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

// One sweep per action execution - GetDynamicObjectPositions is a grid search, and repeating it per
// hazard test across a 25-man raid is the per-tick cost the raid-mechanics notes warn about. Callers
// gather once and pass the vector to every check below.
void GatherVezaxHazards(Player* bot, std::vector<VezaxHazard>& hazards,
                        float searchRadius = ULDUAR_VEZAX_HAZARD_SEARCH_RADIUS);

bool TryGetVezaxNearestHazard(Player* bot, std::vector<VezaxHazard> const& hazards,
                              VezaxHazard& hazard);

// Anything with a mana bar that is not a healer. The field's -70% cost is MOD_POWER_COST_SCHOOL_PCT
// with a school mask of 127, so it covers physical too and a hunter's shots get it as readily as a
// mage's bolts - only the damage half is magic-only. Healers stay out of this list: they stand in
// whichever field lands on the camp, but the -75% healing means one is never worth walking to.
bool VezaxCanSoakShadowCrashField(Player* bot);

// Takes the bot because the camp is centred on the boss, not the anchor.
bool TryGetVezaxSlotPosition(Player* bot, uint8 slotIndex, Position& position);
// The tank only has to be on the anchor; a camp slot is 3 yd from its neighbours and needs tighter.
float VezaxSlotTolerance(Player* bot);
void EnsureVezaxSlotAssignments(Player* bot);

// The spot this bot should be standing on, hazards taken into account. False for melee, who hold the
// boss instead; the main tank is answered here too, from the anchor, because a tank standing on
// Vezax's spawn is what keeps him there for everyone else's radii.
bool TryGetVezaxSlot(Player* bot, Position& position);

// Where this bot goes while it carries the mark. A camp member steps sideways on its own group's
// side; everyone else takes the nearest southern spot.
bool TryGetVezaxMarkSpot(Player* bot, Position& position);

// The nearest living ally carrying the mark, or nullptr. Not the bot itself: its own leech skips it.
Unit* GetVezaxMarkedAlly(Player* bot);

// Somewhere outside the leech around a marked ally. For melee, who otherwise stand in it for the
// whole ten ticks - the camp is far enough out that no mark on the ball ever reaches it.
bool TryGetVezaxMarkBreakSpot(Player* bot, Unit* marked, Position& spot);

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

#endif
