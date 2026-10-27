#!/usr/bin/env python3
"""Read a Rotface pull from a RaidObs trace: the Big Oozes, the explosion slots, Vile Gas, the skull.

    rotface.py <file>               every section
    rotface.py <file> --oozes       who the Big Oozes hit, and each tank's Big Ooze duty
    rotface.py <file> --explosion   Unstable Ooze Explosion casts and the escape slots
    rotface.py <file> --vilegas     heroic Vile Gas: each victim and the spot it ran to
    rotface.py <file> --skull       every move of the skull onto Rotface
    rotface.py <file> --cheats      this encounter's cheat uses

Rotface has no phase. What the generic views miss here:

- **`rf.bigooze` is per tank**: `pickup <ooze>` while walking to a Big Ooze not yet on it, `kite <ooze>`
  once every one is, `none` with no Big Ooze up, or for the main tank while the assist tank lives.
- **The Big Oozes' victims are the snapshot's target column.** Nothing fixates them, so a Big Ooze
  on a healer is threat the assist tank lost, and the share on a tank says how well it held them.
- **`rf.explosionslot` is a traced map**: a slot index per bot on a 40 yd ring, at most two bots a slot,
  kept between explosions. An erase is written as `0`, which is also a real slot.
- **`rf.vilegas` names the victim of Putricide's heroic Vile Gas cast**; `rf.vilegasspot` is the spot
  that bot then took: `offset N` 15 degree steps off its own bearing from the centre, `blocked` with no
  safe bearing, `none` when the window closes. A bot hit by the splash only writes the spot.
- **`rf.skull` is written only by the bot that moved the icon**, so a bot repeating its own last
  placement writes nothing.
"""
from __future__ import annotations

import pathlib
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from bosses import icecrown_citadel as icc  # noqa: E402
from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.geometry import guids_of_entry  # noqa: E402
from raidobs.probes import emitted_keys, holder_spans, resolve_guids, silent_keys  # noqa: E402
from raidobs.space import threat_share  # noqa: E402
from raidobs.trace import Trace, clock, notes  # noqa: E402

ENCOUNTER = "rotface"
TITLE = "Rotface"
BIG_OOZE = (36899, 38123)
SPELL_UNSTABLE_OOZE_EXPLOSION = 69839
VILE_GAS_WINDOW_MS = 8000
HEROIC = (2, 3)
HEROIC_ONLY = {"rf.vilegas", "rf.vilegasspot"}


def mine(trace: Trace) -> bool:
    return encounter_of(trace) == ENCOUNTER


def refuse(trace: Trace, title: str) -> bool:
    print(title)
    if mine(trace):
        return False
    print(f"  not a {TITLE} pull")
    return True


def heroic(trace: Trace) -> bool:
    return trace.header.get("diff") in HEROIC


def missing_probes(trace: Trace) -> list[str]:
    gone = [key for key, _, _ in silent_keys(emitted_keys(trace), ENCOUNTER)]
    return [key for key in gone if heroic(trace) or key not in HEROIC_ONLY]


def show_banner(trace: Trace) -> None:
    icc.show_banner(trace)
    if not icc.is_icc(trace):
        return
    if not mine(trace):
        print(f"  not a {TITLE} pull, every section below reads empty")
        return
    gone = missing_probes(trace)
    print(f"probes absent from this trace: {', '.join(gone)}" if gone else "all rf.* probes present")


def big_oozes(trace: Trace) -> set[int]:
    return set().union(*(guids_of_entry(trace, entry) for entry in BIG_OOZE))


def ooze_share(trace: Trace) -> dict[str, float]:
    """Percent of the Big Oozes' sampled time on each role."""
    held, _ = threat_share(trace, big_oozes(trace), lambda when: True)
    total = sum(held.values())
    return {role: span * 100.0 / total for role, span in held.most_common()} if total else {}


def duty_rows(trace: Trace) -> dict[int, list[tuple[str, int, int]]]:
    end = icc.pull_end(trace)
    return {guid: [(resolve_guids(trace, value), start, stop) for value, start, stop in spans]
            for guid, spans in holder_spans(trace, "rf.bigooze", end).items()}


def show_oozes(trace: Trace) -> None:
    if refuse(trace, "OOZES"):
        return
    share = ooze_share(trace)
    if share:
        print("  Big Oozes' time on target: " + ", ".join(f"{role} {pct:.1f}%" for role, pct in share.items()))
    else:
        print("  no Big Ooze sampled with a target")
    rows = duty_rows(trace)
    if not rows:
        print("  rf.bigooze never written")
        return
    print("\n  rf.bigooze per tank, each change")
    for guid, spans in sorted(rows.items(), key=lambda item: trace.name(item[0])):
        print(f"    {trace.name(guid)}")
        for value, start, stop in spans:
            print(f"      {clock(start):>9}  {clock(stop - start):>9}  {value}")


def show_explosion(trace: Trace) -> None:
    if refuse(trace, "EXPLOSION"):
        return
    oozes = big_oozes(trace)
    casts = [rec["t"] for rec in trace.of("cast")
             if rec.get("sp") == SPELL_UNSTABLE_OOZE_EXPLOSION and rec.get("s") in oozes]
    print(f"  {len(casts)} Unstable Ooze Explosion cast(s)" +
          (": " + ", ".join(clock(when) for when in casts) if casts else ""))
    slots = holder_spans(trace, "rf.explosionslot", icc.pull_end(trace))
    if not slots:
        print("  rf.explosionslot never written")
        return
    print("\n  rf.explosionslot per bot, each slot it held")
    for guid, spans in sorted(slots.items(), key=lambda item: trace.name(item[0])):
        print(f"    {trace.name(guid):<14}  " + ", ".join(f"{value}@{clock(start)}" for value, start, _ in spans))


def vile_gas_rows(trace: Trace) -> list[dict]:
    """Per cast: `t`, the `victim`, and the first spot the victim noted inside the window, or None."""
    spots = notes(trace, "rf.vilegasspot")
    rows = []
    for rec in notes(trace, "rf.vilegas"):
        victim = rec["g"]
        taken = next((spot.get("txt") for spot in spots
                      if spot["g"] == victim and rec["t"] <= spot["t"] < rec["t"] + VILE_GAS_WINDOW_MS
                      and spot.get("txt") != "none"), None)
        rows.append({"t": rec["t"], "victim": victim, "spot": taken})
    return rows


def show_vilegas(trace: Trace) -> None:
    if refuse(trace, "VILE GAS"):
        return
    if not heroic(trace):
        print("  normal mode: Putricide casts no Vile Gas on Rotface")
        return
    rows = vile_gas_rows(trace)
    if not rows:
        print("  rf.vilegas never written")
        return
    print(f"  {len(rows)} cast(s); spot is the first the victim took inside {VILE_GAS_WINDOW_MS // 1000} s\n")
    for row in rows:
        spot = row["spot"] or ("human" if row["victim"] in trace.humans else "-")
        print(f"    {clock(row['t']):>9}  {trace.name(row['victim']):<14}  {spot}")
    victims = {row["victim"] for row in rows}
    splashed = {rec["g"] for rec in notes(trace, "rf.vilegasspot") if rec["g"] not in victims}
    if splashed:
        print(f"\n  ran without being the target: {', '.join(sorted(trace.name(guid) for guid in splashed))}")


def show_skull(trace: Trace) -> None:
    if refuse(trace, "SKULL"):
        return
    rows = notes(trace, "rf.skull")
    if not rows:
        print("  rf.skull never written")
        return
    for rec in rows:
        print(f"    {clock(rec['t']):>9}  {trace.name(rec['g']):<14}  {resolve_guids(trace, str(rec.get('txt', '')))}")


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
    ("oozes", "who the Big Oozes hit, each tank's Big Ooze duty", show_oozes),
    ("explosion", "Unstable Ooze Explosion casts and escape slots", show_explosion),
    ("vilegas", "heroic Vile Gas victims and the spot each took", show_vilegas),
    ("skull", "every move of the skull onto Rotface", show_skull),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
