/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_ROTFACE_H
#define PLAYERBOTS_ICCHELPERS_ROTFACE_H

#include <string>

#include "Define.h"
#include "Position.h"

class Creature;
class Player;
class PlayerbotAI;
class Unit;

constexpr uint32 NPC_RF_ROTFACE = 36627;
constexpr uint32 NPC_RF_LITTLE_OOZE = 36897;
constexpr uint32 NPC_RF_BIG_OOZE = 36899;
constexpr uint32 NPC_RF_STICKY_OOZE = 37006;
constexpr uint32 NPC_RF_PUDDLE_STALKER = 37013;
constexpr uint32 NPC_RF_SPRAY_STALKER = 37986;
constexpr uint32 NPC_RF_EXPLOSION_STALKER = 38107;

constexpr uint32 SPELL_RF_SLIME_SPRAY = 69508;
constexpr uint32 SPELL_RF_UNSTABLE_OOZE = 69558;
constexpr uint32 SPELL_RF_UNSTABLE_OOZE_EXPLOSION = 69839;
constexpr uint32 SPELL_RF_UNSTABLE_OOZE_SUICIDE = 71441;
constexpr uint32 SPELL_RF_OOZE_FLOOD_WARNINGS[] = {69783, 69797, 69799, 69802};
constexpr uint32 SPELL_RF_OOZE_FLOOD_ACTIVE = 69788;
constexpr uint32 SPELL_RF_MUTATED_INFECTION = 69674;
constexpr uint32 SPELL_RF_VILE_GAS = 69240;

inline Position const ICC_RF_CENTER(4445.87f, 3137.31f, 360.385f);
constexpr float RF_ROOM_RADIUS = 41.0f;

constexpr float RF_TANK_SPOT_DISTANCE = 5.0f;
constexpr float RF_BOSS_DRIFT = 4.5f;
constexpr float RF_BOSS_SETTLED = 2.5f;
constexpr float RF_DRAG_DISTANCE = 9.0f;

constexpr float RF_INNER_RADIUS = 12.0f;
constexpr uint8 RF_INNER_SLOTS = 6;
constexpr float RF_INNER_OFFSET_DEG = 30.0f;
constexpr float RF_OUTER_RADIUS = 21.5f;
constexpr uint8 RF_OUTER_SLOTS = 13;
constexpr float RF_OUTER_OFFSET_DEG = 180.0f / RF_OUTER_SLOTS;
constexpr uint8 RF_SLOT_COUNT = RF_INNER_SLOTS + RF_OUTER_SLOTS;
constexpr float RF_STACK_DISTANCE = 8.0f;

constexpr float RF_KITE_RADIUS = 36.0f;
constexpr uint8 RF_KITE_POINTS = 36;

constexpr float RF_STAND_PARK = 0.75f;
constexpr float RF_STAND_UNPARK = 2.5f;
static_assert(RF_STAND_PARK < RF_STAND_UNPARK, "a walk has to end inside its own trigger");

constexpr float RF_FLOOD_RADIUS = 25.0f;
constexpr float RF_STICKY_RADIUS = 6.0f;
constexpr float RF_MARK_RADIUS = 6.0f;
constexpr float RF_VILE_GAS_RADIUS = 8.0f;
constexpr float RF_RADIATING_RADIUS = 10.0f;
constexpr float RF_MERGE_RADIUS = 8.0f;
constexpr float RF_SPRAY_ARC_DEG = 25.0f;
constexpr float RF_SPRAY_BOUNDARY = 2.0f;
constexpr float RF_MARK_SPEED = 5.0f;

constexpr float RF_MERGE_TOUCH = 6.0f;
constexpr float RF_DISPEL_RANGE = 30.0f;
constexpr float RF_DISPEL_EMERGENCY_PCT = 35.0f;

constexpr uint8 RF_KITE_FROM_STACKS = 3;
constexpr uint8 RF_OOZE_DEFENSIVE_STACKS = 4;
constexpr float RF_TANK_DEFENSIVE_PCT = 40.0f;
constexpr float RF_TANK_LAST_RESORT_PCT = 25.0f;

bool IsRfLive(PlayerbotAI* botAI);
Creature* GetRfBoss(PlayerbotAI* botAI);

Unit* GetRfTarget(PlayerbotAI* botAI);
Unit* GetRfPetTarget(PlayerbotAI* botAI);

Unit* GetRfOozeToTaunt(PlayerbotAI* botAI);
bool GetRfKiteWalk(PlayerbotAI* botAI, Position& spot, int8& dir, bool& wade);
void CommitRfKiteLeg(PlayerbotAI* botAI, int8 dir, bool wade);
bool IsRfKiteSpotSafe(PlayerbotAI* botAI, Position const& spot);
bool IsRfKiteUrgent(PlayerbotAI* botAI);
bool IsRfKiter(PlayerbotAI* botAI);
bool IsRfKiteHeld(PlayerbotAI* botAI);
Unit* GetRfFreedom(PlayerbotAI* botAI, uint32& spellId);

bool GetRfCarrierWalk(PlayerbotAI* botAI, Position& spot);
bool IsRfCarrierHeld(PlayerbotAI* botAI);
Player* GetRfDispelTarget(PlayerbotAI* botAI, char const*& spell);
bool GetRfCureWalk(PlayerbotAI* botAI, Position& spot);
bool IsRfCureWalkHeld(PlayerbotAI* botAI);
char const* GetRfTotemSwap(PlayerbotAI* botAI);

char const* GetRfHazardKind(PlayerbotAI* botAI);
bool FindRfDodgeSpot(PlayerbotAI* botAI, Position& spot, bool& interrupt, std::string& branch);
bool IsRfDodgeSpotClear(PlayerbotAI* botAI, Position const& spot);
bool IsRfHazardHeld(PlayerbotAI* botAI);

bool IsRfHoldingStand(PlayerbotAI* botAI);
bool GetRfStandWalk(PlayerbotAI* botAI, Position& spot);
bool IsRfMovementOwned(PlayerbotAI* botAI);

uint32 GetRfTankDefensive(PlayerbotAI* botAI, char const* noteKind);
bool IsRfTankDefensiveHeld(PlayerbotAI* botAI);

bool IsRfAreaDamageHeld(PlayerbotAI* botAI);

void TickRfEncounter(PlayerbotAI* botAI);

#endif
