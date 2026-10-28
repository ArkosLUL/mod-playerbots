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

// One of the heroic run's paths per heading: the circle it ends on, the way round, and how far the
// straight leg runs before it, the whole leg when that's past the circle
struct PathMember
{
    float radius = 40.0f;
    float turn = 1.0f;
    float straightCap = 1e9f;
};

// MakeKiterPath with the way round set, the straight leg cut short at straightCap: the kiter then rounds
// the circle it stands on
inline KiterPath MakeKiterPath(Point start, Point dir, float speed, Point centre, PathMember const& member)
{
    KiterPath path = MakeKiterPath(start, dir, speed, centre, member.radius, member.turn);
    path.turn = member.turn;
    if (member.straightCap < path.straight)
    {
        path.straight = std::max(0.0f, member.straightCap);
        path.radius = Dist({start.x + dir.x * path.straight, start.y + dir.y * path.straight}, centre);
    }

    return path;
}

// Every member for one heading: both ways round the ring and the rim, and while the spike is at its base
// speed both ways round from where the straight leg stands as it speeds up, the rim at the latest. Ringed
// one way only, a kite never showed the corner the spike cut the other way.
inline std::vector<PathMember> PathFamily(float ring, float rim, float stepCapYd)
{
    std::vector<PathMember> out;
    for (float turn : {1.0f, -1.0f})
    {
        out.push_back({ring, turn, 1e9f});
        out.push_back({rim, turn, 1e9f});
        if (stepCapYd > 0.0f)
            out.push_back({rim, turn, stepCapYd});
    }

    return out;
}

// Through each leg's end in turn, then stands on the last
struct LegPath
{
    Point start;
    std::vector<Point> legs;
    float speed = 7.0f;
    // Where it stands every TICK_S from the start once Slow() ran, else empty
    std::vector<Point> track;

    // Inside radius of centre it goes at speed * factor, judged where each tick starts: Permafrost's
    // slow. Set the legs first; the track runs until the last leg or horizonS.
    void Slow(Point centre, float radius, float factor, float horizonS)
    {
        track.assign(1, start);
        if (speed <= 0.0f || legs.empty())
            return;

        Point at = start;
        size_t leg = 0;
        int const ticks = static_cast<int>(std::ceil(horizonS / TICK_S));
        for (int i = 0; i < ticks && leg < legs.size(); ++i)
        {
            float left = speed * TICK_S * (Dist(at, centre) < radius ? factor : 1.0f);
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

// Catches Simulate leaves out: before fromS, while Hand of Protection holds, and with the kiter inside
// safeRadius of safe
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

// Path is a KiterPath or a LegPath. arrives holds a radius per point, 0 for none.
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

// The same arrive radius round every point
template <typename Path>
Pursuit Simulate(Spike spike, Path const& path, float horizonS, std::vector<Point> const& points, float reach,
                 float arrive = 0.0f)
{
    return Simulate(spike, path, horizonS, points, reach, std::vector<float>(points.size(), arrive));
}

// The walk now: how much sooner than the catch the spike comes within patchReach of the patch and fails
// there, less leadS; negative when it doesn't. Under Hand of Protection the catch counts from the hand's
// end, immuneS in. A kiter within patchReach - reach of the patch can't be caught: the spike fails first.
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

// How a patch ends a kite
enum class PatchMove : int
{
    // Straight to the centre, through the slow
    In,
    // Past the patch's edge with it on the kiter's left, then on along the tangent
    EdgeLeft,
    EdgeRight
};

struct PatchRules
{
    // The spike's patch test and Impale's reach, centre to centre
    float patchReach = 11.5f;
    float impaleReach = 7.5f;
    float slowReach = 7.5f;
    // The kiter's speed inside the slow, as a share of its own: 1 for none
    float slowFactor = 1.0f;
    // An edge pass touches this ring round the centre, then runs on this far
    float edgeRadius = 8.5f;
    float edgeRunOn = 15.0f;
    float horizonS = 20.0f;
    float leadS = 1.0f;
};

struct PatchWalk
{
    bool valid = false;
    PatchMove move = PatchMove::In;
    std::vector<Point> legs;
    // PatchMargin of the walk
    float marginS = -1.0f;
};

// The move's legs from from. An edge needs from outside the edge ring: the tangent point, then edgeRunOn
// on along the tangent.
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

inline float PatchWalkMargin(Spike const& spike, Point from, float speed, Point patch, std::vector<Point> const& legs,
                             PatchRules const& rules, float immuneS)
{
    LegPath walk;
    walk.start = from;
    walk.legs = legs;
    walk.speed = speed;
    if (rules.slowFactor < 1.0f)
        walk.Slow(patch, rules.slowReach, rules.slowFactor, rules.horizonS);

    return PatchMargin(spike, walk, patch, rules.patchReach, rules.impaleReach, immuneS, rules.horizonS, rules.leadS);
}

// The move with the widest margin of those allow(move, legs) passes. Under Hand of Protection only In: the
// spike sits on the kiter, so it has to be walked into its test.
template <typename Allow>
PatchWalk BestPatchWalk(Spike const& spike, Point from, float speed, Point patch, PatchRules const& rules,
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
        walk.marginS = PatchWalkMargin(spike, from, speed, patch, walk.legs, rules, immuneS);
        if (!best.valid || walk.marginS > best.marginS)
            best = walk;
    }

    return best;
}

inline PatchWalk BestPatchWalk(Spike const& spike, Point from, float speed, Point patch, PatchRules const& rules,
                               float immuneS)
{
    return BestPatchWalk(spike, from, speed, patch, rules, immuneS,
                         [](PatchMove, std::vector<Point> const&) { return true; });
}

// Runs the chase on along run for delayS: where the kiter and the spike are then, the spike's speed
// schedule moved on with it
template <typename Path>
float ChaseAlong(Spike& spike, Path const& run, float delayS, Point& kiter)
{
    kiter = run.start;
    int const ticks = static_cast<int>(std::ceil(delayS / TICK_S - 0.001f));
    for (int i = 0; i < ticks; ++i)
    {
        float const t = static_cast<float>(i) * TICK_S;
        ChaseTick(spike, kiter, t);
        kiter = run.At(t + TICK_S);
    }

    float const elapsed = static_cast<float>(ticks) * TICK_S;
    spike.nextStepS -= elapsed;
    return elapsed;
}

// The best patch walk once the kiter has run on along run for delayS: whether turning that much later
// still makes it. The walk goes at walkSpeed.
template <typename Path, typename Allow>
PatchWalk PatchWalkAfter(Spike spike, Path const& run, float delayS, float walkSpeed, Point patch,
                         PatchRules const& rules, float immuneS, Allow const& allow)
{
    Point kiter;
    float const elapsed = ChaseAlong(spike, run, delayS, kiter);
    return BestPatchWalk(spike, kiter, walkSpeed, patch, rules, std::max(immuneS - elapsed, 0.0f), allow);
}

template <typename Path>
PatchWalk PatchWalkAfter(Spike spike, Path const& run, float delayS, float walkSpeed, Point patch,
                         PatchRules const& rules, float immuneS)
{
    return PatchWalkAfter(spike, run, delayS, walkSpeed, patch, rules, immuneS,
                          [](PatchMove, std::vector<Point> const&) { return true; });
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
    // Seconds to the spike's despawn
    float emergeS = 0.0f;
    // A patch the kite may end on
    bool patch = false;
    // A Hand of Protection can reach it: an escort with one ready, or a marked paladin's own
    bool hand = false;
    bool own = false;
    // The kiter is a paladin with Divine Shield ready, which a Hand's Forbearance would cost it
    bool paladinShield = false;
};

// A patch spent early buys a fresh run from the spike's base speed; a later catch is left to a Hand,
// which covers 10 s
constexpr float KITE_PATCH_EARLY_S = 12.0f;
constexpr float KITE_OUTLAST_S = 1.0f;

// Early with a Hand ready, the Hand at the catch and the patch under it. Else the kiter's own immunity,
// which leaves the patch to the next kiter; late, a non-paladin takes the Hand first and its own as the
// Hand runs out. Then a Hand, the patch alone.
inline KiteTerminal PickTerminal(KiteOptions const& o)
{
    if (o.catchS >= o.emergeS - KITE_OUTLAST_S)
        return KiteTerminal::None;

    bool const early = o.patch && o.emergeS - o.catchS >= KITE_PATCH_EARLY_S;
    if (early && o.hand && !o.paladinShield)
        return KiteTerminal::PatchHand;

    if (o.own)
        return o.hand && !early && !o.paladinShield ? KiteTerminal::Hand : KiteTerminal::Own;

    if (o.hand)
        return KiteTerminal::Hand;

    return o.patch ? KiteTerminal::Patch : KiteTerminal::None;
}

// The submerge's second tank patch, read when the kite would otherwise die
struct SecondPatchRead
{
    float handLeftS = 0.0f;
    // On the held run, negative past the horizon
    float catchS = -1.0f;
    bool own = false;
    // A ready Hand in reach could still land
    bool otherHand = false;
    // The catch, or the Hand's end, comes before the spike's last second
    bool beforeEnd = false;
    // More spheres flying than tank patches missing
    bool sphereSpare = false;
    // PatchMargin of the walk straight in now, and after running on the commit delay
    float walkS = -1.0f;
    float walkLaterS = -1.0f;
};

// Only when nothing else saves the kiter and the walk in still makes it, at the walk's last moment, the way
// a patch kite turns: under a Hand the kiter keeps 15.5 yd off the tank patches until then, and without one
// a walk left for the catch's last second never beats it. Without a Hand only once a catch is predicted.
inline bool SecondPatchDue(SecondPatchRead const& r)
{
    if (r.own || r.otherHand || !r.beforeEnd || !r.sphereSpare || r.walkS < 0.0f)
        return false;

    return (r.handLeftS > 0.0f || r.catchS >= 0.0f) && r.walkLaterS < 0.0f;
}

// A run heading as the hold judges it
struct HeadingRead
{
    // The spike's closest pass by any patch, and when it first comes inside a patch's clearance
    float patchPass = 0.0f;
    float firstInS = 0.0f;
    float catchS = 0.0f;
};

struct HeadingHold
{
    // The spike's fail test round a patch
    float testReach = 11.5f;
    float catchSlackS = 2.0f;
    float holdS = 1.5f;
    // With every heading passing a patch, the held one stays while it gets there at most this much sooner
    float firstInSlackS = 1.0f;
    float dwellS = 1.0f;
    float dwellDegrees = 90.0f;
};

// Whether the held heading stays ahead of the best one: while it clears the fail test, young or ranked with
// the best; while every heading passes a patch, as long as its pass comes no sooner than the best's by the
// slack; and a turn past dwellDegrees waits until it has been held dwellS. Re-picked freely, the two
// headings left at the rim swapped every tick and the kiter stood between them.
inline bool KeepHeading(HeadingRead const& held, HeadingRead const& best, bool sameClass, bool anyIntact,
                        float heldAgeS, float turnDegrees, HeadingHold const& rules = HeadingHold())
{
    bool const catchKept = held.catchS >= best.catchS - rules.catchSlackS;
    if (held.patchPass >= rules.testReach && catchKept && (heldAgeS < rules.holdS || sameClass))
        return true;

    if (!anyIntact && catchKept && held.firstInS >= best.firstInS - rules.firstInSlackS)
        return true;

    return turnDegrees > rules.dwellDegrees && heldAgeS < rules.dwellS;
}

// Where the spike is every everyTicks of its pursuit along path, from its start to the horizon
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

// Twice the signed area of a, b, p: which side of a to b p is on
inline float Cross(Point a, Point b, Point p) { return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x); }

// Closest approach of segments ab and cd, 0 where they cross
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

// point keeps clearance off every leg of track, or off its one point. An empty track is clear.
inline bool TrackClear(std::vector<Point> const& track, Point point, float clearance)
{
    if (track.size() == 1)
        return Dist(track.front(), point) >= clearance;

    for (size_t k = 1; k < track.size(); ++k)
        if (SegmentDist(track[k - 1], track[k], point) < clearance)
            return false;

    return true;
}

// The same for the whole walk from to to
inline bool TrackClear(std::vector<Point> const& track, Point from, Point to, float clearance)
{
    if (track.size() == 1)
        return SegmentDist(from, to, track.front()) >= clearance;

    for (size_t k = 1; k < track.size(); ++k)
        if (SegmentGap(track[k - 1], track[k], from, to) < clearance)
            return false;

    return true;
}

// Straight on from pos at velocity, in yd/s: where it is every stepS up to horizonS, the last point on
// the horizon. Standing still, pos alone.
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
    // Off the spike's track, and off its lane from where it is to the catch
    float trackClearance = 15.0f;
    float laneClearance = 9.0f;
};

// Where the escort waits for the catch: round the predicted catch point, off the spike's track to it and
// on the paladin's side of the spike's line to the kiter, so the walk there never crosses the chase and the
// dodge never sends it off again. The nearest the paladin first.
inline std::vector<Point> EscortPosts(Point catchPoint, Point spike, Point kiter, Point paladin,
                                      std::vector<Point> const& track, std::vector<Point> const& patches,
                                      EscortRules const& rules)
{
    float const side = Cross(spike, kiter, paladin);
    std::vector<Point> out;
    for (float radius : rules.rings)
        for (int i = 0; i < rules.bearings; ++i)
        {
            float const angle = 6.28318531f * static_cast<float>(i) / static_cast<float>(rules.bearings);
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

struct HoldDirs
{
    Point a;
    Point b;
    bool turned = false;
};

// Which way each side tank's hold sits off its patch centre. Its burrowers sit about inset off the centre
// that way, and the two sides' within Spider Frenzy's reach haste each other: the given directions while
// they keep apart, else both turned off the line between the patches toward the boss by the smallest step
// that does, past square to it if need be, at most straight away from each other.
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
        float const c = std::cos(degrees * 0.0174532925f);
        float const s = std::sin(degrees * 0.0174532925f);
        out.a = {along.x * c + normal.x * s, along.y * c + normal.y * s};
        out.b = {-along.x * c + normal.x * s, -along.y * c + normal.y * s};
        if (burrowersApart(out.a, out.b))
            return out;
    }

    return out;
}

// A drag's first step when the tank's walk to its leg spot crosses a patch's slow: sixteen points on each
// radius round the boss, out of keepout of every patch, the walk there and the walk on to the leg spot
// both clear of every slow; the nearest the tank first
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
        for (int i = 0; i < 16; ++i)
        {
            float const angle = static_cast<float>(i) * 0.392699082f;
            Point const point = {boss.x + radius * std::cos(angle), boss.y + radius * std::sin(angle)};
            if (clear(tank, point) && clear(point, legSpot))
                out.push_back(point);
        }

    std::sort(out.begin(), out.end(), [tank](Point const& a, Point const& b) { return Dist(tank, a) < Dist(tank, b); });
    return out;
}

// Where a grip relay's next link grips the landed sphere again: reach short of the landing toward the
// spot, then turned 20 and 40 degrees either way round the landing. The spot alone when it's in reach.
inline std::vector<Point> RelayPosts(Point landing, Point spot, float reach)
{
    float const length = Dist(landing, spot);
    if (length <= reach)
        return {spot};

    Point const dir = {(spot.x - landing.x) / length, (spot.y - landing.y) / length};
    std::vector<Point> out;
    for (float degrees : {0.0f, 20.0f, -20.0f, 40.0f, -40.0f})
    {
        float const angle = degrees * 0.0174532925f;
        float const c = std::cos(angle);
        float const s = std::sin(angle);
        out.push_back({landing.x + (dir.x * c - dir.y * s) * reach, landing.y + (dir.x * s + dir.y * c) * reach});
    }

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
