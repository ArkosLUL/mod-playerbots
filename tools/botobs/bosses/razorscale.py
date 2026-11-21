#!/usr/bin/env python3
"""Read a Razorscale pull from a RaidObs trace: phases, the ranged stack, Devouring Flame, deaths.

    razorscale.py <file>          every section
    razorscale.py <file> --stack  where 'razorscale grounded' sent the ranged and healers, and how
                                  many of those walks ended where the flame dodge moves them off again

What the generic views miss here:

- **Air and ground are her height.** She is up above z 440. Every landing but the last is a harpoon
  knockdown; once under 50% on the ground she stays down, so the last one can begin above 50%.
- **A Devouring Flame patch is a creature** (34188), sampled in the snapshots like any other unit.
  The dodge fires on `GetDistance2d`, which takes both combat reaches off, so a bot is dodged within
  ~10 yd of a patch's centre (90% of dodges in the 25-man traces started inside 10.3), not the 7 yd
  constant.
- **The stack walk and the dodge fight.** A grounded walk that ends inside that reach is undone by the
  dodge the next tick and re-issued the tick after.
- **Most move rows are refusals.** A mover asks every tick and gets `wait` while the last walk runs,
  so only the issued rows count as walks.
"""
from __future__ import annotations

import bisect
import collections
import pathlib
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.geometry import dist2, frames  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths  # noqa: E402

ENCOUNTER = "razorscale"

NPC_RAZORSCALE = 33186
NPC_DEVOURING_FLAME = 34188
ADDS = {33846: "Dark Rune Sentinel", 33453: "Dark Rune Watcher", 33388: "Dark Rune Guardian"}

FLYING_Z = 440.0
DODGE_REACH_YD = 10.0
STACK_ROLES = ("ranged", "heal")
STACK_MOVER = "razorscale grounded"


def mine(trace: Trace) -> bool:
    return encounter_of(trace) == ENCOUNTER


def entry(trace: Trace, guid) -> int | None:
    return trace.entries.get(guid)


def show_banner(trace: Trace) -> None:
    print(f"Razorscale  {trace.path.name}")
    if not mine(trace):
        print("  not a Razorscale pull, every section below reads empty")
        return
    ends = trace.of("end")
    out = ends[-1].get("out", "?") if ends else "?"
    when = f" at {clock(ends[-1]['t'])}" if ends else ""
    print(f"  outcome {out}{when}, {len(combat_deaths(trace))} death(s)")


def phases(trace: Trace) -> list[dict]:
    """Her air and ground stretches off the snapshots: start, end, and her health when each began."""
    out: list[dict] = []
    for snap in frames(trace):
        for row in snap.get("u", []):
            if entry(trace, row[0]) != NPC_RAZORSCALE:
                continue
            kind = "air" if row[3] >= FLYING_Z else "ground"
            if not out or out[-1]["kind"] != kind:
                out.append({"kind": kind, "t": snap["t"], "end": snap["t"], "hp": row[5], "last": False})
            out[-1]["end"] = snap["t"]
    if out:
        out[-1]["last"] = True
    return out


def phase_at(spans: list[dict], t: int) -> dict | None:
    """The stretch under way at t: the last one begun by then, since a row can fall between samples."""
    under_way = None
    for span in spans:
        if span["t"] > t:
            break
        under_way = span
    return under_way


def ground_label(span: dict) -> str:
    return "final" if span["last"] else "knockdown"


def show_phases(trace: Trace) -> None:
    print("PHASES")
    if not mine(trace):
        return
    spans = phases(trace)
    if not spans:
        print("  Razorscale never sampled")
    for span in spans:
        tail = f", {ground_label(span)}" if span["kind"] == "ground" else ""
        print(f"  {clock(span['t'])} .. {clock(span['end'])}  {span['kind']:<6} from {span['hp']:.1f}%{tail}")


def live_flames(trace: Trace, snap: dict) -> list[tuple]:
    return [(row[1], row[2]) for row in snap.get("u", [])
            if entry(trace, row[0]) == NPC_DEVOURING_FLAME and row[5] > 0]


def snapshot_before(trace: Trace, t: int) -> dict | None:
    snaps = frames(trace)
    stamps = getattr(trace, "_razorscale_stamps", None)
    if stamps is None:
        stamps = [snap["t"] for snap in snaps]
        trace._razorscale_stamps = stamps
    index = bisect.bisect_right(stamps, t) - 1
    return snaps[index] if index >= 0 else None


def stack_walks(trace: Trace) -> list[dict]:
    """Every ranged or healer walk the stack mover issued, with whether a live patch covered its end."""
    walks: list[dict] = []
    for rec in trace.of("move"):
        if rec.get("by") != STACK_MOVER or trace.role(rec.get("g")) not in STACK_ROLES or not rec.get("ok"):
            continue
        snap = snapshot_before(trace, rec["t"])
        spot = (rec["x"], rec["y"])
        fire = snap is not None and any(dist2(spot, flame) < DODGE_REACH_YD for flame in live_flames(trace, snap))
        walks.append({"t": rec["t"], "g": rec["g"], "spot": spot, "fire": fire})
    return walks


def show_stack(trace: Trace) -> None:
    print("RANGED STACK")
    if not mine(trace):
        return
    walks = stack_walks(trace)
    if not walks:
        print("  no stack walks")
        return
    spans = phases(trace)
    by_span: dict[int, list[dict]] = collections.defaultdict(list)
    for walk in walks:
        span = phase_at(spans, walk["t"])
        if span is not None:
            by_span[id(span)].append(walk)
    for span in spans:
        inside = by_span.get(id(span))
        if not inside:
            continue
        spots = collections.Counter((round(w["spot"][0]), round(w["spot"][1])) for w in inside)
        bots = len({walk["g"] for walk in inside})
        onto = sum(1 for walk in inside if walk["fire"])
        top, top_n = spots.most_common(1)[0]
        label = ground_label(span) if span["kind"] == "ground" else "air"
        print(f"  {clock(span['t'])} {label:<9}  {len(inside)} walks by {bots} bots to "
              f"{len(spots)} spot(s), busiest ({top[0]}, {top[1]}) x{top_n}; onto a live patch {onto}")
    onto = sum(1 for walk in walks if walk["fire"])
    print(f"  whole pull: {onto} of {len(walks)} walks ended within {DODGE_REACH_YD:.0f} yd of a live patch")


def show_fire(trace: Trace) -> None:
    print("DEVOURING FLAME")
    if not mine(trace):
        return
    spans = phases(trace)
    taken: collections.Counter = collections.Counter()
    hits = 0
    for rec in trace.of("dmg"):
        if entry(trace, rec.get("s")) != NPC_DEVOURING_FLAME:
            continue
        role = trace.role(rec.get("d"))
        if role == "?":
            continue
        span = phase_at(spans, rec["t"])
        taken[(span["kind"] if span else "?", role)] += rec.get("a", 0)
        hits += 1
    if not hits:
        print("  no damage taken")
        return
    for kind in ("air", "ground", "?"):
        row = {role: amount for (k, role), amount in taken.items() if k == kind}
        if row:
            parts = ", ".join(f"{role} {amount // 1000}k" for role, amount in sorted(row.items()))
            print(f"  {kind:<6} {parts}")
    print(f"  {hits} hits on the raid")


def mechanic(trace: Trace, death: dict) -> str:
    killer = entry(trace, death.get("killer"))
    if killer == NPC_DEVOURING_FLAME:
        return "Devouring Flame"
    if killer in ADDS:
        return ADDS[killer]
    rewind = sorted(death.get("rewind") or [], key=lambda row: row[0])
    spell_id = rewind[-1][2] if rewind else 0
    spell = (trace.spells.get(spell_id) or f"spell {spell_id}") if spell_id else "melee"
    if killer == NPC_RAZORSCALE:
        return f"Razorscale {spell}"
    return f"{trace.name(death.get('killer'))} {spell}".strip()


def show_deaths(trace: Trace) -> None:
    print("DEATHS BY MECHANIC")
    if not mine(trace):
        return
    spans = phases(trace)
    counts: collections.Counter = collections.Counter()
    for death in combat_deaths(trace):
        span = phase_at(spans, death["t"])
        counts[(mechanic(trace, death), span["kind"] if span else "?")] += 1
    if not counts:
        print("  none")
    for (name, where), n in counts.most_common():
        print(f"  {n:3d}  {name} ({where})")


SECTIONS = (
    ("phases", "her air and ground phases, with her health at each change", show_phases),
    ("stack", "where the stack mover sent ranged and healers, and how often onto a live patch", show_stack),
    ("fire", "Devouring Flame damage taken by role, air and ground", show_fire),
    ("deaths", "deaths by mechanic and phase", show_deaths),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
