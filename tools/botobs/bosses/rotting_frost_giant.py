#!/usr/bin/env python3
"""Read a Rotting Frost Giant pull from a RaidObs trace: Death Plague on the raid, and the cheat strip.

    rotting_frost_giant.py <file>           every section
    rotting_frost_giant.py <file> --plague  each player's infections, doubles, cures and deaths carrying it
    rotting_frost_giant.py <file> --cheats  this encounter's cheat uses

The giant has no phase and no assignment to publish. The pull opens through `MarkPull` once a bot and
the giant are both in combat. What the generic views miss here:

- **A double is two Death Plagues from two casters.** The giant casts one on a random player without
  Recently Infected, whether or not it already carries one, and a spread one is cast by the carrier
  on its neighbour; auras from different casters stack, so both tick.
- **With the raid cheat on, a bot's plague is stripped the tick it lands.** `rfg.deathplague` counts
  those strips; an infection with a removal a few ms later is one of them, not a cure.
"""
from __future__ import annotations

import collections
import pathlib
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of, prefix_matches_boss  # noqa: E402
from raidobs.trace import Trace, cheat_uses, clock, combat_deaths  # noqa: E402

SLUG = "rotting-frost-giant"
NAME = "Rotting Frost Giant"
SPELL_DEATH_PLAGUE = 72865
SPELL_RECENTLY_INFECTED = 72884
# A held aura is stamped removed at the death's own time.
HELD_TO_DEATH_MS = 50
DIFFICULTIES = {0: "10N", 1: "25N", 2: "10H", 3: "25H"}


def pull_end(trace: Trace) -> int:
    ends = trace.of("end")
    if ends:
        return ends[-1]["t"]
    return trace.records[-1].get("t", 0) if trace.records else 0


def mine(trace: Trace) -> bool:
    return encounter_of(trace) == SLUG


def plague_rows(trace: Trace) -> dict[int, dict]:
    """Per player: `infected` applications, `doubles` landing beside another caster's, `cured` Recently
    Infected applications, `died` deaths while carrying it."""
    out: dict[int, dict] = collections.defaultdict(lambda: {"infected": 0, "doubles": 0, "cured": 0, "died": 0})
    holding: dict[int, set[int]] = collections.defaultdict(set)
    for rec in sorted(trace.of("aura"), key=lambda rec: rec["t"]):
        target, caster = rec.get("d"), rec.get("s")
        if rec.get("sp") == SPELL_DEATH_PLAGUE:
            if rec.get("r"):
                holding[target].discard(caster)
                continue
            out[target]["infected"] += 1
            if holding[target] - {caster}:
                out[target]["doubles"] += 1
            holding[target].add(caster)
        elif rec.get("sp") == SPELL_RECENTLY_INFECTED and not rec.get("r"):
            out[target]["cured"] += 1
    for death in trace.of("death"):
        for row in death.get("auras") or []:
            if row[0] == SPELL_DEATH_PLAGUE and (row[5] == -1 or row[5] >= death["t"] - HELD_TO_DEATH_MS):
                out[death["g"]]["died"] += 1
                break
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


def show_plague(trace: Trace) -> None:
    print("DEATH PLAGUE")
    if not mine(trace):
        return
    rows = plague_rows(trace)
    if not rows:
        print("  nobody caught Death Plague")
        return
    print(f"  {'player':<14}{'infected':>9}{'doubles':>9}{'cured':>7}{'died':>6}")
    for guid in sorted(rows, key=lambda guid: trace.name(guid)):
        row = rows[guid]
        who = trace.name(guid) + (" (human)" if guid in trace.humans else "")
        print(f"  {who:<14}{row['infected']:>9}{row['doubles']:>9}{row['cured']:>7}{row['died']:>6}")


def show_cheats(trace: Trace) -> None:
    print("CHEATS")
    if not mine(trace):
        return
    rows = own_cheats(trace)
    if not rows:
        print("  no rfg.* or icc.* cheat fired")
        return
    for key, entry in rows:
        print(f"  {key}  {entry['n']} use(s) from {clock(entry['first'])}  by {names(trace, entry['bots'])}")


SECTIONS = (
    ("plague", "each player's infections, doubles, cures and deaths carrying it", show_plague),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
