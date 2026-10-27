#!/usr/bin/env python3
"""Read a Stinky or Precious pull from a RaidObs trace: who held each dog, Mortal Wound, cheats.

    stinky_and_precious.py <file>            every section
    stinky_and_precious.py <file> --tanks    per dog: who it hit, the swaps, peak Mortal Wound
    stinky_and_precious.py <file> --cheats   this encounter's cheat uses

The dogs publish no phase and no probe: who holds one is the snapshot's target column and a swap is a
taunt the act and cast streams already carry. What the generic views miss here:

- **The trace opens through `MarkPull`** once a bot and a dog are both in combat, filed under
  `stinky-and-precious` for either dog, so one file is one dog: Stinky stands in Festergut's room,
  Precious in Rotface's.
- **The tanks swap at 8 Mortal Wound** (71127, one id on every difficulty): the stacked tank stops
  attacking and the other taunts.
"""
from __future__ import annotations

import collections
import pathlib
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from bosses import icecrown_citadel as icc  # noqa: E402
from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.geometry import guids_of_entry, track  # noqa: E402
from raidobs.trace import Trace, clock  # noqa: E402

ENCOUNTER = "stinky-and-precious"
TITLE = "Stinky and Precious"
DOGS = {37025: "Stinky", 38064: "Stinky", 37217: "Precious", 38103: "Precious"}
SPELL_MORTAL_WOUND = 71127
MORTAL_WOUND_SWAP = 8


def mine(trace: Trace) -> bool:
    return encounter_of(trace) == ENCOUNTER


def refuse(trace: Trace, title: str) -> bool:
    print(title)
    if mine(trace):
        return False
    print(f"  not a {TITLE} pull")
    return True


def show_banner(trace: Trace) -> None:
    icc.show_banner(trace)
    if icc.is_icc(trace) and not mine(trace):
        print(f"  not a {TITLE} pull, every section below reads empty")


def victim_runs(trace: Trace, guid: int) -> list[tuple[int, int, int]]:
    """`(victim, start, stop)` per run of a unit's target column, 0 for no target."""
    runs: list[list[int]] = []
    for when, target in track(trace, {guid}, ("t", "target")).get(guid, []):
        if runs and runs[-1][0] == target:
            runs[-1][2] = when
            continue
        if runs:
            runs[-1][2] = when
        runs.append([target, when, when])
    return [tuple(run) for run in runs]


def mortal_wound_peaks(trace: Trace) -> dict[int, int]:
    peaks: dict[int, int] = collections.defaultdict(int)
    for rec in trace.of("aura"):
        if rec.get("sp") == SPELL_MORTAL_WOUND and not rec.get("r"):
            peaks[rec["d"]] = max(peaks[rec["d"]], int(rec.get("st") or 1))
    return dict(peaks)


def dog_rows(trace: Trace) -> list[dict]:
    """Per dog sampled: `guid`, `name`, its target `runs` and the `swaps` between tanks."""
    rows = []
    for entry, name in DOGS.items():
        for guid in sorted(guids_of_entry(trace, entry)):
            runs = [run for run in victim_runs(trace, guid) if run[0]]
            tanks = [victim for victim, _, _ in runs if trace.role(victim) == "tank"]
            swaps = sum(1 for before, after in zip(tanks, tanks[1:]) if before != after)
            rows.append({"guid": guid, "name": name, "runs": runs, "swaps": swaps})
    return rows


def show_tanks(trace: Trace) -> None:
    if refuse(trace, "TANKS"):
        return
    rows = dog_rows(trace)
    if not rows:
        print("  no dog sampled")
        return
    for row in rows:
        print(f"  {row['name']}: who it hit, each run; {row['swaps']} swap(s) between tanks\n")
        for victim, start, stop in row["runs"]:
            print(f"    {clock(start):>9}  {clock(stop - start):>9}  {trace.name(victim)} ({trace.role(victim)})")
        print()
    peaks = mortal_wound_peaks(trace)
    if peaks:
        print(f"  Mortal Wound peak, the swap is at {MORTAL_WOUND_SWAP}: " + ", ".join(
            f"{trace.name(guid)} {stacks}" for guid, stacks in sorted(peaks.items(), key=lambda item: -item[1])))


def show_cheats(trace: Trace) -> None:
    if refuse(trace, "CHEATS"):
        return
    if (trace.header.get("v") or 0) < icc.CHEAT_SCHEMA:
        print(f"  schema v{trace.header.get('v') or 0} predates the cheat record (v{icc.CHEAT_SCHEMA})")
        return
    rows = icc.cheat_rows(trace)
    own = [row for row in rows if row["own"]]
    if not own:
        print("  no cheat of this encounter fired")
    for row in own:
        print(f"  {row['key']}  {row['n']} use(s) from {clock(row['first'])}")
        print(f"    by  {icc.ranked(trace, row['bots'])}")
        print(f"    on  {icc.named(trace, row['targets'])}")
    others = sum(row["n"] for row in rows if not row["own"])
    if others:
        print(f"  {others} use(s) under other encounters' keys: icecrown_citadel.py --cheats")


SECTIONS = (
    ("tanks", "per dog: who it hit, swaps, peak Mortal Wound", show_tanks),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
