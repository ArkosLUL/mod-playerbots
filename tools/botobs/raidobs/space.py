"""Where a mover put people, where things happened, who the enemy was actually on, and how
tightly the raid stacked.

"957 flee moves, 96.4% ended further from the band" is the kind of answer these give, and that one
named the Yogg-Saron phase 1 defect. Nothing in them is Yogg-specific: a mover walking the raid out
of a band, a kill landing too far from where it counts, and adds parked on somebody who is not a tank
are the same questions in every fight. What the fight supplies is the point to measure from, and
`geometry` reads those out of the raid tree by name.
"""
from __future__ import annotations

import collections
import math
import statistics

from . import geometry
from .geometry import Unknown
from .probes import latch_windows, parse_during
from .trace import Trace, clock, combat_deaths, first_deaths, roster_guids

# A mover named in fewer than this many rows is noise in the roll-up; ask for it by name to see it.
MIN_MOVES = 5


def scope(trace: Trace, during: str | None):
    """A predicate on a record's `t`, narrowed to when a latch held a value.

    Whole-pull numbers hide phase-scoped defects - the lesson `--probes --during` already carries -
    and it applies just as hard to where people stood.
    """
    if not during:
        return lambda when: when >= 0

    key, value = parse_during(during)
    spans = latch_windows(trace, key, value)
    if not spans:
        return None
    return lambda when: any(start <= when < stop for start, stop in spans)


def event_spots(trace: Trace, spec: str, inside=None) -> list[tuple[int, int, tuple]]:
    """`(t, guid, point)` for every event of a stream, as `death`, `cast:<spell>` or `note:<key>`.

    `cast:` exists because the interesting death is usually not in the `death` stream at all - that is
    roster-only, so a creature dying is invisible and what stands in for it is whatever it casts on
    the way out. Yogg's Guardians are read through their Shadow Nova for exactly this reason.
    """
    kind, _, argument = spec.partition(":")
    found: list[tuple[int, int, tuple]] = []

    if kind == "death":
        # The wipe command kills everyone wherever they stand, which says nothing about where.
        for rec in combat_deaths(trace):
            if rec.get("x") is None or (inside and not inside(rec["t"])):
                continue
            found.append((rec["t"], rec.get("g", 0), (rec["x"], rec["y"], rec.get("z", 0.0))))
        return found

    if kind == "cast":
        spell = int(argument)
        for rec in trace.of("cast"):
            if rec.get("sp") != spell or (inside and not inside(rec["t"])):
                continue
            guid = rec.get("s", 0)
            spot = geometry.at(trace, guid, rec["t"])
            if spot:
                found.append((rec["t"], guid, spot))
        return found

    if kind == "note":
        for rec in trace.of("note"):
            if rec.get("k") != argument or (inside and not inside(rec["t"])):
                continue
            guid = rec.get("g", 0)
            spot = geometry.at(trace, guid, rec["t"])
            if spot:
                found.append((rec["t"], guid, spot))
        return found

    raise Unknown(f"unknown event {spec!r}: expected death, cast:<spell> or note:<key>")


def band_of(spec: str | None) -> float | None:
    """A band is a named `constexpr float` from the raid tree, or a bare number when the fight never
    gave the distance a name - a spell's own radius usually has not."""
    if spec is None:
        return None
    try:
        return float(spec)
    except ValueError:
        return geometry.radius(spec)


def show_where(trace: Trace, spec: str, frm: str, band: str | None = None,
               during: str | None = None) -> int:
    """How far from a named point each event of a stream happened."""
    inside = scope(trace, during)
    if inside is None:
        print(f"  nothing held {during} in this trace")
        return 0

    try:
        origin = geometry.reference(frm, trace)
        edge = band_of(band)
        events = event_spots(trace, spec, inside)
    except (Unknown, ValueError) as exc:
        print(f"  {exc}")
        return 1

    if not events:
        print(f"  no {spec} events{' while ' + during if during else ''}")
        return 0

    radii = [geometry.dist2(spot, origin) for _, _, spot in events]
    print(f"{spec} against {frm}{', band ' + band if band else ''}"
          f"{' while ' + during if during else ''}")
    print(f"  {len(radii)} event(s), median {statistics.median(radii):.1f} yd, "
          f"range {min(radii):.1f}-{max(radii):.1f}")
    if edge is not None:
        out = sum(1 for r in radii if r > edge)
        print(f"  outside {edge:.1f} yd: {out} of {len(radii)}")

    print()
    for (when, guid, _), gap in sorted(zip(events, radii)):
        flag = "" if edge is None or gap <= edge else "   <- outside"
        print(f"  {clock(when):>9}  {gap:5.1f} yd  {trace.name(guid)}{flag}")
    return 0


def move_rows(trace: Trace, origin, inside):
    """Per move: the action that asked, the role, and the radius it started and ended at.

    A `move` record carries its destination and not its origin, so where the bot was standing comes
    from the snapshot before it.
    """
    rows = []
    for rec in trace.of("move"):
        when = rec.get("t", 0)
        if not inside(when) or rec.get("x") is None:
            continue
        guid = rec.get("g", 0)
        start = geometry.at(trace, guid, when)
        if not start:
            continue
        rows.append((
            str(rec.get("by", "?")),
            trace.roles.get(guid, "?"),
            geometry.dist2(start, origin),
            geometry.dist2((rec["x"], rec["y"]), origin),
            bool(rec.get("ok")),
        ))
    return rows


def show_moves(trace: Trace, action: str | None, frm: str, band: str | None = None,
               during: str | None = None) -> int:
    """What each mover did to the raid's distance from a named point."""
    inside = scope(trace, during)
    if inside is None:
        print(f"  nothing held {during} in this trace")
        return 0

    try:
        origin = geometry.reference(frm, trace)
        edge = band_of(band)
    except (Unknown, ValueError) as exc:
        print(f"  {exc}")
        return 1

    rows = move_rows(trace, origin, inside)
    if action:
        rows = [row for row in rows if action in row[0]]
    if not rows:
        print(f"  no moves matched{' ' + action if action else ''}")
        return 0

    grouped = collections.defaultdict(list)
    for row in rows:
        grouped[row[0]].append(row)

    print(f"moves against {frm}{', band ' + band if band else ''}"
          f"{' while ' + during if during else ''}")
    print(f"\n  {'mover':<44} {'n':>5} {'from':>6} {'to':>6} {'out':>6} "
          f"{'offband':>8} {'inband':>7} {'ok':>5}")
    order = sorted(grouped.items(), key=lambda item: -len(item[1]))
    for mover, group in order:
        if len(group) < MIN_MOVES and not action:
            continue
        starts = [row[2] for row in group]
        ends = [row[3] for row in group]
        further = sum(1 for row in group if row[3] > row[2]) * 100.0 / len(group)
        accepted = sum(1 for row in group if row[4]) * 100.0 / len(group)
        if edge is None:
            offband, inband = "       -", "      -"
        else:
            left = sum(1 for row in group if abs(row[3] - edge) > abs(row[2] - edge))
            offband = f"{left * 100.0 / len(group):7.1f}%"
            inband = f"{sum(1 for end in ends if end <= edge) * 100.0 / len(group):6.1f}%"
        print(f"  {mover[:44]:<44} {len(group):5} {statistics.median(starts):6.1f}"
              f" {statistics.median(ends):6.1f} {further:5.1f}% {offband} {inband} {accepted:4.0f}%")

    if action:
        roles = collections.Counter(row[1] for row in rows)
        print("\n  by role: " + "  ".join(f"{role} {count}" for role, count in roles.most_common()))
    print("\n  from/to are median radius before and after the move. out is the share that ended")
    print("  further from the point; offband the share that ended further from the band than it")
    print("  started, which is what leaving a ring looks like whichever way the bot went.")
    return 0


def role_held(trace: Trace, target: int) -> str:
    """What a hostile's victim counts as. A pet soaking hits is a real outcome and not the same as an
    unknown guid, so `owners` decides before the bucket does."""
    if not target:
        return "nobody"
    role = trace.roles.get(target)
    if role:
        return role
    return "pet" if target in trace.owners else "other"


def threat_share(trace: Trace, subjects: set[int] | None, inside):
    """Milliseconds each hostile spent on each role, as `(total, per_unit)` Counters.

    `subjects` narrows it to those guids; None means every hostile, which leaves out the raid's own
    pets as well as the raid (the snapshot carries pets since v10). A frame counts until the next
    snapshot, in scope or not, so a gap between two `--during` windows is never credited to the last
    frame before it.
    """
    roster = set(trace.roles)
    held = collections.Counter()
    per_unit: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
    frames = geometry.frames(trace)
    for index, snap in enumerate(frames):
        if not inside(snap["t"]):
            continue
        step = (frames[index + 1]["t"] - snap["t"]) if index + 1 < len(frames) else 0
        for row in snap.get("u", []):
            guid = row[0]
            # Pre-v8 rows stop before the target column, and a corpse holds whatever it died on.
            if len(row) < 8 or row[5] <= 0 or guid in roster:
                continue
            if subjects is not None:
                if guid not in subjects:
                    continue
            elif not row[7] or trace.owners.get(guid) in roster:
                continue
            role = role_held(trace, row[7])
            held[role] += step
            per_unit[guid][role] += step
    return held, per_unit


def show_share(held: collections.Counter, label: str = "time on target") -> None:
    total = sum(held.values())
    print(f"  {label}  " + "  ".join(f"{role}: {span * 100.0 / total:4.1f}%"
                                     for role, span in held.most_common()))
    print(f"    on a tank: {held['tank'] * 100.0 / total:.1f}%")


def show_threat(trace: Trace, entry: int | None = None, during: str | None = None) -> int:
    """Who the hostiles were beating on, by role, weighted by how long they held it.

    Redirects and taunts aim threat but cannot govern what nobody aimed at, so the share is the
    measurement and the casts are only the attempt. Reads `snap.u[7]`, which is the unit's victim.
    """
    inside = scope(trace, during)
    if inside is None:
        print(f"  nothing held {during} in this trace")
        return 0

    subjects = geometry.guids_of_entry(trace, entry) if entry else None
    held, per_unit = threat_share(trace, subjects, inside)

    label = f"entry {entry}" if entry else "every hostile sampled"
    print(f"threat, {label}{' while ' + during if during else ''}")
    if not sum(held.values()):
        print("  nothing hostile was ever sampled holding a target")
        return 0

    show_share(held)

    mostly = collections.Counter()
    for guid, roles in per_unit.items():
        if roles:
            mostly[roles.most_common(1)[0][0]] += 1
    print("  per unit, whoever held it longest: "
          + "  ".join(f"{role} {count}" for role, count in mostly.most_common()))
    return 0


def clump_histogram(trace: Trace, radius: float, inside=None) -> dict[int, int]:
    """How much of the pull had the raid stacked inside one AoE, as size -> snapshot count.

    Counts distinct *positions*, not bodies: passengers share their vehicle's coordinates exactly, so
    five riders in one siege engine are one thing an area spell can hit, not five.
    """
    roster = roster_guids(trace)
    dead_at = first_deaths(trace)

    histogram: dict = collections.defaultdict(int)
    for snap in trace.of("snap"):
        if snap["t"] < 0 or (inside and not inside(snap["t"])):
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
    return histogram


def show_clump(trace: Trace, radius: float, during: str | None = None) -> int:
    inside = scope(trace, during)
    if inside is None:
        print(f"  nothing held {during} in this trace")
        return 0

    histogram = clump_histogram(trace, radius, inside)
    frames = sum(histogram.values())
    scoped = f" while {during}" if during else ""
    if not frames:
        print(f"no snapshots with two or more live positions{scoped}")
        return 0

    whole = "the window" if during else "the pull"
    print(f"most distinct positions inside one {radius:.0f} yd circle, per snapshot{scoped}\n")
    running = 0
    for size in sorted(histogram, reverse=True):
        running += histogram[size]
        print(
            f"  {size:3d} together  {histogram[size]:6d} frames  {100 * histogram[size] / frames:5.1f}%"
            f"     >= {size}: {100 * running / frames:5.1f}% of {whole}"
        )
    return 0
