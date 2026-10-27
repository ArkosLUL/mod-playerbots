#!/usr/bin/env python3
"""Read a Sindragosa gauntlet pull from a RaidObs trace: its waves of adds, cheats.

    sindragosa_gauntlet.py <file>           every section
    sindragosa_gauntlet.py <file> --waves   each kind of add: how many came, first and last sampled
    sindragosa_gauntlet.py <file> --cheats  cheat uses in this pull

The gauntlet has no phase and no probe of its own: no ICC node runs it, and every `sg.*` key is
Sindragosa's. What the generic views miss here:

- **Three waves, each once only Nerub'ar Broodlings are left**: the Nerub'ar Champions and Webweavers
  waiting on the webs drop first, Frostwarden Warriors and Sorceresses walk in second, a fresh set of
  spiders drops third. Broodlings come with every wave. The controller dies after the third, which
  is the `DONE`.
- **Spiders split on the first Frostwarden sighting**, so the first and third waves read apart. The
  first wave's spiders hang on the webs from the reset, so some may be sampled before the pull.
"""
from __future__ import annotations

import pathlib
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from bosses import icecrown_citadel as icc  # noqa: E402
from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.geometry import guids_of_entry, track  # noqa: E402
from raidobs.trace import Trace, clock  # noqa: E402

ENCOUNTER = "sindragosa-gauntlet"
TITLE = "Sindragosa gauntlet"
SPIDERS = (37501, 37502)
FROSTWARDENS = (37228, 37229)
BROODLINGS = (37232,)


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
    if not icc.is_icc(trace):
        return
    if not mine(trace):
        print(f"  not a {TITLE} pull, every section below reads empty")
        return
    print("no probes of its own: sg.* keys are Sindragosa's")


def sampled(trace: Trace, entries: tuple[int, ...]) -> dict[int, tuple[int, int]]:
    """guid -> (first, last) sample, for every unit of these entries the snapshots carried."""
    guids = set().union(*(guids_of_entry(trace, entry) for entry in entries))
    return {guid: (rows[0][0], rows[-1][0]) for guid, rows in track(trace, guids, cols=("t",)).items() if rows}


def wave_rows(trace: Trace) -> list[tuple[str, int, int, int]]:
    """`(kind, units, first, last)` per kind of add sampled, spiders split at the first Frostwarden."""
    wardens = sampled(trace, FROSTWARDENS)
    split = min((first for first, _ in wardens.values()), default=None)
    spiders = sampled(trace, SPIDERS)
    groups = [
        ("spiders, first wave", {g: s for g, s in spiders.items() if split is None or s[0] < split}),
        ("frostwardens", wardens),
        ("spiders, third wave", {g: s for g, s in spiders.items() if split is not None and s[0] >= split}),
        ("broodlings", sampled(trace, BROODLINGS)),
    ]
    return [(kind, len(units), min(first for first, _ in units.values()), max(last for _, last in units.values()))
            for kind, units in groups if units]


def show_waves(trace: Trace) -> None:
    if refuse(trace, "WAVES"):
        return
    rows = wave_rows(trace)
    if not rows:
        print("  no gauntlet add sampled")
        return
    print(f"    {'kind':<20}  {'units':>5}  {'first':>9}  {'last':>9}")
    for kind, units, first, last in rows:
        print(f"    {kind:<20}  {units:>5}  {clock(first):>9}  {clock(last):>9}")


def show_cheats(trace: Trace) -> None:
    if refuse(trace, "CHEATS"):
        return
    if (trace.header.get("v") or 0) < icc.CHEAT_SCHEMA:
        print(f"  schema v{trace.header.get('v') or 0} predates the cheat record (v{icc.CHEAT_SCHEMA})")
        return
    rows = icc.cheat_rows(trace)
    if not rows:
        print("  no cheat fired")
        return
    print(f"  {sum(row['n'] for row in rows)} use(s), none from gauntlet code: icecrown_citadel.py --cheats")


SECTIONS = (
    ("waves", "each kind of add: how many came, first and last sampled", show_waves),
    ("cheats", "cheat uses in this pull", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
