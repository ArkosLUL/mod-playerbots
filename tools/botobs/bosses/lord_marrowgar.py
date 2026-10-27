#!/usr/bin/env python3
"""Read a Lord Marrowgar pull from a RaidObs trace: Bone Storm, its anchors, the spikes, the cheats.

    lord_marrowgar.py <file>            every section
    lord_marrowgar.py <file> --phases   lm.phase: each Bone Storm, from the first bot to note it
    lord_marrowgar.py <file> --storms   who drew each Bone Storm to the far stand
    lord_marrowgar.py <file> --spikes   the icon layout over the pull, and the spike duty each bot held
    lord_marrowgar.py <file> --cheats   this encounter's cheat uses

What the generic views miss here:

- **An anchor is decided again in every storm but written only on change.** `lm.stormanchor` is
  asked only while Bone Storm is up, so a bot that anchored the last storm and still does writes no
  row in the next one. A storm's anchors are the bots that wrote `1` in it, or held `1` from earlier
  and wrote nothing.
- **`lm.spike` is the duty a bot took, not what it hit.** `skull`, `cross`, `star` are the ranged
  buckets, `nearest` a melee on the closest safe spike, `nosafe` a melee that found none, `tank` the
  assist tank on the tank's spike, `boss` a melee left on him while only the tank's spike stands.
- **`lm.mark` is the icon rule, and every bot writes it.** `spikesN` puts skull, cross and star on
  the N spikes in guid order, `tankspike` skull on the tank's spike and cross on him, `boss` skull on
  him; the steps start at the first bot to note each.
"""
from __future__ import annotations

import collections
import pathlib
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of, prefix_matches_boss  # noqa: E402
from raidobs.probes import holder_spans, latch_spans  # noqa: E402
from raidobs.trace import Trace, cheat_uses, clock, combat_deaths, notes  # noqa: E402

SLUG = "lord-marrowgar"
NAME = "Lord Marrowgar"
PHASE = "lm.phase"
STORM = "bonestorm"
ANCHOR = "lm.stormanchor"
MARK = "lm.mark"
SPIKE = "lm.spike"
DIFFICULTIES = {0: "10N", 1: "25N", 2: "10H", 3: "25H"}


def pull_end(trace: Trace) -> int:
    ends = trace.of("end")
    if ends:
        return ends[-1]["t"]
    return trace.records[-1].get("t", 0) if trace.records else 0


def mine(trace: Trace) -> bool:
    return encounter_of(trace) == SLUG


def phase_steps(trace: Trace) -> list[tuple[str, int, int]]:
    return latch_spans(trace, PHASE, pull_end(trace))


def storms(trace: Trace) -> list[tuple[int, int]]:
    return [(start, stop) for value, start, stop in phase_steps(trace) if value == STORM]


def anchors(trace: Trace, start: int, stop: int) -> set[int]:
    """The bots that drew the storm in `[start, stop)`."""
    rows: dict[int, list[tuple[int, str]]] = collections.defaultdict(list)
    for rec in sorted(notes(trace, ANCHOR), key=lambda rec: rec["t"]):
        rows[rec["g"]].append((rec["t"], str(rec.get("txt", ""))))
    out = set()
    for guid, track in rows.items():
        held = next((value for when, value in reversed(track) if when < start), "0")
        inside = [value for when, value in track if start <= when < stop]
        if "1" in inside or (held == "1" and not inside):
            out.add(guid)
    return out


def duties(trace: Trace) -> dict[str, set[int]]:
    """Per spike duty other than `none`, the bots that held it at some point."""
    out: dict[str, set[int]] = collections.defaultdict(set)
    for guid, spans in holder_spans(trace, SPIKE, pull_end(trace)).items():
        for value, _, _ in spans:
            if value != "none":
                out[value].add(guid)
    return dict(out)


def own_cheats(trace: Trace) -> list[tuple[str, dict]]:
    rows = [(key, entry) for key, entry in cheat_uses(trace).items()
            if prefix_matches_boss(key.partition(".")[0], SLUG)]
    return sorted(rows, key=lambda row: (row[1]["first"], row[0]))


def names(trace: Trace, guids) -> str:
    return ", ".join(sorted(trace.name(guid) for guid in guids)) or "nobody"


def show_banner(trace: Trace) -> None:
    end = pull_end(trace)
    outcome = trace.of("end")[-1].get("out", "?") if trace.of("end") else "no end record"
    diff = DIFFICULTIES.get(trace.header.get("diff"), "?")
    print(f"{encounter_of(trace)} {diff}  {outcome} at {clock(end)}  {len(combat_deaths(trace))} death(s)")
    if not mine(trace):
        print(f"  not a {NAME} pull, every section below reads empty")


def show_phases(trace: Trace) -> None:
    print("PHASES")
    if not mine(trace):
        return
    steps = phase_steps(trace)
    if not steps:
        print(f"  {PHASE} never written")
        return
    print(f"    {'from':>9}  {'held':>9}  phase")
    for value, start, stop in steps:
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {value}")


def show_storms(trace: Trace) -> None:
    print("BONE STORMS")
    if not mine(trace):
        return
    spans = storms(trace)
    if not spans:
        print("  no Bone Storm in this pull")
        return
    for index, (start, stop) in enumerate(spans, 1):
        drawn = names(trace, anchors(trace, start, stop))
        print(f"  {index}  {clock(start)} for {clock(stop - start)}  anchors {drawn}")


def show_spikes(trace: Trace) -> None:
    print("SPIKES")
    if not mine(trace):
        return
    layout = latch_spans(trace, MARK, pull_end(trace))
    if not layout:
        print(f"  {MARK} never written")
    for value, start, stop in layout:
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {value}")
    held = duties(trace)
    if not held:
        print("  no bot took a spike duty")
        return
    print()
    for value in sorted(held):
        print(f"  {value:<8} {names(trace, held[value])}")


def show_cheats(trace: Trace) -> None:
    print("CHEATS")
    if not mine(trace):
        return
    rows = own_cheats(trace)
    if not rows:
        print("  no lm.* or icc.* cheat fired")
        return
    for key, entry in rows:
        print(f"  {key}  {entry['n']} use(s) from {clock(entry['first'])}  by {names(trace, entry['bots'])}")


SECTIONS = (
    ("phases", "lm.phase: each Bone Storm, from the first bot to note it", show_phases),
    ("storms", "who drew each Bone Storm to the far stand", show_storms),
    ("spikes", "the icon layout over the pull, and the spike duty each bot held", show_spikes),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
