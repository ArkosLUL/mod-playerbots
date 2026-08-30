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
#include "Unit.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <list>

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

// Whole-number spawn offsets straight out of FlameTsunamiLeftOffsets / FlameTsunamiRightOffsets. The
// two sets are disjoint, which is what makes the truncating comparison below exact.
std::array<int32, 9> const LEFT_WAVE_Y = { 476, 484, 492, 524, 532, 540, 572, 580, 588 };
std::array<int32, 6> const RIGHT_WAVE_Y = { 500, 508, 516, 548, 556, 564 };

// Spell 57491 radius 7.0 plus the player's 1.5yd combat reach. The caster's own reach is not added.
constexpr float TSUNAMI_LETHAL_HALF_WIDTH = 8.5f;

// SendLavaWaves(false) strips the damage aura and shrinks the wave to 0.1 scale in the same call, 11s
// after the summon. A wave is 1.0 before and during its run, so scale tells a spent wave from a live
// one without the ambiguity the aura carries - it is also absent for the first 3.6s, while the wave is
// still parked at its spawn X and very much coming.
constexpr float TSUNAMI_SPENT_SCALE = 0.5f;

// Base flight speed 7.0 at the rate the script sets right before MovePoint(POINT_LANDING).
constexpr float DRAKE_LANDING_SPEED = 21.0f;

// Anything below this is the pre-call patrol, which runs at rate 1.0.
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

TsunamiWave WaveSideOf(Creature const* tsunami)
{
    int32 const y = int32(tsunami->GetPositionY());
    if (std::find(LEFT_WAVE_Y.begin(), LEFT_WAVE_Y.end(), y) != LEFT_WAVE_Y.end())
        return TsunamiWave::Left;
    if (std::find(RIGHT_WAVE_Y.begin(), RIGHT_WAVE_Y.end(), y) != RIGHT_WAVE_Y.end())
        return TsunamiWave::Right;

    return TsunamiWave::None;
}

// Everything a shifted bot knows about the platform comes through here. Phase 16 filters the tsunamis
// out of its own searches, so it asks someone who is still outside - which is what a raid does over
// voice. The two tanks are who it asks: ResolveAssignments skips every tank bar the second assist, so
// the main tank and the first off-tank are the only two bots that can never be in the realm. Coordinates
// are shared across phases, so their list still answers "can it reach me" against the shifted bot's own X.
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

        // Both tanks down. Anyone left outside answers the same question - the side is a property of the
        // wave, not of who is looking at it.
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

    for (Creature* tsunami : tsunamis)
    {
        if (!tsunami || !tsunami->IsAlive() || tsunami->GetObjectScale() < TSUNAMI_SPENT_SCALE)
            continue;

        // Left waves spawn at X 3211 and travel +X, right waves at 3286 travelling -X. A wave that has
        // already swept past this bot cannot come back, so it stops counting even though the creature
        // is on the map until its 13.5s despawn. Waiting for that despawn is what left the raid sitting
        // on the wrong hold for 5s after every wave. The pre-launch window still counts: the wave is
        // parked at its spawn X, which reads as coming, and those 3.6s are the head start a 15.5yd
        // corridor swap needs.
        switch (WaveSideOf(tsunami))
        {
            case TsunamiWave::Left:
                if (tsunami->GetPositionX() <= bot->GetPositionX() + TSUNAMI_LETHAL_HALF_WIDTH)
                    return TsunamiWave::Left;
                break;
            case TsunamiWave::Right:
                if (tsunami->GetPositionX() >= bot->GetPositionX() - TSUNAMI_LETHAL_HALF_WIDTH)
                    return TsunamiWave::Right;
                break;
            default:
                break;
        }
    }
    return TsunamiWave::None;
}

bool WaveClearsY(float y, TsunamiWave wave)
{
    // The lethal half width plus the raid-wide arrival tolerance, so a hold that passes here is safe
    // across the whole disc a bot can idle on rather than only at the coordinate itself.
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

    // In the realm the boss is unresolvable, and the melee and tank splits below are all about his cones,
    // none of which exist in phase 16. Everyone shifted takes the raid pair.
    if (HasTwilightShift(bot))
        return CorridorGroup::Raid;

    // Healers and ranged never leave the raid corridor, whatever they are shooting at. Sartharion
    // parks 36.8yd from their home hold and 50.5yd from the left one, so he is out of their 28.5yd
    // spell range for the whole fight and they are on the drakes and the adds instead. Closing that
    // gap means walking in behind him, into a 30yd Tail Lash, which costs more than the uptime.
    if (PlayerbotAI::IsHeal(bot) || PlayerbotAI::IsRanged(bot))
        return CorridorGroup::Raid;

    Unit* boss = GetSartharion(bot);
    if (!boss)
        return CorridorGroup::Raid;

    // The main tank keeps his own pair for the whole fight, drakes up or not. His two holds are the
    // only spots on the platform south of where the drag parks Sartharion, and that is what keeps the
    // boss facing away from everyone else - walking him up to the raid line to shorten their range
    // would turn him around and put the line in a 60yd frontal cone.
    if (PlayerbotAI::IsMainTank(bot))
        return CorridorGroup::Tank;

    // Melee and the off-tank dodge in the corridor their own target is standing in. The off-tank is
    // locked off Sartharion for the whole fight, so he is always Raid.
    //
    // Melee on the boss get their own profile rather than the tank's, because his right hold is one
    // place they cannot follow him to: the platform is 11yd wide below Y 492, so the clamp puts them
    // 39.5 degrees off his facing - inside a 98-degree Flame Breath - with the healers 46yd away and
    // nothing able to heal it off. The 524.5-539.5 band is no better, everything in it inside his
    // 20.83yd melee range being inside a 30yd Tail Lash, so they give the right wave up instead.
    return bot->GetVictim() == boss ? CorridorGroup::Melee : CorridorGroup::Raid;
}

float SafeCorridorY(Player* bot)
{
    // Home and away, with no shared state behind it. Each group idles on the hold that is safe under
    // one wave pattern and steps out for the other, so the tank only ever moves for a right wave and
    // the raid only for a left one.
    //
    // Bots at different X release at slightly different times, since each reads the wave against its
    // own position and the raid band is 16yd wide - about 1.5s of spread. That is safe: the release
    // means the wave is already past that bot's X, and the walk home only changes Y.
    CorridorGroup const group = CorridorGroupFor(bot);
    TsunamiWave const wave = ClassifyTsunamiWave(bot);

    // Melee keep the tank's lane except under a right wave, where they take the raid's. It costs them
    // the ~11s a wave is in the air - 31yd from the boss is well outside melee range - and buys them
    // the only Y under a right wave that is neither in one of his cones nor out of heal range.
    if (group == CorridorGroup::Melee)
        return wave == TsunamiWave::Right ? RAID_CORRIDOR_RIGHT_Y : TANK_CORRIDOR_LEFT_Y;

    bool const tank = group == CorridorGroup::Tank;
    switch (wave)
    {
        case TsunamiWave::Left:
            return tank ? TANK_CORRIDOR_LEFT_Y : RAID_CORRIDOR_LEFT_Y;
        case TsunamiWave::Right:
            return tank ? TANK_CORRIDOR_RIGHT_Y : RAID_CORRIDOR_RIGHT_Y;
        default:
            return tank ? TANK_CORRIDOR_LEFT_Y : RAID_CORRIDOR_RIGHT_Y;
    }
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
    // Y first: both X edges are functions of it, so clamping X against an out-of-range Y would pick
    // the wrong band.
    y = std::clamp(y, PLATFORM_MIN_Y, PLATFORM_MAX_Y);
    x = std::clamp(x, PlatformMinX(y), PlatformMaxX(y));
}

void ClampToRangeMarker(float& x, float& y)
{
    // Y is the safety axis - it is what a tsunami dodge just bought - so the destination is pulled
    // along X only, onto the chord the marker circle cuts at this Y. The marker whose chord is wider
    // here wins; on the corridor holds that is always the northern trigger.
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
        // This Y is further from both markers than their radius, so no X on the line is covered.
        // Give up the Y and pull straight at the marker rather than leave the bot in Pyrobuffet.
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

    // Searched around the bot, so the radius has to reach a destination he is not standing on yet as
    // well as the fissure's own reach around it. FindUnitByEntries cannot answer this - it ranks by
    // distance to the bot rather than to the point.
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

        // The script raises flight rate to 3.0 in the same handler that issues
        // MovePoint(POINT_LANDING) and leaves the pre-call patrol at 1.0, so the speed rate splits
        // "coming down" from "circling the room" exactly. Distance on its own reports a 3s ETA
        // minutes early, every time a patrol waypoint happens to pass near a landing spot.
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
    // Sartharion calls them on a fixed schedule - Tenebron, Shadron, Vesperon - so reverse call order
    // is landing recency, and the newest is the one nobody has threat on yet. The drake he leaves still
    // has him top of its threat table, so it trails him onto the new spot rather than staying on its
    // own; the two overlap until the older one dies.
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

    // Lava Blazes spawn on a random player and whelps hatch mid-platform, so both are taken at taunt
    // range and never chased across the room.
    return FindUnitByEntries(bot, OFFTANK_PICKUP_ENTRIES, LAVA_BLAZE_TAUNT_RANGE);
}

Position OffTankAnchor(Player* bot)
{
    if (!bot)
        return Position();

    // Drakes and nothing else. Taunt reaches 30yd and a Lava Blaze or a whelp walks to him, so letting
    // one set the anchor only bought a 32yd round trip every time one spawned or died inside that
    // radius. With nothing down he waits on Tenebron's spot rather than beside the boss: it is the
    // first drake called, so he is standing where he will tank it 20s before it lands, and the spot is
    // 60yd from Sartharion, which is the other half of keeping him off the boss entirely.
    Unit* drake = NewestLandedDrake(bot);
    Position const* spot = drake ? DrakeTankSpotFor(drake->GetEntry()) : &TENEBRON_TANK_SPOT;

    // The spot gives way only to a wave that can actually reach it. Shadron's clears every right line
    // and Vesperon's every left one, and walking 28yd to a corridor that is no safer costs the trip
    // twice.
    float const y = WaveClearsY(spot->GetPositionY(), ClassifyTsunamiWave(bot)) ? spot->GetPositionY()
                                                                               : SafeCorridorY(bot);

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

    // Doubled because HasInArc takes the full arc: 140 degrees off the facing either way is the
    // 280-degree front arc, and behind is what is left of it.
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

    // The same primitives the spell engine picks cone targets with, so this cannot drift from the
    // mechanic: HasInArc takes the full arc, and a rear cone is its complement (Unit::isInBack).
    if (dist <= FLAME_BREATH_RANGE && boss->HasInArc(arc, &point))
        return true;

    return dist <= TAIL_LASH_RANGE && !boss->HasInArc(2.0f * static_cast<float>(M_PI) - arc, &point);
}

}
