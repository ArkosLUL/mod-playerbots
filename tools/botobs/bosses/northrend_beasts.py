#!/usr/bin/env python3
"""Score a Northrend Beasts pull from a RaidObs trace: the stages, the tank duties, the charge, Arctic
Breath, snobolds, Fire Bomb, the worms and the Toxin cure.

    northrend_beasts.py <file>             every section
    northrend_beasts.py <file> --stage     which beasts were up when, deaths per stage, each beast's engage
    northrend_beasts.py <file> --tanks     tank duties, Gormok's victims with Impale stacks, swaps, defensives
    northrend_beasts.py <file> --targets   each DPS bot's time on its nb.dps pick, another beast or neither
    northrend_beasts.py <file> --charge    every Icehowl charge: gaze, lane, outcome, who stood at its end, the daze
    northrend_beasts.py <file> --breath    every Arctic Breath: target, who froze, the cone, spread, nb.breath, his walk
    northrend_beasts.py <file> --snobold   who carried a snobold and for how long, and what the DPS picked
    northrend_beasts.py <file> --bomb      every Fire Bomb: target, impact hits, who stood inside, dodges
    northrend_beasts.py <file> --worms     worm forms, emerges, Spew, pools, Sweep, Spray, caster spread, holds
    northrend_beasts.py <file> --cure      Paralytic Toxin spans and how each ended, Bile pulses on the clean

What the generic views get wrong here, and what this reads instead:

- **`nb.stage` is a mask, not a phase.** Heroic brings the next beast in on a timer, so one span can
  hold Gormok and the worms at once: 1 Gormok, 2 a worm, 4 Icehowl, added up. 0 is a walk-in or the
  gap after a kill, where nothing is in combat.
- **Creature auras are never recorded**, so Staggered Daze and Frothing Rage on Icehowl have no aura
  row. `nb.charge` 4 and 5 are the only record of how a charge ended, and `--charge` reads the daze
  as the 15 s from the 4.
- **Heroic Berserk 26662 shows only as a triggered `cast` row** on a beast. With none the banner
  takes its due time, 509 s into the trace: the trace opens at Gormok's engage, 11 s after the
  intro's end, and the script casts Berserk 520 s after that end. A dead creature drops out of the
  snapshot, so a beast with no row in the 1.5 s before it reads dead.
- **The charge has no world object.** Its path is a `haz` `lane` row, which never reaches `snap.hz`,
  so no death block says STOOD IN for Trample. `--charge` measures the lane's end against the nearest
  snapshot instead: the script tests contact on arrival, 12 yd round his end point.
- **Impale sits on the tank**, so its stacks are in the `aura` rows' `st`, under any of the four
  difficulty ids. A taunt that lands shows as a change in Gormok's target column, not as an action.
- **Arctic Breath picks its victims once, at the cast**, inside half the cone's width of the target's
  bearing from Icehowl: 30° on 10N (`hdr.diff` 0), 12° on the rest. `--breath` predicts that set from
  the snapshot nearest the cast and prints it beside the `aura` applies; a mismatch is a bot that
  moved between that snapshot and the cast. On 25N his casts never reach the `cast` stream, so with
  no cast row it groups his `aura` applies into breaths instead, with no target and no prediction.
- **Fire Bomb's impact has no world object.** The NPC is at best a `snap.u` row and nothing marks when
  66317 lands. Each bomb writes one `haz` `circle` row whose `ttl` is the modelled impact, not a
  measured one, so `--bomb` takes 66317 hits up to 1.5 s past it.
- **`nb.tank`, `nb.swap`, `nb.snobold`, `nb.dps`, `nb.dodge`, `nb.spread`, `nb.bomb` and
  `nb.defensive` are per-bot change-only notes**: a bot that stays on one answer writes one row, so
  counts here are changes, not ticks. `nb.bomb` writes `clear` whenever a bot is outside every young
  bomb's trigger, so a dodge that repeats the last one's branch still shows. `nb.arc` and `nb.breath`
  are one value for the raid.
- **The worms' threat wipe shows only in their target column.** Each resets its threat list under
  ground and comes up on whoever threatens it first, so `--worms` times each worm from its own Emerge
  cast (66947 in the snapshot's casting column) until a tank is the target again: `nb.worm` leaves 3
  only at the later worm's emerge, up to 1.5 s after the first. `nb.cure`, `nb.wormmove` and
  `nb.wormhold` are per-bot change-only notes too.
- **Neither worm hazard reaches `snap.hz`.** The Spew cone is a `haz` `wedge` row, `arc` degrees either
  side of `facing`; a Slime Pool is a creature (35176) that grows 0.3 yd a second from 2 till it
  despawns (11 yd at 30 s on normal, 20 at 60 s on 25H), so `--worms` rebuilds its radius from its
  first snapshot.
- **A Spray's `cast` row is its start.** The splash lands on its target when the cast ends, `ct`
  later, so `--worms` reads who stood in it from the snapshot nearest that end. Its debuff arrives as
  one burst of applies 1-3 s after the row, so the first burst 0.8-4 s after it is who took it.
- **Nothing records what a bot attacks but the snapshot's target column**, its victim. `--targets`
  reads it against the `nb.dps` pick in force at each snapshot, so every switch shows up to one
  snapshot gap off the pick.
- **A cure has no record of its own.** The Bile pulse strips Toxin as it lands, so `--cure` calls a
  removal cured when a pulse hit the carrier within 250 ms of it.
- **A Bile pulse names Dreadscale as its source**, never the carrier, so `--cure` gives each hit to the
  carrier nearest its victim within the pulse's 10 yd, in the snapshot nearest the hit.
"""
from __future__ import annotations

import bisect
import collections
import math
import pathlib
import re
import statistics
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
SPELL_BERSERK = 26662
WHIRL = (67345, 67663, 67664, 67665)

HEROIC = (2, 3)
BERSERK_DUE_MS = 509000

STAGE_BITS = ((1, "gormok"), (2, "worms"), (4, "icehowl"))

CHARGE_ACTIVE = ("1", "2", "3")
CHARGE_OUTCOMES = {"0": "none", "4": "daze", "5": "rage"}
# Staggered Daze's stun
DAZE_MS = 15000

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
# His walk before a breath is read over this long: a Whirl throw and his follow after the tank fit.
BREATH_WALK_MS = 4000
BREATH_STATES = {"0": "off", "1": "up", "2": "grace", "3": "moving", "4": "daze"}
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

    rage = berserk(trace)
    if rage is None:
        return
    if not rage["reached"]:
        print(f"Berserk {SPELL_BERSERK} due at {clock(rage['t'])}, not reached")
        return
    health = ", ".join(f"{name} {health_text(hp)}" for name, hp in rage["health"].items())
    print(f"Berserk {SPELL_BERSERK} at {clock(rage['t'])} ({'cast' if rage['cast'] else 'due, no cast row'}):"
          f" {health}")


def health_text(hp: float | None) -> str:
    return "dead" if hp is None else f"{hp:.1f}%"


def beast_health(trace: Trace, entry: int, when: int) -> float | None:
    """A beast's health % in its last sampled row up to `when`, None with no living row in the
    `NEAREST_TOLERANCE_MS` before it."""
    guids = guids_of_entry(trace, entry)
    snaps = frames(trace)
    stamps = [snap["t"] for snap in snaps]
    for snap in reversed(snaps[:bisect.bisect_right(stamps, when)]):
        if snap["t"] < when - NEAREST_TOLERANCE_MS:
            break
        rows = [row[5] for row in snap.get("u", []) if row[0] in guids and row[5] > 0]
        if rows:
            return max(rows)
    return None


def berserk(trace: Trace) -> dict | None:
    """Heroic only: when Berserk landed, from its cast row on a beast, else when it was due, whether
    the trace reached it, and each beast's health then by name, None for dead."""
    if trace.header.get("diff") not in HEROIC:
        return None
    beasts = {guid for _, entry in BEASTS for guid in guids_of_entry(trace, entry)}
    cast = next((rec for rec in trace.of("cast") if rec.get("sp") == SPELL_BERSERK and rec.get("s") in beasts),
                None)
    when = cast["t"] if cast else BERSERK_DUE_MS
    reached = when <= pull_end(trace)
    return {
        "t": when,
        "cast": cast is not None,
        "reached": reached,
        "health": {name: beast_health(trace, entry, when) for name, entry in BEASTS} if reached else {},
    }


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


NPC_SNOBOLD_VASSAL = 34800
# nb.dps branch words, in the resolver's order
DPS_BRANCHES = ("skull", "snobold", "gormok", "mobile", "icehowl")
DPS_ROLES = ("melee", "ranged")


def encounter_units(trace: Trace) -> set[int]:
    """Every beast's and snobold's guid."""
    entries = [entry for _, entry in BEASTS] + [NPC_SNOBOLD_VASSAL]
    return {guid for entry in entries for guid in guids_of_entry(trace, entry)}


def dps_bots(trace: Trace) -> set[int]:
    return {guid for guid in roster_guids(trace) if trace.role(guid) in DPS_ROLES and guid not in trace.humans}


def target_time(trace: Trace) -> dict[int, dict[str, collections.Counter]]:
    """Per DPS bot and `nb.dps` branch, the sampled living ms its target column sat on its pick (`on`),
    on another beast or snobold (`other`), or on anything else (`neither`). Time under `none`, or
    before the bot's first `nb.dps` row, counts nowhere. A snapshot's ms run to the next one, capped
    at 2 s."""
    bots = dps_bots(trace)
    picks = holder_spans(trace, "nb.dps", pull_end(trace))
    units = encounter_units(trace)
    snaps = frames(trace)
    out: dict[int, dict[str, collections.Counter]] = collections.defaultdict(
        lambda: collections.defaultdict(collections.Counter))
    for index, snap in enumerate(snaps):
        step = min(snaps[index + 1]["t"] - snap["t"], MAX_STEP_MS) if index + 1 < len(snaps) else 0
        if step <= 0:
            continue
        for row in snap.get("u", []):
            if row[0] not in bots or row[5] <= 0:
                continue
            held = value_at(picks.get(row[0]), snap["t"])
            branch, pick = split_duty(held) if held is not None else ("", None)
            if pick is None:
                continue
            target = row[7] if len(row) > 7 else 0
            place = "on" if target == pick else ("other" if target in units else "neither")
            out[row[0]][branch][place] += step
    return {guid: dict(branches) for guid, branches in out.items()}


def on_pick_share(table: dict[int, dict[str, collections.Counter]]) -> dict[str, tuple[float, int]]:
    """Per branch, the raid's share of DPS time on its pick and the ms that share is of."""
    totals: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
    for branches in table.values():
        for branch, places in branches.items():
            totals[branch].update(places)
    out = {}
    for branch, places in totals.items():
        total = sum(places.values())
        if total:
            out[branch] = (places["on"] / total, total)
    return out


def branch_rank(branch: str) -> int:
    return DPS_BRANCHES.index(branch) if branch in DPS_BRANCHES else len(DPS_BRANCHES)


def show_targets(trace: Trace) -> None:
    print("DPS TARGETS  (sampled living seconds against each bot's nb.dps pick)")
    table = target_time(trace)
    if not table:
        print("  no nb.dps pick sampled")
        return

    print(f"  {'bot':14} {'role':6} {'branch':8} {'on':>6} {'other':>6} {'neither':>7}")
    for guid, branches in sorted(table.items(), key=lambda item: trace.name(item[0])):
        for branch in sorted(branches, key=branch_rank):
            places = branches[branch]
            print(f"  {trace.name(guid)[:14]:14} {trace.role(guid):6} {branch:8} {places['on'] / 1000:6.1f}"
                  f" {places['other'] / 1000:6.1f} {places['neither'] / 1000:7.1f}")

    shares = on_pick_share(table)
    print("\n  raid on its pick: " + ", ".join(
        f"{branch} {share:.0%} of {total / 1000:.1f} s"
        for branch, (share, total) in sorted(shares.items(), key=lambda item: branch_rank(item[0]))))


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
            "daze": daze_movement(trace, stop, min(stop + DAZE_MS, end)) if outcome == "daze" else None,
        })
    return out


def daze_movement(trace: Trace, start: int, stop: int) -> dict[int, dict]:
    """Per roster member sampled alive in `[start, stop)`: the share of those snapshots it was moving
    in, the yards between them, `(action, rows)` for the action owning most of its accepted moves
    (None with none), and its untriggered cast rows, since procs outnumber them several times over."""
    roster = roster_guids(trace)
    samples: dict[int, list[tuple[float, float, bool]]] = collections.defaultdict(list)
    for snap in frames(trace):
        if not start <= snap["t"] < stop:
            continue
        for row in snap.get("u", []):
            if row[0] in roster and row[5] > 0:
                samples[row[0]].append((row[1], row[2], len(row) > 8 and bool(row[8])))

    movers: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
    for rec in trace.of("move"):
        if rec.get("ok") and start <= rec["t"] < stop:
            movers[rec.get("g", 0)][rec.get("by") or "?"] += 1
    casts = collections.Counter(rec.get("s", 0) for rec in trace.of("cast")
                                if start <= rec["t"] < stop and not rec.get("tr"))

    out = {}
    for guid, rows in samples.items():
        top = movers[guid].most_common(1)
        out[guid] = {
            "share": sum(1 for row in rows if row[2]) / len(rows),
            "yards": sum(dist2(a, b) for a, b in zip(rows, rows[1:])),
            "mover": top[0] if top else None,
            "casts": casts[guid],
        }
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
        if row["daze"] is not None:
            show_daze(trace, row["daze"])

    tally = collections.Counter(row["outcome"] for row in rows)
    print("\n  " + ", ".join(f"{outcome} {n}" for outcome, n in tally.most_common()))


def show_daze(trace: Trace, table: dict[int, dict]) -> None:
    if not table:
        print(f"    daze ({DAZE_MS // 1000} s): nobody sampled")
        return
    print(f"    daze ({DAZE_MS // 1000} s), by share of snapshots moving:"
          f"\n      {'bot':14} {'role':6} {'moving':>6} {'yards':>6} {'casts':>5}  top accepted mover")
    for guid, stats in sorted(table.items(), key=lambda item: (-item[1]["share"], trace.name(item[0]))):
        mover = f"{stats['mover'][0]} ({stats['mover'][1]})" if stats["mover"] else "-"
        print(f"      {trace.name(guid)[:14]:14} {trace.role(guid):6} {stats['share']:6.0%} {stats['yards']:6.1f}"
              f" {stats['casts']:5}  {mover}")


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


def aura_breaths(applies: list[dict], icehowls: set[int]) -> list[tuple[int, int]]:
    """`(first apply, caster)` per breath when only the auras say one landed: Icehowl's applies, a new
    breath once one lands past `BREATH_VICTIM_MS` after the current one's first."""
    out: list[tuple[int, int]] = []
    for rec in sorted((rec for rec in applies if rec.get("s") in icehowls), key=lambda rec: rec["t"]):
        if not out or rec["t"] > out[-1][0] + BREATH_VICTIM_MS:
            out.append((rec["t"], rec["s"]))
    return out


def walked(trace: Trace, guid: int, start: int, stop: int) -> float | None:
    """Yards between where a unit stood at `start` and at `stop`, None when either is unsampled."""
    first = at(trace, guid, start, NEAREST, NEAREST_TOLERANCE_MS)
    last = at(trace, guid, stop, NEAREST, NEAREST_TOLERANCE_MS)
    return None if first is None or last is None else dist2(first, last)


def breaths(trace: Trace) -> list[dict]:
    """One row per Arctic Breath cast by Icehowl: its target, who froze, who the cone predicts, each
    victim's `nb.spread`, the raid's `nb.arc` and `nb.breath` at the cast, ms since his last Whirl
    cast row (None with none) and his displacement over the `BREATH_WALK_MS` before it.

    A trace with no cast row for it, as on 25N, gets one row per burst of his aura applies instead,
    with target 0 and no prediction."""
    icehowls = guids_of_entry(trace, NPC_ICEHOWL)
    roster = roster_guids(trace)
    half = breath_half_width(trace.header.get("diff", 0))
    applies = [rec for rec in trace.of("aura")
               if rec.get("sp") in ARCTIC_BREATH and not rec.get("r") and rec.get("d") in roster]
    spread = holder_spans(trace, "nb.spread")
    arc = latch_spans(trace, "nb.arc")
    state = latch_spans(trace, "nb.breath")
    whirls: dict[int, list[int]] = collections.defaultdict(list)
    for rec in trace.of("cast"):
        if rec.get("sp") in WHIRL:
            whirls[rec.get("s", 0)].append(rec["t"])

    casts = [(rec["t"], rec["s"], rec.get("tgt", 0)) for rec in trace.of("cast")
             if rec.get("sp") in ARCTIC_BREATH and rec.get("s") in icehowls]
    if not casts:
        casts = [(when, caster, 0) for when, caster in aura_breaths(applies, icehowls)]

    out = []
    for when, caster, target in casts:
        victims: list[int] = []
        for hit in applies:
            if when <= hit["t"] <= when + BREATH_VICTIM_MS and hit["d"] not in victims:
                victims.append(hit["d"])
        whirl = max((stamp for stamp in whirls.get(caster, ()) if stamp <= when), default=None)
        out.append({
            "t": when,
            "target": target,
            "victims": victims,
            "predicted": breath_cone(trace, caster, target, when, half) if target else None,
            "spread": {guid: value_at(spread.get(guid), when) for guid in victims},
            "arc": value_at(arc, when),
            "state": value_at(state, when),
            "since_whirl": None if whirl is None else when - whirl,
            "walked": walked(trace, caster, when - BREATH_WALK_MS, when),
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
        target = f"{trace.name(row['target'])} ({trace.role(row['target'])})" if row["target"] else "?"
        state = "-" if row["state"] is None else BREATH_STATES.get(row["state"], row["state"])
        print(f"  {clock(row['t'])}  on {target}  nb.arc {row['arc'] if row['arc'] is not None else '-'}"
              f"  nb.breath {state}")
        whirl = "no Whirl before" if row["since_whirl"] is None else f"{row['since_whirl'] / 1000:.1f} s after a Whirl"
        moved = "?" if row["walked"] is None else f"{row['walked']:.1f} yd"
        print(f"    {whirl}, he moved {moved} in the {BREATH_WALK_MS // 1000} s before")
        frozen = ", ".join(f"{trace.name(guid)} [{row['spread'][guid] or '-'}]" for guid in row["victims"])
        print(f"    frozen [nb.spread]: {frozen or 'nobody'}")
        if row["predicted"] is not None:
            predicted = names(row["predicted"])
        else:
            predicted = "? (no snapshot)" if row["target"] else "? (no cast row, so no target)"
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


NPC_SLIME_POOL = 35176
WORMS = (("Acidmaw", NPC_ACIDMAW), ("Dreadscale", NPC_DREADSCALE))

# 10N, 25N, 10H, 25H
SPELL_PARALYTIC_TOXIN = (66823, 67618, 67619, 67620)
SPELL_BURNING_BILE = 66869
SPELL_BILE_PULSE = (66870, 67621, 67622, 67623)
SPELL_PARALYTIC_SPRAY = (66901, 67615, 67616, 67617)
SPELL_BURNING_SPRAY = (66902, 67627, 67628, 67629)
# The Bile pulse's reach and the Spray's splash
WORM_SPLASH_RADIUS = radius("WORM_SPLASH_RADIUS")
WORM_SPREAD_TRIGGER = radius("WORM_SPREAD_TRIGGER")
SPELL_ACIDIC_SPEW_TICK = (66819, 67609, 67610, 67611)
SPELL_MOLTEN_SPEW_TICK = (66820, 67635, 67636, 67637)
SPELL_SLIME_POOL = (66881, 67638, 67639, 67640)
SPELL_SWEEP = (66794, 67644, 67645, 67646)
SPELL_EMERGE = 66947

WORM_STATES = {"0": "none", "1": "dreadscale mobile", "2": "acidmaw mobile", "3": "under ground",
               "4": "lone dreadscale", "5": "lone acidmaw"}
WORM_UNDER = "3"

# The script's numbers, not the bots': the pool aura casts each 1 s tick at 2 + 0.3 * tick yd up to its
# 60th, and Toxin lasts 60 s with a snare that reaches a full stop at 18 s, 13.5 s on heroic. A pool
# despawns at 30 s on normal, 45 s on 10H and 60 s on 25H, which ends its snapshots.
POOL_BASE_RADIUS = 2.0
POOL_GROWTH = 0.3
POOL_MAX_RADIUS = 20.0
TOXIN_EXPIRED_MS = 59000
TOXIN_STUCK_MS = 18000
TOXIN_STUCK_HEROIC_MS = 13500
# The pulse strips Toxin as it lands, so the removal and the damage row can come in either order.
CURE_WINDOW_MS = 250
# The row goes out at the 1 s cast and the last of the 2.5 s of ticks lands 3.5 s after it.
SPEW_WINDOW_MS = 3500
# A Spray's debuff lands as one burst 1-3 s after its cast row; a later apply has another source.
SPRAY_APPLY_FROM_MS = 800
SPRAY_APPLY_TO_MS = 4000
SPRAY_BURST_MS = 400
CASTER_ROLES = ("ranged", "heal")
# nb.worm values with both worms alive and up
BOTH_WORMS_UP = ("1", "2")
# one snapshot gap longer than this is a hole in the trace, not time spent anywhere
MAX_STEP_MS = 2000

NUMBER_WORD = re.compile(r"-?\d+(?:\.\d+)?")


def worm_spans(trace: Trace) -> list[dict]:
    """Each `nb.worm` value with its span and the combat deaths inside it."""
    deaths = combat_deaths(trace)
    return [{"value": value, "start": start, "stop": stop,
             "deaths": [death.get("g") for death in deaths if start <= death["t"] < stop]}
            for value, start, stop in latch_spans(trace, "nb.worm", pull_end(trace))]


def emerges(trace: Trace) -> list[tuple[int, int]]:
    """`(start, stop)` for the first engage and every time `nb.worm` left 3, each running to the next
    submerge, the next 0 or the end."""
    end = pull_end(trace)
    spans = latch_spans(trace, "nb.worm", end)
    out = []
    for index, (value, start, _) in enumerate(spans):
        if value in ("0", WORM_UNDER):
            continue
        previous = spans[index - 1][0] if index else None
        if previous == WORM_UNDER or (not out and previous in (None, "0")):
            stop = next((later for held, later, _ in spans[index + 1:] if held in ("0", WORM_UNDER)), end)
            out.append((start, stop))
    return out


def own_emerges(snaps: list[dict], stamps: list[int], worms: set[int], since: int, stop: int) -> dict[int, int]:
    """`{worm: ms}` of each worm's first sampled Emerge cast in `[since, stop)`."""
    out: dict[int, int] = {}
    for snap in snaps[bisect.bisect_left(stamps, since):]:
        if snap["t"] >= stop:
            break
        for row in snap.get("u", []):
            if row[0] in worms and len(row) > 10 and row[10] == SPELL_EMERGE:
                out.setdefault(row[0], snap["t"])
    return out


def worm_pickups(trace: Trace) -> list[dict]:
    """Per emerge, per worm sampled alive in it: ms from its own emerge until its target column named
    a tank (None if it never did before the next submerge), that tank, and the non-tanks it targeted
    first.

    The threat wipe under ground only shows here: the worm comes up on whoever threatens it first.
    `nb.worm` leaves 3 at the later worm's emerge, up to 1.5 s after the first, so each worm is timed
    from its own Emerge cast in the snapshots, or from the `nb.worm` change when none was sampled."""
    worms = {guid for _, entry in WORMS for guid in guids_of_entry(trace, entry)}
    snaps = frames(trace)
    stamps = [snap["t"] for snap in snaps]
    under_since = {stop: start for value, start, stop in latch_spans(trace, "nb.worm", pull_end(trace))
                   if value == WORM_UNDER}
    out = []
    for start, stop in emerges(trace):
        since = under_since.get(start)
        up = own_emerges(snaps, stamps, worms, since, stop) if since is not None else {}
        held: dict[int, dict] = {}
        for snap in snaps[bisect.bisect_left(stamps, min(up.values(), default=start)):]:
            if snap["t"] >= stop:
                break
            for row in snap.get("u", []):
                if row[0] not in worms or len(row) <= 7 or row[5] <= 0:
                    continue
                emerged = up.get(row[0], start)
                if snap["t"] < emerged:
                    continue
                pickup = held.setdefault(row[0], {"tank_ms": None, "tank": None, "victims": []})
                victim = row[7]
                if pickup["tank"] is not None or not victim:
                    continue
                if trace.role(victim) == "tank":
                    pickup["tank_ms"] = snap["t"] - emerged
                    pickup["tank"] = victim
                elif victim not in pickup["victims"]:
                    pickup["victims"].append(victim)
        out.append({"t": start, "worms": held})
    return out


def wedge_angle_off(origin, facing: float, point) -> float:
    """Degrees between a facing in radians and the bearing from origin to point, 0 to 180."""
    bearing = math.atan2(point[1] - origin[1], point[0] - origin[0])
    return abs(math.degrees((bearing - facing + math.pi) % (2 * math.pi) - math.pi))


def spew_wedges(trace: Trace) -> list[dict]:
    """Per `wedge` row on a Spew tick: the living non-tanks inside it at the nearest snapshot, and that
    worm's Spew tick victims by role over the next 3.5 s. `arc` is degrees either side of `facing`."""
    roster = roster_guids(trace)
    ticks = [rec for rec in trace.of("dmg")
             if rec.get("sp") in SPELL_ACIDIC_SPEW_TICK + SPELL_MOLTEN_SPEW_TICK]
    out = []
    for rec in trace.of("haz"):
        if rec.get("shape") != "wedge" or rec.get("sp") not in SPELL_ACIDIC_SPEW_TICK + SPELL_MOLTEN_SPEW_TICK:
            continue
        family = SPELL_ACIDIC_SPEW_TICK if rec["sp"] in SPELL_ACIDIC_SPEW_TICK else SPELL_MOLTEN_SPEW_TICK
        origin = (rec.get("x", 0.0), rec.get("y", 0.0))
        facing = float(rec.get("facing", 0.0))
        arc = float(rec.get("arc", 0.0))
        reach = float(rec.get("range", 0.0))

        snap = nearest_frame(trace, rec["t"])
        inside = set() if snap is None else {
            row[0] for row in snap.get("u", [])
            if row[0] in roster and row[5] > 0 and trace.role(row[0]) != "tank"
            and dist2(row[1:3], origin) <= reach and wedge_angle_off(origin, facing, row[1:3]) <= arc}

        hit: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
        for tick in ticks:
            if tick["sp"] in family and rec["t"] <= tick["t"] <= rec["t"] + SPEW_WINDOW_MS:
                hit[trace.role(tick.get("d", 0))][tick.get("d", 0)] += 1

        out.append({
            "t": rec["t"],
            "worm": "Acidmaw" if family is SPELL_ACIDIC_SPEW_TICK else "Dreadscale",
            "facing": facing,
            "arc": arc,
            "inside": inside,
            "hit": dict(hit),
        })
    return out


def pool_radius(age_ms: int) -> float:
    return min(POOL_BASE_RADIUS + POOL_GROWTH * (age_ms // 1000), POOL_MAX_RADIUS)


def slime_pools(trace: Trace) -> list[dict]:
    """Per pool, from its first snapshot: its spot, the ms each living roster member stood inside its
    radius at the time, and its damage rows by victim.

    A pool is a creature, not a dynamic object, and grows, so neither `snap.hz` nor a fixed radius
    says who stood in it."""
    pools = guids_of_entry(trace, NPC_SLIME_POOL)
    if not pools:
        return []
    roster = roster_guids(trace)
    snaps = frames(trace)
    found: dict[int, dict] = {}
    for index, snap in enumerate(snaps):
        rows = snap.get("u", [])
        present = [row for row in rows if row[0] in pools]
        if not present:
            continue
        step = min(snaps[index + 1]["t"] - snap["t"], MAX_STEP_MS) if index + 1 < len(snaps) else 0
        members = [row for row in rows if row[0] in roster and row[5] > 0]
        for pool in present:
            entry = found.setdefault(pool[0], {"guid": pool[0], "first": snap["t"], "x": pool[1], "y": pool[2],
                                               "inside": collections.Counter(), "hits": collections.Counter()})
            entry["last"] = snap["t"]
            reach = pool_radius(snap["t"] - entry["first"])
            for member in members:
                if dist2(member[1:3], pool[1:3]) <= reach:
                    entry["inside"][member[0]] += step

    for rec in trace.of("dmg"):
        if rec.get("sp") in SPELL_SLIME_POOL and rec.get("s") in found:
            found[rec["s"]]["hits"][rec.get("d", 0)] += 1
    return sorted(found.values(), key=lambda entry: entry["first"])


def sweep_victims(trace: Trace) -> dict[str, collections.Counter]:
    """Sweep hits per victim, by role."""
    out: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
    for rec in trace.of("dmg"):
        if rec.get("sp") in SPELL_SWEEP:
            out[trace.role(rec.get("d", 0))][rec.get("d", 0)] += 1
    return dict(out)


def drop_numbers(text: str) -> str:
    return " ".join(word for word in text.split() if not NUMBER_WORD.fullmatch(word))


def wormmove_branches(trace: Trace) -> dict[int, collections.Counter]:
    """`nb.wormmove` per bot by branch, the yards dropped: `move pool full`, `hold spew`, `drag`, ..."""
    return branch_counts(trace, "nb.wormmove", drop_numbers)


def wormhold_branches(trace: Trace) -> dict[int, collections.Counter]:
    """`nb.wormhold` per bot by its first word and reason or cause, the tier dropped: `set pool`,
    `park`, `unpark`, `release drift`, ..."""
    return branch_counts(trace, "nb.wormhold", lambda text: " ".join(text.split()[:2]))


def hold_releases(trace: Trace) -> dict[str, collections.Counter]:
    """`nb.wormhold` releases raid-wide, per cause, counted by role."""
    out: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
    for rec in notes(trace, "nb.wormhold"):
        words = str(rec.get("txt", "")).split()
        if len(words) > 1 and words[0] == "release":
            out[words[1]][trace.role(rec.get("g", 0))] += 1
    return dict(out)


def splash_inside(trace: Trace, target: int, when: int) -> set[int] | None:
    """Living roster members within the splash of `target` in the snapshot nearest `when`, the target
    included. None when that snapshot misses the target."""
    snap = nearest_frame(trace, when)
    if snap is None:
        return None
    rows = {row[0]: row for row in snap.get("u", [])}
    aimed = rows.get(target)
    if aimed is None:
        return None
    roster = roster_guids(trace)
    return {guid for guid, row in rows.items()
            if guid in roster and row[5] > 0 and dist2(row[1:3], aimed[1:3]) <= WORM_SPLASH_RADIUS}


def spray_casts(trace: Trace) -> list[dict]:
    """One row per Spray cast by a worm: its target, who stood in the splash at the cast's end, who
    took its debuff from that worm (the first burst of applies, in order) and each bot's `spray`
    `nb.wormmove` branches during the cast, the yards dropped."""
    worms = {guid for _, entry in WORMS for guid in guids_of_entry(trace, entry)}
    roster = roster_guids(trace)
    applies = [rec for rec in trace.of("aura") if not rec.get("r") and rec.get("d") in roster]
    moves = [(rec["t"], rec.get("g", 0), drop_numbers(str(rec.get("txt", ""))))
             for rec in notes(trace, "nb.wormmove")]

    out = []
    for rec in trace.of("cast"):
        burning = rec.get("sp") in SPELL_BURNING_SPRAY
        if rec.get("s") not in worms or not (burning or rec.get("sp") in SPELL_PARALYTIC_SPRAY):
            continue
        when = rec["t"]
        ends = when + int(rec.get("ct", 0) or 0)
        debuff = (SPELL_BURNING_BILE,) if burning else SPELL_PARALYTIC_TOXIN
        late = [hit for hit in applies if hit.get("sp") in debuff and hit.get("s") == rec["s"]
                and when + SPRAY_APPLY_FROM_MS <= hit["t"] <= when + SPRAY_APPLY_TO_MS]
        took: list[int] = []
        for hit in late:
            if hit["t"] <= late[0]["t"] + SPRAY_BURST_MS and hit["d"] not in took:
                took.append(hit["d"])

        branches: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
        for stamp, guid, text in moves:
            if when <= stamp <= ends and text.split()[1:2] == ["spray"]:
                branches[guid][text] += 1

        out.append({
            "t": when,
            "worm": rec["s"],
            "burning": burning,
            "target": rec.get("tgt", 0),
            "inside": splash_inside(trace, rec.get("tgt", 0), ends),
            "took": took,
            "moves": dict(branches),
        })
    return out


def caster_spread(trace: Trace) -> dict:
    """Over the snapshots with both worms up: per snapshot, the pairs of living casters carrying
    neither Bile nor Toxin within `WORM_SPREAD_TRIGGER` of each other, and how many snapshots had one
    such caster beside two others."""
    up = [(start, stop) for value, start, stop in latch_spans(trace, "nb.worm", pull_end(trace))
          if value in BOTH_WORMS_UP]
    toxins = spans_by_guid(carried(trace, SPELL_PARALYTIC_TOXIN))
    biles = spans_by_guid(carried(trace, (SPELL_BURNING_BILE,)))
    casters = {guid for guid in roster_guids(trace) if trace.role(guid) in CASTER_ROLES}

    pairs: list[int] = []
    crowded = 0
    for snap in frames(trace):
        when = snap["t"]
        if not any(start <= when < stop for start, stop in up):
            continue
        clean = [row for row in snap.get("u", []) if row[0] in casters and row[5] > 0
                 and not carrying(toxins, row[0], when) and not carrying(biles, row[0], when)]
        beside: collections.Counter = collections.Counter()
        count = 0
        for index, first in enumerate(clean):
            for second in clean[index + 1:]:
                if dist2(first[1:3], second[1:3]) <= WORM_SPREAD_TRIGGER:
                    count += 1
                    beside[first[0]] += 1
                    beside[second[0]] += 1
        pairs.append(count)
        crowded += any(n >= 2 for n in beside.values())
    return {"pairs": pairs, "crowded": crowded}


def named(trace: Trace, guid: int) -> str:
    return f"{trace.name(guid)} ({trace.role(guid)})"


def by_role(trace: Trace, table: dict[str, collections.Counter]) -> str:
    return "; ".join(f"{role} " + ", ".join(f"{trace.name(guid)} {n}" for guid, n in counts.most_common())
                     for role, counts in sorted(table.items()))


def show_worms(trace: Trace) -> None:
    print("WORMS")
    spans = worm_spans(trace)
    if not spans:
        print("  no nb.worm rows")
    else:
        print(f"  {'from':>9} {'to':>9} {'secs':>6}  {'state':18} deaths")
        for span in spans:
            dead = ", ".join(trace.name(guid) for guid in span["deaths"]) or "-"
            state = WORM_STATES.get(span["value"], span["value"])
            print(f"  {clock(span['start']):>9} {clock(span['stop']):>9}"
                  f" {(span['stop'] - span['start']) / 1000:6.1f}  {state:18} {dead}")

    pickups = worm_pickups(trace)
    print("\n  emerges, seconds until each worm targeted a tank:" + ("" if pickups else " none"))
    for row in pickups:
        parts = []
        for guid, pickup in sorted(row["worms"].items(), key=lambda item: trace.name(item[0])):
            took = (f"{pickup['tank_ms'] / 1000:.1f} ({trace.name(pickup['tank'])})" if pickup["tank"]
                    else "never")
            before = ", ".join(named(trace, victim) for victim in pickup["victims"])
            parts.append(f"{trace.name(guid).split()[0]} {took}" + (f" after {before}" if before else ""))
        print(f"    {clock(row['t']):>9}  " + ("  ".join(parts) or "no worm sampled"))

    wedges = spew_wedges(trace)
    print("  Spew wedges:" + ("" if wedges else " none"))
    for row in wedges:
        inside = ", ".join(named(trace, guid) for guid in sorted(row["inside"], key=trace.name)) or "nobody"
        print(f"    {clock(row['t']):>9}  {row['worm']:10} facing {row['facing']:.2f} arc {row['arc']:.0f}"
              f"  non-tanks inside: {inside}  ticks: {by_role(trace, row['hit']) or 'none'}")

    pools = slime_pools(trace)
    print("  slime pools:" + ("" if pools else " none sampled"))
    for pool in pools:
        inside = ", ".join(f"{trace.name(guid)} {ms / 1000:.1f}s" for guid, ms in pool["inside"].most_common() if ms)
        hits = ", ".join(f"{trace.name(guid)} {n}" for guid, n in pool["hits"].most_common())
        print(f"    {clock(pool['first'])}-{clock(pool['last'])} ({pool['x']:.1f}, {pool['y']:.1f})"
              f"  inside: {inside or 'nobody'}  hit: {hits or 'nobody'}")
    sources = {pool["guid"] for pool in pools}
    stray = collections.Counter(rec.get("d", 0) for rec in trace.of("dmg")
                                if rec.get("sp") in SPELL_SLIME_POOL and rec.get("s") not in sources)
    if stray:
        print("    pool hits from no sampled pool: "
              + ", ".join(f"{trace.name(guid)} {n}" for guid, n in stray.most_common()))

    print(f"  Sweep hits: {by_role(trace, sweep_victims(trace)) or 'nobody'}")

    show_sprays(trace)

    spread = caster_spread(trace)
    pairs = spread["pairs"]
    if not pairs:
        print("  caster spread: no snapshot with both worms up")
    else:
        print(f"  caster spread over {len(pairs)} snapshot(s) with both worms up, casters carrying neither debuff:"
              f" pairs within {WORM_SPREAD_TRIGGER:.0f} yd median {statistics.median(pairs):g}, max {max(pairs)};"
              f" one beside two others in {spread['crowded'] / len(pairs):.0%}")

    releases = hold_releases(trace)
    print("  nb.wormhold releases by cause:" + ("" if releases else " none"))
    for cause, roles in sorted(releases.items(), key=lambda item: (-sum(item[1].values()), item[0])):
        print(f"    {cause:10} {sum(roles.values()):5}  " + ", ".join(f"{role} {n}" for role, n in roles.most_common()))

    for title, branches in (("nb.wormmove", wormmove_branches(trace)), ("nb.wormhold", wormhold_branches(trace))):
        if not branches:
            print(f"  {title}: none")
            continue
        print(f"  {title}:")
        for guid, counts in sorted(branches.items(), key=lambda item: trace.name(item[0])):
            print(f"    {trace.name(guid)[:14]:14} {trace.role(guid):6} "
                  + ", ".join(f"{name} {n}" for name, n in counts.most_common()))


def show_sprays(trace: Trace) -> None:
    rows = spray_casts(trace)
    print("  Spray casts:" + ("" if rows else " none"))

    def names(guids) -> str:
        return ", ".join(named(trace, guid) for guid in guids) or "nobody"

    for row in rows:
        kind, debuff = ("Burning", "Bile") if row["burning"] else ("Paralytic", "Toxin")
        inside = "? (no snapshot)" if row["inside"] is None else names(sorted(row["inside"], key=trace.name))
        worm = trace.name(row["worm"]).split()[0]
        print(f"    {clock(row['t'])}  {worm:10} {kind:9} on {named(trace, row['target'])}")
        print(f"      within {WORM_SPLASH_RADIUS:.0f} yd at the cast's end: {inside}")
        print(f"      took {debuff}: {names(row['took'])}")
        for guid, counts in sorted(row["moves"].items(), key=lambda item: trace.name(item[0])):
            print(f"      nb.wormmove {trace.name(guid)[:14]:14} " + ", ".join(
                f"{name} {n}" for name, n in counts.most_common()))


def toxin_stuck_ms(diff) -> int:
    return TOXIN_STUCK_HEROIC_MS if diff in HEROIC else TOXIN_STUCK_MS


def carried(trace: Trace, spells) -> list[tuple[int, int, int, bool]]:
    """`(carrier, start, stop, removed)` per aura span on any of these ids, by start. One still up at
    the end runs to the end, `removed` False."""
    end = pull_end(trace)
    opened: dict[int, int] = {}
    out = []
    for rec in trace.of("aura"):
        if rec.get("sp") not in spells:
            continue
        guid = rec.get("d", 0)
        if rec.get("r"):
            if guid in opened:
                out.append((guid, opened.pop(guid), rec["t"], True))
        elif guid not in opened:
            opened[guid] = rec["t"]
    out += [(guid, start, end, False) for guid, start in opened.items()]
    return sorted(out, key=lambda row: (row[1], row[0]))


def toxin_spans(trace: Trace) -> list[dict]:
    """Every Paralytic Toxin span with how it ended: `died`, `cured` (a Bile pulse hit the carrier
    within 250 ms of the removal), `expired` (59 s or more), `other`, or `open` at the end of the file.
    `stuck` once carried past the snare's full stop."""
    pulses: dict[int, list[int]] = collections.defaultdict(list)
    for rec in trace.of("dmg"):
        if rec.get("sp") in SPELL_BILE_PULSE:
            pulses[rec.get("d", 0)].append(rec["t"])
    deaths: dict[int, list[int]] = collections.defaultdict(list)
    for rec in combat_deaths(trace):
        deaths[rec.get("g", 0)].append(rec["t"])
    stuck = toxin_stuck_ms(trace.header.get("diff"))

    out = []
    for guid, start, stop, removed in carried(trace, SPELL_PARALYTIC_TOXIN):
        if any(start <= when <= stop + CURE_WINDOW_MS for when in deaths[guid]):
            ended = "died"
        elif removed and any(abs(when - stop) <= CURE_WINDOW_MS for when in pulses[guid]):
            ended = "cured"
        elif removed and stop - start >= TOXIN_EXPIRED_MS:
            ended = "expired"
        else:
            ended = "other" if removed else "open"
        out.append({"carrier": guid, "start": start, "stop": stop, "end": ended, "stuck": stop - start > stuck})
    return out


def spans_by_guid(spans) -> dict[int, list[tuple[int, int]]]:
    out: dict[int, list[tuple[int, int]]] = collections.defaultdict(list)
    for guid, start, stop, _ in spans:
        out[guid].append((start, stop))
    return out


def carrying(spans: dict[int, list[tuple[int, int]]], guid: int, when: int) -> bool:
    return any(low <= when <= high + CURE_WINDOW_MS for low, high in spans.get(guid, ()))


def pulse_carrier(trace: Trace, hit: dict, spans: list[dict]) -> dict | None:
    """The Bile span a pulse hit belongs to: the carrier nearest its victim within the pulse's reach,
    in the snapshot nearest the hit, other than the victim, its span covering the hit. None when no
    carrier is in reach or no snapshot places the victim."""
    when, victim = hit["t"], hit.get("d", 0)
    snap = nearest_frame(trace, when)
    if snap is None:
        return None
    places = {row[0]: row[1:3] for row in snap.get("u", [])}
    spot = places.get(victim)
    if spot is None:
        return None
    best = None
    for span in spans:
        place = places.get(span["carrier"])
        if span["carrier"] == victim or place is None or not span["start"] <= when <= span["stop"] + CURE_WINDOW_MS:
            continue
        gap = dist2(place, spot)
        if gap <= WORM_SPLASH_RADIUS and (best is None or gap < best[0]):
            best = (gap, span)
    return best[1] if best else None


def bile_spans(trace: Trace) -> list[dict]:
    """Every Burning Bile span, with the pulse hits given to it on bots carrying neither Bile nor Toxin
    at the time, and their damage: damage with nothing to cure.

    Every pulse row names Dreadscale as its source, so a hit goes to the carrier `pulse_carrier`
    places by its victim, and one it places by nobody counts nowhere."""
    toxins = spans_by_guid(carried(trace, SPELL_PARALYTIC_TOXIN))
    bile = carried(trace, (SPELL_BURNING_BILE,))
    biles = spans_by_guid(bile)
    out = [{"carrier": guid, "start": start, "stop": stop, "clean": collections.Counter(), "damage": 0}
           for guid, start, stop, _ in bile]

    for rec in trace.of("dmg"):
        if rec.get("sp") not in SPELL_BILE_PULSE:
            continue
        victim = rec.get("d", 0)
        if carrying(toxins, victim, rec["t"]) or carrying(biles, victim, rec["t"]):
            continue
        span = pulse_carrier(trace, rec, out)
        if span is None:
            continue
        span["clean"][victim] += 1
        span["damage"] += int(rec.get("a", 0) or 0)
    return out


def cure_branches(trace: Trace) -> dict[int, collections.Counter]:
    """`nb.cure` per bot by branch, the guid dropped: `seek`, `run`, `wait`, `none`."""
    return branch_counts(trace, "nb.cure", lambda text: text.split()[0] if text.split() else "")


def show_cure(trace: Trace) -> None:
    print("CURE")
    toxins = toxin_spans(trace)
    if not toxins:
        print("  no Paralytic Toxin carried")
    else:
        print(f"  Paralytic Toxin:\n    {'carrier':14} {'role':6} {'from':>9} {'to':>9} {'secs':>6}  end")
        for span in toxins:
            print(f"    {trace.name(span['carrier'])[:14]:14} {trace.role(span['carrier']):6}"
                  f" {clock(span['start']):>9} {clock(span['stop']):>9} {(span['stop'] - span['start']) / 1000:6.1f}"
                  f"  {span['end']}" + ("  stuck" if span["stuck"] else ""))
        tally = collections.Counter(span["end"] for span in toxins)
        stuck = sum(1 for span in toxins if span["stuck"])
        print("  ended: " + ", ".join(f"{end} {n}" for end, n in tally.most_common())
              + f"; stuck past {toxin_stuck_ms(trace.header.get('diff')) / 1000:.1f} s: {stuck}")

    biles = bile_spans(trace)
    if not biles:
        print("  no Burning Bile carried")
    else:
        print(f"  Burning Bile, pulse hits on bots carrying neither Bile nor Toxin (nearest carrier within"
              f" {WORM_SPLASH_RADIUS:.0f} yd):")
        for span in biles:
            clean = ", ".join(f"{named(trace, guid)} {n}" for guid, n in span["clean"].most_common()) or "none"
            damage = f"  ({span['damage'] / 1000:.1f}k)" if span["damage"] else ""
            print(f"    {trace.name(span['carrier'])[:14]:14} {trace.role(span['carrier']):6}"
                  f" {clock(span['start'])}-{clock(span['stop'])}  {clean}{damage}")
        hits = sum(sum(span["clean"].values()) for span in biles)
        damage = sum(span["damage"] for span in biles)
        print(f"  raid: {hits} clean hit(s), {damage / 1000:.1f}k damage")

    branches = cure_branches(trace)
    if not branches:
        print("  nb.cure: none")
        return
    print("  nb.cure:")
    for guid, counts in sorted(branches.items(), key=lambda item: trace.name(item[0])):
        print(f"    {trace.name(guid)[:14]:14} {trace.role(guid):6} "
              + ", ".join(f"{name} {n}" for name, n in counts.most_common()))


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
    ("targets", "each DPS bot's time on its nb.dps pick, another beast or neither, by branch", show_targets),
    ("charge", "every Icehowl charge: gaze, lane, outcome, who stood at its end, each bot's daze", show_charge),
    ("breath", "every Arctic Breath: target, who froze, who the cone predicts, spread, nb.breath, his Whirl walk",
     show_breath),
    ("snobold", "snobold riders and the DPS picks", show_snobold),
    ("bomb", "every Fire Bomb: target, impact hits, who stood inside, dodges", show_bomb),
    ("worms", "worm form spans, emerge pickups, Spew wedges, slime pools, Sweep, Spray casts, caster spread,"
     " hold releases, reposition and hold branches", show_worms),
    ("cure", "Paralytic Toxin spans and how each ended, Burning Bile pulses on the clean, cure branches",
     show_cure),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
