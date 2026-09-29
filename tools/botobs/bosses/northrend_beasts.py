#!/usr/bin/env python3
"""Score a Northrend Beasts pull from a RaidObs trace: the stages, the tank duties, the charge, Arctic
Breath, snobolds, Fire Bomb.

    northrend_beasts.py <file>             every section
    northrend_beasts.py <file> --stage     which beasts were up when, deaths per stage, each beast's engage
    northrend_beasts.py <file> --tanks     tank duties, Gormok's victims with Impale stacks, swaps, defensives
    northrend_beasts.py <file> --charge    every Icehowl charge: gaze, lane, outcome, who stood at its end
    northrend_beasts.py <file> --breath    every Arctic Breath: target, who froze, who the cone predicts, spread
    northrend_beasts.py <file> --snobold   who carried a snobold and for how long, and what the DPS picked
    northrend_beasts.py <file> --bomb      every Fire Bomb: target, impact hits, who stood inside, dodges

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
- **Arctic Breath picks its victims once, at the cast**, inside half the cone's width of the target's
  bearing from Icehowl: 30° on 10N (`hdr.diff` 0), 12° on the rest. `--breath` predicts that set from
  the snapshot nearest the cast and prints it beside the `aura` applies; a mismatch is a bot that
  moved between that snapshot and the cast.
- **Fire Bomb's impact has no world object.** The NPC is at best a `snap.u` row and nothing marks when
  66317 lands. Each bomb writes one `haz` `circle` row whose `ttl` is the modelled impact, not a
  measured one, so `--bomb` takes 66317 hits up to 1.5 s past it.
- **`nb.tank`, `nb.swap`, `nb.snobold`, `nb.dodge`, `nb.spread`, `nb.bomb` and `nb.defensive` are
  per-bot change-only notes**: a bot that stays on one answer writes one row, so counts here are
  changes, not ticks. `nb.bomb` writes `clear` whenever a bot is outside every young bomb's trigger,
  so a dodge that repeats the last one's branch still shows. `nb.arc` is one value for the raid.
"""
from __future__ import annotations

import bisect
import collections
import math
import pathlib
import re
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.geometry import NEAREST, at, dist2, frames, guids_of_entry, radius  # noqa: E402
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
ARCTIC_BREATH = (66689, 67650, 67651, 67652)
SPELL_SNOBOLLED = 66406
SPELL_TRAMPLE = 66734
SPELL_FIRE_BOMB_IMPACT = 66317

STAGE_BITS = ((1, "gormok"), (2, "worms"), (4, "icehowl"))

CHARGE_ACTIVE = ("1", "2", "3")
CHARGE_OUTCOMES = {"0": "none", "4": "daze", "5": "rage"}

SWAP_TAUNT = "gormok tank swap taunt"

# Read from the source so a retune shows up here without a second edit.
TRAMPLE_RADIUS = radius("ICEHOWL_TRAMPLE_RADIUS")
FIRE_BOMB_IMPACT_RADIUS = radius("FIRE_BOMB_IMPACT_RADIUS")

# A snapshot further than this from the outcome says nothing about where the raid stood at it.
NEAREST_TOLERANCE_MS = 1500
# Trample lands on arrival, a moment after the outcome is read.
TRAMPLE_SLACK_MS = 2000

# Half of Arctic Breath's cone: `spell_cone` makes 66689 60° wide, the other three ids fall back to
# TARGET_UNIT_CONE_ENEMY_24.
BREATH_HALF_WIDTH_10N = 30.0
BREATH_HALF_WIDTH = 12.0
# The breath's stun lands with the cast, so an apply this long after it belongs to it.
BREATH_VICTIM_MS = 1000
# A bomb's `ttl` is a model of the impact, so its hits are read this far past it.
BOMB_HIT_SLACK_MS = 1500


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


def living_within(trace: Trace, when: int, spot, reach: float) -> set[int]:
    """Living roster members within `reach` of `spot` in the snapshot nearest `when`."""
    snap = nearest_frame(trace, when)
    if snap is None:
        return set()
    roster = roster_guids(trace)
    return {row[0] for row in snap.get("u", [])
            if row[0] in roster and row[5] > 0 and dist2(row[1:3], spot) <= reach}


def inside_lane_end(trace: Trace, when: int, end_point) -> set[int]:
    """Living roster members within Trample's reach of the charge's end point, from the snapshot
    nearest the outcome."""
    return living_within(trace, when, end_point, TRAMPLE_RADIUS)


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


def breath_half_width(diff) -> float:
    return BREATH_HALF_WIDTH_10N if diff == 0 else BREATH_HALF_WIDTH


def bearing(origin, point) -> float:
    return math.degrees(math.atan2(point[1] - origin[1], point[0] - origin[0]))


def angle_off(a: float, b: float) -> float:
    return abs((a - b + 180.0) % 360.0 - 180.0)


def value_at(spans, when: int) -> str | None:
    """The value a span list held at `when`, or None."""
    for held, start, stop in spans or ():
        if start <= when < stop:
            return held
    return None


def breath_cone(trace: Trace, caster: int, target: int, when: int, half: float) -> set[int] | None:
    """Living roster members inside the cone as the snapshot nearest the cast places them, the target
    included. None when that snapshot misses him or the target."""
    snap = nearest_frame(trace, when)
    if snap is None:
        return None
    rows = {row[0]: row for row in snap.get("u", [])}
    boss, aimed = rows.get(caster), rows.get(target)
    if boss is None or aimed is None:
        return None
    front = bearing(boss[1:3], aimed[1:3])
    roster = roster_guids(trace)
    return {guid for guid, row in rows.items() if guid in roster and row[5] > 0
            and (guid == target or angle_off(bearing(boss[1:3], row[1:3]), front) <= half)}


def breaths(trace: Trace) -> list[dict]:
    """One row per Arctic Breath cast by Icehowl: its target, who froze, who the cone predicts, each
    victim's `nb.spread` and the raid's `nb.arc` at the cast."""
    icehowls = guids_of_entry(trace, NPC_ICEHOWL)
    roster = roster_guids(trace)
    half = breath_half_width(trace.header.get("diff", 0))
    applies = [rec for rec in trace.of("aura")
               if rec.get("sp") in ARCTIC_BREATH and not rec.get("r") and rec.get("d") in roster]
    spread = holder_spans(trace, "nb.spread")
    arc = latch_spans(trace, "nb.arc")

    out = []
    for rec in trace.of("cast"):
        if rec.get("sp") not in ARCTIC_BREATH or rec.get("s") not in icehowls:
            continue
        when = rec["t"]
        target = rec.get("tgt", 0)
        victims: list[int] = []
        for hit in applies:
            if when <= hit["t"] <= when + BREATH_VICTIM_MS and hit["d"] not in victims:
                victims.append(hit["d"])
        out.append({
            "t": when,
            "target": target,
            "victims": victims,
            "predicted": breath_cone(trace, rec["s"], target, when, half) if target else None,
            "spread": {guid: value_at(spread.get(guid), when) for guid in victims},
            "arc": value_at(arc, when),
        })
    return out


def times_frozen(rows: list[dict]) -> collections.Counter:
    return collections.Counter(guid for row in rows for guid in row["victims"])


def show_breath(trace: Trace) -> None:
    half = breath_half_width(trace.header.get("diff", 0))
    print(f"ARCTIC BREATH  (cone half-width {half:.0f} deg)")
    rows = breaths(trace)
    if not rows:
        print("  no Arctic Breath cast")
        return

    def names(guids) -> str:
        return ", ".join(trace.name(guid) for guid in sorted(guids, key=trace.name)) or "nobody"

    for row in rows:
        print(f"  {clock(row['t'])}  on {trace.name(row['target'])} ({trace.role(row['target'])})"
              f"  nb.arc {row['arc'] if row['arc'] is not None else '-'}")
        frozen = ", ".join(f"{trace.name(guid)} [{row['spread'][guid] or '-'}]" for guid in row["victims"])
        print(f"    frozen [nb.spread]: {frozen or 'nobody'}")
        predicted = "? (no snapshot)" if row["predicted"] is None else names(row["predicted"])
        print(f"    in the cone: {predicted}")

    counts = [len(row["victims"]) for row in rows]
    print(f"\n  {len(rows)} breath(s), {sum(counts) / len(counts):.1f} frozen a breath, max {max(counts)}")
    tally = sorted(times_frozen(rows).items(), key=lambda item: (-item[1], trace.name(item[0])))
    if tally:
        print("  times frozen: " + ", ".join(f"{trace.name(guid)} {n}" for guid, n in tally))


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


def nearest_member(trace: Trace, when: int, spot) -> tuple[int | None, float | None]:
    """The living roster member nearest `spot` in the snapshot nearest `when`, and how far off."""
    snap = nearest_frame(trace, when)
    if snap is None:
        return None, None
    roster = roster_guids(trace)
    living = [(dist2(row[1:3], spot), row[0]) for row in snap.get("u", []) if row[0] in roster and row[5] > 0]
    if not living:
        return None, None
    gap, guid = min(living)
    return guid, gap


def bombs(trace: Trace) -> list[dict]:
    """One row per Fire Bomb circle: its target, the 66317 hits it took, who stood inside at the
    modelled impact and each bot's `nb.bomb` branches up to it.

    A hit whose window holds two bombs goes to the one nearest the victim, from the snapshot nearest
    the hit, or to the one whose impact is nearest in time when no snapshot places the victim.
    """
    out = []
    for rec in trace.of("haz"):
        if rec.get("sp") != SPELL_FIRE_BOMB_IMPACT or rec.get("shape") != "circle":
            continue
        when = rec["t"]
        ttl = int(rec.get("ttl", 0) or 0)
        spot = (rec.get("x", 0.0), rec.get("y", 0.0))
        target, gap = nearest_member(trace, when, spot)
        out.append({
            "t": when,
            "ttl": ttl,
            "spot": spot,
            "target": target,
            "gap": gap,
            "hits": [],
            "inside": living_within(trace, when + ttl, spot, FIRE_BOMB_IMPACT_RADIUS),
            "dodges": {},
        })

    for hit in trace.of("dmg"):
        if hit.get("sp") != SPELL_FIRE_BOMB_IMPACT:
            continue
        holding = [row for row in out if row["t"] <= hit["t"] <= row["t"] + row["ttl"] + BOMB_HIT_SLACK_MS]
        if not holding:
            continue
        victim = hit.get("d", 0)
        place = at(trace, victim, hit["t"], NEAREST, NEAREST_TOLERANCE_MS)
        if place is None:
            pick = min(holding, key=lambda row: abs(row["t"] + row["ttl"] - hit["t"]))
        else:
            pick = min(holding, key=lambda row: dist2(place, row["spot"]))
        pick["hits"].append(victim)

    dodges = notes(trace, "nb.bomb")
    for row in out:
        branches: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
        for rec in dodges:
            if row["t"] <= rec["t"] <= row["t"] + row["ttl"]:
                words = str(rec.get("txt", "")).split()
                branches[rec.get("g", 0)][words[0] if words else ""] += 1
        row["dodges"] = dict(branches)
    return out


def show_bomb(trace: Trace) -> None:
    print(f"FIRE BOMB  (impact {FIRE_BOMB_IMPACT_RADIUS:.0f} yd)")
    rows = bombs(trace)
    if not rows:
        print("  no Fire Bomb circle")
        return

    for row in rows:
        target = "-" if row["target"] is None else (
            f"{trace.name(row['target'])} ({trace.role(row['target'])}, {row['gap']:.1f} yd)")
        print(f"  {clock(row['t'])}  at ({row['spot'][0]:.1f}, {row['spot'][1]:.1f})"
              f"  impact +{row['ttl'] / 1000:.1f} s  on {target}")
        hit = ", ".join(trace.name(guid) for guid in row["hits"]) or "nobody"
        print(f"    66317 hit: {hit}")
        inside = ", ".join(trace.name(guid) for guid in sorted(row["inside"], key=trace.name)) or "nobody"
        print(f"    inside at the impact: {inside}")
        for guid, counts in sorted(row["dodges"].items(), key=lambda item: trace.name(item[0])):
            print(f"    nb.bomb {trace.name(guid)[:14]:14} " + ", ".join(
                f"{name} {n}" for name, n in counts.most_common()))

    hits = sum(len(row["hits"]) for row in rows)
    print(f"\n  {len(rows)} bomb(s), {hits} hit(s), {hits / len(rows):.1f} a bomb")
    stray = sum(1 for rec in trace.of("dmg") if rec.get("sp") == SPELL_FIRE_BOMB_IMPACT) - hits
    if stray:
        print(f"  {stray} 66317 hit(s) outside every bomb's window")


SECTIONS = (
    ("stage", "which beasts were up when, deaths per stage, each beast's engage", show_stage),
    ("tanks", "tank duties, Gormok's victims with Impale stacks, swaps, defensives", show_tanks),
    ("charge", "every Icehowl charge: gaze, lane, outcome, who stood at its end", show_charge),
    ("breath", "every Arctic Breath: target, who froze, who the cone predicts, spread", show_breath),
    ("snobold", "snobold riders and the DPS picks", show_snobold),
    ("bomb", "every Fire Bomb: target, impact hits, who stood inside, dodges", show_bomb),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
