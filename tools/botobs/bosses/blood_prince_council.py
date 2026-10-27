#!/usr/bin/env python3
"""Read a Blood Prince Council pull from a RaidObs trace: the empowered prince, bombs, spread, skull, cheats.

    blood_prince_council.py <file>            every section
    blood_prince_council.py <file> --phases   bpc.phase, the prince Invocation of Blood is on
    blood_prince_council.py <file> --bombs    per ranged bot: the Kinetic Bombs it was given, and how
    blood_prince_council.py <file> --spread   per bot: its slot on the rings round Valanar
    blood_prince_council.py <file> --skull    every move of the skull onto the empowered prince
    blood_prince_council.py <file> --cheats   this encounter's cheat uses

What the generic views miss here:

- **`bpc.phase` is the empowered prince**: `valanar` from the pull, then `keleseth` or `taldaram` at
  random, then the other, moving every 46 s; `none` in the moment between. Per bot, so a step starts
  at the first bot to note it.
- **`bpc.bomb` is per ranged dps bot**: `hunter`, `druid` or `any` and the bomb, the tier of the greedy
  pass that gave it; `spare` when bombs are down but every one went to someone else; `none` while no
  bomb is low enough. A switch is one bomb straight to another, the assignment flipping under it.
- **`bpc.spread` is per bot, only while Valanar is empowered**: `vortex i/n` on the ring every non-tank
  takes during Empowered Shock Vortex, `ranged i/n` on the 25 yd ring ranged and healers hold outside
  it, `none` for melee outside it or once Valanar loses the Invocation. ` nudged` means a Shock Vortex
  pushed the slot round, ` blocked` that no angle within 180 degrees was clear.
- **`bpc.skull` is written only by the tank that moved the icon**, the prince and the unit it came
  off. The same tank making the very same move again later writes nothing.
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
from raidobs.probes import emitted_keys, holder_spans, latch_spans, resolve_guids, silent_keys  # noqa: E402
from raidobs.trace import Trace, clock  # noqa: E402

ENCOUNTER = "blood-prince-council"
TITLE = "Blood Prince Council"
BOMB = "bpc.bomb"
SPREAD = "bpc.spread"
SKULL = "bpc.skull"
BOMB_TIERS = ("hunter", "druid", "any")
RINGS = ("vortex", "ranged")


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
    print(f"probes absent from this trace: {', '.join(gone)}" if gone else "all bpc.* probes present")


def show_phases(trace: Trace) -> None:
    if refuse(trace, "PHASES"):
        return
    steps = icc.phase_steps(trace)
    if not steps:
        print("  bpc.phase never written")
        return
    print("  bpc.phase, each step from the first bot to note it\n")
    print(f"    {'from':>9}  {'held':>9}  empowered")
    for value, start, stop in steps:
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {value}")


def bomb_rows(trace: Trace) -> list[dict]:
    """Per bot that noted bpc.bomb: `tiers` (assignments per tier), `bombs` (distinct bombs given),
    `switches` (one bomb straight to another) and `spare` (spans with bombs down and none for it)."""
    rows = []
    for guid, spans in holder_spans(trace, BOMB, icc.pull_end(trace)).items():
        tiers: collections.Counter = collections.Counter()
        bombs: set[str] = set()
        switches = spare = 0
        last = None
        for value, _, _ in spans:
            tier, _, bomb = value.partition(" ")
            if tier in BOMB_TIERS:
                tiers[tier] += 1
                bombs.add(bomb)
                if last is not None and last != bomb:
                    switches += 1
                last = bomb
                continue
            spare += value == "spare"
            last = None
        rows.append({"guid": guid, "tiers": tiers, "bombs": len(bombs), "switches": switches, "spare": spare})
    return sorted(rows, key=lambda row: (-row["bombs"], trace.name(row["guid"])))


def show_bombs(trace: Trace) -> None:
    if refuse(trace, "BOMBS"):
        return
    rows = bomb_rows(trace)
    if not rows:
        print("  bpc.bomb never written")
        return
    print("  per ranged bot: assignments by tier, bombs given, bomb-to-bomb switches, spare spans\n")
    for row in rows:
        tiers = ", ".join(f"{tier} {row['tiers'][tier]}" for tier in BOMB_TIERS if row["tiers"][tier]) or "-"
        print(f"    {trace.name(row['guid'])}  {tiers}  bombs {row['bombs']}  switches {row['switches']}"
              f"  spare {row['spare']}")


def spread_rows(trace: Trace) -> list[dict]:
    """Per bot that noted bpc.spread: per ring, the slot spans and how many were `nudged` or `blocked`."""
    rows = []
    for guid, spans in holder_spans(trace, SPREAD, icc.pull_end(trace)).items():
        rings = {ring: collections.Counter() for ring in RINGS}
        for value, _, _ in spans:
            words = value.split()
            if words and words[0] in rings:
                rings[words[0]]["slots"] += 1
                for flag in words[2:]:
                    rings[words[0]][flag] += 1
        rows.append({"guid": guid, "rings": rings})
    return sorted(rows, key=lambda row: trace.name(row["guid"]))


def show_spread(trace: Trace) -> None:
    if refuse(trace, "SPREAD"):
        return
    rows = spread_rows(trace)
    if not rows:
        print("  bpc.spread never written")
        return
    print("  per bot and ring: slots taken, of them nudged off a Shock Vortex or blocked\n")
    for row in rows:
        parts = []
        for ring in RINGS:
            counts = row["rings"][ring]
            if counts["slots"]:
                parts.append(f"{ring} {counts['slots']} (nudged {counts['nudged']}, blocked {counts['blocked']})")
        print(f"    {trace.name(row['guid'])}  {'  '.join(parts) or 'none'}")


def show_skull(trace: Trace) -> None:
    if refuse(trace, "SKULL"):
        return
    spans = latch_spans(trace, SKULL, icc.pull_end(trace))
    if not spans:
        print("  bpc.skull never written")
        return
    print("  each placement, merged across the tanks that made it\n")
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
    ("phases", "bpc.phase, the empowered prince", show_phases),
    ("bombs", "per ranged bot: Kinetic Bomb assignments and switches", show_bombs),
    ("spread", "per bot: its slots on the rings round Valanar", show_spread),
    ("skull", "every move of the skull onto the empowered prince", show_skull),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
