#ifndef PLAYERBOTS_RAID_TOCHELPERS_GORMOK_H
#define PLAYERBOTS_RAID_TOCHELPERS_GORMOK_H

#include <vector>

#include "ObjectDefines.h"
#include "PlayerbotAI.h"
#include "Position.h"
#include "ToCData.h"

namespace TrialOfTheCrusaderHelpers
{

inline constexpr uint32 SPELL_SNOBOLLED = 66406;
inline constexpr uint32 SPELL_FIRE_BOMB_IMPACT = 66317;

inline constexpr uint32 NPC_FIRE_BOMB = 34854;

inline constexpr uint32 FIRE_BOMB_LIFETIME_MS = 60000;
inline constexpr uint32 FIRE_BOMB_CAST_MS = 1000;
inline constexpr uint32 FIRE_BOMB_IMPACT_SLACK_MS = 500;
inline constexpr uint32 FIRE_BOMB_FALLBACK_IMPACT_MS = 5000;
inline constexpr uint32 FIRE_BOMB_SCAN_MS = 200;
inline constexpr float FIRE_BOMB_MISSILE_SPEED = 14.0f;
inline constexpr float FIRE_BOMB_IMPACT_RADIUS = 8.0f;
inline constexpr float FIRE_BOMB_TRIGGER = 9.0f;
inline constexpr float FIRE_BOMB_TIGHT_CLEARANCE = 9.5f;
inline constexpr float FIRE_BOMB_CLEARANCE = 11.0f;
inline constexpr float FIRE_BOMB_PULSE_RADIUS = 2.0f;
inline constexpr float FIRE_BOMB_COMBAT_REACH = 1.0f;
inline constexpr float FIRE_BOMB_PULSE_CLEARANCE = 6.0f;
inline constexpr float FIRE_BOMB_SWEEP_RADIUS = 20.0f;
static_assert(FIRE_BOMB_IMPACT_RADIUS < FIRE_BOMB_TRIGGER && FIRE_BOMB_TRIGGER < FIRE_BOMB_TIGHT_CLEARANCE &&
              FIRE_BOMB_TIGHT_CLEARANCE < FIRE_BOMB_CLEARANCE);
static_assert(FIRE_BOMB_PULSE_CLEARANCE > FIRE_BOMB_PULSE_RADIUS + FIRE_BOMB_COMBAT_REACH + DEFAULT_COMBAT_REACH,
              "a dodge spot sits past avoid aoe's reach for a landed bomb");

inline constexpr uint32 GORMOK_IMPALE_SWAP_STACKS = 3;
inline constexpr uint32 GORMOK_IMPALE_DEFENSIVE_STACKS = 5;
inline constexpr uint32 GORMOK_HOP_RESET_STACKS = 2;
inline constexpr float GORMOK_HOP_RANGE = 30.0f;

inline constexpr float GORMOK_STOMP_INTERRUPT_RADIUS = 20.0f;
inline constexpr float GORMOK_STOMP_TRIGGER = 22.0f;
inline constexpr float GORMOK_STOMP_CLEARANCE = 24.0f;
static_assert(GORMOK_STOMP_TRIGGER > GORMOK_STOMP_INTERRUPT_RADIUS && GORMOK_STOMP_CLEARANCE > GORMOK_STOMP_TRIGGER);

inline constexpr float GORMOK_COMBAT_REACH = 10.5f;

inline constexpr float GORMOK_CARRIER_MELEE_RANGE = 10.0f;
static_assert(GORMOK_CARRIER_MELEE_RANGE < PlayerMeleeRange(GORMOK_COMBAT_REACH), "the rider stops inside his melee ring");

uint32 GetGormokImpaleStacks(Unit* unit);

Unit* GetGormokSwapTauntTarget(PlayerbotAI* botAI);

bool GormokTankNeedsDefensive(PlayerbotAI* botAI);
Player* GetGormokImpaleResetTarget(PlayerbotAI* botAI);
bool GormokHandOfProtectionToCancel(PlayerbotAI* botAI);

Unit* GetGormokSnoboldPick(PlayerbotAI* botAI);

Unit* GetGormokForSnoboldCarrier(PlayerbotAI* botAI);

Unit* GetGormokStompThreat(PlayerbotAI* botAI);

Unit* GetGormokForStompCaster(PlayerbotAI* botAI);

bool GetFireBombDodgeAnchor(PlayerbotAI* botAI, Position& anchor);

void GetFireBombs(PlayerbotAI* botAI, std::vector<Position>& young, std::vector<Position>& old);

bool IsInFireBombImpact(PlayerbotAI* botAI);

}

#endif
