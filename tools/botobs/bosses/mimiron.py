#!/usr/bin/env python3
"""Score a Mimiron pull from a RaidObs trace: who walked, what for, and what it cost.

    mimiron.py <file>            every section
    mimiron.py <file> --phases   phase spans, and every death by phase and killer
    mimiron.py <file> --heal     the phase 2 healing race, 2.5 s at a time
    mimiron.py <file> --walk     walking time per phase and role, charged to the move that started it
    mimiron.py <file> --flips    A-B-A between one bot's moves, and what the formation walked back from
    mimiron.py <file> --burst    every Rapid Burst: carrier, cone, outside, and the ticks each took
    mimiron.py <file> --bomb     every Frost Bomb: who had to run, casting lost, where escapes landed
    mimiron.py <file> --slots    how often formation slots moved, and why

What the generic views get wrong here, and what this reads instead:

- **Phases come from `mimiron.phase`**: 1 to 4, 5 for a handover, 0 for nothing attackable, which
  also covers the gap between the MK II dying and VX-001 arriving. They print as P1..P4, H1.., Z1..
- **`snap.u[8]` says a bot is moving, not why.** Walking is charged to the bot's last accepted move
  inside 6 s before the sample.
- **A Frost Bomb's fuse starts at its summon, not at VX-001's cast.** 64623 has a 2 s cast and a
  flight, and the bomb first shows up 2.5 to 4 s after it; the blast lands 10 s after that.
- **A move record carries its destination only.** Where the bot started is its last sample before it.
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
from raidobs.geometry import dist2, frames, guids_of_entry, radius  # noqa: E402
from raidobs.probes import emitted_keys, silent_keys  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, death_records, notes, roster_guids  # noqa: E402

NPC_VX001 = 33651
NPC_FROST_BOMB = 34149

SPELL_RAPID_BURST = 63382
SPELL_RAPID_BURST_HITS = (64531, 64532, 63387, 64019)
SPELL_HEAT_WAVE = 64533
SPELL_FLAMES = 64566
SPELL_FROST_BOMB = 64623
SPELL_FROST_BOMB_EXPLOSIONS = (64626, 65333)
SPELL_SPINNING_UP = 63414
SPELL_ROCKET_STRIKE = (64402, 65034)

FORMATION = "mimiron arc spread action"
BOMB_DODGE = "mimiron frost bomb action"
BURST_STEP = "mimiron rapid burst action"

# Read from the source so a retune shows up here without a second edit.
BOMB_RADIUS = radius("ULDUAR_MIMIRON_FROST_BOMB_RADIUS")
BOMB_STAND = radius("ULDUAR_MIMIRON_FROST_BOMB_STAND_RADIUS")
BOMB_CLEARANCE = radius("ULDUAR_MIMIRON_FROST_BOMB_CLEARANCE")

# ULDUAR_MIMIRON_RAPID_BURST_HALF_ANGLE is an expression the source parser does not read.
BURST_HALF_ANGLE = math.radians(30.0)
BURST_WINDOW_MS = 3400  # the 3 s aura plus the last tick landing
FUSE_MS = 10000
# 64623's 2 s cast plus the flight to the node it picked.
LANDING_MS = 6000
# In casting range of VX-001 centre to centre: AiPlayerbot.SpellDistance 28.5, its 8 yd reach, a
# player's 1.5.
VX_REACH = 38.0

PHASE_NAMES = {"1": "P1", "2": "P2", "3": "P3", "4": "P4", "5": "H", "0": "Z"}
ROLES = ("heal", "ranged", "melee", "tank")
BUCKET_MS = 2500
ATTRIBUTE_MS = 6000
LEG_GAP_MS = 8000
FORMATION_CAUSE_MS = 10000
SAMPLE_MS = 250
# A snapshot gap past this is a hole in the trace, and weighs one ordinary sample.
HOLE_MS = 1000

SLOT = re.compile(r"(\S+) (\d+)/(\d+) (-?\d+),(-?\d+),(-?\d+)")


def pull_end(trace: Trace) -> int:
    ends = trace.of("end")
    if ends:
        return ends[-1]["t"]
    return trace.records[-1].get("t", 0) if trace.records else 0


def phases(trace: Trace) -> list[tuple[int, int, str]]:
    """`(start, stop, label)` per stretch the `mimiron.phase` latch held one value."""
    marks: list[tuple[int, str]] = []
    for rec in notes(trace, "mimiron.phase"):
        value = str(rec.get("txt", ""))
        if not marks or marks[-1][1] != value:
            marks.append((rec["t"], value))

    end = pull_end(trace)
    seen: collections.Counter = collections.Counter()
    out = []
    for index, (when, value) in enumerate(marks):
        stop = marks[index + 1][0] if index + 1 < len(marks) else end
        label = PHASE_NAMES.get(value, value)
        if label in ("H", "Z"):
            seen[label] += 1
            label = f"{label}{seen[label]}"
        out.append((when, stop, label))
    return out


def phase_at(spans, when: int) -> str:
    for start, stop, label in spans:
        if start <= when < stop:
            return label
    if spans and when >= spans[-1][1]:
        return spans[-1][2]
    return "pre"


def span_of(spans, label: str):
    return next(((start, stop) for start, stop, name in spans if name == label), None)


def weights(snaps: list[dict]):
    """`(snap, ms)` per snapshot but the last, weighted by the gap to the next one."""
    for index in range(len(snaps) - 1):
        gap = snaps[index + 1]["t"] - snaps[index]["t"]
        yield snaps[index], SAMPLE_MS if gap > HOLE_MS else gap


def living(row) -> bool:
    return len(row) > 5 and row[5] > 0


def moving(row) -> bool:
    return len(row) > 8 and bool(row[8])


def casting(row) -> bool:
    return len(row) > 10 and bool(row[10])


class Samples:
    """Each snapshot's rows by guid, built on first ask, so views can look a time up by bisecting."""

    def __init__(self, trace: Trace):
        self.snaps = frames(trace)
        self.stamps = [snap["t"] for snap in self.snaps]
        self._rows: dict[int, dict[int, list]] = {}

    def rows(self, index: int) -> dict[int, list]:
        if index not in self._rows:
            self._rows[index] = {row[0]: row for row in self.snaps[index].get("u", [])}
        return self._rows[index]

    def before(self, when: int) -> dict[int, list]:
        """Every row of the last snapshot at or before `when`."""
        index = bisect.bisect_right(self.stamps, when) - 1
        return self.rows(index) if index >= 0 else {}

    def window(self, low: int, high: int):
        start = bisect.bisect_left(self.stamps, low)
        stop = bisect.bisect_left(self.stamps, high)
        return [self.rows(index) for index in range(start, stop)]


def accepted_moves(trace: Trace) -> dict[int, list[dict]]:
    """Per roster bot, the moves that reached a MotionMaster, in time order."""
    roster = roster_guids(trace)
    out: dict[int, list[dict]] = collections.defaultdict(list)
    for rec in trace.of("move"):
        if rec.get("ok") == 1 and rec.get("g") in roster:
            out[rec["g"]].append(rec)
    return out


def missing_probes(trace: Trace) -> list[str]:
    return [key for key, _, _ in silent_keys(emitted_keys(trace), encounter_of(trace))]


def show_banner(trace: Trace) -> None:
    end = pull_end(trace)
    outcome = trace.of("end")[-1].get("out", "?") if trace.of("end") else "no end record"
    encounter = encounter_of(trace)
    print(f"{encounter}  {outcome} at {clock(end)}  {len(combat_deaths(trace))} death(s)")
    if encounter != "mimiron":
        print("  not a Mimiron pull, every section below reads empty")
        return

    gone = missing_probes(trace)
    print("all mimiron.* probes present" if not gone else f"probes absent from this trace: {', '.join(gone)}")


# --------------------------------------------------------------------------------------------- phases

def deaths_by_phase(trace: Trace) -> dict[str, list[dict]]:
    spans = phases(trace)
    out: dict[str, list[dict]] = collections.defaultdict(list)
    for rec in combat_deaths(trace):
        out[phase_at(spans, rec["t"])].append(rec)
    return out


def show_phases(trace: Trace) -> None:
    print("PHASES")
    spans = phases(trace)
    if not spans:
        print("  no mimiron.phase rows")
        return

    dead = deaths_by_phase(trace)
    resets = sum(1 for rec in death_records(trace) if rec.get("cause") == "reset")
    for start, stop, label in spans:
        rows = dead.get(label, [])
        print(f"  {label:4} {clock(start)} .. {clock(stop)}  {(stop - start) / 1000:6.1f} s  {len(rows):2} dead")
        for rec in rows:
            blow = rec.get("blow") or [0, 0]
            print(f"         {clock(rec['t'])}  {trace.name(rec['g'])[:14]:14} {trace.role(rec['g']):6}"
                  f"  by {trace.name(rec.get('killer'))} ({blow[1] if len(blow) > 1 else 0})")
    if resets:
        print(f"  plus {resets} to the wipe command")


# ----------------------------------------------------------------------------------------------- heal

def role_shares(trace: Trace, low: int, high: int) -> dict[str, dict[str, float]]:
    """Per role, the share of living time spent moving and casting between `low` and `high`."""
    roster = roster_guids(trace)
    alive: collections.Counter = collections.Counter()
    walked: collections.Counter = collections.Counter()
    cast: collections.Counter = collections.Counter()
    for snap, ms in weights(frames(trace)):
        if not low <= snap["t"] < high:
            continue
        for row in snap.get("u", []):
            if row[0] not in roster or not living(row):
                continue
            role = trace.role(row[0])
            alive[role] += ms
            walked[role] += ms if moving(row) else 0
            cast[role] += ms if casting(row) else 0
    return {role: {"moving": walked[role] / alive[role], "casting": cast[role] / alive[role],
                   "seconds": alive[role] / 1000.0}
            for role in alive}


def intake(trace: Trace, low: int, high: int) -> collections.Counter:
    roster = roster_guids(trace)
    out: collections.Counter = collections.Counter()
    for rec in trace.of("dmg"):
        if low <= rec["t"] < high and rec.get("d") in roster:
            out[rec.get("sp", 0)] += rec.get("a", 0)
    return out


def heal_rows(trace: Trace, label: str = "P2") -> list[dict]:
    """One row per 2.5 s of the phase: raid health, healers walking and casting, damage in by its three
    big sources, effective healing out, and what was cast or died inside it."""
    span = span_of(phases(trace), label)
    if not span:
        return []
    low, high = span
    roster = roster_guids(trace)
    heals = {guid for guid in roster if trace.role(guid) == "heal"}
    buckets: dict[int, dict] = collections.defaultdict(lambda: {
        "hp": [], "heal_rows": 0, "heal_moving": 0, "heal_casting": 0, "in": 0, "heat": 0, "burst": 0,
        "fire": 0, "healed": 0, "events": []})

    for snap in frames(trace):
        if not low <= snap["t"] < high:
            continue
        row_bucket = buckets[(snap["t"] - low) // BUCKET_MS]
        for row in snap.get("u", []):
            if row[0] not in roster or not living(row):
                continue
            row_bucket["hp"].append(row[5])
            if row[0] in heals:
                row_bucket["heal_rows"] += 1
                row_bucket["heal_moving"] += moving(row)
                row_bucket["heal_casting"] += casting(row)

    events = {SPELL_FROST_BOMB: "BOMB", SPELL_SPINNING_UP: "spinup", SPELL_HEAT_WAVE: "heatwave"}
    events.update({spell: "rocket" for spell in SPELL_ROCKET_STRIKE})
    for rec in trace.records:
        when = rec.get("t", 0)
        if not low <= when < high:
            continue
        bucket = buckets[(when - low) // BUCKET_MS]
        kind = rec.get("e")
        if kind == "dmg" and rec.get("d") in roster:
            amount = rec.get("a", 0)
            bucket["in"] += amount
            spell = rec.get("sp")
            if spell == SPELL_HEAT_WAVE:
                bucket["heat"] += amount
            elif spell in SPELL_RAPID_BURST_HITS:
                bucket["burst"] += amount
            elif spell == SPELL_FLAMES:
                bucket["fire"] += amount
        elif kind == "heal" and rec.get("d") in roster:
            bucket["healed"] += rec.get("a", 0) - rec.get("oh", 0)
        elif kind == "cast" and rec.get("sp") in events:
            bucket["events"].append(events[rec["sp"]])
        elif kind == "death" and rec.get("g") in roster and rec.get("cause") != "reset":
            bucket["events"].append(trace.name(rec["g"]))

    rows = []
    for index in sorted(buckets):
        bucket = buckets[index]
        rows.append({
            "t": low + index * BUCKET_MS,
            "hp": statistics.mean(bucket["hp"]) if bucket["hp"] else None,
            "heal_moving": bucket["heal_moving"] / bucket["heal_rows"] if bucket["heal_rows"] else None,
            "heal_casting": bucket["heal_casting"] / bucket["heal_rows"] if bucket["heal_rows"] else None,
            **{key: bucket[key] for key in ("in", "heat", "burst", "fire", "healed", "events")},
        })
    return rows


def show_heal(trace: Trace) -> None:
    print("HEAL (phase 2)")
    span = span_of(phases(trace), "P2")
    if not span:
        print("  no phase 2 in this pull")
        return

    low, high = span
    taken = intake(trace, low, high)
    total = sum(taken.values())
    print(f"  {clock(low)} .. {clock(high)}, {total:,} taken:"
          + ", ".join(f" {trace.spell(spell)} {amount / total * 100:.0f}%"
                      for spell, amount in taken.most_common(4) if total))
    shares = role_shares(trace, low, high)
    print("  " + "   ".join(f"{role} moving {shares[role]['moving'] * 100:.1f}% casting "
                            f"{shares[role]['casting'] * 100:.1f}%" for role in ROLES if role in shares))

    def pct(value):
        return "  -" if value is None else f"{value * 100:3.0f}"

    print(f"\n  {'time':9} {'hp':>5}  heal mov/cast  {'taken':>8} (heat burst fire)  {'healed':>8}  events")
    for row in heal_rows(trace):
        hp = f"{row['hp']:5.1f}" if row["hp"] is not None else "    -"
        print(f"  {clock(row['t'])} {hp}     {pct(row['heal_moving'])}/{pct(row['heal_casting'])}"
              f"      {row['in']:8,} ({row['heat'] // 1000:3}k {row['burst'] // 1000:3}k {row['fire'] // 1000:3}k)"
              f"  {row['healed']:8,}  {' '.join(row['events'])}")


# ----------------------------------------------------------------------------------------------- walk

def walk_table(trace: Trace) -> dict[tuple[str, str], dict]:
    """Per (phase, role): living ms, moving ms, and the moving ms by the mover that started each walk."""
    spans = phases(trace)
    roster = roster_guids(trace)
    moves = accepted_moves(trace)
    stamps = {guid: [rec["t"] for rec in recs] for guid, recs in moves.items()}
    out: dict[tuple[str, str], dict] = collections.defaultdict(
        lambda: {"alive": 0, "moving": 0, "by": collections.Counter()})

    for snap, ms in weights(frames(trace)):
        label = phase_at(spans, snap["t"])
        for row in snap.get("u", []):
            guid = row[0]
            if guid not in roster or not living(row):
                continue
            cell = out[(label, trace.role(guid))]
            cell["alive"] += ms
            if not moving(row):
                continue
            cell["moving"] += ms
            mover = "?"
            index = bisect.bisect_right(stamps.get(guid, []), snap["t"]) - 1
            if index >= 0 and snap["t"] - stamps[guid][index] <= ATTRIBUTE_MS:
                mover = moves[guid][index].get("by", "?")
            cell["by"][mover] += ms
    return out


def show_walk(trace: Trace) -> None:
    print("WALK")
    table = walk_table(trace)
    if not table:
        print("  nothing sampled")
        return

    for _, _, label in phases(trace):
        if label[0] not in "PH":
            continue
        for role in ROLES:
            cell = table.get((label, role))
            if not cell or not cell["alive"]:
                continue
            top = ", ".join(f"{mover} {ms / cell['alive'] * 100:.0f}%" for mover, ms in cell["by"].most_common(5)
                            if ms / cell["alive"] >= 0.01)
            print(f"  {label:3} {role:6} moving {cell['moving'] / cell['alive'] * 100:4.1f}% of"
                  f" {cell['alive'] / 1000:5.0f} bot-s  {top}")
    print("\n  '?' is walking with no accepted move of the bot's own inside 6 s: a human, or a chase")


# ---------------------------------------------------------------------------------------------- flips

def legs(trace: Trace) -> dict[int, list[dict]]:
    """Per bot, accepted moves with a mover's back-to-back repeats inside 8 s folded into one leg."""
    out: dict[int, list[dict]] = {}
    for guid, recs in accepted_moves(trace).items():
        folded: list[dict] = []
        for rec in recs:
            if folded and folded[-1].get("by") == rec.get("by") and rec["t"] - folded[-1]["t"] < LEG_GAP_MS:
                folded[-1] = rec
                continue
            folded.append(rec)
        out[guid] = folded
    return out


def flip_counts(trace: Trace) -> collections.Counter:
    """(phase, "a <-> b") for every leg that went back to the mover two legs before it."""
    spans = phases(trace)
    out: collections.Counter = collections.Counter()
    for folded in legs(trace).values():
        for index in range(2, len(folded)):
            first, middle, last = folded[index - 2], folded[index - 1], folded[index]
            if (first.get("by") == last.get("by") and middle.get("by") != last.get("by")
                    and last["t"] - first["t"] < 2 * LEG_GAP_MS):
                pair = " <-> ".join(sorted((last.get("by", "?"), middle.get("by", "?"))))
                out[(phase_at(spans, last["t"]), pair)] += 1
    return out


def formation_yards(trace: Trace) -> dict[tuple[str, str], list]:
    """(phase, what came before) -> [legs, yards] for every accepted formation leg a ranged bot or
    healer walked, from where it stood to where it was sent."""
    spans = phases(trace)
    samples = Samples(trace)
    out: dict[tuple[str, str], list] = collections.defaultdict(lambda: [0, 0.0])
    for guid, recs in accepted_moves(trace).items():
        if trace.role(guid) not in ("ranged", "heal"):
            continue
        previous = None
        for rec in recs:
            if rec.get("by") == FORMATION:
                row = samples.before(rec["t"]).get(guid)
                if row:
                    if previous is None:
                        cause = "(nothing before)"
                    elif previous.get("by") == FORMATION:
                        cause = "(formation again)"
                    elif rec["t"] - previous["t"] < FORMATION_CAUSE_MS:
                        cause = previous.get("by", "?")
                    else:
                        cause = "(nothing inside 10 s)"
                    cell = out[(phase_at(spans, rec["t"]), cause)]
                    cell[0] += 1
                    cell[1] += dist2((row[1], row[2]), (rec["x"], rec["y"]))
            previous = rec
    return out


def show_flips(trace: Trace) -> None:
    print("FLIPS")
    counts = flip_counts(trace)
    yards = formation_yards(trace)
    labels = [label for _, _, label in phases(trace) if label[0] in "PH"]
    if not counts and not yards:
        print("  no accepted moves")
        return

    for label in labels:
        pairs = sorted(((n, pair) for (where, pair), n in counts.items() if where == label), reverse=True)[:6]
        if pairs:
            print(f"  {label:3} A-B-A  " + "; ".join(f"{pair} {n}" for n, pair in pairs))
        walked = sorted(((cell[1], cell[0], cause) for (where, cause), cell in yards.items() if where == label),
                        reverse=True)
        if walked:
            total = sum(row[0] for row in walked)
            print(f"      formation walked {total:.0f} yd, after: "
                  + "; ".join(f"{cause} {yd:.0f} yd/{n}" for yd, n, cause in walked[:6]))


# ---------------------------------------------------------------------------------------------- burst

def burst_rows(trace: Trace) -> list[dict]:
    """One row per Rapid Burst: the carrier's ticks, and every other living bot inside or outside the
    60 degree cone VX-001 held on the carrier, with its ticks and whether it stepped."""
    roster = roster_guids(trace)
    vx = guids_of_entry(trace, NPC_VX001)
    samples = Samples(trace)
    hits: dict[int, list[int]] = collections.defaultdict(list)
    for rec in trace.of("dmg"):
        if rec.get("sp") in SPELL_RAPID_BURST_HITS:
            hits[rec.get("d")].append(rec["t"])
    steps = {guid: [rec["t"] for rec in recs if rec.get("by") == BURST_STEP]
             for guid, recs in accepted_moves(trace).items()}

    rows = []
    for cast in trace.of("cast"):
        if cast.get("sp") != SPELL_RAPID_BURST:
            continue
        when = cast["t"]
        units = samples.before(when)
        boss = next((units[guid] for guid in vx if guid in units), None)
        carrier = units.get(cast.get("tgt"))
        if not boss or not carrier:
            continue

        centreline = math.atan2(carrier[2] - boss[2], carrier[1] - boss[1])
        row = {"t": when, "carrier": cast.get("tgt"), "carrier_ticks": 0, "cone": [], "outside": []}
        for guid, unit in units.items():
            if guid not in roster or not living(unit):
                continue
            ticks = sum(1 for hit in hits.get(guid, []) if when <= hit < when + BURST_WINDOW_MS)
            if guid == row["carrier"]:
                row["carrier_ticks"] = ticks
                continue
            bearing = math.atan2(unit[2] - boss[2], unit[1] - boss[1])
            off = abs((bearing - centreline + math.pi) % (2 * math.pi) - math.pi)
            stepped = any(when - 300 <= step < when + 3000 for step in steps.get(guid, []))
            row["cone" if off < BURST_HALF_ANGLE else "outside"].append((guid, ticks, stepped))
        rows.append(row)
    return rows


def show_burst(trace: Trace) -> None:
    print("BURST")
    rows = burst_rows(trace)
    if not rows:
        print("  no Rapid Burst cast with VX-001 and its carrier sampled")
        return

    def mean_ticks(members):
        return sum(ticks for _, ticks, _ in members) / len(members) if members else 0.0

    cone = [member for row in rows for member in row["cone"]]
    outside = [member for row in rows for member in row["outside"]]
    carrier = [row["carrier_ticks"] for row in rows]
    total = sum(carrier) + sum(ticks for _, ticks, _ in cone + outside)
    print(f"  {len(rows)} casts, {total} hits, {total / len(rows):.1f} a cast")
    print(f"  carrier   {len(carrier):4} bot-casts  {sum(carrier) / len(carrier):4.2f} ticks each")
    stepped = [member for member in cone if member[2]]
    still = [member for member in cone if not member[2]]
    if stepped:
        print(f"  cone step {len(stepped):4} bot-casts  {mean_ticks(stepped):4.2f} ticks each")
    print(f"  cone      {len(still):4} bot-casts  {mean_ticks(still):4.2f} ticks each"
          + ("  (stood still)" if stepped else ""))
    print(f"  outside   {len(outside):4} bot-casts  {mean_ticks(outside):4.2f} ticks each")


# ----------------------------------------------------------------------------------------------- bomb

def bomb_rows(trace: Trace) -> list[dict]:
    """One row per Frost Bomb cast. The bomb exists only from the summon at the end of 64623's 2 s cast
    and flight, and its 10 s fuse starts there, so everything keys off the bomb's first sample. A cast
    with no bomb after it summoned nothing, and its row says so."""
    roster = roster_guids(trace)
    vx = guids_of_entry(trace, NPC_VX001)
    bombs = guids_of_entry(trace, NPC_FROST_BOMB)
    samples = Samples(trace)
    blasts = [rec for rec in trace.of("dmg") if rec.get("sp") in SPELL_FROST_BOMB_EXPLOSIONS]
    dodges = [rec for recs in accepted_moves(trace).values() for rec in recs if rec.get("by") == BOMB_DODGE]

    sampled = {}
    for index, snap in enumerate(samples.snaps):
        for guid in bombs:
            if guid not in sampled and guid in samples.rows(index):
                row = samples.rows(index)[guid]
                sampled[guid] = (snap["t"], row[1], row[2])

    rows = []
    for cast in trace.of("cast"):
        if cast.get("sp") != SPELL_FROST_BOMB:
            continue
        when = cast["t"]
        row = {"t": when, "seen": None, "spot": None, "inside": collections.Counter(), "moving": 0.0,
               "casting": 0.0, "out_of_reach": 0, "hits": 0, "escapes": 0, "short_stand": 0,
               "short_clearance": 0}
        rows.append(row)
        first = min((found for found in sampled.values() if when <= found[0] <= when + LANDING_MS),
                    default=None)
        if not first:
            continue

        seen, spot = first[0], (first[1], first[2])
        row["seen"], row["spot"] = seen, spot
        for guid, unit in samples.before(seen).items():
            if guid in roster and living(unit) and dist2((unit[1], unit[2]), spot) < BOMB_RADIUS:
                row["inside"][trace.role(guid)] += 1

        rows_seen = walked = cast_rows = 0
        for units in samples.window(seen, seen + FUSE_MS):
            for guid, unit in units.items():
                if guid in roster and living(unit) and trace.role(guid) in ("ranged", "heal"):
                    rows_seen += 1
                    walked += moving(unit)
                    cast_rows += casting(unit)
        row["moving"] = walked / rows_seen if rows_seen else 0.0
        row["casting"] = cast_rows / rows_seen if rows_seen else 0.0

        end_units = samples.before(seen + FUSE_MS)
        boss = next((end_units[guid] for guid in vx if guid in end_units), None)
        if boss:
            row["out_of_reach"] = sum(
                1 for guid, unit in end_units.items()
                if guid in roster and living(unit) and trace.role(guid) in ("ranged", "heal")
                and dist2((unit[1], unit[2]), (boss[1], boss[2])) > VX_REACH)

        row["hits"] = sum(1 for rec in blasts if seen <= rec["t"] <= seen + FUSE_MS + 1500)
        for rec in dodges:
            if when <= rec["t"] <= seen + FUSE_MS + 500:
                landed = dist2((rec["x"], rec["y"]), spot)
                row["escapes"] += 1
                row["short_stand"] += landed < BOMB_STAND
                row["short_clearance"] += landed < BOMB_CLEARANCE
    return rows


def show_bomb(trace: Trace) -> None:
    print("BOMB")
    rows = bomb_rows(trace)
    if not rows:
        print("  no Frost Bomb cast")
        return

    for row in rows:
        if row["seen"] is None:
            print(f"  {clock(row['t'])}  cast, no bomb sampled after it")
            continue
        inside = ", ".join(f"{role} {n}" for role, n in row["inside"].most_common())
        print(f"  {clock(row['t'])}  bomb {(row['seen'] - row['t']) / 1000:.1f} s after the cast; inside"
              f" {BOMB_RADIUS:.0f} yd: {sum(row['inside'].values())} ({inside or 'nobody'})")
        print(f"            ranged and healers over the fuse: moving {row['moving'] * 100:.0f}%, casting"
              f" {row['casting'] * 100:.0f}%, {row['out_of_reach']} out of VX-001's reach at the blast;"
              f" blast hits {row['hits']}")
        if row["escapes"]:
            print(f"            {row['escapes']} escapes, {row['short_stand']} under {BOMB_STAND:.0f} yd,"
                  f" {row['short_clearance']} under {BOMB_CLEARANCE:.0f}")


# ---------------------------------------------------------------------------------------------- slots

def slot_churn(trace: Trace) -> dict[str, dict]:
    """Per phase, how each bot's `mimiron.slot` changed: a new shape, a new count (somebody died or
    came back), a new index, or the same index moved (the shape itself slid). Yards are what the slot
    moved, to the whole yard the note rounds to."""
    spans = phases(trace)
    last: dict[int, tuple] = {}
    out: dict[str, dict] = collections.defaultdict(
        lambda: {"new": 0, "count": [], "index": [], "anchor": []})
    for rec in notes(trace, "mimiron.slot"):
        found = SLOT.match(str(rec.get("txt", "")))
        guid = rec["g"]
        if not found:
            last.pop(guid, None)
            continue
        now = (found.group(1), int(found.group(2)), int(found.group(3)),
               float(found.group(4)), float(found.group(5)))
        before = last.get(guid)
        last[guid] = now
        cell = out[phase_at(spans, rec["t"])]
        if not before or before[0] != now[0]:
            cell["new"] += 1
            continue
        moved = dist2(before[3:5], now[3:5])
        if before[2] != now[2]:
            cell["count"].append(moved)
        elif before[1] != now[1]:
            cell["index"].append(moved)
        else:
            cell["anchor"].append(moved)
    return out


def show_slots(trace: Trace) -> None:
    print("SLOTS")
    churn = slot_churn(trace)
    if not churn:
        print("  no mimiron.slot rows")
        return

    for _, _, label in phases(trace):
        cell = churn.get(label)
        if not cell:
            continue
        parts = [f"new {cell['new']}"]
        for kind in ("count", "index", "anchor"):
            moved = cell[kind]
            if moved:
                parts.append(f"{kind} {len(moved)} (median {statistics.median(moved):.1f} yd,"
                             f" over 5 yd {sum(1 for yd in moved if yd > 5)})")
        print(f"  {label:4} " + "  ".join(parts))


SECTIONS = (
    ("phases", "phase spans and deaths by phase", show_phases),
    ("heal", "the phase 2 healing race, 2.5 s at a time", show_heal),
    ("walk", "walking time per phase and role, by mover", show_walk),
    ("flips", "A-B-A between moves, and formation yards by cause", show_flips),
    ("burst", "Rapid Burst carrier, cone and outside ticks", show_burst),
    ("bomb", "Frost Bomb evacuations and where escapes landed", show_bomb),
    ("slots", "formation slot churn per phase", show_slots),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
