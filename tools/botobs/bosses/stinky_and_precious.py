#!/usr/bin/env python3
"""Read a Stinky or Precious pull from a RaidObs trace: the tanks, swaps, picks, Decimate, cheats.

    stinky_and_precious.py <file>               every section
    stinky_and_precious.py <file> --tanks       per dog: who it hit, the swaps, peak Mortal Wound
    stinky_and_precious.py <file> --swaps       each swap election, its taunt, the yields after it
    stinky_and_precious.py <file> --targets     what the resolver had the bots on, by rule
    stinky_and_precious.py <file> --defensives  tank buttons, calls to a human tank, the cover after Decimate
    stinky_and_precious.py <file> --cheats      this encounter's cheat uses

What the generic views miss here:

- **The trace opens through `MarkPull`** once a bot and a dog are both in combat, filed under
  `stinky-and-precious` for either dog, so one file is one dog: Stinky stands in Festergut's room,
  Precious in Rotface's.
- **The swap is one elected taunt**: `dogs.swap` goes `1` for the tank taking a dog whose holder carries 6
  Mortal Wound (71127, one id on every difficulty), with 4 fewer of its own; the taunt is its
  `icc dogs swap` act row. `dogs.yield` then goes `1` on the relieved tank until its stacks drop.
- **`dogs.target` is the resolver's rule**: `dog`, `zombie` (a free tank on a zombie hitting a non-tank),
  `skull`, `none` for a yielding tank with no zombie.
- **`dogs.defensive` is the tank ladder's pick**, a cast name, `idle` outside a window: the holder through
  Decimate's cast, any tank under 40%. `icc.cover` deals the raid cover while the dog stuns itself after
  Decimate, everyone at 15%.
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
from raidobs.geometry import guids_of_entry, track  # noqa: E402
from raidobs.probes import holder_spans  # noqa: E402
from raidobs.trace import Trace, clock, notes, roster_guids  # noqa: E402

ENCOUNTER = "stinky-and-precious"
TITLE = "Stinky and Precious"
DOGS = {37025: "Stinky", 38064: "Stinky", 37217: "Precious", 38103: "Precious"}
SPELL_MORTAL_WOUND = 71127
SPELL_DECIMATE = 71123
MORTAL_WOUND_SWAP = 6
SWAP = "dogs.swap"
YIELD = "dogs.yield"
TARGET = "dogs.target"
DEFENSIVE = "dogs.defensive"
HUMAN_CALL = "dogs.humancall"
COVER = "icc.cover"
SWAP_ACTION = "icc dogs swap"
# a button counts for a window from this long before it opens until this long after it closes
WINDOW_LEAD_MS = 1500
WINDOW_TAIL_MS = 1500
TANK_BUTTONS = frozenset({"Last Stand", "Shield Wall", "Divine Protection", "Barkskin", "Frenzied Regeneration",
                          "Survival Instincts", "Anti-Magic Shell", "Vampiric Blood", "Icebound Fortitude"})
NO_PICK = {"none", "idle", "covered", "channel", "free", ""}


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
    if icc.is_icc(trace) and not mine(trace):
        print(f"  not a {TITLE} pull, every section below reads empty")


def victim_runs(trace: Trace, guid: int) -> list[tuple[int, int, int]]:
    """`(victim, start, stop)` per run of a unit's target column, 0 for no target."""
    runs: list[list[int]] = []
    for when, target in track(trace, {guid}, ("t", "target")).get(guid, []):
        if runs and runs[-1][0] == target:
            runs[-1][2] = when
            continue
        if runs:
            runs[-1][2] = when
        runs.append([target, when, when])
    return [tuple(run) for run in runs]


def mortal_wound_peaks(trace: Trace) -> dict[int, int]:
    peaks: dict[int, int] = collections.defaultdict(int)
    for rec in trace.of("aura"):
        if rec.get("sp") == SPELL_MORTAL_WOUND and not rec.get("r"):
            peaks[rec["d"]] = max(peaks[rec["d"]], int(rec.get("st") or 1))
    return dict(peaks)


def dog_rows(trace: Trace) -> list[dict]:
    """Per dog sampled: `guid`, `name`, its target `runs` and the `swaps` between tanks."""
    rows = []
    for entry, name in DOGS.items():
        for guid in sorted(guids_of_entry(trace, entry)):
            runs = [run for run in victim_runs(trace, guid) if run[0]]
            tanks = [victim for victim, _, _ in runs if trace.role(victim) == "tank"]
            swaps = sum(1 for before, after in zip(tanks, tanks[1:]) if before != after)
            rows.append({"guid": guid, "name": name, "runs": runs, "swaps": swaps})
    return rows


def show_tanks(trace: Trace) -> None:
    if refuse(trace, "TANKS"):
        return
    rows = dog_rows(trace)
    if not rows:
        print("  no dog sampled")
        return
    for row in rows:
        print(f"  {row['name']}: who it hit, each run; {row['swaps']} swap(s) between tanks\n")
        for victim, start, stop in row["runs"]:
            print(f"    {clock(start):>9}  {clock(stop - start):>9}  {trace.name(victim)} ({trace.role(victim)})")
        print()
    peaks = mortal_wound_peaks(trace)
    if peaks:
        print(f"  Mortal Wound peak, the swap is at {MORTAL_WOUND_SWAP}: " + ", ".join(
            f"{trace.name(guid)} {stacks}" for guid, stacks in sorted(peaks.items(), key=lambda item: -item[1])))


def elections(trace: Trace) -> list[tuple[int, int, int | None]]:
    """`(t, elected tank, t of its swap taunt or None)` per time dogs.swap went 1."""
    end = icc.pull_end(trace)
    taunts = sorted((rec["t"], rec.get("g")) for rec in trace.of("act")
                    if rec.get("a") == SWAP_ACTION and rec.get("vd") == "OK")
    out = []
    for guid, spans in holder_spans(trace, SWAP, end).items():
        for value, start, stop in spans:
            if value != "1":
                continue
            taunt = next((when for when, who in taunts if who == guid and start <= when <= stop + 500), None)
            out.append((start, guid, taunt))
    return sorted(out)


def yields(trace: Trace) -> list[tuple[int, int, int]]:
    """`(start, stop, tank)` per span dogs.yield held 1."""
    end = icc.pull_end(trace)
    return sorted((start, stop, guid) for guid, spans in holder_spans(trace, YIELD, end).items()
                  for value, start, stop in spans if value == "1")


def show_swaps(trace: Trace) -> None:
    if refuse(trace, "SWAPS"):
        return
    rows = elections(trace)
    if not rows:
        print(f"  {SWAP} never elected a tank")
    for when, guid, taunt in rows:
        landed = f"taunted {(taunt - when) / 1000:.1f} s after" if taunt is not None else "no taunt"
        print(f"  {clock(when)}  {trace.name(guid)} elected, {landed}")
    for start, stop, guid in yields(trace):
        print(f"  {clock(start)}  {trace.name(guid)} yielded {clock(stop - start)}")


def target_time(trace: Trace) -> dict[str, tuple[int, int]]:
    """Per rule: `(bots, ms held summed over bots)`."""
    bots: dict[str, set[int]] = collections.defaultdict(set)
    held: dict[str, int] = collections.defaultdict(int)
    for guid, spans in holder_spans(trace, TARGET, icc.pull_end(trace)).items():
        for value, start, stop in spans:
            bots[value].add(guid)
            held[value] += stop - start
    return {value: (len(bots[value]), held[value]) for value in held}


def show_targets(trace: Trace) -> None:
    if refuse(trace, "TARGETS"):
        return
    rows = target_time(trace)
    if not rows:
        print(f"  {TARGET} never written")
        return
    for value, (bots, held) in sorted(rows.items(), key=lambda row: -row[1][1]):
        print(f"  {value:<8} {bots:>3} bot(s)  {clock(held):>9} held in all")


def casts_named(trace: Trace, names: frozenset) -> list[tuple[int, int, str]]:
    """`(t, guid, spell name)` for every roster cast of one of `names`, once per ms."""
    roster = roster_guids(trace)
    named = ((rec["t"], rec.get("s"), trace.spells.get(rec.get("sp"), "")) for rec in trace.of("cast")
             if rec.get("s") in roster)
    return sorted({cast for cast in named if cast[2] in names})


def tank_windows(trace: Trace) -> list[tuple[int, int, int, list[str]]]:
    """`(start, stop, tank, values)` per run of non-idle `dogs.defensive` values of one tank."""
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
    for when in sorted({rec["t"] for rec in trace.of("cast") if rec.get("sp") == SPELL_DECIMATE}):
        covers = [rec for rec in notes(trace, COVER)
                  if when <= rec["t"] <= when + 4000 and str(rec.get("txt", "")).lower() not in NO_PICK]
        picks = ", ".join(f"{rec['txt']} {trace.name(rec.get('g'))}" for rec in covers) or "no cover"
        print(f"  Decimate {clock(when)}: {picks}")


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
    ("tanks", "per dog: who it hit, swaps, peak Mortal Wound", show_tanks),
    ("swaps", "each swap election, its taunt, the yields after it", show_swaps),
    ("targets", "what the resolver had the bots on, by rule", show_targets),
    ("defensives", "tank buttons, calls to a human tank, the cover after Decimate", show_defensives),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
