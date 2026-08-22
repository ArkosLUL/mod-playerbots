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
// everyone behind the corpse and the whole arc shuffles mid-fight, so an assignment is kept until
// its holder is gone. Index space is one range: [0, HEALER_SLOTS) is the healer ring, the rest are
// the two ranged rings.
struct VezaxEncounterState
{
    RaidObs::ObsGuidMap<uint8> slotAssignments{"vezax.slot"};
    // Where a bot stands while its own slot is buried under a hazard. Cleared as soon as the real
    // slot is clear again.
    RaidObs::ObsGuidMap<uint8> displacedAssignments{"vezax.displaced"};
};

extern std::unordered_map<uint32 /*instanceId*/, VezaxEncounterState> vezaxEncounterStates;

// By entry, not "find target": that value walks only the bot's own threat list, so any bot that
// switched to the Animus or a vapor would stop seeing the boss and every Vezax trigger would
// silently go inert.
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

// Mana casters only. The field boosts magic damage and cuts mana cost, so it does nothing for
// physical melee, and its 75% healing penalty makes it actively wrong for a healer.
bool VezaxCanSoakShadowCrashField(Player* bot);

// Only healers are actually hurt by the field. Melee and tanks gain nothing from it either, but
// nothing in it damages them, so walking them out would spend uptime to avoid a buff they cannot use.
bool VezaxMustLeaveShadowCrashField(Player* bot);

// Anything with a mana bar that is not already full. Everyone else takes the puddle's doubling
// damage for no return and should be out of it from the first tick.
bool VezaxWantsVaporPuddleMana(Player* bot);

// The puddle deals 100 * 2^stacks every 4s. Leaving is a prediction about the next tick, not a
// stack count: the same cap kills a 10-man healer and wastes mana for a geared 25-man one.
bool VezaxShouldLeaveVaporPuddle(Player* bot);

// What this particular bot has to stay out of. A mana caster wants to be standing in a Shadow Crash
// field, so for it only the vapor puddles count.
void VezaxBuildAvoidPositions(Player* bot, std::vector<VezaxHazard> const& hazards,
                              std::vector<Position>& avoid);

bool TryGetVezaxSlotPosition(uint8 slotIndex, Position& position);
void EnsureVezaxSlotAssignments(Player* bot);

// The spot this bot should be standing on, hazards taken into account. False for tanks and melee,
// who hold the boss instead.
bool TryGetVezaxSlot(Player* bot, std::vector<VezaxHazard> const& hazards, Position& position);

bool TryGetVezaxMarkSpot(Player* bot, Position& position);

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

// A handful of ranged, not the whole raid: a skull mark would pull 25 bots off the boss every 30s
// against a 10 minute berserk.
bool VezaxIsVaporKiller(Player* bot);

#endif
