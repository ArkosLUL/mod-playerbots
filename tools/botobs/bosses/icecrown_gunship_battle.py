#!/usr/bin/env python3
"""Read an Icecrown Gunship Battle pull from a RaidObs trace: the freeze, the decks, the icons.

    icecrown_gunship_battle.py <file>           every section
    icecrown_gunship_battle.py <file> --phases  gsb.phase: fight, or frozen while Below Zero holds the cannons
    icecrown_gunship_battle.py <file> --decks   per bot, time on each deck and the trips across
    icecrown_gunship_battle.py <file> --icons   the mage skull window and the ranged star picks
    icecrown_gunship_battle.py <file> --cheats  this encounter's cheat uses

What the generic views miss here:

- **`gsb.deck` comes from the rocket-jump action, so a bot that never runs it writes nothing.** A bot
  in a cannon fires instead of reaching it, and shows `cannon` only on the ticks its cannon was cooling
  down. `between` is anywhere off both decks: mid-jump, the middle point, or falling.
- **`gsb.star` is written by the bot that moved the star.** `handoff` picked the add nearest the
  last star's spot, `fresh` the add nearest the picking bot, since no star had been placed yet; the
  guid after it is the add. A bot that found the star alive writes nothing.
- **A pull can file under the captain's name.** The credit is a spell, so without the boss-state table
  the trace takes `muradin-bronzebeard` or `high-overlord-saurfang`, which the aliases fold here.
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

SLUG = "icecrown-gunship-battle"
NAME = "Gunship Battle"
PHASE = "gsb.phase"
DECK = "gsb.deck"
SKULL = "gsb.skull"
STAR = "gsb.star"
DECKS = ("friendly", "between", "enemy", "cannon")
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


def deck_rows(trace: Trace) -> dict[int, dict]:
    """Per bot: ms held on each deck value, and `trips`, its spans on the enemy deck."""
    out = {}
    for guid, spans in holder_spans(trace, DECK, pull_end(trace)).items():
        held = collections.Counter()
        for value, start, stop in spans:
            held[value] += stop - start
        out[guid] = {"held": dict(held), "trips": sum(1 for value, _, _ in spans if value == "enemy")}
    return out


def star_picks(trace: Trace) -> collections.Counter:
    """Star moves per rule: `handoff`, `fresh`, `none`."""
    return collections.Counter(str(rec.get("txt", "")).split(" ")[0] for rec in notes(trace, STAR))


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


def show_steps(key: str, trace: Trace) -> None:
    steps = latch_spans(trace, key, pull_end(trace))
    if not steps:
        print(f"  {key} never written")
        return
    print(f"    {'from':>9}  {'held':>9}  {key}")
    for value, start, stop in steps:
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {value}")


def show_phases(trace: Trace) -> None:
    print("PHASES")
    if mine(trace):
        show_steps(PHASE, trace)


def show_decks(trace: Trace) -> None:
    print("DECKS")
    if not mine(trace):
        return
    rows = deck_rows(trace)
    if not rows:
        print(f"  {DECK} never written")
        return
    print(f"  {'bot':<14}" + "".join(f"{value:>10}" for value in DECKS) + "  trips")
    for guid in sorted(rows, key=lambda guid: trace.name(guid)):
        held = rows[guid]["held"]
        cells = "".join(f"{clock(held.get(value, 0)):>10}" for value in DECKS)
        print(f"  {trace.name(guid):<14}{cells}  {rows[guid]['trips']:>5}")


def show_icons(trace: Trace) -> None:
    print("ICONS")
    if not mine(trace):
        return
    show_steps(SKULL, trace)
    picks = star_picks(trace)
    if picks:
        print("  star moves: " + ", ".join(f"{rule} {count}" for rule, count in sorted(picks.items())))
    else:
        print(f"  {STAR} never written")


def show_cheats(trace: Trace) -> None:
    print("CHEATS")
    if not mine(trace):
        return
    rows = own_cheats(trace)
    if not rows:
        print("  no gsb.* or icc.* cheat fired")
        return
    for key, entry in rows:
        print(f"  {key}  {entry['n']} use(s) from {clock(entry['first'])}  by {names(trace, entry['bots'])}")


SECTIONS = (
    ("phases", "gsb.phase: fight, or frozen while Below Zero holds the cannons", show_phases),
    ("decks", "per bot, time on each deck and the trips across", show_decks),
    ("icons", "the mage skull window and the ranged star picks", show_icons),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
