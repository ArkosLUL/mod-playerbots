#!/usr/bin/env python3
"""Score the three things that decide a Flame Leviathan pull, from a RaidObs trace.

    flame_leviathan.py <file>          every section
    flame_leviathan.py <file> --ram    Battering Ram exposure only
    flame_leviathan.py <file> --fury   Hodir's Fury only
    flame_leviathan.py <file> --adds   Freya's Ward adds only

**Battering Ram (62376)** is cast `me->CastSpell(me->GetVictim(), ...)` when the boss is
`IsWithinCombatRange(victim, 15.0f)`, and its ImplicitTargetA is 53 = TARGET_DEST_TARGET_ENEMY with
EffectRadiusIndex 20. It is therefore a 25 yd sphere centred on the **pursued vehicle**, not a cone
off the boss's front. `--ram` scores every other vehicle against that sphere, and against the
distance-to-boss gate the module uses, so the two can be compared directly.

**Hodir's Fury** is not a chaser at the moment that matters. npc_hodirs_fury walks to a target with
MoveFollow(target, 0, 0); on arrival it roots itself and runs a 5 s fuse, then drops a 10 yd blast
carrying an undispellable 60 s stun (62297) **where it stopped**. So the dodge window is the 5 s the
reticle spends stationary, and `--fury` measures who cleared 10 yd inside it.

**Freya's Ward** spawns four wards once, 30 s in, at the arena corners; each fires a wave every 29 s
for the rest of the pull, and the adds carry TEMPSUMMON_MANUAL_DESPAWN so they never time out. Every
wave also re-runs SelectNearestTarget(200) over the whole standing population, which is why they do
not stay in their corner. `--adds` scores what the fleet did about them.

Two things this file will not tell you, both of which have already fooled a reading of these traces:

- **The snapshot cast column only catches spells with a cast time.** Ram, Mortar and Fire Cannon are
  instant, so they never appear in it. Absence there is not evidence a seat held its fire - judge
  that from the add health deltas below.
- **Riders are not at their vehicle's exact coordinates.** A turret gunner sits 0.20 yd off its
  hull, so matching rider to hull on equal positions reports a crewed bot as dismounted.
"""
from __future__ import annotations

import argparse
import collections
import math
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from analysis import roster_guids  # noqa: E402
from obstrace import Trace  # noqa: E402

BOSS_ENTRY = 33113
RETICLE_ENTRY = 33108

# creature_model_info.CombatReach, which is what WorldObject::GetObjectSize returns.
VEHICLE_SIZE = {33060: 7.7, 33109: 2.25, 33062: 1.0}
VEHICLE_NAME = {33060: "siege", 33109: "demolisher", 33062: "chopper"}

BOSS_REACH = 15.0
RAM_RADIUS = 25.0
RAM_CAST_RANGE = 15.0       # boss_flame_leviathan.cpp: IsWithinCombatRange(victim, 15.0f)
FURY_RADIUS = 10.0          # EffectRadiusIndex 13
FURY_FUSE_MS = 5000

ADD_ENTRIES = {33387: "Writhing Lasher", 34275: "Ward of Life"}
LASH_SPELL = 65062

# The four NPC_FREYA_WARD_TARGET spawn points, boss_flame_leviathan.cpp SummonTowerHelpers.
ARENA_CORNERS = [(159.4, 64.1), (382.9, 74.0), (374.0, -141.0), (157.7, -140.3)]

# Weapon bands off Spell.dbc, as (vehicle entry, min, max, label). A cone is scored on range alone,
# so the cone rows are an upper bound - facing is the action's problem, not this table's.
WEAPON_BANDS = [
    (33060, 10.0, 70.0, "siege turret, Fire Cannon"),
    (33060, 0.0, 15.0, "siege driver, Ram cone"),
    (33109, 0.0, 50.0, "demo gunner, Mortar"),
    (33109, 10.0, 70.0, "demo driver, Hurl Boulder"),
    (33109, 0.0, 15.0, "demo driver, Ram cone"),
    (33062, 0.0, 35.0, "chopper, Sonic Horn cone"),
]
SPLASH_YD = 20.0            # Fire Cannon and Hurl Boulder, EffectRadiusIndex 9
MELEE_YD = 8.0              # close enough for Lash (0-5 yd) plus a snapshot's worth of travel
ATTRITION_WINDOW_MS = 5000


def boss_guid(ents: dict):
    for guid, entry in ents.items():
        if entry == BOSS_ENTRY:
            return guid
    return None


def stations(trace: Trace) -> dict:
    out = {}
    for rec in trace.of("note"):
        if rec.get("k") == "fl.station":
            out[rec["g"]] = rec.get("txt")
    return out


def deaths(trace: Trace) -> dict:
    out: dict = {}
    for rec in trace.of("death"):
        out.setdefault(rec["g"], rec["t"])
    return out


class Frame:
    """One snapshot reduced to the boss plus one entry per distinct vehicle."""

    def __init__(self, snap, trace, ents, roster, dead, station):
        self.t = snap["t"]
        self.boss = None
        self.reticles = []
        self.vehicles = {}      # (x, y) -> [size, station, is_real_vehicle_row]
        self.adds = []          # (guid, x, y, hp) - Freya's Ward spawns
        self.hulls = {}         # guid -> (x, y, hp, entry); identity, which self.vehicles drops

        rows = snap.get("u", [])
        for row in rows:
            entry = ents.get(row[0])
            if entry == BOSS_ENTRY:
                self.boss = (row[1], row[2], row[4], row[7])
            elif entry == RETICLE_ENTRY:
                self.reticles.append((row[0], row[1], row[2]))
            elif entry in ADD_ENTRIES:
                self.adds.append((row[0], row[1], row[2], row[5]))
            elif entry in VEHICLE_SIZE:
                # A trace written after vehicles joined the snapshot roster: use the real unit.
                self.vehicles[(round(row[1], 1), round(row[2], 1))] = [
                    VEHICLE_SIZE[entry], VEHICLE_NAME[entry], True
                ]
                self.hulls[row[0]] = (row[1], row[2], row[5], entry)

        if self.vehicles:
            return

        # Older traces carry riders only. A passenger's coordinates are its vehicle's, so identical
        # positions collapse back to one vehicle; the station note supplies the class.
        for row in rows:
            guid = row[0]
            if guid not in roster or (guid in dead and self.t >= dead[guid]):
                continue
            spot = (round(row[1], 1), round(row[2], 1))
            role = station.get(guid)
            if role and spot not in self.vehicles:
                size = {"siege": 7.7, "demolisher": 2.25}.get(role, 1.0)
                self.vehicles[spot] = [size, role, False]

    def victim(self):
        """The pursued vehicle: the boss chases it, so it is the position his facing points at.

        The snapshot names his target by guid, but a vehicle guid cannot be resolved to a position
        until vehicles are in the snapshot themselves, so fall back to the facing ray.
        """
        if not self.boss or not self.boss[3] or not self.vehicles:
            return None
        bx, by, bo, _ = self.boss
        best, score = None, None
        for (x, y) in self.vehicles:
            dist = math.hypot(x - bx, y - by)
            if dist > 60:
                continue
            bearing = math.atan2(y - by, x - bx)
            off = abs((bearing - bo + math.pi) % (2 * math.pi) - math.pi)
            rank = off + dist / 200.0
            if score is None or rank < score:
                best, score = (x, y, dist), rank
        return best


def frames(trace: Trace):
    ents = trace.entries
    if boss_guid(ents) is None:
        print("warning: no Flame Leviathan in this trace", file=sys.stderr)
        return
    roster, dead, station = roster_guids(trace), deaths(trace), stations(trace)
    for snap in trace.of("snap"):
        if snap["t"] < 0:
            continue
        frame = Frame(snap, trace, ents, roster, dead, station)
        if frame.boss:
            yield frame


def show_ram(trace: Trace) -> int:
    real = collections.Counter()        # inside the 25 yd sphere on the victim
    gated = collections.Counter()       # what dist-to-boss <= 25 + size would have caught
    both = collections.Counter()
    per_role = collections.defaultdict(collections.Counter)
    lead_to_victim = []
    lead_to_boss = []

    for frame in frames(trace):
        spot = frame.victim()
        if not spot:
            continue
        vx, vy, vdist = spot
        # He cannot fire from further out than his own cast test allows.
        if vdist > BOSS_REACH + RAM_CAST_RANGE + 7.7:
            continue
        bx, by = frame.boss[0], frame.boss[1]

        for (x, y), (size, role, _) in frame.vehicles.items():
            if (x, y) == (vx, vy):
                continue                # the victim is already kiting; it is exempt by design
            danger = math.hypot(x - vx, y - vy) <= RAM_RADIUS
            gate = math.hypot(x - bx, y - by) <= RAM_RADIUS + size
            real[danger] += 1
            gated[gate] += 1
            if danger and gate:
                both[True] += 1
            per_role[role][("danger", danger)] += 1
            if danger and not gate:
                per_role[role]["missed"] += 1
            if role == "tar-lead":
                lead_to_victim.append(math.hypot(x - vx, y - vy))
                lead_to_boss.append(math.hypot(x - bx, y - by))

    exposed = real[True]
    fired = gated[True]
    print("Battering Ram: a 25 yd sphere on the pursued vehicle\n")
    print(f"  vehicle-frames scored (boss in range to fire) : {real[True] + real[False]}")
    print(f"  genuinely inside the blast                    : {exposed}")
    if exposed:
        print(f"     caught by the distance-to-boss gate        : {both[True]} ({100 * both[True] / exposed:.1f}%)")
    if fired:
        print(f"  gate fired while safe (dragged off station)   : {fired - both[True]}"
              f" ({100 * (fired - both[True]) / fired:.1f}% of its activations)")

    print("\n  by station:")
    print(f"     {'station':12s} {'in blast':>9s} {'invisible to the gate':>23s}")
    for role in sorted(per_role):
        counts = per_role[role]
        inside = counts[("danger", True)]
        missed = counts["missed"]
        share = f"{100 * missed / inside:.0f}%" if inside else "-"
        print(f"     {role:12s} {inside:9d} {missed:14d} ({share})")

    if lead_to_victim:
        lead_to_victim.sort()
        lead_to_boss.sort()
        pick = lambda seq, q: seq[min(int(len(seq) * q), len(seq) - 1)]
        inside = sum(1 for d in lead_to_victim if d <= RAM_RADIUS)
        print(f"\n  lead chopper ({len(lead_to_victim)} frames):")
        print(f"     to the boss    p25 {pick(lead_to_boss, .25):5.1f}  median {pick(lead_to_boss, .5):5.1f}"
              f"  p75 {pick(lead_to_boss, .75):5.1f}")
        print(f"     to the victim  p25 {pick(lead_to_victim, .25):5.1f}  median {pick(lead_to_victim, .5):5.1f}"
              f"  p75 {pick(lead_to_victim, .75):5.1f}")
        print(f"     inside the blast: {inside}/{len(lead_to_victim)}"
              f" = {100 * inside / len(lead_to_victim):.1f}%")
    return 0


def show_fury(trace: Trace) -> int:
    snapshots = list(frames(trace))

    tracks = collections.defaultdict(list)
    for frame in snapshots:
        for guid, x, y in frame.reticles:
            tracks[guid].append((frame.t, x, y))

    def where(track, when):
        seen = None
        for point in track:
            if point[0] <= when + 400:
                seen = point
            else:
                break
        return seen if seen and abs(seen[0] - when) <= 1500 else None

    # A commit is the reticle going still after a walk: MovementInform roots it, then the fuse runs.
    commits = []
    for track in tracks.values():
        index, count = 0, len(track)
        while index < count:
            end = index + 1
            while end < count and math.hypot(track[end][1] - track[index][1],
                                             track[end][2] - track[index][2]) <= 0.6:
                end += 1
            if track[end - 1][0] - track[index][0] >= 4000:
                before = where(track, track[index][0] - 3000)
                if before and math.hypot(before[1] - track[index][1],
                                         before[2] - track[index][2]) >= 2.0:
                    commits.append((track[index][0], track[index][1], track[index][2]))
            index = end if end > index + 1 else index + 1
    commits.sort()

    by_time = {frame.t: frame for frame in snapshots}
    times = sorted(by_time)

    def frame_at(when):
        best = None
        for t in times:
            if t <= when + 400:
                best = t
            else:
                break
        return by_time[best] if best is not None and abs(best - when) <= 1500 else None

    caught = collections.Counter()
    escaped = collections.Counter()
    finals = []
    print(f"Hodir's Fury: {len(commits)} commit(s) - reticle stopped, 5 s fuse, then a 10 yd blast\n")
    for when, hx, hy in commits:
        start = frame_at(when)
        landing = frame_at(when + FURY_FUSE_MS)
        if not start or not landing:
            continue
        for (x, y), (_size, role, _) in start.vehicles.items():
            if math.hypot(x - hx, y - hy) > FURY_RADIUS:
                continue
            # Follow the same vehicle by taking the nearest position in the landing frame.
            nearest, best = None, None
            for (px, py) in landing.vehicles:
                step = math.hypot(px - x, py - y)
                if best is None or step < best:
                    nearest, best = (px, py), step
            if nearest is None:
                continue
            out = math.hypot(nearest[0] - hx, nearest[1] - hy)
            finals.append(out)
            (escaped if out > FURY_RADIUS else caught)[role] += 1

    total = sum(caught.values()) + sum(escaped.values())
    print(f"  vehicles standing in the circle when the fuse started: {total}")
    if total:
        print(f"     cleared {FURY_RADIUS:.0f} yd inside the fuse : {sum(escaped.values())}"
              f" ({100 * sum(escaped.values()) / total:.0f}%)")
        print(f"     still inside when it landed      : {sum(caught.values())}"
              f" ({100 * sum(caught.values()) / total:.0f}%)   each costs a 60 s stun")
        finals.sort()
        print(f"     distance from centre at +5 s: median {finals[len(finals) // 2]:.1f} yd")
        print("\n     by station:  caught / escaped")
        for role in sorted(set(caught) | set(escaped)):
            print(f"       {role:12s} {caught[role]:4d} / {escaped[role]}")
    return 0


def pick(seq, quantile):
    return seq[min(int(len(seq) * quantile), len(seq) - 1)]


def nearest_corner(x, y):
    return min(((math.hypot(x - cx, y - cy), i) for i, (cx, cy) in enumerate(ARENA_CORNERS)))


def show_adds(trace: Trace) -> int:
    snapshots = list(frames(trace))
    if not snapshots:
        return 0

    tracks = collections.defaultdict(list)      # add guid -> [(t, x, y, hp)]
    population = []
    for frame in snapshots:
        for guid, x, y, hp in frame.adds:
            tracks[guid].append((frame.t, x, y, hp))
        population.append((frame.t, sum(1 for a in frame.adds if a[3] > 0)))

    print("Freya's Ward: four wards at the corners, a wave every 29 s, adds that never despawn\n")
    if not tracks:
        print("  no adds in this trace - the Tower of Life was down.")
        return 0

    # ---- where they spawn, and whether they stay there -------------------------------
    corners = collections.Counter()
    wander = []
    for points in tracks.values():
        _, x0, y0, _ = points[0]
        corners[nearest_corner(x0, y0)[1]] += 1
        wander.append(max(math.hypot(p[1] - x0, p[2] - y0) for p in points))
    wander.sort()
    print(f"  adds seen: {len(tracks)}   spawn corner: "
          + "  ".join(f"{i}:{corners[i]}" for i in range(len(ARENA_CORNERS))))
    print(f"  travelled from spawn: median {pick(wander, .5):.0f} yd  p90 {pick(wander, .9):.0f}"
          f"  max {wander[-1]:.0f}")
    for band in (10, 30, 80):
        near = sum(1 for w in wander if w <= band)
        print(f"     never left {band:2d} yd of it: {near}/{len(wander)}"
              f" ({100 * near / len(wander):.0f}%)")

    # ---- population, and how fast one dies -------------------------------------------
    buckets = collections.defaultdict(int)
    for t, alive in population:
        buckets[t // 20000] = max(buckets[t // 20000], alive)
    print("\n  most alive at once, per 20 s:")
    cells = [f"{k * 20:3d}s:{v}" for k, v in sorted(buckets.items())]
    for start in range(0, len(cells), 10):
        print("     " + "  ".join(cells[start:start + 10]))
    zeroed = sum(1 for k, v in sorted(buckets.items()) if v == 0 and k * 20000 > 40000)
    print(f"     20 s windows with the field clear after 40 s: {zeroed}")

    rates = []
    for points in tracks.values():
        span = (points[-1][0] - points[0][0]) / 1000.0
        if len(points) >= 8 and span > 5:
            rates.append((points[0][3] - points[-1][3]) / span)
    rates = sorted(r for r in rates if r > 0)
    if rates:
        print(f"  health lost per second: median {pick(rates, .5):.2f}%"
              f"  ->  median time to kill one add {100 / pick(rates, .5):.0f} s")

    # ---- what could have shot them, without anyone moving ----------------------------
    in_band = collections.Counter()
    reachable = 0
    add_frames = 0
    clump = []
    for frame in snapshots:
        if not frame.hulls:
            continue
        live = [(x, y) for _g, x, y, hp in frame.adds if hp > 0]
        for ax, ay in live:
            add_frames += 1
            clump.append(sum(1 for bx, by in live if math.hypot(ax - bx, ay - by) <= SPLASH_YD))
            covered = False
            for entry, low, high, label in WEAPON_BANDS:
                near = [math.hypot(ax - hx, ay - hy)
                        for hx, hy, _hp, he in frame.hulls.values() if he == entry]
                if near and low <= min(near) <= high:
                    in_band[label] += 1
                    covered = True
            reachable += covered

    if add_frames:
        print(f"\n  add-frames in a weapon band with nobody moving ({add_frames} scored):")
        for _entry, _low, _high, label in WEAPON_BANDS:
            hits = in_band[label]
            print(f"     {label:28s} {hits:6d}  {100 * hits / add_frames:5.1f}%")
        print(f"     {'>> reachable by something':28s} {reachable:6d}"
              f"  {100 * reachable / add_frames:5.1f}%")
        clump.sort()
        print(f"  adds inside one {SPLASH_YD:.0f} yd splash: median {pick(clump, .5)}"
              f"  p75 {pick(clump, .75)}  max {clump[-1]}")

    # ---- do they actually cost the fleet anything? -----------------------------------
    windows = collections.defaultdict(lambda: [None, None, 0, 0])
    for frame in snapshots:
        live = [(x, y) for _g, x, y, hp in frame.adds if hp > 0]
        for guid, (hx, hy, hp, _entry) in frame.hulls.items():
            cell = windows[(guid, frame.t // ATTRITION_WINDOW_MS)]
            if cell[0] is None:
                cell[0] = hp
            cell[1] = hp
            cell[2] += sum(1 for ax, ay in live if math.hypot(ax - hx, ay - hy) <= MELEE_YD)
            cell[3] += 1

    clean, engaged = [], []
    for first, last, adds, samples in windows.values():
        if samples < 5:
            continue
        (engaged if adds / samples >= 1.0 else clean).append(first - last)
    if clean and engaged:
        print(f"\n  hull health lost per {ATTRITION_WINDOW_MS // 1000} s:")
        print(f"     with no add within {MELEE_YD:.0f} yd : {sum(clean) / len(clean):5.2f}%"
              f"   ({len(clean)} windows)")
        print(f"     with one or more    : {sum(engaged) / len(engaged):5.2f}%"
              f"   ({len(engaged)} windows)")

    # ---- and what share of the raid's damage taken is theirs? ------------------------
    taken = collections.Counter()
    for rec in trace.of("dmg"):
        taken[rec.get("sp")] += rec.get("a", 0)
    total = sum(taken.values())
    if total:
        print(f"\n  Lash {LASH_SPELL} share of raid damage taken: "
              f"{100 * taken[LASH_SPELL] / total:.1f}% ({taken[LASH_SPELL]:,} of {total:,})")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("file", type=pathlib.Path)
    parser.add_argument("--ram", action="store_true", help="Battering Ram exposure only")
    parser.add_argument("--fury", action="store_true", help="Hodir's Fury only")
    parser.add_argument("--adds", action="store_true", help="Freya's Ward adds only")
    args = parser.parse_args()

    if not args.file.is_file():
        print(f"no such trace: {args.file}", file=sys.stderr)
        return 1

    trace = Trace(args.file)
    every = not (args.ram or args.fury or args.adds)
    if args.ram or every:
        show_ram(trace)
    if every:
        print()
    if args.fury or every:
        show_fury(trace)
    if every:
        print()
    if args.adds or every:
        show_adds(trace)
    return 0


if __name__ == "__main__":
    sys.exit(main())
