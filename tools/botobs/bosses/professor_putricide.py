#!/usr/bin/env python3
"""Read a Professor Putricide pull from a RaidObs trace: phases, Gaseous Bloat, the skull, cheats.

    professor_putricide.py <file>            every section
    professor_putricide.py <file> --phases   pp.phase, 1 to 3
    professor_putricide.py <file> --bloat    each Gaseous Bloat: victim, how long, how the flee went
    professor_putricide.py <file> --skull    every move of the skull, and the rule behind it
    professor_putricide.py <file> --cheats   this encounter's cheat uses

What the generic views miss here:

- **`pp.phase` comes from his health**, the thresholds his own DamageTaken hook tests: 1 above 80%,
  2 above 35%, 3 at 35% and under. It is per bot, so a step starts at the first bot to note it.
- **`pp.bloatflee` is per bot**, the search branch that produced the bloated bot's flee leg: `corner`
  (an open spot after a wall check), `wedge` (the most open spot when nothing passed), `open` (the
  scored search), `loose` (its relaxed pass), `straight` (straight away from the cloud), `none` once the
  bloat is gone. Change-only, so counts are switches, not legs.
- **`pp.skull` is written only by the bot that moved the icon**: `ooze`, `cloud` or `boss` and the
  unit, or `clear`. A bot repeating its own last placement writes nothing.
- **Gaseous Bloat is row 2176** (70672/72455/72832/72833), the debuff the Gas Cloud chases.
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
from raidobs.probes import emitted_keys, latch_spans, resolve_guids, silent_keys  # noqa: E402
from raidobs.trace import Trace, clock, first_deaths, notes  # noqa: E402

ENCOUNTER = "professor-putricide"
TITLE = "Professor Putricide"
GASEOUS_BLOAT = (70672, 72455, 72832, 72833)
FLEE_RULES = ("corner", "wedge", "open", "loose", "straight")


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
    gone = [key for key, _, _ in silent_keys(emitted_keys(trace), ENCOUNTER)]
    print(f"probes absent from this trace: {', '.join(gone)}" if gone else "all pp.* probes present")


def show_phases(trace: Trace) -> None:
    if refuse(trace, "PHASES"):
        return
    steps = icc.phase_steps(trace)
    if not steps:
        print("  pp.phase never written")
        return
    print("  pp.phase, each step from the first bot to note it\n")
    print(f"    {'from':>9}  {'held':>9}  phase")
    for value, start, stop in steps:
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {value}")


def bloat_windows(trace: Trace) -> list[dict]:
    """Per Gaseous Bloat: `victim`, `start`, `stop` (the aura's removal, else the pull's end), whether
    the victim `died` inside it, and `flee`, the count of each pp.bloatflee branch it noted inside."""
    end = icc.pull_end(trace)
    open_at: dict[int, int] = {}
    windows = []
    for rec in trace.of("aura"):
        if rec.get("sp") not in GASEOUS_BLOAT:
            continue
        victim = rec["d"]
        if not rec.get("r"):
            open_at.setdefault(victim, rec["t"])
        elif victim in open_at:
            windows.append((victim, open_at.pop(victim), rec["t"]))
    windows += [(victim, start, end) for victim, start in open_at.items()]

    dead = first_deaths(trace)
    flees = notes(trace, "pp.bloatflee")
    rows = []
    for victim, start, stop in sorted(windows, key=lambda window: window[1]):
        counts = collections.Counter(rec.get("txt") for rec in flees
                                     if rec["g"] == victim and start <= rec["t"] <= stop
                                     and rec.get("txt") in FLEE_RULES)
        rows.append({"victim": victim, "start": start, "stop": stop,
                     "died": start <= dead.get(victim, -1) <= stop, "flee": counts})
    return rows


def show_bloat(trace: Trace) -> None:
    if refuse(trace, "BLOAT"):
        return
    rows = bloat_windows(trace)
    if not rows:
        print("  no Gaseous Bloat landed")
        return
    print(f"  {len(rows)} bloat(s); flee counts the branch switches pp.bloatflee noted inside each\n")
    for row in rows:
        flee = ", ".join(f"{rule} {row['flee'][rule]}" for rule in FLEE_RULES if row["flee"][rule]) or "-"
        died = "  died" if row["died"] else ""
        human = " human" if row["victim"] in trace.humans else ""
        print(f"    {clock(row['start']):>9}  {clock(row['stop'] - row['start']):>9}  "
              f"{trace.name(row['victim'])}{human}  {flee}{died}")


def show_skull(trace: Trace) -> None:
    if refuse(trace, "SKULL"):
        return
    spans = latch_spans(trace, "pp.skull", icc.pull_end(trace))
    if not spans:
        print("  pp.skull never written")
        return
    print("  each placement, merged across the bots that made it\n")
    for value, start, stop in spans:
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {resolve_guids(trace, value)}")


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
    ("phases", "pp.phase, 1 to 3", show_phases),
    ("bloat", "each Gaseous Bloat: victim, length, flee branches", show_bloat),
    ("skull", "every move of the skull and its rule", show_skull),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
