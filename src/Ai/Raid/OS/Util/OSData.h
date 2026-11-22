/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef _PLAYERBOT_OSDATA_H
#define _PLAYERBOT_OSDATA_H

#include "Position.h"
#include <cstdint>
#include <vector>

namespace OsHelpers
{

constexpr uint32 OS_MAP_ID = 615;

constexpr uint32 OS_BOSS_SARTHARION = 0;

namespace NpcId
{
constexpr uint32 Sartharion = 28860;
constexpr uint32 SartharionH = 31311;
constexpr uint32 Tenebron = 30452;
constexpr uint32 TenebronH = 31534;
constexpr uint32 Shadron = 30451;
constexpr uint32 ShadronH = 31520;
constexpr uint32 Vesperon = 30449;
constexpr uint32 VesperonH = 31535;
constexpr uint32 AcolyteOfShadron = 31218;
constexpr uint32 AcolyteOfShadronH = 31541;
constexpr uint32 AcolyteOfVesperon = 31219;
constexpr uint32 AcolyteOfVesperonH = 31543;
constexpr uint32 DiscipleOfShadron = 30688;
constexpr uint32 DiscipleOfShadronH = 31544;
constexpr uint32 DiscipleOfVesperon = 30858;
constexpr uint32 DiscipleOfVesperonH = 31546;
constexpr uint32 TwilightEgg = 30882;
constexpr uint32 TwilightEggH = 31539;
constexpr uint32 TwilightWhelp = 30890;
constexpr uint32 TwilightWhelpH = 31540;
constexpr uint32 LavaBlaze = 30643;
constexpr uint32 LavaBlazeH = 31317;
constexpr uint32 TwilightFissure = 30641;
constexpr uint32 TwilightFissureH = 31521;
constexpr uint32 FlameTsunami = 30616;
constexpr uint32 SafeAreaTrigger = 30494;
}

namespace SpellId
{
constexpr uint32 TwilightShift = 57620;
constexpr uint32 TwilightShiftAlt = 57874;
constexpr uint32 GiftOfTwilightFire = 58766;
constexpr uint32 SartharionBerserk = 61632;
constexpr uint32 MoltenFury = 60430;
constexpr uint32 FlameTsunamiDamageAura = 57492;
constexpr uint32 TwilightTormentSartharion = 58835;
constexpr uint32 TwilightTormentVesperon = 57935;
constexpr uint32 PowerOfShadron = 58105;
}

namespace GoId
{
constexpr uint32 TwilightPortal = 193988;
constexpr uint32 NormalPortal = 193989;
}

enum class TsunamiWave : uint8
{
    None,
    Left,
    Right
};

enum class CorridorGroup : uint8
{
    Tank,
    Melee,
    Raid
};

extern Position const SAFE_AREA_SOUTH;
extern Position const SAFE_AREA_NORTH;
extern Position const TENEBRON_LANDING;
extern Position const SHADRON_LANDING;
extern Position const VESPERON_LANDING;

extern Position const TENEBRON_TANK_SPOT;
extern Position const SHADRON_TANK_SPOT;
extern Position const VESPERON_TANK_SPOT;

constexpr float TANK_HOLD_LEFT_X = 3221.3743f;
constexpr float TANK_CORRIDOR_LEFT_Y = 513.0f;
constexpr float TANK_HOLD_RIGHT_X = 3220.0706f;
constexpr float TANK_CORRIDOR_RIGHT_Y = 490.0f;
constexpr float RAID_CORRIDOR_LEFT_Y = 551.0f;
constexpr float RAID_CORRIDOR_RIGHT_Y = 535.5f;
constexpr float CORRIDOR_ARRIVAL_TOLERANCE = 2.0f;
constexpr float TANK_ARRIVAL_TOLERANCE = 1.0f;

constexpr float FISSURE_CLEAR_RADIUS = 8.0f;
constexpr float FISSURE_STEP_MARGIN = 2.0f;

constexpr float MAIN_TANK_DRAG_X = 3220.9905f;
constexpr float MAIN_TANK_DRAG_Y = 485.262f;
constexpr float MAIN_TANK_DRAG_TOLERANCE = 2.0f;
constexpr uint32 MAIN_TANK_DRAG_DWELL_MS = 5000;
constexpr uint32 MAIN_TANK_DRAG_TIMEOUT_MS = 45000;

constexpr float SARTHARION_CONE_DEGREES = 82.0f;
constexpr float SARTHARION_CONE_MARGIN_DEGREES = 16.0f;
constexpr float FLAME_BREATH_RANGE = 60.0f;
constexpr float TAIL_LASH_RANGE = 30.0f;
constexpr float FLANK_STAND_BACK = 2.0f;

constexpr float DRAKE_BREATH_RANGE = 15.0f;
constexpr float DRAKE_REAR_GATE_DEGREES = 140.0f;
constexpr float DRAKE_REAR_DRAW_DEGREES = 30.0f;

constexpr float RANGE_MARKER_SAFE_RADIUS = 36.0f;

constexpr float PLATFORM_MIN_Y = 487.0f;
constexpr float PLATFORM_MAX_Y = 569.0f;
constexpr float PLATFORM_MIN_X_SOUTH = 3220.0f;
constexpr float PLATFORM_MIN_X_CORE = 3227.0f;
constexpr float PLATFORM_MAX_X_TIP = 3231.0f;
constexpr float PLATFORM_MAX_X_NECK = 3242.0f;
constexpr float PLATFORM_MAX_X_SOUTH = 3250.0f;
constexpr float PLATFORM_MAX_X_CORE = 3266.0f;
constexpr float PLATFORM_MAX_X_WIDE = 3268.0f;
constexpr float PLATFORM_TIP_MAX_Y = 492.0f;
constexpr float PLATFORM_NECK_MAX_Y = 496.0f;
constexpr float PLATFORM_SOUTH_MAX_Y = 504.0f;
constexpr float PLATFORM_WIDE_MIN_Y = 520.0f;
constexpr float PLATFORM_WIDE_MAX_Y = 566.0f;

constexpr float ROOM_MIN_X = 3208.0f;
constexpr float ROOM_MAX_X = 3286.0f;
constexpr float ROOM_MIN_Y = 474.0f;
constexpr float ROOM_MAX_Y = 591.0f;
constexpr float ROOM_MAX_Z = 65.0f;

constexpr float RAID_ENTRY_ANCHOR_X = 3228.58f;

constexpr float RAID_LINE_OFFSET_X = 20.0f;
constexpr float OFFTANK_EAST_OFFSET = 4.0f;

constexpr float RAID_LINE_TOLERANCE_X = 8.0f;

constexpr float ROOM_SEARCH_RADIUS = 200.0f;

constexpr uint32 PULL_WINDOW_MS = 10000;

constexpr float MAIN_TANK_COOLDOWN_SHADRON_PCT = 50.0f;
constexpr float MAIN_TANK_COOLDOWN_PANIC_PCT = 25.0f;

constexpr float LAVA_BLAZE_TAUNT_RANGE = 30.0f;
constexpr float TRANQUILIZING_SHOT_RANGE = 35.0f;

inline std::vector<uint32> const DRAKE_ENTRIES = { NpcId::Tenebron,  NpcId::TenebronH, NpcId::Shadron,
                                                   NpcId::ShadronH,  NpcId::Vesperon,  NpcId::VesperonH };

inline std::vector<uint32> const OFFTANK_PICKUP_ENTRIES = { NpcId::LavaBlaze,     NpcId::LavaBlazeH,
                                                            NpcId::TwilightWhelp, NpcId::TwilightWhelpH };

inline std::vector<uint32> const TWILIGHT_ADD_ENTRIES = { NpcId::AcolyteOfShadron,  NpcId::AcolyteOfShadronH,
                                                          NpcId::AcolyteOfVesperon, NpcId::AcolyteOfVesperonH,
                                                          NpcId::DiscipleOfShadron, NpcId::DiscipleOfShadronH,
                                                          NpcId::DiscipleOfVesperon, NpcId::DiscipleOfVesperonH };

}

#endif
