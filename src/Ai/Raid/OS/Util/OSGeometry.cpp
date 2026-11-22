/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OSHelpers.h"
#include "Creature.h"
#include "Group.h"
#include "GroupReference.h"
#include "Map.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "Unit.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <list>
#include <string>

namespace OsHelpers
{

Position const SAFE_AREA_SOUTH    = { 3244.140f, 512.597f, 58.6534f, 0.0f };
Position const SAFE_AREA_NORTH    = { 3242.840f, 553.979f, 58.8272f, 0.0f };
Position const TENEBRON_LANDING   = { 3249.750f, 566.952f, 59.4240f, 0.0f };
Position const SHADRON_LANDING    = { 3230.496f, 533.000f, 59.5600f, 0.0f };
Position const VESPERON_LANDING   = { 3269.713f, 532.791f, 59.5130f, 0.0f };

Position const TENEBRON_TANK_SPOT = { 3249.8467f, 563.38010f, 59.4240f, 0.0f };
Position const SHADRON_TANK_SPOT  = { 3228.9385f, 534.65955f, 59.5600f, 0.0f };
Position const VESPERON_TANK_SPOT = { 3266.0000f, 556.00000f, 59.5130f, 0.0f };

std::array<int32, 9> const LEFT_WAVE_Y = { 476, 484, 492, 524, 532, 540, 572, 580, 588 };
std::array<int32, 6> const RIGHT_WAVE_Y = { 500, 508, 516, 548, 556, 564 };

constexpr float TSUNAMI_LETHAL_HALF_WIDTH = 8.5f;

constexpr float TSUNAMI_SPENT_SCALE = 0.5f;

constexpr uint32 TSUNAMI_HAZARD_TTL_MS = 11000;

constexpr float DRAKE_LANDING_SPEED = 21.0f;

constexpr float DRAKE_LANDING_SPEED_RATE = 2.0f;

namespace
{

template <size_t N>
float NearestWaveLine(float y, std::array<int32, N> const& lines)
{
    float best = std::fabs(y - static_cast<float>(lines[0]));
    for (int32 const line : lines)
        best = std::min(best, std::fabs(y - static_cast<float>(line)));

    return best;
}

char const* WaveName(TsunamiWave wave)
{
    switch (wave)
    {
        case TsunamiWave::Left:
            return "left";
        case TsunamiWave::Right:
            return "right";
        default:
            return "none";
    }
}

char const* CorridorGroupName(CorridorGroup group)
{
    switch (group)
    {
        case CorridorGroup::Tank:
            return "tank";
        case CorridorGroup::Melee:
            return "melee";
        default:
            return "raid";
    }
}

TsunamiWave WaveSideOf(Creature const* tsunami)
{
    int32 const y = int32(tsunami->GetPositionY());
    if (std::find(LEFT_WAVE_Y.begin(), LEFT_WAVE_Y.end(), y) != LEFT_WAVE_Y.end())
        return TsunamiWave::Left;
    if (std::find(RIGHT_WAVE_Y.begin(), RIGHT_WAVE_Y.end(), y) != RIGHT_WAVE_Y.end())
        return TsunamiWave::Right;

    return TsunamiWave::None;
}

Player* PlatformEyes(Player* bot)
{
    if (!bot)
        return nullptr;

    if (!HasTwilightShift(bot))
        return bot;

    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    Player* fallback = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->GetMapId() != OS_MAP_ID ||
            HasTwilightShift(member) || !InsideRoom(member))
        {
            continue;
        }

        if (PlayerbotAI::IsMainTank(member) || IsOffTank(member))
            return member;

        if (!fallback)
            fallback = member;
    }
    return fallback;
}

void CollectTsunamis(Player* bot, std::list<Creature*>& out)
{
    if (Player* eyes = PlatformEyes(bot))
        eyes->GetCreatureListWithEntryInGrid(out, NpcId::FlameTsunami, ROOM_SEARCH_RADIUS);
}

}

TsunamiWave ClassifyTsunamiWave(Player* bot)
{
    std::list<Creature*> tsunamis;
    CollectTsunamis(bot, tsunamis);

    TsunamiWave live = TsunamiWave::None;
    for (Creature* tsunami : tsunamis)
    {
        if (!tsunami || !tsunami->IsAlive() || tsunami->GetObjectScale() < TSUNAMI_SPENT_SCALE)
            continue;

        switch (WaveSideOf(tsunami))
        {
            case TsunamiWave::Left:
                if (tsunami->GetPositionX() <= bot->GetPositionX() + TSUNAMI_LETHAL_HALF_WIDTH)
                    live = TsunamiWave::Left;
                break;
            case TsunamiWave::Right:
                if (tsunami->GetPositionX() >= bot->GetPositionX() - TSUNAMI_LETHAL_HALF_WIDTH)
                    live = TsunamiWave::Right;
                break;
            default:
                break;
        }

        if (live != TsunamiWave::None)
            break;
    }

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "sartharion.wave", WaveName(live));

    return live;
}

void NoteTsunamiHazards(Player* bot, std::unordered_set<ObjectGuid>& traced)
{
    std::list<Creature*> tsunamis;
    CollectTsunamis(bot, tsunamis);

    for (Creature* tsunami : tsunamis)
    {
        if (!tsunami || !tsunami->IsAlive() || !traced.insert(tsunami->GetGUID()).second)
            continue;

        TsunamiWave const side = WaveSideOf(tsunami);
        if (side == TsunamiWave::None)
            continue;

        char params[64];
        snprintf(params, sizeof(params), "\"y\":%d,\"side\":\"%s\",\"half\":%.1f",
                 int32(tsunami->GetPositionY()), WaveName(side), TSUNAMI_LETHAL_HALF_WIDTH);

        RaidObs::NoteHazard(bot->GetMap(), SpellId::FlameTsunamiDamageAura, tsunami->GetPosition(),
                            "wave", params, TSUNAMI_HAZARD_TTL_MS);
    }
}

bool WaveClearsY(float y, TsunamiWave wave)
{
    float const margin = TSUNAMI_LETHAL_HALF_WIDTH + CORRIDOR_ARRIVAL_TOLERANCE;

    switch (wave)
    {
        case TsunamiWave::Left:
            return NearestWaveLine(y, LEFT_WAVE_Y) >= margin;
        case TsunamiWave::Right:
            return NearestWaveLine(y, RIGHT_WAVE_Y) >= margin;
        default:
            return true;
    }
}

CorridorGroup CorridorGroupFor(Player* bot)
{
    if (!bot)
        return CorridorGroup::Raid;

    if (HasTwilightShift(bot))
        return CorridorGroup::Raid;

    if (PlayerbotAI::IsHeal(bot) || PlayerbotAI::IsRanged(bot))
        return CorridorGroup::Raid;

    Unit* boss = GetSartharion(bot);
    if (!boss)
        return CorridorGroup::Raid;

    if (PlayerbotAI::IsMainTank(bot))
        return CorridorGroup::Tank;

    return bot->GetVictim() == boss ? CorridorGroup::Melee : CorridorGroup::Raid;
}

float SafeCorridorY(Player* bot)
{
    CorridorGroup const group = CorridorGroupFor(bot);
    TsunamiWave const wave = ClassifyTsunamiWave(bot);

    float y;

    if (group == CorridorGroup::Melee)
        y = wave == TsunamiWave::Right ? RAID_CORRIDOR_RIGHT_Y : TANK_CORRIDOR_LEFT_Y;
    else if (group == CorridorGroup::Tank)
        y = wave == TsunamiWave::Right ? TANK_CORRIDOR_RIGHT_Y : TANK_CORRIDOR_LEFT_Y;
    else
        y = wave == TsunamiWave::Left ? RAID_CORRIDOR_LEFT_Y : RAID_CORRIDOR_RIGHT_Y;

    if (RaidObs::Active())
    {
        char note[32];
        snprintf(note, sizeof(note), "%s %.1f", CorridorGroupName(group), y);
        RaidObs::NoteDerived(bot, "sartharion.corridor", note);
    }

    return y;
}

float TankHoldX(Player* bot)
{
    return ClassifyTsunamiWave(bot) == TsunamiWave::Right ? TANK_HOLD_RIGHT_X : TANK_HOLD_LEFT_X;
}

float CorridorToleranceFor(Player* bot)
{
    return bot && PlayerbotAI::IsMainTank(bot) ? TANK_ARRIVAL_TOLERANCE : CORRIDOR_ARRIVAL_TOLERANCE;
}

bool InsideRoom(WorldObject const* object)
{
    if (!object)
        return false;

    return object->GetPositionX() >= ROOM_MIN_X && object->GetPositionX() <= ROOM_MAX_X &&
           object->GetPositionY() >= ROOM_MIN_Y && object->GetPositionY() <= ROOM_MAX_Y &&
           object->GetPositionZ() <= ROOM_MAX_Z;
}

bool OffThePlatform(Player* bot)
{
    if (!bot)
        return false;

    float const y = bot->GetPositionY();
    if (y < PLATFORM_MIN_Y - OFF_PLATFORM_MARGIN || y > PLATFORM_MAX_Y + OFF_PLATFORM_MARGIN)
        return true;

    float const x = bot->GetPositionX();
    return x < PlatformMinX(y) - OFF_PLATFORM_MARGIN || x > PlatformMaxX(y) + OFF_PLATFORM_MARGIN;
}

float PlatformMaxX(float y)
{
    if (y < PLATFORM_TIP_MAX_Y)
        return PLATFORM_MAX_X_TIP;

    if (y < PLATFORM_NECK_MAX_Y)
        return PLATFORM_MAX_X_NECK;

    if (y < PLATFORM_SOUTH_MAX_Y)
        return PLATFORM_MAX_X_SOUTH;

    if (y >= PLATFORM_WIDE_MIN_Y && y <= PLATFORM_WIDE_MAX_Y)
        return PLATFORM_MAX_X_WIDE;

    return PLATFORM_MAX_X_CORE;
}

float PlatformMinX(float y)
{
    return y < PLATFORM_WIDE_MIN_Y ? PLATFORM_MIN_X_SOUTH : PLATFORM_MIN_X_CORE;
}

void ClampToPlatform(float& x, float& y)
{
    y = std::clamp(y, PLATFORM_MIN_Y, PLATFORM_MAX_Y);
    x = std::clamp(x, PlatformMinX(y), PlatformMaxX(y));
}

void ClampToRangeMarker(float& x, float& y)
{
    Position const* anchor = nullptr;
    float bestChord = -1.0f;
    for (Position const* candidate : { &SAFE_AREA_SOUTH, &SAFE_AREA_NORTH })
    {
        float const dy = y - candidate->GetPositionY();
        float const chord = RANGE_MARKER_SAFE_RADIUS * RANGE_MARKER_SAFE_RADIUS - dy * dy;
        if (chord > bestChord)
        {
            bestChord = chord;
            anchor = candidate;
        }
    }

    if (!anchor)
        return;

    if (bestChord <= 0.0f)
    {
        float const dx = x - anchor->GetPositionX();
        float const dy = y - anchor->GetPositionY();
        float const dist = std::sqrt(dx * dx + dy * dy);
        if (dist < 0.01f)
            return;

        x = anchor->GetPositionX() + dx / dist * RANGE_MARKER_SAFE_RADIUS;
        y = anchor->GetPositionY() + dy / dist * RANGE_MARKER_SAFE_RADIUS;
        ClampToPlatform(x, y);
        return;
    }

    float const halfChord = std::sqrt(bestChord);
    x = std::clamp(x, anchor->GetPositionX() - halfChord, anchor->GetPositionX() + halfChord);
}

void ClampDestination(float& x, float& y)
{
    ClampToPlatform(x, y);
    ClampToRangeMarker(x, y);
}

bool ResolveMoveDestination(Player* bot, float& x, float& y, float& z, bool rejectOnCollision)
{
    z = bot->GetMapWaterOrGroundLevel(x, y, bot->GetPositionZ());
    if (z <= INVALID_HEIGHT)
        z = bot->GetPositionZ();

    return bot->GetMap()->CheckCollisionAndGetValidCoords(bot, bot->GetPositionX(), bot->GetPositionY(),
                                                          bot->GetPositionZ(), x, y, z, rejectOnCollision);
}

Position const* LandingPositionFor(uint32 entry)
{
    switch (entry)
    {
        case NpcId::Tenebron:
        case NpcId::TenebronH:
            return &TENEBRON_LANDING;
        case NpcId::Shadron:
        case NpcId::ShadronH:
            return &SHADRON_LANDING;
        case NpcId::Vesperon:
        case NpcId::VesperonH:
            return &VESPERON_LANDING;
        default:
            return nullptr;
    }
}

Position const* DrakeTankSpotFor(uint32 entry)
{
    switch (entry)
    {
        case NpcId::Tenebron:
        case NpcId::TenebronH:
            return &TENEBRON_TANK_SPOT;
        case NpcId::Shadron:
        case NpcId::ShadronH:
            return &SHADRON_TANK_SPOT;
        case NpcId::Vesperon:
        case NpcId::VesperonH:
            return &VESPERON_TANK_SPOT;
        default:
            return nullptr;
    }
}

Unit* FindUnitByEntries(Player* bot, std::vector<uint32> const& entries, float radius,
                        bool requireSelectable)
{
    if (!bot)
        return nullptr;

    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, entries, radius);

    Unit* nearest = nullptr;
    float best = 0.0f;
    for (Creature* creature : found)
    {
        if (!creature || !creature->IsAlive())
            continue;

        if (requireSelectable && creature->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            continue;

        if (!InsideRoom(creature))
            continue;

        float const dist = bot->GetExactDist2d(creature);
        if (!nearest || dist < best)
        {
            nearest = creature;
            best = dist;
        }
    }
    return nearest;
}

bool FissureBlocks(Player* bot, float x, float y)
{
    if (!bot)
        return false;

    float const radius = FISSURE_CLEAR_RADIUS + bot->GetExactDist2d(x, y);

    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, { NpcId::TwilightFissure, NpcId::TwilightFissureH },
                                        radius);

    for (Creature* fissure : found)
    {
        if (!fissure || !fissure->IsAlive() || !InsideRoom(fissure))
            continue;

        if (fissure->GetExactDist2d(x, y) < FISSURE_CLEAR_RADIUS)
            return true;
    }
    return false;
}

Unit* FindInboundDrake(Player* bot)
{
    if (!bot)
        return nullptr;

    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, DRAKE_ENTRIES, ROOM_SEARCH_RADIUS);

    Unit* soonest = nullptr;
    float bestEta = 0.0f;
    for (Creature* drake : found)
    {
        if (!drake || !drake->IsAlive() || !drake->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            continue;

        if (drake->GetSpeedRate(MOVE_FLIGHT) < DRAKE_LANDING_SPEED_RATE)
            continue;

        float const eta = SecondsUntilLanding(drake);
        if (eta < 0.0f)
            continue;

        if (!soonest || eta < bestEta)
        {
            soonest = drake;
            bestEta = eta;
        }
    }
    return soonest;
}

float SecondsUntilLanding(Unit* drake)
{
    if (!drake)
        return -1.0f;

    Position const* landing = LandingPositionFor(drake->GetEntry());
    if (!landing)
        return -1.0f;

    return drake->GetExactDist(landing) / DRAKE_LANDING_SPEED;
}

bool IsDrakeEntry(uint32 entry)
{
    return std::find(DRAKE_ENTRIES.begin(), DRAKE_ENTRIES.end(), entry) != DRAKE_ENTRIES.end();
}

std::vector<Unit*> LandedDrakes(Player* bot)
{
    std::vector<Unit*> landed;
    if (!bot)
        return landed;

    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, DRAKE_ENTRIES, ROOM_SEARCH_RADIUS);

    for (Creature* drake : found)
    {
        if (drake && drake->IsAlive() && !drake->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) && InsideRoom(drake))
            landed.push_back(drake);
    }
    return landed;
}

Unit* NewestLandedDrake(Player* bot)
{
    static std::array<std::vector<uint32>, 3> const drakeOrder = { {
        { NpcId::Vesperon, NpcId::VesperonH },
        { NpcId::Shadron, NpcId::ShadronH },
        { NpcId::Tenebron, NpcId::TenebronH },
    } };

    std::vector<Unit*> drakes = LandedDrakes(bot);
    for (auto const& tier : drakeOrder)
    {
        for (Unit* drake : drakes)
        {
            if (std::find(tier.begin(), tier.end(), drake->GetEntry()) != tier.end())
                return drake;
        }
    }
    return nullptr;
}

Unit* OffTankChargeFor(Player* bot)
{
    if (Unit* drake = NewestLandedDrake(bot))
        return drake;

    return FindUnitByEntries(bot, OFFTANK_PICKUP_ENTRIES, LAVA_BLAZE_TAUNT_RANGE);
}

Position OffTankAnchor(Player* bot)
{
    if (!bot)
        return Position();

    Unit* drake = NewestLandedDrake(bot);
    Position const* spot = drake ? DrakeTankSpotFor(drake->GetEntry()) : &TENEBRON_TANK_SPOT;

    bool const onSpot = WaveClearsY(spot->GetPositionY(), ClassifyTsunamiWave(bot));
    float const y = onSpot ? spot->GetPositionY() : SafeCorridorY(bot);

    if (RaidObs::Active())
    {
        std::string const anchor = std::string(drake ? drake->GetName() : "none") +
                                   (onSpot ? " spot" : " corridor");
        RaidObs::NoteDerived(bot, "sartharion.offtank", anchor);
    }

    return Position(spot->GetPositionX(), y, bot->GetPositionZ(), 0.0f);
}

float RaidLineX(Player* bot)
{
    float x = RAID_ENTRY_ANCHOR_X + RAID_LINE_OFFSET_X;
    float y = SafeCorridorY(bot);
    ClampDestination(x, y);
    return x;
}

bool TwilightRealmWaveWait(Player* bot)
{
    if (!bot || bot->GetMapId() != OS_MAP_ID || !HasTwilightShift(bot))
        return false;

    return !TwilightAddsAlive(bot) && ClassifyTsunamiWave(bot) != TsunamiWave::None;
}

bool BehindDrake(Unit* drake, Position const& point)
{
    if (!drake)
        return false;

    float const front = 2.0f * DRAKE_REAR_GATE_DEGREES * static_cast<float>(M_PI) / 180.0f;
    return !drake->HasInArc(front, &point);
}

bool InSartharionCone(Unit* boss, Position const& point)
{
    if (!boss)
        return false;

    float const arc = (SARTHARION_CONE_DEGREES + SARTHARION_CONE_MARGIN_DEGREES) *
                      static_cast<float>(M_PI) / 180.0f;
    float const dist = boss->GetExactDist2d(point.GetPositionX(), point.GetPositionY());

    if (dist <= FLAME_BREATH_RANGE && boss->HasInArc(arc, &point))
        return true;

    return dist <= TAIL_LASH_RANGE && !boss->HasInArc(2.0f * static_cast<float>(M_PI) - arc, &point);
}

}
