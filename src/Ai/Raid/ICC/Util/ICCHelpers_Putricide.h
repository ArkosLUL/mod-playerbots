/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_PUTRICIDE_H
#define PLAYERBOTS_ICCHELPERS_PUTRICIDE_H

#include <string>
#include <vector>

#include "Define.h"
#include "Position.h"
#include "RaidBurstWindow.h"

class Creature;
class GameObject;
class Player;
class PlayerbotAI;
class Unit;

// icecrown_citadel.h CreaturesIds. GetEntry reports these on every difficulty, and the 25-man abomination is a
// template of its own.
constexpr uint32 NPC_PP_PUTRICIDE = 36678;
constexpr uint32 NPC_PP_VOLATILE_OOZE = 37697;
constexpr uint32 NPC_PP_GAS_CLOUD = 37562;
constexpr uint32 NPC_PP_PUDDLE = 37690;
constexpr uint32 NPC_PP_GAS_BOMB = 38159;
constexpr uint32 NPC_PP_ABOMINATION_10 = 37672;
constexpr uint32 NPC_PP_ABOMINATION_25 = 38285;
constexpr uint32 GO_PP_DRINK_ME = 201584;

// Difficulty rows: remap at the call
constexpr uint32 SPELL_PP_ADHESIVE = 70447;
constexpr uint32 SPELL_PP_GASEOUS_BLOAT = 70672;
constexpr uint32 SPELL_PP_MUTATED_PLAGUE = 72451;
constexpr uint32 SPELL_PP_UNBOUND_PLAGUE = 70911;
constexpr uint32 SPELL_PP_PLAGUE_SICKNESS = 70953;
constexpr uint32 SPELL_PP_OOZE_VARIABLE = 70352;
constexpr uint32 SPELL_PP_GAS_VARIABLE = 70353;
constexpr uint32 SPELL_PP_REGURGITATED_OOZE = 70539;
constexpr uint32 SPELL_PP_SLIME_PUDDLE = 70346;
// No difficulty row: one id on every difficulty
constexpr uint32 SPELL_PP_MUTATION_INIT = 70308;
constexpr uint32 SPELL_PP_UNBOUND_SEARCHER = 70917;
constexpr uint32 SPELL_PP_BOUNCE_PROTECTION = 70955;

// Tear Gas Target Stalker 38317, the room's centre. The floor is flat at 389.40 out to 45 yd (navprobe
// --nav 0x09, rings of 24-36 headings from 6 to 45 yd) and every ray from it to 42 yd is clear.
inline Position const ICC_PP_CENTER(4356.64f, 3211.78f, 389.40f);
// 3.5 yd south of the Drink Me! table, the floor below its platform (navprobe: settles at 389.397, a
// normal path from the centre)
inline Position const ICC_PP_DRINK_STAND(4356.60f, 3265.74f, 389.40f);

// The boss spot is the centre, a ring of 8 at 8 yd, or one of 8 at 16 yd offset 22.5 degrees. Around each,
// casters and healers take one of 14 slots at 12 yd, hunters and the overflow one of 24 at 22 yd offset 7.5
// degrees. All 646 slots sit on the floor (settled Z 389.39-389.45) and see their spot (navprobe los).
constexpr uint8 PP_SPOT_COUNT = 17;
constexpr float PP_SPOT_RING_NEAR = 8.0f;
constexpr float PP_SPOT_RING_FAR = 16.0f;
// Malleable Goo never picks a player within 7 yd plus both reaches of him: 16.3 yd, 17.5 once he transforms.
// The inner ring keeps a caster under that with his drift off the spot and the park tolerance, and its slots
// stay more than 4.5 yd apart, past Unbound Plague's 3 yd pass.
constexpr float PP_INNER_RADIUS = 12.0f;
constexpr uint8 PP_INNER_SLOTS = 14;
// A hunter's minimum range is 5 yd plus his melee range, 16-17 yd, so hunters stand out here
constexpr float PP_OUTER_RADIUS = 22.0f;
constexpr uint8 PP_OUTER_SLOTS = 24;
constexpr float PP_OUTER_OFFSET_DEG = 7.5f;
constexpr uint8 PP_SLOT_COUNT = PP_INNER_SLOTS + PP_OUTER_SLOTS;

// A stand parks within PP_STAND_PARK and walks back once pushed past PP_STAND_UNPARK
constexpr float PP_STAND_PARK = 0.75f;
constexpr float PP_STAND_UNPARK = 2.5f;
static_assert(PP_STAND_PARK < PP_STAND_UNPARK, "a walk has to end inside its own trigger");

// The Gas Cloud's victim kites inside this disk round the centre, so legs follow the wall once they reach it
constexpr float PP_KITE_RADIUS = 36.0f;

// Hazard reach, centre to centre: a puddle hits within 2.5 yd times its scale, and every other one is a
// creature's area
constexpr float PP_PUDDLE_SCALE_RADIUS = 2.5f;
constexpr float PP_GAS_RADIUS = 3.0f;
constexpr float PP_BLAST_RADIUS = 10.0f;
constexpr float PP_GOO_RADIUS = 5.0f;
// Unbound Plague jumps 3 yd from its carrier, so others keep this far off
constexpr float PP_CARRIER_CLEAR = 4.5f;

// Any tank under PP_TANK_DEFENSIVE_PCT spends a button. Under PP_TANK_LAST_RESORT_PCT its class buttons are
// free again.
constexpr float PP_TANK_DEFENSIVE_PCT = 40.0f;
constexpr float PP_TANK_LAST_RESORT_PCT = 25.0f;

// Professor Putricide's phase, 1 to 3, from the health his DamageTaken hook tests: phase 2 at 80% and
// below, phase 3 at 35% and below. Publishes pp.phase.
uint8 IccPutricidePhase(Player* bot, Unit* boss);

// He is alive and in combat and the bot is in his room
Creature* GetPpBoss(PlayerbotAI* botAI);
bool IsPpLive(PlayerbotAI* botAI);

// Tanks and DPS: a player's skull for all but his holder, then the add the bot's 25H Variable lets it hurt,
// the Volatile Ooze, the Gas Cloud, him. His holder stays on him, a braked bot off him, the driver on
// nothing. Notes pp.target.
Unit* GetPpTarget(PlayerbotAI* botAI);

// The hazard the bot stands in, nullptr when none: puddle, gas, blast, goo, cloud, carrier
char const* GetPpHazardKind(PlayerbotAI* botAI);
// A spot clear of every hazard. `interrupt` says the hit is close enough to drop a cast for.
bool FindPpDodgeSpot(PlayerbotAI* botAI, Position& spot, bool& interrupt, std::string& branch);
bool IsPpDodgeSpotClear(PlayerbotAI* botAI, Position const& spot);

// The Gas Cloud's victim: its next leg, straight away from the cloud or along the kite disk's edge
bool IsPpKiter(PlayerbotAI* botAI);
bool GetPpKiteLeg(PlayerbotAI* botAI, Position& spot, char const*& branch);
bool IsPpKiteLegSafe(PlayerbotAI* botAI, Position const& spot);
void CommitPpKiteLeg(PlayerbotAI* botAI, Position const& to);

// Ranged, healers and tanks with nothing to hold: the walk onto the Volatile Ooze's rooted victim
bool GetPpStackWalk(PlayerbotAI* botAI, Position& spot);
bool IsPpStacking(PlayerbotAI* botAI);

// Heroic Unbound Plague carrier: onto its receiver once due, else clear of everyone
bool IsPpPlagueCarrier(PlayerbotAI* botAI);
bool GetPpPlagueWalk(PlayerbotAI* botAI, Position& spot, char const*& branch);

// The Mutated Abomination's driver, on foot or riding
enum class PpAbomDuty : uint8
{
    None,
    Drink,
    Spit,
    Eat,
    Slash
};

struct PpAbomPlan
{
    PpAbomDuty duty = PpAbomDuty::None;
    Unit* target = nullptr;
    GameObject* table = nullptr;
    Position spot;
    bool walk = false;
    bool cast = false;
};

bool IsPpDriver(PlayerbotAI* botAI);
bool IsPpDriving(PlayerbotAI* botAI);
bool GetPpAbomPlan(PlayerbotAI* botAI, PpAbomPlan& plan);
char const* PpAbomDutyName(PpAbomDuty duty);

// The taunt this bot spends on him now: a pickup off a non-tank, the phase 3 Mutated Plague rotation, a
// sharer handing him back. A sharer casts `defensive` first while it has one ready and none up.
bool IsPpSwapTaunter(PlayerbotAI* botAI, uint32& defensive);

// Past 90% of his holder's threat until under 80%: holds damage on him
bool IsPpBraked(PlayerbotAI* botAI);

// The button this tank spends now, 0 for none: the start of each phase 3 hold, any tank under
// PP_TANK_DEFENSIVE_PCT
uint32 GetPpTankDefensive(PlayerbotAI* botAI, char const* noteKind);
bool IsPpTankDefensiveHeld(PlayerbotAI* botAI);

// The holder's drag spot and the ring slots
bool IsPpHoldingStand(PlayerbotAI* botAI);
bool GetPpStandWalk(PlayerbotAI* botAI, Position& spot);
// A stand, the kite, the ooze stack or a plague walk owns the bot's feet
bool IsPpMovementOwned(PlayerbotAI* botAI);

// Lust waits for phase 3, the race against Mutated Plague and the puddles
RaidBurstWindow IccPutricideBurstWindow(PlayerbotAI* botAI);
std::vector<uint32> const& GetPpOpenerBosses(PlayerbotAI* botAI);
// Slime Puddle, Mutated Plague and Unbound Plague all hit nature and shadow
uint8 IccPutricideResistSchools(PlayerbotAI* botAI, uint8 previous);

// The scan, the boss spot, the slots, the driver, the plague receivers, the brake, the stand latch, the calls
// to a human tank, the reset after a wipe
void TickPpEncounter(PlayerbotAI* botAI);

#endif
