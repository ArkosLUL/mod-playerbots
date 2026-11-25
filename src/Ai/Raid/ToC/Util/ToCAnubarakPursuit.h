#ifndef PLAYERBOTS_RAID_TOCANUBARAKPURSUIT_H
#define PLAYERBOTS_RAID_TOCANUBARAKPURSUIT_H

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

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

inline constexpr float SPEEDS[] = {3.5f, 3.5f * 1.75f, 3.5f * 3.25f};
inline constexpr int LAST_STEP = 2;
inline constexpr float STEP_S = 7.0f;
inline constexpr float TICK_S = 0.1f;

inline constexpr float DEG_TO_RAD = 0.0174532925f;
inline constexpr float TWO_PI = 6.28318531f;

struct Spike
{
    Point pos;
    float speed = SPEEDS[0];
    int step = 0;
    float nextStepS = STEP_S;
};

struct Pace
{
    float speed = 7.0f;
    float boostSpeed = 0.0f;
    float boostS = 0.0f;
    float boostFromS = 0.0f;

    Pace() = default;
    Pace(float runSpeed) : speed(runSpeed) {}
    Pace(float runSpeed, float boost, float boostSeconds, float fromS = 0.0f)
        : speed(runSpeed), boostSpeed(boost), boostS(boostSeconds), boostFromS(fromS)
    {
    }

    float Length(float t) const
    {
        float const boosted = std::clamp(t - boostFromS, 0.0f, std::max(boostS, 0.0f));
        return boostSpeed * boosted + speed * (t - boosted);
    }

    float SpeedAt(float t) const { return t >= boostFromS && t < boostFromS + boostS ? boostSpeed : speed; }

    Pace After(float t) const
    {
        if (boostFromS >= t)
            return Pace(speed, boostSpeed, boostS, boostFromS - t);

        return Pace(speed, boostSpeed, std::max(boostS - (t - boostFromS), 0.0f));
    }
};

struct KiterPath
{
    Point start;
    Point dir;
    Pace pace;
    Point centre;
    float radius = 40.0f;
    float turn = 1.0f;
    float straight = 0.0f;

    Point At(float t) const
    {
        float const s = pace.Length(t);
        if (s <= straight || radius <= 0.0f)
            return {start.x + dir.x * s, start.y + dir.y * s};

        Point const hit = {start.x + dir.x * straight, start.y + dir.y * straight};
        float const angle = std::atan2(hit.y - centre.y, hit.x - centre.x) + turn * (s - straight) / radius;
        return {centre.x + radius * std::cos(angle), centre.y + radius * std::sin(angle)};
    }
};

inline KiterPath MakeKiterPath(Point start, Point dir, Pace pace, Point centre, float radius, float fallbackTurn)
{
    KiterPath path;
    path.start = start;
    path.dir = dir;
    path.pace = pace;
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
    path.turn = std::fabs(cross) > 0.1f * path.radius ? (cross > 0.0f ? 1.0f : -1.0f) : fallbackTurn;
    return path;
}

struct PathMember
{
    float radius = 40.0f;
    float turn = 1.0f;
    float straightCap = 1e9f;
};

inline KiterPath MakeKiterPath(Point start, Point dir, Pace pace, Point centre, PathMember const& member,
                               float walkFirst = 0.0f)
{
    KiterPath path = MakeKiterPath(start, dir, pace, centre, member.radius, member.turn);
    path.turn = member.turn;
    float const straight = std::max({0.0f, std::min(member.straightCap, path.straight), walkFirst});
    if (straight != path.straight)
    {
        path.straight = straight;
        path.radius = Dist({start.x + dir.x * straight, start.y + dir.y * straight}, centre);
    }

    return path;
}

struct LegPath
{
    Point start;
    std::vector<Point> legs;
    Pace pace;
    std::vector<Point> track;

    void Slow(Point centre, float radius, float factor, float horizonS)
    {
        track.assign(1, start);
        if ((pace.speed <= 0.0f && pace.boostS <= 0.0f) || legs.empty())
            return;

        Point at = start;
        size_t leg = 0;
        int const ticks = static_cast<int>(std::ceil(horizonS / TICK_S));
        for (int i = 0; i < ticks && leg < legs.size(); ++i)
        {
            float left = pace.SpeedAt(static_cast<float>(i) * TICK_S) * TICK_S *
                         (Dist(at, centre) < radius ? factor : 1.0f);
            while (leg < legs.size() && left > 0.0f)
            {
                float const length = Dist(at, legs[leg]);
                if (left < length)
                {
                    at = {at.x + (legs[leg].x - at.x) * left / length, at.y + (legs[leg].y - at.y) * left / length};
                    break;
                }

                left -= length;
                at = legs[leg++];
            }

            track.push_back(at);
        }
    }

    Point At(float t) const
    {
        if (!track.empty())
        {
            float const tick = std::max(t, 0.0f) / TICK_S;
            size_t const i = static_cast<size_t>(tick);
            if (i + 1 >= track.size())
                return track.back();

            float const f = tick - static_cast<float>(i);
            return {track[i].x + (track[i + 1].x - track[i].x) * f, track[i].y + (track[i + 1].y - track[i].y) * f};
        }

        float left = pace.Length(t);
        Point at = start;
        for (Point const& next : legs)
        {
            float const length = Dist(at, next);
            if (left < length)
                return {at.x + (next.x - at.x) * left / length, at.y + (next.y - at.y) * left / length};

            left -= length;
            at = next;
        }

        return at;
    }
};

struct Pursuit
{
    std::vector<float> spikeClosest;
    std::vector<float> kiterClosest;
    std::vector<float> spikeArriveS;
    float catchS = -1.0f;
};

inline void ChaseTick(Spike& spike, Point kiter, float t)
{
    while (spike.step < LAST_STEP && t >= spike.nextStepS)
    {
        ++spike.step;
        spike.speed = SPEEDS[spike.step];
        spike.nextStepS += STEP_S;
    }

    Point const from = spike.pos;
    float const gap = Dist(from, kiter);
    if (gap > 0.001f)
    {
        float const move = std::min(spike.speed * TICK_S, gap) / gap;
        spike.pos = {from.x + (kiter.x - from.x) * move, from.y + (kiter.y - from.y) * move};
    }
}

struct CatchRule
{
    float fromS = 0.0f;
    Point safe;
    float safeRadius = 0.0f;

    bool Counts(float t, Point kiter) const
    {
        return t >= fromS - 0.001f && (safeRadius <= 0.0f || Dist(kiter, safe) > safeRadius);
    }
};

template <typename Path>
Pursuit Simulate(Spike spike, Path const& path, float horizonS, std::vector<Point> const& points, float reach,
                 std::vector<float> const& arrives, CatchRule const& rule = CatchRule())
{
    Pursuit out;
    Point kiter = path.start;
    for (size_t k = 0; k < points.size(); ++k)
    {
        out.spikeClosest.push_back(Dist(spike.pos, points[k]));
        out.kiterClosest.push_back(Dist(kiter, points[k]));
        out.spikeArriveS.push_back(arrives[k] > 0.0f && Dist(spike.pos, points[k]) <= arrives[k] ? 0.0f : -1.0f);
    }

    if (Dist(spike.pos, kiter) <= reach && rule.Counts(0.0f, kiter))
        out.catchS = 0.0f;

    int const ticks = static_cast<int>(std::ceil(horizonS / TICK_S - 0.001f));
    for (int i = 0; i < ticks; ++i)
    {
        float const t = static_cast<float>(i) * TICK_S;
        Point const spikeFrom = spike.pos;
        ChaseTick(spike, kiter, t);

        Point const kiterFrom = kiter;
        kiter = path.At(t + TICK_S);
        for (size_t k = 0; k < points.size(); ++k)
        {
            float const pass = SegmentDist(spikeFrom, spike.pos, points[k]);
            out.spikeClosest[k] = std::min(out.spikeClosest[k], pass);
            out.kiterClosest[k] = std::min(out.kiterClosest[k], SegmentDist(kiterFrom, kiter, points[k]));
            if (arrives[k] > 0.0f && out.spikeArriveS[k] < 0.0f && pass <= arrives[k])
                out.spikeArriveS[k] = t + TICK_S;
        }

        if (out.catchS < 0.0f && Dist(spike.pos, kiter) <= reach && rule.Counts(t + TICK_S, kiter))
            out.catchS = t + TICK_S;
    }

    return out;
}

template <typename Path>
Pursuit Simulate(Spike spike, Path const& path, float horizonS, std::vector<Point> const& points, float reach,
                 float arrive = 0.0f)
{
    return Simulate(spike, path, horizonS, points, reach, std::vector<float>(points.size(), arrive));
}

template <typename Path>
float PatchMargin(Spike const& spike, Path const& walk, Point patch, float patchReach, float reach, float immuneS,
                  float horizonS, float leadS)
{
    CatchRule rule;
    rule.fromS = immuneS;
    rule.safe = patch;
    rule.safeRadius = patchReach - reach;
    Pursuit const run = Simulate(spike, walk, horizonS, {patch}, reach, std::vector<float>{patchReach}, rule);
    float const arrive = run.spikeArriveS[0];
    if (arrive < 0.0f)
        return run.catchS < 0.0f ? horizonS : -1.0f;

    return (run.catchS < 0.0f ? horizonS : run.catchS) - arrive - leadS;
}

enum class PatchMove : int
{
    In,
    EdgeLeft,
    EdgeRight
};

struct PatchRules
{
    float patchReach = 11.5f;
    float impaleReach = 7.5f;
    float slowReach = 7.5f;
    float slowFactor = 1.0f;
    float edgeRadius = 8.5f;
    float edgeRunOn = 15.0f;
    float horizonS = 20.0f;
    float leadS = 1.0f;
    float catchFromS = 0.0f;
};

struct PatchWalk
{
    bool valid = false;
    PatchMove move = PatchMove::In;
    std::vector<Point> legs;
    float marginS = -1.0f;
};

inline bool PatchMoveLegs(PatchMove move, Point from, Point patch, PatchRules const& rules, std::vector<Point>& legs)
{
    legs.clear();
    if (move == PatchMove::In)
    {
        legs.push_back(patch);
        return true;
    }

    float const d = Dist(from, patch);
    if (d <= rules.edgeRadius + 0.01f)
        return false;

    float const base = std::atan2(from.y - patch.y, from.x - patch.x);
    float const turn = std::acos(rules.edgeRadius / d) * (move == PatchMove::EdgeLeft ? 1.0f : -1.0f);
    Point const tangent = {patch.x + rules.edgeRadius * std::cos(base + turn),
                           patch.y + rules.edgeRadius * std::sin(base + turn)};
    float const along = Dist(from, tangent);
    legs.push_back(tangent);
    legs.push_back({tangent.x + (tangent.x - from.x) / along * rules.edgeRunOn,
                    tangent.y + (tangent.y - from.y) / along * rules.edgeRunOn});
    return true;
}

inline float PatchWalkMargin(Spike const& spike, Point from, Pace pace, Point patch, std::vector<Point> const& legs,
                             PatchRules const& rules, float immuneS)
{
    LegPath walk;
    walk.start = from;
    walk.legs = legs;
    walk.pace = pace;
    if (rules.slowFactor < 1.0f)
        walk.Slow(patch, rules.slowReach, rules.slowFactor, rules.horizonS);

    return PatchMargin(spike, walk, patch, rules.patchReach, rules.impaleReach, std::max(immuneS, rules.catchFromS),
                       rules.horizonS, rules.leadS);
}

template <typename Allow>
PatchWalk BestPatchWalk(Spike const& spike, Point from, Pace pace, Point patch, PatchRules const& rules,
                        float immuneS, Allow const& allow)
{
    PatchWalk best;
    for (PatchMove move : {PatchMove::In, PatchMove::EdgeLeft, PatchMove::EdgeRight})
    {
        PatchWalk walk;
        walk.move = move;
        if ((immuneS > 0.0f && move != PatchMove::In) || !PatchMoveLegs(move, from, patch, rules, walk.legs) ||
            !allow(move, walk.legs))
            continue;

        walk.valid = true;
        walk.marginS = PatchWalkMargin(spike, from, pace, patch, walk.legs, rules, immuneS);
        if (!best.valid || walk.marginS > best.marginS)
            best = walk;
    }

    return best;
}

inline PatchWalk BestPatchWalk(Spike const& spike, Point from, Pace pace, Point patch, PatchRules const& rules,
                               float immuneS)
{
    return BestPatchWalk(spike, from, pace, patch, rules, immuneS,
                         [](PatchMove, std::vector<Point> const&) { return true; });
}

inline std::array<Point, 2> PairSpots(Point north, Point south, float maxApart, Point centre, float floorRadius)
{
    std::array<Point, 2> spots = {north, south};
    float const apart = Dist(north, south);
    if (apart > maxApart)
    {
        Point const toSouth = {(south.x - north.x) / apart, (south.y - north.y) / apart};
        float const pull = (apart - maxApart) / 2.0f;
        spots[0] = {north.x + toSouth.x * pull, north.y + toSouth.y * pull};
        spots[1] = {south.x - toSouth.x * pull, south.y - toSouth.y * pull};
    }

    for (Point& spot : spots)
    {
        float const radius = Dist(centre, spot);
        if (radius > floorRadius)
            spot = {centre.x + (spot.x - centre.x) * floorRadius / radius,
                    centre.y + (spot.y - centre.y) * floorRadius / radius};
    }

    return spots;
}

inline float TeleportBuyS(float circleDist, float range, float reach, float spikeSpeed, float kiterSpeed)
{
    if (circleDist > range || circleDist <= reach)
        return 0.0f;

    float const closing = std::max(spikeSpeed - kiterSpeed, 1.0f);
    return (circleDist - reach) / closing;
}

inline float BoostLeadS(float gap, float boostSpeed, float spikeSpeed, float reach, float margin, float capS)
{
    float const need = reach + margin - gap;
    if (need <= 0.0f)
        return 0.0f;

    float const gain = boostSpeed - spikeSpeed;
    return gain > 0.0f ? std::min(need / gain, capS) : capS;
}

template <typename Path>
std::vector<Point> SpikeTrack(Spike spike, Path const& path, float horizonS, int everyTicks)
{
    std::vector<Point> track = {spike.pos};
    Point kiter = path.start;
    int const ticks = static_cast<int>(std::ceil(horizonS / TICK_S - 0.001f));
    for (int i = 0; i < ticks; ++i)
    {
        float const t = static_cast<float>(i) * TICK_S;
        ChaseTick(spike, kiter, t);
        kiter = path.At(t + TICK_S);
        if ((i + 1) % everyTicks == 0 || i + 1 == ticks)
            track.push_back(spike.pos);
    }

    return track;
}

inline float Cross(Point a, Point b, Point p) { return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x); }

inline float SegmentGap(Point a, Point b, Point c, Point d)
{
    float const c1 = Cross(a, b, c);
    float const c2 = Cross(a, b, d);
    float const c3 = Cross(c, d, a);
    float const c4 = Cross(c, d, b);
    bool const straddlesAb = (c1 > 0.0f && c2 < 0.0f) || (c1 < 0.0f && c2 > 0.0f);
    bool const straddlesCd = (c3 > 0.0f && c4 < 0.0f) || (c3 < 0.0f && c4 > 0.0f);
    if (straddlesAb && straddlesCd)
        return 0.0f;

    return std::min({SegmentDist(a, b, c), SegmentDist(a, b, d), SegmentDist(c, d, a), SegmentDist(c, d, b)});
}

inline bool TrackClear(std::vector<Point> const& track, Point point, float clearance)
{
    if (track.size() == 1)
        return Dist(track.front(), point) >= clearance;

    for (size_t k = 1; k < track.size(); ++k)
        if (SegmentDist(track[k - 1], track[k], point) < clearance)
            return false;

    return true;
}

inline bool TrackClear(std::vector<Point> const& track, Point from, Point to, float clearance)
{
    if (track.size() == 1)
        return SegmentDist(from, to, track.front()) >= clearance;

    for (size_t k = 1; k < track.size(); ++k)
        if (SegmentGap(track[k - 1], track[k], from, to) < clearance)
            return false;

    return true;
}

inline std::vector<Point> MotionTrack(Point pos, Point velocity, float horizonS, float stepS)
{
    std::vector<Point> track = {pos};
    if (stepS <= 0.0f || (velocity.x == 0.0f && velocity.y == 0.0f))
        return track;

    int const steps = static_cast<int>(std::ceil(horizonS / stepS - 0.001f));
    for (int i = 1; i <= steps; ++i)
    {
        float const t = std::min(static_cast<float>(i) * stepS, horizonS);
        track.push_back({pos.x + velocity.x * t, pos.y + velocity.y * t});
    }

    return track;
}

struct EscortRules
{
    std::vector<float> rings = {15.0f, 20.0f, 25.0f};
    int bearings = 12;
    Point centre;
    float centreReach = 45.0f;
    float patchClearance = 8.5f;
    float trackClearance = 15.0f;
    float laneClearance = 9.0f;
};

inline std::vector<Point> EscortPosts(Point catchPoint, Point spike, Point kiter, Point paladin,
                                      std::vector<Point> const& track, std::vector<Point> const& patches,
                                      EscortRules const& rules)
{
    float const side = Cross(spike, kiter, paladin);
    std::vector<Point> out;
    for (float radius : rules.rings)
        for (int i = 0; i < rules.bearings; ++i)
        {
            float const angle = TWO_PI * static_cast<float>(i) / static_cast<float>(rules.bearings);
            Point const post = {catchPoint.x + radius * std::cos(angle), catchPoint.y + radius * std::sin(angle)};
            if (Dist(post, rules.centre) > rules.centreReach || side * Cross(spike, kiter, post) < 0.0f ||
                SegmentDist(spike, catchPoint, post) < rules.laneClearance)
                continue;

            if (std::any_of(patches.begin(), patches.end(),
                            [&](Point const& patch) { return Dist(post, patch) < rules.patchClearance; }))
                continue;

            bool clear = track.size() != 1 || Dist(track.front(), post) >= rules.trackClearance;
            for (size_t k = 1; clear && k < track.size(); ++k)
                clear = SegmentDist(track[k - 1], track[k], post) >= rules.trackClearance;

            if (clear)
                out.push_back(post);
        }

    std::sort(out.begin(), out.end(),
              [paladin](Point const& a, Point const& b) { return Dist(paladin, a) < Dist(paladin, b); });
    return out;
}

inline std::vector<Point> DragDetours(Point boss, Point anchor, std::vector<Point> const& patches, float trigger,
                                      float standOff, float keepout)
{
    std::vector<Point> out;
    float const length = Dist(boss, anchor);
    if (length < 1.0f)
        return out;

    Point const normal = {-(anchor.y - boss.y) / length, (anchor.x - boss.x) / length};
    for (Point const& patch : patches)
    {
        if (SegmentDist(boss, anchor, patch) >= trigger)
            continue;

        for (float side : {standOff, -standOff})
        {
            Point const point = {patch.x + normal.x * side, patch.y + normal.y * side};
            if (std::none_of(patches.begin(), patches.end(),
                             [&point, keepout](Point const& other) { return Dist(point, other) < keepout; }))
                out.push_back(point);
        }
    }

    std::sort(out.begin(), out.end(), [boss](Point const& a, Point const& b) { return Dist(boss, a) < Dist(boss, b); });
    return out;
}

struct HoldDirs
{
    Point a;
    Point b;
    bool turned = false;
};

inline HoldDirs SideHoldDirs(Point a, Point b, Point dirA, Point dirB, Point boss, float inset, float apart,
                             float stepDegrees = 15.0f)
{
    auto const burrowersApart = [&](Point da, Point db)
    {
        return Dist({a.x + da.x * inset, a.y + da.y * inset}, {b.x + db.x * inset, b.y + db.y * inset}) >= apart;
    };

    HoldDirs out = {dirA, dirB, false};
    float const length = Dist(a, b);
    if (length < 0.1f || burrowersApart(dirA, dirB))
        return out;

    Point const along = {(b.x - a.x) / length, (b.y - a.y) / length};
    Point normal = {-along.y, along.x};
    if (Cross(a, b, boss) < 0.0f)
        normal = {-normal.x, -normal.y};

    out.turned = true;
    for (float degrees = stepDegrees; degrees <= 180.0f + 0.01f; degrees += stepDegrees)
    {
        float const c = std::cos(degrees * DEG_TO_RAD);
        float const s = std::sin(degrees * DEG_TO_RAD);
        out.a = {along.x * c + normal.x * s, along.y * c + normal.y * s};
        out.b = {-along.x * c + normal.x * s, -along.y * c + normal.y * s};
        if (burrowersApart(out.a, out.b))
            return out;
    }

    return out;
}

inline constexpr int DRAG_PRE_STEP_BEARINGS = 16;
inline constexpr float DRAG_PRE_STEP_RAD = TWO_PI / static_cast<float>(DRAG_PRE_STEP_BEARINGS);

inline std::vector<Point> DragPreSteps(Point boss, Point tank, Point legSpot, std::vector<Point> const& patches,
                                       std::vector<float> const& radii, float keepout, float slow)
{
    auto const clear = [&patches, keepout, slow](Point from, Point to)
    {
        return std::none_of(patches.begin(), patches.end(), [&](Point const& patch)
                            { return Dist(to, patch) < keepout || SegmentDist(from, to, patch) < slow; });
    };

    std::vector<Point> out;
    for (float radius : radii)
        for (int i = 0; i < DRAG_PRE_STEP_BEARINGS; ++i)
        {
            float const angle = static_cast<float>(i) * DRAG_PRE_STEP_RAD;
            Point const point = {boss.x + radius * std::cos(angle), boss.y + radius * std::sin(angle)};
            if (clear(tank, point) && clear(point, legSpot))
                out.push_back(point);
        }

    std::sort(out.begin(), out.end(), [tank](Point const& a, Point const& b) { return Dist(tank, a) < Dist(tank, b); });
    return out;
}

inline constexpr float RELAY_POST_TURNS_DEG[] = {0.0f, 20.0f, -20.0f, 40.0f, -40.0f};

inline std::vector<Point> RelayPosts(Point landing, Point spot, float reach)
{
    float const length = Dist(landing, spot);
    if (length <= reach)
        return {spot};

    Point const dir = {(spot.x - landing.x) / length, (spot.y - landing.y) / length};
    std::vector<Point> out;
    for (float degrees : RELAY_POST_TURNS_DEG)
    {
        float const angle = degrees * DEG_TO_RAD;
        float const c = std::cos(angle);
        float const s = std::sin(angle);
        out.push_back({landing.x + (dir.x * c - dir.y * s) * reach, landing.y + (dir.x * s + dir.y * c) * reach});
    }

    return out;
}

inline constexpr int RING_POINT_BEARINGS = 8;
inline constexpr float RING_POINT_RAD = TWO_PI / static_cast<float>(RING_POINT_BEARINGS);

inline std::vector<Point> RingPoints(Point centre, float radius, Point from, std::vector<Point> const& patches,
                                     float keepout)
{
    std::vector<Point> out;
    for (int i = 0; i < RING_POINT_BEARINGS; ++i)
    {
        float const angle = static_cast<float>(i) * RING_POINT_RAD;
        Point const point = {centre.x + radius * std::cos(angle), centre.y + radius * std::sin(angle)};
        if (std::none_of(patches.begin(), patches.end(),
                         [&point, keepout](Point const& patch) { return Dist(point, patch) < keepout; }))
            out.push_back(point);
    }

    std::sort(out.begin(), out.end(), [from](Point const& a, Point const& b) { return Dist(from, a) < Dist(from, b); });
    return out;
}

inline bool BackUpKeepsFront(Point from, Point to, std::vector<Point> const& units)
{
    Point const walk = {to.x - from.x, to.y - from.y};
    int ahead = 0;
    int behind = 0;
    for (Point const& unit : units)
    {
        float const along = (unit.x - from.x) * walk.x + (unit.y - from.y) * walk.y;
        if (along > 0.0f)
            ++ahead;
        else if (along < 0.0f)
            ++behind;
    }

    return behind > ahead;
}
}

#endif
