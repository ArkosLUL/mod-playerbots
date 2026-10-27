#ifndef PLAYERBOTS_RAID_TOCANUBARAKPURSUIT_H
#define PLAYERBOTS_RAID_TOCANUBARAKPURSUIT_H

#include <algorithm>
#include <cmath>
#include <vector>

// The Pursuing Spike runs pure pursuit: every moment it heads straight at where its target is right then.
// Replayed from the kiter's real path over six 25H pulls, that put it within 0.9 yd of the real spike 6 s
// on, nine times in ten. No game headers here so nativetest can build it; the drag's waypoints live here
// for the same reason.
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

// Through each leg's end in turn, then stands on the last
struct LegPath
{
    Point start;
    std::vector<Point> legs;
    float speed = 7.0f;

    Point At(float t) const
    {
        float left = speed * t;
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
    // Closest pass by each point, in the order given
    std::vector<float> spikeClosest;
    std::vector<float> kiterClosest;
    // First time the spike comes within arrive of each point, negative when it doesn't
    std::vector<float> spikeArriveS;
    // First time the spike is within reach of the kiter, negative when it isn't within the horizon
    float catchS = -1.0f;
};

// One tick of the chase, t seconds in: the speed step due by then, then straight at the kiter
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

// Path is a KiterPath or a LegPath
template <typename Path>
Pursuit Simulate(Spike spike, Path const& path, float horizonS, std::vector<Point> const& points, float reach,
                 float arrive = 0.0f)
{
    Pursuit out;
    Point kiter = path.start;
    for (Point const& point : points)
    {
        out.spikeClosest.push_back(Dist(spike.pos, point));
        out.kiterClosest.push_back(Dist(kiter, point));
        out.spikeArriveS.push_back(arrive > 0.0f && Dist(spike.pos, point) <= arrive ? 0.0f : -1.0f);
    }

    if (Dist(spike.pos, kiter) <= reach)
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
            if (arrive > 0.0f && out.spikeArriveS[k] < 0.0f && pass <= arrive)
                out.spikeArriveS[k] = t + TICK_S;
        }

        if (out.catchS < 0.0f && Dist(spike.pos, kiter) <= reach)
            out.catchS = t + TICK_S;
    }

    return out;
}

// The walk now: how much sooner than the catch the spike comes within patchReach of the patch and fails
// there, less leadS; negative when it doesn't. Under Hand of Protection the catch counts from the hand's
// end, immuneS in.
template <typename Path>
float PatchMargin(Spike const& spike, Path const& walk, Point patch, float patchReach, float reach, float immuneS,
                  float horizonS, float leadS)
{
    Pursuit const run = Simulate(spike, walk, horizonS, {patch}, reach, patchReach);
    float const arrive = run.spikeArriveS[0];
    float const caught = run.catchS < 0.0f ? -1.0f : std::max(run.catchS, immuneS);
    if (arrive < 0.0f)
        return caught < 0.0f ? horizonS : -1.0f;

    return (caught < 0.0f ? horizonS : caught) - arrive - leadS;
}

// offset past the patch on the far side from the spike
inline Point StandPast(Point patch, Point spike, float offset)
{
    float const d = Dist(spike, patch);
    if (d < 0.001f)
        return patch;

    return {patch.x + (patch.x - spike.x) / d * offset, patch.y + (patch.y - spike.y) / d * offset};
}

// PatchMargin of the walk to the stand past the patch, started once the kiter has run on along run for
// delayS: whether turning that much later still makes it. The walk goes at walkSpeed.
template <typename Path>
float PatchMarginAfter(Spike spike, Path const& run, float delayS, float walkSpeed, Point patch, float standOffset,
                       float patchReach, float reach, float immuneS, float horizonS, float leadS)
{
    Point kiter = run.start;
    int const ticks = static_cast<int>(std::ceil(delayS / TICK_S - 0.001f));
    for (int i = 0; i < ticks; ++i)
    {
        float const t = static_cast<float>(i) * TICK_S;
        ChaseTick(spike, kiter, t);
        kiter = run.At(t + TICK_S);
    }

    float const elapsed = static_cast<float>(ticks) * TICK_S;
    spike.nextStepS -= elapsed;
    LegPath walk;
    walk.start = kiter;
    walk.legs = {StandPast(patch, spike.pos, standOffset)};
    walk.speed = walkSpeed;
    return PatchMargin(spike, walk, patch, patchReach, reach, std::max(immuneS - elapsed, 0.0f), horizonS, leadS);
}

// What ends a heroic kite
enum class KiteTerminal : int
{
    // It outlasts the emerge, or nothing is left
    None,
    Patch,
    // Hand of Protection first, then the patch
    PatchHand,
    Hand,
    // Divine Shield, Ice Block, Vanish or Feign Death: each hands the spike on
    Own
};

struct KiteOptions
{
    // On the kiter's best run
    float catchS = 0.0f;
    float emergeS = 0.0f;
    // A patch the kite may end on, and the walk to it beats the catch now
    bool patch = false;
    bool patchReachable = false;
    // A Hand of Protection can reach it: an escort with one ready, or a marked paladin's own
    bool hand = false;
    // Paladin bots with one ready, the kiter counted
    int handsReady = 0;
    bool own = false;
};

// A patch spent early buys a fresh run from the spike's base speed; a later catch is left to a Hand,
// which covers 10 s
constexpr float KITE_PATCH_EARLY_S = 12.0f;
constexpr float KITE_OUTLAST_S = 1.0f;

// The patch first while the catch leaves 12+ s, a Hand bridging when the walk to it is too late or a
// second Hand stays ready for a later kite; then a Hand, the patch as the last resort, the kiter's own
inline KiteTerminal PickTerminal(KiteOptions const& o)
{
    if (o.catchS >= o.emergeS - KITE_OUTLAST_S)
        return KiteTerminal::None;

    bool const early = o.patch && o.emergeS - o.catchS >= KITE_PATCH_EARLY_S;
    if (early && o.hand && (!o.patchReachable || o.handsReady >= 2))
        return KiteTerminal::PatchHand;

    if (early && o.patchReachable)
        return KiteTerminal::Patch;

    if (o.hand)
        return KiteTerminal::Hand;

    if (o.patch && (o.patchReachable || !o.own))
        return KiteTerminal::Patch;

    return o.own ? KiteTerminal::Own : KiteTerminal::None;
}

// Drag waypoints for when the boss's straight line to the anchor passes within trigger of a patch:
// standOff either side of each such patch, square to that line, none within keepout of any patch, the
// nearest the boss first
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

// Eight points radius round centre, none within keepout of a patch, the nearest from first
inline std::vector<Point> RingPoints(Point centre, float radius, Point from, std::vector<Point> const& patches,
                                     float keepout)
{
    std::vector<Point> out;
    for (int i = 0; i < 8; ++i)
    {
        float const angle = static_cast<float>(i) * 0.785398163f;
        Point const point = {centre.x + radius * std::cos(angle), centre.y + radius * std::sin(angle)};
        if (std::none_of(patches.begin(), patches.end(),
                         [&point, keepout](Point const& patch) { return Dist(point, patch) < keepout; }))
            out.push_back(point);
    }

    std::sort(out.begin(), out.end(), [from](Point const& a, Point const& b) { return Dist(from, a) < Dist(from, b); });
    return out;
}
}  // namespace AnubarakPursuit

#endif
