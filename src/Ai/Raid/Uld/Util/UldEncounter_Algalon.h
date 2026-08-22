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
#include "UldBossHelper.h"

#include <unordered_map>
#include <vector>

class Player;
class PlayerbotAI;
class Unit;

// Algalon the Observer.
//
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
