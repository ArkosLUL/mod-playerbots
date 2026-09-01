"""One function per query mode: --bot, --track, --notes, --stalls, --clump.

Each returns the process exit code, so main() can hand it straight to sys.exit.
"""
from __future__ import annotations

import math
import sys
from collections import defaultdict

from analysis import position_runs, roster_guids
from obstrace import Trace, clock
from records import move_line, note_text

def show_bot(trace: Trace, name: str) -> int:
    guid = trace.guid_by_name(name)
    if guid is None:
        print(f"no unit matching '{name}'", file=sys.stderr)
        return 1

    print(f"timeline for {trace.name(guid)} ({trace.roles.get(guid, '?')})")
    for rec in trace.records:
        event = rec.get("e")
        stamp = clock(rec["t"])
        if event == "act" and rec.get("g") == guid:
            print(f"{stamp}  act    {rec['vd']:<10} rel {rec['rel']:<8} {rec['a']}")
        elif event == "veto" and rec.get("g") == guid:
            print(f"{stamp}  VETO   {rec['m']} killed {rec['a']}")
        elif event == "move" and rec.get("g") == guid:
            print(f"{stamp}  move   {move_line(trace, rec)}")
        elif event == "note" and rec.get("g") == guid:
            print(f"{stamp}  note   {rec['k']} = {note_text(trace, rec)}")
        elif event == "dmg" and rec.get("d") == guid:
            print(f"{stamp}  dmg    {rec['a']:>7} from {trace.name(rec['s'])} {trace.spell(rec['sp'])} -> {rec['hp']}%")
        elif event == "heal" and rec.get("d") == guid:
            print(f"{stamp}  heal   {rec['a']:>7} from {trace.name(rec['s'])} {trace.spell(rec['sp'])} -> {rec['hp']}%")
        elif event == "abs" and rec.get("d") == guid:
            print(f"{stamp}  absorb {rec['a']:>7} by {trace.name(rec['s'])} shield {trace.spell(rec['sp'])}")
        elif event == "aura" and rec.get("d") == guid:
            verb = "lost" if rec.get("r") else "got"
            stacks = f" x{rec['st']}" if "st" in rec else ""
            print(f"{stamp}  aura   {verb} {trace.spell(rec['sp'])}{stacks} from {trace.name(rec['s'])}")
        elif event == "death" and rec.get("g") == guid:
            print(f"{stamp}  DIED   killed by {trace.name(rec.get('killer'))}")

    return 0


def show_track(trace: Trace, name: str) -> int:
    guid = trace.guid_by_name(name)
    if guid is None:
        print(f"no unit matching '{name}'", file=sys.stderr)
        return 1

    print(f"track for {trace.name(guid)}  (step = distance moved since previous sample)")
    header = "time      x        y        z       hp    step   " + "  ".join(
        f"d:{trace.name(b)[:10]}" for b in sorted(trace.bosses)
    )
    print(header)

    previous = None
    for snap in trace.of("snap"):
        rows = {row[0]: row for row in snap.get("u", [])}
        row = rows.get(guid)
        if not row:
            continue

        _, x, y, z, _o, hp = row[0], row[1], row[2], row[3], row[4], row[5]
        step = 0.0 if previous is None else math.dist((x, y, z), previous)
        previous = (x, y, z)

        dists = []
        for boss in sorted(trace.bosses):
            brow = rows.get(boss)
            dists.append(f"{math.dist((x, y, z), (brow[1], brow[2], brow[3])):8.1f}" if brow else "       -")

        print(f"{clock(snap['t']):>9} {x:8.1f} {y:8.1f} {z:7.1f} {hp:5.1f} {step:6.2f}  " + " ".join(dists))

    return 0


def show_notes(trace: Trace, prefix: str | None = None) -> int:
    for rec in trace.records:
        event = rec.get("e")
        # A note key is prefixed with the encounter that owns it, so one raid's latches can be read
        # without the others - Iron Assembly legitimately spans three bosses under one slug.
        if prefix and event == "note" and not rec.get("k", "").startswith(prefix):
            continue

        if event in ("pull", "end"):
            detail = rec.get("boss") or rec.get("out")
            print(f"{clock(rec['t']):>9}  {event.upper():<6} {detail}")
        elif event == "note":
            print(
                f"{clock(rec['t']):>9}  note   {trace.name(rec.get('g')):<16} {rec['k']} = {note_text(trace, rec)}"
            )
        elif event == "haz":
            shape = rec.get("shape", "?")
            # Shape fields differ per shape, so print whatever the probe attached rather than a
            # fixed set: "side" for a lane, "rad" for a circle, "lead"/"sweep"/"rate" for a sweep.
            extra = " ".join(
                f"{k}={v}"
                for k, v in rec.items()
                if k not in ("t", "e", "sp", "shape", "x", "y", "z", "ttl")
            )
            place = f"({rec.get('x')}, {rec.get('y')}, {rec.get('z')})"
            print(f"{clock(rec['t']):>9}  haz    {trace.spell(rec['sp'])} {shape} at {place} ttl {rec.get('ttl')} {extra}")

    return 0


ANSWERED_MOVE_YD = 5.0


def show_stalls(trace: Trace, min_ms: int) -> int:
    """Ordered to move and didn't.

    A bot that has *arrived* is also motionless, so standing still is not the signal on its own. The
    discriminator is that a mover which parks returns before it ever calls MoveTo and so leaves no
    `move` record at all: a stationary window that still contains accepted moves is one where the
    engine asked for a walk and the unit ignored it. That is what a spline-less POINT generator looks
    like from outside - PointMovementGenerator::DoInitialize returns without launching one while the
    unit is in UNIT_STATE_NOT_MOVE, so `ok` means "MovePoint was called", never "the unit moved".
    """
    roster = roster_guids(trace)
    dead_at: dict = {}
    for rec in trace.of("death"):
        dead_at.setdefault(rec["g"], rec["t"])

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

    print(f"stationary windows of at least {min_ms / 1000:.0f}s that still contain accepted moves\n")
    total = 0
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
            total += end - start
            owners = sorted({m.get("by") or "?" for m in issued})
            print(
                f"{trace.name(guid):<16} {clock(start):>9} -> {clock(end):>9}"
                f"  {(end - start) / 1000:6.1f}s at ({x0:7.1f},{y0:7.1f})"
                f"  {len(issued)} move(s) accepted, furthest goal {furthest:.0f} yd"
            )
            print(f"{'':16} {'':9}    {'':9}  wanted by: {', '.join(owners)}")

    print(f"\ntotal: {total / 1000:.0f}s")
    return 0


def show_clump(trace: Trace, radius: float) -> int:
    """How much of the pull had the raid stacked inside one AoE.

    Counts distinct *positions*, not bodies: passengers share their vehicle's coordinates exactly, so
    five riders in one siege engine are one thing an area spell can hit, not five.
    """
    roster = roster_guids(trace)
    dead_at: dict = {}
    for rec in trace.of("death"):
        dead_at.setdefault(rec["g"], rec["t"])

    histogram: dict = defaultdict(int)
    for snap in trace.of("snap"):
        if snap["t"] < 0:
            continue
        spots = {
            (round(row[1], 1), round(row[2], 1))
            for row in snap.get("u", [])
            if row[0] in roster and not (row[0] in dead_at and snap["t"] >= dead_at[row[0]])
        }
        if len(spots) < 2:
            continue
        biggest = max(
            sum(1 for x, y in spots if math.hypot(x - cx, y - cy) <= radius) for cx, cy in spots
        )
        histogram[biggest] += 1

    frames = sum(histogram.values())
    if not frames:
        print("no snapshots with two or more live positions")
        return 0

    print(f"most distinct positions inside one {radius:.0f} yd circle, per snapshot\n")
    running = 0
    for size in sorted(histogram, reverse=True):
        running += histogram[size]
        print(
            f"  {size:3d} together  {histogram[size]:6d} frames  {100 * histogram[size] / frames:5.1f}%"
            f"     >= {size}: {100 * running / frames:5.1f}% of the pull"
        )
    return 0
