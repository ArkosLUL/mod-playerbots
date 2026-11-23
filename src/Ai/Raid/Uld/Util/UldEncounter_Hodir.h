/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ULDENCOUNTERHODIR_H
#define PLAYERBOTS_ULDENCOUNTERHODIR_H

#include "EncounterHelpers.h"
#include "ObjectGuid.h"
#include "Position.h"
#include "UldData.h"

#include <vector>

class Creature;
class Player;
class PlayerbotAI;
class Unit;

enum UlduarHodirIds
{
    NPC_HODIR_ICICLE_SMALL = 33169,
    NPC_HODIR_ICICLE_DRIFT = 33173,
    NPC_SNOWPACKED_ICICLE = 33174,
    NPC_TOASTY_FIRE = 33342,
    NPC_HODIR_FLASH_FREEZE_BLOCK = 32938,
    NPC_HODIR_FLASH_FREEZE_PLAYER = 32926,
    NPC_HODIR_MAGE_ALLIANCE_10 = 32893,
    NPC_HODIR_MAGE_ALLIANCE_25 = 33327,
    NPC_HODIR_MAGE_HORDE_10 = 32946,
    NPC_HODIR_MAGE_HORDE_25 = 33331,
    NPC_HODIR_DRUID_ALLIANCE_10 = 32901,
    NPC_HODIR_DRUID_ALLIANCE_25 = 33325,
    NPC_HODIR_DRUID_HORDE_10 = 32941,
    NPC_HODIR_DRUID_HORDE_25 = 33333,
    NPC_HODIR_SHAMAN_ALLIANCE_10 = 32900,
    NPC_HODIR_SHAMAN_ALLIANCE_25 = 33328,
    NPC_HODIR_SHAMAN_HORDE_10 = 32950,
    NPC_HODIR_SHAMAN_HORDE_25 = 33332,
    NPC_HODIR_PRIEST_ALLIANCE_10 = 32897,
    NPC_HODIR_PRIEST_ALLIANCE_25 = 33326,
    NPC_HODIR_PRIEST_HORDE_10 = 32948,
    NPC_HODIR_PRIEST_HORDE_25 = 33330,
    SPELL_FLASH_FREEZE = 61968,
    SPELL_BITING_COLD_PLAYER_AURA = 62039,
    SPELL_HODIR_FLASH_FREEZE_TRAPPED = 61969,
    SPELL_HODIR_STARLIGHT = 62807,
    SPELL_HODIR_TOASTY_FIRE_AURA = 62821,
    SPELL_HODIR_FROZEN_BLOWS = 62478,
    SPELL_HODIR_STORM_CLOUD = 65123,
    SPELL_HODIR_STORM_POWER = 63711,
    SPELL_HODIR_SHATTER_CHEST_TIMER = 65272,
};

inline constexpr float ULDUAR_HODIR_STARLIGHT_RADIUS = 3.0f;
inline constexpr float ULDUAR_HODIR_TOASTY_FIRE_RADIUS = 11.0f;
inline constexpr float ULDUAR_HODIR_SAFE_AREA_RADIUS = 9.0f;
inline constexpr float ULDUAR_HODIR_SAFE_AREA_TOLERANCE = 6.0f;
inline constexpr float ULDUAR_HODIR_SAFE_AREA_RELEASE = 8.0f;
static_assert(ULDUAR_HODIR_SAFE_AREA_RELEASE < ULDUAR_HODIR_SAFE_AREA_RADIUS,
              "the release ring has to stay inside what Safe Area actually covers");
static_assert(ULDUAR_HODIR_SAFE_AREA_TOLERANCE < ULDUAR_HODIR_SAFE_AREA_RELEASE,
              "the park ring has to sit inside the release ring or arriving releases the bot");

inline constexpr float ULDUAR_HODIR_STORM_CLOUD_STACK_RADIUS = 3.0f;

inline constexpr float ULDUAR_HODIR_STORM_CLOUD_COLLECT_PARK = 2.0f;
inline constexpr float ULDUAR_HODIR_STORM_CLOUD_COLLECT_RELEASE = 2.5f;
static_assert(ULDUAR_HODIR_STORM_CLOUD_COLLECT_PARK < ULDUAR_HODIR_STORM_CLOUD_STACK_RADIUS,
              "a parked receiver has to be inside the pulse, not on its edge");
static_assert(ULDUAR_HODIR_STORM_CLOUD_COLLECT_PARK < ULDUAR_HODIR_STORM_CLOUD_COLLECT_RELEASE &&
                  ULDUAR_HODIR_STORM_CLOUD_COLLECT_RELEASE < ULDUAR_HODIR_STORM_CLOUD_STACK_RADIUS,
              "a receiver that stops walking has to still be inside the pulse");

inline constexpr float ULDUAR_HODIR_STORM_CLOUD_COLLECT_LEASH = 15.0f;

inline constexpr uint32 ULDUAR_HODIR_STORM_GATHER_TICK = 4;
inline constexpr int32 ULDUAR_HODIR_STORM_GATHER_LEAD_MS = 600;
inline constexpr int32 ULDUAR_HODIR_STORM_GATHER_HOLD_MS = 1500;
inline constexpr int32 ULDUAR_HODIR_STORM_GATHER_CAST_SLACK_MS = 250;

struct HodirStormGather
{
    ObjectGuid carrier;
    Position rally;
    float dist = 0.0f;
    int32 msToPulse = 0;
    int32 msToWalk = 0;
};

inline constexpr float ULDUAR_HODIR_ICE_SHARDS_RADIUS = 4.0f;
inline constexpr float ULDUAR_HODIR_ICE_SHARDS_CLEAR = 6.0f;
inline constexpr float ULDUAR_HODIR_BIG_SHARDS_RADIUS = 7.0f;
inline constexpr float ULDUAR_HODIR_BIG_SHARDS_CLEAR = 9.0f;

inline constexpr uint32 ULDUAR_HODIR_ICICLE_SPENT_MS = 3300;

inline constexpr float ULDUAR_HODIR_TANK_HOLD_TOLERANCE = 3.0f;

inline constexpr float ULDUAR_HODIR_CASTER_MAX_BOSS_GAP = 35.0f;

inline constexpr float ULDUAR_HODIR_DODGE_LEASH = 12.0f;
inline constexpr float ULDUAR_HODIR_DECLUMP_RADIUS = 4.5f;

inline constexpr float ULDUAR_HODIR_SWEEP_STEP = 2.0f;
inline constexpr float ULDUAR_HODIR_SWEEP_ANGLE_STEP = static_cast<float>(M_PI) / 8.0f;

inline constexpr float ULDUAR_HODIR_DODGE_TRIGGER_MARGIN = 0.5f;

inline constexpr float ULDUAR_HODIR_DODGE_ARRIVE = 0.8f;
inline constexpr float ULDUAR_HODIR_DODGE_SLIP = 1.0f;

inline constexpr float ULDUAR_HODIR_STARLIGHT_SEARCH_RADIUS = 30.0f;

inline constexpr float ULDUAR_HODIR_STARLIGHT_STAND_RADIUS = 1.5f;
inline constexpr float ULDUAR_HODIR_STARLIGHT_STAND_TOLERANCE = 1.0f;

inline constexpr float ULDUAR_HODIR_STARLIGHT_ZONE_MATCH = 1.0f;
static_assert(ULDUAR_HODIR_STARLIGHT_STAND_RADIUS + ULDUAR_HODIR_STARLIGHT_STAND_TOLERANCE <=
                  ULDUAR_HODIR_STARLIGHT_RADIUS,
              "a bot at the edge of its tolerance has to still be inside Starlight");

inline constexpr uint32 ULDUAR_HODIR_STAND_REJECT_DROP_MS = 3000;

inline constexpr float ULDUAR_HODIR_STARLIGHT_SHED_RADIUS = 2.0f;
static_assert(ULDUAR_HODIR_STARLIGHT_SHED_RADIUS < ULDUAR_HODIR_STARLIGHT_RADIUS,
              "both ends of the shed shuttle have to stay inside Starlight");

inline constexpr float ULDUAR_HODIR_FIRE_STAND_RADIUS = 8.0f;
inline constexpr float ULDUAR_HODIR_FIRE_STAND_TOLERANCE = 2.0f;
static_assert(ULDUAR_HODIR_FIRE_STAND_RADIUS + ULDUAR_HODIR_FIRE_STAND_TOLERANCE <
                  ULDUAR_HODIR_TOASTY_FIRE_RADIUS,
              "a bot at the edge of its tolerance has to still be inside the fire");

inline constexpr float ULDUAR_HODIR_BUFF_WALK = 15.0f;
inline constexpr float ULDUAR_HODIR_BUFF_WALK_UNBUFFED = 30.0f;
static_assert(ULDUAR_HODIR_BUFF_WALK_UNBUFFED <= ULDUAR_HODIR_STARLIGHT_SEARCH_RADIUS,
              "walking further than the zone sweep looks would find nothing to walk to");

inline constexpr float ULDUAR_HODIR_RANGED_MIN_BOSS_GAP = 15.0f;
static_assert(ULDUAR_HODIR_RANGED_MIN_BOSS_GAP < ULDUAR_HODIR_CASTER_MAX_BOSS_GAP,
              "the caster band has to have room between its ends");

inline constexpr float ULDUAR_HODIR_HOLD_FIRE_LEASH = 25.0f;

inline constexpr float ULDUAR_HODIR_HOLD_BEARING_MIN_GAP = 3.0f;

inline constexpr float ULDUAR_HODIR_HOLD_REAIM_GAP = 5.0f;
inline constexpr uint32 ULDUAR_HODIR_HOLD_REAIM_MS = 2000;

inline constexpr float ULDUAR_HODIR_SHUTTLE_HALF_LEG = 3.0f;
inline constexpr uint32 ULDUAR_HODIR_BITING_COLD_SHED_STACKS = 4;

inline constexpr uint32 ULDUAR_HODIR_BITING_COLD_TICK_MS = 1000;

inline constexpr float ULDUAR_HODIR_SHED_HOP_HALF = 0.5f;

inline constexpr float ULDUAR_HODIR_SHED_HOP_MIN_TRAVEL = 0.3f;

inline constexpr uint32 ULDUAR_HODIR_SHED_HOP_TICK_MARGIN_MS = 150;
static_assert(ULDUAR_HODIR_SHED_HOP_MIN_TRAVEL * 1000.0f /
                      (2 * ULDUAR_HODIR_BITING_COLD_TICK_MS + ULDUAR_HODIR_SHED_HOP_TICK_MARGIN_MS) >
                  0.1f,
              "MoveJump drops a jump at 0.1 yd/s or slower, and the shortest hop over the longest flight is that slow");

inline constexpr float ULDUAR_HODIR_SHED_HOP_SPEED_Z = 4.0f;

inline constexpr float ULDUAR_HODIR_SHED_HOP_HOME_SLIP = 1.5f;
static_assert(ULDUAR_HODIR_SHED_HOP_HALF * 2.0f < ULDUAR_HODIR_SHED_HOP_HOME_SLIP,
              "landing on the far end must not count as being pushed off the spot");

inline constexpr uint32 ULDUAR_HODIR_BITING_COLD_SHED_STACKS_IN_STARLIGHT = 5;
static_assert(ULDUAR_HODIR_BITING_COLD_SHED_STACKS_IN_STARLIGHT <= 6,
              "62039 caps at 8 stacks and 200*2^7 is 25600 a tick, so past 6 the shed arms too late to matter");

inline constexpr uint32 ULDUAR_HODIR_BITING_COLD_SHED_STACKS_FROZEN_BLOWS_DROP = 1;

inline constexpr uint32 ULDUAR_HODIR_BITING_COLD_SHED_FLOOR = 1;
static_assert(ULDUAR_HODIR_BITING_COLD_SHED_FLOOR <
                  ULDUAR_HODIR_BITING_COLD_SHED_STACKS - ULDUAR_HODIR_BITING_COLD_SHED_STACKS_FROZEN_BLOWS_DROP,
              "a shed has to have somewhere to go, Frozen Blows up or not");

inline constexpr float ULDUAR_HODIR_SHELTER_SHED_RADIUS = 3.0f;
static_assert(ULDUAR_HODIR_SHELTER_SHED_RADIUS < ULDUAR_HODIR_SAFE_AREA_TOLERANCE,
              "the shelter shuttle has to stay inside the park ring");

inline constexpr uint32 ULDUAR_HODIR_STALL_MS = 500;

inline constexpr float ULDUAR_HODIR_TRAPPED_ALLY_RANGE = 45.0f;
inline constexpr uint32 ULDUAR_HODIR_TRAPPED_ALLY_BREAKERS = 5;
inline constexpr float ULDUAR_HODIR_TRAPPED_ALLY_SWITCH_MARGIN = 10.0f;

inline constexpr uint32 ULDUAR_HODIR_HELPER_BLOCK_BREAKERS = 8;

inline constexpr uint32 ULDUAR_HODIR_MAGE_BLOCK_BREAKERS = 5;

inline constexpr uint32 ULDUAR_HODIR_SHAMAN_BLOCK_BREAKERS = 3;
static_assert(ULDUAR_HODIR_MAGE_BLOCK_BREAKERS + ULDUAR_HODIR_SHAMAN_BLOCK_BREAKERS <=
                  ULDUAR_HODIR_HELPER_BLOCK_BREAKERS,
              "with the first mage free, the second mage block keeps all its breakers beside the first shaman's");

inline constexpr uint32 ULDUAR_HODIR_LATE_BLOCK_BREAKERS = 3;
static_assert(ULDUAR_HODIR_LATE_BLOCK_BREAKERS <= ULDUAR_HODIR_HELPER_BLOCK_BREAKERS,
              "the wave standing down cannot pull in more bots than it had while a mage or shaman was iced");

inline constexpr uint32 ULDUAR_HODIR_HELPER_BLOCK_MIN_FREE = 2;

inline constexpr float ULDUAR_HODIR_LAST_WAVE_HEALTH_PCT = 12.0f;

inline constexpr float ULDUAR_HODIR_TAUNT_HEALTH_FLOOR = 50.0f;

inline constexpr uint32 ULDUAR_HODIR_TANK_DEFENSIVE_HOLD_MS = 1000;

inline constexpr float ULDUAR_HODIR_TANK_DEFENSIVE_LAST_RESORT_PCT = 25.0f;

inline constexpr uint32 ULDUAR_HODIR_LUST_FALLBACK_MS = 30000;

inline constexpr float ULDUAR_HODIR_ROOM_SEARCH_RADIUS = 100.0f;

extern const Position ULDUAR_HODIR_CENTRE;

Unit* GetHodir(PlayerbotAI* botAI);

bool IsHodirEngaged(PlayerbotAI* botAI);

bool IsHodirFlashFreezeIncoming(PlayerbotAI* botAI);

bool IsHodirLustHeld(PlayerbotAI* botAI);

bool HodirFrozenBlowsActive(PlayerbotAI* botAI, Player* bot);

bool HodirTauntWouldBeSuicide(PlayerbotAI* botAI, Player* bot);

uint32 GetHodirTankDefensive(PlayerbotAI* botAI, Player* bot, char const* noteKind);

void LatchHodirTankDefensive(PlayerbotAI* botAI, Player* bot);

bool IsHodirTankDefensiveHeld(PlayerbotAI* botAI);

Creature* GetHodirShelter(PlayerbotAI* botAI, Player* bot);

float GetHodirShelterPark(Creature* shelter);
float GetHodirShelterRelease(Creature* shelter);

bool IsHodirInLandedShelter(PlayerbotAI* botAI, Player* bot);

bool GetHodirStormCloudRally(Player* bot, Player* carrier, Position& out);

bool GetHodirStormGather(PlayerbotAI* botAI, Player* bot, HodirStormGather& out);

bool IsHodirStormGatherHeld(PlayerbotAI* botAI);

int32 GetHodirStormGatherCastSlackMs(PlayerbotAI* botAI, Player* bot);

bool GetHodirAnchor(PlayerbotAI* botAI, Player* bot, Position& out, float& tolerance);

bool GetHodirStarlightZoneAt(PlayerbotAI* botAI, Player* bot, Position& out);

bool GetHodirShuttleLeg(PlayerbotAI* botAI, Player* bot, Position& out);

bool GetHodirShedHop(PlayerbotAI* botAI, Player* bot, Position& out, uint32& flightMs);

void LatchHodirShedHop(Player* bot, Position const& to);

bool IsHodirShedHopInFlight(Player* bot);

void DropHodirShedHop(Player* bot);

bool IsHodirBitingColdShedArmed(PlayerbotAI* botAI, Player* bot);

bool IsHodirIcicleLethal(Creature* icicle);

bool IsHodirWalkThroughLiveIcicle(Player* bot, Position const& dest, float smallRadius, float bigRadius);

std::vector<EncounterHelpers::HazardCircle> CollectHodirIcicleHazards(Player* bot, float radius, float smallClear,
                                                                     float bigClear);

bool GetHodirDodgePreference(PlayerbotAI* botAI, Player* bot, Position& out);

Player* GetHodirResistancePaladin(PlayerbotAI* botAI, Player* bot);

bool IsHodirTrappedAllyBreaker(Player* bot, Unit* block);

Unit* GetHodirAssignedHelperBlock(PlayerbotAI* botAI, Player* bot);

char const* GetHodirHelperBlockKind(Unit* block);

#endif
