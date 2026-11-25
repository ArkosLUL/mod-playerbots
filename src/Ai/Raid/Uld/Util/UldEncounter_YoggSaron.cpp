/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_YoggSaron.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <limits>
#include <list>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "AiObjectContext.h"
#include "RaidDifficulty.h"
#include "RaidHumanTankCall.h"
#include "RaidInstanceState.h"
#include "RaidTankDefensive.h"
#include "CellImpl.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Creature.h"
#include "DBCEnums.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "InstanceScript.h"
#include "Item.h"
#include "GameObject.h"
#include "LastMovementValue.h"
#include "Map.h"
#include "Group.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "ThreatManager.h"
#include "UldEncounterGate.h"
#include "UldHardMode.h"
#include "UldYoggSaronPile.h"
#include "Unit.h"
#include "Vehicle.h"
#include "WorldSession.h"

const std::vector<uint32> ULDUAR_YOGG_SARON_ILLUSION_MOBS = {
    NPC_INFLUENCE_TENTACLE, NPC_SUIT_OF_ARMOR,   NPC_DEATHSWORN_ZEALOT,  NPC_RUBY_CONSORT,
    NPC_AZURE_CONSORT,      NPC_BRONZE_CONSORT,  NPC_EMERALD_CONSORT,    NPC_OBSIDIAN_CONSORT};

const Position ULDUAR_YOGG_SARON_PORTAL_SPOTS[ULDUAR_YOGG_SARON_PORTAL_SPOTS_25MAN] = {
    Position(1964.60f, -42.71f, 325.08f), Position(1986.94f, -46.21f, 324.98f),
    Position(1989.50f, -6.70f, 325.08f),  Position(1965.52f, -8.09f, 324.95f),
    Position(2000.84f, -25.40f, 325.19f), Position(1960.22f, -26.14f, 325.01f),
    Position(1976.30f, -47.83f, 325.11f), Position(1997.69f, -37.46f, 325.04f),
    Position(1998.07f, -13.36f, 325.17f), Position(1976.99f, -3.96f, 325.17f)};

const Position ULDUAR_YOGG_SARON_MIDDLE = Position(1980.28f, -25.5868f, 329.397f);
const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE = Position(1927.1511f, 68.507256f, 242.37657f);
const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE = Position(1925.6553f, -121.59296f, 239.98965f);
const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE = Position(2104.5667f, -25.509348f, 242.64679f);
const Position ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE = Position(1980.1971f, -27.854689f, 236.06789f);
const Position ULDUAR_YOGG_SARON_ICECROWN_CLUSTERS[ULDUAR_YOGG_SARON_ICECROWN_CLUSTER_COUNT] = {
    Position(1901.60f, -107.58f, 240.09f), Position(1916.27f, -137.88f, 240.07f),
    Position(1952.63f, -135.08f, 240.10f)};
const Position ULDUAR_YOGG_SARON_ICECROWN_STAGE_SPOT = Position(1952.10f, -73.62f, 240.008f);
const Position ULDUAR_YOGG_SARON_STORMWIND_STAGE_SPOT = Position(1955.17f, 18.87f, 239.721f);
const Position ULDUAR_YOGG_SARON_BRAIN_MELEE_SPOT = Position(1977.0f, -25.43f, 236.30f);
const Position ULDUAR_YOGG_SARON_BRAIN_WEST_SPOT = Position(1948.05f, -23.76f, 241.785f);
const Position ULDUAR_YOGG_SARON_BRAIN_SOUTH_SPOT = Position(1992.4f, -52.8f, 240.585f);
const Position ULDUAR_YOGG_SARON_BRAIN_NORTH_SPOT = Position(1994.6f, -0.1f, 240.292f);
const Position ULDUAR_YOGG_SARON_STORMWIND_KEEPER_ENTRANCE = Position(1954.06f, 21.66f, 239.71f);
const Position ULDUAR_YOGG_SARON_ICECROWN_CITADEL_ENTRANCE = Position(1950.11f, -79.284f, 239.98982f);
const Position ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_ENTRANCE = Position(2048.63f, -25.5f, 239.72f);
const Position ULDUAR_YOGG_SARON_P1_RANGED_SPOT = Position(1958.78f, -25.587f, 324.889f);
const Position ULDUAR_YOGG_SARON_HANDOVER_MELEE_SPOT = Position(1973.315f, -5.246f, 324.889f);
const Position ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT = Position(1998.5377f, -22.90317f, 324.8895f);
const Position ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT = Position(2018.7628f, -18.896868f, 327.07245f);
const std::vector<Position> ULDUAR_YOGG_SARON_PHASE_3_RANGED_STANDBY_SPOTS = {
    Position(2019.18f, -29.08f, 327.154f), Position(2015.72f, -9.16f, 327.155f),
    Position(2016.95f, -39.03f, 327.678f), Position(2010.26f, -0.55f, 327.291f)};
const Position ULDUAR_YOGG_SARON_BEACON_DRAG_NORTH = Position(1983.68f, 13.27f, 327.472f);
const Position ULDUAR_YOGG_SARON_BEACON_DRAG_SOUTH = Position(1993.62f, -62.23f, 327.625f);
const Position ULDUAR_YOGG_SARON_BEACON_DRAG_NORTHEAST = Position(2017.05f, 5.26f, 328.781f);
const Position ULDUAR_YOGG_SARON_BEACON_HUNTER_NORTH = Position(1986.0f, 7.63f, 326.797f);
const Position ULDUAR_YOGG_SARON_BEACON_HUNTER_SOUTH = Position(1994.44f, -55.65f, 326.394f);
const Position ULDUAR_YOGG_SARON_BEACON_STAGE_NORTH = Position(1990.94f, -4.40f, 325.104f);
const Position ULDUAR_YOGG_SARON_BEACON_STAGE_SOUTH = Position(1996.06f, -42.75f, 324.889f);
const Position ULDUAR_YOGG_SARON_BEACON_STAGE_NORTHEAST = Position(2009.52f, -6.19f, 326.989f);

namespace
{
struct YoggSaronBeaconLane
{
    Position const* drag;
    Position const* hunter;
    Position const* stage;
    char const* name;
};

constexpr uint8 YOGG_SARON_BEACON_LANE_NORTHEAST = 2;
std::array<YoggSaronBeaconLane, 3> const YOGG_SARON_BEACON_LANES = {
    {{&ULDUAR_YOGG_SARON_BEACON_DRAG_NORTH, &ULDUAR_YOGG_SARON_BEACON_HUNTER_NORTH,
      &ULDUAR_YOGG_SARON_BEACON_STAGE_NORTH, "north"},
     {&ULDUAR_YOGG_SARON_BEACON_DRAG_SOUTH, &ULDUAR_YOGG_SARON_BEACON_HUNTER_SOUTH,
      &ULDUAR_YOGG_SARON_BEACON_STAGE_SOUTH, "south"},
     {&ULDUAR_YOGG_SARON_BEACON_DRAG_NORTHEAST, &ULDUAR_YOGG_SARON_BEACON_DRAG_NORTHEAST,
      &ULDUAR_YOGG_SARON_BEACON_STAGE_NORTHEAST, "northeast"}}};

struct YoggSaronEncounterState
{
    RaidObs::ObsValue<uint32> phase{"yogg.phase"};
    RaidObs::ObsValue<uint32> wave{"yogg.wave"};
    RaidObs::ObsGuidSet brainTeam{"yogg.brainteam"};
    RaidObs::ObsGuidMap<uint8> portalSlot{"yogg.portalslot"};
    RaidObs::ObsGuidSet reprisalHold{"yogg.reprisal"};

    uint32 phase2StartMs = 0;
    uint32 lastWaveMs = 0;
    uint32 nextWaveMs = 0;
    uint32 waveOrdinal = 0;
    uint32 slotWave = 0;
    bool slotPortalsUp = false;

    RaidObs::ObsGuidSet benched{"yogg.benched"};
    uint32 returnPlanWave = 0;

    std::unordered_map<ObjectGuid, uint32> leavingUntilMs;
    std::unordered_map<ObjectGuid, uint32> exitLeadMs;

    std::unordered_map<ObjectGuid, YoggSaronRoom> lastIllusionRoom;

    uint32 shareWave = 0;
    std::unordered_map<ObjectGuid, uint8> icecrownCluster;
    enum StormwindRole : uint8
    {
        STORMWIND_FIRST,
        STORMWIND_HELPER,
        STORMWIND_DONE,
    };
    struct StormwindShare
    {
        ObjectGuid held;
        bool heldNear = false;
        StormwindRole role = STORMWIND_FIRST;
    };
    std::unordered_map<ObjectGuid, StormwindShare> stormwindShare;
    struct IllusionShare
    {
        std::vector<ObjectGuid> allowed;
        bool staged = false;
    };
    std::unordered_map<ObjectGuid, IllusionShare> illusionShare;
    std::array<int8, 3> brainSpotSight{-1, -1, -1};

    uint32 handoverStartMs = 0;
    uint32 handoverRingMs = 0;

    std::unordered_map<ObjectGuid, uint32> squeezeClaims;

    std::unordered_map<ObjectGuid, uint32> novaClaims;
    std::unordered_map<ObjectGuid, std::pair<ObjectGuid, uint32>> novaCovers;
    std::unordered_map<ObjectGuid, uint32> novaGates;

    ObjectGuid phase1Focus;

    uint32 lastGuardianDeathMs = 0;

    struct BeaconDrag
    {
        ObjectGuid dragger;
        uint32 markEndedMs = 0;
        uint8 lane = 0;
        uint32 tauntMs = 0;
        uint32 hiddenMs = 0;
    };
    std::unordered_map<ObjectGuid, BeaconDrag> beaconDrags;
    uint32 beaconClockMs = 0;
    uint32 lastMarkMs = 0;
    uint32 stageDueMs = 0;
    uint32 beaconScanMs = 0;
    bool markWindow = false;
    uint32 healCutScanMs = 0;
    std::string healCutNote;
    std::unordered_map<ObjectGuid, uint8> beaconStages;
    std::vector<ObjectGuid> markKickers;
    struct BeaconReturn
    {
        ObjectGuid dragger;
        uint32 releasedMs = 0;
    };
    std::unordered_map<ObjectGuid, BeaconReturn> beaconReturns;
    std::unordered_map<ObjectGuid, uint32> beaconExternalClaims;
    uint32 humanCallScanMs = 0;
    uint32 castCutScanMs = 0;

    struct PowerBreak
    {
        ObjectGuid crusher;
        Position spot;
        Position exit;
        bool out = false;
        uint32 outMs = 0;
    };
    std::unordered_map<ObjectGuid, PowerBreak> powerBreaks;

    bool roarExpected = false;
    uint32 lastRoarMs = 0;
    uint32 roarClaimKey = 0;
    ObjectGuid roarClaimOwner;

    enum BrainHoldMode : uint8
    {
        BRAIN_HOLD_NONE,
        BRAIN_HOLD_DOTS,
        BRAIN_HOLD_FULL,
        BRAIN_HOLD_FINISH,
    };
    struct BrainHold
    {
        BrainHoldMode mode = BRAIN_HOLD_NONE;
        uint32 seenMs = 0;
        uint32 finishCastEndMs = 0;
    };
    std::unordered_map<ObjectGuid, BrainHold> brainHold;

    std::unordered_map<ObjectGuid, std::pair<ObjectGuid, uint32>> interruptClaims;
    std::unordered_map<ObjectGuid, uint32> kickNudges;
    uint32 kickScanMs = 0;

    struct TankSwap
    {
        ObjectGuid from;
        ObjectGuid to;
        uint32 startMs = 0;
    };
    TankSwap tankSwap;
    std::unordered_map<ObjectGuid, std::pair<ObjectGuid, uint32>> tankLohClaims;

    uint32 phase3StartMs = 0;

    uint32 hazardNoteMs = 0;
    std::unordered_map<ObjectGuid, uint32> obsScanMs;
};

RaidInstanceState<YoggSaronEncounterState> yoggSaronStates;

YoggSaronEncounterState& YoggSaronStateFor(Player* bot) { return yoggSaronStates.For(bot->GetInstanceId()); }

void TickYoggSaronObs(PlayerbotAI* botAI);

constexpr uint32 YOGG_SARON_CACHED_CREATURES[] = {NPC_SARA_PHASE_1, NPC_YOGG_SARON, NPC_BRAIN, NPC_SANITY_WELL,
                                                  NPC_GUARDIAN_OF_YS, NPC_CRUSHER_TENTACLE, NPC_CONSTRICTOR_TENTACLE,
                                                  NPC_CORRUPTOR_TENTACLE};
constexpr uint32 YOGG_SARON_CACHED_DOORS[] = {GO_CHAMBER_ILLUSION_DOORS, GO_ICECROWN_ILLUSION_DOORS,
                                              GO_STORMWIND_ILLUSION_DOORS};

struct YoggSaronPassReads
{
    PlayerbotAI* botAI = nullptr;
    uint32 passId = 0;

    std::array<bool, std::size(YOGG_SARON_CACHED_CREATURES)> creatureRead{};
    std::array<ObjectGuid, std::size(YOGG_SARON_CACHED_CREATURES)> creature{};

    std::array<bool, std::size(YOGG_SARON_CACHED_DOORS)> doorRead{};
    std::array<ObjectGuid, std::size(YOGG_SARON_CACHED_DOORS)> door{};

    struct ClearRead
    {
        float x;
        float y;
        float radius;
        bool cleared;
    };
    std::vector<ClearRead> tentaclesCleared;

    bool leaveBrainLevelRead = false;
    bool leaveBrainLevel = false;

    bool platformTentacleRead = false;
    bool platformTentacleAlive = false;

    bool skullsInArcRead = false;
    std::vector<ObjectGuid> skullsInArc;

    bool healerStationRead = false;
    bool healerStationFound = false;
    Position healerStation;

};

thread_local YoggSaronPassReads yoggSaronPassReads;

YoggSaronPassReads* YoggSaronPassReadsFor(PlayerbotAI* botAI)
{
    uint32 const passId = EncounterTriggerPassId(botAI);
    if (!passId)
        return nullptr;

    if (yoggSaronPassReads.botAI != botAI || yoggSaronPassReads.passId != passId)
    {
        yoggSaronPassReads = YoggSaronPassReads();
        yoggSaronPassReads.botAI = botAI;
        yoggSaronPassReads.passId = passId;
    }

    return &yoggSaronPassReads;
}

int32 YoggSaronCacheSlot(uint32 const* entries, size_t count, uint32 entry)
{
    for (size_t slot = 0; slot < count; ++slot)
        if (entries[slot] == entry)
            return static_cast<int32>(slot);

    return -1;
}

Creature* YoggSaronFindNearestCreature(Player* bot, uint32 entry)
{
    constexpr float range = ULDUAR_YOGG_SARON_SEARCH_RANGE;

    uint32 type = 0;
    switch (entry)
    {
        case NPC_YOGG_SARON:
            type = ULD_BOSS_YOGGSARON;
            break;
        case NPC_SARA_PHASE_1:
            type = ULD_DATA_SARA;
            break;
        case NPC_BRAIN:
            type = ULD_DATA_BRAIN_OF_YOGG_SARON;
            break;
        default:
            return bot->FindNearestCreature(entry, range, true);
    }

    InstanceScript* instance = bot->GetInstanceScript();
    if (!instance)
        return bot->FindNearestCreature(entry, range, true);

    Creature* creature = instance->GetCreature(type);
    if (!creature)
        return nullptr;

    float const taxicab = std::abs(bot->GetPositionX() - creature->GetPositionX()) +
                          std::abs(bot->GetPositionY() - creature->GetPositionY());
    if (creature->GetEntry() != entry || taxicab >= range)
        return bot->FindNearestCreature(entry, range, true);

    return creature->IsAlive() && bot->IsWithinDist(creature, range) && bot->InSamePhase(creature) ? creature : nullptr;
}

GameObject* YoggSaronNearestDoor(PlayerbotAI* botAI, uint32 entry)
{
    Player* bot = botAI->GetBot();

    int32 const slot =
        YoggSaronCacheSlot(YOGG_SARON_CACHED_DOORS, std::size(YOGG_SARON_CACHED_DOORS), entry);
    YoggSaronPassReads* reads = slot < 0 ? nullptr : YoggSaronPassReadsFor(botAI);
    if (!reads)
        return bot->FindNearestGameObject(entry, ULDUAR_YOGG_SARON_SEARCH_RANGE);

    if (!reads->doorRead[slot])
    {
        GameObject* found = bot->FindNearestGameObject(entry, ULDUAR_YOGG_SARON_SEARCH_RANGE);
        reads->doorRead[slot] = true;
        reads->door[slot] = found ? found->GetGUID() : ObjectGuid::Empty;
        return found;
    }

    return reads->door[slot] ? ObjectAccessor::GetGameObject(*bot, reads->door[slot]) : nullptr;
}
}

Creature* YoggSaronNearestCreature(PlayerbotAI* botAI, uint32 entry)
{
    Player* bot = botAI->GetBot();

    int32 const slot =
        YoggSaronCacheSlot(YOGG_SARON_CACHED_CREATURES, std::size(YOGG_SARON_CACHED_CREATURES), entry);
    YoggSaronPassReads* reads = slot < 0 ? nullptr : YoggSaronPassReadsFor(botAI);
    if (!reads)
        return YoggSaronFindNearestCreature(bot, entry);

    if (!reads->creatureRead[slot])
    {
        Creature* found = YoggSaronFindNearestCreature(bot, entry);
        reads->creatureRead[slot] = true;
        reads->creature[slot] = found ? found->GetGUID() : ObjectGuid::Empty;
        return found;
    }

    return reads->creature[slot] ? ObjectAccessor::GetCreature(*bot, reads->creature[slot]) : nullptr;
}

uint32 YoggSaronPhase(PlayerbotAI* botAI)
{
    if (!BossStateGateLive(botAI, ULD_BOSS_YOGGSARON))
        return 0;

    Creature* yogg = YoggSaronNearestCreature(botAI, NPC_YOGG_SARON);

    uint32 phase = 0;
    if (yogg && yogg->IsAlive() && yogg->HasAura(SPELL_SHADOW_BARRIER))
        phase = 2;
    else if (yogg && yogg->IsAlive() && YoggSaronNearestCreature(botAI, NPC_BRAIN))
        phase = 3;
    else if (YoggSaronNearestCreature(botAI, NPC_SARA_PHASE_1))
        phase = 1;

    return phase;
}

bool YoggSaronEncounterActive(PlayerbotAI* botAI) { return YoggSaronPhase(botAI) != 0; }
bool YoggSaronInPhase1(PlayerbotAI* botAI) { return YoggSaronPhase(botAI) == 1; }
bool YoggSaronInPhase2(PlayerbotAI* botAI) { return YoggSaronPhase(botAI) == 2; }
bool YoggSaronInPhase3(PlayerbotAI* botAI) { return YoggSaronPhase(botAI) == 3; }

bool YoggSaronOnBrainLevel(Player* player)
{
    return player && player->GetPositionZ() > ULDUAR_YOGG_SARON_BRAIN_LEVEL_MIN_Z &&
           player->GetPositionZ() < ULDUAR_YOGG_SARON_BRAIN_LEVEL_MAX_Z;
}

YoggSaronRoom YoggSaronRoomOf(Player* player)
{
    if (!player)
        return YOGG_SARON_ROOM_NONE;

    if (player->GetPositionZ() >= ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
        return YOGG_SARON_ROOM_ARENA;

    if (!YoggSaronOnBrainLevel(player))
        return YOGG_SARON_ROOM_NONE;

    if (player->GetDistance2d(ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE.GetPositionX(),
                              ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE.GetPositionY()) <
        ULDUAR_YOGG_SARON_BRAIN_ROOM_RADIUS)
        return YOGG_SARON_ROOM_BRAIN;

    if (player->GetDistance2d(ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE.GetPositionX(),
                              ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE.GetPositionY()) <
        ULDUAR_YOGG_SARON_STORMWIND_KEEPER_RADIUS)
        return YOGG_SARON_ROOM_STORMWIND;

    if (player->GetDistance2d(ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE.GetPositionX(),
                              ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE.GetPositionY()) <
        ULDUAR_YOGG_SARON_ICECROWN_CITADEL_RADIUS)
        return YOGG_SARON_ROOM_ICECROWN;

    if (player->GetDistance2d(ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE.GetPositionX(),
                              ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE.GetPositionY()) <
        ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_RADIUS)
        return YOGG_SARON_ROOM_CHAMBER;

    return YOGG_SARON_ROOM_NONE;
}

bool YoggSaronRoomMiddle(Player* player, Position& middle)
{
    switch (YoggSaronRoomOf(player))
    {
        case YOGG_SARON_ROOM_STORMWIND:
            middle = ULDUAR_YOGG_SARON_STORMWIND_KEEPER_MIDDLE;
            return true;
        case YOGG_SARON_ROOM_ICECROWN:
            middle = ULDUAR_YOGG_SARON_ICECROWN_CITADEL_MIDDLE;
            return true;
        case YOGG_SARON_ROOM_CHAMBER:
            middle = ULDUAR_YOGG_SARON_CHAMBER_OF_ASPECTS_MIDDLE;
            return true;
        default:
            return false;
    }
}

bool YoggSaronBrainRoomApproachable(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    if (!YoggSaronInfluenceTentaclesCleared(botAI))
        return false;

    uint32 doorEntry = 0;
    switch (YoggSaronRoomOf(bot))
    {
        case YOGG_SARON_ROOM_BRAIN:
            for (uint32 entry = GO_CHAMBER_ILLUSION_DOORS; entry <= GO_STORMWIND_ILLUSION_DOORS; ++entry)
                if (GameObject* open = YoggSaronNearestDoor(botAI, entry))
                    if (open->GetGoState() == GO_STATE_ACTIVE)
                        return true;

            return false;
        case YOGG_SARON_ROOM_STORMWIND:
            doorEntry = GO_STORMWIND_ILLUSION_DOORS;
            break;
        case YOGG_SARON_ROOM_ICECROWN:
            doorEntry = GO_ICECROWN_ILLUSION_DOORS;
            break;
        case YOGG_SARON_ROOM_CHAMBER:
            doorEntry = GO_CHAMBER_ILLUSION_DOORS;
            break;
        default:
            return false;
    }

    GameObject* door = YoggSaronNearestDoor(botAI, doorEntry);

    return door && door->GetGoState() == GO_STATE_ACTIVE;
}

YoggSaronRoomState YoggSaronRoomStateOf(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    YoggSaronRoomState state = YOGG_SARON_ROOM_STATE_NONE;
    switch (YoggSaronRoomOf(bot))
    {
        case YOGG_SARON_ROOM_BRAIN:
            state = YOGG_SARON_ROOM_STATE_AT_BRAIN;
            break;
        case YOGG_SARON_ROOM_STORMWIND:
        case YOGG_SARON_ROOM_ICECROWN:
        case YOGG_SARON_ROOM_CHAMBER:
        {
            if (YoggSaronInfluenceTentaclesCleared(botAI))
            {
                state = YoggSaronBrainRoomApproachable(botAI) ? YOGG_SARON_ROOM_STATE_TO_BRAIN
                                                              : YOGG_SARON_ROOM_STATE_DOOR_SHUT;
                break;
            }

            if (YoggSaronReprisalHold(botAI))
            {
                state = YOGG_SARON_ROOM_STATE_REPRISAL;
                break;
            }

            Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
            if (target && target->IsAlive() && bot->IsWithinLOSInMap(target))
                state = YOGG_SARON_ROOM_STATE_FIGHTING;
            else if (YoggSaronIllusionStaged(botAI))
                state = YOGG_SARON_ROOM_STATE_STAGED;
            else
                state = YOGG_SARON_ROOM_STATE_WALKING_IN;
            break;
        }
        default:
            break;
    }

    if (RaidObs::Active())
    {
        char const* names[] = {"none",   "walkingin", "fighting", "doorshut",
                               "tobrain", "atbrain",  "reprisal", "staged"};
        RaidObs::NoteDerived(bot, "yogg.roomstate", names[state]);
    }

    return state;
}

namespace
{

bool YoggSaronLeaveBrainLevelNow(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    Creature const* brain = YoggSaronNearestCreature(botAI, NPC_BRAIN);
    if (!brain || !brain->IsAlive())
        return false;

    char const* branch = "safe";
    bool leave = false;

    if (brain->HasUnitState(UNIT_STATE_CASTING))
    {
        Spell* induceMadness = brain->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (induceMadness && induceMadness->m_spellInfo->Id == SPELL_INDUCE_MADNESS)
        {
            uint32 const castingTimeLeft = induceMadness->GetCastTimeRemaining();
            uint32 const now = getMSTime();

            bool const latched = YoggSaronLeavingBrainLevel(bot);

            bool const inBrainRoom = YoggSaronRoomOf(bot) == YOGG_SARON_ROOM_BRAIN;

            uint32 lead = ULDUAR_YOGG_SARON_EXIT_LEAD_FLOOR_MS;
            GameObject* portal =
                latched || !inBrainRoom
                    ? nullptr
                    : bot->FindNearestGameObject(GO_FLEE_TO_THE_SURFACE_PORTAL, ULDUAR_YOGG_SARON_SEARCH_RANGE);
            if (portal)
                lead = std::max(lead, static_cast<uint32>(bot->GetDistance2d(portal) / baseMoveSpeed[MOVE_RUN] *
                                                          ULDUAR_YOGG_SARON_EXIT_LEAD_SAFETY * IN_MILLISECONDS));

            YoggSaronStateFor(bot).exitLeadMs[bot->GetGUID()] = lead;

            if (latched || (inBrainRoom && castingTimeLeft < lead))
            {
                leave = true;
                branch = castingTimeLeft < ULDUAR_YOGG_SARON_EXIT_LEAD_FLOOR_MS ? "late" : "leaving";
                YoggSaronStateFor(bot).leavingUntilMs[bot->GetGUID()] = now + castingTimeLeft;
            }
        }
    }
    else if (brain->GetHealth() < brain->GetMaxHealth() * ULDUAR_YOGG_SARON_BRAIN_PHASE_3_SHARE)
    {
        leave = true;
        branch = "leaving";
    }

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.exit", branch);

    return leave;
}

}

bool YoggSaronShouldLeaveBrainLevel(PlayerbotAI* botAI)
{
    YoggSaronPassReads* reads = YoggSaronPassReadsFor(botAI);
    if (reads && reads->leaveBrainLevelRead)
        return reads->leaveBrainLevel;

    bool const leave = YoggSaronLeaveBrainLevelNow(botAI);

    if (reads)
    {
        reads->leaveBrainLevelRead = true;
        reads->leaveBrainLevel = leave;
    }

    return leave;
}

bool YoggSaronLeavingBrainLevel(Player* bot)
{
    YoggSaronEncounterState const& state = YoggSaronStateFor(bot);
    auto const latch = state.leavingUntilMs.find(bot->GetGUID());

    // Signed on purpose: getMSTimeDiff would read an end still ahead as a timer wrap.
    return latch != state.leavingUntilMs.end() && static_cast<int32>(latch->second - getMSTime()) > 0;
}

uint32 YoggSaronCancelAbilityLock(Player* bot)
{
    std::vector<uint32> locks;
    for (AuraEffect const* effect : bot->GetAuraEffectsByType(SPELL_AURA_ALLOW_ONLY_ABILITY))
    {
        SpellInfo const* spellInfo = effect->GetSpellInfo();
        if (!spellInfo->HasAttribute(SPELL_ATTR0_NO_AURA_CANCEL) && !spellInfo->IsChanneled() &&
            spellInfo->IsPositive() && !spellInfo->IsPassive())
            locks.push_back(spellInfo->Id);
    }

    for (uint32 const spell : locks)
        bot->RemoveOwnedAura(spell, ObjectGuid::Empty, 0, AURA_REMOVE_BY_CANCEL);

    return locks.empty() ? 0 : locks.front();
}

uint32 YoggSaronSanityOf(Player* player)
{
    Aura const* sanity = player ? player->GetAura(SPELL_SANITY) : nullptr;
    return sanity ? sanity->GetStackAmount() : 0;
}

Position YoggSaronBrainSpot(Player* bot)
{
    if (PlayerbotAI::IsMelee(bot) && !PlayerbotAI::IsHeal(bot))
        return ULDUAR_YOGG_SARON_BRAIN_MELEE_SPOT;

    YoggSaronEncounterState& state = YoggSaronStateFor(bot);

    YoggSaronRoom const here = YoggSaronRoomOf(bot);
    if (here == YOGG_SARON_ROOM_STORMWIND || here == YOGG_SARON_ROOM_ICECROWN || here == YOGG_SARON_ROOM_CHAMBER)
        state.lastIllusionRoom[bot->GetGUID()] = here;

    auto const came = state.lastIllusionRoom.find(bot->GetGUID());
    YoggSaronRoom const room = came == state.lastIllusionRoom.end() ? YOGG_SARON_ROOM_NONE : came->second;

    uint8 index = 0;
    Position const* spot = nullptr;
    char const* name = "middle";
    switch (room)
    {
        case YOGG_SARON_ROOM_STORMWIND:
            index = 0;
            spot = &ULDUAR_YOGG_SARON_BRAIN_WEST_SPOT;
            name = "west";
            break;
        case YOGG_SARON_ROOM_ICECROWN:
            index = 1;
            spot = &ULDUAR_YOGG_SARON_BRAIN_SOUTH_SPOT;
            name = "south";
            break;
        case YOGG_SARON_ROOM_CHAMBER:
            index = 2;
            spot = &ULDUAR_YOGG_SARON_BRAIN_NORTH_SPOT;
            name = "north";
            break;
        default:
            break;
    }

    if (spot && state.brainSpotSight[index] < 0)
    {
        if (Creature* brain = YoggSaronFindNearestCreature(bot, NPC_BRAIN))
        {
            Position const eye(spot->GetPositionX(), spot->GetPositionY(),
                               spot->GetPositionZ() + bot->GetCollisionHeight());
            Position const hit = brain->GetHitSpherePointFor(eye);
            state.brainSpotSight[index] =
                brain->GetMap()->isInLineOfSight(eye.GetPositionX(), eye.GetPositionY(), eye.GetPositionZ(),
                                                 hit.GetPositionX(), hit.GetPositionY(), hit.GetPositionZ(),
                                                 brain->GetPhaseMask(), LINEOFSIGHT_ALL_CHECKS,
                                                 VMAP::ModelIgnoreFlags::Nothing)
                    ? 1
                    : 0;
        }
    }

    if (!spot || state.brainSpotSight[index] != 1)
    {
        spot = &ULDUAR_YOGG_SARON_BRAIN_ROOM_MIDDLE;
        name = "middle";
    }

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.brainspot", name);

    return *spot;
}

Position const& YoggSaronPhaseThreeSpot(Player* bot)
{
    if (!PlayerbotAI::IsRanged(bot) || PlayerbotAI::IsHeal(bot))
        return ULDUAR_YOGG_SARON_PHASE_3_MELEE_SPOT;

    float const standbyReach = 40.0f;
    std::list<Creature*> crushers;
    bot->GetCreatureListWithEntryInGrid(crushers, NPC_CRUSHER_TENTACLE,
                                        bot->GetExactDist(&ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT) + standbyReach);

    auto const clearance = [&crushers](Position const& spot)
    {
        float nearest = std::numeric_limits<float>::max();
        for (Creature* crusher : crushers)
            if (crusher->IsAlive())
                nearest = std::min(nearest, crusher->GetExactDist2d(spot));

        return nearest;
    };

    Position const* best = &ULDUAR_YOGG_SARON_PHASE_3_RANGED_SPOT;
    float bestClearance = clearance(*best);
    if (bestClearance >= ULDUAR_YOGG_SARON_PHASE_3_RANGED_CRUSHER_CLEARANCE)
        return *best;

    for (Position const& spot : ULDUAR_YOGG_SARON_PHASE_3_RANGED_STANDBY_SPOTS)
    {
        float const spotClearance = clearance(spot);
        if (spotClearance >= ULDUAR_YOGG_SARON_PHASE_3_RANGED_CRUSHER_CLEARANCE)
            return spot;

        if (spotClearance > bestClearance)
        {
            best = &spot;
            bestClearance = spotClearance;
        }
    }

    return *best;
}

std::vector<Unit*> GetYoggSaronSkullsInArc(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    YoggSaronPassReads* reads = YoggSaronPassReadsFor(botAI);
    if (reads && reads->skullsInArcRead)
    {
        std::vector<Unit*> cached;
        for (ObjectGuid const& guid : reads->skullsInArc)
            if (Creature* skull = ObjectAccessor::GetCreature(*bot, guid))
                cached.push_back(skull);

        return cached;
    }

    std::list<Creature*> skulls;
    bot->GetCreatureListWithEntryInGrid(skulls, NPC_LAUGHING_SKULL, ULDUAR_YOGG_SARON_LAUGHING_SKULL_RADIUS);

    std::vector<Unit*> inArc;
    for (Creature* skull : skulls)
    {
        if (!skull->IsAlive())
            continue;

        if (bot->GetExactDist2d(skull) > ULDUAR_YOGG_SARON_LAUGHING_SKULL_RADIUS)
            continue;

        if (bot->HasInArc(static_cast<float>(M_PI), skull))
            inArc.push_back(skull);
    }

    if (reads)
    {
        reads->skullsInArcRead = true;
        for (Unit* skull : inArc)
            reads->skullsInArc.push_back(skull->GetGUID());
    }

    return inArc;
}

std::vector<Position> GetYoggSaronSkullsInRange(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    std::list<Creature*> skulls;
    bot->GetCreatureListWithEntryInGrid(skulls, NPC_LAUGHING_SKULL, ULDUAR_YOGG_SARON_LAUGHING_SKULL_RADIUS);

    std::vector<Position> found;
    for (Creature* skull : skulls)
        if (skull->IsAlive() && bot->GetExactDist2d(skull) <= ULDUAR_YOGG_SARON_LAUGHING_SKULL_RADIUS)
            found.push_back(skull->GetPosition());

    return found;
}

float YoggSaronHeadingGap(float from, float to)
{
    float const gap = Position::NormalizeOrientation(from - to);

    return gap > static_cast<float>(M_PI) ? 2.0f * static_cast<float>(M_PI) - gap : gap;
}

uint32 YoggSaronSkullsAhead(std::vector<Position> const& skulls, float x, float y, float heading)
{
    uint32 ahead = 0;
    for (Position const& skull : skulls)
        if (YoggSaronHeadingGap(std::atan2(skull.GetPositionY() - y, skull.GetPositionX() - x), heading) <
            static_cast<float>(M_PI) / 2.0f)
            ++ahead;

    return ahead;
}

uint32 YoggSaronAngledSkullHeading(std::vector<Position> const& skulls, float x, float y, float targetBearing,
                                   float& heading)
{
    heading = Position::NormalizeOrientation(targetBearing);
    uint32 fewest = YoggSaronSkullsAhead(skulls, x, y, heading);

    int32 const steps =
        static_cast<int32>(ULDUAR_YOGG_SARON_SKULL_TARGET_TURN / ULDUAR_YOGG_SARON_SKULL_TARGET_STEP + 0.5f);
    for (int32 step = 1; fewest && step <= steps; ++step)
    {
        float const turn = step * ULDUAR_YOGG_SARON_SKULL_TARGET_STEP;
        for (float const side : {turn, -turn})
        {
            float const candidate = Position::NormalizeOrientation(targetBearing + side);
            uint32 const ahead = YoggSaronSkullsAhead(skulls, x, y, candidate);
            if (ahead < fewest)
            {
                fewest = ahead;
                heading = candidate;
            }
        }
    }

    return fewest;
}

bool YoggSaronFacingClearOfSkulls(std::vector<Position> const& skulls, float x, float y, float targetX,
                                  float targetY)
{
    float heading = 0.0f;
    return YoggSaronAngledSkullHeading(skulls, x, y, std::atan2(targetY - y, targetX - x), heading) == 0;
}

YoggSaronPortalWave YoggSaronPortalWaveState(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    uint32 const now = getMSTime();

    YoggSaronEncounterState& state = YoggSaronStateFor(bot);

    if (YoggSaronPhase(botAI) != 2)
    {
        state.phase2StartMs = 0;
        state.lastWaveMs = 0;
        state.nextWaveMs = 0;
        return YoggSaronPortalWave();
    }

    state.phase3StartMs = 0;
    state.lastRoarMs = 0;
    state.tankSwap = YoggSaronEncounterState::TankSwap();
    state.tankLohClaims.clear();
    state.kickNudges.clear();

    bool const portalsUp =
        bot->FindNearestCreature(NPC_DESCEND_INTO_MADNESS, ULDUAR_YOGG_SARON_PORTAL_SEARCH_RADIUS, true) != nullptr;

    if (!state.phase2StartMs)
    {
        state.phase2StartMs = now;
        state.nextWaveMs = now + ULDUAR_YOGG_SARON_PORTAL_FIRST_WAVE_MS;
    }

    if (portalsUp &&
        (!state.lastWaveMs || getMSTimeDiff(state.lastWaveMs, now) > ULDUAR_YOGG_SARON_PORTAL_WAVE_DEBOUNCE_MS))
    {
        state.lastWaveMs = now;
        state.nextWaveMs = now + ULDUAR_YOGG_SARON_PORTAL_WAVE_PERIOD_MS;
        ++state.waveOrdinal;
        state.wave = state.waveOrdinal;
    }
    // Signed on purpose: getMSTimeDiff reads a wave still ahead as a timer wrap, ~4.29e9 ms late.
    else if (!portalsUp && state.nextWaveMs &&
             static_cast<int32>(now - state.nextWaveMs) > static_cast<int32>(ULDUAR_YOGG_SARON_PORTAL_WAVE_DEBOUNCE_MS))
    {
        state.nextWaveMs += ULDUAR_YOGG_SARON_PORTAL_WAVE_PERIOD_MS;
    }

    YoggSaronPortalWave answer;
    answer.active = true;
    answer.ordinal = state.waveOrdinal;
    answer.portalsUp = portalsUp;

    int32 const remaining = static_cast<int32>(state.nextWaveMs - now);
    answer.msToNextWave = remaining > 0 ? static_cast<uint32>(remaining) : 0;

    return answer;
}

std::vector<Player*> GetYoggSaronBrainTeam(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    Group* group = bot->GetGroup();
    if (!group)
        return {};

    uint32 size = IsRaid25Man(bot) ? ULDUAR_YOGG_SARON_PORTAL_SPOTS_25MAN
                                  : ULDUAR_YOGG_SARON_PORTAL_SPOTS_10MAN;

    Player* master = botAI->GetMaster();
    bool const masterTakesOne = master && master->GetGroup() == group && !PlayerbotAI::IsTank(master);
    if (masterTakesOne && size)
        --size;

    YoggSaronEncounterState const& state = YoggSaronStateFor(bot);

    std::vector<Player*> melee, healers, ranged;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || PlayerbotAI::IsTank(member))
            continue;

        if (masterTakesOne && member == master)
            continue;

        if (state.benched.count(member->GetGUID()))
            continue;

        if (PlayerbotAI::IsHeal(member))
            healers.push_back(member);
        else if (PlayerbotAI::IsMelee(member))
            melee.push_back(member);
        else
            ranged.push_back(member);
    }

    auto const byGuid = [](Player* left, Player* right) { return left->GetGUID() < right->GetGUID(); };
    std::sort(melee.begin(), melee.end(), byGuid);
    std::sort(ranged.begin(), ranged.end(), byGuid);

    auto const wieldsValanyr = [](Player* healer)
    {
        Item* mainHand = healer->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
        return mainHand && mainHand->GetEntry() == ITEM_VALANYR;
    };
    auto const mobileRank = [](Player* healer)
    {
        switch (healer->getClass())
        {
            case CLASS_PRIEST:
                return 0;
            case CLASS_DRUID:
                return 1;
            case CLASS_SHAMAN:
                return 2;
            case CLASS_PALADIN:
                return 3;
            default:
                return 4;
        }
    };
    std::sort(healers.begin(), healers.end(),
              [&](Player* left, Player* right)
              {
                  bool const leftValanyr = wieldsValanyr(left);
                  if (leftValanyr != wieldsValanyr(right))
                      return leftValanyr;

                  int const leftRank = mobileRank(left);
                  int const rightRank = mobileRank(right);
                  return leftRank != rightRank ? leftRank < rightRank : left->GetGUID() < right->GetGUID();
              });

    bool const botHealerGoes = !healers.empty() && !(masterTakesOne && PlayerbotAI::IsHeal(master));
    if (botHealerGoes && size && melee.size() >= size)
        melee.resize(size - 1);

    std::vector<Player*> team = melee;
    auto otherHealers = healers.begin();
    if (botHealerGoes)
        team.push_back(*otherHealers++);
    team.insert(team.end(), ranged.begin(), ranged.end());
    team.insert(team.end(), otherHealers, healers.end());

    if (team.size() > size)
        team.resize(size);

    return team;
}

namespace
{
bool YoggSaronPortalOpen(Creature const* portal)
{
    return portal && portal->IsAlive() && portal->HasNpcFlag(UNIT_NPC_FLAG_SPELLCLICK);
}

bool YoggSaronPortalClaimedByOther(Player* bot, YoggSaronEncounterState const& state, Creature const* portal,
                                   uint32 spotCount)
{
    for (auto const& [guid, slot] : state.portalSlot)
    {
        if (guid == bot->GetGUID() || slot >= spotCount ||
            portal->GetExactDist2d(ULDUAR_YOGG_SARON_PORTAL_SPOTS[slot]) > ULDUAR_YOGG_SARON_PORTAL_CLICK_RADIUS)
            continue;

        Player* owner = ObjectAccessor::GetPlayer(*bot, guid);
        return owner && owner->IsAlive() && !YoggSaronOnBrainLevel(owner);
    }

    return false;
}

void YoggSaronBenchLowSanity(Player* bot, YoggSaronEncounterState& state)
{
    std::unordered_set<ObjectGuid> low;
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member->IsAlive() && YoggSaronSanityOf(member) < ULDUAR_YOGG_SARON_BRAIN_TEAM_MIN_SANITY)
                low.insert(member->GetGUID());
        }
    }

    std::vector<ObjectGuid> const was(state.benched.begin(), state.benched.end());
    for (ObjectGuid const& guid : was)
        if (!low.count(guid))
            state.benched.erase(guid);

    for (ObjectGuid const& guid : low)
        state.benched.insert(guid);
}
}

YoggSaronPortalIntent YoggSaronPortalPlan(PlayerbotAI* botAI, Position& spot)
{
    Player* bot = botAI->GetBot();

    YoggSaronPortalWave const wave = YoggSaronPortalWaveState(botAI);
    if (!wave.active)
        return YOGG_SARON_PORTAL_NOT_TEAM;

    YoggSaronEncounterState& state = YoggSaronStateFor(bot);

    uint32 const spotCount = IsRaid25Man(bot) ? ULDUAR_YOGG_SARON_PORTAL_SPOTS_25MAN
                                             : ULDUAR_YOGG_SARON_PORTAL_SPOTS_10MAN;

    uint32 const assignmentWave = wave.ordinal + (wave.portalsUp ? 0 : 1);

    bool teamBack = false;
    if (!wave.portalsUp && wave.ordinal && state.returnPlanWave != assignmentWave)
    {
        Creature* brain = YoggSaronNearestCreature(botAI, NPC_BRAIN);
        teamBack = brain && brain->IsAlive() && !brain->FindCurrentSpellBySpellId(SPELL_INDUCE_MADNESS);
    }

    bool const newWave = state.slotWave != assignmentWave;

    if (newWave || teamBack || (wave.portalsUp && !state.slotPortalsUp))
    {
        if (teamBack)
            state.returnPlanWave = assignmentWave;

        if (newWave || teamBack)
            YoggSaronBenchLowSanity(bot, state);

        std::unordered_map<ObjectGuid, uint8> const previous = newWave || teamBack
                                                                    ? std::unordered_map<ObjectGuid, uint8>()
                                                                    : state.portalSlot.Raw();

        state.slotWave = assignmentWave;
        state.slotPortalsUp = wave.portalsUp;
        state.portalSlot.clear();
        state.brainTeam.clear();

        std::vector<Player*> const team = GetYoggSaronBrainTeam(botAI);
        std::vector<bool> taken(spotCount, false);
        for (Player* member : team)
        {
            auto const held = previous.find(member->GetGUID());
            if (held == previous.end() || held->second >= spotCount || taken[held->second] ||
                member->GetExactDist2d(ULDUAR_YOGG_SARON_PORTAL_SPOTS[held->second]) >
                    ULDUAR_YOGG_SARON_PORTAL_CLICK_RADIUS)
                continue;

            taken[held->second] = true;
            state.portalSlot[member->GetGUID()] = held->second;
        }

        for (Player* member : team)
            state.brainTeam.insert(member->GetGUID());

        while (true)
        {
            Player* pick = nullptr;
            uint32 pickSlot = spotCount;
            float pickDistance = 0.0f;
            for (Player* member : team)
            {
                if (state.portalSlot.count(member->GetGUID()))
                    continue;

                for (uint32 candidate = 0; candidate < spotCount; ++candidate)
                {
                    if (taken[candidate])
                        continue;

                    float const distance = member->GetExactDist2d(ULDUAR_YOGG_SARON_PORTAL_SPOTS[candidate]);
                    if (!pick || distance < pickDistance)
                    {
                        pick = member;
                        pickSlot = candidate;
                        pickDistance = distance;
                    }
                }
            }

            if (!pick)
                break;

            taken[pickSlot] = true;
            state.portalSlot[pick->GetGUID()] = static_cast<uint8>(pickSlot);
        }
    }

    uint8 slot = 0;
    bool assigned = false;
    auto const assignment = state.portalSlot.find(bot->GetGUID());
    if (assignment != state.portalSlot.end())
    {
        slot = assignment->second;
        assigned = true;
    }

    YoggSaronPortalIntent intent = YOGG_SARON_PORTAL_NOT_TEAM;
    if (assigned)
    {
        spot = ULDUAR_YOGG_SARON_PORTAL_SPOTS[slot];

        if (wave.portalsUp)
        {
            std::list<Creature*> portals;
            bot->GetCreatureListWithEntryInGrid(portals, NPC_DESCEND_INTO_MADNESS,
                                                ULDUAR_YOGG_SARON_PORTAL_SEARCH_RADIUS);

            portals.remove_if([](Creature* portal) { return !YoggSaronPortalOpen(portal); });

            auto const atSpot = [&spot](Creature* portal)
            { return portal->GetExactDist2d(spot) <= ULDUAR_YOGG_SARON_PORTAL_CLICK_RADIUS; };
            if (std::none_of(portals.begin(), portals.end(), atSpot))
            {
                Creature* nearest = nullptr;
                for (Creature* portal : portals)
                    if (!YoggSaronPortalClaimedByOther(bot, state, portal, spotCount) &&
                        (!nearest || bot->GetExactDist2d(portal) < bot->GetExactDist2d(nearest)))
                        nearest = portal;

                if (nearest)
                    spot = nearest->GetPosition();
            }
        }

        float const distance = bot->GetExactDist2d(spot);
        if (distance <= ULDUAR_YOGG_SARON_PORTAL_CLICK_RADIUS)
            intent = YOGG_SARON_PORTAL_HOLDING;
        else if (wave.portalsUp)
            intent = YOGG_SARON_PORTAL_LATE;
        else
        {
            float const speed = bot->GetSpeed(MOVE_RUN);
            uint32 lead = ULDUAR_YOGG_SARON_PORTAL_SPREAD_LEAD_FLOOR_MS;
            if (speed > 0.0f)
                lead = std::max(lead, static_cast<uint32>(distance / speed *
                                                          ULDUAR_YOGG_SARON_PORTAL_SPREAD_LEAD_SAFETY *
                                                          IN_MILLISECONDS));

            intent = wave.msToNextWave > lead ? YOGG_SARON_PORTAL_WAITING : YOGG_SARON_PORTAL_SPREADING;
        }
    }

    if (RaidObs::Active())
    {
        char const* names[] = {"notteam", "waiting", "spreading", "holding", "late"};
        RaidObs::NoteDerived(bot, "yogg.portal", names[intent]);
    }

    return intent;
}

Creature* YoggSaronPlannedPortal(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (YoggSaronRoomOf(bot) != YOGG_SARON_ROOM_ARENA)
        return nullptr;

    Position spot;
    if (YoggSaronPortalPlan(botAI, spot) != YOGG_SARON_PORTAL_HOLDING)
        return nullptr;

    std::list<Creature*> portals;
    bot->GetCreatureListWithEntryInGrid(portals, NPC_DESCEND_INTO_MADNESS,
                                        ULDUAR_YOGG_SARON_PORTAL_CLICK_RADIUS * 2.0f);
    for (Creature* portal : portals)
        if (YoggSaronPortalOpen(portal) &&
            portal->GetExactDist2d(spot) <= ULDUAR_YOGG_SARON_PORTAL_CLICK_RADIUS)
            return portal;

    return nullptr;
}

bool YoggSaronPortalWalkPending(PlayerbotAI* botAI)
{
    Position spot;
    YoggSaronPortalIntent const intent = YoggSaronPortalPlan(botAI, spot);

    return intent == YOGG_SARON_PORTAL_SPREADING || intent == YOGG_SARON_PORTAL_HOLDING ||
           intent == YOGG_SARON_PORTAL_LATE;
}

bool YoggSaronHoldingPortalSpot(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->GetPositionZ() < ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
        return false;

    YoggSaronEncounterState const& state = YoggSaronStateFor(bot);
    auto const assignment = state.portalSlot.find(bot->GetGUID());
    if (assignment == state.portalSlot.end() ||
        bot->GetExactDist2d(ULDUAR_YOGG_SARON_PORTAL_SPOTS[assignment->second]) > ULDUAR_YOGG_SARON_PORTAL_CLICK_RADIUS)
        return false;

    Position spot;
    return YoggSaronPortalPlan(botAI, spot) == YOGG_SARON_PORTAL_HOLDING;
}

namespace
{
bool YoggSaronHandoverOpen(PlayerbotAI* botAI)
{
    if (!BossStateGateLive(botAI, ULD_BOSS_YOGGSARON))
        return false;

    Creature* yogg = YoggSaronNearestCreature(botAI, NPC_YOGG_SARON);
    return yogg && yogg->IsAlive() && !yogg->HasAura(SPELL_SHADOW_BARRIER) &&
           !YoggSaronNearestCreature(botAI, NPC_BRAIN);
}

void YoggSaronTickHandover(PlayerbotAI* botAI)
{
    YoggSaronEncounterState& state = YoggSaronStateFor(botAI->GetBot());
    uint32 const now = getMSTime();

    if (YoggSaronHandoverOpen(botAI))
    {
        state.handoverRingMs = 0;
        if (!state.handoverStartMs)
            state.handoverStartMs = now;

        return;
    }

    if (state.handoverStartMs)
    {
        state.handoverRingMs = now;
        state.handoverStartMs = 0;
    }

    if (state.handoverRingMs && getMSTimeDiff(state.handoverRingMs, now) >= ULDUAR_YOGG_SARON_HANDOVER_HOLD_MS)
        state.handoverRingMs = 0;
}

void YoggSaronTickRtiMark(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (YoggSaronRoomOf(bot) != YOGG_SARON_ROOM_ARENA)
        return;

    std::string const mark = botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Get();
    if (mark == "skull" || mark == "diamond")
        return;

    botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("skull");
}
}

YoggSaronHandover YoggSaronHandoverState(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    YoggSaronEncounterState const& state = YoggSaronStateFor(bot);

    YoggSaronHandover answer;
    uint32 const now = getMSTime();
    answer.active = state.handoverStartMs != 0;

    bool const holding = !answer.active && state.handoverRingMs &&
                         getMSTimeDiff(state.handoverRingMs, now) < ULDUAR_YOGG_SARON_HANDOVER_HOLD_MS;

    if (!answer.active && !holding)
        return answer;

    answer.clearing = PlayerbotAI::IsTank(bot) || PlayerbotAI::IsMelee(bot);

    return answer;
}

bool YoggSaronInBodyKnockback(Player* player)
{
    return player && player->GetPositionZ() > ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT &&
           player->GetDistance2d(ULDUAR_YOGG_SARON_MIDDLE.GetPositionX(),
                                 ULDUAR_YOGG_SARON_MIDDLE.GetPositionY()) <
               ULDUAR_YOGG_SARON_BODY_KNOCKBACK_RADIUS;
}

namespace
{
float ClosestApproach(float fromX, float fromY, float toX, float toY, Position const& to)
{
    float const legX = toX - fromX;
    float const legY = toY - fromY;
    float const legSquared = legX * legX + legY * legY;

    float along = 0.0f;
    if (legSquared > 0.0f)
    {
        along = ((to.GetPositionX() - fromX) * legX + (to.GetPositionY() - fromY) * legY) / legSquared;
        along = std::max(0.0f, std::min(1.0f, along));
    }

    return to.GetExactDist2d(fromX + legX * along, fromY + legY * along);
}
}

bool YoggSaronRouteClearOfBody(Player* bot, float x, float y)
{
    if (YoggSaronInBodyKnockback(bot))
        return true;

    return ClosestApproach(bot->GetPositionX(), bot->GetPositionY(), x, y, ULDUAR_YOGG_SARON_MIDDLE) >=
           ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS;
}

bool YoggSaronRouteClearOfDeathRays(Player* bot, std::vector<Position> const& rays, float x, float y)
{
    constexpr float slack = 0.05f;
    for (Position const& ray : rays)
    {
        float const now = ray.GetExactDist2d(bot->GetPositionX(), bot->GetPositionY());
        float const keep = std::min(now, ULDUAR_YOGG_SARON_DEATH_RAY_CLEAR_RADIUS);
        if (ClosestApproach(bot->GetPositionX(), bot->GetPositionY(), x, y, ray) < keep - slack)
            return false;
    }

    return true;
}

bool YoggSaronDeathRayNear(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    return bot->GetPositionZ() > ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT &&
           bot->FindNearestCreature(NPC_DEATH_RAY, ULDUAR_YOGG_SARON_DEATH_RAY_CLEAR_RADIUS, true);
}

bool YoggSaronBodyDetour(Player* bot, Position const& destination, Position& waypoint)
{
    if (YoggSaronRouteClearOfBody(bot, destination.GetPositionX(), destination.GetPositionY()))
        return false;

    float const middleX = ULDUAR_YOGG_SARON_MIDDLE.GetPositionX();
    float const middleY = ULDUAR_YOGG_SARON_MIDDLE.GetPositionY();

    float const from = std::atan2(bot->GetPositionY() - middleY, bot->GetPositionX() - middleX);
    float const to = std::atan2(destination.GetPositionY() - middleY, destination.GetPositionX() - middleX);

    float turn = Position::NormalizeOrientation(to - from);
    if (turn > static_cast<float>(M_PI))
        turn -= 2.0f * static_cast<float>(M_PI);

    float const heading = from + turn / 2.0f;

    waypoint = Position(middleX + std::cos(heading) * ULDUAR_YOGG_SARON_BODY_DETOUR_RADIUS,
                        middleY + std::sin(heading) * ULDUAR_YOGG_SARON_BODY_DETOUR_RADIUS,
                        destination.GetPositionZ());

    return true;
}

Position YoggSaronBodyRoute(Player* bot, Position const& spot)
{
    Position waypoint;
    if (!YoggSaronBodyDetour(bot, spot, waypoint))
        return spot;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.detour", "spot");

    return waypoint;
}

bool IsYoggSaronHoldableGuardian(Unit* unit)
{
    if (!unit || !unit->IsAlive())
        return false;

    uint32 const entry = unit->GetEntry();
    if (entry != NPC_IMMORTAL_GUARDIAN && entry != NPC_MARKED_IMMORTAL_GUARDIAN)
        return false;

    if (unit->HasAura(SPELL_WEAKENED))
        return false;

    return !yoggSaronStates.For(unit->GetInstanceId()).beaconDrags.count(unit->GetGUID());
}

bool YoggSaronHoldableGuardianWithin(Player* bot, float radius)
{
    for (uint32 const entry : {NPC_IMMORTAL_GUARDIAN, NPC_MARKED_IMMORTAL_GUARDIAN})
    {
        std::list<Creature*> guardians;
        bot->GetCreatureListWithEntryInGrid(guardians, entry, radius);
        for (Creature* guardian : guardians)
            if (IsYoggSaronHoldableGuardian(guardian))
                return true;
    }

    return false;
}

namespace
{
struct ImmortalGuardianInRangeCheck
{
    Acore::AnyUnitInObjectRangeCheck inRange;

    bool operator()(Unit* unit)
    {
        uint32 const entry = unit->GetEntry();
        return (entry == NPC_IMMORTAL_GUARDIAN || entry == NPC_MARKED_IMMORTAL_GUARDIAN) && inRange(unit);
    }
};
}

GuidVector GetYoggSaronGuardiansInSight(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    float const range = sPlayerbotAIConfig.SightDistance;

    std::vector<Unit*> found;
    ImmortalGuardianInRangeCheck check{Acore::AnyUnitInObjectRangeCheck(bot, range)};
    Acore::UnitListSearcher<ImmortalGuardianInRangeCheck> searcher(bot, found, check);
    Cell::VisitObjects(bot, searcher, range);

    GuidVector guardians;
    for (Unit* unit : found)
        if (bot->IsWithinLOSInMap(unit))
            guardians.push_back(unit->GetGUID());

    return guardians;
}

bool YoggSaronGuardianLoose(PlayerbotAI* botAI)
{
    return YoggSaronGuardianLoose(botAI, GetYoggSaronGuardiansInSight(botAI));
}

bool YoggSaronGuardianLoose(PlayerbotAI* botAI, GuidVector const& guardians)
{
    for (ObjectGuid const& guid : guardians)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!IsYoggSaronHoldableGuardian(unit))
            continue;

        Player* victim = unit->GetVictim() ? unit->GetVictim()->ToPlayer() : nullptr;
        if (!victim || !PlayerbotAI::IsTank(victim))
            return true;
    }

    return false;
}

bool YoggSaronTauntFadesShort(Player* bot, Unit* guardian)
{
    if (!IsYoggSaronHoldableGuardian(guardian) || YoggSaronBeaconDragging(bot))
        return false;

    Player* holder = guardian->GetVictim() ? guardian->GetVictim()->ToPlayer() : nullptr;
    if (!holder || holder == bot || !PlayerbotAI::IsTank(holder) || guardian->IsWithinMeleeRange(holder))
        return false;

    if (bot->GetExactDist2d(guardian) >
        holder->GetExactDist2d(guardian) + ULDUAR_YOGG_SARON_PHASE_3_STACK_RADIUS)
        return false;

    int32 left = -1;
    for (AuraEffect const* taunt : guardian->GetAuraEffectsByType(SPELL_AURA_MOD_TAUNT))
        if (taunt->GetCasterGUID() == holder->GetGUID())
            left = std::max(left, taunt->GetBase()->GetDuration());

    return left >= 0 && left <= ULDUAR_YOGG_SARON_TAUNT_CHAIN_LEAD_MS;
}

namespace
{
uint32 YoggSaronTauntSpellId(Player* player)
{
    switch (player->getClass())
    {
        case CLASS_WARRIOR:
            return SPELL_WARRIOR_TAUNT;
        case CLASS_PALADIN:
            return SPELL_PALADIN_HAND_OF_RECKONING;
        case CLASS_DEATH_KNIGHT:
            return SPELL_DEATH_KNIGHT_DARK_COMMAND;
        case CLASS_DRUID:
            return SPELL_DRUID_GROWL;
        case CLASS_HUNTER:
            return SPELL_DISTRACTING_SHOT;
        default:
            return 0;
    }
}

bool IsYoggSaronBot(Player* player) { return player && player->GetSession() && player->GetSession()->IsHeadless(); }

std::vector<Player*> YoggSaronBeaconDraggerOrder(Player* bot, std::list<Creature*> const& unmarked,
                                                 uint32 tauntWithinMs)
{
    std::vector<Player*> tanks;
    Group* group = bot->GetGroup();
    if (!group)
        return tanks;

    std::unordered_set<ObjectGuid> held;
    for (Creature* guardian : unmarked)
        if (Unit* victim = guardian->IsAlive() ? guardian->GetVictim() : nullptr)
        {
            held.insert(victim->GetGUID());
            if (ObjectGuid const owner = victim->GetCharmerOrOwnerGUID(); owner.IsPlayer())
                held.insert(owner);
        }

    uint32 tanksAlive = 0;
    uint32 tanksDragging = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        if (Player* member = ref->GetSource(); member && member->IsAlive() && PlayerbotAI::IsTank(member))
        {
            ++tanksAlive;
            if (YoggSaronBeaconDragging(member))
                ++tanksDragging;
        }

    std::vector<Player*> backups;
    std::vector<Player*> hunters;
    std::vector<Player*> bears;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!IsYoggSaronBot(member) || !member->IsAlive() || member->GetMap() != bot->GetMap() ||
            member->GetPositionZ() < ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT ||
            held.count(member->GetGUID()) || YoggSaronBeaconDragging(member))
            continue;

        uint32 const taunt = YoggSaronTauntSpellId(member);
        if (!taunt || !member->HasSpell(taunt) || member->GetSpellCooldownDelay(taunt) > tauntWithinMs)
            continue;

        if (PlayerbotAI::IsTank(member))
            tanks.push_back(member);
        else if (!PlayerbotAI::IsHeal(member) &&
                 (member->getClass() == CLASS_PALADIN || member->getClass() == CLASS_DEATH_KNIGHT))
            backups.push_back(member);
        else if (member->getClass() == CLASS_HUNTER)
            hunters.push_back(member);
        else if (member->getClass() == CLASS_DRUID && !PlayerbotAI::IsHeal(member) &&
                 !PlayerbotAI::IsRanged(member) && YoggSaronBearReady(member))
            bears.push_back(member);
    }

    auto const byGuid = [](Player* left, Player* right) { return left->GetGUID() < right->GetGUID(); };
    std::sort(tanks.begin(), tanks.end(), byGuid);
    uint32 const tankSlots = tanksAlive > tanksDragging + 1 ? tanksAlive - tanksDragging - 1 : 0;
    if (tanks.size() > tankSlots)
        tanks.resize(tankSlots);
    std::sort(backups.begin(), backups.end(), byGuid);
    std::sort(hunters.begin(), hunters.end(), byGuid);
    std::sort(bears.begin(), bears.end(), byGuid);
    tanks.insert(tanks.end(), backups.begin(), backups.end());
    tanks.insert(tanks.end(), hunters.begin(), hunters.end());
    tanks.insert(tanks.end(), bears.begin(), bears.end());

    return tanks;
}

bool YoggSaronDrainLifeChannelling(Unit* guardian)
{
    return guardian->FindCurrentSpellBySpellId(SPELL_GUARDIAN_DRAIN_LIFE) ||
           guardian->FindCurrentSpellBySpellId(SPELL_GUARDIAN_DRAIN_LIFE_H);
}

int32 YoggSaronDrainLifeElapsed(Unit* guardian)
{
    Spell* channel = guardian->FindCurrentSpellBySpellId(SPELL_GUARDIAN_DRAIN_LIFE);
    if (!channel)
        channel = guardian->FindCurrentSpellBySpellId(SPELL_GUARDIAN_DRAIN_LIFE_H);

    return channel ? std::max(0, channel->GetSpellInfo()->GetDuration() - channel->GetCastTimeRemaining()) : -1;
}

Spell* YoggSaronDrainLifeKickable(Unit* guardian)
{
    Spell* channel = guardian->FindCurrentSpellBySpellId(SPELL_GUARDIAN_DRAIN_LIFE);
    if (!channel)
        channel = guardian->FindCurrentSpellBySpellId(SPELL_GUARDIAN_DRAIN_LIFE_H);

    return channel && channel->getState() == SPELL_STATE_CASTING ? channel : nullptr;
}

bool YoggSaronHealingCut(Unit* guardian)
{
    return guardian->GetMaxNegativeAuraModifier(SPELL_AURA_MOD_HEALING_PCT) <= ULDUAR_YOGG_SARON_HEALING_CUT_PCT;
}

bool YoggSaronBeaconHealOn(Unit* guardian)
{
    return guardian->HasAura(SPELL_EMPOWERING_SHADOWS_25) || guardian->HasAura(SPELL_EMPOWERING_SHADOWS_10);
}
}

Unit* YoggSaronBeaconDragGuardian(PlayerbotAI* botAI, bool claim)
{
    Player* bot = botAI->GetBot();
    YoggSaronEncounterState& state = YoggSaronStateFor(bot);

    if (!YoggSaronInPhase3(botAI) || YoggThorimKeeperActive(botAI))
    {
        state.beaconDrags.clear();
        state.beaconReturns.clear();
        state.beaconExternalClaims.clear();
        return nullptr;
    }

    uint32 const now = getMSTime();
    for (auto it = state.beaconDrags.begin(); it != state.beaconDrags.end();)
    {
        Creature* guardian = ObjectAccessor::GetCreature(*bot, it->first);
        Player* dragger = ObjectAccessor::GetPlayer(*bot, it->second.dragger);
        bool done = !guardian || !guardian->IsAlive() || !dragger || !dragger->IsAlive();

        int32 const channelMs = done ? -1 : YoggSaronDrainLifeElapsed(guardian);
        uint32 const sinceTaunt = getMSTimeDiff(it->second.tauntMs, now);
        bool const switchHidden = it->second.tauntMs && channelMs >= 0 &&
                                  sinceTaunt <= static_cast<uint32>(channelMs) + ULDUAR_YOGG_SARON_BEACON_TAUNT_LAND_MS;
        if (switchHidden)
            it->second.hiddenMs = now;

        bool const victimSettled =
            !switchHidden && (!it->second.hiddenMs ||
                              getMSTimeDiff(it->second.hiddenMs, now) >= ULDUAR_YOGG_SARON_BEACON_TAUNT_LAND_MS);
        if (!done && it->second.tauntMs && guardian->GetVictim() != dragger &&
            sinceTaunt >= ULDUAR_YOGG_SARON_BEACON_TAUNT_CHECK_MS && victimSettled)
        {
            if (RaidObs::Active())
                RaidObs::NoteDerived(dragger, "yogg.beacondrag", "missed");

            it = state.beaconDrags.erase(it);
            continue;
        }

        if (!done && guardian->GetEntry() != NPC_MARKED_IMMORTAL_GUARDIAN)
        {
            if (!it->second.markEndedMs)
                it->second.markEndedMs = now;

            done = guardian->HasAura(SPELL_EMPOWERING_SHADOWS_25) ||
                   guardian->HasAura(SPELL_EMPOWERING_SHADOWS_10) ||
                   getMSTimeDiff(it->second.markEndedMs, now) >= ULDUAR_YOGG_SARON_BEACON_HEAL_WAIT_MS;

            if (done && dragger)
                state.beaconReturns[it->first] = {dragger->GetGUID(), now};
        }

        if (done && dragger && RaidObs::Active())
            RaidObs::NoteDerived(dragger, "yogg.beacondrag", "release");

        it = done ? state.beaconDrags.erase(it) : std::next(it);
    }

    for (auto it = state.beaconReturns.begin(); it != state.beaconReturns.end();)
        it = getMSTimeDiff(it->second.releasedMs, now) > ULDUAR_YOGG_SARON_BEACON_RETURN_MS
                 ? state.beaconReturns.erase(it)
                 : std::next(it);

    for (auto const& [guid, drag] : state.beaconDrags)
        if (drag.dragger == bot->GetGUID())
            return ObjectAccessor::GetCreature(*bot, guid);

    Creature* yogg = YoggSaronNearestCreature(botAI, NPC_YOGG_SARON);
    if (!yogg)
        return nullptr;

    std::list<Creature*> found;
    yogg->GetCreatureListWithEntryInGrid(found, NPC_MARKED_IMMORTAL_GUARDIAN,
                                         ULDUAR_YOGG_SARON_BEACON_DRAG_SEARCH_RADIUS);

    std::vector<Creature*> marked;
    for (Creature* guardian : found)
        if (guardian->IsAlive() && !state.beaconDrags.count(guardian->GetGUID()))
            marked.push_back(guardian);

    if (marked.empty())
        return nullptr;

    std::list<Creature*> unmarked;
    yogg->GetCreatureListWithEntryInGrid(unmarked, NPC_IMMORTAL_GUARDIAN, ULDUAR_YOGG_SARON_BEACON_DRAG_SEARCH_RADIUS);

    std::vector<Player*> const tanks = YoggSaronBeaconDraggerOrder(bot, unmarked, 0);
    std::sort(marked.begin(), marked.end(),
              [](Creature* left, Creature* right) { return left->GetGUID() < right->GetGUID(); });

    for (size_t i = 0; i < marked.size() && i < tanks.size(); ++i)
    {
        if (tanks[i] != bot)
            continue;

        if (claim)
        {
            std::array<bool, YOGG_SARON_BEACON_LANES.size()> taken{};
            for (auto const& [guid, drag] : state.beaconDrags)
                taken[drag.lane] = true;

            auto const clearance = [&unmarked](Position const& spot)
            {
                float nearest = std::numeric_limits<float>::max();
                for (Creature* guardian : unmarked)
                    if (guardian->IsAlive())
                        nearest = std::min(nearest, guardian->GetExactDist2d(spot));

                return nearest;
            };

            uint8 lane = 0;
            auto const staged = state.beaconStages.find(bot->GetGUID());
            if (staged != state.beaconStages.end() && !taken[staged->second])
                lane = staged->second;
            else
            {
                std::tuple<bool, float, bool, float> best;
                for (uint8 index = 0; index < YOGG_SARON_BEACON_LANES.size(); ++index)
                {
                    Position const& spot = *YOGG_SARON_BEACON_LANES[index].drag;
                    auto const key = std::make_tuple(taken[index], -clearance(spot),
                                                     index == YOGG_SARON_BEACON_LANE_NORTHEAST,
                                                     bot->GetExactDist2d(spot));
                    if (!index || key < best)
                    {
                        lane = index;
                        best = key;
                    }
                }
            }

            state.beaconDrags[marked[i]->GetGUID()] = {bot->GetGUID(), 0, lane, 0};
            if (RaidObs::Active())
            {
                RaidObs::NoteDerived(bot, "yogg.beaconspot", YOGG_SARON_BEACON_LANES[lane].name);
                RaidObs::NoteDerived(bot, "yogg.beacondrag", "claim");
            }
        }

        return marked[i];
    }

    return nullptr;
}

void YoggSaronBeaconTaunted(Player* bot)
{
    for (auto& [guid, drag] : YoggSaronStateFor(bot).beaconDrags)
        if (drag.dragger == bot->GetGUID())
        {
            drag.tauntMs = getMSTime();
            drag.hiddenMs = 0;
        }
}

bool YoggSaronBeaconDragging(Player* bot)
{
    for (auto const& [guid, drag] : YoggSaronStateFor(bot).beaconDrags)
        if (drag.dragger == bot->GetGUID())
            return true;

    return false;
}

Position const* YoggSaronBeaconStageSpot(Player* bot)
{
    YoggSaronEncounterState& state = YoggSaronStateFor(bot);
    if (state.markWindow)
        return nullptr;

    auto const it = state.beaconStages.find(bot->GetGUID());
    if (it == state.beaconStages.end())
        return nullptr;

    YoggSaronBeaconLane const& lane = YOGG_SARON_BEACON_LANES[it->second];

    return bot->getClass() == CLASS_HUNTER ? lane.hunter : lane.stage;
}

namespace
{
YoggSaronBeaconLane const& YoggSaronBeaconLaneOf(Player* bot)
{
    for (auto const& [guid, drag] : YoggSaronStateFor(bot).beaconDrags)
        if (drag.dragger == bot->GetGUID())
            return YOGG_SARON_BEACON_LANES[drag.lane];

    return YOGG_SARON_BEACON_LANES.front();
}
}

Position const& YoggSaronBeaconDragSpot(Player* bot) { return *YoggSaronBeaconLaneOf(bot).drag; }

Position const& YoggSaronBeaconHunterSpot(Player* bot) { return *YoggSaronBeaconLaneOf(bot).hunter; }

Unit* YoggSaronBeaconReturning(Player* bot)
{
    uint32 const now = getMSTime();
    for (auto const& [guid, release] : YoggSaronStateFor(bot).beaconReturns)
    {
        if (release.dragger != bot->GetGUID() ||
            getMSTimeDiff(release.releasedMs, now) > ULDUAR_YOGG_SARON_BEACON_RETURN_MS)
            continue;

        Creature* guardian = ObjectAccessor::GetCreature(*bot, guid);
        if (guardian && guardian->IsAlive() && guardian->GetVictim() == bot)
            return guardian;
    }

    return nullptr;
}

uint8 YoggSaronLooseGuardianTier(Player* bot, Unit* guardian)
{
    Player* victim = guardian->GetVictim() ? guardian->GetVictim()->ToPlayer() : nullptr;
    if (!victim)
        return 1;

    if (victim->getClass() == CLASS_HUNTER)
        return guardian->HasAura(SPELL_DISTRACTING_SHOT, victim->GetGUID()) ? 2 : 1;

    auto const& returns = YoggSaronStateFor(bot).beaconReturns;
    auto const release = returns.find(guardian->GetGUID());
    bool const dragged = release != returns.end() && release->second.dragger == victim->GetGUID() &&
                         getMSTimeDiff(release->second.releasedMs, getMSTime()) <= ULDUAR_YOGG_SARON_BEACON_RETURN_MS;

    return dragged ? 0 : 1;
}

namespace
{
bool IsYoggSaronHealedGuardian(Unit* unit)
{
    if (!unit || !unit->IsAlive() || unit->GetHealthPct() < ULDUAR_YOGG_SARON_BEACON_DANGER_HEALTH_PCT)
        return false;

    uint32 const entry = unit->GetEntry();
    if (entry == NPC_MARKED_IMMORTAL_GUARDIAN)
        return true;

    return entry == NPC_IMMORTAL_GUARDIAN &&
           (unit->HasAura(SPELL_EMPOWERING_SHADOWS_25) || unit->HasAura(SPELL_EMPOWERING_SHADOWS_10));
}
}

Unit* YoggSaronHealedGuardianOn(PlayerbotAI* /*botAI*/, Player* victim)
{
    if (!victim)
        return nullptr;

    for (Unit* attacker : victim->getAttackers())
        if (attacker->GetVictim() == victim && IsYoggSaronHealedGuardian(attacker))
            return attacker;

    return nullptr;
}

uint32 YoggSaronGuardiansOn(Player* tank)
{
    if (!tank)
        return 0;

    uint32 count = 0;
    for (Unit* attacker : tank->getAttackers())
        if (attacker->GetVictim() == tank && IsYoggSaronHoldableGuardian(attacker))
            ++count;

    return count;
}

uint32 YoggSaronPileHealedOn(Player* tank)
{
    if (!tank)
        return 0;

    auto const healedByOther = [](Unit* guardian)
    {
        for (uint32 const spell : {SPELL_EMPOWERING_SHADOWS_25, SPELL_EMPOWERING_SHADOWS_10})
        {
            auto const range = guardian->GetAppliedAuras().equal_range(spell);
            for (auto it = range.first; it != range.second; ++it)
                if (it->second->GetBase()->GetCasterGUID() != guardian->GetGUID())
                    return true;
        }

        return false;
    };

    uint32 count = 0;
    for (Unit* attacker : tank->getAttackers())
    {
        uint32 const entry = attacker->GetEntry();
        if (attacker->GetVictim() == tank && attacker->IsAlive() &&
            (entry == NPC_IMMORTAL_GUARDIAN || entry == NPC_MARKED_IMMORTAL_GUARDIAN) && healedByOther(attacker))
            ++count;
    }

    return count;
}

Unit* YoggSaronMortalStrikeTarget(PlayerbotAI* botAI, char const*& reason)
{
    reason = nullptr;
    Player* bot = botAI->GetBot();
    if (YoggSaronOnBrainLevel(bot) || !YoggSaronInPhase3(botAI) || YoggThorimKeeperActive(botAI))
        return nullptr;

    constexpr float searchRadius = 10.0f;
    std::list<Creature*> guardians;
    bot->GetCreatureListWithEntryInGrid(guardians, NPC_IMMORTAL_GUARDIAN, searchRadius);

    bool const gaze = YoggSaronGazeActive(botAI);
    Creature* yogg = gaze ? YoggSaronNearestCreature(botAI, NPC_YOGG_SARON) : nullptr;

    static constexpr char const* reasons[] = {"hot", "drain", "low"};
    Unit* best = nullptr;
    size_t bestTier = 0;
    for (Creature* guardian : guardians)
    {
        if (!guardian->IsAlive() || YoggSaronHealingCut(guardian) || !bot->IsWithinMeleeRange(guardian))
            continue;

        size_t tier = 2;
        if (YoggSaronBeaconHealOn(guardian))
            tier = 0;
        else if (YoggSaronDrainLifeChannelling(guardian))
            tier = 1;
        else if (guardian->GetHealthPct() >= ULDUAR_YOGG_SARON_DRAIN_LIFE_HEALTH_PCT)
            continue;

        if ((gaze && (!yogg || !YoggSaronOutsideGaze(bot, yogg, guardian))) ||
            !YoggSaronGuardianThreatAllows(bot, guardian, guardian->GetGUID() == bot->GetTarget()))
            continue;

        if (!best || std::make_tuple(tier, guardian->GetHealth()) < std::make_tuple(bestTier, best->GetHealth()))
        {
            best = guardian;
            bestTier = tier;
        }
    }

    if (best)
        reason = reasons[bestTier];

    return best;
}

char const* YoggSaronWarlockSummon(PlayerbotAI* botAI)
{
    static constexpr std::pair<char const*, char const*> pets[] = {{"felhunter", "summon felhunter"},
                                                                   {"felguard", "summon felguard"},
                                                                   {"imp", "summon imp"},
                                                                   {"voidwalker", "summon voidwalker"},
                                                                   {"succubus", "summon succubus"}};
    for (auto const& [strategy, summon] : pets)
        if (botAI->HasStrategy(strategy, BOT_STATE_NON_COMBAT))
            return summon;

    return nullptr;
}

namespace
{
struct YoggSaronDot
{
    char const* spell;
    bool refreshEarly;
};

std::vector<YoggSaronDot> YoggSaronDotSpells(PlayerbotAI* botAI)
{
    switch (botAI->GetBot()->getClass())
    {
        case CLASS_WARLOCK:
            if (botAI->HasSpell("unstable affliction"))
                return {{"corruption", true}, {"unstable affliction", true}};
            return {{"corruption", true}, {"immolate", true}};
        case CLASS_PRIEST:
            return {{"vampiric touch", true}, {"shadow word: pain", true}, {"devouring plague", false}};
        case CLASS_DRUID:
            return {{"insect swarm", true}, {"moonfire", true}};
        case CLASS_HUNTER:
            return {{"serpent sting", true}};
        case CLASS_MAGE:
            return {{"living bomb", false}};
        case CLASS_SHAMAN:
            return {{"flame shock", true}};
        default:
            return {};
    }
}
}

char const* YoggSaronPhase3YoggDot(PlayerbotAI* botAI, Unit*& yogg)
{
    yogg = nullptr;
    Player* bot = botAI->GetBot();
    if (!PlayerbotAI::IsRanged(bot) || PlayerbotAI::IsHeal(bot) || PlayerbotAI::IsTank(bot) ||
        YoggSaronOnBrainLevel(bot) || !YoggSaronInPhase3(botAI))
        return nullptr;

    std::vector<YoggSaronDot> const dots = YoggSaronDotSpells(botAI);
    if (dots.empty())
        return nullptr;

    Creature* boss = YoggSaronNearestCreature(botAI, NPC_YOGG_SARON);
    if (!boss || !boss->IsAlive() || boss->HasAura(SPELL_LUNATIC_GAZE_YS))
        return nullptr;

    for (YoggSaronDot const& dot : dots)
    {
        uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", dot.spell)->Get();
        SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
        if (!info || !bot->IsWithinCombatRange(boss, info->GetMaxRange(false, bot)))
            continue;

        if (Aura const* aura = boss->GetAura(spellId, bot->GetGUID()))
        {
            int32 const lead = static_cast<int32>(info->CalcCastTime(bot)) + ULDUAR_YOGG_SARON_PHASE_3_DOT_REFRESH_MS;
            if (!dot.refreshEarly || aura->GetDuration() > lead)
                continue;
        }

        if (!bot->IsWithinLOSInMap(boss))
            return nullptr;

        if (!botAI->CanCastSpell(dot.spell, boss))
            continue;

        yogg = boss;
        return dot.spell;
    }

    return nullptr;
}

bool YoggSaronPhase3Burn(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!PlayerbotAI::IsRanged(bot) || PlayerbotAI::IsHeal(bot) || PlayerbotAI::IsTank(bot) ||
        YoggSaronOnBrainLevel(bot) || !YoggSaronInPhase3(botAI))
        return false;

    Creature* boss = YoggSaronNearestCreature(botAI, NPC_YOGG_SARON);
    bool const burn = boss && boss->IsAlive() && boss->GetHealthPct() < ULDUAR_YOGG_SARON_PHASE_3_BURN_PCT;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.burn", burn ? "burn" : "guardians");

    return burn;
}

Player* YoggSaronBeaconExternalVictim(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    YoggSaronEncounterState& state = YoggSaronStateFor(bot);
    uint32 const now = getMSTime();

    Player* best = nullptr;
    bool bestDragger = false;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMap() != bot->GetMap() ||
            !YoggSaronHealedGuardianOn(botAI, member))
            continue;

        bool const dragger = YoggSaronBeaconDragging(member) || YoggSaronBeaconReturning(member);
        if (PlayerbotAI::IsTank(member) && !dragger &&
            member->GetHealthPct() >= ULDUAR_YOGG_SARON_BEACON_TANK_EXTERNAL_PCT)
            continue;

        auto const claim = state.beaconExternalClaims.find(member->GetGUID());
        if (claim != state.beaconExternalClaims.end() &&
            getMSTimeDiff(claim->second, now) < ULDUAR_YOGG_SARON_BEACON_EXTERNAL_CLAIM_MS)
            continue;

        if (IsYoggSaronBot(member) && (RunningTankDefensive(member, true, 0) || ReadyTankDefensive(member, true)))
            continue;

        if (!best || (dragger && !bestDragger) ||
            (dragger == bestDragger && member->GetHealthPct() < best->GetHealthPct()))
        {
            best = member;
            bestDragger = dragger;
        }
    }

    return best;
}

bool ClaimYoggSaronBeaconExternal(PlayerbotAI* botAI, Player* victim)
{
    if (!victim)
        return false;

    YoggSaronEncounterState& state = YoggSaronStateFor(botAI->GetBot());
    uint32 const now = getMSTime();

    auto const claim = state.beaconExternalClaims.find(victim->GetGUID());
    if (claim != state.beaconExternalClaims.end() &&
        getMSTimeDiff(claim->second, now) < ULDUAR_YOGG_SARON_BEACON_EXTERNAL_CLAIM_MS)
        return false;

    state.beaconExternalClaims[victim->GetGUID()] = now;

    return true;
}

namespace
{
bool YoggSaronTankSwapLive(Player* bot, YoggSaronEncounterState& state)
{
    YoggSaronEncounterState::TankSwap& swap = state.tankSwap;
    if (!swap.startMs)
        return false;

    Player* from = ObjectAccessor::GetPlayer(*bot, swap.from);
    Player* to = ObjectAccessor::GetPlayer(*bot, swap.to);
    if (getMSTimeDiff(swap.startMs, getMSTime()) >= ULDUAR_YOGG_SARON_TANK_SWAP_MS || !from || !from->IsAlive() ||
        !to || !to->IsAlive() || to->GetHealthPct() < ULDUAR_YOGG_SARON_TANK_SWAP_PCT)
    {
        swap = YoggSaronEncounterState::TankSwap();
        return false;
    }

    return true;
}
}

Player* YoggSaronTankSwapHolder(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    YoggSaronEncounterState& state = YoggSaronStateFor(bot);
    if (YoggSaronTankSwapLive(bot, state))
        return state.tankSwap.to == bot->GetGUID() ? ObjectAccessor::GetPlayer(*bot, state.tankSwap.from) : nullptr;

    Group* group = bot->GetGroup();
    if (!group || bot->GetHealthPct() < ULDUAR_YOGG_SARON_TANK_SWAP_TAKER_PCT || YoggSaronBeaconDragging(bot) ||
        YoggSaronBeaconReturning(bot))
        return nullptr;

    uint32 const mine = YoggSaronGuardiansOn(bot);
    Player* holder = nullptr;
    uint32 holderCount = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->GetMap() != bot->GetMap() ||
            !PlayerbotAI::IsTank(member) || member->GetHealthPct() >= ULDUAR_YOGG_SARON_TANK_SWAP_PCT)
            continue;

        uint32 const count = YoggSaronGuardiansOn(member);
        if (count >= ULDUAR_YOGG_SARON_TANK_SWAP_MIN_GUARDIANS && count > mine && count > holderCount)
        {
            holder = member;
            holderCount = count;
        }
    }

    return holder;
}

void YoggSaronTankSwapTake(PlayerbotAI* botAI, Player* holder)
{
    Player* bot = botAI->GetBot();
    YoggSaronEncounterState& state = YoggSaronStateFor(bot);
    if (!holder || YoggSaronTankSwapLive(bot, state))
        return;

    state.tankSwap = {holder->GetGUID(), bot->GetGUID(), getMSTime()};
    if (RaidObs::Active())
        RaidObs::Note(bot, "yogg.tankswap",
                      "take " + holder->GetName() + " " + std::to_string(YoggSaronGuardiansOn(holder)));
}

bool YoggSaronTankShedding(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    YoggSaronEncounterState& state = YoggSaronStateFor(bot);

    return state.tankSwap.from == bot->GetGUID() && YoggSaronTankSwapLive(bot, state);
}

bool YoggSaronTankLowHold(Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group || bot->GetHealthPct() >= ULDUAR_YOGG_SARON_TANK_SWAP_PCT)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member != bot && member->IsAlive() && member->GetMap() == bot->GetMap() &&
            PlayerbotAI::IsTank(member) && member->GetHealthPct() >= ULDUAR_YOGG_SARON_TANK_SWAP_TAKER_PCT)
            return true;
    }

    return false;
}

bool YoggSaronTankWrathHeld(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    return bot->getClass() == CLASS_PALADIN &&
           ReadyTankDefensive(bot, true, ULDUAR_YOGG_SARON_TANK_WRATH_LOCK_MS) && YoggSaronInPhase3(botAI);
}

bool YoggSaronBeforePhase3(PlayerbotAI* botAI)
{
    uint32 const phase = YoggSaronPhase(botAI);

    return phase == 1 || phase == 2;
}

Player* YoggSaronTankLayOnHandsTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group || bot->getClass() != CLASS_PALADIN ||
        bot->GetPositionZ() < ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
        return nullptr;

    YoggSaronEncounterState& state = YoggSaronStateFor(bot);
    uint32 const now = getMSTime();

    Player* best = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMap() != bot->GetMap() || !PlayerbotAI::IsTank(member) ||
            member->GetHealthPct() >= ULDUAR_YOGG_SARON_TANK_LOH_PCT ||
            !bot->IsWithinDistInMap(member, ULDUAR_YOGG_SARON_TANK_LOH_RANGE))
            continue;

        auto const claim = state.tankLohClaims.find(member->GetGUID());
        if (claim != state.tankLohClaims.end() && claim->second.first != bot->GetGUID() &&
            getMSTimeDiff(claim->second.second, now) < ULDUAR_YOGG_SARON_TANK_LOH_CLAIM_MS)
            continue;

        if ((!best || member->GetHealthPct() < best->GetHealthPct()) && botAI->CanCastSpell("lay on hands", member))
            best = member;
    }

    return best;
}

bool ClaimYoggSaronTankLayOnHands(PlayerbotAI* botAI, Player* tank)
{
    Player* bot = botAI->GetBot();
    YoggSaronEncounterState& state = YoggSaronStateFor(bot);
    uint32 const now = getMSTime();

    auto const claim = state.tankLohClaims.find(tank->GetGUID());
    if (claim != state.tankLohClaims.end() && claim->second.first != bot->GetGUID() &&
        getMSTimeDiff(claim->second.second, now) < ULDUAR_YOGG_SARON_TANK_LOH_CLAIM_MS)
        return false;

    state.tankLohClaims[tank->GetGUID()] = {bot->GetGUID(), now};

    return true;
}

Player* YoggSaronVigilanceTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group || bot->getClass() != CLASS_WARRIOR || !PlayerbotAI::IsTank(bot) ||
        !bot->HasSpell(SPELL_WARRIOR_VIGILANCE) || !YoggSaronInPhase3(botAI))
        return nullptr;

    Player* best = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->GetMap() != bot->GetMap() ||
            !PlayerbotAI::IsTank(member))
            continue;

        if (member->HasAura(SPELL_WARRIOR_VIGILANCE, bot->GetGUID()))
            return nullptr;

        if (member->HasAura(SPELL_WARRIOR_VIGILANCE) ||
            !bot->IsWithinDistInMap(member, ULDUAR_YOGG_SARON_VIGILANCE_RANGE) ||
            !botAI->CanCastSpell("vigilance", member))
            continue;

        if (!best || member->getAttackers().size() > best->getAttackers().size())
            best = member;
    }

    return best;
}

bool YoggSaronHealerTargetless(PlayerbotAI* botAI)
{
    return botAI->GetBot()->IsInCombat() &&
           !botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get() && YoggSaronEncounterActive(botAI);
}

bool YoggSaronBeaconImmunityHeld(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->getClass() != CLASS_PALADIN && bot->getClass() != CLASS_MAGE)
        return false;

    return YoggSaronHealedGuardianOn(botAI, bot) != nullptr;
}

bool YoggSaronBeaconHunterHold(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    return bot->getClass() == CLASS_HUNTER && YoggSaronBeaconDragging(bot);
}

bool YoggSaronBeaconBearHeld(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->getClass() != CLASS_DRUID)
        return false;

    bool const held = YoggSaronStateFor(bot).beaconStages.count(bot->GetGUID()) || YoggSaronBeaconDragging(bot) ||
                      YoggSaronBeaconReturning(bot);

    return held && YoggSaronInPhase3(botAI);
}

bool YoggSaronBearReady(Player* druid)
{
    if (druid->GetShapeshiftForm() == FORM_BEAR || druid->GetShapeshiftForm() == FORM_DIREBEAR)
        return true;

    for (uint32 const spell : {SPELL_DRUID_DIRE_BEAR_FORM, SPELL_DRUID_BEAR_FORM})
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spell);
        if (!spellInfo || !druid->HasSpell(spell))
            continue;

        int32 const cost = spellInfo->CalcPowerCost(druid, spellInfo->GetSchoolMask());
        if (static_cast<int32>(druid->GetPower(POWER_MANA)) >= cost)
            return true;
    }

    return false;
}

namespace
{
uint32 YoggSaronMsToRoar(YoggSaronEncounterState const& state, uint32 now)
{
    if (!state.phase3StartMs)
        return std::numeric_limits<uint32>::max();

    uint32 const next = state.lastRoarMs ? state.lastRoarMs + ULDUAR_YOGG_SARON_ROAR_PERIOD_MS
                                         : state.phase3StartMs + ULDUAR_YOGG_SARON_ROAR_FIRST_MS;

    return next > now ? next - now : 0;
}
}

bool YoggSaronRoarExpected(PlayerbotAI* botAI) { return YoggSaronStateFor(botAI->GetBot()).roarExpected; }

bool YoggSaronRoarCasting(PlayerbotAI* botAI)
{
    Creature* yogg = YoggSaronNearestCreature(botAI, NPC_YOGG_SARON);
    if (!yogg || !yogg->IsAlive() || !yogg->HasUnitState(UNIT_STATE_CASTING))
        return false;

    Spell* cast = yogg->GetCurrentSpell(CURRENT_GENERIC_SPELL);

    return cast && cast->m_spellInfo->Id == SPELL_DEAFENING_ROAR;
}

Player* YoggSaronRoarPaladin(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    YoggSaronEncounterState& state = YoggSaronStateFor(bot);
    Group* group = bot->GetGroup();
    if (!state.roarExpected || !group)
        return nullptr;

    uint32 const msToRoar = YoggSaronRoarCasting(botAI) ? 0 : YoggSaronMsToRoar(state, getMSTime());

    struct Candidate
    {
        Player* player;
        bool ready;
        bool dragging;
        bool aura;
    };
    std::vector<Candidate> candidates;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!IsYoggSaronBot(member) || member->getClass() != CLASS_PALADIN || !member->IsAlive() ||
            member->GetMap() != bot->GetMap() ||
            member->GetPositionZ() < ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT ||
            !member->HasSpell(SPELL_AURA_MASTERY) || !member->HasSpell(SPELL_CONCENTRATION_AURA))
            continue;

        candidates.push_back({member, member->GetSpellCooldownDelay(SPELL_AURA_MASTERY) <= msToRoar,
                              YoggSaronBeaconDragging(member) || YoggSaronBeaconReturning(member) != nullptr,
                              member->HasAura(SPELL_CONCENTRATION_AURA, member->GetGUID())});
    }

    auto const best = std::min_element(candidates.begin(), candidates.end(),
                                       [](Candidate const& left, Candidate const& right)
                                       {
                                           return std::make_tuple(!left.ready, left.dragging, !left.aura,
                                                                  left.player->GetGUID()) <
                                                  std::make_tuple(!right.ready, right.dragging, !right.aura,
                                                                  right.player->GetGUID());
                                       });

    return best == candidates.end() ? nullptr : best->player;
}

bool YoggSaronRoarAuraHeld(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->getClass() != CLASS_PALADIN)
        return false;

    YoggSaronEncounterState& state = YoggSaronStateFor(bot);
    if (!state.roarExpected ||
        (YoggSaronMsToRoar(state, getMSTime()) > ULDUAR_YOGG_SARON_ROAR_PRESWITCH_MS && !YoggSaronRoarCasting(botAI)))
        return false;

    return YoggSaronRoarPaladin(botAI) == bot;
}

bool ClaimYoggSaronRoar(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    YoggSaronEncounterState& state = YoggSaronStateFor(bot);
    if (!state.lastRoarMs || !YoggSaronRoarCasting(botAI))
        return false;

    if (state.roarClaimKey == state.lastRoarMs)
        return state.roarClaimOwner == bot->GetGUID();

    if (YoggSaronRoarPaladin(botAI) != bot &&
        getMSTimeDiff(state.lastRoarMs, getMSTime()) < ULDUAR_YOGG_SARON_ROAR_HANDOFF_MS)
        return false;

    state.roarClaimKey = state.lastRoarMs;
    state.roarClaimOwner = bot->GetGUID();

    return true;
}

bool YoggSaronGazeActive(PlayerbotAI* botAI)
{
    Creature* yogg = YoggSaronNearestCreature(botAI, NPC_YOGG_SARON);

    return yogg && yogg->IsAlive() && yogg->HasAura(SPELL_LUNATIC_GAZE_YS);
}

bool YoggSaronOutsideGaze(Player* bot, Unit* yogg, Unit* target)
{
    if (!bot || !yogg || !target)
        return false;

    float const pi = static_cast<float>(M_PI);
    float const offset = Position::NormalizeOrientation(bot->GetAngle(target) - bot->GetAngle(yogg));
    float const separation = offset > pi ? 2.0f * pi - offset : offset;

    return separation >= ULDUAR_YOGG_SARON_GAZE_SAFE_SEPARATION;
}

bool YoggSaronPhase3LustReady(PlayerbotAI* botAI)
{
    if (!YoggSaronInPhase3(botAI))
        return false;

    Player* bot = botAI->GetBot();
    YoggSaronEncounterState& state = YoggSaronStateFor(bot);
    uint32 const now = getMSTime();
    if (!state.phase3StartMs)
        state.phase3StartMs = now;

    if (getMSTimeDiff(state.phase3StartMs, now) >= ULDUAR_YOGG_SARON_LUST_FALLBACK_MS)
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "yogg.lustready", "timeout");

        return true;
    }

    Group* group = bot->GetGroup();
    if (!group)
        return true;

    uint32 living = 0;
    uint32 placed = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMap() != bot->GetMap())
            continue;

        ++living;
        if (member->GetPositionZ() < ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
            continue;

        if (member->GetExactDist2d(YoggSaronPhaseThreeSpot(member)) <= ULDUAR_YOGG_SARON_PHASE_3_STATION_RADIUS)
        {
            ++placed;
            continue;
        }

        Unit* target = member->isMoving() ? nullptr : ObjectAccessor::GetUnit(*member, member->GetTarget());
        if (!target || !target->IsAlive())
            continue;

        bool const inReach = PlayerbotAI::IsRanged(member)
                                 ? member->GetDistance(target) <= sPlayerbotAIConfig.SpellDistance
                                 : member->IsWithinMeleeRange(target);
        if (inReach)
            ++placed;
    }

    bool const ready = living && placed >= ULDUAR_YOGG_SARON_LUST_POSITION_SHARE * living;
    if (ready && RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.lustready", "position");

    return ready;
}

bool YoggSaronGuardianThreatAllows(Player* bot, Unit* guardian, bool holding)
{
    Player* tank = guardian->GetVictim() ? guardian->GetVictim()->ToPlayer() : nullptr;
    if (!tank || !PlayerbotAI::IsTank(tank))
    {
        Group* group = bot->GetGroup();
        if (!group)
            return true;

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member->IsAlive() && PlayerbotAI::IsTank(member))
                return false;
        }

        return true;
    }

    float const share =
        holding ? ULDUAR_YOGG_SARON_GUARDIAN_THREAT_KEEP_SHARE : ULDUAR_YOGG_SARON_GUARDIAN_THREAT_START_SHARE;

    return guardian->GetThreatMgr().GetThreat(bot) < share * guardian->GetThreatMgr().GetThreat(tank);
}

float YoggSaronTankThreatLead(Player* tank, Unit* guardian)
{
    float highest = 0.0f;
    for (ThreatReference const* ref : guardian->GetThreatMgr().GetUnsortedThreatList())
    {
        Player* player = ref->GetVictim() ? ref->GetVictim()->ToPlayer() : nullptr;
        if (player && PlayerbotAI::IsTank(player))
            continue;

        highest = std::max(highest, ref->GetThreat());
    }

    float const own = guardian->GetThreatMgr().GetThreat(tank);
    if (highest <= 0.0f)
        return own > 0.0f ? std::numeric_limits<float>::max() : 0.0f;

    return own / highest;
}

std::vector<float> YoggSaronPileBearings(Player* bot)
{
    std::vector<float> bearings;
    for (Unit* attacker : bot->getAttackers())
        if (attacker->GetVictim() == bot && IsYoggSaronHoldableGuardian(attacker) &&
            attacker->IsWithinMeleeRange(bot))
            bearings.push_back(bot->GetAngle(attacker));

    return bearings;
}

std::optional<YoggSaronPileTurnPlan> YoggSaronPileTurn(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!PlayerbotAI::IsTank(bot) || !YoggSaronInPhase3(botAI) || bot->isMoving() || YoggSaronBeaconDragging(bot) ||
        YoggSaronGazeActive(botAI))
        return std::nullopt;

    std::vector<float> const pile = YoggSaronPileBearings(bot);
    if (pile.empty())
        return std::nullopt;

    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    std::optional<float> targetBearing;
    if (target && target != bot && target->IsAlive())
        targetBearing = bot->GetAngle(target);

    float const facing = bot->GetOrientation();
    std::optional<YoggSaronPile::Facing> const best = YoggSaronPile::Best(pile, targetBearing, std::nullopt, facing);
    int const before = YoggSaronPile::Covered(pile, facing);
    if (!best || best->covered <= before)
        return std::nullopt;

    return YoggSaronPileTurnPlan{Position::NormalizeOrientation(best->heading), before, best->covered};
}

Unit* YoggSaronPileTarget(Player* bot, std::vector<Unit*> const& candidates, Unit* current)
{
    std::vector<float> const pile = YoggSaronPileBearings(bot);
    float const facing = bot->GetOrientation();

    Unit* pick = nullptr;
    int pickCovered = -1;
    for (Unit* candidate : candidates)
    {
        std::optional<YoggSaronPile::Facing> const best =
            YoggSaronPile::Best(pile, bot->GetAngle(candidate), std::nullopt, facing);
        int const covered = best ? best->covered : 0;
        if (covered > pickCovered || (covered == pickCovered && candidate == current))
        {
            pick = candidate;
            pickCovered = covered;
        }
    }

    return pick;
}

bool YoggSaronConstrictorHolding(Unit* unit)
{
    if (!unit || !unit->IsAlive() || unit->GetEntry() != NPC_CONSTRICTOR_TENTACLE)
        return false;

    Vehicle* seat = unit->GetVehicleKit();
    return seat && seat->IsVehicleInUse();
}

namespace
{
struct YoggSaronWalkLatch
{
    Position destination;
    float bestDistance = 0.0f;
    uint32 lastProgressMs = 0;
    uint32 lastAskedMs = 0;
    uint32 gaveUpMs = 0;
};

using YoggSaronWalkLatches =
    std::unordered_map<ObjectGuid, std::unordered_map<std::string, std::vector<YoggSaronWalkLatch>>>;
RaidInstanceState<YoggSaronWalkLatches> yoggSaronWalkLatches;
}

bool YoggSaronWalkMakingProgress(PlayerbotAI* botAI, char const* node, Position const& destination)
{
    Player* bot = botAI->GetBot();
    uint32 const now = getMSTime();
    float const distance = bot->GetExactDist(destination);

    bool giveUp = false;
    char const* branch = "walking";
    std::vector<YoggSaronWalkLatch>& latches = yoggSaronWalkLatches.For(bot->GetInstanceId())[bot->GetGUID()][node];

    YoggSaronWalkLatch* latch = nullptr;
    for (YoggSaronWalkLatch& candidate : latches)
    {
        if (candidate.destination.GetExactDist(destination) < 1.0f)
        {
            latch = &candidate;
            break;
        }
    }

    if (!latch)
    {
        latches.push_back(YoggSaronWalkLatch{destination, distance, now, now});
        latch = &latches.back();
    }

    bool const fresh = getMSTimeDiff(latch->lastAskedMs, now) >= ULDUAR_YOGG_SARON_WALK_GIVE_UP_MS;
    bool const arrived = distance <= ULDUAR_YOGG_SARON_WALK_ARRIVED_RADIUS;
    latch->lastAskedMs = now;

    if (latch->gaveUpMs && getMSTimeDiff(latch->gaveUpMs, now) >= ULDUAR_YOGG_SARON_WALK_RETRY_MS)
    {
        latch->gaveUpMs = 0;
        latch->bestDistance = distance;
        latch->lastProgressMs = now;
    }

    if (latch->gaveUpMs)
    {
        giveUp = true;
        branch = "gaveup";
    }
    else if (fresh || arrived || distance < latch->bestDistance)
    {
        latch->bestDistance = distance;
        latch->lastProgressMs = now;
        branch = arrived ? "arrived" : "walking";
    }
    else if (getMSTimeDiff(latch->lastProgressMs, now) >= ULDUAR_YOGG_SARON_WALK_GIVE_UP_MS)
    {
        latch->gaveUpMs = now;
        giveUp = true;
        branch = "gaveup";
    }

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.walk", std::string(node) + " " + branch);

    return !giveUp;
}

void YoggSaronReleaseStalledWalk(PlayerbotAI* botAI, char const* node, Position const& destination)
{
    Player* bot = botAI->GetBot();

    LastMovement& last = botAI->GetAiObjectContext()->GetValue<LastMovement&>("last movement")->Get();
    if (bot->isMoving() || !last.msTime ||
        getMSTimeDiff(last.msTime, getMSTime()) <= ULDUAR_YOGG_SARON_WALK_STALL_MS)
        return;

    if (RaidObs::Active())
        RaidObs::Note(bot, "yogg.walkstep",
                      std::string(node) + " " + std::to_string(static_cast<int32>(bot->GetExactDist(destination))) +
                          " yd short");

    last.clear();
}

bool YoggSaronMarkCaster(Unit* caster)
{
    return caster->GetEntry() == NPC_MARKED_IMMORTAL_GUARDIAN ||
           yoggSaronStates.For(caster->GetInstanceId()).beaconDrags.count(caster->GetGUID());
}

bool YoggSaronMarkKicker(Player* bot)
{
    YoggSaronEncounterState const& state = YoggSaronStateFor(bot);

    return state.markWindow &&
           std::find(state.markKickers.begin(), state.markKickers.end(), bot->GetGUID()) != state.markKickers.end();
}

namespace
{
std::vector<Unit*> YoggSaronInterruptCastersAround(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    std::list<Creature*> guardians;
    bot->GetCreatureListWithEntryInGrid(guardians, NPC_GUARDIAN_OF_YS, ULDUAR_YOGG_SARON_INTERRUPT_SEARCH_RADIUS);

    std::vector<Unit*> casters;
    for (Creature* guardian : guardians)
    {
        if (!guardian->IsAlive())
            continue;

        if (guardian->FindCurrentSpellBySpellId(SPELL_DARK_VOLLEY) ||
            guardian->FindCurrentSpellBySpellId(SPELL_DARK_VOLLEY_H))
            casters.push_back(guardian);
    }

    if (!YoggSaronInPhase3(botAI))
        return casters;

    std::vector<std::pair<int32, Unit*>> drains;
    for (uint32 const entry : {NPC_IMMORTAL_GUARDIAN, NPC_MARKED_IMMORTAL_GUARDIAN})
    {
        std::list<Creature*> immortals;
        bot->GetCreatureListWithEntryInGrid(immortals, entry, ULDUAR_YOGG_SARON_INTERRUPT_SEARCH_RADIUS);
        for (Creature* guardian : immortals)
        {
            Spell* channel = guardian->IsAlive() ? YoggSaronDrainLifeKickable(guardian) : nullptr;
            if (channel && channel->GetCastTimeRemaining() >= ULDUAR_YOGG_SARON_KICK_MIN_LEFT_MS)
                drains.emplace_back(channel->GetCastTimeRemaining(), guardian);
        }
    }

    std::stable_sort(drains.begin(), drains.end(), [](auto const& a, auto const& b) { return a.first > b.first; });
    for (auto const& [left, guardian] : drains)
        casters.push_back(guardian);

    std::stable_partition(casters.begin(), casters.end(), YoggSaronMarkCaster);

    return casters;
}
}

std::vector<Unit*> GetYoggSaronInterruptCasters(PlayerbotAI* botAI)
{
    std::vector<Unit*> casters = YoggSaronInterruptCastersAround(botAI);

    if (YoggSaronMarkKicker(botAI->GetBot()))
        casters.erase(std::find_if_not(casters.begin(), casters.end(), YoggSaronMarkCaster), casters.end());

    return casters;
}

std::vector<char const*> YoggSaronInterruptSpells(Player* bot)
{
    switch (bot->getClass())
    {
        case CLASS_DEATH_KNIGHT:
            return {"mind freeze", "strangulate"};
        case CLASS_HUNTER:
            return {"silencing shot"};
        case CLASS_MAGE:
            return {"counterspell"};
        case CLASS_ROGUE:
            return {"kick"};
        case CLASS_SHAMAN:
            return {"wind shear"};
        case CLASS_WARRIOR:
            return {"pummel", "shield bash"};
        case CLASS_WARLOCK:
            return {"spell lock"};
        default:
            if (bot->getRace() == RACE_BLOODELF)
                return {"arcane torrent"};

            return {};
    }
}

bool YoggSaronCanInterrupt(Player* bot) { return !YoggSaronInterruptSpells(bot).empty(); }

namespace
{
bool YoggSaronKickInReach(PlayerbotAI* botAI, char const* spell, Unit* caster)
{
    Player* bot = botAI->GetBot();
    uint32 const id = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", spell)->Get();
    SpellInfo const* info = id ? sSpellMgr->GetSpellInfo(id) : nullptr;
    if (!info || bot->HasSpellCooldown(id) || info->CheckShapeshift(bot->GetShapeshiftForm()) != SPELL_CAST_OK ||
        !bot->HasItemFitToSpellRequirements(info))
        return false;

    if (info->PowerType < MAX_POWERS &&
        info->CalcPowerCost(bot, info->GetSchoolMask()) > static_cast<int32>(bot->GetPower(Powers(info->PowerType))))
        return false;

    if (info->RangeEntry && (info->RangeEntry->Flags & SPELL_RANGE_MELEE))
        return bot->IsWithinMeleeRange(caster);

    return bot->IsWithinCombatRange(caster, bot->GetSpellMaxRangeForTarget(caster, info));
}
}

bool YoggSaronInterruptReady(PlayerbotAI* botAI, char const* spell, Unit* caster)
{
    if (!YoggSaronKickInReach(botAI, spell, caster))
        return false;

    return botAI->CanCastSpell(botAI->GetAiObjectContext()->GetValue<uint32>("spell id", spell)->Get(), caster);
}

std::vector<char const*> YoggSaronInterruptSpellsFor(Player* bot, Unit* caster)
{
    std::vector<char const*> spells = YoggSaronInterruptSpells(bot);

    uint32 const entry = caster ? caster->GetEntry() : 0;
    if (entry != NPC_IMMORTAL_GUARDIAN && entry != NPC_MARKED_IMMORTAL_GUARDIAN)
        return spells;

    spells.erase(std::remove_if(spells.begin(), spells.end(),
                                [](char const* spell)
                                {
                                    std::string const name(spell);
                                    return name == "strangulate" || name == "silencing shot" ||
                                           name == "arcane torrent";
                                }),
                 spells.end());

    return spells;
}

bool YoggSaronInterruptClaimedByOther(Player* bot, Unit* caster)
{
    YoggSaronEncounterState& state = YoggSaronStateFor(bot);
    auto const it = state.interruptClaims.find(caster->GetGUID());
    if (it == state.interruptClaims.end())
        return false;

    if (it->second.first == bot->GetGUID() ||
        getMSTimeDiff(it->second.second, getMSTime()) >= ULDUAR_YOGG_SARON_INTERRUPT_CLAIM_MS)
        return false;

    return true;
}

void ClaimYoggSaronInterrupt(Player* bot, Unit* caster)
{
    YoggSaronEncounterState& state = YoggSaronStateFor(bot);
    uint32 const now = getMSTime();

    for (auto it = state.interruptClaims.begin(); it != state.interruptClaims.end();)
        it = getMSTimeDiff(it->second.second, now) >= ULDUAR_YOGG_SARON_INTERRUPT_CLAIM_MS
                 ? state.interruptClaims.erase(it)
                 : std::next(it);

    state.interruptClaims[caster->GetGUID()] = {bot->GetGUID(), now};
}

namespace
{
bool YoggSaronPowerChannelling(Unit* crusher)
{
    Spell const* channel = crusher->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
    return channel && channel->GetSpellInfo()->Id == SPELL_DIMINISH_POWER &&
           channel->getState() == SPELL_STATE_CASTING;
}
}

std::vector<Unit*> GetYoggSaronChannellingCrushers(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    std::list<Creature*> crushers;
    bot->GetCreatureListWithEntryInGrid(crushers, NPC_CRUSHER_TENTACLE, ULDUAR_YOGG_SARON_CRUSH_RANGE);

    std::vector<Unit*> channelling;
    for (Creature* crusher : crushers)
        if (crusher->IsAlive() && YoggSaronPowerChannelling(crusher))
            channelling.push_back(crusher);

    return channelling;
}

std::vector<char const*> YoggSaronJudgementSpells(Player* bot)
{
    if (bot->getClass() != CLASS_PALADIN)
        return {};

    return {"judgement of wisdom", "judgement of light", "judgement of justice"};
}

namespace
{
bool YoggSaronPowerBreaker(Player* member, Player* bot)
{
    return IsYoggSaronBot(member) && member->IsAlive() && member->GetMap() == bot->GetMap() &&
           PlayerbotAI::IsTank(member) && YoggSaronRoomOf(member) == YOGG_SARON_ROOM_ARENA &&
           !member->GetVehicle() && !member->IsCharmed() &&
           !member->HasUnitState(UNIT_STATE_STUNNED | UNIT_STATE_FLEEING | UNIT_STATE_CONFUSED);
}

bool YoggSaronPowerCarriedBelow(Player* bot, Unit* crusher)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && member->GetMap() == bot->GetMap() &&
            member->HasAura(SPELL_ILLUSION_ROOM) && member->HasAura(SPELL_DIMINISH_POWER, crusher->GetGUID()))
            return true;
    }

    return false;
}

bool YoggSaronPowerSpotOnPlatform(float x, float y)
{
    float const fromMiddle = ULDUAR_YOGG_SARON_MIDDLE.GetExactDist2d(x, y);
    return fromMiddle >= ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS &&
           fromMiddle <= ULDUAR_YOGG_SARON_P2_SPACING_MAX_FROM_MIDDLE;
}

Position YoggSaronPowerBreakSpot(Player* bot, Creature* crusher)
{
    float const standoff = crusher->GetMeleeRange(bot) - ULDUAR_YOGG_SARON_POWER_BREAK_MELEE_SLACK;
    float const arc = ULDUAR_YOGG_SARON_CRUSH_CLEAR_ARC * static_cast<float>(M_PI) / 180.0f;

    std::vector<Position> raid;
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member != bot && member->IsAlive() && member->GetMap() == bot->GetMap() &&
                YoggSaronRoomOf(member) == YOGG_SARON_ROOM_ARENA &&
                crusher->GetExactDist2d(member) <= ULDUAR_YOGG_SARON_CRUSH_RANGE)
                raid.push_back(member->GetPosition());
        }
    }

    auto const at = [crusher](float bearing, float distance)
    {
        return Position(crusher->GetPositionX() + std::cos(bearing) * distance,
                        crusher->GetPositionY() + std::sin(bearing) * distance, crusher->GetPositionZ());
    };

    bool found = false;
    Position best = at(crusher->GetAngle(bot), standoff);
    std::size_t bestInCone = 0;
    float bestWalk = 0.0f;
    for (uint32 step = 0; step < ULDUAR_YOGG_SARON_POWER_BREAK_BEARINGS; ++step)
    {
        float const bearing = 2.0f * static_cast<float>(M_PI) * step / ULDUAR_YOGG_SARON_POWER_BREAK_BEARINGS;
        Position const spot = at(bearing, standoff);
        Position const exit = at(bearing, ULDUAR_YOGG_SARON_POWER_BREAK_OUT_RADIUS);
        if (!YoggSaronPowerSpotOnPlatform(spot.GetPositionX(), spot.GetPositionY()) ||
            !YoggSaronPowerSpotOnPlatform(exit.GetPositionX(), exit.GetPositionY()))
            continue;

        std::size_t const inCone = static_cast<std::size_t>(std::count_if(
            raid.begin(), raid.end(),
            [crusher, bearing, arc](Position const& member)
            {
                float offset = Position::NormalizeOrientation(crusher->GetAngle(&member) - bearing);
                if (offset > static_cast<float>(M_PI))
                    offset = 2.0f * static_cast<float>(M_PI) - offset;

                return offset <= arc;
            }));
        float const walk = bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY());
        if (!found || inCone < bestInCone || (inCone == bestInCone && walk < bestWalk))
        {
            found = true;
            best = spot;
            bestInCone = inCone;
            bestWalk = walk;
        }
    }

    return best;
}

Position YoggSaronPowerBreakExit(Player* bot, Creature* crusher)
{
    float const toward = crusher->GetAngle(bot);
    constexpr float turns[] = {0.0f, 0.5236f, -0.5236f, 1.0472f, -1.0472f, 1.5708f, -1.5708f};

    for (float const turn : turns)
    {
        float const bearing = toward + turn;
        float const x = crusher->GetPositionX() + std::cos(bearing) * ULDUAR_YOGG_SARON_POWER_BREAK_OUT_RADIUS;
        float const y = crusher->GetPositionY() + std::sin(bearing) * ULDUAR_YOGG_SARON_POWER_BREAK_OUT_RADIUS;
        if (YoggSaronPowerSpotOnPlatform(x, y))
            return Position(x, y, bot->GetPositionZ());
    }

    return Position(crusher->GetPositionX() + std::cos(toward) * ULDUAR_YOGG_SARON_POWER_BREAK_OUT_RADIUS,
                    crusher->GetPositionY() + std::sin(toward) * ULDUAR_YOGG_SARON_POWER_BREAK_OUT_RADIUS,
                    bot->GetPositionZ());
}

Creature* YoggSaronPowerBreakClaim(Player* bot, YoggSaronEncounterState& state)
{
    std::list<Creature*> crushers;
    bot->GetCreatureListWithEntryInGrid(crushers, NPC_CRUSHER_TENTACLE, ULDUAR_YOGG_SARON_POWER_BREAK_SEARCH_RADIUS);
    crushers.remove_if(
        [bot](Creature* crusher)
        {
            return !crusher->IsAlive() || !YoggSaronPowerChannelling(crusher) ||
                   !YoggSaronPowerCarriedBelow(bot, crusher);
        });
    if (crushers.empty())
        return nullptr;

    std::vector<Player*> unclaimed{bot};
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member != bot && YoggSaronPowerBreaker(member, bot) &&
                !state.powerBreaks.count(member->GetGUID()))
                unclaimed.push_back(member);
        }
    }

    auto const taken = [bot, &state](Creature* crusher)
    {
        for (auto const& [owner, held] : state.powerBreaks)
        {
            if (held.out || held.crusher != crusher->GetGUID() || owner == bot->GetGUID())
                continue;

            Player* tank = ObjectAccessor::GetPlayer(*bot, owner);
            if (tank && YoggSaronPowerBreaker(tank, bot))
                return true;
        }

        return false;
    };

    Creature* pick = nullptr;
    for (Creature* crusher : crushers)
    {
        if (taken(crusher))
            continue;

        Player* nearest = nullptr;
        for (Player* tank : unclaimed)
        {
            float const gap = tank->GetExactDist2d(crusher);
            float const best = nearest ? nearest->GetExactDist2d(crusher) : 0.0f;
            if (!nearest || gap < best || (!(best < gap) && tank->GetGUID() < nearest->GetGUID()))
                nearest = tank;
        }

        if (nearest == bot && (!pick || bot->GetExactDist2d(crusher) < bot->GetExactDist2d(pick)))
            pick = crusher;
    }

    if (!pick)
        return nullptr;

    YoggSaronEncounterState::PowerBreak& claim = state.powerBreaks[bot->GetGUID()];
    claim = YoggSaronEncounterState::PowerBreak();
    claim.crusher = pick->GetGUID();
    claim.spot = YoggSaronPowerBreakSpot(bot, pick);

    return pick;
}
}

YoggSaronPowerBreakStep YoggSaronPowerBreakPlan(PlayerbotAI* botAI, Unit*& target, Position& spot)
{
    Player* bot = botAI->GetBot();
    target = nullptr;
    if (!PlayerbotAI::IsTank(bot))
        return YOGG_SARON_POWER_BREAK_NONE;

    YoggSaronEncounterState& state = YoggSaronStateFor(bot);
    bool const live = YoggSaronInPhase2(botAI) && YoggSaronPowerBreaker(bot, bot);

    YoggSaronPowerBreakStep step = YOGG_SARON_POWER_BREAK_NONE;
    Creature* crusher = nullptr;

    auto const own = state.powerBreaks.find(bot->GetGUID());
    if (own != state.powerBreaks.end())
    {
        YoggSaronEncounterState::PowerBreak& held = own->second;
        crusher = ObjectAccessor::GetCreature(*bot, held.crusher);
        bool const usable = live && crusher && crusher->IsAlive() && !crusher->HasAura(SPELL_SHATTERED_ILLUSION);
        bool const inReach =
            usable && bot->GetExactDist2d(crusher) < ULDUAR_YOGG_SARON_CRUSHER_REACH_CLEAR_RADIUS;

        if (!usable)
            step = YOGG_SARON_POWER_BREAK_NONE;
        else if (held.out)
            step = inReach && getMSTimeDiff(held.outMs, getMSTime()) < ULDUAR_YOGG_SARON_POWER_BREAK_OUT_MS
                       ? YOGG_SARON_POWER_BREAK_OUT
                       : YOGG_SARON_POWER_BREAK_NONE;
        else if (YoggSaronPowerChannelling(crusher) && YoggSaronPowerCarriedBelow(bot, crusher))
            step = bot->GetExactDist2d(held.spot) <= ULDUAR_YOGG_SARON_POWER_BREAK_ARRIVED_RADIUS
                       ? YOGG_SARON_POWER_BREAK_HIT
                       : YOGG_SARON_POWER_BREAK_WALK;
        else if (inReach)
        {
            held.out = true;
            held.outMs = getMSTime();
            held.exit = YoggSaronPowerBreakExit(bot, crusher);
            step = YOGG_SARON_POWER_BREAK_OUT;
        }

        if (step == YOGG_SARON_POWER_BREAK_NONE)
        {
            state.powerBreaks.erase(own);
            crusher = nullptr;
        }
        else
            spot = step == YOGG_SARON_POWER_BREAK_OUT ? held.exit : held.spot;
    }

    if (step == YOGG_SARON_POWER_BREAK_NONE && live)
    {
        crusher = YoggSaronPowerBreakClaim(bot, state);
        if (crusher)
        {
            spot = state.powerBreaks[bot->GetGUID()].spot;
            step = bot->GetExactDist2d(spot) <= ULDUAR_YOGG_SARON_POWER_BREAK_ARRIVED_RADIUS
                       ? YOGG_SARON_POWER_BREAK_HIT
                       : YOGG_SARON_POWER_BREAK_WALK;
        }
    }

    if (RaidObs::Active())
    {
        char const* names[] = {"none", "walk", "hit", "out"};
        std::string text = names[step];
        if (crusher)
            text += " " + RaidObs::DescribeAssignment(crusher->GetGUID());

        RaidObs::NoteDerived(bot, "yogg.powerbreak", text);
    }

    target = crusher;
    return step;
}

std::vector<char const*> YoggSaronPowerBreakStrikes(Player* bot)
{
    switch (bot->getClass())
    {
        case CLASS_WARRIOR:
            return {"shield slam", "revenge", "devastate"};
        case CLASS_PALADIN:
            return {"shield of righteousness"};
        case CLASS_DEATH_KNIGHT:
            return {"plague strike", "death strike", "blood strike"};
        case CLASS_DRUID:
            return {"mangle (bear)", "lacerate"};
        default:
            return {};
    }
}

namespace
{
std::vector<Creature*> YoggSaronLowGuardians(Unit* from, float gate, float radius)
{
    std::list<Creature*> guardians;
    from->GetCreatureListWithEntryInGrid(guardians, NPC_GUARDIAN_OF_YS, radius);

    std::vector<Creature*> low;
    for (Creature* guardian : guardians)
    {
        if (guardian->IsAlive() && guardian->GetHealthPct() <= gate)
            low.push_back(guardian);
    }

    return low;
}

uint32 YoggSaronNovaDamageEach(Unit* target)
{
    return target->GetResistance(SPELL_SCHOOL_SHADOW) >= ULDUAR_YOGG_SARON_NOVA_RESISTED_AT
               ? ULDUAR_YOGG_SARON_NOVA_RESISTED_DAMAGE
               : ULDUAR_YOGG_SARON_NOVA_EXPECTED_DAMAGE;
}

bool YoggSaronNovaChained(Player* bot, Unit* member, uint32 expected)
{
    if (!expected)
        return false;

    return expected >= ULDUAR_YOGG_SARON_NOVA_MAX_CHAIN * YoggSaronNovaDamageEach(member) ||
           member->HasAura(SPELL_SARAS_BLESSING) ||
           YoggSaronGuardianDiedWithin(bot, ULDUAR_YOGG_SARON_P1_NOVA_GAP_MS);
}

uint32 YoggSaronNovaDamageFrom(std::vector<Creature*> const& low, Unit* target)
{
    uint32 count = 0;
    for (Creature* guardian : low)
    {
        if (target->GetExactDist2d(guardian) <= ULDUAR_YOGG_SARON_SHADOW_NOVA_TRIGGER_RADIUS)
            ++count;
    }

    if (!count)
        return 0;

    count = std::min<uint32>(count, ULDUAR_YOGG_SARON_NOVA_MAX_CHAIN);

    uint32 const expected = count * YoggSaronNovaDamageEach(target);

    return target->HasAura(SPELL_SARAS_FERVOR) ? expected * 2 : expected;
}

bool YoggSaronNovaLeavesNothing(uint32 expected, Unit* target)
{
    return expected && target->GetHealth() <= expected;
}

uint32 YoggSaronNovaGateMs(Creature const* guardian, uint32 now)
{
    return yoggSaronStates.For(guardian->GetInstanceId()).novaGates.emplace(guardian->GetGUID(), now).first->second;
}

void StampYoggSaronNovaGates(std::vector<Creature*> const& low)
{
    uint32 const now = getMSTime();
    for (Creature const* guardian : low)
    {
        if (guardian->GetHealthPct() <= ULDUAR_YOGG_SARON_NOVA_NOW_HEALTH_PCT)
            YoggSaronNovaGateMs(guardian, now);
    }
}

uint32 YoggSaronNovaLeftMs(Creature const* guardian)
{
    uint32 const now = getMSTime();
    uint32 const since = getMSTimeDiff(YoggSaronNovaGateMs(guardian, now), now);

    return since < ULDUAR_YOGG_SARON_NOVA_NOW_DUE_MS ? ULDUAR_YOGG_SARON_NOVA_NOW_DUE_MS - since
                                                     : ULDUAR_YOGG_SARON_NOVA_OVERDUE_NEED_MS;
}

uint32 YoggSaronNovaNeedFrom(std::vector<Creature*> const& low, Unit* target)
{
    uint32 needMs = 0;
    for (Creature const* guardian : low)
    {
        if (guardian->GetHealthPct() > ULDUAR_YOGG_SARON_NOVA_NOW_HEALTH_PCT ||
            target->GetExactDist2d(guardian) > ULDUAR_YOGG_SARON_SHADOW_NOVA_TRIGGER_RADIUS)
            continue;

        uint32 const left = YoggSaronNovaLeftMs(guardian);
        if (!needMs || left < needMs)
            needMs = left;
    }

    return needMs ? needMs : ULDUAR_YOGG_SARON_NOVA_SOON_WINDOW_MS;
}

uint32 YoggSaronNovaAfterOwnButton(std::vector<Creature*> const& low, Player* member, uint32 expected)
{
    PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
    if (!memberAI)
        return expected;

    uint8 const cut = BestMagicDefensiveCut(memberAI, member, YoggSaronNovaNeedFrom(low, member));

    return expected - expected * cut / 100;
}

bool YoggSaronNovaCandidate(Player* member)
{
    return member && member->IsAlive() && !PlayerbotAI::IsRanged(member) && !PlayerbotAI::IsHeal(member);
}

Player* ElectShadowAuraPaladin(Group* group)
{
    Player* best = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        if (!memberAI || !member->IsAlive() || member->getClass() != CLASS_PALADIN ||
            !member->HasActiveSpell(SPELL_AURA_MASTERY) ||
            !memberAI->GetAiObjectContext()->GetValue<uint32>("spell id", "shadow resistance aura")->Get())
            continue;

        if (!best || member->GetGUID() < best->GetGUID())
            best = member;
    }

    return best;
}
}

std::vector<Unit*> GetYoggSaronNovaThreats(PlayerbotAI* botAI, float radius)
{
    Player* bot = botAI->GetBot();

    bool const fervor = bot->HasAura(SPELL_SARAS_FERVOR);
    bool const atRange = PlayerbotAI::IsRanged(bot) || PlayerbotAI::IsHeal(bot);
    if (!fervor && !atRange && (PlayerbotAI::IsTank(bot) || YoggSaronInPhase1(botAI)))
        return {};

    std::list<Creature*> guardians;
    bot->GetCreatureListWithEntryInGrid(guardians, NPC_GUARDIAN_OF_YS, radius);

    std::vector<Creature*> low;
    for (Creature* guardian : guardians)
    {
        if (!guardian->IsAlive())
            continue;

        float const gate = fervor ? ULDUAR_YOGG_SARON_FERVOR_NOVA_HEALTH_PCT
                                  : ULDUAR_YOGG_SARON_GUARDIAN_NOVA_HEALTH_PCT;
        if (guardian->GetHealthPct() > gate)
            continue;

        if (atRange && !fervor && guardian->GetVictim() != bot && YoggSaronGuardianOnTheStack(guardian))
            continue;

        low.push_back(guardian);
    }

    return std::vector<Unit*>(low.begin(), low.end());
}

uint32 YoggSaronNovaDamageOn(Unit* target)
{
    if (!target || !target->IsAlive())
        return 0;

    std::vector<Creature*> const low = YoggSaronLowGuardians(target, ULDUAR_YOGG_SARON_NOVA_SOON_HEALTH_PCT,
                                                             ULDUAR_YOGG_SARON_SHADOW_NOVA_TRIGGER_RADIUS);

    return YoggSaronNovaDamageFrom(low, target);
}

bool YoggSaronNovaWouldHurt(Unit* target)
{
    return YoggSaronNovaLeavesNothing(YoggSaronNovaDamageOn(target), target);
}

uint32 YoggSaronNovaNeedMs(Unit* target, bool now)
{
    if (!target || !target->IsAlive())
        return 0;

    float const gate = now ? ULDUAR_YOGG_SARON_NOVA_NOW_HEALTH_PCT
                           : ULDUAR_YOGG_SARON_NOVA_SOON_HEALTH_PCT;
    std::vector<Creature*> const low =
        YoggSaronLowGuardians(target, gate, ULDUAR_YOGG_SARON_SHADOW_NOVA_TRIGGER_RADIUS);
    if (!YoggSaronNovaLeavesNothing(YoggSaronNovaDamageFrom(low, target), target))
        return 0;

    return YoggSaronNovaNeedFrom(low, target);
}

bool YoggSaronNovaNearby(PlayerbotAI* botAI)
{
    return !YoggSaronLowGuardians(botAI->GetBot(), 100.0f,
                                  ULDUAR_YOGG_SARON_SHADOW_NOVA_TRIGGER_RADIUS).empty();
}

bool YoggSaronNovaHold(PlayerbotAI* botAI)
{
    return YoggSaronInPhase1(botAI) ||
           !YoggSaronLowGuardians(botAI->GetBot(), 100.0f, ULDUAR_YOGG_SARON_P1_ROOM_RADIUS).empty();
}

uint32 YoggSaronNovaHurtCount(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    Group* group = bot->GetGroup();
    if (!group)
        return 0;

    std::vector<Creature*> const low = YoggSaronLowGuardians(bot, ULDUAR_YOGG_SARON_NOVA_SOON_HEALTH_PCT,
                                                             ULDUAR_YOGG_SARON_P1_ROOM_RADIUS);
    if (low.empty())
        return 0;

    uint32 count = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!YoggSaronNovaCandidate(member))
            continue;

        uint32 const expected = YoggSaronNovaDamageFrom(low, member);
        if (YoggSaronNovaChained(bot, member, expected) && YoggSaronNovaLeavesNothing(expected, member))
            ++count;
    }

    return count;
}

bool IsYoggSaronShadowAuraPaladin(Player* bot)
{
    if (!bot || bot->getClass() != CLASS_PALADIN || !YoggSaronInPhase1Room(bot) ||
        YoggSaronStateFor(bot).roarExpected)
        return false;

    Group* group = bot->GetGroup();

    return group && ElectShadowAuraPaladin(group) == bot;
}

bool YoggSaronShadowAuraHeld(PlayerbotAI* botAI)
{
    return botAI && IsYoggSaronShadowAuraPaladin(botAI->GetBot());
}

bool YoggSaronShadowAuraOtherHeld(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || bot->getClass() != CLASS_PALADIN || !YoggSaronInPhase1Room(bot) ||
        YoggSaronStateFor(bot).roarExpected)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    Player* const elected = ElectShadowAuraPaladin(group);

    return elected && elected != bot;
}

bool YoggSaronBattleRezHeld(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || !YoggSaronInPhase1Room(bot))
        return false;

    return !YoggSaronHandoverState(botAI).clearing && YoggSaronInPhase1(botAI);
}

char const* YoggSaronNovaRaidCover(PlayerbotAI* botAI)
{
    Player* bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || bot->getClass() != CLASS_PALADIN)
        return nullptr;

    if (IsYoggSaronShadowAuraPaladin(bot) && botAI->HasAura("shadow resistance aura", bot, false, true) &&
        botAI->CanCastSpell("aura mastery", bot))
        return "aura mastery";

    return botAI->CanCastSpell("divine sacrifice", bot) ? "divine sacrifice" : nullptr;
}

Player* YoggSaronNovaVictim(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    std::vector<Creature*> const low = YoggSaronLowGuardians(bot, ULDUAR_YOGG_SARON_NOVA_NOW_HEALTH_PCT,
                                                             ULDUAR_YOGG_SARON_P1_ROOM_RADIUS);
    if (low.empty())
        return nullptr;

    StampYoggSaronNovaGates(low);

    Player* victim = nullptr;
    int64 worst = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!YoggSaronNovaCandidate(member) || member == bot)
            continue;

        if (bot->GetExactDist2d(member) > sPlayerbotAIConfig.SpellDistance)
            continue;

        uint32 const expected = YoggSaronNovaDamageFrom(low, member);
        if (!YoggSaronNovaChained(bot, member, expected) || !YoggSaronNovaLeavesNothing(expected, member))
            continue;

        uint32 const after = YoggSaronNovaAfterOwnButton(low, member, expected);
        if (member->GetHealth() > after)
            continue;

        int64 const margin = int64(member->GetHealth()) - int64(after);
        if (!victim || margin < worst)
        {
            victim = member;
            worst = margin;
        }
    }

    return victim;
}

namespace
{

ObjectGuid NovaGuardian(Player* bot)
{
    std::vector<Creature*> const low = YoggSaronLowGuardians(bot, ULDUAR_YOGG_SARON_NOVA_SOON_HEALTH_PCT,
                                                             ULDUAR_YOGG_SARON_SHADOW_NOVA_TRIGGER_RADIUS);
    Creature* next = nullptr;
    for (Creature* guardian : low)
    {
        if (!next || guardian->GetHealth() < next->GetHealth())
            next = guardian;
    }

    return next ? next->GetGUID() : ObjectGuid::Empty;
}

}

bool ClaimYoggSaronNovaExternal(PlayerbotAI* botAI)
{
    ObjectGuid const guardian = NovaGuardian(botAI->GetBot());
    if (!guardian)
        return false;

    YoggSaronEncounterState& state = YoggSaronStateFor(botAI->GetBot());

    uint32 const now = getMSTime();
    uint32& claimed = state.novaClaims[guardian];
    if (claimed && getMSTimeDiff(claimed, now) < ULDUAR_YOGG_SARON_NOVA_WINDOW_MS)
        return false;

    claimed = now;

    return true;
}

bool ClaimYoggSaronNovaWindow(Player* bot)
{
    ObjectGuid const guardian = NovaGuardian(bot);
    if (!guardian)
        return false;

    YoggSaronEncounterState& state = YoggSaronStateFor(bot);

    auto const held = state.novaCovers.find(guardian);
    if (held != state.novaCovers.end() &&
        getMSTimeDiff(held->second.second, getMSTime()) < ULDUAR_YOGG_SARON_NOVA_WINDOW_MS)
        return held->second.first == bot->GetGUID();

    state.novaCovers[guardian] = {bot->GetGUID(), getMSTime()};

    return true;
}

void AppendYoggSaronTargetExclusions(PlayerbotAI* botAI, GuidSet& exclusions, GuidVector const& candidates)
{
    if (std::none_of(candidates.begin(), candidates.end(),
                     [](ObjectGuid const& guid) { return guid.GetEntry() == NPC_GUARDIAN_OF_YS; }))
        return;

    std::vector<Creature*> const low =
        YoggSaronLowGuardians(botAI->GetBot(), ULDUAR_YOGG_SARON_P1_NOVA_GAP_HEALTH_PCT,
                              ULDUAR_YOGG_SARON_P1_ROOM_RADIUS);

    for (Creature* guardian : low)
    {
        for (Creature* other : low)
        {
            if (other == guardian ||
                other->GetExactDist2d(guardian) > ULDUAR_YOGG_SARON_P1_NOVA_CHAIN_RADIUS)
                continue;

            if (guardian->GetHealth() > other->GetHealth() ||
                (guardian->GetHealth() == other->GetHealth() && other->GetGUID() < guardian->GetGUID()))
            {
                exclusions.insert(guardian->GetGUID());
                break;
            }
        }
    }
}

namespace
{
float YoggSaronDistanceFromMiddle(Unit* unit)
{
    return unit->GetExactDist2d(ULDUAR_YOGG_SARON_MIDDLE.GetPositionX(),
                                ULDUAR_YOGG_SARON_MIDDLE.GetPositionY());
}
}

bool YoggSaronGuardianCountsForSara(Unit* guardian)
{
    return guardian && YoggSaronDistanceFromMiddle(guardian) <= ULDUAR_YOGG_SARON_P1_SARA_NOVA_RADIUS;
}

bool YoggSaronGuardianOnTheStack(Unit* guardian)
{
    return guardian && YoggSaronDistanceFromMiddle(guardian) <= ULDUAR_YOGG_SARON_P1_LEASH;
}

bool YoggSaronPhase1TankPullSpot(PlayerbotAI* botAI, Position& spot)
{
    Player* bot = botAI->GetBot();
    if (!PlayerbotAI::IsTank(bot) || !YoggSaronInPhase1(botAI) || !YoggSaronInPhase1Room(bot))
        return false;

    Unit* pulled = nullptr;
    for (Unit* attacker : bot->getAttackers())
    {
        if (attacker->GetEntry() != NPC_GUARDIAN_OF_YS || !attacker->IsAlive() || attacker->GetVictim() != bot ||
            YoggSaronGuardianOnTheStack(attacker))
            continue;

        if (!pulled || attacker->GetHealth() < pulled->GetHealth())
            pulled = attacker;
    }

    if (!pulled || YoggSaronHandoverState(botAI).active)
        return false;

    Position const& middle = ULDUAR_YOGG_SARON_MIDDLE;
    float const across = pulled->GetAngle(middle.GetPositionX(), middle.GetPositionY());
    spot.Relocate(middle.GetPositionX() + ULDUAR_YOGG_SARON_P1_TANK_PULL_OFFSET * std::cos(across),
                  middle.GetPositionY() + ULDUAR_YOGG_SARON_P1_TANK_PULL_OFFSET * std::sin(across),
                  middle.GetPositionZ());
    return true;
}

bool YoggSaronPhase1GuardianPreferred(Unit* candidate, Unit* incumbent)
{
    if (!candidate)
        return false;

    if (!incumbent)
        return true;

    bool const candidateIn = YoggSaronGuardianOnTheStack(candidate);
    if (candidateIn != YoggSaronGuardianOnTheStack(incumbent))
        return candidateIn;

    return candidate->GetHealth() < incumbent->GetHealth();
}

Unit* YoggSaronPhase1TauntTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    std::list<Creature*> guardians;
    bot->GetCreatureListWithEntryInGrid(guardians, NPC_GUARDIAN_OF_YS, sPlayerbotAIConfig.SightDistance);

    Unit* focus = nullptr;
    for (Creature* guardian : guardians)
    {
        if (!guardian->IsAlive())
            continue;

        Player* victim = botAI->GetPlayer(guardian->GetTarget());
        if (victim && PlayerbotAI::IsTank(victim))
            continue;

        if (YoggSaronGuardianOnTheStack(guardian))
            continue;

        if (bot->GetDistance(guardian) > ULDUAR_YOGG_SARON_P1_TAUNT_RANGE)
            continue;

        if (YoggSaronPhase1GuardianPreferred(guardian, focus))
            focus = guardian;
    }

    return focus;
}

Unit* YoggSaronPhase1StackTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    std::list<Creature*> guardians;
    bot->GetCreatureListWithEntryInGrid(guardians, NPC_GUARDIAN_OF_YS, ULDUAR_YOGG_SARON_P1_TAUNT_RANGE);

    Unit* pick = nullptr;
    bool pickKillable = false;
    for (Creature* guardian : guardians)
    {
        if (!guardian->IsAlive() || !bot->IsWithinMeleeRange(guardian))
            continue;

        bool const killable = YoggSaronPhase1GuardianKillable(botAI, guardian);
        bool better = !pick || (killable && !pickKillable);
        if (pick && killable == pickKillable)
            better = killable ? guardian->GetHealth() < pick->GetHealth() : guardian->GetHealth() > pick->GetHealth();

        if (better)
        {
            pick = guardian;
            pickKillable = killable;
        }
    }

    return pick;
}

bool YoggSaronBotTankAlive(PlayerbotAI* botAI)
{
    Group* group = botAI->GetBot()->GetGroup();
    if (!group)
        return PlayerbotAI::IsTank(botAI->GetBot());

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !PlayerbotAI::IsTank(member))
            continue;

        if (member->GetSession() && member->GetSession()->IsHeadless())
            return true;
    }

    return false;
}

bool YoggSaronInPhase1Room(Player* bot)
{
    return bot->GetDistance2d(ULDUAR_YOGG_SARON_MIDDLE.GetPositionX(), ULDUAR_YOGG_SARON_MIDDLE.GetPositionY()) <=
           ULDUAR_YOGG_SARON_P1_ROOM_RADIUS;
}

bool YoggSaronInPhase1Approach(Player* bot)
{
    return bot->GetDistance2d(ULDUAR_YOGG_SARON_MIDDLE.GetPositionX(), ULDUAR_YOGG_SARON_MIDDLE.GetPositionY()) <=
           ULDUAR_YOGG_SARON_P1_DOOR_RADIUS;
}

void YoggSaronNoteGuardianDeath(Unit* guardian)
{
    if (!guardian)
        return;

    yoggSaronStates.For(guardian->GetInstanceId()).lastGuardianDeathMs = getMSTime();
}

bool YoggSaronGuardianDiedWithin(Player* bot, uint32 ms)
{
    YoggSaronEncounterState& state = YoggSaronStateFor(bot);
    return state.lastGuardianDeathMs && getMSTimeDiff(state.lastGuardianDeathMs, getMSTime()) < ms;
}

namespace
{
bool YoggSaronGuardianWaitsOutNovaGap(Player* bot, Unit* guardian)
{
    return guardian->GetHealthPct() < ULDUAR_YOGG_SARON_P1_NOVA_GAP_HEALTH_PCT &&
           YoggSaronGuardianDiedWithin(bot, ULDUAR_YOGG_SARON_P1_NOVA_GAP_MS);
}

bool YoggSaronGuardianWaitsOnLowerNeighbour(Unit* guardian)
{
    if (guardian->GetHealthPct() >= ULDUAR_YOGG_SARON_P1_NOVA_GAP_HEALTH_PCT)
        return false;

    std::list<Creature*> guardians;
    guardian->GetCreatureListWithEntryInGrid(guardians, NPC_GUARDIAN_OF_YS, ULDUAR_YOGG_SARON_P1_NOVA_CHAIN_RADIUS);
    for (Creature* other : guardians)
    {
        if (other != guardian && other->IsAlive() && other->GetHealth() < guardian->GetHealth() &&
            other->GetExactDist2d(guardian) <= ULDUAR_YOGG_SARON_P1_NOVA_CHAIN_RADIUS)
            return true;
    }

    return false;
}
}

bool YoggSaronPhase1GuardianKillable(PlayerbotAI* botAI, Unit* guardian)
{
    if (!guardian || !guardian->IsAlive())
        return false;

    if (YoggSaronGuardianWaitsOutNovaGap(botAI->GetBot(), guardian) || YoggSaronGuardianWaitsOnLowerNeighbour(guardian))
        return false;

    return YoggSaronGuardianOnTheStack(guardian) ||
           guardian->GetHealthPct() > ULDUAR_YOGG_SARON_P1_PARK_HEALTH_PCT ||
           YoggSaronHandoverState(botAI).active || !YoggSaronBotTankAlive(botAI);
}

Unit* YoggSaronPhase1Focus(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    YoggSaronEncounterState& state = YoggSaronStateFor(bot);
    ObjectGuid const heldGuid = state.phase1Focus;

    Creature* held = heldGuid ? ObjectAccessor::GetCreature(*bot, heldGuid) : nullptr;
    bool const heldKillable = YoggSaronPhase1GuardianKillable(botAI, held);

    if (!YoggSaronInPhase1Room(bot))
        return heldKillable ? held : nullptr;

    std::list<Creature*> guardians;
    bot->GetCreatureListWithEntryInGrid(guardians, NPC_GUARDIAN_OF_YS, sPlayerbotAIConfig.SightDistance);

    bool const handover = YoggSaronHandoverState(botAI).active;

    Unit* best = nullptr;
    bool killableOnStack = false;
    for (Creature* guardian : guardians)
    {
        if (!YoggSaronPhase1GuardianKillable(botAI, guardian))
            continue;

        killableOnStack = killableOnStack || YoggSaronGuardianOnTheStack(guardian);
        if (handover ? !best || guardian->GetHealth() < best->GetHealth()
                     : YoggSaronPhase1GuardianPreferred(guardian, best))
            best = guardian;
    }

    bool const lower = heldKillable && best && best != held && YoggSaronLowGuardianTakesOver(best, held);
    bool const abandon =
        heldKillable && (lower || (!handover && !YoggSaronGuardianCountsForSara(held) && killableOnStack));
    if (heldKillable && !abandon)
        return held;

    ObjectGuid const next = best ? best->GetGUID() : ObjectGuid::Empty;
    bool const changed = state.phase1Focus != next;
    state.phase1Focus = next;

    if (changed && RaidObs::Active())
    {
        char const* reason = "picked";
        if (!best)
            reason = "none";
        else if (lower)
            reason = "lower";
        else if (abandon)
            reason = "abandoned";
        else if (held && held->IsAlive())
        {
            if (YoggSaronGuardianWaitsOutNovaGap(bot, held))
                reason = "spaced";
            else if (YoggSaronGuardianWaitsOnLowerNeighbour(held))
                reason = "chain";
            else
                reason = "parked";
        }

        RaidObs::NoteDerived(bot, "yogg.p1focus", reason);
    }

    return best;
}

bool YoggSaronPhase1AoeHold(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    ObjectGuid const focusGuid = YoggSaronStateFor(bot).phase1Focus;

    Creature* focus = focusGuid ? ObjectAccessor::GetCreature(*bot, focusGuid) : nullptr;
    if (focus && !focus->IsAlive())
        focus = nullptr;

    std::list<Creature*> guardians;
    bot->GetCreatureListWithEntryInGrid(guardians, NPC_GUARDIAN_OF_YS, sPlayerbotAIConfig.SightDistance);

    for (Creature* guardian : guardians)
    {
        if (guardian == focus || !guardian->IsAlive() ||
            guardian->GetHealthPct() >= ULDUAR_YOGG_SARON_P1_NOVA_GAP_HEALTH_PCT)
            continue;

        if (guardian->GetDistance2d(ULDUAR_YOGG_SARON_MIDDLE.GetPositionX(),
                                    ULDUAR_YOGG_SARON_MIDDLE.GetPositionY()) <= ULDUAR_YOGG_SARON_P1_AOE_HOLD_RADIUS ||
            (focus && guardian->GetExactDist2d(focus) <= ULDUAR_YOGG_SARON_P1_AOE_HOLD_RADIUS))
            return true;
    }

    return false;
}

bool YoggSaronLeftoverGuardianKillable(PlayerbotAI* botAI, Unit* guardian)
{
    return guardian && guardian->IsAlive() && !YoggSaronGuardianWaitsOutNovaGap(botAI->GetBot(), guardian) &&
           !YoggSaronGuardianWaitsOnLowerNeighbour(guardian);
}

bool YoggSaronLowGuardianTakesOver(Unit* candidate, Unit* current)
{
    return candidate->GetHealthPct() < ULDUAR_YOGG_SARON_P1_NOVA_GAP_HEALTH_PCT &&
           candidate->GetHealth() < current->GetHealth();
}

bool YoggSaronLeftoverAoeHold(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    if (target && !target->IsAlive())
        target = nullptr;

    std::list<Creature*> guardians;
    bot->GetCreatureListWithEntryInGrid(guardians, NPC_GUARDIAN_OF_YS, sPlayerbotAIConfig.SightDistance);

    for (Creature* guardian : guardians)
    {
        if (guardian == target || !guardian->IsAlive() ||
            guardian->GetHealthPct() >= ULDUAR_YOGG_SARON_P1_NOVA_GAP_HEALTH_PCT)
            continue;

        if (guardian->GetExactDist2d(bot) <= ULDUAR_YOGG_SARON_P1_AOE_HOLD_RADIUS ||
            (target && guardian->GetExactDist2d(target) <= ULDUAR_YOGG_SARON_P1_AOE_HOLD_RADIUS))
            return true;
    }

    return false;
}

Position YoggSaronCloudLead(Creature* cloud, uint32 leadMs)
{
    float const travel = cloud->GetSpeed(MOVE_RUN) * leadMs / IN_MILLISECONDS;
    float const heading = cloud->GetOrientation();

    return Position(cloud->GetPositionX() + std::cos(heading) * travel,
                    cloud->GetPositionY() + std::sin(heading) * travel, cloud->GetPositionZ());
}

bool YoggSaronRouteClearOfClouds(Player* bot, std::vector<Position> const& clouds, float x, float y)
{
    float const originX = bot->GetPositionX();
    float const originY = bot->GetPositionY();
    float const legX = x - originX;
    float const legY = y - originY;
    float const legSquared = legX * legX + legY * legY;

    for (Position const& cloud : clouds)
    {
        float along = 0.0f;
        if (legSquared > 0.0f)
        {
            along = ((cloud.GetPositionX() - originX) * legX + (cloud.GetPositionY() - originY) * legY) / legSquared;
            along = std::max(0.0f, std::min(1.0f, along));
        }

        if (cloud.GetExactDist2d(originX + legX * along, originY + legY * along) <
            ULDUAR_YOGG_SARON_CLOUD_AVOID_RADIUS)
            return false;
    }

    return true;
}

bool YoggSaronWalkCrossesCloud(Player* bot, Position const& to, Position const* exempt, float exemptRadius)
{
    float const leg = bot->GetExactDist2d(to.GetPositionX(), to.GetPositionY());

    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, NPC_OMINOUS_CLOUD, leg + ULDUAR_YOGG_SARON_CLOUD_AVOID_RADIUS);
    if (found.empty())
        return false;

    float const speed = bot->GetSpeed(MOVE_RUN);
    uint32 const arrivalMs = speed > 0.0f ? static_cast<uint32>(leg / speed * IN_MILLISECONDS) : 0;

    float const originX = bot->GetPositionX();
    float const originY = bot->GetPositionY();
    float const legX = to.GetPositionX() - originX;
    float const legY = to.GetPositionY() - originY;

    for (Creature* cloud : found)
    {
        if (!cloud->IsAlive())
            continue;

        if (bot->GetExactDist2d(cloud) < ULDUAR_YOGG_SARON_CLOUD_AVOID_RADIUS)
            return false;

        if (exempt && cloud->GetExactDist2d(exempt) <= exemptRadius)
            continue;

        for (uint32 step = 0; step <= ULDUAR_YOGG_SARON_WALK_CLOUD_STEPS; ++step)
        {
            float const share = static_cast<float>(step) / ULDUAR_YOGG_SARON_WALK_CLOUD_STEPS;
            Position const at = YoggSaronCloudLead(cloud, static_cast<uint32>(arrivalMs * share));
            if (at.GetExactDist2d(originX + legX * share, originY + legY * share) <
                ULDUAR_YOGG_SARON_CLOUD_AVOID_RADIUS)
                return true;
        }
    }

    return false;
}

namespace
{

struct IllusionMobInRangeCheck
{
    Acore::AnyUnitInObjectRangeCheck inRange;
    Position centre;
    float radius;

    bool operator()(Unit* unit)
    {
        if (!unit->IsAlive())
            return false;

        return std::find(ULDUAR_YOGG_SARON_ILLUSION_MOBS.begin(), ULDUAR_YOGG_SARON_ILLUSION_MOBS.end(),
                         unit->GetEntry()) != ULDUAR_YOGG_SARON_ILLUSION_MOBS.end() &&
               centre.GetExactDist2d(unit) <= radius && inRange(unit);
    }
};

const std::vector<uint32> yoggSaronPetTargets = {NPC_GUARDIAN_OF_YS, NPC_CONSTRICTOR_TENTACLE,
                                                 NPC_CORRUPTOR_TENTACLE, NPC_MARKED_IMMORTAL_GUARDIAN,
                                                 NPC_IMMORTAL_GUARDIAN};

struct PetTargetInRangeCheck
{
    Acore::AnyUnitInObjectRangeCheck inRange;

    bool operator()(Unit* unit)
    {
        if (!unit->IsAlive())
            return false;

        return std::find(yoggSaronPetTargets.begin(), yoggSaronPetTargets.end(), unit->GetEntry()) !=
                   yoggSaronPetTargets.end() &&
               inRange(unit);
    }
};

bool YoggSaronCrusherCanSwing(PlayerbotAI* botAI, Creature* crusher)
{
    if (YoggSaronCrusherStunHolds(botAI, crusher))
        return false;

    Unit* victim = crusher->GetVictim();

    return victim && victim->IsAlive() && crusher->IsWithinMeleeRange(victim);
}

}

Unit* YoggSaronLiveIllusionMob(PlayerbotAI* botAI, float radius, Position const* centre)
{
    Player* bot = botAI->GetBot();
    Position const around = centre ? *centre : bot->GetPosition();

    float const sweep = bot->GetExactDist2d(around.GetPositionX(), around.GetPositionY()) + radius;

    std::vector<Unit*> found;
    IllusionMobInRangeCheck check{Acore::AnyUnitInObjectRangeCheck(bot, sweep), around, radius};
    Acore::UnitListSearcher<IllusionMobInRangeCheck> searcher(bot, found, check);
    Cell::VisitObjects(bot, searcher, sweep);

    return found.empty() ? nullptr : found.front();
}

std::vector<Unit*> GetYoggSaronRoomIllusionMobs(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    Position middle;
    if (!YoggSaronRoomMiddle(bot, middle))
        return {};

    float const radius = ULDUAR_YOGG_SARON_STORMWIND_KEEPER_RADIUS;
    float const sweep = bot->GetExactDist2d(middle.GetPositionX(), middle.GetPositionY()) + radius;

    std::vector<Unit*> found;
    IllusionMobInRangeCheck check{Acore::AnyUnitInObjectRangeCheck(bot, sweep), middle, radius};
    Acore::UnitListSearcher<IllusionMobInRangeCheck> searcher(bot, found, check);
    Cell::VisitObjects(bot, searcher, sweep);

    std::sort(found.begin(), found.end(),
              [bot](Unit* left, Unit* right) { return bot->GetExactDist(left) < bot->GetExactDist(right); });

    return found;
}

namespace
{
bool YoggSaronIllusionHealerStationNow(PlayerbotAI* botAI, Position& station)
{
    Player* bot = botAI->GetBot();

    Position middle;
    if (!YoggSaronRoomMiddle(bot, middle))
        return false;

    std::vector<Unit*> const mobs = GetYoggSaronRoomIllusionMobs(botAI);
    if (mobs.empty())
    {
        station = middle;
        return true;
    }

    float x = 0.0f;
    float y = 0.0f;
    for (Unit* mob : mobs)
    {
        x += mob->GetPositionX();
        y += mob->GetPositionY();
    }

    x /= static_cast<float>(mobs.size());
    y /= static_cast<float>(mobs.size());

    station = Position(x, y, middle.GetPositionZ());

    return true;
}
}

bool YoggSaronIllusionHealerStation(PlayerbotAI* botAI, Position& station)
{
    YoggSaronPassReads* reads = YoggSaronPassReadsFor(botAI);
    if (reads && reads->healerStationRead)
    {
        station = reads->healerStation;
        return reads->healerStationFound;
    }

    bool const found = YoggSaronIllusionHealerStationNow(botAI, station);

    if (reads)
    {
        reads->healerStationRead = true;
        reads->healerStationFound = found;
        reads->healerStation = station;
    }

    return found;
}

Unit* YoggSaronPetFallbackTarget(Creature* pet)
{
    if (!pet)
        return nullptr;

    std::vector<Unit*> found;
    PetTargetInRangeCheck check{Acore::AnyUnitInObjectRangeCheck(pet, ULDUAR_YOGG_SARON_PET_TARGET_RADIUS)};
    Acore::UnitListSearcher<PetTargetInRangeCheck> searcher(pet, found, check);
    Cell::VisitObjects(pet, searcher, ULDUAR_YOGG_SARON_PET_TARGET_RADIUS);

    Unit* nearest = nullptr;
    float best = 0.0f;
    for (Unit* candidate : found)
    {
        float const distance = pet->GetExactDist2d(candidate);
        if (!nearest || distance < best)
        {
            nearest = candidate;
            best = distance;
        }
    }

    return nearest;
}

bool YoggSaronBeaconHealReaches(Unit* unit)
{
    float const reach =
        ULDUAR_YOGG_SARON_BEACON_HEAL_RADIUS + ULDUAR_YOGG_SARON_BEACON_HEAL_SLACK + unit->GetObjectSize();

    for (YoggSaronBeaconLane const& lane : YOGG_SARON_BEACON_LANES)
        for (Position const* spot : {lane.drag, lane.hunter})
            if (unit->GetExactDist2d(spot->GetPositionX(), spot->GetPositionY()) <= reach)
                return true;

    return false;
}

Unit* YoggSaronPhase3PetLeftover(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (YoggSaronOnBrainLevel(bot) || !YoggSaronInPhase3(botAI))
        return nullptr;

    bool const thorim = YoggThorimKeeperActive(botAI);

    Creature* yogg = YoggSaronNearestCreature(botAI, NPC_YOGG_SARON);
    if (!yogg)
        return nullptr;

    std::list<Creature*> leftovers;
    yogg->GetCreatureListWithEntryInGrid(leftovers, {NPC_CORRUPTOR_TENTACLE, NPC_CONSTRICTOR_TENTACLE},
                                         ULDUAR_YOGG_SARON_PET_LEFTOVER_SEARCH_RADIUS);
    if (leftovers.empty())
        return nullptr;

    std::list<Creature*> crushers;
    yogg->GetCreatureListWithEntryInGrid(crushers, NPC_CRUSHER_TENTACLE, ULDUAR_YOGG_SARON_PET_LEFTOVER_SEARCH_RADIUS);

    auto const rank = [thorim](Creature* tentacle)
    {
        return std::make_tuple(!YoggSaronConstrictorHolding(tentacle), thorim || !YoggSaronBeaconHealReaches(tentacle),
                               tentacle->GetHealth(), tentacle->GetGUID());
    };

    Creature* best = nullptr;
    for (Creature* leftover : leftovers)
    {
        if (!leftover->IsAlive())
            continue;

        bool const nearCrusher = std::any_of(crushers.begin(), crushers.end(), [botAI, leftover](Creature* crusher)
            {
                return crusher->IsAlive() && !YoggSaronCrusherStunHolds(botAI, crusher) &&
                       crusher->GetExactDist2d(leftover) <= ULDUAR_YOGG_SARON_CRUSH_RANGE;
            });
        if (nearCrusher)
            continue;

        if (!best || rank(leftover) < rank(best))
            best = leftover;
    }

    return best;
}

bool YoggSaronInfluenceTentaclesCleared(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    bool const brainRoom = YoggSaronRoomOf(bot) == YOGG_SARON_ROOM_BRAIN;
    float const radius = brainRoom ? ULDUAR_YOGG_SARON_BRAIN_ROOM_RADIUS + ULDUAR_YOGG_SARON_STORMWIND_KEEPER_RADIUS
                                   : ULDUAR_YOGG_SARON_STORMWIND_KEEPER_RADIUS;

    Position middle;
    Position const around = !brainRoom && YoggSaronRoomMiddle(bot, middle) ? middle : bot->GetPosition();

    YoggSaronPassReads* reads = YoggSaronPassReadsFor(botAI);
    if (reads)
        for (YoggSaronPassReads::ClearRead const& answer : reads->tentaclesCleared)
            if (answer.radius == radius && answer.x == around.GetPositionX() &&
                answer.y == around.GetPositionY())
                return answer.cleared;

    bool const cleared = !YoggSaronLiveIllusionMob(botAI, radius, &around);
    if (reads)
        reads->tentaclesCleared.push_back(
            {around.GetPositionX(), around.GetPositionY(), radius, cleared});

    return cleared;
}

bool YoggSaronPlatformIdle(PlayerbotAI* botAI)
{
    for (uint32 entry : {NPC_GUARDIAN_OF_YS, NPC_CRUSHER_TENTACLE, NPC_CONSTRICTOR_TENTACLE, NPC_CORRUPTOR_TENTACLE})
        if (YoggSaronNearestCreature(botAI, entry))
            return false;

    return true;
}

std::vector<Position> GetYoggSaronCrushWedges(PlayerbotAI* botAI, float searchRadius)
{
    Player* bot = botAI->GetBot();

    std::list<Creature*> crushers;
    bot->GetCreatureListWithEntryInGrid(crushers, NPC_CRUSHER_TENTACLE, searchRadius);

    std::vector<Position> wedges;
    for (Creature* crusher : crushers)
    {
        if (!crusher->IsAlive() || crusher->GetVictim() == bot)
            continue;

        if (!YoggSaronCrusherCanSwing(botAI, crusher))
            continue;

        wedges.push_back(crusher->GetPosition());
    }

    return wedges;
}

bool InYoggSaronCrushWedge(std::vector<Position> const& wedges, float x, float y, float arcDegrees)
{
    float const arc = arcDegrees * static_cast<float>(M_PI) / 180.0f;

    for (Position const& wedge : wedges)
    {
        if (wedge.GetExactDist2d(x, y) > ULDUAR_YOGG_SARON_CRUSH_RANGE)
            continue;

        float const bearing = std::atan2(y - wedge.GetPositionY(), x - wedge.GetPositionX());
        float offset = Position::NormalizeOrientation(bearing - wedge.GetOrientation());
        if (offset > static_cast<float>(M_PI))
            offset = 2.0f * static_cast<float>(M_PI) - offset;

        if (offset <= arc)
            return true;
    }

    return false;
}

std::vector<Position> GetYoggSaronCrusherReaches(PlayerbotAI* botAI, float searchRadius)
{
    Player* bot = botAI->GetBot();

    std::list<Creature*> crushers;
    bot->GetCreatureListWithEntryInGrid(crushers, NPC_CRUSHER_TENTACLE, searchRadius);

    bool const melee = PlayerbotAI::IsMelee(bot);

    std::vector<Position> reaches;
    for (Creature* crusher : crushers)
    {
        if (!crusher->IsAlive())
            continue;

        if (melee &&
            (!crusher->HasAura(SPELL_SHATTERED_ILLUSION) || YoggSaronCrusherStunHolds(botAI, crusher)))
            continue;

        reaches.push_back(crusher->GetPosition());
    }

    return reaches;
}

namespace
{
bool YoggSaronBrainHoldInForce(Player* bot)
{
    uint32 const now = getMSTime();
    for (auto const& [guid, hold] : YoggSaronStateFor(bot).brainHold)
        if ((hold.mode == YoggSaronEncounterState::BRAIN_HOLD_FULL ||
             hold.mode == YoggSaronEncounterState::BRAIN_HOLD_DOTS) &&
            getMSTimeDiff(hold.seenMs, now) < ULDUAR_YOGG_SARON_BRAIN_HOLD_SEEN_MS)
            return true;

    return false;
}
}

bool YoggSaronCrusherStunHolds(PlayerbotAI* botAI, Unit* crusher)
{
    if (!crusher || !crusher->IsAlive() || !crusher->HasAura(SPELL_SHATTERED_ILLUSION))
        return false;

    Creature* brain = YoggSaronNearestCreature(botAI, NPC_BRAIN);
    if (!brain || !brain->IsAlive())
        return false;

    bool const held = brain->GetHealthPct() > ULDUAR_YOGG_SARON_BRAIN_HOLD_GUARD_PCT &&
                      YoggSaronBrainHoldInForce(botAI->GetBot());
    if (!held && brain->GetHealthPct() <= ULDUAR_YOGG_SARON_STUN_BRAIN_FLOOR_PCT)
        return false;

    uint32 const lead = held ? ULDUAR_YOGG_SARON_BRAIN_RESUME_MS + ULDUAR_YOGG_SARON_STUN_EXIT_LEAD_MS
                             : ULDUAR_YOGG_SARON_STUN_EXIT_LEAD_MS;

    Spell* induceMadness = brain->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    return induceMadness && induceMadness->m_spellInfo->Id == SPELL_INDUCE_MADNESS &&
           induceMadness->GetCastTimeRemaining() > static_cast<int32>(lead);
}

bool YoggSaronInSwingingCrusherReach(PlayerbotAI* botAI, float slack)
{
    Player* bot = botAI->GetBot();

    std::list<Creature*> crushers;
    bot->GetCreatureListWithEntryInGrid(crushers, NPC_CRUSHER_TENTACLE,
                                        ULDUAR_YOGG_SARON_CRUSHER_REACH_CLEAR_RADIUS + slack);

    for (Creature* crusher : crushers)
        if (crusher->IsAlive() && bot->GetExactDist2d(crusher) <= crusher->GetMeleeRange(bot) + slack &&
            !YoggSaronCrusherStunHolds(botAI, crusher))
            return true;

    return false;
}

bool YoggSaronInCrusherReach(PlayerbotAI* botAI, float radius)
{
    Player* bot = botAI->GetBot();

    for (Position const& reach : GetYoggSaronCrusherReaches(botAI, radius))
        if (reach.GetExactDist2d(bot) <= radius)
            return true;

    return false;
}

namespace
{
struct YoggSaronBrainLinkPair
{
    ObjectGuid owner;
    ObjectGuid partner;
    uint32 seenMs = 0;
};

RaidInstanceState<std::vector<YoggSaronBrainLinkPair>> yoggSaronBrainLinkPairs;
}

void YoggSaronNoteBrainLinkPair(Unit* owner, Unit* partner)
{
    if (!owner || !partner || owner == partner)
        return;

    uint32 const now = getMSTime();
    std::vector<YoggSaronBrainLinkPair>& pairs = yoggSaronBrainLinkPairs.For(owner->GetInstanceId());

    for (YoggSaronBrainLinkPair& pair : pairs)
    {
        if (pair.owner == owner->GetGUID())
        {
            pair.partner = partner->GetGUID();
            pair.seenMs = now;
            return;
        }
    }

    pairs.push_back(YoggSaronBrainLinkPair{owner->GetGUID(), partner->GetGUID(), now});
}

Player* YoggSaronBrainLinkTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    if (bot->GetPositionZ() < ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
        return nullptr;

    uint32 const now = getMSTime();
    std::vector<YoggSaronBrainLinkPair>& pairs = yoggSaronBrainLinkPairs.For(bot->GetInstanceId());

    pairs.erase(std::remove_if(pairs.begin(), pairs.end(),
                               [now](YoggSaronBrainLinkPair const& pair)
                               { return getMSTimeDiff(pair.seenMs, now) > ULDUAR_YOGG_SARON_BRAIN_LINK_PAIR_TTL_MS; }),
                pairs.end());

    ObjectGuid other;
    for (YoggSaronBrainLinkPair const& pair : pairs)
    {
        if (pair.owner == bot->GetGUID())
            other = pair.partner;
        else if (pair.partner == bot->GetGUID())
            other = pair.owner;
    }

    Player* partner = other ? ObjectAccessor::FindPlayer(other) : nullptr;
    if (partner && (!partner->IsAlive() || partner->GetMapId() != bot->GetMapId()))
        partner = nullptr;

    bool const tooFar = partner && bot->GetExactDist2d(partner) > ULDUAR_YOGG_SARON_BRAIN_LINK_CLOSE;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.brainlink", !partner ? "none" : (tooFar ? "closing" : "clear"));

    return tooFar ? partner : nullptr;
}

Player* YoggSaronSqueezeVictim(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();

    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    uint32 const squeeze = sSpellMgr->GetSpellIdForDifficulty(SPELL_SQUEEZE, bot);

    Player* victim = nullptr;
    float best = 0.0f;
    float bestDistance = 0.0f;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || !member->HasAura(squeeze))
            continue;

        float const distance = bot->GetExactDist2d(member);
        if (distance > ULDUAR_YOGG_SARON_HAND_OF_PROTECTION_RANGE)
            continue;

        float const health = member->GetHealthPct();
        if (!victim || health < best || (health == best && distance < bestDistance))
        {
            victim = member;
            best = health;
            bestDistance = distance;
        }
    }

    return victim;
}

bool ClaimYoggSaronSqueezeRescue(PlayerbotAI* botAI, Player* victim)
{
    if (!victim)
        return false;

    YoggSaronEncounterState& state = YoggSaronStateFor(botAI->GetBot());

    uint32 const now = getMSTime();
    uint32& claimed = state.squeezeClaims[victim->GetGUID()];
    if (claimed && getMSTimeDiff(claimed, now) < ULDUAR_YOGG_SARON_SQUEEZE_CLAIM_MS)
        return false;

    claimed = now;

    return true;
}

bool YoggSaronFearWindowActive(PlayerbotAI* botAI)
{
    uint32 const phase = YoggSaronPhase(botAI);

    return phase == 2 || phase == 3;
}

namespace
{
std::string DescribeCloudReach(Player* bot)
{
    float const fromMiddle =
        bot->GetDistance2d(ULDUAR_YOGG_SARON_MIDDLE.GetPositionX(), ULDUAR_YOGG_SARON_MIDDLE.GetPositionY());

    constexpr size_t orbits = sizeof(ULDUAR_YOGG_SARON_CLOUD_ORBITS) / sizeof(ULDUAR_YOGG_SARON_CLOUD_ORBITS[0]);

    std::string reach;
    for (size_t orbit = 0; orbit < orbits; ++orbit)
    {
        if (std::abs(fromMiddle - ULDUAR_YOGG_SARON_CLOUD_ORBITS[orbit]) > ULDUAR_YOGG_SARON_CLOUD_SUMMON_REACH)
            continue;

        if (!reach.empty())
            reach += "+";

        reach += "orbit" + std::to_string(orbit + 1);
    }

    return reach.empty() ? "clear" : reach;
}

void NoteYoggSaronHazards(Player* bot, uint32 phase)
{
    if (phase == 2 || phase == 3)
        RaidObs::NoteHazardCircle(bot->GetMap(), SPELL_KNOCK_BACK_TRIGGERED, ULDUAR_YOGG_SARON_MIDDLE,
                                  ULDUAR_YOGG_SARON_BODY_KNOCKBACK_RADIUS, ULDUAR_YOGG_SARON_OBS_SCAN_INTERVAL_MS);

    std::list<Creature*> crushers;
    bot->GetCreatureListWithEntryInGrid(crushers, NPC_CRUSHER_TENTACLE, ULDUAR_YOGG_SARON_P2_SPACING_MAX_FROM_MIDDLE);
    for (Creature* crusher : crushers)
    {
        if (!crusher->IsAlive())
            continue;

        char params[80];
        snprintf(params, sizeof(params), "\"facing\":%.2f,\"arc\":%.1f,\"range\":%.1f", crusher->GetOrientation(),
                 ULDUAR_YOGG_SARON_CRUSH_TRIGGER_ARC, ULDUAR_YOGG_SARON_CRUSH_RANGE);

        RaidObs::NoteHazard(bot->GetMap(), SPELL_CRUSH_CONE, crusher->GetPosition(), "wedge", params,
                            ULDUAR_YOGG_SARON_OBS_SCAN_INTERVAL_MS);
    }
}

void TickYoggSaronObs(PlayerbotAI* botAI)
{
    if (!RaidObs::Active())
        return;

    Player* bot = botAI->GetBot();
    YoggSaronEncounterState& state = YoggSaronStateFor(bot);

    YoggSaronHandover const handover = YoggSaronHandoverState(botAI);
    char const* handoverNote = "clear";
    if (handover.clearing)
        handoverNote = "clearing";
    else if (handover.active || state.handoverRingMs)
        handoverNote = "holding";

    RaidObs::NoteDerived(bot, "yogg.handover", handoverNote);

    uint32 const now = getMSTime();
    uint32& last = state.obsScanMs[bot->GetGUID()];
    if (last && getMSTimeDiff(last, now) < ULDUAR_YOGG_SARON_OBS_SCAN_INTERVAL_MS)
        return;

    last = now;
    uint32 const phase = YoggSaronPhase(botAI);
    state.phase = phase;

    bool noteHazards = false;
    if (phase &&
        (!state.hazardNoteMs || getMSTimeDiff(state.hazardNoteMs, now) >= ULDUAR_YOGG_SARON_OBS_SCAN_INTERVAL_MS))
    {
        state.hazardNoteMs = now;
        noteHazards = true;
    }

    RaidObs::NoteDerived(bot, "yogg.engaged", phase ? "engaged" : "idle");

    if (!phase)
        return;

    char const* roomNames[] = {"none", "arena", "stormwind", "icecrown", "chamber", "brain"};
    YoggSaronRoom const room = YoggSaronRoomOf(bot);
    RaidObs::NoteDerived(bot, "yogg.room", roomNames[room]);

    if (room == YOGG_SARON_ROOM_ARENA)
    {
        RaidObs::NoteDerived(bot, "yogg.cloudreach", DescribeCloudReach(bot));

        bool const handover = phase == 1 && YoggSaronHandoverState(botAI).active;

        if (phase != 1 || handover)
            RaidObs::NoteDerived(bot, "yogg.knockback", YoggSaronInBodyKnockback(bot) ? "inside" : "clear");

        if (phase == 2 || phase == 3)
        {
            std::vector<Position> const wedges = GetYoggSaronCrushWedges(botAI, ULDUAR_YOGG_SARON_CRUSH_RANGE);
            char const* crush = "clear";
            if (YoggSaronInCrusherReach(botAI, ULDUAR_YOGG_SARON_CRUSHER_REACH_TRIGGER_RADIUS))
                crush = "reach";
            else if (InYoggSaronCrushWedge(wedges, bot->GetPositionX(), bot->GetPositionY(),
                                           ULDUAR_YOGG_SARON_CRUSH_TRIGGER_ARC))
                crush = "wedge";
            else if (!wedges.empty())
                crush = "range";

            RaidObs::NoteDerived(bot, "yogg.crush", crush);

            if (phase == 2 && PlayerbotAI::IsMelee(bot))
            {
                Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
                bool const engaged = target && target->GetEntry() == NPC_CRUSHER_TENTACLE &&
                                     YoggSaronCrusherStunHolds(botAI, target);
                RaidObs::NoteDerived(bot, "yogg.stunned", engaged ? "engage" : "clear");
            }

            RaidObs::NoteDerived(
                bot, "yogg.deathray",
                bot->FindNearestCreature(NPC_DEATH_RAY, ULDUAR_YOGG_SARON_DEATH_RAY_TRIGGER_RADIUS, true) ? "inside"
                                                                                                          : "clear");
        }

        if (noteHazards)
            NoteYoggSaronHazards(bot, phase);

        return;
    }

    if (room == YOGG_SARON_ROOM_NONE)
        return;

    YoggSaronRoomStateOf(botAI);

    size_t const skulls = GetYoggSaronSkullsInArc(botAI).size();
    RaidObs::NoteDerived(bot, "yogg.skull", skulls ? std::to_string(skulls) + " in arc" : "clear");

    Unit* tentacle = YoggSaronLiveIllusionMob(botAI, ULDUAR_YOGG_SARON_SEARCH_RANGE);
    if (!tentacle)
    {
        RaidObs::NoteDerived(bot, "yogg.tentacle", "none");
        return;
    }

    float const reach = bot->GetExactDist2d(tentacle);
    RaidObs::NoteDerived(bot, "yogg.tentacle",
                         reach <= sPlayerbotAIConfig.MeleeDistance   ? "melee"
                         : reach <= sPlayerbotAIConfig.SpellDistance ? "spell"
                                                                     : "far");
}
}

bool YoggSaronReprisalHold(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return YoggSaronStateFor(bot).reprisalHold.count(bot->GetGUID()) != 0;
}

Player* YoggSaronRoomHealer(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    YoggSaronRoom const room = YoggSaronRoomOf(bot);
    Group* group = bot->GetGroup();
    if (!group || (room != YOGG_SARON_ROOM_STORMWIND && room != YOGG_SARON_ROOM_ICECROWN &&
                   room != YOGG_SARON_ROOM_CHAMBER))
        return nullptr;

    Player* nearest = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || !PlayerbotAI::IsHeal(member) ||
            YoggSaronRoomOf(member) != room)
            continue;

        if (!nearest || bot->GetExactDist2d(member) < bot->GetExactDist2d(nearest))
            nearest = member;
    }

    return nearest;
}

Player* YoggSaronReprisalHealTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    YoggSaronRoom const room = YoggSaronRoomOf(bot);
    Group* group = bot->GetGroup();
    if (!group || !bot->IsAlive() || !PlayerbotAI::IsHeal(bot) ||
        bot->GetHealthPct() < ULDUAR_YOGG_SARON_REPRISAL_STOP_PCT ||
        (room != YOGG_SARON_ROOM_STORMWIND && room != YOGG_SARON_ROOM_ICECROWN && room != YOGG_SARON_ROOM_CHAMBER))
        return nullptr;

    YoggSaronEncounterState const& state = YoggSaronStateFor(bot);
    if (state.reprisalHold.empty())
        return nullptr;

    Player* lowest = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || !state.reprisalHold.count(member->GetGUID()) ||
            member->GetHealthPct() >= ULDUAR_YOGG_SARON_REPRISAL_RESUME_PCT || YoggSaronRoomOf(member) != room ||
            !bot->IsWithinDistInMap(member, sPlayerbotAIConfig.HealDistance) || !bot->IsWithinLOSInMap(member))
            continue;

        if (!lowest || member->GetHealthPct() < lowest->GetHealthPct())
            lowest = member;
    }

    return lowest;
}

char const* YoggSaronReprisalHealSpell(PlayerbotAI* botAI, Player* target)
{
    Player* bot = botAI->GetBot();
    std::vector<char const*> heals;
    switch (bot->getClass())
    {
        case CLASS_PRIEST:
            heals = {"penance", "flash heal"};
            if (!botAI->HasAura("renew", target, false, true))
                heals.push_back("renew");
            break;
        case CLASS_SHAMAN:
            heals = {"riptide", "healing wave", "lesser healing wave"};
            break;
        case CLASS_PALADIN:
            heals = {"holy shock", "holy light", "flash of light"};
            break;
        case CLASS_DRUID:
            if (!botAI->HasAura("rejuvenation", target, false, true))
                heals.push_back("rejuvenation");
            heals.insert(heals.end(), {"swiftmend", "nourish"});
            break;
        default:
            break;
    }

    for (char const* heal : heals)
        if (botAI->CanCastSpell(heal, target))
            return heal;

    return nullptr;
}

uint32 YoggSaronInduceMadnessLeftMs(PlayerbotAI* botAI)
{
    Creature* brain = YoggSaronNearestCreature(botAI, NPC_BRAIN);
    if (!brain || !brain->IsAlive() || !brain->HasUnitState(UNIT_STATE_CASTING))
        return 0;

    Spell* induceMadness = brain->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!induceMadness || induceMadness->m_spellInfo->Id != SPELL_INDUCE_MADNESS)
        return 0;

    return static_cast<uint32>(std::max(1, induceMadness->GetCastTimeRemaining()));
}

uint32 YoggSaronExitLeadMs(Player* bot)
{
    YoggSaronEncounterState const& state = YoggSaronStateFor(bot);
    auto const lead = state.exitLeadMs.find(bot->GetGUID());
    return lead == state.exitLeadMs.end() ? ULDUAR_YOGG_SARON_EXIT_LEAD_FLOOR_MS : lead->second;
}

namespace
{
bool YoggSaronInduceMadnessWithin(PlayerbotAI* botAI, uint32 ms)
{
    uint32 const left = YoggSaronInduceMadnessLeftMs(botAI);
    return left && left <= ms;
}

bool YoggSaronPlatformTentacleAlive(PlayerbotAI* botAI)
{
    YoggSaronPassReads* reads = YoggSaronPassReadsFor(botAI);
    if (reads && reads->platformTentacleRead)
        return reads->platformTentacleAlive;

    bool alive = false;
    if (Creature* yogg = YoggSaronNearestCreature(botAI, NPC_YOGG_SARON))
    {
        std::list<Creature*> tentacles;
        yogg->GetCreatureListWithEntryInGrid(tentacles,
                                             {NPC_CRUSHER_TENTACLE, NPC_CORRUPTOR_TENTACLE, NPC_CONSTRICTOR_TENTACLE},
                                             ULDUAR_YOGG_SARON_PET_LEFTOVER_SEARCH_RADIUS);
        alive = std::any_of(tentacles.begin(), tentacles.end(), [](Creature* tentacle) { return tentacle->IsAlive(); });
    }

    if (reads)
    {
        reads->platformTentacleRead = true;
        reads->platformTentacleAlive = alive;
    }

    return alive;
}

void YoggSaronTickBrainHold(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    YoggSaronEncounterState& state = YoggSaronStateFor(bot);
    ObjectGuid const guid = bot->GetGUID();

    if (!bot->IsAlive() || PlayerbotAI::IsHeal(bot) || YoggSaronRoomOf(bot) != YOGG_SARON_ROOM_BRAIN)
    {
        state.brainHold.erase(guid);
        return;
    }

    Creature* brain = YoggSaronNearestCreature(botAI, NPC_BRAIN);
    Spell* induceMadness = brain && brain->IsAlive() ? brain->GetCurrentSpell(CURRENT_GENERIC_SPELL) : nullptr;
    if (!induceMadness || induceMadness->m_spellInfo->Id != SPELL_INDUCE_MADNESS)
    {
        state.brainHold.erase(guid);
        return;
    }

    uint32 const now = getMSTime();
    uint32 const left = static_cast<uint32>(std::max(0, induceMadness->GetCastTimeRemaining()));
    uint32 const castEndMs = now + left;

    YoggSaronEncounterState::BrainHold& hold = state.brainHold[guid];
    hold.seenMs = now;

    bool const finishing = hold.mode == YoggSaronEncounterState::BRAIN_HOLD_FINISH &&
                           (castEndMs > hold.finishCastEndMs ? castEndMs - hold.finishCastEndMs
                                                             : hold.finishCastEndMs - castEndMs) < IN_MILLISECONDS;
    if (finishing)
        return;

    float const health = brain->GetHealthPct();
    YoggSaronEncounterState::BrainHoldMode mode = YoggSaronEncounterState::BRAIN_HOLD_NONE;
    char const* branch = "open";
    if (health > ULDUAR_YOGG_SARON_BRAIN_DOT_STOP_PCT)
        mode = YoggSaronEncounterState::BRAIN_HOLD_NONE;
    else if (!YoggSaronPlatformTentacleAlive(botAI))
    {
        mode = YoggSaronEncounterState::BRAIN_HOLD_FINISH;
        branch = "clear";
    }
    else if (left <= ULDUAR_YOGG_SARON_BRAIN_RESUME_MS)
    {
        mode = YoggSaronEncounterState::BRAIN_HOLD_FINISH;
        branch = "finish";
    }
    else if (health <= ULDUAR_YOGG_SARON_BRAIN_HOLD_PCT)
    {
        mode = YoggSaronEncounterState::BRAIN_HOLD_FULL;
        branch = "hold";
    }
    else
    {
        mode = YoggSaronEncounterState::BRAIN_HOLD_DOTS;
        branch = "dots";
    }

    hold.mode = mode;
    if (mode == YoggSaronEncounterState::BRAIN_HOLD_FINISH)
        hold.finishCastEndMs = castEndMs;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.brainhold", branch);
}

void YoggSaronTickRoar(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->getClass() != CLASS_PALADIN)
        return;

    YoggSaronEncounterState& state = YoggSaronStateFor(bot);
    state.roarExpected = bot->GetMap()->Is25ManRaid() && YoggSaronInPhase3(botAI) &&
                         YoggKeepersFreedCount(botAI) < ULDUAR_YOGG_SARON_KEEPER_COUNT;
    if (!state.roarExpected)
        return;

    uint32 const now = getMSTime();
    if (!state.phase3StartMs)
        state.phase3StartMs = now;

    Creature* yogg = YoggSaronNearestCreature(botAI, NPC_YOGG_SARON);
    Spell* cast = yogg && yogg->IsAlive() ? yogg->GetCurrentSpell(CURRENT_GENERIC_SPELL) : nullptr;
    if (!cast || cast->m_spellInfo->Id != SPELL_DEAFENING_ROAR)
        return;

    uint32 const elapsed = static_cast<uint32>(std::max(0, cast->GetCastTime() - cast->GetCastTimeRemaining()));
    uint32 const start = now - elapsed;
    // Signed on purpose: a start a few ms early wraps getMSTimeDiff and lets a second paladin press.
    int32 const sinceLast = static_cast<int32>(start - state.lastRoarMs);
    if (!state.lastRoarMs || sinceLast > static_cast<int32>(ULDUAR_YOGG_SARON_ROAR_PERIOD_MS / 2))
        state.lastRoarMs = start;
}

void YoggSaronTickReprisal(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    YoggSaronEncounterState& state = YoggSaronStateFor(bot);
    ObjectGuid const guid = bot->GetGUID();
    bool const held = state.reprisalHold.count(guid) != 0;

    YoggSaronRoom const room = YoggSaronRoomOf(bot);
    bool const inIllusion =
        room == YOGG_SARON_ROOM_STORMWIND || room == YOGG_SARON_ROOM_ICECROWN || room == YOGG_SARON_ROOM_CHAMBER;

    float const line = held ? ULDUAR_YOGG_SARON_REPRISAL_RESUME_PCT : ULDUAR_YOGG_SARON_REPRISAL_STOP_PCT;
    bool hold = inIllusion && bot->IsAlive() && !PlayerbotAI::IsTank(bot) && !PlayerbotAI::IsHeal(bot) &&
                bot->GetHealthPct() < line;
    hold = hold && YoggSaronRoomHealer(botAI) && !YoggSaronInfluenceTentaclesCleared(botAI) &&
           !YoggSaronInduceMadnessWithin(botAI, ULDUAR_YOGG_SARON_REPRISAL_ALL_IN_MS);

    if (hold)
        state.reprisalHold.insert(guid);
    else if (held)
        state.reprisalHold.erase(guid);
}

YoggSaronEncounterState& YoggSaronShareStateFor(Player* bot)
{
    YoggSaronEncounterState& state = YoggSaronStateFor(bot);
    if (state.shareWave != state.waveOrdinal)
    {
        state.shareWave = state.waveOrdinal;
        state.icecrownCluster.clear();
        state.stormwindShare.clear();
    }

    return state;
}

uint8 YoggSaronIcecrownClusterAt(Unit const* unit)
{
    uint8 nearest = 0;
    for (uint8 cluster = 1; cluster < ULDUAR_YOGG_SARON_ICECROWN_CLUSTER_COUNT; ++cluster)
        if (unit->GetExactDist2d(ULDUAR_YOGG_SARON_ICECROWN_CLUSTERS[cluster]) <
            unit->GetExactDist2d(ULDUAR_YOGG_SARON_ICECROWN_CLUSTERS[nearest]))
            nearest = cluster;

    return nearest;
}

uint8 YoggSaronIcecrownClusterOf(YoggSaronEncounterState& state, Player* bot)
{
    auto const it = state.icecrownCluster.find(bot->GetGUID());
    if (it != state.icecrownCluster.end())
        return it->second;

    std::array<uint32, ULDUAR_YOGG_SARON_ICECROWN_CLUSTER_COUNT> planned{};
    for (auto const& [guid, cluster] : state.icecrownCluster)
        ++planned[cluster];

    constexpr std::array<uint8, ULDUAR_YOGG_SARON_ICECROWN_CLUSTER_COUNT> byLandingDistance = {2, 0, 1};
    uint8 joined = byLandingDistance[0];
    for (uint8 const cluster : byLandingDistance)
        if (planned[cluster] < planned[joined])
            joined = cluster;

    state.icecrownCluster[bot->GetGUID()] = joined;
    return joined;
}

bool YoggSaronIcecrownClusterManned(YoggSaronEncounterState const& state, Player* bot, uint8 cluster)
{
    for (auto const& [guid, planned] : state.icecrownCluster)
    {
        if (planned != cluster || guid == bot->GetGUID())
            continue;

        Player* member = ObjectAccessor::FindPlayer(guid);
        if (member && member->IsAlive() && YoggSaronRoomOf(member) == YOGG_SARON_ROOM_ICECROWN)
            return true;
    }

    return false;
}

uint32 YoggSaronTentacleAttackers(Player* bot, Unit* tentacle, YoggSaronRoom room, bool lowerGuidOnly = false)
{
    Group* group = bot->GetGroup();
    if (!group)
        return 0;

    uint32 attackers = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member != bot && member->IsAlive() && !PlayerbotAI::IsHeal(member) &&
            member->GetTarget() == tentacle->GetGUID() && YoggSaronRoomOf(member) == room &&
            (!lowerGuidOnly || member->GetGUID() < bot->GetGUID()))
            ++attackers;
    }

    return attackers;
}

Unit* YoggSaronTentacleToJoin(PlayerbotAI* botAI, std::vector<Unit*> const& mobs, YoggSaronRoom room)
{
    Player* bot = botAI->GetBot();
    Position stage;
    if (!YoggSaronIllusionStageSpot(bot, stage))
        return nullptr;

    std::vector<Unit*> nearStage;
    std::copy_if(mobs.begin(), mobs.end(), std::back_inserter(nearStage), [&stage](Unit* mob)
                 { return mob->GetExactDist2d(stage) <= ULDUAR_YOGG_SARON_STAGE_JOIN_RADIUS; });

    Unit* best = nullptr;
    std::pair<uint32, float> bestKey;
    for (Unit* mob : nearStage)
    {
        std::pair<uint32, float> const key{YoggSaronTentacleAttackers(bot, mob, room), mob->GetExactDist2d(stage)};
        if (!best || key < bestKey)
        {
            best = mob;
            bestKey = key;
        }
    }

    Unit* current = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    if (best && current && std::find(nearStage.begin(), nearStage.end(), current) != nearStage.end() &&
        (YoggSaronTentacleAttackers(bot, current, room) <= bestKey.first ||
         !YoggSaronTentacleAttackers(bot, current, room, true)))
        return current;

    return best;
}

void YoggSaronTickIllusionShare(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    YoggSaronRoom const room = YoggSaronRoomOf(bot);
    bool const applies = (room == YOGG_SARON_ROOM_ICECROWN || room == YOGG_SARON_ROOM_STORMWIND) &&
                         bot->IsAlive() && !PlayerbotAI::IsHeal(bot);

    YoggSaronEncounterState& state = YoggSaronShareStateFor(bot);
    if (!applies)
    {
        state.illusionShare.erase(bot->GetGUID());
        return;
    }

    std::vector<Unit*> const mobs = GetYoggSaronRoomIllusionMobs(botAI);
    YoggSaronEncounterState::IllusionShare& share = state.illusionShare[bot->GetGUID()];
    share.allowed.clear();

    auto const allow = [&share](Unit* mob) { share.allowed.push_back(mob->GetGUID()); };

    std::string label;
    if (YoggSaronInduceMadnessWithin(botAI, ULDUAR_YOGG_SARON_SHARE_LIFT_MS))
    {
        std::for_each(mobs.begin(), mobs.end(), allow);
        label = "lifted";
    }
    else if (room == YOGG_SARON_ROOM_ICECROWN)
    {
        static constexpr char const* names[ULDUAR_YOGG_SARON_ICECROWN_CLUSTER_COUNT] = {"west", "south", "east"};
        uint8 const own = YoggSaronIcecrownClusterOf(state, bot);

        std::array<bool, ULDUAR_YOGG_SARON_ICECROWN_CLUSTER_COUNT> live{};
        for (Unit* mob : mobs)
            live[YoggSaronIcecrownClusterAt(mob)] = true;

        if (live[own])
        {
            for (Unit* mob : mobs)
                if (YoggSaronIcecrownClusterAt(mob) == own)
                    allow(mob);

            label = names[own];
        }
        else
        {
            for (uint8 cluster = 0; cluster < ULDUAR_YOGG_SARON_ICECROWN_CLUSTER_COUNT; ++cluster)
            {
                if (!live[cluster] || YoggSaronIcecrownClusterManned(state, bot, cluster))
                    continue;

                for (Unit* mob : mobs)
                    if (YoggSaronIcecrownClusterAt(mob) == cluster)
                        allow(mob);
            }

            label = "orphan";
        }
    }
    else
    {
        YoggSaronEncounterState::StormwindShare& stormwind = state.stormwindShare[bot->GetGUID()];
        Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
        bool const onTentacle =
            target && target->IsAlive() &&
            std::find(ULDUAR_YOGG_SARON_ILLUSION_MOBS.begin(), ULDUAR_YOGG_SARON_ILLUSION_MOBS.end(),
                      target->GetEntry()) != ULDUAR_YOGG_SARON_ILLUSION_MOBS.end();

        if (stormwind.role == YoggSaronEncounterState::STORMWIND_FIRST)
        {
            if (onTentacle)
            {
                stormwind.held = target->GetGUID();
                stormwind.heldNear = target->GetExactDist2d(ULDUAR_YOGG_SARON_STORMWIND_STAGE_SPOT) <=
                                     ULDUAR_YOGG_SARON_STORMWIND_HELPER_RADIUS;
            }
            else if (!stormwind.held.IsEmpty())
            {
                Unit* held = ObjectAccessor::GetUnit(*bot, stormwind.held);
                if (!held || !held->IsAlive())
                    stormwind.role = stormwind.heldNear ? YoggSaronEncounterState::STORMWIND_HELPER
                                                        : YoggSaronEncounterState::STORMWIND_DONE;
            }
        }

        if (stormwind.role == YoggSaronEncounterState::STORMWIND_DONE)
        {
            for (Unit* mob : mobs)
                if ((onTentacle && mob == target) || !YoggSaronTentacleAttackers(bot, mob, room))
                    allow(mob);

            label = "done";
        }
        else
        {
            std::for_each(mobs.begin(), mobs.end(), allow);
            label = stormwind.role == YoggSaronEncounterState::STORMWIND_HELPER ? "helper" : "first";
        }
    }

    Unit* marked = botAI->GetAiObjectContext()->GetValue<Unit*>("rti target")->Get();
    if (marked && std::find(mobs.begin(), mobs.end(), marked) != mobs.end() &&
        std::find(share.allowed.begin(), share.allowed.end(), marked->GetGUID()) == share.allowed.end())
        allow(marked);

    share.staged = false;
    if (!mobs.empty() && share.allowed.empty())
    {
        if (Unit* join = YoggSaronTentacleToJoin(botAI, mobs, room))
        {
            allow(join);
            label = "joined";
        }
        else
        {
            share.staged = true;
            label = "staged";
        }
    }

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "yogg.share", label);
}

uint32 YoggSaronKickRank(char const* spell)
{
    static constexpr char const* order[] = {"wind shear",  "kick",         "pummel",    "mind freeze",
                                            "shield bash", "counterspell", "spell lock"};
    for (uint32 rank = 0; rank < std::size(order); ++rank)
        if (std::string(spell) == order[rank])
            return rank;

    return std::size(order);
}

void YoggSaronNudgeDrainLifeKicker(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group || !bot->IsAlive() || bot->GetPositionZ() < ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
        return;

    constexpr uint32 scanIntervalMs = 200;
    YoggSaronEncounterState& state = YoggSaronStateFor(bot);
    uint32 const now = getMSTime();
    if (getMSTimeDiff(state.kickScanMs, now) < scanIntervalMs)
        return;

    state.kickScanMs = now;
    for (auto it = state.kickNudges.begin(); it != state.kickNudges.end();)
        it = getMSTimeDiff(it->second, now) >= ULDUAR_YOGG_SARON_KICK_NUDGE_MS ? state.kickNudges.erase(it)
                                                                              : std::next(it);

    if (!YoggSaronInPhase3(botAI))
        return;

    std::unordered_set<ObjectGuid> nudged;
    for (Unit* guardian : YoggSaronInterruptCastersAround(botAI))
    {
        Spell* channel = YoggSaronDrainLifeKickable(guardian);

        bool const mark = YoggSaronMarkCaster(guardian);
        if (!channel || state.kickNudges.count(guardian->GetGUID()) ||
            (!mark && channel->GetSpellInfo()->GetDuration() - channel->GetCastTimeRemaining() <
                          static_cast<int32>(ULDUAR_YOGG_SARON_KICK_LAST_RESORT_MS)))
            continue;

        auto const claim = state.interruptClaims.find(guardian->GetGUID());
        if (claim != state.interruptClaims.end() &&
            getMSTimeDiff(claim->second.second, now) < ULDUAR_YOGG_SARON_INTERRUPT_CLAIM_MS)
            continue;

        bool waiting = false;
        Player* pick = nullptr;
        char const* pickSpell = nullptr;
        std::tuple<uint32, int32> pickKey;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            PlayerbotAI* memberAI = IsYoggSaronBot(member) ? GET_PLAYERBOT_AI(member) : nullptr;
            if (!memberAI || !member->IsAlive() || member->GetMap() != bot->GetMap() ||
                nudged.count(member->GetGUID()) || (!mark && YoggSaronMarkKicker(member)))
                continue;

            char const* ready = nullptr;
            for (char const* spell : YoggSaronInterruptSpellsFor(member, guardian))
            {
                if (YoggSaronKickInReach(memberAI, spell, guardian))
                {
                    ready = spell;
                    break;
                }
            }

            if (!ready)
                continue;

            Spell* cast = member->GetCurrentSpell(CURRENT_GENERIC_SPELL);
            if (cast && cast->getState() != SPELL_STATE_PREPARING)
                cast = nullptr;

            if (!cast)
                cast = member->GetCurrentSpell(CURRENT_CHANNELED_SPELL);

            if (!cast || cast->GetCastTimeRemaining() <= static_cast<int32>(ULDUAR_YOGG_SARON_KICK_WAIT_MS))
            {
                waiting = true;
                break;
            }

            if (PlayerbotAI::IsHeal(member))
                continue;

            std::tuple<uint32, int32> const key{YoggSaronKickRank(ready), -cast->GetCastTimeRemaining()};
            if (!pick || key < pickKey)
            {
                pick = member;
                pickSpell = ready;
                pickKey = key;
            }
        }

        if (waiting || !pick)
            continue;

        GET_PLAYERBOT_AI(pick)->RequestSpellInterrupt();
        state.kickNudges[guardian->GetGUID()] = now;
        nudged.insert(pick->GetGUID());

        if (RaidObs::Active())
            RaidObs::Note(bot, "yogg.kicknudge", pick->GetName() + " " + pickSpell);
    }
}

void YoggSaronPickMarkKickers(Player* bot, YoggSaronEncounterState& state, Unit* mark)
{
    state.markKickers.clear();

    using Rank = std::tuple<float, uint32, bool, ObjectGuid>;
    std::vector<std::pair<Rank, Player*>> ranked;
    for (GroupReference* ref = bot->GetGroup()->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        PlayerbotAI* memberAI = IsYoggSaronBot(member) ? GET_PLAYERBOT_AI(member) : nullptr;
        if (!memberAI || !member->IsAlive() || member->GetMap() != bot->GetMap() ||
            member->GetPositionZ() < ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT ||
            YoggSaronBeaconDragging(member) || state.beaconStages.count(member->GetGUID()))
            continue;

        bool found = false;
        Rank best;
        for (char const* spell : YoggSaronInterruptSpellsFor(member, mark))
        {
            uint32 const id = memberAI->GetAiObjectContext()->GetValue<uint32>("spell id", spell)->Get();
            SpellInfo const* info = id ? sSpellMgr->GetSpellInfo(id) : nullptr;
            if (!info || member->GetSpellCooldownDelay(id) > ULDUAR_YOGG_SARON_MARK_KICKER_READY_MS)
                continue;

            Rank const rank{-info->GetMaxRange(false), std::max(info->RecoveryTime, info->CategoryRecoveryTime),
                            PlayerbotAI::IsHeal(member), member->GetGUID()};
            if (!found || rank < best)
            {
                best = rank;
                found = true;
            }
        }

        if (found)
            ranked.emplace_back(best, member);
    }

    std::sort(ranked.begin(), ranked.end(),
              [](auto const& left, auto const& right) { return left.first < right.first; });

    std::string names;
    for (size_t i = 0; i < ranked.size() && i < ULDUAR_YOGG_SARON_MARK_KICKERS; ++i)
    {
        state.markKickers.push_back(ranked[i].second->GetGUID());
        names += (names.empty() ? "" : " ") + ranked[i].second->GetName();
    }

    if (RaidObs::Active())
        RaidObs::Note(bot, "yogg.markkick", "reserve " + names);
}

void YoggSaronSampleHealCut(Player* bot, YoggSaronEncounterState& state, Creature* yogg, uint32 now)
{
    constexpr uint32 sampleIntervalMs = 1000;
    if (!RaidObs::Active() || getMSTimeDiff(state.healCutScanMs, now) < sampleIntervalMs)
        return;

    state.healCutScanMs = now;

    std::list<Creature*> guardians;
    yogg->GetCreatureListWithEntryInGrid(guardians, NPC_IMMORTAL_GUARDIAN, ULDUAR_YOGG_SARON_BEACON_DRAG_SEARCH_RADIUS);

    uint32 hot = 0;
    uint32 hotCut = 0;
    uint32 drain = 0;
    uint32 drainCut = 0;
    for (Creature* guardian : guardians)
    {
        if (!guardian->IsAlive())
            continue;

        bool const cut = YoggSaronHealingCut(guardian);
        if (YoggSaronBeaconHealOn(guardian))
        {
            ++hot;
            hotCut += cut;
        }

        if (YoggSaronDrainLifeChannelling(guardian))
        {
            ++drain;
            drainCut += cut;
        }
    }

    std::string note = "hot " + std::to_string(hotCut) + "/" + std::to_string(hot) + " drain " +
                       std::to_string(drainCut) + "/" + std::to_string(drain);
    if (note == state.healCutNote)
        return;

    state.healCutNote = std::move(note);
    RaidObs::Note(bot, "yogg.healcut", state.healCutNote);
}

void YoggSaronTickBeacon(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot->IsAlive() || !bot->GetGroup() ||
        bot->GetPositionZ() < ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
        return;

    constexpr uint32 scanIntervalMs = 200;
    YoggSaronEncounterState& state = YoggSaronStateFor(bot);
    uint32 const now = getMSTime();
    if (getMSTimeDiff(state.beaconScanMs, now) < scanIntervalMs)
        return;

    state.beaconScanMs = now;

    if (!YoggSaronInPhase3(botAI) || YoggThorimKeeperActive(botAI))
    {
        state.beaconClockMs = 0;
        state.lastMarkMs = 0;
        state.stageDueMs = 0;
        state.markWindow = false;
        state.beaconStages.clear();
        state.markKickers.clear();
        state.healCutNote.clear();
        return;
    }

    if (!state.beaconClockMs)
        state.beaconClockMs = now;

    Creature* yogg = YoggSaronNearestCreature(botAI, NPC_YOGG_SARON);
    if (!yogg)
        return;

    YoggSaronSampleHealCut(bot, state, yogg, now);

    std::list<Creature*> found;
    yogg->GetCreatureListWithEntryInGrid(found, NPC_MARKED_IMMORTAL_GUARDIAN,
                                         ULDUAR_YOGG_SARON_BEACON_DRAG_SEARCH_RADIUS);
    Unit* mark = nullptr;
    for (Creature* guardian : found)
        if (guardian->IsAlive())
            mark = guardian;

    bool const window = mark || !state.beaconDrags.empty();
    if (window && !state.markWindow && mark)
    {
        state.markWindow = true;
        state.lastMarkMs = now;
        YoggSaronPickMarkKickers(bot, state, mark);
    }
    else if (!window && state.markWindow)
    {
        state.markWindow = false;
        state.markKickers.clear();
        state.beaconStages.clear();
    }

    if (window)
        return;

    uint32 const due = (state.lastMarkMs ? state.lastMarkMs : state.beaconClockMs) + ULDUAR_YOGG_SARON_BEACON_PERIOD_MS;
    int32 const left = static_cast<int32>(due - now);
    if (left < -static_cast<int32>(ULDUAR_YOGG_SARON_BEACON_STAGE_TIMEOUT_MS))
    {
        for (auto const& [guid, lane] : state.beaconStages)
            if (Player* stager = ObjectAccessor::GetPlayer(*bot, guid); stager && RaidObs::Active())
                RaidObs::NoteDerived(stager, "yogg.beaconstage", "timeout");

        state.beaconStages.clear();
        return;
    }

    if (left > static_cast<int32>(ULDUAR_YOGG_SARON_BEACON_STAGE_LEAD_MS) || state.stageDueMs == due)
        return;

    state.stageDueMs = due;

    std::list<Creature*> unmarked;
    yogg->GetCreatureListWithEntryInGrid(unmarked, NPC_IMMORTAL_GUARDIAN, ULDUAR_YOGG_SARON_BEACON_DRAG_SEARCH_RADIUS);
    std::vector<Player*> const order =
        YoggSaronBeaconDraggerOrder(bot, unmarked, static_cast<uint32>(std::max(0, left)));

    size_t const stagers =
        bot->GetMap()->Is25ManRaid() ? ULDUAR_YOGG_SARON_BEACON_STAGERS_25 : ULDUAR_YOGG_SARON_BEACON_STAGERS_10;
    size_t const count = std::min<size_t>(stagers, YOGG_SARON_BEACON_LANES.size());
    std::string names;
    for (size_t i = 0; i < order.size() && i < count; ++i)
    {
        state.beaconStages[order[i]->GetGUID()] = static_cast<uint8>(i);
        names += (names.empty() ? "" : " ") + order[i]->GetName();
    }

    if (RaidObs::Active())
        RaidObs::Note(bot, "yogg.beaconstage", "pick " + names);
}
}

bool YoggSaronIllusionTentacleAllowed(PlayerbotAI* botAI, Unit* tentacle)
{
    Player* bot = botAI->GetBot();
    YoggSaronEncounterState const& state = YoggSaronStateFor(bot);
    auto const it = state.illusionShare.find(bot->GetGUID());
    if (it == state.illusionShare.end())
        return true;

    std::vector<ObjectGuid> const& allowed = it->second.allowed;
    return std::find(allowed.begin(), allowed.end(), tentacle->GetGUID()) != allowed.end();
}

bool YoggSaronIllusionStaged(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    YoggSaronEncounterState const& state = YoggSaronStateFor(bot);
    auto const it = state.illusionShare.find(bot->GetGUID());

    return it != state.illusionShare.end() && it->second.staged;
}

bool YoggSaronIllusionStageSpot(Player* bot, Position& spot)
{
    switch (YoggSaronRoomOf(bot))
    {
        case YOGG_SARON_ROOM_ICECROWN:
            spot = ULDUAR_YOGG_SARON_ICECROWN_STAGE_SPOT;
            return true;
        case YOGG_SARON_ROOM_STORMWIND:
            spot = ULDUAR_YOGG_SARON_STORMWIND_STAGE_SPOT;
            return true;
        default:
            return false;
    }
}

namespace
{
bool YoggSaronHoldsPhase1Guardian(Player* human)
{
    for (Unit* attacker : human->getAttackers())
        if (attacker->GetVictim() == human && attacker->GetEntry() == NPC_GUARDIAN_OF_YS && attacker->IsAlive())
            return true;

    return false;
}

void YoggSaronCallHumanNova(Player* sender, Player* human)
{
    if (!YoggSaronInPhase1Room(human) || !YoggSaronHoldsPhase1Guardian(human))
        return;

    uint32 const weakest = ReadyTankDefensiveSpell(human, false, 0, 0);
    uint32 const needMs = YoggSaronNovaNeedMs(human, weakest && IsMagicOnlyTankDefensive(weakest));
    if (!needMs)
        return;

    CallHumanTankDefensive(sender, human, ReadyTankDefensiveSpell(human, false, 0, needMs), 0,
                           "a Guardian next to you is about to explode", "yogg.humancall");
}

void YoggSaronCallHumanPile(Player* sender, Player* human)
{
    if (human->getAttackers().empty() || YoggSaronOnBrainLevel(human))
        return;

    uint32 const spellId = ReadyTankDefensiveSpell(human, true, 0, 0);
    if (!spellId)
        return;

    uint32 const healed = YoggSaronPileHealedOn(human);
    if (healed >= ULDUAR_YOGG_SARON_BEACON_PILE_RESET_GUARDIANS)
    {
        CallHumanTankDefensive(sender, human, spellId, 0,
                               "the beacon healed " + std::to_string(healed) + " Guardians on you", "yogg.humancall");
        return;
    }

    uint32 const pile = YoggSaronGuardiansOn(human);
    if (pile >= ULDUAR_YOGG_SARON_TANK_SWAP_MIN_GUARDIANS && human->GetHealthPct() < ULDUAR_YOGG_SARON_TANK_SWAP_PCT)
        CallHumanTankDefensive(sender, human, spellId, 0, std::to_string(pile) + " Guardians on you and you are low",
                               "yogg.humancall");
}

void YoggSaronTickHumanTankCalls(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group || !bot->IsAlive())
        return;

    constexpr uint32 scanIntervalMs = 250;
    YoggSaronEncounterState& state = YoggSaronStateFor(bot);
    uint32 const now = getMSTime();
    if (getMSTimeDiff(state.humanCallScanMs, now) < scanIntervalMs)
        return;

    state.humanCallScanMs = now;

    uint32 const phase = YoggSaronPhase(botAI);
    if (phase != 1 && phase != 3)
        return;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* human = ref->GetSource();
        if (!human || IsYoggSaronBot(human) || !human->IsAlive() || human->GetMap() != bot->GetMap())
            continue;

        if (phase == 1)
            YoggSaronCallHumanNova(bot, human);
        else
            YoggSaronCallHumanPile(bot, human);
    }
}

void YoggSaronCutCastsInDeathRays(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group || !bot->IsAlive())
        return;

    YoggSaronEncounterState& state = YoggSaronStateFor(bot);
    uint32 const now = getMSTime();
    if (getMSTimeDiff(state.castCutScanMs, now) < ULDUAR_YOGG_SARON_DEATH_RAY_CAST_CUT_SCAN_MS)
        return;

    state.castCutScanMs = now;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!IsYoggSaronBot(member) || !member->IsAlive() || member->GetMap() != bot->GetMap() ||
            member->GetPositionZ() <= ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
            continue;

        Spell* spell = member->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!spell || spell->getState() != SPELL_STATE_PREPARING ||
            spell->GetCastTimeRemaining() <= ULDUAR_YOGG_SARON_DEATH_RAY_CAST_FINISH_MS)
            continue;

        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        if (!memberAI || !memberAI->CanMove() ||
            !member->FindNearestCreature(NPC_DEATH_RAY, ULDUAR_YOGG_SARON_DEATH_RAY_TRIGGER_RADIUS, true))
            continue;

        memberAI->RequestSpellInterrupt();
        if (RaidObs::Active())
            RaidObs::Note(member, "yogg.castcut", std::to_string(spell->GetSpellInfo()->Id));
    }
}
}

void YoggSaronTick(PlayerbotAI* botAI)
{
    YoggSaronTickHumanTankCalls(botAI);
    YoggSaronCutCastsInDeathRays(botAI);
    YoggSaronTickHandover(botAI);
    YoggSaronTickRtiMark(botAI);
    YoggSaronTickReprisal(botAI);
    YoggSaronTickIllusionShare(botAI);
    YoggSaronTickBrainHold(botAI);
    YoggSaronTickRoar(botAI);
    YoggSaronTickBeacon(botAI);
    YoggSaronNudgeDrainLifeKicker(botAI);
    TickYoggSaronObs(botAI);
}

bool YoggSaronBrainHeld(PlayerbotAI* botAI)
{
    YoggSaronEncounterState& state = YoggSaronStateFor(botAI->GetBot());
    auto const it = state.brainHold.find(botAI->GetBot()->GetGUID());

    return it != state.brainHold.end() && it->second.mode == YoggSaronEncounterState::BRAIN_HOLD_FULL;
}

bool YoggSaronBrainDotHeld(PlayerbotAI* botAI)
{
    YoggSaronEncounterState& state = YoggSaronStateFor(botAI->GetBot());
    auto const it = state.brainHold.find(botAI->GetBot()->GetGUID());

    return it != state.brainHold.end() && (it->second.mode == YoggSaronEncounterState::BRAIN_HOLD_FULL ||
                                           it->second.mode == YoggSaronEncounterState::BRAIN_HOLD_DOTS);
}
