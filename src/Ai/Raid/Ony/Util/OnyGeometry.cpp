/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OnyGeometry.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Map.h"
#include "Player.h"

namespace OnyHelpers
{
namespace
{
constexpr float DEG_TO_RAD = static_cast<float>(M_PI) / 180.0f;

// Spacing of the circles a lane is drawn with. At 4yd a clearance of 22 still covers 21.9yd of the
// lane's half width between two of them.
constexpr float LANE_HAZARD_STEP = 4.0f;

// Where the floor height search starts: above the main floor and the egg pit, below her hover.
constexpr float FLOOR_PROBE_Z = -70.0f;
constexpr float LOS_EYE_HEIGHT = 2.0f;

struct AirCandidate
{
    Position point;
    bool laneClear;
    float toHover;
};
}  // namespace

float DistanceToSegment2d(float x, float y, Position const& from, Position const& to)
{
    float const dx = to.GetPositionX() - from.GetPositionX();
    float const dy = to.GetPositionY() - from.GetPositionY();
    float const lengthSq = dx * dx + dy * dy;
    float t = 0.0f;
    if (lengthSq > 0.0f)
        t = std::clamp(((x - from.GetPositionX()) * dx + (y - from.GetPositionY()) * dy) / lengthSq, 0.0f, 1.0f);

    float const px = from.GetPositionX() + t * dx;
    float const py = from.GetPositionY() + t * dy;
    return std::hypot(x - px, y - py);
}

BreathLane const* BreathLaneForSpell(uint32 spellId)
{
    for (BreathLane const& lane : BREATH_LANES)
        if (lane.spellId == spellId)
            return &lane;

    return nullptr;
}

BreathLane const& BreathLaneForWaypoint(uint8 waypoint)
{
    uint8 const index = std::clamp<uint8>(waypoint, 1, static_cast<uint8>(BREATH_LANES.size())) - 1;
    return BREATH_LANES[index];
}

uint8 NearestWaypoint(float x, float y)
{
    uint8 nearest = ONYXIA_FIRST_HOVER_WAYPOINT;
    float best = std::numeric_limits<float>::max();
    for (BreathLane const& lane : BREATH_LANES)
    {
        float const distance = lane.hover.GetExactDist2d(x, y);
        if (distance < best)
        {
            best = distance;
            nearest = lane.waypoint;
        }
    }

    return nearest;
}

void AppendLaneHazards(BreathLane const& lane, float clearance, std::vector<EncounterHelpers::HazardCircle>& out)
{
    float const length = lane.from.GetExactDist2d(&lane.to);
    uint32 const steps = std::max<uint32>(1, static_cast<uint32>(std::ceil(length / LANE_HAZARD_STEP)));
    for (uint32 i = 0; i <= steps; ++i)
    {
        float const t = static_cast<float>(i) / steps;
        float const x = lane.from.GetPositionX() + t * (lane.to.GetPositionX() - lane.from.GetPositionX());
        float const y = lane.from.GetPositionY() + t * (lane.to.GetPositionY() - lane.from.GetPositionY());
        out.emplace_back(Position(x, y, lane.from.GetPositionZ()), clearance);
    }
}

float NearestFissureDistance(float x, float y)
{
    float best = std::numeric_limits<float>::max();
    for (Position const& fissure : LAVA_FISSURES)
        best = std::min(best, fissure.GetExactDist2d(x, y));

    return best;
}

bool IsFireballSpell(uint32 spellId) { return spellId == SpellId::Fireball || spellId == SpellId::FireballH; }

bool IsFlameBreathSpell(uint32 spellId)
{
    return spellId == SpellId::FlameBreath || spellId == SpellId::FlameBreathH;
}

float BearingOffFacingDeg(Position const& origin, float facing, float x, float y)
{
    float angle = std::atan2(y - origin.GetPositionY(), x - origin.GetPositionX()) - facing;
    while (angle > static_cast<float>(M_PI))
        angle -= 2.0f * static_cast<float>(M_PI);
    while (angle < -static_cast<float>(M_PI))
        angle += 2.0f * static_cast<float>(M_PI);

    return angle / DEG_TO_RAD;
}

float GroundSlotRadius(uint8 slot)
{
    uint8 const ring = slot / ONYXIA_SLOTS_PER_RING;
    switch (ring)
    {
        case 0:
            return ONYXIA_SLOT_RADIUS_HEAL;
        case 1:
            return ONYXIA_SLOT_RADIUS_CASTER;
        case 2:
            return ONYXIA_SLOT_RADIUS_HUNTER;
        default:
            return ONYXIA_SLOT_RADIUS_OVERFLOW + 6.0f * (ring - 3);
    }
}

float GroundSlotBearingDeg(uint8 slot)
{
    uint8 const k = slot % ONYXIA_SLOTS_PER_RING;
    float const magnitude = ONYXIA_SLOT_BEARING_FIRST + ONYXIA_SLOT_BEARING_STEP * (k / 2);
    return k % 2 == 0 ? magnitude : -magnitude;
}

Position GroundSlotPoint(Position const& anchor, float facing, uint8 slot, float radiusDelta, float bearingDeltaDeg)
{
    float const bearing = GroundSlotBearingDeg(slot);
    float const signedDelta = bearing >= 0.0f ? bearingDeltaDeg : -bearingDeltaDeg;
    float const angle = facing + (bearing + signedDelta) * DEG_TO_RAD;
    float const radius = GroundSlotRadius(slot) + radiusDelta;
    return Position(anchor.GetPositionX() + radius * std::cos(angle), anchor.GetPositionY() + radius * std::sin(angle),
                    anchor.GetPositionZ());
}

std::vector<Position> BuildAirCells(Player* bot, BreathLane const& lane, std::vector<Position> const& eggs)
{
    std::vector<Position> cells;
    if (!bot || !bot->GetMap())
        return cells;

    float const dx = lane.to.GetPositionX() - lane.from.GetPositionX();
    float const dy = lane.to.GetPositionY() - lane.from.GetPositionY();
    float const length = std::hypot(dx, dy);
    if (length <= 0.0f)
        return cells;

    float const ux = dx / length;
    float const uy = dy / length;
    Map* map = bot->GetMap();

    std::vector<AirCandidate> candidates;
    for (float along = 0.0f; along <= length + 0.01f; along += ONYXIA_AIR_STEP_ALONG)
    {
        for (float offset : ONYXIA_AIR_OFFSETS)
        {
            for (float side : {1.0f, -1.0f})
            {
                float const x = lane.from.GetPositionX() + ux * along - uy * offset * side;
                float const y = lane.from.GetPositionY() + uy * along + ux * offset * side;
                float const z = bot->GetMapWaterOrGroundLevel(x, y, FLOOR_PROBE_Z);
                if (z < ONYXIA_AIR_FLOOR_MIN_Z || z > ONYXIA_AIR_FLOOR_MAX_Z)
                    continue;

                if (ONYXIA_GUARD_SPAWN.GetExactDist2d(x, y) < ONYXIA_AIR_GUARD_CLEARANCE)
                    continue;

                bool const nearEgg = std::any_of(eggs.begin(), eggs.end(), [&](Position const& egg)
                                                 { return egg.GetExactDist2d(x, y) < ONYXIA_EGG_CLEARANCE; });
                if (nearEgg)
                    continue;

                float const toHover = lane.hover.GetExactDist(x, y, z);
                if (toHover > ONYXIA_AIR_SPELL_RANGE)
                    continue;

                if (!map->isInLineOfSight(x, y, z + LOS_EYE_HEIGHT, lane.hover.GetPositionX(), lane.hover.GetPositionY(),
                                          lane.hover.GetPositionZ(), bot->GetPhaseMask(), LINEOFSIGHT_CHECK_VMAP,
                                          VMAP::ModelIgnoreFlags::Nothing))
                    continue;

                candidates.push_back({Position(x, y, z), offset > ONYXIA_BREATH_CLEARANCE, toHover});
            }
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](AirCandidate const& a, AirCandidate const& b)
              { return a.laneClear != b.laneClear ? a.laneClear : a.toHover < b.toHover; });

    for (AirCandidate const& candidate : candidates)
    {
        bool const crowded = std::any_of(cells.begin(), cells.end(), [&](Position const& cell)
                                         { return cell.GetExactDist2d(&candidate.point) < ONYXIA_FIREBALL_SPACING; });
        if (!crowded)
            cells.push_back(candidate.point);
    }

    return cells;
}
}  // namespace OnyHelpers
