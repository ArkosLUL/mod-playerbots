/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERVEZAX_H
#define PLAYERBOTS_ULDENCOUNTERVEZAX_H

#include "Position.h"
#include "RaidObs.h"
#include "UldBossHelper.h"

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
// until its holder is gone. The index space is laid out in UldBossHelper.h next to the radii:
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
