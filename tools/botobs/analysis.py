"""Passes over a whole trace: what covered a death spot, what a rewind adds up to, where a bot stood.

These answer questions spanning many records, where records.py only renders one.
"""
from __future__ import annotations

import math
from collections import defaultdict

from obstrace import Trace

# How close an impact marker or a hazard creature has to be to count as "on top of the bot". A
# zero-radius hazard row is a point, not an area, so containment cannot be tested against it.
MARKER_PROXIMITY_YD = 5.0


def snapshot_before(trace: Trace, when: int) -> dict | None:
    sample = None
    for snap in trace.of("snap"):
        if snap["t"] > when:
            break
        sample = snap
    return sample


def hazards_at(trace: Trace, death: dict) -> tuple[list, list, list, list]:
    """What was on the spot the bot died on, from the last sample before it.

    Four answers, because one rule does not cover them. `covering` is the classic containment test.
    `markers` exists because the boss dynobjects that matter carry no radius at all - Hodir's three
    Icicle spells have a zero-radius DBC row, so CalcRadius has nothing to return - which leaves the
    row a point saying where something landed. `units` is the sweep, and on Hodir it is the real
    answer: the thing that kills is a creature, not a dynobject. `friendly` is the zones the bot was
    inside, because "not standing in the fire that sheds the stacks" is a diagnosis too.
    """
    sample = snapshot_before(trace, death["t"])
    if not sample:
        return [], [], [], []

    where = (death.get("x"), death.get("y"))
    covering, markers, friendly = [], [], []
    for row in sample.get("hz", []):
        if len(row) < 6:
            continue

        spell, hx, hy, _hz, radius, foe = row[:6]
        gap = math.dist(where, (hx, hy))

        if not foe:
            if radius and gap <= radius:
                friendly.append((spell, gap, radius))
            continue

        if radius:
            if gap <= radius:
                covering.append((spell, gap, radius))
        elif gap <= MARKER_PROXIMITY_YD:
            markers.append((spell, gap, radius))

    roster = set(trace.roles)
    units = []
    for row in sample.get("u", []):
        guid = row[0]
        if guid in roster or guid in trace.humans:
            continue

        gap = math.dist(where, (row[1], row[2]))
        if gap <= MARKER_PROXIMITY_YD:
            units.append((guid, gap))

    for group in (covering, markers, friendly, units):
        group.sort(key=lambda h: h[1])

    return covering, markers, units, friendly


def damage_summary(trace: Trace, rewind: list[list]) -> list[tuple[str, int, int]]:
    totals: dict[tuple[int, int], list[int]] = defaultdict(lambda: [0, 0])
    for _, source, spell, amount in rewind:
        entry = totals[(source, spell)]
        entry[0] += amount
        entry[1] += 1

    rows = []
    for (source, spell), (amount, hits) in totals.items():
        rows.append((f"{trace.name(source)} {trace.spell(spell)}", amount, hits))

    rows.sort(key=lambda r: -r[1])
    return rows


def combat_deaths(trace: Trace) -> list[dict]:
    """Deaths worth reading. The master's `wipe` command kills through Unit::Kill, which never reaches
    DealDamage, so those records carry no blow and name the bot as its own killer - 16 of one Freya
    attempt's 29. Numbering over these keeps --death N pointing at deaths that have a cause."""
    return [d for d in trace.of("death") if d.get("cause") != "reset"]


def roster_guids(trace: Trace) -> set:
    return {member["g"] for member in trace.header.get("roster", [])}


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
