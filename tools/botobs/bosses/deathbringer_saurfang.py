#!/usr/bin/env python3
"""Read a Deathbringer Saurfang pull from a RaidObs trace: the ranged grid, the skull, runes and marks.

    deathbringer_saurfang.py <file>           every section
    deathbringer_saurfang.py <file> --slots   each ranged bot's grid slot behind the tank spot
    deathbringer_saurfang.py <file> --skull   what the melee skull rule put the icon on
    deathbringer_saurfang.py <file> --runes   each Rune of Blood on a tank, and every Mark of the Fallen Champion
    deathbringer_saurfang.py <file> --cheats  this encounter's cheat uses

Saurfang has no phase key: the fight has no phase to publish, and the runes and marks are auras the
trace already carries on the players. What the generic views miss here:

- **A `dbs.slot` of `0` can be an erase.** The container writes `0` both for slot 0 and for a bot that
  gave its slot up as the 16th ranged, so the section reads each bot's first slot.
- **`dbs.skull` is the melee rule, by whichever melee ran it.** `beast` is the first live Blood Beast
  in the bot's target list, `boss` him once none is left.
"""
from __future__ import annotations

import pathlib
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of, prefix_matches_boss  # noqa: E402
from raidobs.probes import latch_spans  # noqa: E402
from raidobs.trace import Trace, cheat_uses, clock, combat_deaths, notes  # noqa: E402

SLUG = "deathbringer-saurfang"
NAME = "Deathbringer Saurfang"
SLOT = "dbs.slot"
SKULL = "dbs.skull"
SPELL_RUNE_OF_BLOOD = 72410
SPELL_MARK_OF_THE_FALLEN_CHAMPION = 72293
DIFFICULTIES = {0: "10N", 1: "25N", 2: "10H", 3: "25H"}


def pull_end(trace: Trace) -> int:
    ends = trace.of("end")
    if ends:
        return ends[-1]["t"]
    return trace.records[-1].get("t", 0) if trace.records else 0


def mine(trace: Trace) -> bool:
    return encounter_of(trace) == SLUG


def slots(trace: Trace) -> dict[int, int]:
    out: dict[int, int] = {}
    for rec in sorted(notes(trace, SLOT), key=lambda rec: rec["t"]):
        try:
            out.setdefault(rec["g"], int(rec.get("txt", "")))
        except ValueError:
            continue
    return out


def applications(trace: Trace, spell: int) -> list[tuple[int, int]]:
    return sorted((rec["t"], rec["d"]) for rec in trace.of("aura") if rec.get("sp") == spell and not rec.get("r"))


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


def show_slots(trace: Trace) -> None:
    print("SLOTS")
    if not mine(trace):
        return
    held = slots(trace)
    if not held:
        print(f"  {SLOT} never written")
        return
    for guid, slot in sorted(held.items(), key=lambda item: (item[1], trace.name(item[0]))):
        print(f"  {slot:>3}  {trace.name(guid)}")


def show_skull(trace: Trace) -> None:
    print("SKULL")
    if not mine(trace):
        return
    steps = latch_spans(trace, SKULL, pull_end(trace))
    if not steps:
        print(f"  {SKULL} never written")
        return
    for value, start, stop in steps:
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {value}")


def show_runes(trace: Trace) -> None:
    print("RUNES AND MARKS")
    if not mine(trace):
        return
    runes = applications(trace, SPELL_RUNE_OF_BLOOD)
    print(f"  Rune of Blood {len(runes)} time(s)")
    for when, tank in runes:
        print(f"    {clock(when)}  {trace.name(tank)}")
    marks = applications(trace, SPELL_MARK_OF_THE_FALLEN_CHAMPION)
    print(f"  Mark of the Fallen Champion {len(marks)} time(s): {names(trace, {guid for _, guid in marks})}")


def show_cheats(trace: Trace) -> None:
    print("CHEATS")
    if not mine(trace):
        return
    rows = own_cheats(trace)
    if not rows:
        print("  no dbs.* or icc.* cheat fired")
        return
    for key, entry in rows:
        print(f"  {key}  {entry['n']} use(s) from {clock(entry['first'])}  by {names(trace, entry['bots'])}")


SECTIONS = (
    ("slots", "each ranged bot's grid slot behind the tank spot", show_slots),
    ("skull", "what the melee skull rule put the icon on", show_skull),
    ("runes", "each Rune of Blood on a tank, and every Mark of the Fallen Champion", show_runes),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
