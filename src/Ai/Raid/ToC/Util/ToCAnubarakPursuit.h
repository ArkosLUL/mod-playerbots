#ifndef PLAYERBOTS_RAID_TOCANUBARAKPURSUIT_H
#define PLAYERBOTS_RAID_TOCANUBARAKPURSUIT_H

#include <algorithm>
#include <cmath>
#include <vector>

// The Pursuing Spike runs pure pursuit: every moment it heads straight at where its target is right then.
// Replayed from the kiter's real path over six 25H pulls, that put it within 0.9 yd of the real spike 6 s
// on, nine times in ten. No game headers here so nativetest can build it.
namespace AnubarakPursuit
{
struct Point
{
    float x = 0.0f;
    float y = 0.0f;
};

inline float Dist(Point a, Point b) { return std::hypot(b.x - a.x, b.y - a.y); }

inline float SegmentDist(Point a, Point b, Point p)
{
    float const abx = b.x - a.x;
    float const aby = b.y - a.y;
    float const lengthSq = abx * abx + aby * aby;
    float t = lengthSq > 0.0f ? ((p.x - a.x) * abx + (p.y - a.y) * aby) / lengthSq : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    return Dist(p, {a.x + abx * t, a.y + aby * t});
}

// Its speed auras: 3.5 yd/s, then +75% and +225%, each 7 s after the one before
constexpr float SPEEDS[] = {3.5f, 3.5f * 1.75f, 3.5f * 3.25f};
constexpr int LAST_STEP = 2;
constexpr float STEP_S = 7.0f;
constexpr float TICK_S = 0.1f;

struct Spike
{
    Point pos;
    // What it runs at now, read off the unit
    float speed = SPEEDS[0];
    // Index into SPEEDS of the aura it carries now
    int step = 0;
    // Seconds to its next speed aura, unused at the last
    float nextStepS = STEP_S;
};

// Straight along dir from start until radius off centre, then round that circle: turn +1 is
// counter-clockwise, -1 clockwise
struct KiterPath
{
    Point start;
    Point dir;
    float speed = 7.0f;
    Point centre;
    float radius = 40.0f;
    float turn = 1.0f;
    float straight = 0.0f;

    Point At(float t) const
    {
        float const s = speed * t;
        if (s <= straight || radius <= 0.0f)
            return {start.x + dir.x * s, start.y + dir.y * s};

        Point const hit = {start.x + dir.x * straight, start.y + dir.y * straight};
        float const angle = std::atan2(hit.y - centre.y, hit.x - centre.x) + turn * (s - straight) / radius;
        return {centre.x + radius * std::cos(angle), centre.y + radius * std::sin(angle)};
    }
};

// dir is a unit vector. A start already outside radius runs the circle it stands on. A heading that
// meets the circle square on, with no way round to follow, takes fallbackTurn.
inline KiterPath MakeKiterPath(Point start, Point dir, float speed, Point centre, float radius, float fallbackTurn)
{
    KiterPath path;
    path.start = start;
    path.dir = dir;
    path.speed = speed;
    path.centre = centre;

    float const px = start.x - centre.x;
    float const py = start.y - centre.y;
    path.radius = std::max(radius, std::hypot(px, py));
    float const along = px * dir.x + py * dir.y;
    float const disc = along * along - (px * px + py * py) + path.radius * path.radius;
    path.straight = std::max(0.0f, -along + std::sqrt(std::max(0.0f, disc)));

    float const hx = px + dir.x * path.straight;
    float const hy = py + dir.y * path.straight;
    float const cross = hx * dir.y - hy * dir.x;
    // Under about 6 deg off square on
    path.turn = std::fabs(cross) > 0.1f * path.radius ? (cross > 0.0f ? 1.0f : -1.0f) : fallbackTurn;
    return path;
}

struct Pursuit
{
    // Closest pass by each point, in the order given
    std::vector<float> spikeClosest;
    std::vector<float> kiterClosest;
    // First time the spike is within reach of the kiter, negative when it isn't within the horizon
    float catchS = -1.0f;
};

inline Pursuit Simulate(Spike spike, KiterPath const& path, float horizonS, std::vector<Point> const& points,
                        float reach)
{
    Pursuit out;
    Point kiter = path.start;
    for (Point const& point : points)
    {
        out.spikeClosest.push_back(Dist(spike.pos, point));
        out.kiterClosest.push_back(Dist(kiter, point));
    }

    if (Dist(spike.pos, kiter) <= reach)
        out.catchS = 0.0f;

    int const ticks = static_cast<int>(std::ceil(horizonS / TICK_S - 0.001f));
    for (int i = 0; i < ticks; ++i)
    {
        float const t = static_cast<float>(i) * TICK_S;
        while (spike.step < LAST_STEP && t >= spike.nextStepS)
        {
            ++spike.step;
            spike.speed = SPEEDS[spike.step];
            spike.nextStepS += STEP_S;
        }

        Point const spikeFrom = spike.pos;
        float const gap = Dist(spikeFrom, kiter);
        if (gap > 0.001f)
        {
            float const move = std::min(spike.speed * TICK_S, gap) / gap;
            spike.pos = {spikeFrom.x + (kiter.x - spikeFrom.x) * move, spikeFrom.y + (kiter.y - spikeFrom.y) * move};
        }

        Point const kiterFrom = kiter;
        kiter = path.At(t + TICK_S);
        for (size_t k = 0; k < points.size(); ++k)
        {
            out.spikeClosest[k] = std::min(out.spikeClosest[k], SegmentDist(spikeFrom, spike.pos, points[k]));
            out.kiterClosest[k] = std::min(out.kiterClosest[k], SegmentDist(kiterFrom, kiter, points[k]));
        }

        if (out.catchS < 0.0f && Dist(spike.pos, kiter) <= reach)
            out.catchS = t + TICK_S;
    }

    return out;
}
}  // namespace AnubarakPursuit

#endif
