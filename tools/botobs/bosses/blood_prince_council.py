#!/usr/bin/env python3
"""Read a Blood Prince Council pull from a RaidObs trace: the empowered prince and every bot duty.

    blood_prince_council.py <file>             every section
    blood_prince_council.py <file> --phases    bpc.phase, the prince Invocation of Blood is on
    blood_prince_council.py <file> --targets   per bot: the resolver branches it was handed
    blood_prince_council.py <file> --tanks     per tank: the princes it owned
    blood_prince_council.py <file> --spread    per bot: the lattice nodes it held, its vortex dodges
    blood_prince_council.py <file> --bombs     per duty bot: the Kinetic Bombs it juggled, its shots
    blood_prince_council.py <file> --balls     per bot: ball victim steps and Inferno Flame soaks
    blood_prince_council.py <file> --nucleus   per tank: Keleseth's tank collecting Dark Nuclei
    blood_prince_council.py <file> --defensives  tank buttons and calls to a human tank
    blood_prince_council.py <file> --cheats    this encounter's cheat uses

What the generic views miss here:

- **`bpc.phase` is the empowered prince**: `valanar` from the pull, then `keleseth` or `taldaram` at
  random, then the other, then Valanar again, moving at 45 s and every 46 s after; `none` while the
  Invocation flies to the next one. Per bot, so a step starts at the first bot to note it.
- **`bpc.target` is the resolver branch**: `owned` (a tank on its own prince), `recover` (its prince
  is on somebody else), `bomb`, `skull`, `empowered`, `none`.
- **`bpc.tank` is a bit mask per tank**: 1 Valanar, 2 Keleseth, 4 Taldaram; `0` when it lost them.
- **`bpc.slot` is a lattice node per bot**, 1-41, `0` when it gave it up. Healers keep theirs, the
  ranged follow the empowered prince's side, everyone else holds one only through Empowered Shock
  Vortex. `bpc.dodge` is a bot without a node stepping off a Shock Vortex: `melee`, `clear`,
  `blocked` (no clear spot within 20 yd).
- **`bpc.bomb` is the bomb a duty bot juggles**, `0` once it is gone; `bpc.juggle` `shoot` while the
  bomb falls low with nobody's hit on it, `wait` otherwise.
- **`bpc.ball` is a ball's victim**: `plain` or `inferno`, then `clear` (nobody in reach of the
  explosion), `step` (walking clear) or `none` (no clear spot). `bpc.soak` is `1` on a soaker of the
  live Ball of Inferno Flame.
- **`bpc.nucleus` is Keleseth's tank**: `taunt N` with N Shadow Resonance stacks, `walk` toward one
  out of reach, `cooldown`, `full` at six, `none` with no loose nucleus.
- **`bpc.defensive` is the tank ladder's pick**, a cast name, `idle` outside a window: Keleseth's tank
  through his empowered Shadow Lance, any tank under 40%. `bpc.humancall` is a call to a human tank.
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
from raidobs.probes import emitted_keys, holder_spans, silent_keys  # noqa: E402
from raidobs.trace import Trace, clock, notes, roster_guids  # noqa: E402

ENCOUNTER = "blood-prince-council"
TITLE = "Blood Prince Council"
PRINCES = ((1, "valanar"), (2, "keleseth"), (4, "taldaram"))
TARGET_BRANCHES = ("owned", "recover", "bomb", "skull", "empowered", "none")
DEFENSIVE = "bpc.defensive"
HUMAN_CALL = "bpc.humancall"
# a button counts for a window from this long before it opens until this long after it closes
WINDOW_LEAD_MS = 1500
WINDOW_TAIL_MS = 1500
# cast names, any rank
TANK_BUTTONS = frozenset({"Last Stand", "Shield Wall", "Divine Protection", "Barkskin", "Frenzied Regeneration",
                          "Survival Instincts", "Anti-Magic Shell", "Vampiric Blood", "Icebound Fortitude"})


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


def value_counts(trace: Trace, key: str, word_only: bool = False) -> dict[int, collections.Counter]:
    """Per guid: how many spans each value of `key` started, by its first word when `word_only`."""
    rows: dict[int, collections.Counter] = {}
    for guid, spans in holder_spans(trace, key, icc.pull_end(trace)).items():
        counts: collections.Counter = collections.Counter()
        for value, _, _ in spans:
            counts[value.split(" ")[0] if word_only else value] += 1
        rows[guid] = counts
    return rows


def by_name(trace: Trace, rows: dict) -> list:
    return sorted(rows.items(), key=lambda item: trace.name(item[0]))


def counted(counts: collections.Counter, order: tuple[str, ...] = ()) -> str:
    keys = [key for key in order if counts[key]] + sorted(key for key in counts if key not in order)
    return ", ".join(f"{key} {counts[key]}" for key in keys) or "-"


def show_targets(trace: Trace) -> None:
    if refuse(trace, "TARGETS"):
        return
    rows = value_counts(trace, "bpc.target")
    if not rows:
        print("  bpc.target never written")
        return
    print("  per bot: spans of each resolver branch\n")
    for guid, counts in by_name(trace, rows):
        print(f"    {trace.name(guid)}  {counted(counts, TARGET_BRANCHES)}")


def prince_names(mask: str) -> str:
    bits = int(mask) if mask.isdigit() else 0
    return "+".join(name for bit, name in PRINCES if bits & bit) or "none"


def tank_rows(trace: Trace) -> list[dict]:
    """Per tank that noted bpc.tank: every mask it held in order, as prince names, with its start."""
    rows = []
    for guid, spans in holder_spans(trace, "bpc.tank", icc.pull_end(trace)).items():
        rows.append({"guid": guid, "held": [(prince_names(value), start) for value, start, _ in spans]})
    return sorted(rows, key=lambda row: trace.name(row["guid"]))


def show_tanks(trace: Trace) -> None:
    if refuse(trace, "TANKS"):
        return
    rows = tank_rows(trace)
    if not rows:
        print("  bpc.tank never written")
        return
    print("  per tank: the princes it owned, from when\n")
    for row in rows:
        held = ", ".join(f"{names} {clock(start)}" for names, start in row["held"])
        print(f"    {trace.name(row['guid'])}  {held}")


def spread_rows(trace: Trace) -> list[dict]:
    """Per bot: `nodes` (distinct nodes held), `moves` (node to another node), `released` (back to 0)
    and `dodges` (bpc.dodge spans other than none, by value)."""
    dodges = value_counts(trace, "bpc.dodge")
    rows = {}
    for guid, spans in holder_spans(trace, "bpc.slot", icc.pull_end(trace)).items():
        nodes = {value for value, _, _ in spans if value != "0"}
        moves = released = 0
        last = None
        for value, _, _ in spans:
            if value == "0":
                released += last is not None
                last = None
                continue
            moves += last is not None and last != value
            last = value
        rows[guid] = {"guid": guid, "nodes": len(nodes), "moves": moves, "released": released}
    for guid in dodges:
        rows.setdefault(guid, {"guid": guid, "nodes": 0, "moves": 0, "released": 0})
    for guid, row in rows.items():
        counts = dodges.get(guid, collections.Counter())
        row["dodges"] = collections.Counter({key: n for key, n in counts.items() if key != "none"})
    return sorted(rows.values(), key=lambda row: trace.name(row["guid"]))


def show_spread(trace: Trace) -> None:
    if refuse(trace, "SPREAD"):
        return
    rows = spread_rows(trace)
    if not rows:
        print("  bpc.slot and bpc.dodge never written")
        return
    print("  per bot: nodes held, node-to-node moves, releases, vortex dodges\n")
    for row in rows:
        print(f"    {trace.name(row['guid'])}  nodes {row['nodes']}  moves {row['moves']}"
              f"  released {row['released']}  dodges {counted(row['dodges'])}")


def bomb_rows(trace: Trace) -> list[dict]:
    """Per duty bot: `bombs` (distinct bombs held), `switches` (one bomb straight to another) and
    `shots` (bpc.juggle `shoot` spans)."""
    shots = value_counts(trace, "bpc.juggle")
    rows = []
    for guid, spans in holder_spans(trace, "bpc.bomb", icc.pull_end(trace)).items():
        bombs = {value for value, _, _ in spans if value != "0"}
        switches = 0
        last = None
        for value, _, _ in spans:
            if value == "0":
                last = None
                continue
            switches += last is not None and last != value
            last = value
        rows.append({"guid": guid, "bombs": len(bombs), "switches": switches,
                     "shots": shots.get(guid, collections.Counter())["shoot"]})
    return sorted(rows, key=lambda row: (-row["bombs"], trace.name(row["guid"])))


def show_bombs(trace: Trace) -> None:
    if refuse(trace, "BOMBS"):
        return
    rows = bomb_rows(trace)
    if not rows:
        print("  bpc.bomb never written")
        return
    print("  per duty bot: bombs juggled, bomb-to-bomb switches, shot windows\n")
    for row in rows:
        print(f"    {trace.name(row['guid'])}  bombs {row['bombs']}  switches {row['switches']}  shots {row['shots']}")


def show_balls(trace: Trace) -> None:
    if refuse(trace, "BALLS"):
        return
    victims = value_counts(trace, "bpc.ball")
    soaks = value_counts(trace, "bpc.soak")
    if not victims and not soaks:
        print("  bpc.ball and bpc.soak never written")
        return
    print("  per bot: ball victim spans by kind and answer, Inferno Flame soaks\n")
    for guid in sorted(set(victims) | set(soaks), key=trace.name):
        soaked = soaks.get(guid, collections.Counter())["1"]
        print(f"    {trace.name(guid)}  {counted(victims.get(guid, collections.Counter()))}  soaks {soaked}")


def show_nucleus(trace: Trace) -> None:
    if refuse(trace, "NUCLEUS"):
        return
    rows = value_counts(trace, "bpc.nucleus", word_only=True)
    if not rows:
        print("  bpc.nucleus never written")
        return
    print("  per tank: spans of each nucleus answer\n")
    for guid, counts in by_name(trace, rows):
        print(f"    {trace.name(guid)}  {counted(counts, ('taunt', 'walk', 'cooldown', 'full', 'none'))}")


def casts_named(trace: Trace, names: frozenset) -> list[tuple[int, int, str]]:
    """`(t, guid, spell name)` for every roster cast of one of `names`, once per ms."""
    roster = roster_guids(trace)
    named = ((rec["t"], rec.get("s"), trace.spells.get(rec.get("sp"), "")) for rec in trace.of("cast")
             if rec.get("s") in roster)
    return sorted({cast for cast in named if cast[2] in names})


def tank_windows(trace: Trace) -> list[tuple[int, int, int, list[str]]]:
    """`(start, stop, tank, values)` per run of non-idle `bpc.defensive` values of one tank."""
    end = icc.pull_end(trace)
    out = []
    for guid, spans in holder_spans(trace, DEFENSIVE, end).items():
        run: list[tuple[str, int, int]] = []
        for span in spans + [("idle", end, end)]:
            if span[0] != "idle":
                run.append(span)
            elif run:
                out.append((run[0][1], run[-1][2], guid, [value for value, _, _ in run]))
                run = []
    return sorted(out)


def show_defensives(trace: Trace) -> None:
    if refuse(trace, "DEFENSIVES"):
        return
    windows = tank_windows(trace)
    if not windows:
        print(f"  {DEFENSIVE} never offered a button")
    casts = casts_named(trace, TANK_BUTTONS)
    for start, stop, tank, values in windows:
        low, high = start - WINDOW_LEAD_MS, stop + WINDOW_TAIL_MS
        spent = [cast for cast in casts if cast[1] == tank and low <= cast[0] <= high]
        offered = ", ".join(dict.fromkeys(values))
        cast = ", ".join(f"{name} {(when - start) / 1000:+.1f}s" for when, _, name in spent) or "none"
        print(f"  {clock(start)}  {trace.name(tank):<10} {clock(stop - start):>9}  offered {offered}; cast {cast}")
    calls = notes(trace, HUMAN_CALL)
    if calls:
        print("  calls to a human tank  " + ", ".join(f"{rec['txt']} {clock(rec['t'])}" for rec in calls))


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
    ("targets", "per bot: the resolver branches it was handed", show_targets),
    ("tanks", "per tank: the princes it owned", show_tanks),
    ("spread", "per bot: lattice nodes held and vortex dodges", show_spread),
    ("bombs", "per duty bot: Kinetic Bombs juggled and shots", show_bombs),
    ("balls", "per bot: ball victim steps and Inferno Flame soaks", show_balls),
    ("nucleus", "per tank: Dark Nucleus collection", show_nucleus),
    ("defensives", "tank buttons and calls to a human tank", show_defensives),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
