/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERALGALON_H
#define PLAYERBOTS_ULDENCOUNTERALGALON_H

#include "ObjectGuid.h"
#include "Position.h"
#include "RaidObs.h"
#include "UldData.h"

#include <unordered_map>
#include <vector>

class Player;
class PlayerbotAI;
class Unit;

// Algalon the Observer.

enum UlduarAlgalonIds
{
    PB_NPC_ALGALON = 32871,
    PB_NPC_LIVING_CONSTELLATION = 33052,
    PB_NPC_COLLAPSING_STAR = 32955,
    PB_NPC_BLACK_HOLE = 32953,
    PB_NPC_WORM_HOLE = 34099,
    PB_NPC_UNLEASHED_DARK_MATTER = 34097,
    NPC_ALGALON_ASTEROID_TARGET_1 = 33104,
    NPC_ALGALON_ASTEROID_TARGET_2 = 33105,
    SPELL_ALGALON_BIG_BANG = 64443,
    SPELL_ALGALON_BIG_BANG_25 = 64584,
    SPELL_ALGALON_PHASE_PUNCH = 64412,
    // The one "phased" aura in the encounter: the holes apply it, and so does the fifth Phase Punch
    // stack through 64417. Wearing it is what makes Big Bang miss you.
    SPELL_ALGALON_BLACK_HOLE_DAMAGE = 62169,
};

// Algalon the Observer. His room is a 47 yd disc around the home position with a floor at Z 417.32,
// and he evades the moment he leaves it, so nothing here may pull him or a tank past the edge.
constexpr float ULDUAR_ALGALON_ROOM_RADIUS = 47.0f;
constexpr float ULDUAR_ALGALON_ROOM_SEARCH_RADIUS = 60.0f;

// Black Hole (62168) and Worm Hole (65250) both project a 6 yd field with no target cap, so a single
// hole shelters the whole raid - and standing in one outside a Big Bang costs 1531 a tick.
constexpr float ULDUAR_ALGALON_SHELTER_RADIUS = 6.0f;

// The kiter parks past the hole, on the far side from the constellation, so the chase drags the
// constellation through the field. Anything under 6 yd would park the kiter inside it instead.
constexpr float ULDUAR_ALGALON_BLACK_HOLE_KITE_OFFSET = 9.0f;

// With no hole to spend, the kiter just walks its constellation off the raid. Arcane Barrage is
// capped at one target, so all this has to buy is that the one target keeps being the kiter.
constexpr float ULDUAR_ALGALON_KITE_CROWD_RADIUS = 20.0f;
constexpr float ULDUAR_ALGALON_KITE_LEAD_DISTANCE = 15.0f;

// Phase Punch is a 45s aura refreshed every 15.5s. Swapping at 3 leaves the off-tank arriving at its
// own third stack 46.5s after taking the boss, against a partner whose aura only fell off 45s after
// its third - one missed tick and neither tank may taunt. Four gives ~17s of margin.
constexpr uint32 ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS = 4;

// Big Bang repeats every 90.5s, and the window the raid needs a hole standing in before one lands.
// Dispersion is a 120s cooldown against that cadence, which is why the soak duty has to rotate.
constexpr uint32 ULDUAR_ALGALON_BIG_BANG_INTERVAL_MS = 90500;
constexpr uint32 ULDUAR_ALGALON_SHELTER_WINDOW_SECONDS = 30;

// Collapsing Star pacing. Each death is 16-21k unavoidable raid damage, and Collapse drains 1% of
// max health a second, so an ignored star kills itself after ~100s. The 60s summon only tops up to
// four alive, which is how four ignored stars end up exploding within seconds of each other.
constexpr float ULDUAR_ALGALON_STAR_PACING_RAID_HP_PCT = 80.0f;
constexpr uint32 ULDUAR_ALGALON_STAR_PACING_GAP_MS = 8000;
// Below this Collapse finishes the star on its own, so the pacing gate steps aside and lets the raid
// choose the moment rather than have it chosen for them.
constexpr float ULDUAR_ALGALON_STAR_FINISH_HP_PCT = 15.0f;

// Cosmic Smash: full damage inside 6 yd, dmg/dist*2 out to 10, dmg/dist beyond. The marker lands
// exactly 4s before the meteor, so the clearance has to be reached in one move.
constexpr float ULDUAR_ALGALON_COSMIC_SMASH_MARKER_RADIUS = 11.0f;
constexpr float ULDUAR_ALGALON_COSMIC_SMASH_CLEARANCE = 12.0f;
constexpr float ULDUAR_ALGALON_COSMIC_SMASH_SEARCH_RADIUS = 40.0f;

// Algalon formation. The raid arrives from the +Y side, so the tank slot sits on the -Y edge of the
// hole square and Algalon ends up facing away from everyone. Rings hang off that slot rather than
// off the room centre, because the tank is what healers have to stay in range of.
//
// Every radius and arc below is navprobe-verified on map 603: the floor is a WMO, flat at Z 417.321,
// 19/19 candidate points on mesh at 0.04 from poly. The arcs are trimmed rather than full half
// circles because the two -Y worm hole spots sit level with the tank slot, 10.2 and 16.8 yd out; the
// trims keep every slot at least 7.7 yd from all four, clear of the 6 yd field.
constexpr float ULDUAR_ALGALON_HEALER_RADIUS = 14.0f;
constexpr float ULDUAR_ALGALON_RANGED_INNER_RADIUS = 20.5f;
constexpr float ULDUAR_ALGALON_RANGED_OUTER_RADIUS = 27.0f;
constexpr uint8 ULDUAR_ALGALON_HEALER_SLOTS = 4;
constexpr uint8 ULDUAR_ALGALON_RANGED_INNER_SLOTS = 6;
constexpr uint8 ULDUAR_ALGALON_RANGED_OUTER_SLOTS = 8;
constexpr uint8 ULDUAR_ALGALON_TOTAL_SLOTS =
    ULDUAR_ALGALON_HEALER_SLOTS + ULDUAR_ALGALON_RANGED_INNER_SLOTS + ULDUAR_ALGALON_RANGED_OUTER_SLOTS;
constexpr float ULDUAR_ALGALON_HEALER_ARC_CENTER = 1.5708f;        // +Y, 35..145 degrees
constexpr float ULDUAR_ALGALON_HEALER_ARC_WIDTH = 1.9199f;
constexpr float ULDUAR_ALGALON_RANGED_INNER_ARC_CENTER = 1.7977f;  // 28..178 degrees
constexpr float ULDUAR_ALGALON_RANGED_INNER_ARC_WIDTH = 2.6180f;
constexpr float ULDUAR_ALGALON_RANGED_OUTER_ARC_CENTER = 1.5708f;  // 0..180 degrees
constexpr float ULDUAR_ALGALON_RANGED_OUTER_ARC_WIDTH = 3.1416f;
constexpr float ULDUAR_ALGALON_SLOT_TOLERANCE = 2.0f;
// Slots hold 8.9 to 12.1 yd apart, which is Cosmic Smash's cheap falloff band for the neighbours of
// whoever gets marked. A hole that lands on a slot is only stepped around, never fled from.
constexpr float ULDUAR_ALGALON_SLOT_DISPLACE_RADIUS = 25.0f;

// Per instance, not per bot: the state tick does two sweeps and 25 bots asking every frame is the
// per-tick cost the raid-mechanics notes warn about.
constexpr uint32 ULDUAR_ALGALON_STATE_TICK_MS = 250;

// Algalon's home position, and the tank slot the formation hangs off - 18.7 yd out from home on the
// -Y edge of the hole square, 10.2 yd clear of the nearest worm hole spot.
extern const Position ULDUAR_ALGALON_ROOM_CENTER;
extern const Position ULDUAR_ALGALON_TANK_SLOT;

// Everything the encounter needs across ticks, per instance. Slots are held rather than re-derived
// for the reason Vezax holds his: ranking the raid by guid every tick means one death renumbers
// everyone behind the corpse and the formation shuffles mid-fight.
struct AlgalonEncounterState
{
    RaidObs::ObsGuidMap<uint8> slotAssignments{"algalon.slot"};

    // Where each bot is running for Big Bang, and which hole each kiter is dragging its
    // constellation through. Latched so a walk cannot flip destination mid-spline.
    RaidObs::ObsGuidMap<ObjectGuid> shelterAssignments{"algalon.shelter"};
    RaidObs::ObsGuidMap<ObjectGuid> kiteHoleAssignments{"algalon.kitehole"};

    // One soaker per cast. Trigger and action both read this, so a per-tick re-derivation would
    // strand whoever was exempted from hiding half a second ago.
    RaidObs::ObsValue<ObjectGuid> bigBangSoaker{"algalon.soaker"};

    // The clock is latched from the first cast we actually see, not from pull: the encounter's own
    // timers are offset by an intro that is 26s on the first pull and 8.5s on every one after.
    uint32 firstBigBangMs = 0;
    uint32 lastBigBangMs = 0;
    bool bigBangCasting = false;

    // Collapsing Star pacing. Each death is 16-21k unavoidable raid damage, so the count is watched
    // to know when the last one went off.
    uint32 lastStarDeathMs = 0;
    uint8 starCount = 0;

    uint32 lastTickMs = 0;
};

extern std::unordered_map<uint32 /*instanceId*/, AlgalonEncounterState> algalonEncounterStates;

// By entry, never "find target": that value walks only the bot's own threat list, so healers and
// anyone off Algalon's threat list would fail to resolve him and silently lose every reaction the
// encounter has - Big Bang included. It also has to see him through the intro, while he is still
// neutral and unselectable, which is when the raid has time to reach its formation.
Unit* GetAlgalon(PlayerbotAI* botAI);
bool AlgalonEncounterActive(PlayerbotAI* botAI);

// Folds the per-instance clocks forward. Cheap and idempotent - it does its two sweeps at most once
// every ULDUAR_ALGALON_STATE_TICK_MS per instance, not once per bot.
void AlgalonTickEncounterState(PlayerbotAI* botAI);

bool AlgalonBigBangCasting(PlayerbotAI* botAI);
// Off the latched clock. False until the first cast has been seen, which is ~116s in - long after
// the first stars have died and left holes behind.
bool AlgalonBigBangWithin(PlayerbotAI* botAI, uint32 seconds);

// Black Holes in phase 1, Worm Holes in phase 2. Both carry the same 6 yd phase field, and the field
// has no target cap, so one of them shelters the whole raid.
std::vector<Unit*> CollectAlgalonShelters(PlayerbotAI* botAI);
uint8 AlgalonShelterCount(PlayerbotAI* botAI);
Unit* GetAlgalonShelter(Player* bot);
Unit* GetAlgalonShelterUnderfoot(Player* bot);

// Zero holes with a Big Bang closing in. Everything that could produce or consume one defers to this.
bool AlgalonNeedsShelterUrgently(PlayerbotAI* botAI);

// Lowest guid among the bots whose soak is actually off cooldown - Dispersion first, then Guardian
// Spirit, then a body that stays out and probably dies. Dispersion is 120s against a 90.5s Big Bang
// cadence, so no single bot can cover consecutive casts and the duty has to rotate.
Player* GetAlgalonBigBangSoaker(PlayerbotAI* botAI);

// Whoever Algalon is currently swinging at. The Phase Punch swap makes this alternate between the
// two tanks, so it is read live rather than assumed to be the main tank.
Player* GetAlgalonBossTank(PlayerbotAI* botAI);

// Who picks up the Unleashed Dark Matter in phase 2. Normally the off-tank, since collecting them on
// the boss means it never has to leave its swap position; a third tank takes the duty instead where
// the raid brought one.
Player* GetAlgalonAddTank(PlayerbotAI* botAI, Player* bot);

// The constellation already chasing this bot. Nothing taunts one into position: it picked its victim
// at activation and that bot is the one that can lead it anywhere.
Unit* GetAlgalonKiteTarget(Player* bot);

// A constellation parked on the bot holding Algalon. That one bot cannot kite and tank at once, so
// the other tank pulls it off.
Unit* GetAlgalonConstellationOnBossTank(PlayerbotAI* botAI);

// The hole this bot should drag its constellation through, or nullptr when the last one has to be
// kept for the raid.
Unit* GetAlgalonKiteHole(Player* bot, Unit* constellation);

// Lowest health first. Collapse drains 1% of max health a second, so health percent is the star's
// remaining lifetime - killing the shortest-lived one is what keeps the explosions apart.
Unit* GetAlgalonFocusStar(PlayerbotAI* botAI);

// Whether the raid can afford the next explosion yet. Ignoring the stars is not an option: the 60s
// event only tops up to four alive, so four untouched stars self-destruct within seconds of each
// other about 143s in.
bool AlgalonStarKillWindowOpen(PlayerbotAI* botAI);

uint8 AlgalonAliveStarCount(PlayerbotAI* botAI);

// The marker stalker the meteor is aimed at. It lands exactly 4s later, so this is a deadline, not a
// warning. The second form asks about a spot rather than about the bot, which is what lets the
// formation stand aside instead of walking someone back under one.
Unit* GetAlgalonCosmicSmashMarker(Player* bot);
bool AlgalonCosmicSmashMarkerNear(PlayerbotAI* botAI, Position const& spot, float radius);

// Ranged and healers ring the tank slot; melee and the off-tank keep normal combat positioning.
bool AlgalonTakesRingSlot(Player* bot);
bool TryGetAlgalonSlotPosition(uint8 slotIndex, Position& position);
void EnsureAlgalonSlotAssignments(Player* bot);
// The spot this bot should stand on. Phase 1 holes land wherever a star happened to die, so a slot
// can end up buried - in which case this answers with the nearest clear ground instead.
bool TryGetAlgalonSlot(Player* bot, Position& position);

// Drop this instance's state once Algalon is gone, or the next pull inherits a stale Big Bang clock
// and bots walk to slots nobody is standing in.
void ResetAlgalonEncounterState(Player* bot, bool clearInstance);

#endif
