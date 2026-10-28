#!/usr/bin/env python3
"""Read a Lord Marrowgar pull from a RaidObs trace: Bone Storm, the duties, the spikes, the dodges.

    lord_marrowgar.py <file>            every section
    lord_marrowgar.py <file> --phases   lm.phase: each Bone Storm cast and storm, from the first bot to note it
    lord_marrowgar.py <file> --duties   who tanked, stacked, stood inside him and baited Coldflame
    lord_marrowgar.py <file> --storms   each storm's anchors, and who stood on an anchor or the stack
    lord_marrowgar.py <file> --spikes   the spike rule each bot followed
    lord_marrowgar.py <file> --dodges   the Coldflame dodge branches each bot took
    lord_marrowgar.py <file> --cheats   this encounter's cheat uses

What the generic views miss here:

- **`lm.duty` and `lm.anchor` are the roster, rebuilt once a second while he is in progress.** A value
  is the duty number (1 main tank, 2 off-tank, 3 inside him, 4 Coldflame bait) or the anchor slot (1-3
  for 0, 120 and 240 degrees round the storm stack); `0` is a bot that lost it, dead or gone.
- **`lm.stand` is the stand a bot holds, `none` while it holds none.** A melee or caster on a spike
  holds none, so reach can walk it; so does everyone while he walks between stands.
- **`lm.spike` is the rule that picked, not what was hit.** `skull` a player's mark, `melee` a spike in
  reach, `far` a ranged on a spike no melee reaches, `spread` a ranged helping on the others, `boss`
  him.
- **`lm.dodge` writes `clear` whenever a bot is out of every patch**, so a dodge is the row before it:
  `move N` (yards walked), `tight` (the wide clearance was full), `hold` (still walking), `none`,
  `locked`, `stunned`.
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
DUTY = "lm.duty"
ANCHOR = "lm.anchor"
STAND = "lm.stand"
SPIKE = "lm.spike"
DODGE = "lm.dodge"
DUTIES = {"1": "main tank", "2": "off-tank", "3": "inside", "4": "bait"}
ANCHOR_SLOTS = {"1": "0", "2": "120", "3": "240"}
STORM_STANDS = ("anchor", "stack")
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


def holders_at(trace: Trace, key: str, when: int) -> dict[int, str]:
    """`{guid: value}` for every bot whose `key` held a value other than `0` at `when`."""
    out = {}
    for guid, spans in holder_spans(trace, key, pull_end(trace)).items():
        for value, start, stop in spans:
            if start <= when < stop and value != "0":
                out[guid] = value
    return out


def anchors(trace: Trace, start: int, stop: int) -> dict[str, int]:
    """`{slot: guid}` for the anchors assigned when the storm in `[start, stop)` began."""
    return {ANCHOR_SLOTS.get(value, value): guid for guid, value in holders_at(trace, ANCHOR, start).items()}


def storm_stands(trace: Trace, start: int, stop: int) -> dict[str, set[int]]:
    """Per storm stand, the bots that held it at some point of the storm."""
    out: dict[str, set[int]] = collections.defaultdict(set)
    for guid, spans in holder_spans(trace, STAND, pull_end(trace)).items():
        for value, span_start, span_stop in spans:
            if value in STORM_STANDS and span_start < stop and span_stop > start:
                out[value].add(guid)
    return dict(out)


def held_values(trace: Trace, key: str, skip: tuple[str, ...] = ()) -> dict[str, set[int]]:
    """Per value of a per-bot key, the bots that held it at some point."""
    out: dict[str, set[int]] = collections.defaultdict(set)
    for guid, spans in holder_spans(trace, key, pull_end(trace)).items():
        for value, _, _ in spans:
            if value not in skip:
                out[value].add(guid)
    return dict(out)


def duties(trace: Trace) -> dict[str, set[int]]:
    return {DUTIES.get(value, value): guids for value, guids in held_values(trace, DUTY, ("0",)).items()}


def dodges(trace: Trace) -> dict[int, collections.Counter]:
    """Per bot, how often it took each dodge branch. `move N` counts under `move`."""
    out: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
    for rec in notes(trace, DODGE):
        value = str(rec.get("txt", ""))
        if value != "clear":
            out[rec["g"]][value.split(" ")[0]] += 1
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


def show_duties(trace: Trace) -> None:
    print("DUTIES")
    if not mine(trace):
        return
    held = duties(trace)
    if not held:
        print(f"  {DUTY} never written")
        return
    for value in sorted(held):
        print(f"  {value:<10} {names(trace, held[value])}")


def show_storms(trace: Trace) -> None:
    print("BONE STORMS")
    if not mine(trace):
        return
    spans = storms(trace)
    if not spans:
        print("  no Bone Storm in this pull")
        return
    for index, (start, stop) in enumerate(spans, 1):
        slots = anchors(trace, start, stop)
        drawn = ", ".join(f"{slot}: {trace.name(guid)}" for slot, guid in sorted(slots.items())) or "none"
        stood = storm_stands(trace, start, stop)
        print(f"  {index}  {clock(start)} for {clock(stop - start)}  anchors {drawn}")
        for stand in STORM_STANDS:
            print(f"       on the {stand}: {names(trace, stood.get(stand, ()))}")


def show_spikes(trace: Trace) -> None:
    print("SPIKES")
    if not mine(trace):
        return
    held = held_values(trace, SPIKE)
    if not held:
        print(f"  {SPIKE} never written")
        return
    for value in sorted(held):
        print(f"  {value:<8} {names(trace, held[value])}")


def show_dodges(trace: Trace) -> None:
    print("COLDFLAME DODGES")
    if not mine(trace):
        return
    rows = dodges(trace)
    if not rows:
        print("  no bot dodged")
        return
    for guid, counts in sorted(rows.items(), key=lambda row: trace.name(row[0])):
        line = "  ".join(f"{branch} {count}" for branch, count in sorted(counts.items()))
        print(f"  {trace.name(guid):<16} {line}")


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
    ("phases", "lm.phase: each Bone Storm cast and storm, from the first bot to note it", show_phases),
    ("duties", "who tanked, stacked, stood inside him and baited Coldflame", show_duties),
    ("storms", "each storm's anchors, and who stood on an anchor or the stack", show_storms),
    ("spikes", "the spike rule each bot followed", show_spikes),
    ("dodges", "the Coldflame dodge branches each bot took", show_dodges),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
