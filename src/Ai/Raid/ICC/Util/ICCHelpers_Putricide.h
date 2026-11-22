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

constexpr uint32 NPC_PP_PUTRICIDE = 36678;
constexpr uint32 NPC_PP_VOLATILE_OOZE = 37697;
constexpr uint32 NPC_PP_GAS_CLOUD = 37562;
constexpr uint32 NPC_PP_PUDDLE = 37690;
constexpr uint32 NPC_PP_GAS_BOMB = 38159;
constexpr uint32 NPC_PP_ABOMINATION_10 = 37672;
constexpr uint32 NPC_PP_ABOMINATION_25 = 38285;
constexpr uint32 GO_PP_DRINK_ME = 201584;

constexpr uint32 SPELL_PP_ADHESIVE = 70447;
constexpr uint32 SPELL_PP_GASEOUS_BLOAT = 70672;
constexpr uint32 SPELL_PP_MUTATED_PLAGUE = 72451;
constexpr uint32 SPELL_PP_UNBOUND_PLAGUE = 70911;
constexpr uint32 SPELL_PP_PLAGUE_SICKNESS = 70953;
constexpr uint32 SPELL_PP_OOZE_VARIABLE = 70352;
constexpr uint32 SPELL_PP_GAS_VARIABLE = 70353;
constexpr uint32 SPELL_PP_REGURGITATED_OOZE = 70539;
constexpr uint32 SPELL_PP_SLIME_PUDDLE = 70346;
constexpr uint32 SPELL_PP_MUTATION_INIT = 70308;
constexpr uint32 SPELL_PP_UNBOUND_SEARCHER = 70917;
constexpr uint32 SPELL_PP_BOUNCE_PROTECTION = 70955;

inline Position const ICC_PP_CENTER(4356.64f, 3211.78f, 389.40f);
inline Position const ICC_PP_DRINK_STAND(4356.60f, 3265.74f, 389.40f);

constexpr uint8 PP_SPOT_COUNT = 17;
constexpr float PP_SPOT_RING_NEAR = 8.0f;
constexpr float PP_SPOT_RING_FAR = 16.0f;
constexpr float PP_INNER_RADIUS = 12.0f;
constexpr uint8 PP_INNER_SLOTS = 14;
constexpr float PP_OUTER_RADIUS = 22.0f;
constexpr uint8 PP_OUTER_SLOTS = 24;
constexpr float PP_OUTER_OFFSET_DEG = 7.5f;
constexpr uint8 PP_SLOT_COUNT = PP_INNER_SLOTS + PP_OUTER_SLOTS;

constexpr float PP_STAND_PARK = 0.75f;
constexpr float PP_STAND_UNPARK = 2.5f;
static_assert(PP_STAND_PARK < PP_STAND_UNPARK, "a walk has to end inside its own trigger");

constexpr float PP_KITE_RADIUS = 36.0f;

constexpr float PP_PUDDLE_SCALE_RADIUS = 2.5f;
constexpr float PP_GAS_RADIUS = 3.0f;
constexpr float PP_BLAST_RADIUS = 10.0f;
constexpr float PP_GOO_RADIUS = 5.0f;
constexpr float PP_CARRIER_CLEAR = 4.5f;

constexpr float PP_TANK_DEFENSIVE_PCT = 40.0f;
constexpr float PP_TANK_LAST_RESORT_PCT = 25.0f;

uint8 IccPutricidePhase(Player* bot, Unit* boss);

Creature* GetPpBoss(PlayerbotAI* botAI);
bool IsPpLive(PlayerbotAI* botAI);

Unit* GetPpTarget(PlayerbotAI* botAI);

char const* GetPpHazardKind(PlayerbotAI* botAI);
bool FindPpDodgeSpot(PlayerbotAI* botAI, Position& spot, bool& interrupt, std::string& branch);
bool IsPpDodgeSpotClear(PlayerbotAI* botAI, Position const& spot);

bool IsPpKiter(PlayerbotAI* botAI);
bool GetPpKiteLeg(PlayerbotAI* botAI, Position& spot, char const*& branch);
bool IsPpKiteLegSafe(PlayerbotAI* botAI, Position const& spot);
void CommitPpKiteLeg(PlayerbotAI* botAI, Position const& to);

bool GetPpStackWalk(PlayerbotAI* botAI, Position& spot);
bool IsPpStacking(PlayerbotAI* botAI);

bool IsPpPlagueCarrier(PlayerbotAI* botAI);
bool IsPpPlagueReceiving(PlayerbotAI* botAI);
bool GetPpPlagueWalk(PlayerbotAI* botAI, Position& spot, char const*& branch);

enum class PpAbomDuty : uint8
{
    None,
    Drink,
    Spit,
    Eat,
    Slash,
    Exit
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

bool IsPpSwapTaunter(PlayerbotAI* botAI, uint32& defensive, bool& taunt);

bool IsPpBraked(PlayerbotAI* botAI);

uint32 GetPpTankDefensive(PlayerbotAI* botAI, char const* noteKind);
bool IsPpTankDefensiveHeld(PlayerbotAI* botAI);

bool IsPpHoldingStand(PlayerbotAI* botAI);
bool GetPpStandWalk(PlayerbotAI* botAI, Position& spot);
bool IsPpMovementOwned(PlayerbotAI* botAI);

RaidBurstWindow IccPutricideBurstWindow(PlayerbotAI* botAI);
std::vector<uint32> const& GetPpOpenerBosses(PlayerbotAI* botAI);
uint8 IccPutricideResistSchools(PlayerbotAI* botAI, uint8 previous);

void TickPpEncounter(PlayerbotAI* botAI);

#endif
