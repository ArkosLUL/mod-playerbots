#!/usr/bin/env python3
"""Read a Lady Deathwhisper pull from a RaidObs trace: the barrier phase, the skull, Dominate Mind.

    lady_deathwhisper.py <file>              every section
    lady_deathwhisper.py <file> --phases     ldw.phase: 1 while Mana Barrier holds, 2 once it breaks
    lady_deathwhisper.py <file> --skull      what the tanks' skull rule put the icon on
    lady_deathwhisper.py <file> --dominate   each Dominate Mind and whether a Cyclone followed it
    lady_deathwhisper.py <file> --cheats     this encounter's cheat uses

What the generic views miss here:

- **Phase 2 starts when the barrier breaks, not at 95% health.** The barrier tops her health up every
  second, so health stays full until her mana is gone; `ldw.phase` reads the aura.
- **`ldw.skull` is the icon's unit by rule, from whichever tank ran it.** `add` is a cultist near her,
  `boss` her, `other` a live unit the rule found already marked and left alone.
- **The Cyclone is the raid cheat.** It is `ldw.cyclone` in the cheat rows; a Cyclone aura on a
  dominated player within a second of the Dominate Mind, with the cheat off, would be a druid's.
"""
from __future__ import annotations

import pathlib
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of, prefix_matches_boss  # noqa: E402
from raidobs.probes import latch_spans  # noqa: E402
from raidobs.trace import Trace, cheat_uses, clock, combat_deaths  # noqa: E402

SLUG = "lady-deathwhisper"
NAME = "Lady Deathwhisper"
PHASE = "ldw.phase"
SKULL = "ldw.skull"
SPELL_DOMINATE_MIND = 71289
SPELL_CYCLONE = 33786
CYCLONE_WINDOW_MS = 1000
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


def dominations(trace: Trace) -> list[tuple[int, int, int | None]]:
    """`(t, victim, cyclone t or None)` per Dominate Mind landing."""
    applied = sorted((rec["t"], rec["d"]) for rec in trace.of("aura")
                     if rec.get("sp") == SPELL_DOMINATE_MIND and not rec.get("r"))
    cyclones = [(rec["t"], rec["d"]) for rec in trace.of("aura")
                if rec.get("sp") == SPELL_CYCLONE and not rec.get("r")]
    out = []
    for when, victim in applied:
        hit = next((at for at, guid in sorted(cyclones)
                    if guid == victim and when <= at <= when + CYCLONE_WINDOW_MS), None)
        out.append((when, victim, hit))
    return out


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


def show_steps(title: str, key: str, trace: Trace) -> None:
    print(title)
    if not mine(trace):
        return
    steps = latch_spans(trace, key, pull_end(trace))
    if not steps:
        print(f"  {key} never written")
        return
    print(f"    {'from':>9}  {'held':>9}  value")
    for value, start, stop in steps:
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {value}")


def show_phases(trace: Trace) -> None:
    show_steps("PHASES", PHASE, trace)


def show_skull(trace: Trace) -> None:
    show_steps("SKULL", SKULL, trace)


def show_dominate(trace: Trace) -> None:
    print("DOMINATE MIND")
    if not mine(trace):
        return
    rows = dominations(trace)
    if not rows:
        print("  nobody was dominated (10N casts none)")
        return
    for when, victim, cyclone in rows:
        human = " human" if victim in trace.humans else ""
        after = f"Cyclone {cyclone - when} ms after" if cyclone is not None else "no Cyclone"
        print(f"  {clock(when)}  {trace.name(victim)}{human}  {after}")


def show_cheats(trace: Trace) -> None:
    print("CHEATS")
    if not mine(trace):
        return
    rows = own_cheats(trace)
    if not rows:
        print("  no ldw.* or icc.* cheat fired")
        return
    for key, entry in rows:
        print(f"  {key}  {entry['n']} use(s) from {clock(entry['first'])}  by {names(trace, entry['bots'])}"
              f"  on {names(trace, entry['targets'] - {0}) if entry['targets'] - {0} else 'self'}")


SECTIONS = (
    ("phases", "ldw.phase: 1 while Mana Barrier holds, 2 once it breaks", show_phases),
    ("skull", "what the tanks' skull rule put the icon on", show_skull),
    ("dominate", "each Dominate Mind and whether a Cyclone followed it", show_dominate),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
