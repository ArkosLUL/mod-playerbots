#!/usr/bin/env python3
"""Read a Kologarn pull from a RaidObs trace: Focused Eyebeam runs, antechamber trash pulls, deaths.

    kologarn.py <file>            every section
    kologarn.py <file> --eyebeam  each eyebeam, whom it chased and whether the run left the walkway

What the generic views miss here:

- **The walkway is not a rectangle.** West of x 1750 it narrows to the door corridor (bots walk
  y -33..-15 there), and past the Shattered Walkway Door (x 1740.84) stairs drop 20 yd into the
  antechamber. A move to a point off the walkway still paths: down the stairs, toward the floor
  under that point.
- **The antechamber trash is not part of the encounter.** It stands out of aggro range below the
  stairs and joins only when a bot walks down to it, so the bot each pack first targeted, and where
  it stood, says who brought it.
- **The eyes chase one bot each wave.** An eye's victim is the bot it chases, so the run is read off
  that bot's track for the eye's 10 s life plus the walk that follows.
"""
from __future__ import annotations

import collections
import pathlib
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.geometry import frames  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, roster_guids  # noqa: E402

ENCOUNTER = "kologarn"

NPC_KOLOGARN = 32930
NPC_LEFT_ARM = 32933
NPC_RIGHT_ARM = 32934
NPC_RUBBLE = 33768
NPC_EYE_LEFT = 33632
NPC_EYE_RIGHT = 33802
NPC_ARMSWEEP = 33661
NPC_PIT_BUNNY = 33742
EYES = {NPC_EYE_LEFT, NPC_EYE_RIGHT}
# Hardened Iron Golem, Rune Etched Sentry, Chamber Overseer, Iron Mender, Lightning Charged Iron
# Dwarf, Displacement Device
TRASH = {34190, 34196, 34197, 34198, 34199, 34203}

SPELL_FOCUSED_EYEBEAM = (63347, 63976)

DOOR_X = 1740.84
WALKWAY_MIN_Z = 445.0
# One summon cast spawns both eyes, and their first samples land within a tick or two of each other.
EYE_PAIR_MS = 1500
# Eye life plus the walk a detour keeps going after it: the off-walkway runs crossed the door 5-7 s in.
RUN_WINDOW_MS = 15000
PACK_WINDOW_MS = 2000


def mine(trace: Trace) -> bool:
    return encounter_of(trace) == ENCOUNTER


def entry(trace: Trace, guid) -> int | None:
    return trace.entries.get(guid)


def off_walkway(x: float, z: float) -> bool:
    return x < DOOR_X or z < WALKWAY_MIN_Z


def show_banner(trace: Trace) -> None:
    print(f"Kologarn  {trace.path.name}")
    if not mine(trace):
        print("  not a Kologarn pull, every section below reads empty")
        return
    ends = trace.of("end")
    out = ends[-1].get("out", "?") if ends else "?"
    when = f" at {clock(ends[-1]['t'])}" if ends else ""
    print(f"  outcome {out}{when}, {len(combat_deaths(trace))} death(s)")


def eye_waves(trace: Trace) -> list[dict]:
    """Per summon: when the eyes were first sampled, their guids, and the bot they chased."""
    first: dict[int, int] = {}
    victim: dict[int, int] = {}
    for snap in frames(trace):
        for row in snap.get("u", []):
            if entry(trace, row[0]) not in EYES:
                continue
            first.setdefault(row[0], snap["t"])
            if len(row) > 7 and row[7] and row[0] not in victim:
                victim[row[0]] = row[7]
    waves: list[dict] = []
    for guid, t in sorted(first.items(), key=lambda item: item[1]):
        if waves and t - waves[-1]["t"] <= EYE_PAIR_MS:
            waves[-1]["eyes"].add(guid)
        else:
            waves.append({"t": t, "eyes": {guid}})
    for wave in waves:
        chased = {victim[guid] for guid in wave["eyes"] if guid in victim}
        wave["chased"] = chased.pop() if len(chased) == 1 else None
    return waves


def run_of(trace: Trace, guid: int, start: int) -> dict:
    """Where the chased bot went: its start, westmost x, and the first sample off the walkway."""
    run = {"start": None, "west": None, "left": None}
    for snap in frames(trace):
        if snap["t"] < start or snap["t"] > start + RUN_WINDOW_MS:
            continue
        for row in snap.get("u", []):
            if row[0] != guid:
                continue
            if run["start"] is None:
                run["start"] = (row[1], row[2], row[3])
            if run["west"] is None or row[1] < run["west"]:
                run["west"] = row[1]
            if run["left"] is None and off_walkway(row[1], row[3]):
                run["left"] = (snap["t"], row[1], row[2], row[3])
    return run


def beam_hits(trace: Trace, eyes: set, chased) -> tuple[int, int]:
    on_chased = on_others = 0
    for rec in trace.of("dmg"):
        if rec.get("s") in eyes and rec.get("sp") in SPELL_FOCUSED_EYEBEAM:
            if rec.get("d") == chased:
                on_chased += 1
            else:
                on_others += 1
    return on_chased, on_others


def show_eyebeam(trace: Trace) -> None:
    print("FOCUSED EYEBEAM")
    if not mine(trace):
        return
    waves = eye_waves(trace)
    if not waves:
        print("  no eyes sampled")
    left = started = 0
    for wave in waves:
        chased = wave["chased"]
        hits = beam_hits(trace, wave["eyes"], chased)
        line = f"  {clock(wave['t'])}  "
        if chased is None:
            print(f"{line}chased nobody the trace names, beam hits {hits[1]}")
            continue
        line += f"chased {trace.name(chased)} ({trace.role(chased)})"
        run = run_of(trace, chased, wave["t"])
        start = run["start"]
        if start:
            line += f" from ({start[0]:.0f}, {start[1]:.0f}, {start[2]:.0f})"
        if start and off_walkway(start[0], start[2]):
            line += ", already off the walkway"
        elif run["left"]:
            t, x, y, z = run["left"]
            line += f", LEFT the walkway at {clock(t)} ({x:.0f}, {y:.0f}, {z:.0f})"
            left += 1
            started += 1
        elif run["west"] is not None:
            line += f", stayed on the walkway, westmost x {run['west']:.0f}"
            started += 1
        print(f"{line}; beam hits on it {hits[0]}, on others {hits[1]}")
    print(f"  runs that left the walkway: {left} of {started} begun on it")


def trash_pulls(trace: Trace) -> list[dict]:
    """Each time antechamber trash engaged: when, how many, on whom, and where that bot stood."""
    first: dict[int, tuple] = {}
    for snap in frames(trace):
        for row in snap.get("u", []):
            if entry(trace, row[0]) in TRASH and len(row) > 7 and row[7] and row[0] not in first:
                first[row[0]] = (snap["t"], row[7])
    roster = roster_guids(trace)
    pulls: list[dict] = []
    for _, (t, target) in sorted(first.items(), key=lambda item: item[1][0]):
        if pulls and t - pulls[-1]["t"] <= PACK_WINDOW_MS:
            pulls[-1]["n"] += 1
            continue
        spot = None
        if target in roster:
            for snap in frames(trace):
                if snap["t"] > t:
                    break
                for row in snap.get("u", []):
                    if row[0] == target:
                        spot = (row[1], row[2], row[3])
        pulls.append({"t": t, "n": 1, "target": target, "spot": spot})
    return pulls


def show_trash(trace: Trace) -> None:
    print("ANTECHAMBER TRASH")
    if not mine(trace):
        return
    pulls = trash_pulls(trace)
    if not pulls:
        print("  never engaged")
    for pull in pulls:
        line = f"  {clock(pull['t'])}  {pull['n']} on {trace.name(pull['target'])}"
        if pull["spot"]:
            x, y, z = pull["spot"]
            where = "off the walkway" if off_walkway(x, z) else "on the walkway"
            line += f", standing at ({x:.0f}, {y:.0f}, {z:.0f}) {where}"
        print(line)
    killed = sum(1 for d in combat_deaths(trace) if entry(trace, d.get("killer")) in TRASH)
    print(f"  deaths to trash: {killed}")


def mechanic(trace: Trace, death: dict) -> str:
    killer = entry(trace, death.get("killer"))
    blow = death.get("blow") or []
    if blow and blow[0] == death.get("g"):
        return "fall"
    rewind = sorted(death.get("rewind") or [], key=lambda row: row[0])
    spell_id = rewind[-1][2] if rewind else 0
    spell = (trace.spells.get(spell_id) or f"spell {spell_id}") if spell_id else ""
    if killer in TRASH:
        return "antechamber trash"
    if killer == NPC_ARMSWEEP:
        return "Shockwave"
    if killer in EYES:
        return "Focused Eyebeam"
    if killer == NPC_RUBBLE:
        return "Rubble"
    if killer == NPC_PIT_BUNNY:
        return "pit"
    if killer in (NPC_KOLOGARN, NPC_LEFT_ARM, NPC_RIGHT_ARM) and spell:
        return spell
    if killer == NPC_KOLOGARN:
        return "Kologarn melee on a tank" if trace.role(death["g"]) == "tank" else "Kologarn melee on a non-tank"
    return f"{trace.name(death.get('killer'))} {spell}".strip()


def show_deaths(trace: Trace) -> None:
    print("DEATHS BY MECHANIC")
    if not mine(trace):
        return
    deaths = combat_deaths(trace)
    pulls = trash_pulls(trace)
    trash_from = pulls[0]["t"] if pulls else None
    counts = collections.Counter(mechanic(trace, d) for d in deaths)
    after = collections.Counter(
        mechanic(trace, d) for d in deaths if trash_from is not None and d["t"] >= trash_from)
    if not counts:
        print("  none")
    for name, n in counts.most_common():
        tail = f"   ({after[name]} after the trash joined)" if trash_from is not None else ""
        print(f"  {n:3d}  {name}{tail}")


SECTIONS = (
    ("eyebeam", "each eyebeam, whom it chased and whether the run left the walkway", show_eyebeam),
    ("trash", "antechamber trash pulls and the bot each pack first targeted", show_trash),
    ("deaths", "deaths by mechanic", show_deaths),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
