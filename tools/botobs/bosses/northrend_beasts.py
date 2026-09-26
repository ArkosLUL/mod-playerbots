#!/usr/bin/env python3
"""Score a Northrend Beasts pull from a RaidObs trace: the stages, the tank duties, the charge, snobolds.

    northrend_beasts.py <file>             every section
    northrend_beasts.py <file> --stage     which beasts were up when, deaths per stage, each beast's engage
    northrend_beasts.py <file> --tanks     tank duties, Gormok's victims with Impale stacks, swaps, defensives
    northrend_beasts.py <file> --charge    every Icehowl charge: gaze, lane, outcome, who stood at its end
    northrend_beasts.py <file> --snobold   who carried a snobold and for how long, and what the DPS picked

What the generic views get wrong here, and what this reads instead:

- **`nb.stage` is a mask, not a phase.** Heroic brings the next beast in on a timer, so one span can
  hold Gormok and the worms at once: 1 Gormok, 2 a worm, 4 Icehowl, added up. 0 is a walk-in or the
  gap after a kill, where nothing is in combat.
- **Creature auras are never recorded**, so Staggered Daze and Frothing Rage on Icehowl have no aura
  row. `nb.charge` 4 and 5 are the only record of how a charge ended.
- **The charge has no world object.** Its path is a `haz` `lane` row, which never reaches `snap.hz`,
  so no death block says STOOD IN for Trample. `--charge` measures the lane's end against the nearest
  snapshot instead: the script tests contact on arrival, 12 yd round his end point.
- **Impale sits on the tank**, so its stacks are in the `aura` rows' `st`, under any of the four
  difficulty ids. A taunt that lands shows as a change in Gormok's target column, not as an action.
- **`nb.tank`, `nb.swap`, `nb.snobold`, `nb.dodge` and `nb.defensive` are per-bot change-only
  notes**: a bot that stays on one answer writes one row, so counts here are changes, not ticks.
"""
from __future__ import annotations

import bisect
import collections
import pathlib
import re
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.geometry import dist2, frames, guids_of_entry, radius  # noqa: E402
from raidobs.probes import emitted_keys, holder_spans, latch_spans, silent_keys  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, notes, roster_guids  # noqa: E402

ENCOUNTER = "northrend-beasts"

NPC_GORMOK = 34796
NPC_ACIDMAW = 35144
NPC_DREADSCALE = 34799
NPC_ICEHOWL = 34797
BEASTS = (("Gormok", NPC_GORMOK), ("Acidmaw", NPC_ACIDMAW), ("Dreadscale", NPC_DREADSCALE),
          ("Icehowl", NPC_ICEHOWL))

# 10N, 25N, 10H, 25H
SPELL_IMPALE = (66331, 67477, 67478, 67479)
SPELL_SNOBOLLED = 66406
SPELL_TRAMPLE = 66734

STAGE_BITS = ((1, "gormok"), (2, "worms"), (4, "icehowl"))

CHARGE_ACTIVE = ("1", "2", "3")
CHARGE_OUTCOMES = {"0": "none", "4": "daze", "5": "rage"}

SWAP_TAUNT = "gormok tank swap taunt"

# Read from the source so a retune shows up here without a second edit.
TRAMPLE_RADIUS = radius("ICEHOWL_TRAMPLE_RADIUS")

# A snapshot further than this from the outcome says nothing about where the raid stood at it.
NEAREST_TOLERANCE_MS = 1500
# Trample lands on arrival, a moment after the outcome is read.
TRAMPLE_SLACK_MS = 2000


def pull_end(trace: Trace) -> int:
    ends = trace.of("end")
    if ends:
        return ends[-1]["t"]
    return trace.records[-1].get("t", 0) if trace.records else 0


def stage_name(value: str) -> str:
    try:
        mask = int(value)
    except ValueError:
        return value
    names = [name for bit, name in STAGE_BITS if mask & bit]
    return "+".join(names) if names else "none"


def show_banner(trace: Trace) -> None:
    end = pull_end(trace)
    outcome = trace.of("end")[-1].get("out", "?") if trace.of("end") else "no end record"
    encounter = encounter_of(trace)
    print(f"{encounter}  {outcome} at {clock(end)}  {len(combat_deaths(trace))} death(s)")

    # the probe check matches keys to the encounter, so on another boss it would pass on nothing
    if encounter != ENCOUNTER:
        print("  not a Beasts pull, every section below reads empty")
        return

    gone = [key for key, _, _ in silent_keys(emitted_keys(trace), encounter)]
    print("all nb.* probes present" if not gone else f"probes absent from this trace: {', '.join(gone)}")


def stage_spans(trace: Trace) -> list[dict]:
    """Each `nb.stage` value with its span and the combat deaths inside it."""
    deaths = combat_deaths(trace)
    out = []
    for value, start, stop in latch_spans(trace, "nb.stage", pull_end(trace)):
        out.append({
            "value": value,
            "start": start,
            "stop": stop,
            "deaths": [death.get("g") for death in deaths if start <= death["t"] < stop],
        })
    return out


def first_engaged(trace: Trace) -> dict[int, int]:
    """Per beast entry, the first snapshot where one of its guids targets someone. A beast walking
    in has no target, so that is the nearest the snapshot gets to "in combat"."""
    out: dict[int, int] = {}
    for _, entry in BEASTS:
        guids = guids_of_entry(trace, entry)
        if not guids:
            continue
        for snap in frames(trace):
            if any(row[0] in guids and len(row) > 7 and row[7] and row[5] > 0 for row in snap.get("u", [])):
                out[entry] = snap["t"]
                break
    return out


def show_stage(trace: Trace) -> None:
    print("STAGES")
    spans = stage_spans(trace)
    if not spans:
        print("  no nb.stage rows")
    else:
        print(f"  {'from':>9} {'to':>9} {'secs':>6}  {'stage':16} deaths")
        for span in spans:
            dead = ", ".join(trace.name(guid) for guid in span["deaths"]) or "-"
            print(f"  {clock(span['start']):>9} {clock(span['stop']):>9}"
                  f" {(span['stop'] - span['start']) / 1000:6.1f}  {stage_name(span['value']):16} {dead}")

    engaged = first_engaged(trace)
    if not engaged:
        print("\n  no beast was ever sampled with a target")
        return
    print("\n  first sampled in combat:  " + "  ".join(
        f"{name} {clock(engaged[entry])}" for name, entry in BEASTS if entry in engaged))


def split_duty(text: str) -> tuple[str, int | None]:
    """`gormok 4294968001` into the duty and the beast's guid."""
    parts = str(text).split()
    if len(parts) > 1 and parts[1].isdigit():
        return parts[0], int(parts[1])
    return (parts[0] if parts else ""), None


def impale_stacks(trace: Trace) -> dict[int, list[tuple[int, int]]]:
    """Per target, `(t, stacks)` at every Impale row, 0 on a removal."""
    out: dict[int, list[tuple[int, int]]] = collections.defaultdict(list)
    for rec in trace.of("aura"):
        if rec.get("sp") in SPELL_IMPALE:
            out[rec.get("d", 0)].append((rec["t"], 0 if rec.get("r") else int(rec.get("st", 1) or 1)))
    return out


def stacks_at(track: list[tuple[int, int]], when: int) -> int:
    index = bisect.bisect_right([t for t, _ in track], when)
    return track[index - 1][1] if index else 0


def gormok_victims(trace: Trace) -> list[dict]:
    """Every change of Gormok's target column while he was alive, with the Impale stacks the old and
    the new holder carried at that moment."""
    gormoks = guids_of_entry(trace, NPC_GORMOK)
    stacks = impale_stacks(trace)
    out = []
    previous: dict[int, int] = {}
    for snap in frames(trace):
        for row in snap.get("u", []):
            if row[0] not in gormoks or len(row) <= 7 or row[5] <= 0:
                continue
            victim = row[7]
            if row[0] in previous and previous[row[0]] != victim:
                old = previous[row[0]]
                out.append({
                    "t": snap["t"],
                    "old": old,
                    "new": victim,
                    "old_stacks": stacks_at(stacks.get(old, []), snap["t"]),
                    "new_stacks": stacks_at(stacks.get(victim, []), snap["t"]),
                })
            previous[row[0]] = victim
    return out


def peak_stacks(trace: Trace) -> dict[int, int]:
    return {guid: max(count for _, count in track) for guid, track in impale_stacks(trace).items() if track}


def branch_counts(trace: Trace, key: str, branch) -> dict[int, collections.Counter]:
    out: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
    for rec in notes(trace, key):
        out[rec.get("g", 0)][branch(str(rec.get("txt", "")))] += 1
    return dict(out)


def swap_branches(trace: Trace) -> dict[int, collections.Counter]:
    """`nb.swap` per bot by branch, the stack counts dropped: `go v`, `wait mine`, `wait v`."""
    return branch_counts(trace, "nb.swap", lambda text: re.sub(r"=\S*", "", text).strip())


def swap_taunts(trace: Trace) -> dict[int, collections.Counter]:
    out: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
    for rec in trace.of("act"):
        if rec.get("a") == SWAP_TAUNT:
            out[rec.get("g", 0)][rec.get("vd", "?")] += 1
    return dict(out)


def defensive_picks(trace: Trace) -> dict[int, collections.Counter]:
    return branch_counts(trace, "nb.defensive", lambda text: text)


def show_tanks(trace: Trace) -> None:
    print("TANKS")
    end = pull_end(trace)
    duties = holder_spans(trace, "nb.tank", end)
    if not duties:
        print("  no nb.tank rows")
    for guid, spans in sorted(duties.items(), key=lambda item: trace.name(item[0])):
        parts = []
        for value, start, stop in spans:
            duty, beast = split_duty(value)
            on = f" {trace.name(beast).split()[0]}" if beast else ""
            parts.append(f"{clock(start)}-{clock(stop)} {duty}{on}")
        print(f"  {trace.name(guid)[:14]:14} {trace.role(guid):6} " + "; ".join(parts))

    victims = gormok_victims(trace)
    print("\n  Gormok's victim changes:" + ("" if victims else " none"))
    for row in victims:
        print(f"    {clock(row['t']):>9} {trace.name(row['old'])[:14]:14} ({row['old_stacks']})"
              f" -> {trace.name(row['new'])[:14]:14} ({row['new_stacks']})")

    peaks = peak_stacks(trace)
    if peaks:
        print("  peak Impale stacks: " + ", ".join(
            f"{trace.name(guid)} {count}" for guid, count in sorted(peaks.items(), key=lambda item: -item[1])))

    for title, table in (("nb.swap", swap_branches(trace)), (SWAP_TAUNT, swap_taunts(trace)),
                         ("nb.defensive", defensive_picks(trace))):
        if not table:
            print(f"  {title}: none")
            continue
        print(f"  {title}:")
        for guid, counts in sorted(table.items(), key=lambda item: trace.name(item[0])):
            print(f"    {trace.name(guid)[:14]:14} " + ", ".join(f"{name} {n}" for name, n in counts.most_common()))


def nearest_frame(trace: Trace, when: int) -> dict | None:
    snaps = frames(trace)
    stamps = [snap["t"] for snap in snaps]
    index = bisect.bisect_left(stamps, when)
    options = [i for i in (index - 1, index) if 0 <= i < len(snaps)]
    if not options:
        return None
    pick = min(options, key=lambda i: abs(stamps[i] - when))
    return snaps[pick] if abs(stamps[pick] - when) <= NEAREST_TOLERANCE_MS else None


def inside_lane_end(trace: Trace, when: int, end_point) -> set[int]:
    """Living roster members within Trample's reach of the charge's end point, from the snapshot
    nearest the outcome."""
    snap = nearest_frame(trace, when)
    if snap is None:
        return set()
    roster = roster_guids(trace)
    return {row[0] for row in snap.get("u", [])
            if row[0] in roster and row[5] > 0 and dist2(row[1:3], end_point) <= TRAMPLE_RADIUS}


def charges(trace: Trace) -> list[dict]:
    """One row per `nb.charge` cycle: opened by the first 1, 2 or 3, closed by the next 4 (daze),
    5 (rage) or 0 (neither)."""
    end = pull_end(trace)
    gazes = [(rec["t"], str(rec.get("txt", ""))) for rec in notes(trace, "nb.gaze")]
    lanes = [rec for rec in trace.of("haz") if rec.get("sp") == SPELL_TRAMPLE and rec.get("shape") == "lane"]
    dodges = notes(trace, "nb.dodge")
    tramples = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_TRAMPLE]

    cycles = []
    start = None
    for value, when, _ in latch_spans(trace, "nb.charge", end):
        if value in CHARGE_ACTIVE:
            if start is None:
                start = when
            continue
        if start is not None and value in CHARGE_OUTCOMES:
            cycles.append((start, when, CHARGE_OUTCOMES[value]))
            start = None
    if start is not None:
        cycles.append((start, end, "open"))

    out = []
    for start, stop, outcome in cycles:
        gaze = next((int(text) for when, text in reversed(gazes)
                     if start <= when <= stop and text.isdigit() and int(text)), None)
        # gaze writes a row, the jump back writes the frozen line: the last one is his path
        lane = next((rec for rec in reversed(lanes) if start <= rec["t"] <= stop), None)
        end_point = (lane["ex"], lane["ey"]) if lane and "ex" in lane and "ey" in lane else None

        branches: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
        for rec in dodges:
            if start <= rec["t"] <= stop:
                words = str(rec.get("txt", "")).split()
                branches[rec.get("g", 0)][words[0] if words else ""] += 1

        out.append({
            "start": start,
            "stop": stop,
            "outcome": outcome,
            "gaze": gaze,
            "lane": lane,
            "inside": inside_lane_end(trace, stop, end_point) if end_point and outcome != "open" else set(),
            "dodges": dict(branches),
            "trampled": [rec.get("d") for rec in tramples if start <= rec["t"] <= stop + TRAMPLE_SLACK_MS],
        })
    return out


def show_charge(trace: Trace) -> None:
    print("ICEHOWL CHARGE")
    rows = charges(trace)
    if not rows:
        print("  no nb.charge cycle")
        return

    for row in rows:
        gaze = f"{trace.name(row['gaze'])} ({trace.role(row['gaze'])})" if row["gaze"] else "-"
        print(f"  {clock(row['start'])} to {clock(row['stop'])}  {row['outcome']:5}  gaze {gaze}")
        lane = row["lane"]
        if lane:
            print(f"    lane ({lane.get('x', 0):.1f}, {lane.get('y', 0):.1f}) -> ({lane.get('ex', 0):.1f},"
                  f" {lane.get('ey', 0):.1f}), half {lane.get('half', '?')}")
        else:
            print("    no lane row")
        inside = ", ".join(trace.name(guid) for guid in sorted(row["inside"], key=trace.name)) or "nobody"
        print(f"    within {TRAMPLE_RADIUS:.0f} yd of the end at the outcome: {inside}")
        trampled = ", ".join(trace.name(guid) for guid in row["trampled"]) or "nobody"
        print(f"    Trample hit: {trampled}")
        for guid, counts in sorted(row["dodges"].items(), key=lambda item: trace.name(item[0])):
            print(f"    nb.dodge {trace.name(guid)[:14]:14} " + ", ".join(
                f"{name} {n}" for name, n in counts.most_common()))

    tally = collections.Counter(row["outcome"] for row in rows)
    print("\n  " + ", ".join(f"{outcome} {n}" for outcome, n in tally.most_common()))


def rider_spans(trace: Trace) -> dict[int, list[tuple[int, int]]]:
    """Per rider, Snobolled! apply to remove. One still up at the end runs to the end."""
    end = pull_end(trace)
    opened: dict[int, int] = {}
    out: dict[int, list[tuple[int, int]]] = collections.defaultdict(list)
    for rec in trace.of("aura"):
        if rec.get("sp") != SPELL_SNOBOLLED:
            continue
        guid = rec.get("d", 0)
        if rec.get("r"):
            if guid in opened:
                out[guid].append((opened.pop(guid), rec["t"]))
        elif guid not in opened:
            opened[guid] = rec["t"]
    for guid, start in opened.items():
        out[guid].append((start, end))
    return dict(out)


def snobold_picks(trace: Trace) -> dict[int, collections.Counter]:
    """`nb.snobold` per DPS by the rider's role it picked, `none` for no pick."""
    def role_of(text: str) -> str:
        words = text.split()
        return words[1] if len(words) > 1 else (words[0] if words else "")
    return branch_counts(trace, "nb.snobold", role_of)


def show_snobold(trace: Trace) -> None:
    print("SNOBOLDS")
    spans = rider_spans(trace)
    if not spans:
        print("  nobody carried a snobold")
    else:
        print(f"  {'rider':14} {'role':6} {'times':>5} {'secs':>6}")
        for guid, rows in sorted(spans.items(), key=lambda item: -sum(b - a for a, b in item[1])):
            carried = sum(stop - start for start, stop in rows) / 1000
            print(f"  {trace.name(guid)[:14]:14} {trace.role(guid):6} {len(rows):5} {carried:6.1f}")

    picks = snobold_picks(trace)
    if not picks:
        print("  nb.snobold: none")
        return
    print("  nb.snobold picks by rider role:")
    for guid, counts in sorted(picks.items(), key=lambda item: trace.name(item[0])):
        print(f"    {trace.name(guid)[:14]:14} {trace.role(guid):6} "
              + ", ".join(f"{role} {n}" for role, n in counts.most_common()))


SECTIONS = (
    ("stage", "which beasts were up when, deaths per stage, each beast's engage", show_stage),
    ("tanks", "tank duties, Gormok's victims with Impale stacks, swaps, defensives", show_tanks),
    ("charge", "every Icehowl charge: gaze, lane, outcome, who stood at its end", show_charge),
    ("snobold", "snobold riders and the DPS picks", show_snobold),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
