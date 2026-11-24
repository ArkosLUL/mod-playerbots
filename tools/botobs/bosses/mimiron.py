#!/usr/bin/env python3
"""Score a Mimiron pull from a RaidObs trace: who walked, what for, and what it cost."""
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
from raidobs.geometry import anchor, dist2, frames, guids_of_entry, radius  # noqa: E402
from raidobs.probes import emitted_keys, silent_keys  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, death_records, notes, roster_guids  # noqa: E402

NPC_LEVIATHAN_MKII = 33432
NPC_VX001 = 33651
NPC_FROST_BOMB = 34149
NPC_AERIAL_COMMAND_UNIT = 33670
NPC_ASSAULT_BOT = 34057
NPC_JUNK_BOT = 33855
NPC_EMERGENCY_FIRE_BOT = 34147
NPC_FLAMES_SPREAD = 34121
NPC_BOMB_BOT = 33836

SPELL_RAPID_BURST = 63382
SPELL_RAPID_BURST_HITS = (64531, 64532, 63387, 64019)
SPELL_HEAT_WAVE = 64533
SPELL_FLAMES = 64566
SPELL_FROST_BOMB = 64623
SPELL_FROST_BOMB_EXPLOSIONS = (64626, 65333)
SPELL_SPINNING_UP = 63414
SPELL_ROCKET_STRIKE = (64402, 65034)
SPELL_WATER_SPRAY = 64619
SPELL_DEAFENING_SIREN = 64616
SPELL_FROST_RESISTANCE_AURA = (19888, 19897, 19898, 27152, 48945)
SPELL_LASER_BARRAGE = 63293
SPELL_MINE_EXPLOSIONS = (63009, 66351)
SPELL_BOMB_BOT_BLAST = 63801
SPELL_PLASMA_BALL_P4 = 65647
SPELL_SHOCK_BLAST = 63631
SPELL_HAND_PULSE = (64348, 64352, 64536, 64537)
# Raid cooldowns for --heal's events, off the caster's cast record. Heroism and Bloodlust share one name:
# a raid only ever has one of them, and which one says nothing but the faction.
COOLDOWN_EVENTS = {64205: "guardian", 64843: "hymn", 31821: "mastery", 10060: "PI", 33206: "painsup",
                   32182: "lust", 2825: "lust",
                   **{rank: "tranq" for rank in (740, 8918, 9862, 9863, 26983, 48446, 48447)}}

FORMATION = "mimiron arc spread action"
BOMB_DODGE = "mimiron frost bomb action"
BURST_STEP = "mimiron rapid burst action"
FLAMES_DODGE = "mimiron dodge flames action"
FIREBOT_STEP = "mimiron fire bot action"
FIREBOT_AOE_GUARD = "mimiron fire bot aoe guard"
BOMB_BOT_STEP = "mimiron bomb bot action"
SHOCK_ESCAPE = "mimiron shock blast action"
BOMB_BOT_REFUSED = re.compile(r"bombbot(\d+)")
BOMB_BOT_AIM_LEAD_MS = 300
BOMB_BOT_KILL_MS = 300

# Read from the source so a retune shows up here without a second edit.
BOMB_RADIUS = radius("ULDUAR_MIMIRON_FROST_BOMB_RADIUS")
BOMB_STAND = radius("ULDUAR_MIMIRON_FROST_BOMB_STAND_RADIUS")
BOMB_CLEARANCE = radius("ULDUAR_MIMIRON_FROST_BOMB_CLEARANCE")
SPRAY_LENGTH = radius("ULDUAR_MIMIRON_FIREBOT_SPRAY_LENGTH")
SPRAY_HALF_WIDTH = radius("ULDUAR_MIMIRON_FIREBOT_SPRAY_HALF_WIDTH")
SPRAY_REACH = radius("ULDUAR_MIMIRON_FIREBOT_SPRAY_REACH")
# Inside this the beams hit at any bearing.
BARRAGE_BOUNDARY = radius("ULDUAR_MIMIRON_BARRAGE_BOUNDARY")
# How long before a spray hit the victim is checked against the lanes: time enough to step out.
SPRAY_LEAD_MS = 1000
# The Aerial Command Unit rests at z 379.3 hovering and lands on 364.3, so anything above this is in
# the air whatever the Magnetic Core latch says.
ACU_FLOOR_Z = 366.3
# How far back a landing looks for the uses that fed it: 64436 reaches the unit about 4 s after the use,
# and a banked second core goes in within a second of the first.
CORE_LANDING_LOOKBACK_MS = 12000

# ULDUAR_MIMIRON_RAPID_BURST_HALF_ANGLE is an expression the source parser does not read.
BURST_HALF_ANGLE = math.radians(30.0)
BURST_WINDOW_MS = 3400  # the 3 s aura plus the last tick landing
AIM_LOOKAHEAD_MS = 500
# Spinning Up's 4 s channel plus the 10 s barrage, and the stretch after it in which the frozen
# event map unloads Heat Wave, Rocket Strike and Frost Bomb within a few seconds of each other.
SPIN_WINDOW_MS = 14500
SPIN_AFTER_MS = 15000
SPRAY_NOTE_MS = 3000
WALK_BACK_MS = 2500
WALK_BACK_MOVERS = ("reach melee", "set behind", "mimiron approach target action", "reach spell",
                    "reach party member to heal", "follow")
SPRAY_KILL_MS = 300
# Unit::GetMeleeRange against VX-001: its combat reach of 8, a player's 1.5, plus 4/3.
MELEE_RANGE = 10.83
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
# Fire dodge legs closer together than this are one dodge: the ladder re-issues within a tick or two.
DODGE_GAP_MS = 1500
# A dodge counts as burned when a Flames tick landed on the bot this long before it. Ticks come ~1 s apart.
BURNED_LOOKBACK_MS = 2000
# How near a detour note and its formation leg sit: both are written by the same action pass.
NOTE_MOVE_MS = 300
CAST_CUT_REPEAT_MS = 1000

ROOM_CENTRE = anchor("ULDUAR_MIMIRON_ROOM_CENTER")[:2]
SPREAD_TOLERANCE = radius("ULDUAR_MIMIRON_SPREAD_TOLERANCE")
FLAMES_BURN_RADIUS = radius("ULDUAR_MIMIRON_FLAMES_BURN_RADIUS")
# MimironHandoverStep, as the `mimiron.handover` latch writes it.
HANDOVER_STEPS = {"1": "hold", "2": "late", "3": "walk in"}
# Mimiron boards VX-001 this long before phase 2: where the walk in starts when no step was written.
WALK_IN_LEAD_MS = 12750

NPC_PROXIMITY_MINE = 34362
MINE_STEP = "mimiron proximity mine action"
MINE_TRIGGER = radius("ULDUAR_MIMIRON_MINE_TRIGGER_RADIUS")
MINE_LEG_CLEARANCE = radius("ULDUAR_MIMIRON_MINE_LEG_CLEARANCE")
MINE_CLEARANCE = radius("ULDUAR_MIMIRON_MINE_CLEARANCE")
# A mine hit on a moving bot goes to its last accepted leg when that leg is this recent.
MINE_MOVER_MS = 4000
STANDING = "standing"
OLDER_LEG = "an older leg"

REGEN_RISE = 3.0
REPAIR_FLOOR = 1.0

HEAL_DISTANCE = 38.5
PLAYER_COMBAT_REACH = 1.5
HEAL_REACH = HEAL_DISTANCE + 2 * PLAYER_COMBAT_REACH
PULSE_GROUP_MS = 400
PULSE_KILL_MS = 50
ESCAPE_SETTLE_MS = 3000
ESCAPE_LOOKBACK_MS = 6000
DRY_MANA_PCT = 10.0

WEDGE_FIRST_ROW = radius("ULDUAR_MIMIRON_PHASE3_MIN_RADIUS")
WEDGE_SPACING = radius("ULDUAR_MIMIRON_PHASE3_SPACING")
FLAMES_RADIUS = radius("ULDUAR_MIMIRON_FLAMES_RADIUS")
SPREAD_RANGE_MARGIN = radius("ULDUAR_MIMIRON_SPREAD_RANGE_MARGIN")
SPELL_DISTANCE = 28.5
WEDGE_HALF_ANGLE = math.radians(60.0)
WEDGE_SETTLE_LOOK_MS = 3000
WEDGE_LATER_MS = 10000
WEDGE_AFTER_MS = 25000
CONE_MATE_SAMPLE_MS = 1000
WEDGE_NOTE = re.compile(r"(reaim|fire|keep) (-?\d+)(?: (\d+)>(\d+))?")

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
        seen[label] += 1
        if label in ("H", "Z"):
            label = f"{label}{seen[label]}"
        elif seen[label] > 1:
            # Views key on the label, so a phase that comes round twice needs its own name or its deaths
            # and yards count twice. The first keeps its plain name, so span_of("P4") still finds it.
            label += chr(ord("a") + seen[label] - 1)
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
    events.update(COOLDOWN_EVENTS)
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
        aim = next((frame[cast.get("tgt")] for frame in samples.window(when - SAMPLE_MS, when + AIM_LOOKAHEAD_MS)
                    if cast.get("tgt") in frame), None)
        if not boss or not aim:
            continue

        carrier = cast.get("tgt")
        if carrier not in roster:
            carrier = min((guid for guid, unit in units.items() if guid in roster and living(unit)),
                          key=lambda guid: dist2((units[guid][1], units[guid][2]), (aim[1], aim[2])), default=None)
        centreline = math.atan2(aim[2] - boss[2], aim[1] - boss[1])
        row = {"t": when, "carrier": carrier, "carrier_ticks": 0, "cone": [], "outside": []}
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


def burst_group(trace: Trace, guid) -> str:
    return "melee" if trace.role(guid) in ("melee", "tank") else "ranged"


def burst_cross(trace: Trace, rows: list[dict]) -> collections.Counter:
    """Rapid Burst ticks keyed by (carrier group, victim group), melee and tanks against ranged and
    healers: a bot in the other group's line takes every burst aimed at that group."""
    ticks: collections.Counter = collections.Counter()
    for row in rows:
        side = burst_group(trace, row["carrier"])
        ticks[(side, side)] += row["carrier_ticks"]
        for guid, count, _ in row["cone"] + row["outside"]:
            ticks[(side, burst_group(trace, guid))] += count
    return ticks


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

    cross = burst_cross(trace, rows)
    pairs = (("melee", "melee"), ("melee", "ranged"), ("ranged", "melee"), ("ranged", "ranged"))
    crossed = cross[("melee", "ranged")] + cross[("ranged", "melee")]
    print("  carrier -> victim ticks  " + "  ".join(f"{a}>{b} {cross[(a, b)]}" for a, b in pairs)
          + f"  cross {crossed * 100 / max(sum(cross.values()), 1):.0f}%")


# ----------------------------------------------------------------------------------------------- bomb

def bomb_rows(trace: Trace) -> list[dict]:
    """One row per Frost Bomb cast, keyed off the bomb's first sample: the 10 s fuse starts at the summon,
    not the cast. A cast with no bomb after it summoned nothing."""
    roster = roster_guids(trace)
    vx = guids_of_entry(trace, NPC_VX001)
    bombs = guids_of_entry(trace, NPC_FROST_BOMB)
    samples = Samples(trace)
    blasts = [rec for rec in trace.of("dmg") if rec.get("sp") in SPELL_FROST_BOMB_EXPLOSIONS]
    moves = accepted_moves(trace)
    dodges = [rec for recs in moves.values() for rec in recs if rec.get("by") == BOMB_DODGE]

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
               "short_clearance": 0, "walk_ins": collections.Counter(), "vx_moved": 0.0}
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

        start_units = samples.before(seen)
        start_boss = next((start_units[guid] for guid in vx if guid in start_units), None)
        if boss and start_boss:
            row["vx_moved"] = dist2((start_boss[1], start_boss[2]), (boss[1], boss[2]))

        for guid, recs in moves.items():
            for rec in recs:
                if not seen <= rec["t"] <= seen + FUSE_MS or rec.get("by") == BOMB_DODGE:
                    continue
                unit = samples.before(rec["t"]).get(guid)
                if not unit:
                    continue
                stood = dist2((unit[1], unit[2]), spot)
                if off_segment((unit[1], unit[2]), (rec["x"], rec["y"]), spot) < min(stood, BOMB_STAND):
                    row["walk_ins"][rec.get("by", "?")] += 1

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
        walk_ins = ", ".join(f"{by} {n}" for by, n in row["walk_ins"].most_common())
        print(f"            walks into the live bomb {sum(row['walk_ins'].values())}"
              f"{f' ({walk_ins})' if walk_ins else ''}; VX-001 moved {row['vx_moved']:.0f} yd over the fuse")
    print(f"\n  a walk-in is an accepted leg, any mover but the bomb's own escape, that ran deeper inside"
          f" {BOMB_STAND:.0f} yd of the bomb than the bot stood")


# ---------------------------------------------------------------------------------------------- slots

def slot_churn(trace: Trace) -> dict[str, dict]:
    """Per phase, how each bot's `mimiron.slot` changed: new shape, new count (a death or a return), new
    index, or the same index moved (the shape slid), with the yards it moved to the note's whole yard."""
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


# ----------------------------------------------------------------------------------------------- spin

def raid_hp(units: dict[int, list], roster) -> float | None:
    alive = [unit[5] for guid, unit in units.items() if guid in roster and living(unit)]
    return statistics.mean(alive) if alive else None


def dealt_between(samples: Samples, roster, low: int, high: int) -> int:
    """What the raid put into everything that is not a raid member, off snap.u[11]'s running total."""
    first, last = samples.before(low), samples.before(high)
    total = 0
    for guid in roster:
        was, now = first.get(guid), last.get(guid)
        if was and now and len(was) > 11 and len(now) > 11:
            total += max(now[11] - was[11], 0)
    return total


def spin_rows(trace: Trace) -> list[dict]:
    """One row per Spinning Up cast: the 14.5 s window in which only the fire touches the raid, and the
    15 s after it, when every overdue event fires."""
    roster = roster_guids(trace)
    vx = guids_of_entry(trace, NPC_VX001)
    spans = phases(trace)
    samples = Samples(trace)
    moves = accepted_moves(trace)

    rows = []
    for cast in trace.of("cast"):
        if cast.get("sp") != SPELL_SPINNING_UP:
            continue

        start = cast["t"]
        stop = start + SPIN_WINDOW_MS
        after = stop + SPIN_AFTER_MS
        row = {
            "t": start, "phase": phase_at(spans, start),
            "before": role_shares(trace, start - SPIN_WINDOW_MS, start),
            "inside": role_shares(trace, start, stop),
            "dealt_before": dealt_between(samples, roster, start - SPIN_WINDOW_MS, start),
            "dealt_inside": dealt_between(samples, roster, start, stop),
            "melee_rows": 0, "melee_in_range": 0, "fire": 0, "beam": 0, "boundary": 0, "deaths": [],
            "heal_mines": 0,
            "hp_before": raid_hp(samples.before(stop), roster),
            "hp_after": raid_hp(samples.before(after), roster),
            "taken": 0, "healed": 0, "legs": 0, "yards": 0.0,
            "heal_moving": role_shares(trace, stop, after).get("heal", {}).get("moving"),
        }
        rows.append(row)

        for units in samples.window(start, stop):
            boss = next((units[guid] for guid in vx if guid in units), None)
            for guid, unit in units.items():
                if guid not in roster or not living(unit) or trace.role(guid) != "melee":
                    continue
                row["melee_rows"] += 1
                if boss and dist2((unit[1], unit[2]), (boss[1], boss[2])) <= MELEE_RANGE:
                    row["melee_in_range"] += 1

        # Off combat_deaths, like the phases section: a trace can carry the same death twice.
        row["deaths"] = [trace.name(rec["g"]) for rec in combat_deaths(trace)
                         if start <= rec["t"] < stop]

        for rec in trace.records:
            when = rec.get("t", 0)
            kind = rec.get("e")
            if start <= when < stop:
                if kind == "dmg" and rec.get("d") in roster and rec.get("sp") == SPELL_FLAMES:
                    row["fire"] += rec.get("a", 0)
                elif (kind == "dmg" and rec.get("d") in roster and rec.get("sp") in SPELL_MINE_EXPLOSIONS
                      and trace.role(rec["d"]) == "heal"):
                    row["heal_mines"] += rec.get("a", 0)
                elif kind == "dmg" and rec.get("d") in roster and rec.get("sp") == SPELL_LASER_BARRAGE:
                    row["beam"] += 1
                    units = samples.before(when)
                    boss = next((units[guid] for guid in vx if guid in units), None)
                    victim = units.get(rec["d"])
                    if boss and victim and dist2((victim[1], victim[2]), (boss[1], boss[2])) < BARRAGE_BOUNDARY:
                        row["boundary"] += 1
            elif stop <= when < after:
                if kind == "dmg" and rec.get("d") in roster:
                    row["taken"] += rec.get("a", 0)
                elif kind == "heal" and rec.get("d") in roster:
                    row["healed"] += rec.get("a", 0) - rec.get("oh", 0)

        for guid, recs in moves.items():
            if trace.role(guid) not in ("ranged", "heal"):
                continue
            for rec in recs:
                if stop <= rec["t"] < after and rec.get("by") == FORMATION:
                    unit = samples.before(rec["t"]).get(guid)
                    if unit:
                        row["legs"] += 1
                        row["yards"] += dist2((unit[1], unit[2]), (rec["x"], rec["y"]))
    return rows


def show_spin(trace: Trace) -> None:
    print("SPIN")
    rows = spin_rows(trace)
    if not rows:
        print("  no Spinning Up cast")
        return

    def share(shares, role, key):
        return f"{shares[role][key] * 100:3.0f}%" if role in shares else "   -"

    for row in rows:
        gap = row["dealt_inside"] - row["dealt_before"]
        print(f"  {clock(row['t'])} {row['phase']:4} window {SPIN_WINDOW_MS / 1000:.1f} s:"
              f" raid dealt {row['dealt_inside']:,} against {row['dealt_before']:,} in the 14.5 s"
              f" before ({gap:+,})")
        print("            " + "  ".join(
            f"{role} moving {share(row['inside'], role, 'moving')}"
            f"/{share(row['before'], role, 'moving')}"
            f" casting {share(row['inside'], role, 'casting')}"
            f"/{share(row['before'], role, 'casting')}"
            for role in ROLES if role in row["inside"]))
        melee = row["melee_in_range"] / row["melee_rows"] * 100 if row["melee_rows"] else 0.0
        print(f"            melee within {MELEE_RANGE:.1f} yd of VX-001 {melee:3.0f}% of the window;"
              f" fire {row['fire'] // 1000}k; mines on healers {row['heal_mines'] // 1000}k;"
              f" beam ticks {row['beam']},"
              f" {row['boundary']} inside {BARRAGE_BOUNDARY:.1f} yd of VX-001; {len(row['deaths'])} dead"
              + (f" ({', '.join(row['deaths'])})" if row["deaths"] else ""))
        hp = ("    -" if row["hp_before"] is None or row["hp_after"] is None
              else f"{row['hp_before']:.0f} -> {row['hp_after']:.0f}")
        moved = "   -" if row["heal_moving"] is None else f"{row['heal_moving'] * 100:3.0f}%"
        print(f"            the 15 s after: raid hp {hp}, taken {row['taken']:,},"
              f" healed {row['healed']:,}, healers moving {moved},"
              f" formation {row['legs']} legs / {row['yards']:.0f} yd")
    print("\n  'inside/before' is the window against the 14.5 s before it")


# ----------------------------------------------------------------------------------------------- fire

def slot_point(slot_notes: list[dict], guid: int, when: int):
    """The `(x, y)` the bot's last `mimiron.slot` note at or before `when` named, or None."""
    point = None
    for rec in slot_notes:
        if rec["t"] > when:
            break
        if rec.get("g") == guid:
            match = SLOT.match(str(rec.get("txt", "")))
            point = (float(match.group(4)), float(match.group(5))) if match else None
    return point


def fire_rows(trace: Trace) -> dict[str, dict]:
    """Per phase: fire dodges against the Flames ticks behind them, where the dodges went relative to the
    bot's target, the ladder's fallbacks, the casts cut for the dodge, the formation legs
    `mimiron.approach` named, and Flames damage by role."""
    roster = roster_guids(trace)
    spans = phases(trace)
    samples = Samples(trace)
    flames = guids_of_entry(trace, NPC_FLAMES_SPREAD)

    def blank() -> dict:
        return {"dodges": 0, "burned": 0, "full_unburned": 0, "judged": 0, "farther": 0, "closer": 0,
                "fallbacks": 0, "fallback_legs": 0, "fallback_burning": 0, "cuts": collections.Counter(),
                "approach": collections.Counter(), "detour_extra": [], "fire": collections.Counter()}

    rows: dict[str, dict] = collections.defaultdict(blank)

    ticks: dict[int, list[int]] = collections.defaultdict(list)
    for rec in trace.of("dmg"):
        if rec.get("sp") == SPELL_FLAMES and rec.get("d") in roster:
            ticks[rec["d"]].append(rec["t"])
            rows[phase_at(spans, rec["t"])]["fire"][trace.role(rec["d"])] += rec.get("a", 0)

    moves = accepted_moves(trace)
    for guid, recs in moves.items():
        last = None
        for rec in recs:
            if rec.get("by") != FLAMES_DODGE:
                continue
            if last is not None and rec["t"] - last < DODGE_GAP_MS:
                last = rec["t"]
                continue
            last = rec["t"]

            row = rows[phase_at(spans, rec["t"])]
            row["dodges"] += 1
            stamps = ticks.get(guid, [])
            index = bisect.bisect_right(stamps, rec["t"]) - 1
            burned = index >= 0 and rec["t"] - stamps[index] <= BURNED_LOOKBACK_MS
            row["burned"] += int(burned)

            units = samples.before(rec["t"])
            me = units.get(guid)
            if not me:
                continue
            if not burned and me[5] >= 90.0:
                row["full_unburned"] += 1
            target = units.get(me[7]) if len(me) > 7 and me[7] else None
            if not target:
                continue
            was = dist2((me[1], me[2]), (target[1], target[2]))
            now = dist2((rec["x"], rec["y"]), (target[1], target[2]))
            row["judged"] += 1
            row["farther"] += int(now - was > 1.0)
            row["closer"] += int(was - now > 1.0)

    for rec in notes(trace, "mimiron.flee"):
        text = str(rec.get("txt", "")).split()
        if len(text) < 2 or not text[0].startswith("flames+") or text[1] != "fallback":
            continue
        row = rows[phase_at(spans, rec["t"])]
        row["fallbacks"] += 1
        leg = next((m for m in moves.get(rec.get("g"), []) if m.get("by") == FLAMES_DODGE
                    and abs(m["t"] - rec["t"]) <= NOTE_MOVE_MS), None)
        if not leg:
            continue
        row["fallback_legs"] += 1
        units = samples.before(leg["t"] + 1)
        row["fallback_burning"] += int(any(dist2((leg["x"], leg["y"]), (unit[1], unit[2])) < FLAMES_BURN_RADIUS
                                           for guid, unit in units.items() if guid in flames and living(unit)))

    last_cut: dict[int, int] = {}
    for rec in sorted(notes(trace, "mimiron.castcut"), key=lambda rec: rec["t"]):
        previous = last_cut.get(rec.get("g"))
        last_cut[rec.get("g")] = rec["t"]
        if previous is not None and rec["t"] - previous < CAST_CUT_REPEAT_MS:
            continue
        rows[phase_at(spans, rec["t"])]["cuts"][trace.spell(int(rec.get("txt") or 0))] += 1

    slot_notes = notes(trace, "mimiron.slot")
    for rec in notes(trace, "mimiron.approach"):
        kind = str(rec.get("txt", "")).split(" ")[0]
        row = rows[phase_at(spans, rec["t"])]
        row["approach"][kind] += 1
        if kind != "detour":
            continue

        guid = rec.get("g")
        leg = next((m for m in moves.get(guid, []) if m.get("by") == FORMATION
                    and abs(m["t"] - rec["t"]) <= NOTE_MOVE_MS), None)
        me = samples.before(rec["t"]).get(guid)
        slot = slot_point(slot_notes, guid, rec["t"])
        if leg and me and slot:
            here, waypoint = (me[1], me[2]), (leg["x"], leg["y"])
            row["detour_extra"].append(dist2(here, waypoint) + dist2(waypoint, slot) - dist2(here, slot))
    return rows


def show_fire(trace: Trace) -> None:
    print("FIRE")
    rows = fire_rows(trace)
    order = [label for _, _, label in phases(trace) if label in rows]
    if not order:
        print("  no fire dodge, formation detour or Flames tick")
        return

    for label in order:
        row = rows[label]
        dodges = row["dodges"]
        burned = f"{row['burned'] / dodges * 100:.0f}%" if dodges else "-"
        print(f"  {label:4} dodges {dodges}: burned in the {BURNED_LOOKBACK_MS / 1000:.0f} s before {burned},"
              f" unburned at >=90% hp {row['full_unburned']};"
              f" ended farther from target {row['farther']}, closer {row['closer']} of {row['judged']}")
        if row["fallbacks"] or row["cuts"]:
            cuts = ", ".join(f"{spell} {count}" for spell, count in row["cuts"].most_common())
            print(f"       fallbacks {row['fallbacks']}, {row['fallback_burning']} of {row['fallback_legs']} legs"
                  f" ended in a burn; casts cut for the dodge: {cuts or 'none'}")
        extra = sorted(row["detour_extra"])
        detour = (f" (extra median {statistics.median(extra):.1f} yd, max {extra[-1]:.1f})" if extra else "")
        kinds = ", ".join(f"{kind} {count}" for kind, count in sorted(row["approach"].items()))
        print(f"       formation legs: {kinds or 'none noted'}{detour}")
        fire = row["fire"]
        print(f"       Flames {sum(fire.values()) // 1000}k: "
              + "  ".join(f"{role} {fire[role] // 1000}k" for role in ROLES if fire[role]))
    print(f"\n  'burned' means a Flames tick on the bot inside {BURNED_LOOKBACK_MS / 1000:.0f} s before the dodge;"
          " legs within 1 yd of where they started count neither way; a fallback leg ended in a burn when it"
          f" stopped within {FLAMES_BURN_RADIUS:.1f} yd of a live Flames (Spread)")


# -------------------------------------------------------------------------------------------- handover

def handover_rows(trace: Trace) -> list[dict]:
    """One row per phase 1 to 2 handover: when each step started, the hold band picked, how far the bots
    stood from the room centre, which bots were on a phase 2 slot when phase 2 began, bot healer mana
    across it, the drinks that went out, the raid's Flames damage and deaths. A human is left out of
    the positions and the mana: the handover never moves him."""
    spans = phases(trace)
    roster = roster_guids(trace)
    bots = roster - trace.humans
    samples = Samples(trace)
    slot_notes = notes(trace, "mimiron.slot")
    rows = []
    for index, (_, p1_end, label) in enumerate(spans):
        # A blip of the phase latch splits phase 1 into P1, Z, P1b: the handover follows the last part.
        if not label.startswith("P1"):
            continue
        after = next(((start, name) for start, _, name in spans[index + 1:] if name.startswith("P")), None)
        if not after or after[1] != "P2":
            continue
        low, high = p1_end, after[0]

        steps: dict[str, int] = {}
        for rec in notes(trace, "mimiron.handover"):
            value = str(rec.get("txt", ""))
            if low <= rec["t"] <= high and value in HANDOVER_STEPS:
                steps.setdefault(value, rec["t"])
        hold = next((str(rec.get("txt", "")) for rec in notes(trace, "mimiron.hold") if low <= rec["t"] <= high),
                    None)

        def spread(when: int) -> tuple[float, float] | None:
            units = samples.before(when)
            far = sorted(dist2((row[1], row[2]), ROOM_CENTRE) for guid, row in units.items()
                         if guid in bots and living(row))
            return (statistics.median(far), far[-1]) if far else None

        walk_in = steps.get("3", high - WALK_IN_LEAD_MS)
        at_p2 = samples.before(high)
        living_at_p2 = [guid for guid, row in at_p2.items() if guid in bots and living(row)]
        # Only slots handed out in the handover: a phase 1 one left standing is no phase 2 slot.
        handed = [rec for rec in slot_notes if low <= rec["t"] <= high]
        slotted = on_slot = 0
        for guid in living_at_p2:
            last = next((str(rec.get("txt", "")) for rec in reversed(handed) if rec.get("g") == guid), "")
            point = slot_point(handed, guid, high)
            if point is None or last.startswith("hmhold"):
                continue
            slotted += 1
            on_slot += int(dist2((at_p2[guid][1], at_p2[guid][2]), point) <= SPREAD_TOLERANCE)

        start_units = samples.before(low)
        mana = [(start_units[guid][6], at_p2[guid][6]) for guid in sorted(bots)
                if trace.role(guid) == "heal" and guid in start_units and guid in at_p2]
        drinks = [(trace.name(rec["g"]), (rec["t"] - low) / 1000.0) for rec in trace.of("act")
                  if rec.get("a") in ("drink", "eat") and rec.get("vd") == "OK" and low <= rec["t"] < high]
        fire = sum(rec.get("a", 0) for rec in trace.of("dmg")
                   if rec.get("sp") == SPELL_FLAMES and rec.get("d") in roster and low <= rec["t"] < high)
        dead = [rec for rec in combat_deaths(trace) if low <= rec["t"] < high]

        rows.append({"span": (low, high), "steps": steps, "hold": hold,
                     "spread": {"P1 end": spread(low), "+10 s": spread(low + 10000),
                                "walk in": spread(walk_in), "P2": spread(high)},
                     "living": len(living_at_p2), "slotted": slotted, "on_slot": on_slot,
                     "mana": mana, "drinks": drinks, "fire": fire, "dead": dead})
    return rows


def show_handover(trace: Trace) -> None:
    print("HANDOVER")
    rows = handover_rows(trace)
    if not rows:
        print("  no phase 1 to 2 handover in this pull")
        return

    for row in rows:
        low, high = row["span"]
        print(f"  {clock(low)} .. {clock(high)}  {(high - low) / 1000:.1f} s")
        if row["steps"]:
            print("    steps: " + ", ".join(
                f"{HANDOVER_STEPS[value]} +{(when - low) / 1000:.1f} s" for value, when in sorted(
                    row["steps"].items(), key=lambda item: item[1]))
                  + (f"  (walk in {(high - row['steps']['3']) / 1000:.1f} s before P2)" if "3" in row["steps"] else ""))
            print(f"    hold band: {row['hold'] or 'not noted'}")
        else:
            print("    no steps: not Firefighter, or a build without the handover hold")
        print("    bots from the room centre, median/max yd: " + ", ".join(
            f"{name} {cell[0]:.0f}/{cell[1]:.0f}" for name, cell in row["spread"].items() if cell))
        print(f"    on a phase 2 slot at P2: {row['on_slot']} of {row['living']} living bots"
              f" ({row['slotted']} with a slot)")
        if row["mana"]:
            print("    healer mana: " + "/".join(f"{before:.0f}" for before, _ in row["mana"]) + " -> "
                  + "/".join(f"{after:.0f}" for _, after in row["mana"]))
        print(f"    drinks {len(row['drinks'])}: "
              + (", ".join(f"{name} +{when:.1f}" for name, when in row["drinks"]) or "none"))
        print(f"    Flames {row['fire'] // 1000}k, {len(row['dead'])} dead")


# ------------------------------------------------------------------------------------------------ mines

def off_segment(start, end, point) -> float:
    """Ground distance from `point` to the walk from `start` to `end`."""
    dx, dy = end[0] - start[0], end[1] - start[1]
    length_sq = dx * dx + dy * dy
    along = 0.0 if length_sq <= 0 else ((point[0] - start[0]) * dx + (point[1] - start[1]) * dy) / length_sq
    along = min(1.0, max(0.0, along))
    return dist2(point, (start[0] + along * dx, start[1] + along * dy))


def mine_mover(recs: list[dict], stamps: list[int], when: int, row) -> str:
    """What was moving the bot when a mine hit it: its last accepted leg if recent, else standing."""
    if not row or not moving(row):
        return STANDING
    index = bisect.bisect_right(stamps, when) - 1
    if index >= 0 and when - recs[index]["t"] <= MINE_MOVER_MS:
        return recs[index].get("by", "?")
    return OLDER_LEG


def mine_rows(trace: Trace) -> dict[str, dict]:
    """Per phase: Proximity Mine hits, damage and kills by what was moving the bot, per mover the
    accepted legs whose straight line passed a live mine, passed two or more, or ended beside one, and
    the mine steps with how many began at the trigger radius or farther from the nearest live mine. The
    line is what the bots screen; the walk follows the navmesh and can stray off it."""
    roster = roster_guids(trace)
    spans = phases(trace)
    samples = Samples(trace)
    mine_guids = guids_of_entry(trace, NPC_PROXIMITY_MINE)
    moves = accepted_moves(trace)
    stamps = {guid: [rec["t"] for rec in recs] for guid, recs in moves.items()}

    def blank() -> dict:
        return {"hits": collections.Counter(), "damage": collections.Counter(), "kills": collections.Counter(),
                "legs": collections.defaultdict(lambda: [0, 0, 0, 0]), "steps": [0, 0]}

    rows: dict[str, dict] = collections.defaultdict(blank)

    def mover(guid: int, when: int) -> str:
        return mine_mover(moves.get(guid, []), stamps.get(guid, []), when, samples.before(when).get(guid))

    for rec in trace.of("dmg"):
        if rec.get("sp") in SPELL_MINE_EXPLOSIONS and rec.get("d") in roster:
            row = rows[phase_at(spans, rec["t"])]
            by = mover(rec["d"], rec["t"])
            row["hits"][by] += 1
            row["damage"][by] += rec.get("a", 0)

    for rec in combat_deaths(trace):
        if rec.get("g") in roster and trace.entries.get(rec.get("killer")) == NPC_PROXIMITY_MINE:
            rows[phase_at(spans, rec["t"])]["kills"][mover(rec["g"], rec["t"])] += 1

    for guid, recs in moves.items():
        for rec in recs:
            units = samples.before(rec["t"])
            me = units.get(guid)
            if not me or not living(me):
                continue
            start, end = (me[1], me[2]), (rec["x"], rec["y"])
            mines = [(row[1], row[2]) for key, row in units.items() if key in mine_guids and living(row)]
            if rec.get("by") == MINE_STEP and mines:
                steps = rows[phase_at(spans, rec["t"])]["steps"]
                steps[0] += 1
                steps[1] += int(min(dist2(start, m) for m in mines) >= MINE_TRIGGER)
            if dist2(start, end) < 1.0:
                continue
            # Mines the bot already stands by are left out, as the bots' own leg test does.
            passed = [m for m in mines if dist2(start, m) >= MINE_LEG_CLEARANCE
                      and off_segment(start, end, m) < MINE_LEG_CLEARANCE]
            tally = rows[phase_at(spans, rec["t"])]["legs"][rec.get("by", "?")]
            tally[0] += 1
            tally[1] += int(len(passed) >= 1)
            tally[2] += int(len(passed) >= 2)
            tally[3] += int(any(dist2(end, m) < MINE_CLEARANCE for m in mines))
    return rows


def show_mines(trace: Trace) -> None:
    print("MINES")
    rows = mine_rows(trace)
    order = [label for _, _, label in phases(trace) if label in rows]
    shown = False
    for label in order:
        row = rows[label]
        legs = {by: tally for by, tally in row["legs"].items() if tally[1] or tally[3]}
        if not row["hits"] and not legs and not row["steps"][0]:
            continue
        shown = True
        total = sum(row["damage"].values())
        print(f"  {label:4} hits {sum(row['hits'].values())} for {total // 1000}k,"
              f" killing blows {sum(row['kills'].values())}; mine steps {row['steps'][0]},"
              f" {row['steps'][1]} of them from {MINE_TRIGGER:.1f} yd or farther")
        for by, count in row["hits"].most_common():
            kills = f", {row['kills'][by]} killed" if row["kills"][by] else ""
            print(f"       {by:44} {count:3} hits {row['damage'][by] // 1000:4}k{kills}")
        for by, (count, over, over2, ends) in sorted(legs.items()):
            print(f"       legs {by:39} {over:3} of {count:4} passed a mine, {over2} two or more,"
                  f" {ends} ended beside one")
    if not shown:
        print("  no mine hit and no leg past a live mine")
        return
    print(f"\n  'passed' is the straight line within {MINE_LEG_CLEARANCE:.1f} yd of a live mine the bot did not"
          f" start beside; 'beside' is within {MINE_CLEARANCE:.1f} yd; a hit belongs to the bot's last leg"
          f" when it is moving and the leg is under {MINE_MOVER_MS / 1000:.0f} s old; a mine step's distance is"
          f" ground distance to the nearest live mine in the snapshot before it")


# ------------------------------------------------------------------------------------------------- p3

def spray_lanes(units: dict[int, list], firebot: int, flames: set[int]) -> list[tuple[float, float, float]]:
    """`(x, y, facing)` of each line this fire bot can spray down next, as the C++ hazard read builds them:
    its live facing, and the lane toward its nearest spread flame from where its walk there stops."""
    row = units.get(firebot)
    if not row or not living(row):
        return []

    lanes = [(row[1], row[2], row[4])]
    here = (row[1], row[2])
    near = min(((unit[1], unit[2]) for guid, unit in units.items() if guid in flames and living(unit)),
               key=lambda spot: dist2(spot, here), default=None)
    if near is not None:
        facing = math.atan2(near[1] - here[1], near[0] - here[0])
        walk = max(dist2(near, here) - SPRAY_REACH, 0.0)
        lanes.append((here[0] + walk * math.cos(facing), here[1] + walk * math.sin(facing), facing))
    return lanes


def in_spray_lane(lane: tuple[float, float, float], point) -> bool:
    dx, dy = point[0] - lane[0], point[1] - lane[1]
    ahead = dx * math.cos(lane[2]) + dy * math.sin(lane[2])
    side = dy * math.cos(lane[2]) - dx * math.sin(lane[2])
    return 0.0 <= ahead < SPRAY_LENGTH and abs(side) < SPRAY_HALF_WIDTH


def core_ledger(trace: Trace, low: int, high: int) -> dict:
    """Magnetic Cores from `mimiron.corestep`: taken, spent, never spent, how long each sat in the bags
    before it went down (first in, first out per carrier), and how many a chain spent."""
    bags: dict[int, collections.deque] = collections.defaultdict(collections.deque)
    held: list[int] = []
    looted = used = chained = 0
    for rec in notes(trace, "mimiron.corestep"):
        if not low <= rec["t"] < high:
            continue
        step = str(rec.get("txt", ""))
        if step in ("loot", "loot-second"):
            looted += 1
            bags[rec.get("g")].append(rec["t"])
        elif step == "use":
            used += 1
            if bags[rec.get("g")]:
                held.append(rec["t"] - bags[rec.get("g")].popleft())
        elif step == "chain":
            chained += 1
    return {"looted": looted, "used": used, "lost": looted - used, "held": held, "chained": chained}


def p3_rows(trace: Trace) -> dict:
    """Phase 3: how long each Emergency Fire Bot lived and how much of the raid shot it, what Water Spray
    cost, and how much faster the Aerial Command Unit dies on the floor than in the air."""
    span = span_of(phases(trace), "P3")
    if not span:
        return {}

    low, high = span
    roster = roster_guids(trace)
    samples = Samples(trace)
    bots = guids_of_entry(trace, NPC_EMERGENCY_FIRE_BOT)
    assaults = guids_of_entry(trace, NPC_ASSAULT_BOT)
    junk = guids_of_entry(trace, NPC_JUNK_BOT)
    acu = guids_of_entry(trace, NPC_AERIAL_COMMAND_UNIT)

    firebots: dict[int, dict] = {}
    seen_assault: dict[int, list] = {}
    alive_counts: list[int] = []
    junk_counts: list[int] = []
    acu_z: list[tuple[int, float]] = []
    bot_rows = aimed = junk_aimed = assault_aimed = 0
    for index, snap in enumerate(samples.snaps):
        when = snap["t"]
        if not low <= when < high:
            continue

        units = samples.rows(index)
        living_bots = [guid for guid in bots if guid in units and living(units[guid])]
        alive_counts.append(len(living_bots))
        for guid in living_bots:
            cell = firebots.setdefault(guid, {"first": when, "last": when, "hp": 100.0, "aimed": 0})
            cell["last"] = when
            cell["hp"] = units[guid][5]

        for guid in assaults:
            if guid in units and living(units[guid]):
                seen_assault.setdefault(guid, [when, when])[1] = when

        junk_counts.append(sum(1 for guid in junk if guid in units and living(units[guid])))
        row = next((units[guid] for guid in acu if guid in units), None)
        if row:
            acu_z.append((when, row[3]))

        for guid, unit in units.items():
            if guid not in roster or not living(unit) or guid in trace.humans:
                continue
            bot_rows += 1
            target = unit[7] if len(unit) > 7 else 0
            junk_aimed += target in junk
            assault_aimed += target in assaults
            if target in bots:
                aimed += 1
                if target in firebots:
                    firebots[target]["aimed"] += 1

    hits = [rec for rec in trace.of("dmg")
            if low <= rec["t"] < high and rec.get("d") in roster and rec.get("sp") == SPELL_WATER_SPRAY]
    spray = sum(rec.get("a", 0) for rec in hits)
    resisted = sum(rec.get("rs", 0) for rec in hits)

    frost: dict[int, list[dict]] = collections.defaultdict(list)
    for rec in trace.of("aura"):
        if rec.get("sp") in SPELL_FROST_RESISTANCE_AURA:
            frost[rec.get("d")].append(rec)

    # The lane test takes the source off the damage record: casts are not logged for every fire bot.
    flames = guids_of_entry(trace, NPC_FLAMES_SPREAD)
    on_aura = in_lane = 0
    for rec in hits:
        applied = [aura for aura in frost.get(rec["d"], []) if aura["t"] <= rec["t"]]
        if applied and not max(applied, key=lambda aura: aura["t"]).get("r"):
            on_aura += 1

        units = samples.before(rec["t"] - SPRAY_LEAD_MS)
        victim = units.get(rec["d"])
        if victim and any(in_spray_lane(lane, (victim[1], victim[2]))
                          for lane in spray_lanes(units, rec.get("s"), flames)):
            in_lane += 1

    spray_deaths = sum(1 for rec in combat_deaths(trace)
                       if low <= rec["t"] < high
                       and trace.entries.get(rec.get("killer")) == NPC_EMERGENCY_FIRE_BOT)

    # `mimiron.core` latches 1 while a core has the unit on the floor, 0 when it climbs back.
    windows: list[tuple[int, int]] = []
    start = None
    for rec in notes(trace, "mimiron.core"):
        if str(rec.get("txt", "")) == "1" and start is None:
            start = rec["t"]
        elif str(rec.get("txt", "")) == "0" and start is not None:
            windows.append((start, rec["t"]))
            start = None
    if start is not None:
        windows.append((start, high))

    def acu_hp(when: int):
        units = samples.before(when)
        row = next((units[guid] for guid in acu if guid in units), None)
        return row[5] if row else None

    def drop(begin: int, end: int) -> float:
        was, now = acu_hp(begin), acu_hp(end)
        return was - now if was is not None and now is not None else 0.0

    # Off Z, not the latch: a second core in a live window replaces the aura, that removal runs the lift-off,
    # and the unit climbs back out of reach with the new aura on, still latched.
    def floor_seconds(begin: int, end: int) -> float:
        inside = [z for when, z in acu_z if begin <= when <= end]
        if not inside:
            return 0.0

        return (end - begin) / 1000.0 * sum(1 for z in inside if z <= ACU_FLOOR_Z) / len(inside)

    uses = [rec["t"] for rec in notes(trace, "mimiron.corestep")
            if low <= rec["t"] < high and str(rec.get("txt", "")) == "use"]
    landings = [{"span": (begin, stop), "floor": floor_seconds(begin, stop),
                 "drop": drop(begin, stop),
                 "cores": sum(1 for when in uses
                              if begin - CORE_LANDING_LOOKBACK_MS <= when <= stop)}
                for begin, stop in windows]

    grounded_ms = sum(stop - begin for begin, stop in windows)
    grounded_drop = sum(drop(begin, stop) for begin, stop in windows)
    air = []
    mark = low
    for begin, stop in windows:
        air.append((mark, begin))
        mark = stop
    air.append((mark, high))

    return {
        "span": span,
        "firebots": firebots,
        "alive_mean": statistics.mean(alive_counts) if alive_counts else 0.0,
        "alive_max": max(alive_counts) if alive_counts else 0,
        "aimed_share": aimed / bot_rows if bot_rows else 0.0,
        "aimed_seconds": aimed * SAMPLE_MS / 1000.0,
        "spray": spray,
        "spray_deaths": spray_deaths,
        "spray_hits": len(hits),
        "spray_resisted": resisted / (spray + resisted) if spray + resisted else 0.0,
        "spray_on_aura": on_aura,
        "spray_in_lane": in_lane,
        "cores": core_ledger(trace, low, high),
        "landings": landings,
        "junk_seen": len(junk),
        "junk_mean": statistics.mean(junk_counts) if junk_counts else 0.0,
        "junk_max": max(junk_counts) if junk_counts else 0,
        "junk_aimed": junk_aimed,
        "assault_aimed": assault_aimed,
        "assaults": sorted((stamps[1] - stamps[0]) for stamps in seen_assault.values()),
        "coresteps": collections.Counter(
            str(rec.get("txt", "")) for rec in notes(trace, "mimiron.corestep")
            if low <= rec["t"] < high),
        "grounded": (grounded_ms, grounded_drop),
        "airborne": (sum(stop - begin for begin, stop in air),
                     sum(drop(begin, stop) for begin, stop in air)),
    }


def show_p3(trace: Trace) -> None:
    print("P3")
    rows = p3_rows(trace)
    if not rows:
        print("  no phase 3 in this pull")
        return

    low, high = rows["span"]
    print(f"  {clock(low)} .. {clock(high)}, {(high - low) / 1000:.0f} s")
    print(f"  fire bots {rows['alive_mean']:.1f} alive on average, {rows['alive_max']} at most,"
          f" {len(rows['firebots'])} seen; the raid aimed at one for {rows['aimed_seconds']:.0f}"
          f" bot-s ({rows['aimed_share'] * 100:.1f}% of the phase)")
    for guid, cell in sorted(rows["firebots"].items(), key=lambda item: item[1]["first"]):
        print(f"      {clock(cell['first'])} .. {clock(cell['last'])}"
              f"  {(cell['last'] - cell['first']) / 1000:5.1f} s  ended at {cell['hp']:3.0f}%"
              f"  aimed at {cell['aimed'] * SAMPLE_MS / 1000:5.1f} bot-s")
    print(f"  Water Spray {rows['spray']:,} into the raid, {rows['spray_deaths']} dead to a fire bot")
    if rows["spray_hits"]:
        print(f"      {rows['spray_hits']} hits, {rows['spray_resisted'] * 100:.0f}% resisted,"
              f" {rows['spray_on_aura']} on a Frost Resistance Aura holder, {rows['spray_in_lane']}"
              f" standing in a spray lane {SPRAY_LEAD_MS / 1000:.0f} s before")

    lived = rows["assaults"]
    if lived:
        print(f"  Assault Bots {len(lived)}, alive {statistics.median(lived) / 1000:.0f} s median,"
              f" {min(lived) / 1000:.0f} to {max(lived) / 1000:.0f} s")
    if rows["junk_seen"]:
        print(f"  Junk Bots {rows['junk_seen']} seen, {rows['junk_mean']:.1f} alive on average,"
              f" {rows['junk_max']} at most; the raid aimed at one for"
              f" {rows['junk_aimed'] * SAMPLE_MS / 1000:.0f} bot-s against"
              f" {rows['assault_aimed'] * SAMPLE_MS / 1000:.0f} on Assault Bots")
    if rows["coresteps"]:
        print("  core steps: " + ", ".join(f"{step} {n}" for step, n in rows["coresteps"].most_common()))
    cores = rows["cores"]
    if cores["looted"]:
        held = "/".join(f"{ms / 1000:.0f}" for ms in cores["held"]) or "-"
        print(f"  cores looted {cores['looted']}, used {cores['used']}, never spent {cores['lost']};"
              f" held {held} s before use; {cores['chained']} spent by a chain")

    for landing in rows["landings"]:
        begin, stop = landing["span"]
        print(f"      {clock(begin)} .. {clock(stop)}  {(stop - begin) / 1000:4.1f} s latched,"
              f" {landing['floor']:4.1f} s on the floor, {landing['cores']} core(s) in,"
              f" unit down {landing['drop']:.1f} points")

    floor = sum(landing["floor"] for landing in rows["landings"])
    for label, (ms, points) in (("grounded", rows["grounded"]), ("airborne", rows["airborne"])):
        rate = points / (ms / 1000.0) if ms else 0.0
        tail = f", {floor:.0f} s of it on the floor" if label == "grounded" else ""
        print(f"  {label:9} {ms / 1000:5.0f} s of the phase, unit down {points:5.1f} points"
              f" ({rate:.2f} %/s){tail}")


def siren_silences(trace: Trace, roster) -> list[tuple[int, int]]:
    spans: list[tuple[int, int]] = []
    since: dict[int, int] = {}
    for rec in sorted(trace.of("aura"), key=lambda rec: rec["t"]):
        if rec.get("sp") != SPELL_DEAFENING_SIREN or rec.get("d") not in roster:
            continue
        if not rec.get("r"):
            since.setdefault(rec["d"], rec["t"])
        elif rec["d"] in since:
            spans.append((since.pop(rec["d"]), rec["t"]))
    end = pull_end(trace)
    return spans + [(start, end) for start in since.values()]


def firebot_rows(trace: Trace) -> list[dict]:
    spans = phases(trace)
    first = next((index for index, (_, _, label) in enumerate(spans) if label == "P3"), None)
    if first is None:
        return []

    roster = roster_guids(trace)
    samples = Samples(trace)
    bots = guids_of_entry(trace, NPC_EMERGENCY_FIRE_BOT)
    hits = [rec for rec in trace.of("dmg")
            if rec.get("d") in roster and rec.get("sp") in (SPELL_FLAMES, SPELL_WATER_SPRAY)]
    spray_killed = [(rec["g"], rec["t"]) for rec in combat_deaths(trace)
                    if trace.entries.get(rec.get("killer")) == NPC_EMERGENCY_FIRE_BOT]
    spray_deaths = [when for _, when in spray_killed]
    sirens = [rec["t"] for rec in trace.of("aura")
              if rec.get("sp") == SPELL_DEAFENING_SIREN and rec.get("d") in roster and not rec.get("r")]
    silences = siren_silences(trace, roster)
    holds = [rec["t"] for rec in trace.of("veto") if rec.get("m") == FIREBOT_AOE_GUARD]
    moves = accepted_moves(trace)
    steps = [rec["t"] for recs in moves.values() for rec in recs if rec.get("by") == FIREBOT_STEP]
    sweeps = sorted(rec["t"] for rec in trace.of("haz") if rec.get("shape") == "sweep")
    windows = []
    for rec in trace.of("cast"):
        if rec.get("sp") != SPELL_SPINNING_UP:
            continue
        stop = rec["t"] + SPIN_WINDOW_MS
        swept = [when for when in sweeps if rec["t"] <= when < stop]
        windows.append((rec["t"], swept[-1] + 1 if swept else stop))
    spray_notes: dict[int, list[int]] = collections.defaultdict(list)
    spray_steps: dict[int, list[int]] = collections.defaultdict(list)
    for rec in notes(trace, "mimiron.flee"):
        text = str(rec.get("txt", ""))
        if text.startswith("spray"):
            spray_notes[rec["g"]].append(rec["t"])
            if " ok " in f"{text} ":
                spray_steps[rec["g"]].append(rec["t"])
    walkbacks: dict[int, list[int]] = collections.defaultdict(list)
    for guid, recs in moves.items():
        for rec in recs:
            if rec.get("by") in WALK_BACK_MOVERS and any(
                    0 < rec["t"] - when <= WALK_BACK_MS for when in spray_steps[guid]):
                walkbacks[guid].append(rec["t"])

    def in_barrage(when: int) -> bool:
        return any(start <= when < stop for start, stop in windows)

    def silent(hit: dict) -> bool:
        return not any(hit["t"] - SPRAY_NOTE_MS <= when <= hit["t"] for when in spray_notes[hit["d"]])

    def walked_back(hit: dict) -> bool:
        return any(0 <= hit["t"] - when <= SPRAY_NOTE_MS for when in walkbacks[hit["d"]])

    def killed(hit: dict) -> bool:
        return any(guid == hit["d"] and abs(when - hit["t"]) <= SPRAY_KILL_MS for guid, when in spray_killed)

    rows = []
    for low, high, label in spans[first:]:
        counts: list[int] = []
        seen: set[int] = set()
        alive: set[int] = set()
        aimed = 0
        for units in samples.window(low, high):
            alive = {guid for guid in bots if guid in units and living(units[guid])}
            counts.append(len(alive))
            seen |= alive
            for guid, unit in units.items():
                if guid in roster and guid not in trace.humans and living(unit) and len(unit) > 7:
                    aimed += unit[7] in bots

        inside = [rec for rec in hits if low <= rec["t"] < high]
        flames = [rec.get("a", 0) for rec in inside if rec["sp"] == SPELL_FLAMES]
        spray = [rec.get("a", 0) for rec in inside if rec["sp"] == SPELL_WATER_SPRAY]
        barrage_hits = [rec for rec in inside if rec["sp"] == SPELL_WATER_SPRAY and in_barrage(rec["t"])]
        walked = [rec for rec in inside if rec["sp"] == SPELL_WATER_SPRAY and walked_back(rec)]
        rows.append({
            "label": label, "span": (low, high),
            "open": counts[0] if counts else 0, "close": counts[-1] if counts else 0,
            "mean": statistics.mean(counts) if counts else 0.0, "max": max(counts, default=0),
            "seen": len(seen), "gone": len(seen - alive),
            "flames": sum(flames), "flames_ticks": len(flames),
            "spray": sum(spray), "spray_hits": len(spray),
            "spray_deaths": sum(1 for when in spray_deaths if low <= when < high),
            "siren": sum(1 for when in sirens if low <= when < high),
            "silenced": sum(max(0, min(stop, high) - max(start, low)) for start, stop in silences) / 1000.0,
            "aimed": aimed * SAMPLE_MS / 1000.0,
            "aoe_holds": sum(1 for when in holds if low <= when < high),
            "steps": sum(1 for when in steps if low <= when < high),
            "barrage_s": sum(max(0, min(stop, high) - max(start, low)) for start, stop in windows) / 1000.0,
            "barrage_spray_hits": len(barrage_hits),
            "barrage_spray_deaths": sum(1 for when in spray_deaths if low <= when < high and in_barrage(when)),
            "barrage_spray_silent": sum(1 for rec in barrage_hits if silent(rec)),
            "barrage_steps": sum(1 for when in steps if low <= when < high and in_barrage(when)),
            "walkbacks": sum(1 for times in walkbacks.values() for when in times if low <= when < high),
            "walkback_spray_hits": len(walked),
            "walkback_spray_deaths": sum(1 for rec in walked if killed(rec)),
        })
    return rows


def show_firebots(trace: Trace) -> None:
    print("FIREBOTS")
    rows = firebot_rows(trace)
    if not rows:
        print("  no phase 3 in this pull")
        return

    for row in rows:
        low, high = row["span"]
        seconds = max((high - low) / 1000.0, 0.001)
        print(f"  {row['label']:4} {clock(low)} .. {clock(high)}  {seconds:5.1f} s  fire bots open {row['open']},"
              f" close {row['close']}, {row['mean']:.1f} alive on average, {row['max']} at most,"
              f" {row['seen']} seen, {row['gone']} gone")
        print(f"       Flames {row['flames'] // 1000}k ({row['flames'] / seconds / 1000:.1f}k/s,"
              f" {row['flames_ticks']} ticks); Water Spray {row['spray_hits']} hits {row['spray'] // 1000}k,"
              f" {row['spray_deaths']} dead; Deafening Siren {row['siren']} on, {row['silenced']:.0f} raid-s silenced")
        print(f"       raid aimed at a fire bot {row['aimed']:.0f} bot-s; splash guard holds {row['aoe_holds']};"
              f" spray or siren steps {row['steps']}")
        print(f"       walk-backs after a spray step {row['walkbacks']}; Water Spray hits after one"
              f" {row['walkback_spray_hits']}, {row['walkback_spray_deaths']} dead")
        if row["barrage_s"]:
            print(f"       in barrage windows ({row['barrage_s']:.0f} s): Water Spray {row['barrage_spray_hits']} hits,"
                  f" {row['barrage_spray_deaths']} dead, {row['barrage_spray_silent']} with no spray note before;"
                  f" spray or siren steps {row['barrage_steps']}")
    print("\n  'gone' fire bots were alive in the phase and not at its end; holds are the guard's veto rows,"
          " which latch per action, so they count episodes, not ticks; a barrage window runs from Spinning"
          " Up's cast to the cone's last sweep row (or"
          f" {SPIN_WINDOW_MS / 1000:.1f} s without them), and 'no spray note' means the victim wrote none in"
          f" the {SPRAY_NOTE_MS / 1000:.0f} s before the hit; a walk-back is an accepted {', '.join(WALK_BACK_MOVERS)}"
          f" leg within {WALK_BACK_MS / 1000:.1f} s of the same bot's spray step ('spray ok' note), and a hit"
          f" after one lands within {SPRAY_NOTE_MS / 1000:.0f} s of it")


def unreach_rows(trace: Trace) -> dict:
    samples = Samples(trace)
    end = pull_end(trace)
    spans = phases(trace)
    mkii = guids_of_entry(trace, NPC_LEVIATHAN_MKII)
    steps = [rec for rec in notes(trace, "mimiron.flee") if rec.get("txt", "").startswith("unreach")]

    marks = sorted(notes(trace, "mimiron.unreach"), key=lambda rec: rec["t"])
    latched = []
    for index, rec in enumerate(marks):
        victim = int(rec.get("txt") or 0)
        if not victim:
            continue
        stop = marks[index + 1]["t"] if index + 1 < len(marks) else end
        health = [row[5] for units in samples.window(rec["t"], stop + 1)
                  for guid, row in units.items() if guid in mkii]
        mine = [step for step in steps if step.get("g") == victim and rec["t"] <= step["t"] <= stop]
        latched.append({"span": (rec["t"], stop), "phase": phase_at(spans, rec["t"]), "victim": victim,
                        "health": (health[0], max(health), health[-1]) if health else None,
                        "steps": len(mine), "ok": sum(1 for step in mine if " ok " in f"{step['txt']} ")})

    rises = []
    last: dict[int, float] = {}
    for index, snap in enumerate(samples.snaps):
        for guid, row in samples.rows(index).items():
            if guid not in mkii:
                continue
            before = last.get(guid)
            if before is not None and before >= REPAIR_FLOOR and row[5] - before > REGEN_RISE:
                rises.append({"t": snap["t"], "phase": phase_at(spans, snap["t"]), "from": before, "to": row[5]})
            last[guid] = row[5]

    return {"probed": bool(marks), "latched": latched, "rises": rises}


def show_unreach(trace: Trace) -> None:
    print("UNREACH")
    rows = unreach_rows(trace)
    if not rows["probed"]:
        print("  no mimiron.unreach latch: a build before it")
    elif not rows["latched"]:
        print("  the MK II never flagged anyone unreachable")
    for span in rows["latched"]:
        begin, stop = span["span"]
        health = span["health"]
        hp = f"MK II {health[0]:.1f} -> max {health[1]:.1f} -> {health[2]:.1f}" if health else "MK II unsampled"
        print(f"  {clock(begin)} .. {clock(stop)}  {(stop - begin) / 1000:5.1f} s  {span['phase']:4}"
              f" {trace.name(span['victim'])[:14]:14} {hp}; unreach steps {span['ok']} ok of {span['steps']}")

    print(f"  MK II health rises off the repair floor: {len(rows['rises'])}")
    for rise in rows["rises"]:
        print(f"      {clock(rise['t'])}  {rise['phase']:4} {rise['from']:5.1f} -> {rise['to']:5.1f}")


def bombbot_rows(trace: Trace) -> dict:
    roster = roster_guids(trace)
    samples = Samples(trace)
    bombs = guids_of_entry(trace, NPC_BOMB_BOT)
    if not bombs:
        return {}

    lives: dict[int, dict] = {}
    for index, snap in enumerate(samples.snaps):
        units = samples.rows(index)
        for guid in bombs:
            row = units.get(guid)
            if row and living(row):
                cell = lives.setdefault(guid, {"first": snap["t"], "last": snap["t"], "hp": row[5]})
                cell["last"] = snap["t"]
                cell["hp"] = row[5]

    killers = {(rec["g"], rec["t"]) for rec in combat_deaths(trace) if rec.get("killer") in bombs}
    blasts: dict[int, list[dict]] = collections.defaultdict(list)
    for rec in trace.of("dmg"):
        if rec.get("sp") == SPELL_BOMB_BOT_BLAST and rec.get("s") in bombs and rec.get("d") in roster:
            blasts[rec["s"]].append(rec)

    victims = collections.Counter()
    for guid, hits in blasts.items():
        units = samples.before(hits[0]["t"] - BOMB_BOT_AIM_LEAD_MS)
        chased = units[guid][7] if guid in units and len(units[guid]) > 7 else 0
        seen = set()
        for rec in hits:
            if rec["d"] in seen:
                continue
            seen.add(rec["d"])
            kind = "chased" if rec["d"] == chased else "collateral"
            victims[kind] += 1
            if any(guid_t[0] == rec["d"] and abs(guid_t[1] - rec["t"]) <= BOMB_BOT_KILL_MS for guid_t in killers):
                victims[kind + " dead"] += 1
        if guid in lives:
            lives[guid]["victims"] = len(seen)
            lives[guid]["chased"] = chased

    legs = []
    steps = collections.Counter()
    for rec in trace.of("move"):
        if rec.get("by") != BOMB_BOT_STEP or rec.get("ok") != 1 or rec.get("g") not in roster:
            continue
        units = samples.before(rec["t"])
        me = units.get(rec["g"])
        if not me:
            continue
        legs.append(math.dist((me[1], me[2]), (rec["x"], rec["y"])))
        near = [units[guid] for guid in bombs if guid in units and living(units[guid])]
        nearest = min(near, key=lambda row: math.dist((row[1], row[2]), (me[1], me[2])), default=None)
        steps["chased" if nearest and nearest[7] == rec["g"] else "bystander"] += 1

    outcomes = collections.Counter()
    screened = 0
    for rec in notes(trace, "mimiron.flee"):
        text = str(rec.get("txt", ""))
        if text.startswith("bombbot kite "):
            outcomes["kite " + text.split()[2]] += 1
        elif text.startswith("bombbot "):
            outcomes["step " + text.split()[1]] += 1
        else:
            refused = BOMB_BOT_REFUSED.search(text)
            screened += int(refused.group(1)) if refused else 0

    return {"lives": lives, "victims": victims, "legs": legs, "steps": steps, "outcomes": outcomes,
            "screened": screened}


def show_bombbots(trace: Trace) -> None:
    print("BOMBBOTS")
    rows = bombbot_rows(trace)
    if not rows:
        print("  no Bomb Bot in this pull")
        return

    lives = rows["lives"]
    exploded = sum(1 for cell in lives.values() if cell.get("victims"))
    print(f"  {len(lives)} Bomb Bots, {exploded} exploded, {len(lives) - exploded} killed or never reached anyone")
    for guid, cell in sorted(lives.items(), key=lambda item: item[1]["first"]):
        tail = (f"  blew up on {trace.name(cell['chased'])[:10]} and {cell['victims'] - 1} more"
                if cell.get("victims") else "")
        print(f"      {clock(cell['first'])} .. {clock(cell['last'])}  {(cell['last'] - cell['first']) / 1000:4.1f} s"
              f"  ended at {cell['hp']:3.0f}%{tail}")
    victims = rows["victims"]
    print(f"  blast victims: chased {victims['chased']} ({victims['chased dead']} dead),"
          f" collateral {victims['collateral']} ({victims['collateral dead']} dead)")
    legs = rows["legs"]
    if legs:
        print(f"  step-outs: {len(legs)} legs ({rows['steps']['bystander']} bystander, {rows['steps']['chased']}"
              f" chased), median {statistics.median(legs):.1f} yd, longest {max(legs):.1f}")
    if rows["outcomes"]:
        print("  flee notes: " + ", ".join(f"{key} {n}" for key, n in sorted(rows["outcomes"].items())))
    print(f"  other dodges refused a spot in a blast {rows['screened']} times")
    print("\n  'chased' is the bomb's target in the snapshot before the blast; a step-out is chased when the nearest"
          " bomb was after the bot itself")


def acutank_rows(trace: Trace) -> list[dict]:
    roster = roster_guids(trace)
    samples = Samples(trace)
    acu = guids_of_entry(trace, NPC_AERIAL_COMMAND_UNIT)
    parts = {**{guid: "ACU" for guid in acu},
             **{guid: "VX-001" for guid in guids_of_entry(trace, NPC_VX001)},
             **{guid: "MK II" for guid in guids_of_entry(trace, NPC_LEVIATHAN_MKII)}}
    main = next((rec["g"] for rec in notes(trace, "mimiron.slot")
                 if str(rec.get("txt", "")).startswith("p4tank")), None)
    off = {guid for guid in roster if trace.role(guid) == "tank" and guid != main}

    def who(guid: int) -> str:
        if guid in off:
            return "off-tank"
        if guid == main:
            return "main tank"
        return trace.role(guid) if guid in roster else "none"

    rows = []
    for low, high, label in phases(trace):
        if not label.startswith("P4"):
            continue
        targeted = collections.Counter()
        aim = collections.Counter()
        for units in samples.window(low, high):
            for guid, unit in units.items():
                if guid in acu and living(unit) and len(unit) > 7:
                    targeted[who(unit[7])] += 1
                elif guid in off and living(unit) and len(unit) > 7:
                    aim[parts.get(unit[7], "other")] += 1
        balls = collections.Counter()
        damage = collections.Counter()
        for rec in trace.of("dmg"):
            if rec.get("sp") == SPELL_PLASMA_BALL_P4 and rec.get("d") in roster and low <= rec["t"] < high:
                balls[who(rec["d"])] += 1
                damage[who(rec["d"])] += rec.get("a", 0)
        dead = collections.Counter(who(rec["g"]) for rec in combat_deaths(trace)
                                   if rec.get("killer") in acu and low <= rec["t"] < high)
        rows.append({"label": label, "span": (low, high), "targeted": targeted, "aim": aim,
                     "balls": balls, "damage": damage, "dead": dead})
    return rows


def show_acutank(trace: Trace) -> None:
    print("ACU TANK")
    rows = acutank_rows(trace)
    if not rows:
        print("  no phase 4 in this pull")
        return

    def shares(counter: collections.Counter) -> str:
        total = sum(counter.values())
        return ", ".join(f"{key} {count / total * 100:.0f}%" for key, count in counter.most_common()) if total else "-"

    for row in rows:
        low, high = row["span"]
        print(f"  {row['label']:4} {clock(low)} .. {clock(high)}  ACU target: {shares(row['targeted'])}")
        print(f"       off-tank aim: {shares(row['aim'])}")
        balls = ", ".join(f"{key} {count} for {row['damage'][key] // 1000}k"
                          for key, count in row["balls"].most_common())
        dead = ", ".join(f"{key} {count}" for key, count in row["dead"].most_common())
        print(f"       Plasma Ball: {balls or 'none'}; killed by the ACU: {dead or 'none'}")
    print("\n  shares are of snapshots; the main tank is whoever took a p4tank slot, every other roster tank is"
          " an off-tank")


def pulse_rows(trace: Trace) -> list[dict]:
    roster = roster_guids(trace)
    heals = [guid for guid in roster if trace.role(guid) == "heal"]
    samples = Samples(trace)
    mkii = guids_of_entry(trace, NPC_LEVIATHAN_MKII)
    vx = guids_of_entry(trace, NPC_VX001)
    heals_in = [rec for rec in trace.of("heal") if rec.get("d") in roster]
    escapes_by = collections.defaultdict(list)
    for rec in trace.of("move"):
        if rec.get("ok") == 1 and rec.get("by") == SHOCK_ESCAPE and rec.get("g") in roster:
            escapes_by[rec["g"]].append(rec["t"])
    hits = sorted((rec for rec in trace.of("dmg")
                   if rec.get("sp") in SPELL_HAND_PULSE and rec.get("d") in roster), key=lambda rec: rec["t"])

    rows = []
    for low, high, label in phases(trace):
        if not label.startswith("P4"):
            continue
        pulses: list[list[dict]] = []
        for rec in hits:
            if not low <= rec["t"] < high:
                continue
            if pulses and rec["t"] - pulses[-1][0]["t"] < PULSE_GROUP_MS:
                pulses[-1].append(rec)
            else:
                pulses.append([rec])
        healer_hits = collections.Counter()
        for pulse in pulses:
            struck = len({rec["d"] for rec in pulse if rec["d"] in heals})
            if struck:
                healer_hits[struck] += 1

        casts = sorted(rec["t"] for rec in trace.of("cast")
                       if rec.get("sp") == SPELL_SHOCK_BLAST and rec.get("s") in mkii and low <= rec["t"] < high)
        escaped = with_mana = in_reach = unreached = 0
        nearest: list[float] = []
        gaps: list[float] = []
        escape_pulse = escape_healed = 0
        for cast in casts:
            read = cast + ESCAPE_SETTLE_MS
            units = samples.before(read)
            healers = [units[guid] for guid in heals
                       if guid in units and living(units[guid]) and units[guid][6] >= DRY_MANA_PCT]
            for guid in roster:
                unit = units.get(guid)
                if (trace.role(guid) != "melee" or not unit or not living(unit) or
                        not any(cast <= when <= read for when in escapes_by.get(guid, []))):
                    continue
                apart = [math.dist(healer[1:3], unit[1:3]) for healer in healers]
                reach = sum(1 for gap in apart if gap <= HEAL_REACH)
                escaped += 1
                with_mana += len(healers)
                in_reach += reach
                unreached += reach == 0
                if apart:
                    nearest.append(min(apart))
                    gaps += apart
                stop = cast + ESCAPE_LOOKBACK_MS
                escape_pulse += sum(rec.get("a", 0) for rec in hits if rec["d"] == guid and cast <= rec["t"] < stop)
                escape_healed += sum(rec.get("a", 0) - rec.get("oh", 0) for rec in heals_in
                                     if rec["d"] == guid and cast <= rec["t"] < stop)

        deaths = collections.Counter()
        for rec in combat_deaths(trace):
            if not low <= rec["t"] < high or not any(
                    hit["d"] == rec["g"] and 0 <= rec["t"] - hit["t"] <= PULSE_KILL_MS for hit in hits):
                continue
            after = any(0 <= rec["t"] - when <= ESCAPE_LOOKBACK_MS for when in escapes_by.get(rec["g"], []))
            deaths[(trace.role(rec["g"]), after)] += 1

        healer_radius: list[float] = []
        for units in samples.window(low, high):
            boss = next((units[guid] for guid in vx if guid in units), None)
            if boss:
                healer_radius += [math.dist(units[guid][1:3], boss[1:3]) for guid in heals
                                  if guid in units and living(units[guid])]

        rows.append({"label": label, "span": (low, high), "pulses": len(pulses), "healer_hits": healer_hits,
                     "casts": len(casts), "escaped": escaped, "with_mana": with_mana, "in_reach": in_reach,
                     "unreached": unreached, "deaths": deaths,
                     "nearest": statistics.median(nearest) if nearest else None,
                     "gap": statistics.mean(gaps) if gaps else None,
                     "healer_radius": statistics.median(healer_radius) if healer_radius else None,
                     "pulse_taken": sum(rec.get("a", 0) for rec in hits if low <= rec["t"] < high),
                     "healed": sum(rec.get("a", 0) - rec.get("oh", 0) for rec in heals_in if low <= rec["t"] < high),
                     "escape_pulse": escape_pulse, "escape_healed": escape_healed})
    return rows


def show_pulse(trace: Trace) -> None:
    print("PULSE")
    rows = pulse_rows(trace)
    if not rows:
        print("  no phase 4 in this pull")
        return

    for row in rows:
        low, high = row["span"]
        struck = ", ".join(f"{count} x{pulses}" for count, pulses in sorted(row["healer_hits"].items()))
        radius_text = "-" if row["healer_radius"] is None else f"{row['healer_radius']:.1f} yd"
        print(f"  {row['label']:4} {clock(low)} .. {clock(high)}  Hand Pulse {row['pulses']}; healers hit by one that"
              f" hit a healer: {struck or 'none'}; healers off VX-001 median {radius_text}")
        print(f"       Hand Pulse on the raid {row['pulse_taken'] // 1000}k against {row['healed'] // 1000}k healed")
        if row["escaped"]:
            n = row["escaped"]
            print(f"       Shock Blast {row['casts']}, melee escapes {n}: healers with mana {row['with_mana'] / n:.2f},"
                  f" in heal reach {row['in_reach'] / n:.2f}, none in reach {row['unreached']}")
            nearest = "-" if row["nearest"] is None else f"{row['nearest']:.1f}"
            gap = "-" if row["gap"] is None else f"{row['gap']:.1f}"
            print(f"       escaped melee to healers with mana: nearest median {nearest} yd, mean {gap} yd;"
                  f" Hand Pulse on them {row['escape_pulse'] // 1000}k against {row['escape_healed'] // 1000}k healed"
                  f" in the {ESCAPE_LOOKBACK_MS // 1000} s from the cast")
        else:
            print(f"       Shock Blast {row['casts']}, no melee escape")
        dead = ", ".join(f"{role} {count}{' after its escape' if after else ''}"
                         for (role, after), count in sorted(row["deaths"].items()))
        print(f"       Hand Pulse deaths: {dead or 'none'}")
    print(f"\n  an escape is an accepted `{SHOCK_ESCAPE}` leg by a melee bot within {ESCAPE_SETTLE_MS // 1000} s of"
          f" the cast, healers read then at {DRY_MANA_PCT:.0f}% mana or more; heal reach is HealDistance"
          f" {HEAL_DISTANCE} between the two bots' edges, {HEAL_REACH} yd centre to centre; a death is after its"
          f" escape when the bot took one in the {ESCAPE_LOOKBACK_MS // 1000} s before it; healed is effective"
          " healing (overheal off) landed on the same bots")


def wedge_row_count(count: int) -> int:
    most = 1 + int(max(SPELL_DISTANCE - SPREAD_RANGE_MARGIN - WEDGE_FIRST_ROW, 0.0) / WEDGE_SPACING)
    held = 0
    for rows in range(1, most + 1):
        held += int(2.0 * WEDGE_HALF_ANGLE * (WEDGE_FIRST_ROW + (rows - 1) * WEDGE_SPACING) / WEDGE_SPACING)
        if held >= count:
            return rows
    return most


def wedge_fire(anchor, flames, centre: float, rows: int) -> int:
    outer = WEDGE_FIRST_ROW + (rows - 1) * WEDGE_SPACING
    fire = 0
    for x, y in flames:
        gap = math.hypot(x - anchor[0], y - anchor[1])
        if gap < WEDGE_FIRST_ROW - FLAMES_RADIUS or gap > outer + FLAMES_RADIUS:
            continue
        off = abs((math.atan2(y - anchor[1], x - anchor[0]) - centre + math.pi) % (2 * math.pi) - math.pi)
        if off <= WEDGE_HALF_ANGLE + math.asin(min(1.0, FLAMES_RADIUS / max(gap, FLAMES_RADIUS))):
            fire += 1
    return fire


def wedge_marks(trace: Trace) -> list[dict]:
    out = []
    for rec in sorted(notes(trace, "mimiron.wedge"), key=lambda rec: rec["t"]):
        match = WEDGE_NOTE.match(str(rec.get("txt", "")))
        if match:
            out.append({"t": rec["t"], "kind": match.group(1), "bearing": int(match.group(2)) % 360,
                        "before": int(match.group(3)) if match.group(3) else None,
                        "after": int(match.group(4)) if match.group(4) else None})
    return out


def wedge_rows(trace: Trace) -> list[dict]:
    span = span_of(phases(trace), "P2")
    if not span:
        return []
    low, high = span

    roster = roster_guids(trace)
    back = {guid for guid in roster if trace.role(guid) in ("ranged", "heal")}
    samples = Samples(trace)
    vx = guids_of_entry(trace, NPC_VX001)
    flame_guids = guids_of_entry(trace, NPC_FLAMES_SPREAD)
    marks = wedge_marks(trace)
    sweeps = sorted(rec["t"] for rec in trace.of("haz") if rec.get("shape") == "sweep")

    def fire_at(when: int, centre: float):
        units = samples.before(when)
        boss = next((units[guid] for guid in vx if guid in units), None)
        if not boss:
            return None
        flames = [(units[guid][1], units[guid][2]) for guid in flame_guids if guid in units and living(units[guid])]
        count = sum(1 for guid in back if guid in units and living(units[guid]))
        return wedge_fire((boss[1], boss[2]), flames, centre, wedge_row_count(max(count, 1)))

    rows = []
    for cast in trace.of("cast"):
        if cast.get("sp") != SPELL_SPINNING_UP or not low <= cast["t"] < high:
            continue
        start = cast["t"]
        swept = [when for when in sweeps if start <= when < start + SPIN_WINDOW_MS]
        stop = swept[-1] + 1 if swept else start + SPIN_WINDOW_MS

        prior = [mark for mark in marks if mark["t"] < start]
        reaim = next((mark for mark in marks if start <= mark["t"] < stop and mark["kind"] == "reaim"), None)
        settle = next((mark for mark in marks if start <= mark["t"] < stop + WEDGE_SETTLE_LOOK_MS
                       and mark["kind"] in ("fire", "keep")), None)
        aimed = reaim["bearing"] if reaim else (prior[-1]["bearing"] if prior else 0)
        taken = settle["bearing"] if settle and settle["kind"] == "fire" else aimed
        read = settle["t"] if settle else stop
        later = min(read + WEDGE_LATER_MS, high)

        mates: list[float] = []
        last = None
        for index, when in enumerate(samples.stamps):
            if not stop <= when < min(stop + WEDGE_AFTER_MS, high):
                continue
            if last is not None and when - last < CONE_MATE_SAMPLE_MS:
                continue
            last = when
            units = samples.rows(index)
            boss = next((units[guid] for guid in vx if guid in units), None)
            if not boss:
                continue
            bearings = [math.atan2(units[guid][2] - boss[2], units[guid][1] - boss[1])
                        for guid in back if guid in units and living(units[guid])]
            if len(bearings) > 1:
                mates.append(statistics.mean(
                    sum(1 for other in bearings
                        if abs((other - mine + math.pi) % (2 * math.pi) - math.pi) < BURST_HALF_ANGLE) - 1
                    for mine in bearings))

        after_high = min(stop + WEDGE_AFTER_MS, high)
        flames_taken = sum(rec.get("a", 0) for rec in trace.of("dmg")
                           if rec.get("sp") == SPELL_FLAMES and rec.get("d") in back and stop <= rec["t"] < after_high)
        deaths = [rec["g"] for rec in combat_deaths(trace) if stop <= rec["t"] < after_high]
        healed = sum(rec.get("a", 0) - rec.get("oh", 0) for rec in trace.of("heal")
                     if rec.get("d") in roster and stop <= rec["t"] < after_high)

        turn = abs((taken - aimed + 180) % 360 - 180)
        rows.append({
            "t": start, "stop": stop, "reaim": reaim["bearing"] if reaim else None, "settle": settle,
            "aimed": aimed, "taken": taken, "turn": turn,
            "fire_left": (fire_at(read, math.radians(aimed)), fire_at(later, math.radians(aimed))),
            "fire_taken": (fire_at(read, math.radians(taken)), fire_at(later, math.radians(taken))),
            "mates": statistics.mean(mates) if mates else None,
            "flames": flames_taken, "dead": sum(1 for guid in deaths if guid in back),
            "heal_moving": role_shares(trace, stop, after_high).get("heal", {}).get("moving"),
            "healed": healed,
            "raid_dead": collections.Counter(trace.role(guid) for guid in deaths if guid in roster),
        })
    return rows


def show_wedge(trace: Trace) -> None:
    print("WEDGE")
    rows = wedge_rows(trace)
    if not rows:
        print("  no phase 2 barrage in this pull")
        return

    def pair(values) -> str:
        return " -> ".join("-" if value is None else str(value) for value in values)

    for row in rows:
        settle = row["settle"]
        if settle is None:
            decided = "no settle note (beams ran past phase 2, or a build without it)"
        else:
            decided = (f"{settle['kind']} {settle['bearing']} at {clock(settle['t'])}"
                       f" ({settle['before']}>{settle['after']} burning in its footprint)")
        reaim = "no re-aim" if row["reaim"] is None else f"reaim {row['reaim']}"
        print(f"  {clock(row['t'])} barrage, beams off {clock(row['stop'])}: {reaim}; {decided}; turn {row['turn']}")
        print(f"       live Flames under {row['aimed']} {pair(row['fire_left'])}"
              + ("" if row["taken"] == row["aimed"] else f", under {row['taken']} {pair(row['fire_taken'])}")
              + f" (at the settle, {WEDGE_LATER_MS // 1000} s later)")
        mates = "-" if row["mates"] is None else f"{row['mates']:.1f}"
        print(f"       the {WEDGE_AFTER_MS // 1000} s after the beams: ranged/healer cone-mates {mates},"
              f" Flames on them {row['flames'] // 1000}k, {row['dead']} of them dead")
        moved = "-" if row["heal_moving"] is None else f"{row['heal_moving'] * 100:.0f}%"
        dead = ", ".join(f"{role} {count}" for role, count in sorted(row["raid_dead"].items()))
        print(f"       healers moving {moved}, healed {row['healed'] // 1000}k,"
              f" raid dead {sum(row['raid_dead'].values())}" + (f" ({dead})" if dead else ""))
    print("\n  footprint: the hmwedge rows off VX-001 at +-60 deg, grown by the 5 yd fire screen; the settle note"
          " counts burning nodes, the live counts every living Flames (Spread); cone-mates are the other ranged"
          " and healers inside one's 60 deg Rapid Burst cone off VX-001, 1 s samples; healed is effective healing"
          " (overheal off) landed on the raid")


SECTIONS = (
    ("phases", "phase spans and deaths by phase", show_phases),
    ("heal", "the phase 2 healing race, 2.5 s at a time, cooldowns included", show_heal),
    ("walk", "walking time per phase and role, by mover", show_walk),
    ("flips", "A-B-A between moves, and formation yards by cause", show_flips),
    ("burst", "Rapid Burst carrier, cone and outside ticks, melee/ranged cross", show_burst),
    ("bomb", "Frost Bomb evacuations and where escapes landed", show_bomb),
    ("slots", "formation slot churn per phase", show_slots),
    ("spin", "each Laser Barrage window and the storm after it", show_spin),
    ("fire", "fire dodges against the ticks behind them, where they went, and the formation's detours",
     show_fire),
    ("handover", "the phase 1 to 2 handover: hold band and walk in, where the bots stood, mana and drinks",
     show_handover),
    ("mines", "Proximity Mine hits and kills by what was moving the bot, and legs that passed a live mine",
     show_mines),
    ("p3", "phase 3 fire bots, Water Spray, the Junk Bot pile and what each core landing was worth",
     show_p3),
    ("unreach", "the MK II's cannot-reach flag: whom it named, for how long, and its health rising under it",
     show_unreach),
    ("firebots", "phase 3 on, per phase: fire bots alive, Flames, Water Spray and Deafening Siren on the raid,"
     " aim at fire bots and the splash guard", show_firebots),
    ("bombbots", "Bomb Bots: how each ended, chased and collateral blast victims, the step-outs and their"
     " length", show_bombbots),
    ("acutank", "phase 4: whom the ACU targeted and hit with Plasma Ball, and what the off-tank aimed at",
     show_acutank),
    ("pulse", "phase 4 Hand Pulse: healers hit per pulse, healers in reach of melee after a Shock Blast"
     " escape and how far, Hand Pulse damage against healing, and Hand Pulse deaths after one", show_pulse),
    ("wedge", "phase 2 barrages: the re-aim, the move off fire once the beams stop and its turn, Flames under"
     " the wedge before and after, and in the 25 s after the ranged and healers' cones, healers moving, healing"
     " and deaths", show_wedge),
)
SUMMARY = ("phases",)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner, SUMMARY)


if __name__ == "__main__":
    sys.exit(main())
