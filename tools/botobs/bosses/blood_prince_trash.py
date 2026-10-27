#!/usr/bin/env python3
"""Read a Blood Prince trash pull from a RaidObs trace: the banner and any cheat that fired.

    blood_prince_trash.py <file>            every section
    blood_prince_trash.py <file> --cheats   cheat uses in this pull

What the generic views miss here:

- **A hook opens the trace, not the boss state.** Boss state 14 goes `NOT_STARTED` then `DONE` when
  the fourth of the Darkfallen Advisor, Archmage, Blood Knight and Noble guarding the Crimson Hall
  door dies, never `IN_PROGRESS`, and none of the trash is boss-flagged. So `ICCScripts.cpp` calls
  `MarkPull` when any of those four entries engages a player. The `DONE` closes the pull that kills
  the last guard as `kill`; any other closes `idle`.
- **The strategy has no trash code**, so the trash publishes no phase and no probe. `bpc` is the
  council's prefix too: a `bpc.*` key here is council code running in a trash pull.
"""
from __future__ import annotations

import pathlib
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from bosses import icecrown_citadel as icc  # noqa: E402
from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.trace import Trace, clock  # noqa: E402

ENCOUNTER = "blood-prince-trash"
TITLE = "Blood Prince trash"


def mine(trace: Trace) -> bool:
    return encounter_of(trace) == ENCOUNTER


def show_banner(trace: Trace) -> None:
    icc.show_banner(trace)
    if icc.is_icc(trace) and not mine(trace):
        print(f"  not a {TITLE} pull, every section below reads empty")


def show_cheats(trace: Trace) -> None:
    print("CHEATS")
    if not mine(trace):
        print(f"  not a {TITLE} pull")
        return
    if (trace.header.get("v") or 0) < icc.CHEAT_SCHEMA:
        print(f"  schema v{trace.header.get('v') or 0} predates the cheat record (v{icc.CHEAT_SCHEMA})")
        return
    rows = icc.cheat_rows(trace)
    if not rows:
        print("  no cheat fired")
    for row in rows:
        tag = "" if row["own"] else "  other encounter"
        print(f"  {row['key']}  {row['n']} use(s) from {clock(row['first'])}{tag}")
        print(f"    by  {icc.ranked(trace, row['bots'])}")
        print(f"    on  {icc.named(trace, row['targets'])}")


SECTIONS = (
    ("cheats", "cheat uses in this pull", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
