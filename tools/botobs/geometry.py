"""Where a unit was, and how far that is from something the encounter named.

Every view that asks a spatial question used to answer it alone: six `position_at` implementations
across three files with three different policies, forty-odd open-coded `math.dist` calls in two
dialects, and "distance from a fixed point" written out seven times for one Yogg constant without
ever becoming a function. The questions are the same in every fight - who was inside the band, where
did this mover put people, how far out did they die - so they belong in one place and the fight
supplies only the point to measure from.

Those points already exist, named, in the raid tree: `const Position ULDUAR_YOGG_SARON_MIDDLE` and
`constexpr float ULDUAR_YOGG_SARON_P1_LEASH`. They are parsed out of the source for the same reason
probe keys are - a second copy kept by hand drifts, and the one in probes.py had already drifted
before it was a week old.
"""
from __future__ import annotations

import bisect
import functools
import math
import pathlib
import re

from obstrace import Trace

SRC_ROOT = pathlib.Path(__file__).resolve().parents[2] / "src"
RAID_ROOT = SRC_ROOT / "Ai" / "Raid"

# Snapshot row columns, by the name a caller would say. `t` is the snapshot's own stamp rather than a
# column, which is why it maps to None.
COLUMNS = {
    "t": None, "guid": 0, "x": 1, "y": 2, "z": 3, "o": 4, "hp": 5, "mana": 6,
    "target": 7, "moving": 8, "movegen": 9, "casting": 10, "dealt": 11,
}

# How a position is picked when the ask falls between two samples.
BEFORE = "before"    # the last sample at or before it - what a "where was it when X happened" wants
NEAREST = "nearest"  # the closer of the two, subject to `tol`


def dist2(a, b) -> float:
    """Ground distance. Takes anything subscriptable, so a 3-tuple from `at()` drops in unchanged."""
    return math.hypot(a[0] - b[0], a[1] - b[1])


def dist3(a, b) -> float:
    return math.dist(a[:3], b[:3])


def nearest(point, candidates):
    """`(key, gap)` for the closest candidate, or None. Candidates is a mapping or `(key, point)` pairs."""
    items = candidates.items() if hasattr(candidates, "items") else candidates
    best = None
    for key, spot in items:
        if spot is None:
            continue
        gap = dist2(point, spot)
        if best is None or gap < best[1]:
            best = (key, gap)
    return best


def edge(point, circles) -> float:
    """Signed distance to the nearest circle's edge, negative inside one.

    Standing 2 yd inside a 10 yd pool and 2 yd outside a 3 yd one are different problems, and a
    centre-to-centre distance cannot tell them apart. Circles are `(x, y, radius)`.
    """
    return min((dist2(point, circle) - circle[2] for circle in circles), default=math.inf)


def frames(trace: Trace) -> list[dict]:
    """The snapshots, in time order. Cached on the trace: `of()` walks every record per call and the
    spatial views ask for this inside their own loops."""
    cached = getattr(trace, "_geom_frames", None)
    if cached is None:
        cached = list(trace.of("snap"))
        trace._geom_frames = cached
    return cached


def _index(trace: Trace):
    """guid -> (stamps, spots), both in time order, so `at()` can bisect instead of rescanning.

    The version this replaces walked every snapshot on every call and was reached from inside a
    double loop.
    """
    cached = getattr(trace, "_geom_index", None)
    if cached is not None:
        return cached

    stamps: dict[int, list[int]] = {}
    spots: dict[int, list[tuple[float, float, float]]] = {}
    for snap in frames(trace):
        when = snap["t"]
        for row in snap.get("u", []):
            if len(row) < 4:
                continue
            guid = row[0]
            stamps.setdefault(guid, []).append(when)
            spots.setdefault(guid, []).append((row[1], row[2], row[3]))

    cached = (stamps, spots)
    trace._geom_index = cached
    return cached


def at(trace: Trace, guid: int, when: int, policy: str = BEFORE, tol: int | None = None):
    """Where a guid was at a time, as `(x, y, z)`, or None if no sample supports the answer.

    `tol` applies to NEAREST only: past it the honest answer is that nothing was sampled near enough.
    """
    stamps, spots = _index(trace)
    times = stamps.get(guid)
    if not times:
        return None

    index = bisect.bisect_right(times, when)
    if policy == BEFORE:
        return spots[guid][index - 1] if index else None

    options = [i for i in (index - 1, index) if 0 <= i < len(times)]
    if not options:
        return None
    pick = min(options, key=lambda i: abs(times[i] - when))
    if tol is not None and abs(times[pick] - when) > tol:
        return None
    return spots[guid][pick]


def track(trace: Trace, guids, cols=("t", "x", "y")) -> dict[int, list[tuple]]:
    """Per-guid samples in time order, each row carrying the named columns.

    A row too short for a requested column is skipped rather than indexed into: columns 8 to 11
    arrived in v8, and reading one off an older trace without checking is what takes down every view
    in a file at once.
    """
    want = [COLUMNS[name] for name in cols]
    need = max((column for column in want if column is not None), default=-1)

    out: dict[int, list[tuple]] = {}
    for snap in frames(trace):
        when = snap["t"]
        for row in snap.get("u", []):
            if row[0] not in guids or len(row) <= need:
                continue
            out.setdefault(row[0], []).append(
                tuple(when if column is None else row[column] for column in want))
    return out


def guids_of_entry(trace: Trace, entry: int) -> set[int]:
    return {guid for guid, known in trace.entries.items() if known == entry}


def first_seen(trace: Trace, guids) -> int | None:
    """When any of these guids first appeared in a snapshot."""
    stamps, _ = _index(trace)
    when = [stamps[guid][0] for guid in guids if stamps.get(guid)]
    return min(when) if when else None


# `Position(1980.28f, -25.5868f, 329.397f)` and `constexpr float X = 6.5f;`. Both are declared one per
# line across the raid tree, so a line scan is enough and there is no need to parse C++.
ANCHOR_DECL = re.compile(
    r"\bconst\s+Position\s+([A-Z][A-Z0-9_]*)\s*=\s*Position\s*\(\s*"
    r"(-?\d+(?:\.\d+)?)f?\s*,\s*(-?\d+(?:\.\d+)?)f?\s*,\s*(-?\d+(?:\.\d+)?)f?")
RADIUS_DECL = re.compile(r"\bconstexpr\s+float\s+([A-Z][A-Z0-9_]*)\s*=\s*(-?\d+(?:\.\d+)?)f?\s*;")


@functools.lru_cache(maxsize=4)
def _declared(root: pathlib.Path = RAID_ROOT):
    points: dict[str, tuple[float, float, float]] = {}
    spans: dict[str, float] = {}
    for path in sorted(root.rglob("*.h")) + sorted(root.rglob("*.cpp")):
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for line in text.splitlines():
            found = ANCHOR_DECL.search(line)
            if found:
                points.setdefault(found.group(1),
                                  (float(found.group(2)), float(found.group(3)), float(found.group(4))))
                continue
            found = RADIUS_DECL.search(line)
            if found:
                spans.setdefault(found.group(1), float(found.group(2)))
    return points, spans


def anchors(root: pathlib.Path = RAID_ROOT) -> dict[str, tuple[float, float, float]]:
    return _declared(root)[0]


def radii(root: pathlib.Path = RAID_ROOT) -> dict[str, float]:
    return _declared(root)[1]


class Unknown(LookupError):
    """A name that matched no constant, or more than one."""


def _lookup(table: dict, name: str, kind: str):
    if name in table:
        return table[name]

    wanted = name.upper()
    near = [key for key in table if key == wanted or key.endswith("_" + wanted)]
    if len(near) == 1:
        return table[near[0]]
    if near:
        raise Unknown(f"{name} matches {len(near)} {kind}s: {', '.join(sorted(near)[:6])}")
    raise Unknown(f"no {kind} named {name}")


def anchor(name: str, root: pathlib.Path = RAID_ROOT):
    """A named `Position` from the raid tree. A unique suffix is enough, so `MIDDLE` resolves where
    only one constant ends that way and reports the candidates where several do."""
    return _lookup(anchors(root), name, "anchor")


def radius(name: str, root: pathlib.Path = RAID_ROOT) -> float:
    """A named `constexpr float` from the raid tree, matched like `anchor`."""
    return _lookup(radii(root), name, "radius")


def reference(name: str, trace: Trace | None = None, root: pathlib.Path = RAID_ROOT):
    """A point to measure from, given either a constant's name or `entry:N` for a creature's own spot.

    The creature form exists because some fights have no named anchor worth the constant - the thing
    to measure from is wherever the boss happens to be standing.
    """
    if trace is not None and name.lower().startswith("entry:"):
        guids = guids_of_entry(trace, int(name.split(":", 1)[1]))
        spots = [at(trace, guid, 0, NEAREST) for guid in guids]
        spots = [spot for spot in spots if spot]
        if not spots:
            raise Unknown(f"no creature of {name} was sampled in this trace")
        return spots[0]
    return anchor(name, root)
