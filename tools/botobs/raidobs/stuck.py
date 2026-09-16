"""A bot with something to do and nothing happening: --stalls, --idle, --vetoes.

Three views of one failure. A stall is a walk asked for and never taken, an idle window is a
target held with nothing cast at it, and a veto is often what caused either.
"""
from __future__ import annotations

import math
from collections import Counter, defaultdict

from .trace import Trace, clock, first_deaths, roster_guids


def position_runs(track: list, tol: float, min_ms: int) -> list:
    """Maximal windows in which the unit never left a `tol`-yard disc."""
    runs = []
    index, count = 0, len(track)
    while index < count:
        end = index + 1
        x0, y0 = track[index][1], track[index][2]
        while end < count and math.hypot(track[end][1] - x0, track[end][2] - y0) <= tol:
            end += 1
        span = track[end - 1][0] - track[index][0]
        if span >= min_ms:
            runs.append((track[index][0], track[end - 1][0], x0, y0))
        index = end if end > index + 1 else index + 1
    return runs


ANSWERED_MOVE_YD = 5.0


def stall_windows(trace: Trace, min_ms: int) -> list[dict]:
    """Ordered to move and didn't.

    A bot that has *arrived* is also motionless, so standing still is not the signal on its own. The
    discriminator is that a mover which parks returns before it ever calls MoveTo and so leaves no
    `move` record at all: a stationary window that still contains accepted moves is one where the
    engine asked for a walk and the unit ignored it. That is what a spline-less POINT generator looks
    like from outside - PointMovementGenerator::DoInitialize returns without launching one while the
    unit is in UNIT_STATE_NOT_MOVE, so `ok` means "MovePoint was called", never "the unit moved".
    """
    roster = roster_guids(trace)
    dead_at = first_deaths(trace)

    tracks = defaultdict(list)
    for snap in trace.of("snap"):
        if snap["t"] < 0:
            continue
        for row in snap.get("u", []):
            if row[0] in roster:
                tracks[row[0]].append((snap["t"], row[1], row[2], row[5]))

    moves = defaultdict(list)
    for rec in trace.of("move"):
        if rec.get("ok"):
            moves[rec.get("g")].append(rec)

    found: list[dict] = []
    for guid, track in sorted(tracks.items(), key=lambda kv: trace.name(kv[0])):
        for start, end, x0, y0 in position_runs(track, 1.0, min_ms):
            if guid in dead_at and end > dead_at[guid]:
                continue
            issued = [m for m in moves[guid] if start <= m["t"] <= end]
            if not issued:
                continue
            # A goal only a few yards out is indistinguishable from having arrived: every mover has an
            # arrival deadband, and a walk that ends inside it looks identical to one never taken.
            furthest = max(math.hypot(m["x"] - x0, m["y"] - y0) for m in issued)
            if furthest <= ANSWERED_MOVE_YD:
                continue
            found.append({
                "guid": guid, "start": start, "end": end, "x": x0, "y": y0,
                "issued": len(issued), "furthest": furthest,
                "owners": sorted({m.get("by") or "?" for m in issued}),
            })
    return found


def show_stalls(trace: Trace, min_ms: int) -> int:
    print(f"stationary windows of at least {min_ms / 1000:.0f}s that still contain accepted moves\n")
    total = 0
    for window in stall_windows(trace, min_ms):
        total += window["end"] - window["start"]
        print(
            f"{trace.name(window['guid']):<16} {clock(window['start']):>9} -> {clock(window['end']):>9}"
            f"  {(window['end'] - window['start']) / 1000:6.1f}s"
            f" at ({window['x']:7.1f},{window['y']:7.1f})"
            f"  {window['issued']} move(s) accepted, furthest goal {window['furthest']:.0f} yd"
        )
        print(f"{'':16} {'':9}    {'':9}  wanted by: {', '.join(window['owners'])}")

    print(f"\ntotal: {total / 1000:.0f}s")
    return 0


# How long a bot has to hold a target casting nothing before it means something. Below this it is
# just a global cooldown and a walk.
IDLE_MS = 10000

# Snapshots are 4 Hz, so a target held across a gap wider than this is two holds, not one.
TARGET_GAP_MS = 2000


def idle_windows(trace: Trace, min_ms: int) -> list[dict]:
    """Held a target and cast nothing at all.

    The cast-side twin of `stall_windows`: same failure, other half of the bot. A vetoed walk with
    nothing walking in its place leaves a bot with somewhere to be and no way to get there, and from
    outside that is a unit pointed at something doing nothing to it. Only stretches where the bot
    held a target throughout count - otherwise every corpse and everyone waiting out a phase reads as
    idle.
    """
    roster = roster_guids(trace)

    casts: dict = defaultdict(list)
    for rec in trace.of("cast"):
        if rec.get("s") in roster:
            casts[rec["s"]].append(rec["t"])

    holding: dict = defaultdict(list)
    for snap in trace.of("snap"):
        for row in snap.get("u", []):
            guid = row[0]
            # Columns past 7 arrived in v8, and a corpse keeps whatever it died pointed at.
            if guid not in roster or len(row) < 8 or row[5] <= 0 or not row[7]:
                continue
            runs = holding[guid]
            if runs and snap["t"] - runs[-1][1] <= TARGET_GAP_MS:
                runs[-1] = (runs[-1][0], snap["t"])
            else:
                runs.append((snap["t"], snap["t"]))

    found: list[dict] = []
    for guid, runs in holding.items():
        stamps = sorted(casts.get(guid, []))
        for start, stop in runs:
            spoke = [when for when in stamps if start <= when <= stop]
            edges = [start] + spoke + [stop]
            # `start` is where the silence began, not where the target was picked up.
            longest, began = max(((edges[index + 1] - edges[index], edges[index])
                                  for index in range(len(edges) - 1)), key=lambda gap: gap[0])
            if longest >= min_ms:
                found.append({"guid": guid, "start": began, "quiet": longest})
    return found


def show_idle(trace: Trace, min_ms: int) -> int:
    print(f"bots that held a target and cast nothing for at least {min_ms / 1000:.0f}s\n")
    found = idle_windows(trace, min_ms)
    if not found:
        print("  none")
        return 0

    for window in sorted(found, key=lambda w: -w["quiet"]):
        print(f"{trace.name(window['guid']):<16} {trace.roles.get(window['guid'], '?'):<7}"
              f" {window['quiet'] / 1000.0:6.1f}s quiet from {clock(window['start'])}")
    return 0


def veto_counts(trace: Trace) -> Counter:
    """How often each (multiplier, action) pair zeroed the action."""
    return Counter((str(rec.get("m", "")), str(rec.get("a", ""))) for rec in trace.of("veto"))


def show_vetoes(trace: Trace, limit: int = 20) -> int:
    """Which multiplier zeroed which action, and how often.

    A veto is cheap to write and easy to get wrong. One that zeroes a walk with nothing walking in
    its place is a bot standing still, which this says long before the position views do.
    """
    rows = veto_counts(trace)
    if not rows:
        print("no multiplier vetoed anything in this pull")
        return 0

    print(f"{sum(rows.values())} veto(es), {len(rows)} distinct multiplier/action pairs\n")
    for (multiplier, action), count in rows.most_common(limit):
        print(f"{count:6}  {multiplier} -> {action}")
    return 0
