#!/usr/bin/env python3
"""Score an Onyxia pull from a RaidObs trace: phases, tank, threat, layout, eggs, air phase, phase 3.

    onyxia.py <file>            every section
    onyxia.py <file> --phase    phase spans, from onyxia.phase or her health and height
    onyxia.py <file> --tank     her victims, aggro losses, the tank spot, Wing Buffet drift, tank branches,
                                the secure step, which taunts on her took
    onyxia.py <file> --threat   first hit on her per bot against the pull and the tank's hold, lust
    onyxia.py <file> --layout   bearing and range off her facing by role, cone and tail hits by role
    onyxia.py <file> --eggs     egg whelps against cave whelps, who stood at the eggs, the egg pit
    onyxia.py <file> --air      each waypoint visit, Fireball victims, Deep Breath, Blast Nova
    onyxia.py <file> --p3       Bellowing Roar fears, Eruption hits, aggro after touch-down

What the generic views miss here, and what this reads instead:

- **Every `onyxia.*` probe is optional.** A section reads its key where the trace has one and falls
  back to raw records: phases from her health (65%, 40%) and height (above `ONYXIA_AIR_Z` is air),
  the main tank from who she held longest on the ground, whelps from where and when they appeared.
- **Damage on her has no `dmg` row**, those are written for damage a roster member takes. A bot's
  first hit on her is its first cast at her, or the first rise in its `dealt` column while it
  targets her.
- **A Fireball hit lands after the next cast started**: 3 s cast, about 1 s of flight, a cast every
  4 s. Each hit goes to the latest cast already finished.
- **A Lair Guard is not always a watched creature**, so a Blast Nova can land with no cast row
  behind it. Nova kills are read off the hits, never off the casts.
- **Cave whelps are her own 17646 casts**, two at a time at the two cave mouths. A whelp first seen
  near a cave mouth right after one is a cave whelp; anything else hatched from an egg.
- **A taunt that lands leaves no row on her**, auras are written for roster members only. It is read
  off her victim turning to the caster, or off Hand of Reckoning's damage part 67485, which its taunt
  effect casts only when it hits her while she is on someone else.
"""
from __future__ import annotations

import bisect
import collections
import functools
import math
import pathlib
import re
import statistics
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.geometry import NUMBER, Unknown, anchor, dist2, frames, guids_of_entry, radius  # noqa: E402
from raidobs.paths import RAID_ROOT  # noqa: E402
from raidobs.probes import emitted_keys, holder_spans, latch_spans, silent_keys  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, notes, roster_guids  # noqa: E402
from raidobs.validity import DIFFICULTY  # noqa: E402

ENCOUNTER = "onyxia"

# 25-man creatures keep the 10-man entries
NPC_ONYXIA = 10184
NPC_WHELP = 11262
NPC_LAIR_GUARD = 36561

# 10-man id, then the 25-man one where the spell has a difficulty variant
FIREBALL = (18392, 68926)
FLAME_BREATH = (18435, 68970)
WING_BUFFET = (18500, 69293)
TAIL_SWEEP = (68867, 69286)
CLEAVE = (68868,)
ERUPTION = (17731, 69294)
BELLOWING_ROAR = 18431
BLAST_NOVA = 68958
SUMMON_WHELP = 17646
EGG_TRAP = 17647
LUST = (2825, 32182)
MISDIRECTION = 34477
TRICKS = 57934
FEAR_WARD = 6346
TREMOR_TOTEM = 8143
# Taunt, Mocking Blow, Growl, Dark Command, Hand of Reckoning, Righteous Defense's taunt, Death Grip's
TAUNTS = frozenset({355, 694, 6795, 56222, 62124, 31790, 49560})
RECKONING_DAMAGE = 67485

# Kick, Pummel, Shield Bash, Mind Freeze, Counterspell, Wind Shear, Spell Lock, Silencing Shot,
# Strangulate, Silence, every rank
INTERRUPTS = frozenset({1766, 1767, 1768, 1769, 38768, 6552, 6554, 72, 1671, 1672, 29704, 47528, 2139, 57994,
                        19244, 19647, 34490, 47476, 15487})

CONES = (
    ("Flame Breath", FLAME_BREATH, "front"),
    ("Cleave", CLEAVE, "front"),
    ("Wing Buffet", WING_BUFFET, "front"),
    ("Tail Sweep", TAIL_SWEEP, "rear"),
)

PHASE_KEY = "onyxia.phase"
ANCHOR_KEY = "onyxia.anchor"
BREATH_KEY = "onyxia.breath"
FIREBALL_KEY = "onyxia.fireball"
EGG_KEY = "onyxia.egg"
LUST_KEY = "onyxia.lust"
THREAT_KEY = "onyxia.threat"
SLOT_KEY = "onyxia.slot"
TANK_KEY = "onyxia.tank"
P2SPOT_KEY = "onyxia.p2spot"
P2ROLE_KEY = "onyxia.p2role"
DODGE_KEY = "onyxia.dodge"
SLOTFAIL_KEY = "onyxia.slotfail"
DEFENSIVE_KEY = "onyxia.defensive"
PROBE_KEYS = (PHASE_KEY, ANCHOR_KEY, BREATH_KEY, FIREBALL_KEY, EGG_KEY, LUST_KEY, THREAT_KEY, SLOT_KEY,
              TANK_KEY, P2SPOT_KEY, P2ROLE_KEY, DODGE_KEY, SLOTFAIL_KEY, DEFENSIVE_KEY)

DODGE = "onyxia dodge"
ANTI_FEAR = "onyxia anti fear"
GROUND_SLOT = "onyxia ground slot"
AIR_SPOT = "onyxia air spot"
REAR_FLANK = "onyxia rear flank"
THREAT_HOLD = "onyxia threat hold"
TAUNT_GUARD = "onyxia taunt guard"
BURST_WINDOW = "onyxia burst window"
EGG_GUARD = "onyxia egg guard"
TOTEM_GUARD = "onyxia anti fear totem guard"

NONE, GROUND, LIFTOFF, AIR, LANDING, FINAL = range(6)
PHASE_NAMES = {NONE: "none", GROUND: "ground", LIFTOFF: "liftoff", AIR: "air", LANDING: "landing",
               FINAL: "final"}
GROUNDED = (GROUND, FINAL)
THREAT_STATES = {"0": "free", "1": "dwell", "2": "cap", "3": "landing"}

GROUPS = ("tank", "heal", "melee", "caster", "hunter")
HUNTER_CLASS = (3, "hunter")

# Read from the source so a retune shows up here without a second edit.
TANK_SPOT = anchor("ONYXIA_TANK_SPOT")
HOLD_SPOT = anchor("ONYXIA_BOSS_HOLD_SPOT")
P3_PICKUP = anchor("ONYXIA_P3_PICKUP")
CAVES = (anchor("ONYXIA_CAVE_WEST"), anchor("ONYXIA_CAVE_EAST"))
PHASE_3_PCT = radius("ONYXIA_PHASE_3_HEALTH_PCT")
AIR_Z = radius("ONYXIA_AIR_Z")
EGG_DANGER = radius("ONYXIA_EGG_DANGER")
FISSURE_CLEARANCE = radius("ONYXIA_FISSURE_CLEARANCE")
TANK_TOLERANCE = radius("ONYXIA_TANK_SPOT_TOLERANCE")
SLOT_BEARING_FIRST = radius("ONYXIA_SLOT_BEARING_FIRST")
SLOT_BEARING_STEP = radius("ONYXIA_SLOT_BEARING_STEP")
SLOT_RADII = (radius("ONYXIA_SLOT_RADIUS_HEAL"), radius("ONYXIA_SLOT_RADIUS_CASTER"),
              radius("ONYXIA_SLOT_RADIUS_HUNTER"))
SLOT_RADIUS_OVERFLOW = radius("ONYXIA_SLOT_RADIUS_OVERFLOW")
FLANK_MAX = radius("ONYXIA_FLANK_MAX_DEG")
AIR_SPELL_RANGE = radius("ONYXIA_AIR_SPELL_RANGE")
WAYPOINT_SNAP = radius("ONYXIA_AIR_WAYPOINT_SNAP")

ONY_DATA = RAID_ROOT / "Ony" / "Util" / "OnyData.h"
INT_DECL = re.compile(r"\bconstexpr\s+uint(?:8|16|32)\s+(ONYXIA_[A-Z0-9_]+)\s*=\s*(\d+)\s*;")
LANE_TABLE = re.compile(r"\bBREATH_LANES\s*=\s*\{\{(.*?)\}\};", re.S)
LANE_ROW = re.compile(r"\{\s*(\d+)\s*,\s*(\d+)\s*," + r"\s*,".join(
    [r"\s*\{\s*" + NUMBER + r"\s*,\s*" + NUMBER + r"\s*,\s*" + NUMBER + r"\s*\}"] * 3) + r"\s*\}")
FISSURE_TABLE = re.compile(r"\bLAVA_FISSURES\s*=\s*\{\{(.*?)\}\};", re.S)
FISSURE_ROW = re.compile(r"\{\s*" + NUMBER + r"\s*,\s*" + NUMBER + r"\s*\}")


@functools.lru_cache(maxsize=1)
def ony_source() -> str:
    try:
        return ONY_DATA.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def parse_lanes(text: str) -> dict[int, dict]:
    """Waypoint -> `{spell, hover, from, to}` off `BREATH_LANES`."""
    found = LANE_TABLE.search(text)
    if not found:
        return {}
    out = {}
    for row in LANE_ROW.findall(found.group(1)):
        values = [float(value) for value in row[2:]]
        out[int(row[0])] = {"spell": int(row[1]), "hover": tuple(values[0:3]), "from": tuple(values[3:6]),
                            "to": tuple(values[6:9])}
    return out


def parse_fissures(text: str) -> list[tuple[float, float]]:
    found = FISSURE_TABLE.search(text)
    return [(float(x), float(y)) for x, y in FISSURE_ROW.findall(found.group(1))] if found else []


def source_int(name: str, text: str | None = None) -> int:
    """A `constexpr uintN ONYXIA_*`, which `radius()` only reads as a float."""
    for key, value in INT_DECL.findall(ony_source() if text is None else text):
        if key == name:
            return int(value)
    raise Unknown(f"no integer named {name} in {ONY_DATA.name}")


LANES = parse_lanes(ony_source())
FISSURES = parse_fissures(ony_source())
LANE_SPELLS = {lane["spell"]: waypoint for waypoint, lane in LANES.items()}
THREAT_DWELL_MS = source_int("ONYXIA_THREAT_DWELL_MS")
BREATH_LIVE_MS = source_int("ONYXIA_BREATH_LIVE_MS")
SLOTS_PER_RING = source_int("ONYXIA_SLOTS_PER_RING")

# Not in OnyData.h: the core script's 65% lift-off, and the ring spacing past the overflow one.
PHASE_2_PCT = 65.0
OVERFLOW_RING_STEP = 6.0

# Wing Buffet throws the tank about 30 yd and she walks after him, so 6 s after the cast she has
# arrived wherever he landed.
SETTLE_MS = 6000
KNOCK_MS = 2000
# 4 yd random spread at the cave mouth plus a sample of walking
CAVE_RADIUS = 10.0
CAVE_LAG_MS = 1500
# egg pit floor sits under -90, the main floor and cave mouths above -89.5
PIT_Z = -89.9
STUCK_MS = 10000
HATCH_BURST_MS = 2000
REST_RADIUS = 2.0
REST_MIN_MS = 2000
MIN_VISIT_MS = 1500
TARGET_SHARE = 0.25
FIREBALL_FLIGHT_MS = 4000
LAND_SLACK_MS = 1000
NOVA_KILL_MS = 2000
FEAR_MS = 2000
ERUPTION_MS = 4000
LAYOUT_RANGE = 60.0
FRESH_MS = 1000
# A landed taunt turns her at once, so the next snapshot or two shows it; a threat takeover lands later.
TAUNT_TOOK_MS = 600
RECKONING_MS = 500
# one snapshot gap longer than this is a hole in the trace, not time spent anywhere
MAX_STEP_MS = 2000
SHOWN = 12


def pull_end(trace: Trace) -> int:
    ends = trace.of("end")
    if ends:
        return ends[-1]["t"]
    return trace.records[-1].get("t", 0) if trace.records else 0


def boss_guid(trace: Trace) -> int | None:
    guids = sorted(guids_of_entry(trace, NPC_ONYXIA))
    return guids[0] if guids else None


def size_label(trace: Trace) -> str:
    return DIFFICULTY.get(trace.header.get("diff"), f"difficulty {trace.header.get('diff')}")


def group_of(trace: Trace, guid: int) -> str:
    role = trace.role(guid)
    if role != "ranged":
        return role
    klass = next((member.get("c") for member in trace.header.get("roster", []) if member["g"] == guid), None)
    return "hunter" if klass in HUNTER_CLASS else "caster"


def dist3(a, b) -> float:
    return math.dist(a[:3], b[:3])


def bearing(her, spot) -> float:
    """Degrees off her facing, `her` a snapshot row. Positive turns the way orientation grows, which
    is west while she faces north."""
    angle = math.degrees(math.atan2(spot[1] - her[2], spot[0] - her[1]) - her[4])
    return (angle + 180.0) % 360.0 - 180.0


def median_or_none(values):
    return statistics.median(values) if values else None


def pct(part, whole) -> str:
    return f"{part * 100.0 / whole:.0f}%" if whole else "-"


def steps_line(steps: list[str]) -> str:
    return " -> ".join(steps[:SHOWN]) + (f" and {len(steps) - SHOWN} more" if len(steps) > SHOWN else "")


class Samples:
    """Every snapshot row by guid, so a view can ask where a unit stood at a time without rescanning."""

    def __init__(self, trace: Trace):
        self.stamps: list[int] = []
        self.rows: dict[int, tuple[list[int], list[list]]] = {}
        for snap in frames(trace):
            self.stamps.append(snap["t"])
            for row in snap.get("u", []):
                stamps, rows = self.rows.setdefault(row[0], ([], []))
                stamps.append(snap["t"])
                rows.append(row)

    def row(self, guid: int, when: int, fresh: int | None = None):
        """The guid's last row at or before `when`, or None, also when that row is older than `fresh`."""
        found = self.rows.get(guid)
        if not found:
            return None
        index = bisect.bisect_right(found[0], when)
        if not index or (fresh is not None and when - found[0][index - 1] > fresh):
            return None
        return found[1][index - 1]

    def between(self, guid: int, low: int, high: int) -> list[tuple[int, list]]:
        found = self.rows.get(guid)
        if not found:
            return []
        start = bisect.bisect_left(found[0], low)
        stop = bisect.bisect_right(found[0], high)
        return list(zip(found[0][start:stop], found[1][start:stop]))

    def weighted(self, guid: int, low: int, high: int) -> list[tuple[int, list, int]]:
        """`(t, row, weight)` for the guid's rows in range, weighted by the gap to its next row."""
        rows = self.between(guid, low, high)
        return [(when, row, min((rows[index + 1][0] if index + 1 < len(rows) else when) - when, MAX_STEP_MS))
                for index, (when, row) in enumerate(rows)]


def samples_of(trace: Trace) -> Samples:
    cached = getattr(trace, "_ony_samples", None)
    if cached is None:
        cached = Samples(trace)
        trace._ony_samples = cached
    return cached


def her_casts(trace: Trace, spells, triggered: bool = False) -> list[dict]:
    boss = boss_guid(trace)
    return [rec for rec in trace.of("cast")
            if rec.get("s") == boss and rec.get("sp") in spells and (triggered or not rec.get("tr"))]


def missing_probes(trace: Trace) -> list[str]:
    """Keys the source declares for her, plus the ones this reader reads, that the trace never wrote."""
    emitted = emitted_keys(trace)
    declared = {key for key, _, _ in silent_keys(emitted, encounter_of(trace))}
    return sorted(declared | {key for key in PROBE_KEYS if key not in emitted})


def derived_phases(trace: Trace) -> list[tuple[int, int, int]]:
    """`(phase, start, stop)` off her health and height, for a trace without `onyxia.phase`."""
    boss = boss_guid(trace)
    if boss is None:
        return []
    end = pull_end(trace)
    out: list[list[int]] = []
    seen_air = False
    current = None
    for when, row in samples_of(trace).between(boss, 0, end):
        if row[5] <= 0:
            continue
        if row[3] > AIR_Z:
            seen_air = True
            phase = LANDING if row[5] <= PHASE_3_PCT else AIR
        elif seen_air:
            phase = FINAL if row[5] <= PHASE_3_PCT else current
        else:
            phase = LIFTOFF if row[5] <= PHASE_2_PCT else GROUND
        if phase != current:
            if out:
                out[-1][2] = when
            out.append([phase, when if out else 0, end])
            current = phase
    return [tuple(span) for span in out]


def phase_spans(trace: Trace) -> tuple[list[tuple[int, int, int]], str]:
    """Phase spans clipped to the pull, and where they came from."""
    cached = getattr(trace, "_ony_phases", None)
    if cached is not None:
        return cached
    end = pull_end(trace)
    spans = []
    for held, start, stop in latch_spans(trace, PHASE_KEY, end):
        try:
            phase = int(held)
        except ValueError:
            continue
        start, stop = max(start, 0), min(stop, end)
        if stop > start:
            spans.append((phase, start, stop))
    cached = (spans, PHASE_KEY) if spans else (derived_phases(trace), "her health and height")
    trace._ony_phases = cached
    return cached


def phase_at(spans, when: int) -> int | None:
    return next((phase for phase, start, stop in spans if start <= when < stop), None)


def first_start(spans, phases) -> int | None:
    return min((start for phase, start, _ in spans if phase in phases), default=None)


def liftoff_time(trace: Trace) -> int | None:
    return first_start(phase_spans(trace)[0], (LIFTOFF, AIR, LANDING))


def touchdown_time(trace: Trace) -> int | None:
    return first_start(phase_spans(trace)[0], (FINAL,))


def show_phase(trace: Trace) -> None:
    print("PHASES")
    boss = boss_guid(trace)
    if boss is None:
        print("  Onyxia was never named")
        return
    spans, source = phase_spans(trace)
    if not spans:
        print("  Onyxia was never sampled in the pull")
        return
    samples = samples_of(trace)
    print(f"  from {source}")
    print(f"  {'phase':8} {'from':>9} {'to':>9} {'len':>6} {'hp in':>6} {'hp out':>6}")
    for phase, start, stop in spans:
        rows = samples.between(boss, start, stop - 1)
        hp_in = f"{rows[0][1][5]:.1f}" if rows else "-"
        hp_out = f"{rows[-1][1][5]:.1f}" if rows else "-"
        print(f"  {PHASE_NAMES.get(phase, str(phase)):8} {clock(start):>9} {clock(stop):>9}"
              f" {(stop - start) / 1000:6.1f} {hp_in:>6} {hp_out:>6}")
    reached = {phase for phase, _, _ in spans}
    if FINAL not in reached:
        health = [row[5] for _, row in samples.between(boss, 0, pull_end(trace)) if row[5] > 0]
        floor = f", her health floor {min(health):.2f}%" if health else ""
        print(f"  never reached phase 3{floor}")


def victim_changes(trace: Trace) -> list[tuple[int, int]]:
    """`(t, victim)` at each change of her victim while she lives, 0 for none."""
    boss = boss_guid(trace)
    if boss is None:
        return []
    out: list[tuple[int, int]] = []
    for when, row in samples_of(trace).between(boss, 0, pull_end(trace)):
        if row[5] <= 0:
            continue
        victim = row[7] if len(row) > 7 else 0
        if not out or out[-1][1] != victim:
            out.append((when, victim))
    return out


def main_tank(trace: Trace) -> int | None:
    """Whoever wrote `onyxia.tank`, else the tank she held longest on the ground."""
    marks = collections.Counter(rec.get("g") for rec in notes(trace, TANK_KEY) if rec.get("g"))
    if marks:
        return marks.most_common(1)[0][0]
    spans, _ = phase_spans(trace)
    end = pull_end(trace)
    held: collections.Counter = collections.Counter()
    changes = victim_changes(trace)
    for index, (when, victim) in enumerate(changes):
        stop = changes[index + 1][0] if index + 1 < len(changes) else end
        if victim and phase_at(spans, when) in GROUNDED:
            held[victim] += stop - when
    tanks = [guid for guid, _ in held.most_common() if trace.role(guid) == "tank"]
    if tanks:
        return tanks[0]
    return held.most_common(1)[0][0] if held else None


def tank_hold(trace: Trace, tank: int | None) -> int | None:
    """When the main tank first held her."""
    return next((when for when, victim in victim_changes(trace) if tank and victim == tank), None)


def aggro_losses(trace: Trace, tank: int | None = None) -> list[dict]:
    """Each stretch on the ground where her victim was someone other than the main tank."""
    tank = main_tank(trace) if tank is None else tank
    boss = boss_guid(trace)
    spans, _ = phase_spans(trace)
    samples = samples_of(trace)
    end = pull_end(trace)
    changes = victim_changes(trace)
    out = []
    for index, (when, victim) in enumerate(changes):
        if victim in (0, tank) or phase_at(spans, when) not in GROUNDED:
            continue
        stop = changes[index + 1][0] if index + 1 < len(changes) else end
        mine, hers = samples.row(tank, when, FRESH_MS) if tank else None, samples.row(boss, when)
        out.append({
            "start": when,
            "stop": stop,
            "guid": victim,
            "tank_gap": dist2(mine[1:3], hers[1:3]) if mine and hers else None,
            "tank_alive": bool(mine and mine[5] > 0),
        })
    return out


def tank_spot_stats(trace: Trace, tank: int | None) -> dict | None:
    """The main tank's distance to `ONYXIA_TANK_SPOT` over phase 1."""
    if not tank:
        return None
    spans, _ = phase_spans(trace)
    gaps = [(when, dist2(row[1:3], TANK_SPOT)) for when, row in samples_of(trace).between(tank, 0, pull_end(trace))
            if row[5] > 0 and phase_at(spans, when) == GROUND]
    if not gaps:
        return None
    values = [gap for _, gap in gaps]
    return {
        "median": statistics.median(values),
        "min": min(values),
        "on": sum(1 for gap in values if gap <= TANK_TOLERANCE) / len(values),
        "first_on": next((when for when, gap in gaps if gap <= TANK_TOLERANCE), None),
    }


def rests(trace: Trace, tank: int | None) -> list[dict]:
    """Her standing spots on the ground while the main tank held her, longest first, with his distance
    to the tank spot over each."""
    boss = boss_guid(trace)
    if boss is None or not tank:
        return []
    spans, _ = phase_spans(trace)
    samples = samples_of(trace)
    out = []
    current = None

    def close(stop):
        if current and stop - current["start"] >= REST_MIN_MS:
            current["stop"] = stop
            current["tank_gap"] = median_or_none(current.pop("gaps"))
            out.append(current)

    for when, row in samples.between(boss, 0, pull_end(trace)):
        still = (row[5] > 0 and len(row) > 8 and not row[8] and row[7] == tank
                 and phase_at(spans, when) in GROUNDED)
        if current and (not still or dist2(row[1:3], current["spot"]) > REST_RADIUS):
            close(when)
            current = None
        if not still:
            continue
        if current is None:
            current = {"start": when, "spot": tuple(row[1:4]), "gaps": []}
        mine = samples.row(tank, when, FRESH_MS)
        if mine and mine[5] > 0:
            current["gaps"].append(dist2(mine[1:3], TANK_SPOT))
    if current:
        close(pull_end(trace))
    return sorted(out, key=lambda rest: rest["start"] - rest["stop"])


def refit_spot(stands: list[dict]) -> tuple[float, float, float] | None:
    """Her spot averaged by time over the rests with the main tank on his spot: what
    `ONYXIA_BOSS_HOLD_SPOT` should read."""
    fitted = [rest for rest in stands if rest["tank_gap"] is not None and rest["tank_gap"] <= TANK_TOLERANCE]
    weight = sum(rest["stop"] - rest["start"] for rest in fitted)
    if not weight:
        return None
    return tuple(sum(rest["spot"][axis] * (rest["stop"] - rest["start"]) for rest in fitted) / weight
                 for axis in range(3))


def wing_buffets(trace: Trace, tank: int | None) -> list[dict]:
    """Per Wing Buffet, where she stood at the cast and once she had walked after the tank."""
    boss = boss_guid(trace)
    samples = samples_of(trace)
    out = []
    for rec in her_casts(trace, WING_BUFFET):
        before, after = samples.row(boss, rec["t"]), samples.row(boss, rec["t"] + SETTLE_MS)
        if not before or not after:
            continue
        mine = samples.row(tank, rec["t"], FRESH_MS) if tank else None
        thrown = samples.row(tank, rec["t"] + KNOCK_MS, FRESH_MS) if tank else None
        out.append({
            "t": rec["t"],
            "from": tuple(before[1:4]),
            "to": tuple(after[1:4]),
            "north": after[1] - before[1],
            "moved": dist2(before[1:3], after[1:3]),
            "tank": dist2(mine[1:3], thrown[1:3]) if mine and thrown else None,
        })
    return out


def secure_rows(trace: Trace, tank: int | None) -> list[dict]:
    """Each `secure` stretch of the main tank's branch and the branch after it: `drag` once he had his
    lead, anything else when she left him first."""
    branches = holder_spans(trace, TANK_KEY, pull_end(trace)).get(tank, []) if tank else []
    return [{"start": start, "stop": stop, "next": branches[index + 1][0] if index + 1 < len(branches) else None}
            for index, (held, start, stop) in enumerate(branches) if held == "secure"]


def walk_losses(trace: Trace, tank: int | None) -> list[dict]:
    """Aggro losses that began while he walked her to the tank spot."""
    walks = [(start, stop) for held, start, stop in holder_spans(trace, TANK_KEY, pull_end(trace)).get(tank, [])
             if held in ("drag", "return")] if tank else []
    return [loss for loss in aggro_losses(trace, tank)
            if any(start <= loss["start"] <= stop + FRESH_MS for start, stop in walks)]


def taunt_rows(trace: Trace) -> list[dict]:
    """Each taunt aimed at her: on the caster already when the samples either side of the cast show him,
    or took when her victim turned to the caster within `TAUNT_TOOK_MS` or Hand of Reckoning's damage
    part went off on her."""
    boss = boss_guid(trace)
    if boss is None:
        return []
    samples = samples_of(trace)
    casts = [rec for rec in trace.of("cast") if rec.get("tgt") == boss]
    reckonings = [(rec["t"], rec.get("s")) for rec in casts if rec.get("sp") == RECKONING_DAMAGE]
    out = []
    for rec in casts:
        if rec.get("sp") not in TAUNTS:
            continue
        caster = rec.get("s")
        before = samples.row(boss, rec["t"])
        on_caster = [len(row) > 7 and row[7] == caster
                     for _, row in samples.between(boss, rec["t"] + 1, rec["t"] + TAUNT_TOOK_MS)]
        already = bool(before and len(before) > 7 and before[7] == caster and on_caster and on_caster[0])
        turned = any(on_caster)
        hit = any(source == caster and 0 <= when - rec["t"] <= RECKONING_MS for when, source in reckonings)
        out.append({"t": rec["t"], "caster": caster, "spell": rec["sp"], "already": already,
                    "took": not already and (turned or hit)})
    return out


def show_tank(trace: Trace) -> None:
    print("TANK")
    boss = boss_guid(trace)
    if boss is None:
        print("  Onyxia was never named")
        return
    tank = main_tank(trace)
    source = TANK_KEY if notes(trace, TANK_KEY) else "who she held longest on the ground"
    print(f"  main tank {trace.name(tank)}{' (human)' if tank in trace.humans else ''}, from {source}")

    changes = victim_changes(trace)
    named = [f"{clock(when)} {trace.name(victim) if victim else 'none'}" for when, victim in changes]
    print(f"  her victims: {steps_line(named)}")

    losses = aggro_losses(trace, tank)
    if losses:
        print(f"\n  aggro lost on the ground {len(losses)} time(s):")
        print(f"  {'at':>9} {'len':>6} {'took her':14} {'role':6} {'tank yd':>7}")
        for loss in losses:
            gap = f"{loss['tank_gap']:.1f}" if loss["tank_gap"] is not None else "-"
            if not loss["tank_alive"]:
                gap = "dead"
            print(f"  {clock(loss['start']):>9} {(loss['stop'] - loss['start']) / 1000:6.1f}"
                  f" {trace.name(loss['guid'])[:14]:14} {trace.role(loss['guid']):6} {gap:>7}")
    else:
        print("  never lost aggro on the ground")

    spot = tank_spot_stats(trace, tank)
    if spot:
        first = clock(spot["first_on"]) if spot["first_on"] is not None else "never"
        print(f"\n  main tank from ONYXIA_TANK_SPOT in phase 1: median {spot['median']:.1f} yd, min"
              f" {spot['min']:.1f}, within {TANK_TOLERANCE:.0f} yd {spot['on'] * 100:.0f}% of samples,"
              f" first {first}")

    stands = rests(trace, tank)
    if stands:
        print(f"  where she stood while he held her, from ONYXIA_BOSS_HOLD_SPOT {HOLD_SPOT[:2]}:")
        for rest in stands[:6]:
            x, y, z = rest["spot"]
            gap = f"{rest['tank_gap']:.1f}" if rest["tank_gap"] is not None else "-"
            print(f"    {clock(rest['start']):>9} {(rest['stop'] - rest['start']) / 1000:5.1f} s at"
                  f" ({x:.1f}, {y:.1f}, {z:.2f}), {dist2((x, y), HOLD_SPOT):5.1f} yd off; tank {gap} yd off his spot")
        fitted = refit_spot(stands)
        if fitted:
            print(f"  refit: with the tank on his spot she stood at"
                  f" ({fitted[0]:.1f}, {fitted[1]:.1f}, {fitted[2]:.2f})")
        else:
            print("  no refit: she never stood still with the tank on his spot")

    buffets = wing_buffets(trace, tank)
    if buffets:
        print(f"\n  Wing Buffet, her spot at the cast and {SETTLE_MS / 1000:.0f} s on (+x north):")
        for row in buffets:
            thrown = f"{row['tank']:.1f}" if row["tank"] is not None else "-"
            print(f"    {clock(row['t']):>9} x {row['from'][0]:7.1f} -> {row['to'][0]:7.1f}"
                  f"  north {row['north']:+6.1f}  moved {row['moved']:5.1f}  tank thrown {thrown}")
        print(f"  net {sum(row['north'] for row in buffets):+.1f} yd north over {len(buffets)} buffet(s)")

    branches = holder_spans(trace, TANK_KEY, pull_end(trace)).get(tank, []) if tank else []
    if branches:
        totals: collections.Counter = collections.Counter()
        for held, start, stop in branches:
            totals[held] += stop - start
        print(f"\n  {TANK_KEY}: {steps_line([f'{clock(start)} {held}' for held, start, _ in branches])}")
        print("  time per branch: " + ", ".join(f"{held} {span / 1000:.1f} s" for held, span in totals.most_common()))
        secures = secure_rows(trace, tank)
        if secures:
            print("  secure: " + ", ".join(f"{clock(row['start'])} {(row['stop'] - row['start']) / 1000:.1f} s"
                                         f" -> {row['next'] or 'end'}" for row in secures))
        lost = walk_losses(trace, tank)
        print(f"  aggro lost while he walked her to the spot: {len(lost)}"
              + (" (" + ", ".join(f"{clock(loss['start'])} {trace.name(loss['guid'])}" for loss in lost) + ")"
                 if lost else ""))

    cooldowns = [(start, held) for held, start, _ in holder_spans(trace, DEFENSIVE_KEY).get(tank, [])
                 if held not in ("none", "covered")] if tank else []
    if cooldowns:
        print(f"  {DEFENSIVE_KEY}: " + ", ".join(f"{clock(when)} {held}" for when, held in cooldowns))

    taunts = taunt_rows(trace)
    if taunts:
        took = collections.Counter(row["caster"] for row in taunts if row["took"])
        print(f"\n  taunts on her: {len(taunts)} cast, {sum(took.values())} took,"
              f" {sum(row['already'] for row in taunts)} on the caster already")
        cast_by = collections.Counter(row["caster"] for row in taunts)
        print("    " + ", ".join(f"{trace.name(guid)} {count} (took {took[guid]})" for guid, count in cast_by.most_common()))


def first_hits(trace: Trace) -> dict[int, tuple[int, str]]:
    """Per roster member, `(t, how)` of its first hit on her: a cast at her, or `dealt` rising while
    it targeted her."""
    boss = boss_guid(trace)
    if boss is None:
        return {}
    roster = roster_guids(trace)
    out: dict[int, tuple[int, str]] = {}
    for rec in trace.of("cast"):
        if rec.get("tgt") == boss and rec.get("s") in roster:
            out.setdefault(rec["s"], (rec["t"], "cast"))
    samples = samples_of(trace)
    end = pull_end(trace)
    for guid in roster:
        last = None
        for when, row in samples.between(guid, -(1 << 62), end):
            if len(row) <= 11:
                break
            if last is not None and row[11] > last and row[7] == boss:
                if guid not in out or when < out[guid][0]:
                    out[guid] = (when, "dealt")
                break
            last = row[11]
    return out


def threat_rows(trace: Trace) -> tuple[int | None, int | None, list[dict]]:
    """The main tank, when he first held her, and each member's first hit against both."""
    tank = main_tank(trace)
    hold = tank_hold(trace, tank)
    rows = []
    for guid, (when, how) in first_hits(trace).items():
        since = when - hold if hold is not None else None
        rows.append({
            "guid": guid,
            "t": when,
            "how": how,
            "since_hold": since,
            "early": (guid != tank and trace.role(guid) != "heal" and since is not None
                      and since < THREAT_DWELL_MS),
        })
    return tank, hold, sorted(rows, key=lambda row: row["t"])


def lust_casts(trace: Trace) -> list[dict]:
    spans, _ = phase_spans(trace)
    return [{"t": rec["t"], "caster": rec.get("s"), "spell": rec.get("sp"),
             "phase": PHASE_NAMES.get(phase_at(spans, rec["t"]), "before the pull")}
            for rec in trace.of("cast") if rec.get("sp") in LUST and not rec.get("tr")]


def redirect_casts(trace: Trace) -> dict[tuple[int, int], list[int]]:
    """When each caster sent Misdirection or Tricks to each target."""
    out: dict[tuple[int, int], list[int]] = collections.defaultdict(list)
    for rec in trace.of("cast"):
        if rec.get("sp") in (MISDIRECTION, TRICKS) and not rec.get("tr"):
            out[(rec.get("s"), rec.get("tgt"))].append(rec["t"])
    return dict(out)


def threat_states(trace: Trace) -> dict[str, tuple[int, int]]:
    """Per `onyxia.threat` state, how many bots held it and for how long in total."""
    bots: dict[str, set] = collections.defaultdict(set)
    held: collections.Counter = collections.Counter()
    for guid, spans in holder_spans(trace, THREAT_KEY, pull_end(trace)).items():
        for value, start, stop in spans:
            name = THREAT_STATES.get(value, value)
            bots[name].add(guid)
            held[name] += max(0, stop - max(start, 0))
    return {name: (len(bots[name]), held[name]) for name in bots}


def show_threat(trace: Trace) -> None:
    print("THREAT")
    if boss_guid(trace) is None:
        print("  Onyxia was never named")
        return
    tank, hold, rows = threat_rows(trace)
    print(f"  main tank {trace.name(tank)} first held her at {clock(hold) if hold is not None else 'never'};"
          f" the rest wait {THREAT_DWELL_MS / 1000:.0f} s after that")
    if not rows:
        print("  nobody hit her")
    else:
        print(f"  {'group':7} {'bots':>4} {'first':>9} {'median':>9} {'last':>9} {'early':>5}   (s from his hold)")
        for group in GROUPS:
            mine = [row for row in rows if group_of(trace, row["guid"]) == group]
            since = [row["since_hold"] for row in mine if row["since_hold"] is not None]
            if not mine:
                continue
            if since:
                print(f"  {group:7} {len(mine):4} {min(since) / 1000:+9.1f} {statistics.median(since) / 1000:+9.1f}"
                      f" {max(since) / 1000:+9.1f} {sum(1 for row in mine if row['early']):5}")
            else:
                print(f"  {group:7} {len(mine):4} {'-':>9} {'-':>9} {'-':>9} {'-':>5}")
        early = [row for row in rows if row["early"]]
        if early:
            print(f"  on her inside the {THREAT_DWELL_MS / 1000:.0f} s dwell:")
            for row in early:
                print(f"    {clock(row['t']):>9} {row['since_hold'] / 1000:+6.1f} s"
                      f"  {trace.name(row['guid'])[:14]:14}"
                      f" {group_of(trace, row['guid']):6} by {row['how']}")
        missing = sorted(trace.name(guid) for guid in roster_guids(trace)
                         if guid not in {row["guid"] for row in rows} and trace.role(guid) != "heal")
        if missing:
            print(f"  never on her: {', '.join(missing)}")

    lusts = lust_casts(trace)
    for row in lusts:
        rel = f", {(row['t'] - hold) / 1000:+.1f} s from his hold" if hold is not None else ""
        print(f"  {trace.spell(row['spell'])} by {trace.name(row['caster'])} at {clock(row['t'])},"
              f" {row['phase']} phase{rel}")
    if not lusts:
        print("  no lust cast")
    allowed = [start for held, start, _ in latch_spans(trace, LUST_KEY) if held == "1"]
    if allowed:
        print(f"  {LUST_KEY} allowed it at {clock(allowed[0])}")

    states = threat_states(trace)
    if states:
        print(f"  {THREAT_KEY}: " + ", ".join(f"{name} {bots} bot(s) {span / 1000:.0f} s"
                                             for name, (bots, span) in sorted(states.items())))
    vetoes = collections.Counter(rec.get("m") for rec in trace.of("veto")
                                 if rec.get("m") in (THREAT_HOLD, BURST_WINDOW, TAUNT_GUARD))
    if vetoes:
        print("  vetoes: " + ", ".join(f"{name} {count}" for name, count in vetoes.most_common()))
    redirects = redirect_casts(trace)
    if redirects:
        print("  redirects: " + ", ".join(
            f"{trace.name(caster)} -> {trace.name(target)} ({trace.role(target)})"
            f" x{len(times)} first {clock(times[0])}"
            for (caster, target), times in sorted(redirects.items(), key=lambda item: item[1][0])))


def layout(trace: Trace) -> dict[str, dict]:
    """Per group, every grounded sample's bearing off her facing and range from her."""
    boss = boss_guid(trace)
    if boss is None:
        return {}
    spans, _ = phase_spans(trace)
    samples = samples_of(trace)
    roster = roster_guids(trace)
    groups = {guid: group_of(trace, guid) for guid in roster}
    out: dict[str, dict] = {}
    for when, her in samples.between(boss, 0, pull_end(trace)):
        if her[5] <= 0 or len(her) <= 7 or not her[7] or phase_at(spans, when) not in GROUNDED:
            continue
        for guid in roster:
            mine = samples.row(guid, when, FRESH_MS)
            if not mine or mine[5] <= 0:
                continue
            gap = dist2(mine[1:3], her[1:3])
            if gap > LAYOUT_RANGE:
                continue
            row = out.setdefault(groups[guid], {"bots": set(), "bearings": [], "gaps": []})
            row["bots"].add(guid)
            row["bearings"].append(bearing(her, mine[1:3]))
            row["gaps"].append(gap)
    return out


def cone_hits(trace: Trace) -> list[dict]:
    """Per frontal or rear spell, her casts and its hits by group."""
    out = []
    for label, spells, side in CONES:
        hits = collections.Counter(group_of(trace, rec.get("d")) for rec in trace.of("dmg")
                                   if rec.get("sp") in spells)
        out.append({"label": label, "side": side, "casts": len(her_casts(trace, spells)), "hits": hits})
    return out


def slot_spot(slot: int) -> tuple[float, float]:
    """`(radius, bearing)` of a ground slot, bearing signed positive for the west side."""
    ring, k = divmod(slot, SLOTS_PER_RING)
    reach = SLOT_RADII[ring] if ring < len(SLOT_RADII) else (
        SLOT_RADIUS_OVERFLOW + OVERFLOW_RING_STEP * (ring - len(SLOT_RADII)))
    turn = SLOT_BEARING_FIRST + SLOT_BEARING_STEP * (k // 2)
    return reach, turn if k % 2 == 0 else -turn


def slot_rows(trace: Trace) -> list[dict]:
    """Per bot, the slot it held longest on the ground, against where it actually stood. Longest because
    an erase writes "0", the same text as slot 0."""
    boss = boss_guid(trace)
    held = holder_spans(trace, SLOT_KEY, pull_end(trace))
    if boss is None or not held:
        return []
    spans, _ = phase_spans(trace)
    samples = samples_of(trace)
    seen: dict[tuple[int, int], dict] = {}
    for when, her in samples.between(boss, 0, pull_end(trace)):
        if her[5] <= 0 or phase_at(spans, when) not in GROUNDED:
            continue
        for guid, marks in held.items():
            value = next((value for value, start, stop in marks if start <= when < stop), None)
            mine = samples.row(guid, when, FRESH_MS)
            if value is None or not value.isdigit() or not mine or mine[5] <= 0:
                continue
            row = seen.setdefault((guid, int(value)), {"gaps": [], "bearings": []})
            row["gaps"].append(dist2(mine[1:3], her[1:3]))
            row["bearings"].append(bearing(her, mine[1:3]))
    fails = {guid: sum(1 for value, _, _ in marks if value != "none")
             for guid, marks in holder_spans(trace, SLOTFAIL_KEY).items()}
    best: dict[int, tuple[int, dict]] = {}
    for (guid, slot), row in seen.items():
        if guid not in best or len(row["gaps"]) > len(best[guid][1]["gaps"]):
            best[guid] = (slot, row)
    out = []
    for guid, (slot, row) in best.items():
        reach, turn = slot_spot(slot)
        out.append({"guid": guid, "slot": slot, "radius": reach, "bearing": turn,
                    "gap": statistics.median(row["gaps"]), "turn": statistics.median(row["bearings"]),
                    "fails": fails.get(guid, 0)})
    return sorted(out, key=lambda row: row["slot"])


def anchor_drift(trace: Trace) -> tuple[int, float | None]:
    """How many anchors `onyxia.anchor` named, and their median distance from her on the ground."""
    boss = boss_guid(trace)
    marks = []
    for held, start, stop in latch_spans(trace, ANCHOR_KEY, pull_end(trace)):
        try:
            marks.append((tuple(float(part) for part in held.split(",")[:3]), start, stop))
        except ValueError:
            continue
    if not marks or boss is None:
        return len(marks), None
    spans, _ = phase_spans(trace)
    gaps = []
    for when, her in samples_of(trace).between(boss, 0, pull_end(trace)):
        spot = next((spot for spot, start, stop in marks if start <= when < stop), None)
        if spot and her[5] > 0 and phase_at(spans, when) in GROUNDED:
            gaps.append(dist2(spot, her[1:3]))
    return len(marks), median_or_none(gaps)


def show_layout(trace: Trace) -> None:
    print("LAYOUT")
    if boss_guid(trace) is None:
        print("  Onyxia was never named")
        return
    groups = layout(trace)
    if groups:
        print(f"  on the ground, within {LAYOUT_RANGE:.0f} yd of her; bearing off her facing, front under 90,"
              f" rear past {FLANK_MAX:.0f}")
        print(f"  {'group':7} {'bots':>4} {'samples':>7} {'|bearing|':>9} {'range':>6} {'front':>6} {'flank':>6}"
              f" {'rear':>6}")
        for group in GROUPS:
            row = groups.get(group)
            if not row:
                continue
            turns = [abs(value) for value in row["bearings"]]
            front = sum(1 for value in turns if value < 90.0)
            rear = sum(1 for value in turns if value > FLANK_MAX)
            print(f"  {group:7} {len(row['bots']):4} {len(turns):7} {statistics.median(turns):9.0f}"
                  f" {statistics.median(row['gaps']):6.1f} {pct(front, len(turns)):>6}"
                  f" {pct(len(turns) - front - rear, len(turns)):>6} {pct(rear, len(turns)):>6}")
    else:
        print("  nobody sampled near her on the ground")

    print(f"\n  {'spell':13} {'side':5} {'casts':>5} {'hits':>5}  by group")
    for row in cone_hits(trace):
        hits = sum(row["hits"].values())
        groups_hit = "  ".join(f"{group} {row['hits'][group]}" for group in GROUPS if row["hits"][group])
        print(f"  {row['label']:13} {row['side']:5} {row['casts']:5} {hits:5}  {groups_hit or '-'}")

    slots = slot_rows(trace)
    if slots:
        print(f"\n  {'bot':14} {'slot':>4} {'want yd':>7} {'want deg':>8} {'yd':>6} {'deg':>6} {'fails':>5}")
        for row in slots:
            print(f"  {trace.name(row['guid'])[:14]:14} {row['slot']:4} {row['radius']:7.0f} {row['bearing']:+8.0f}"
                  f" {row['gap']:6.1f} {row['turn']:+6.0f} {row['fails']:5}")
    count, drift = anchor_drift(trace)
    if count:
        print(f"  {ANCHOR_KEY}: {count} anchor(s)"
              + (f", median {drift:.1f} yd from her on the ground" if drift is not None else ""))
    moves = collections.Counter(rec.get("by") for rec in trace.of("move")
                                if rec.get("ok") and rec.get("by") in (GROUND_SLOT, REAR_FLANK))
    if moves:
        print("  accepted moves: " + ", ".join(f"{name} {count}" for name, count in moves.most_common()))


def whelp_rows(trace: Trace) -> list[dict]:
    """Each sampled whelp's first sighting, `cave` when it came off one of her cave summons."""
    summons = sorted(rec["t"] for rec in her_casts(trace, (SUMMON_WHELP,), triggered=True))
    samples = samples_of(trace)
    out = []
    for guid in guids_of_entry(trace, NPC_WHELP):
        found = samples.rows.get(guid)
        if not found:
            continue
        when, row = found[0][0], found[1][0]
        index = bisect.bisect_right(summons, when)
        summoned = index > 0 and when - summons[index - 1] <= CAVE_LAG_MS
        near_cave = min(dist2(row[1:3], cave) for cave in CAVES) <= CAVE_RADIUS
        out.append({"guid": guid, "t": when, "spot": tuple(row[1:4]),
                    "kind": "cave" if summoned and near_cave else "egg"})
    return sorted(out, key=lambda row: row["t"])


def hatch_bursts(trace: Trace, whelps: list[dict]) -> list[dict]:
    """Egg whelps grouped by sighting, with the member nearest any of them in 3D."""
    samples = samples_of(trace)
    roster = roster_guids(trace)
    out: list[dict] = []
    for whelp in (row for row in whelps if row["kind"] == "egg"):
        if not out or whelp["t"] - out[-1]["t"] > HATCH_BURST_MS:
            out.append({"t": whelp["t"], "count": 0, "nearest": None, "gap": None})
        burst = out[-1]
        burst["count"] += 1
        for guid in roster:
            mine = samples.row(guid, whelp["t"], FRESH_MS)
            if not mine or mine[5] <= 0:
                continue
            gap = dist3(mine[1:4], whelp["spot"])
            if burst["gap"] is None or gap < burst["gap"]:
                burst["nearest"], burst["gap"] = guid, gap
    return out


def egg_spots(trace: Trace) -> list[tuple[float, float, float]]:
    return sorted({(round(rec["x"], 1), round(rec["y"], 1), round(rec.get("z", 0.0), 1))
                   for rec in trace.of("haz") if rec.get("sp") == EGG_TRAP and "x" in rec})


def egg_exposure(trace: Trace, eggs) -> dict[int, tuple[int, int]]:
    """Per member, `(ms, first t)` it stood within `ONYXIA_EGG_DANGER` of an egg in 3D."""
    samples = samples_of(trace)
    out = {}
    for guid in roster_guids(trace):
        near = [(when, weight) for when, row, weight in samples.weighted(guid, 0, pull_end(trace))
                if row[5] > 0 and any(dist3(row[1:4], egg) <= EGG_DANGER for egg in eggs)]
        if near:
            out[guid] = (sum(weight for _, weight in near), near[0][0])
    return out


def pit_rows(trace: Trace) -> list[dict]:
    """Per member, time spent alive on the egg pit floor, first entry, the longest stretch, and the
    moves it issued from down there: `refused` counts `nopath` and `blocked`, `by` the action owning most."""
    samples = samples_of(trace)
    moves: dict[int, list[dict]] = collections.defaultdict(list)
    for rec in trace.of("move"):
        moves[rec.get("g")].append(rec)
    out = []
    for guid in roster_guids(trace):
        total = run = longest = 0
        first = None
        for when, row, weight in samples.weighted(guid, 0, pull_end(trace)):
            if row[5] > 0 and row[3] < PIT_Z:
                first = when if first is None else first
                total += weight
                run += weight
                longest = max(longest, run)
            else:
                run = 0
        if not total:
            continue
        issued = []
        for rec in moves.get(guid, ()):
            mine = samples.row(guid, rec["t"], FRESH_MS)
            if rec["t"] >= 0 and mine and mine[5] > 0 and mine[3] < PIT_Z:
                issued.append(rec)
        owners = collections.Counter(rec.get("by") or "?" for rec in issued)
        out.append({"guid": guid, "ms": total, "first": first, "longest": longest, "moves": len(issued),
                    "refused": sum(1 for rec in issued if rec.get("r") in ("nopath", "blocked")),
                    "by": owners.most_common(1)[0][0] if owners else None})
    return sorted(out, key=lambda row: -row["ms"])


def show_eggs(trace: Trace) -> None:
    print("EGGS AND WHELPS")
    if boss_guid(trace) is None:
        print("  Onyxia was never named")
        return
    whelps = whelp_rows(trace)
    named = len(guids_of_entry(trace, NPC_WHELP))
    summons = len(her_casts(trace, (SUMMON_WHELP,), triggered=True))
    lift = liftoff_time(trace)
    cave = [row for row in whelps if row["kind"] == "cave"]
    hatched = [row for row in whelps if row["kind"] == "egg"]
    early = [row for row in hatched if lift is None or row["t"] < lift]
    print(f"  {named} whelp(s) named, {len(whelps)} sampled: {len(cave)} from the caves ({summons} summon"
          f" cast(s)), {len(hatched)} hatched from eggs")
    if hatched:
        when = f"lift-off at {clock(lift)}" if lift is not None else "lift-off, which never came"
        print(f"  hatched before {when}: {len(early)}, after: {len(hatched) - len(early)};"
              f" first at {clock(hatched[0]['t'])}")

    bursts = hatch_bursts(trace, whelps)
    if bursts:
        print("  hatches, with the member nearest in 3D at the sighting:")
        for burst in bursts[:SHOWN * 2]:
            nearest = (f"{trace.name(burst['nearest'])} {burst['gap']:.1f} yd" if burst["nearest"] else "nobody")
            print(f"    {clock(burst['t']):>9} {burst['count']:2}  {nearest}")
        if len(bursts) > SHOWN * 2:
            print(f"    and {len(bursts) - SHOWN * 2} more")
        tally = collections.Counter(burst["nearest"] for burst in bursts
                                    if burst["nearest"] and burst["gap"] <= EGG_DANGER + 2.0)
        if tally:
            print(f"  nearest within {EGG_DANGER + 2.0:.0f} yd at a hatch: "
                  + ", ".join(f"{trace.name(guid)} {count}" for guid, count in tally.most_common()))

    eggs = egg_spots(trace)
    if eggs:
        exposure = egg_exposure(trace, eggs)
        print(f"\n  {len(eggs)} egg(s) from the hazard rows; within {EGG_DANGER:.0f} yd of one in 3D:")
        for guid, (span, first) in sorted(exposure.items(), key=lambda item: -item[1][0]):
            print(f"    {trace.name(guid)[:14]:14} {trace.role(guid):6} {span / 1000:6.1f} s, first {clock(first)}")
        if not exposure:
            print("    nobody")
    else:
        print("\n  no egg hazard rows, so the nearest member at each hatch stands in for who walked onto an egg")

    pits = pit_rows(trace)
    if pits:
        print(f"\n  on the egg pit floor (z under {PIT_Z}):")
        print(f"  {'bot':14} {'role':6} {'total':>6} {'first':>9} {'longest':>7} {'moves':>5} {'refused':>7}"
              "  mostly by")
        for row in pits:
            if row["guid"] in trace.humans:
                flag = "  human"
            else:
                flag = "  stuck" if row["longest"] >= STUCK_MS else ""
            print(f"  {trace.name(row['guid'])[:14]:14} {trace.role(row['guid']):6} {row['ms'] / 1000:6.1f}"
                  f" {clock(row['first']):>9} {row['longest'] / 1000:7.1f} {row['moves']:5} {row['refused']:7}"
                  f"  {(row['by'] or '-')[:24]:24}{flag}".rstrip())
        print(f"  stuck is a bot down there {STUCK_MS / 1000:.0f} s straight or more; refused moves are nopath or"
              " blocked")
    else:
        print("  nobody on the egg pit floor")

    counts = [held for held, _, _ in latch_spans(trace, EGG_KEY) if held.isdigit()]
    if counts:
        print(f"  {EGG_KEY}: {counts[0]} at first, {counts[-1]} at the end")
    guard = sum(1 for rec in trace.of("veto") if rec.get("m") == EGG_GUARD)
    dodgers = dodge_bots(trace).get("egg", 0)
    if guard or dodgers:
        print(f"  {EGG_GUARD} vetoes {guard}; {dodgers} bot(s) dodged an egg")


def visits(trace: Trace) -> list[dict]:
    """Each stay at a breath lane's hover point: waypoint, start, stop."""
    boss = boss_guid(trace)
    if boss is None or not LANES:
        return []
    out: list[dict] = []
    current = None
    for when, row in samples_of(trace).between(boss, 0, pull_end(trace)):
        waypoint = None
        if row[5] > 0 and row[3] > AIR_Z:
            waypoint = next((wp for wp, lane in LANES.items() if dist3(row[1:4], lane["hover"]) <= WAYPOINT_SNAP),
                            None)
        if current and current["waypoint"] == waypoint:
            current["stop"] = when
            continue
        if current and current["stop"] - current["start"] >= MIN_VISIT_MS:
            out.append(current)
        current = {"waypoint": waypoint, "start": when, "stop": when} if waypoint else None
    if current and current["stop"] - current["start"] >= MIN_VISIT_MS:
        out.append(current)
    return out


def airborne(trace: Trace) -> tuple[int, int] | None:
    """From her first air sample to her touch-down, or the end of the pull."""
    spans, _ = phase_spans(trace)
    start = first_start(spans, (AIR, LANDING))
    if start is None:
        return None
    return start, touchdown_time(trace) or pull_end(trace)


def targeting_share(trace: Trace, low: int, high: int) -> dict[int, float]:
    """Per ranged or healer that targeted her at all in `[low, high)`, the share of its living samples
    on her."""
    boss = boss_guid(trace)
    out = {}
    for guid in roster_guids(trace):
        if trace.role(guid) not in ("ranged", "heal"):
            continue
        alive = [row for _, row in samples_of(trace).between(guid, low, high - 1) if row[5] > 0 and len(row) > 7]
        on = sum(1 for row in alive if row[7] == boss)
        if on:
            out[guid] = on / len(alive)
    return out


def reach_shares(trace: Trace, low: int, high: int) -> dict[str, tuple[int, int, int]]:
    """Over her samples in range, per `ranged` and `heal`: member samples, in 3D spell range, on her."""
    boss = boss_guid(trace)
    samples = samples_of(trace)
    roster = roster_guids(trace)
    out = {"ranged": [0, 0, 0], "heal": [0, 0, 0]}
    for when, her in samples.between(boss, low, high):
        for guid in roster:
            side = "heal" if trace.role(guid) == "heal" else "ranged" if trace.role(guid) == "ranged" else None
            mine = samples.row(guid, when, FRESH_MS) if side else None
            if not mine or mine[5] <= 0:
                continue
            out[side][0] += 1
            out[side][1] += dist3(mine[1:4], her[1:4]) <= AIR_SPELL_RANGE
            out[side][2] += len(mine) > 7 and mine[7] == boss
    return {side: tuple(values) for side, values in out.items()}


def fireballs(trace: Trace) -> list[dict]:
    """Each Fireball she started, with the hits it landed."""
    casts = sorted(her_casts(trace, FIREBALL), key=lambda rec: rec["t"])
    ready = [rec["t"] + (rec.get("ct") or 0) for rec in casts]
    out = [{"t": rec["t"], "target": rec.get("tgt"), "victims": [], "damage": 0} for rec in casts]
    for rec in trace.of("dmg"):
        if rec.get("sp") not in FIREBALL:
            continue
        # bisect needs ready times sorted, which holds while every Fireball has the same cast time
        index = bisect.bisect_right(ready, rec["t"]) - 1
        if index < 0 or rec["t"] - ready[index] > FIREBALL_FLIGHT_MS:
            continue
        out[index]["victims"].append(rec.get("d"))
        out[index]["damage"] += rec.get("a", 0)
    return out


def breath_spells(trace: Trace) -> set[int]:
    """Every lane's cast id, plus the fire hops the trace names `Breath`."""
    return set(LANE_SPELLS) | {spell for spell, name in trace.spells.items() if name == "Breath"}


def breaths(trace: Trace) -> list[dict]:
    """Each Deep Breath she cast, its waypoint and the hits inside its live window."""
    spells = breath_spells(trace)
    hits = [rec for rec in trace.of("dmg") if rec.get("sp") in spells]
    out = []
    for rec in her_casts(trace, LANE_SPELLS):
        mine = [hit for hit in hits if rec["t"] <= hit["t"] <= rec["t"] + BREATH_LIVE_MS + LAND_SLACK_MS]
        out.append({"t": rec["t"], "waypoint": LANE_SPELLS[rec["sp"]], "victims": [hit.get("d") for hit in mine],
                    "damage": sum(hit.get("a", 0) for hit in mine)})
    return out


def novas(trace: Trace) -> tuple[list[dict], list[dict]]:
    """Each Blast Nova cast with its outcome, and the hit bursts no cast row explains."""
    hits = [rec for rec in trace.of("dmg") if rec.get("sp") == BLAST_NOVA]
    used: set[int] = set()
    kicks = [rec for rec in trace.of("cast") if rec.get("sp") in INTERRUPTS]
    casts = []
    for rec in trace.of("cast"):
        if rec.get("sp") != BLAST_NOVA or rec.get("tr"):
            continue
        guard, start = rec.get("s"), rec["t"]
        stop = start + (rec.get("ct") or 5000)
        landed = [index for index, hit in enumerate(hits)
                  if hit.get("s") == guard and start <= hit["t"] <= stop + LAND_SLACK_MS]
        used.update(landed)
        inside = [(kick["t"], kick.get("s"), kick.get("sp")) for kick in kicks
                  if kick.get("tgt") == guard and start <= kick["t"] < stop]
        casts.append({"t": start, "guard": guard, "victims": [hits[index].get("d") for index in landed],
                      "kicks": inside, "outcome": "landed" if landed else "kicked" if inside else "no hit"})
    stray: list[dict] = []
    for index, hit in enumerate(hits):
        if index in used:
            continue
        if stray and stray[-1]["guard"] == hit.get("s") and hit["t"] - stray[-1]["t"] <= LAND_SLACK_MS:
            stray[-1]["victims"].append(hit.get("d"))
        else:
            stray.append({"t": hit["t"], "guard": hit.get("s"), "victims": [hit.get("d")]})
    return casts, stray


def nova_kills(trace: Trace) -> list[dict]:
    """Combat deaths a Blast Nova hit landed shortly before."""
    hits = [rec for rec in trace.of("dmg") if rec.get("sp") == BLAST_NOVA]
    return [death for death in combat_deaths(trace)
            if any(hit.get("d") == death.get("g") and 0 <= death["t"] - hit["t"] <= NOVA_KILL_MS for hit in hits)]


def dodge_bots(trace: Trace) -> collections.Counter:
    """Per `onyxia.dodge` reason, how many bots ever dodged for it."""
    reasons: dict[str, set] = collections.defaultdict(set)
    for rec in notes(trace, DODGE_KEY):
        for reason in str(rec.get("txt", "")).split("+"):
            if reason and reason != "none":
                reasons[reason].add(rec.get("g"))
    return collections.Counter({reason: len(bots) for reason, bots in reasons.items()})


def show_air(trace: Trace) -> None:
    print("AIR PHASE")
    boss = boss_guid(trace)
    if boss is None:
        print("  Onyxia was never named")
        return
    stays = visits(trace)
    balls = fireballs(trace)
    deep = breaths(trace)
    if not stays and not balls:
        print("  she never flew")
        return

    print(f"  per waypoint visit; reach is ranged or healers within {AIR_SPELL_RANGE} yd of her in 3D, on is"
          " those targeting her; br hits is the Deep Breath cast there")
    print(f"  {'at':>9} {'wp':>2} {'len':>5} {'fb':>3} {'fb hits':>7} {'br hits':>7}"
          f" {'ranged reach/on':>15} {'heal reach/on':>13}")
    for stay in stays:
        start, stop = stay["start"], stay["stop"]
        mine = [ball for ball in balls if start <= ball["t"] <= stop]
        breath = [row for row in deep if start <= row["t"] <= stop]
        shares = reach_shares(trace, start, stop)
        ranged, heal = shares["ranged"], shares["heal"]
        print(f"  {clock(start):>9} {stay['waypoint']:2} {(stop - start) / 1000:5.1f} {len(mine):3}"
              f" {sum(len(ball['victims']) for ball in mine):7}"
              f" {len(breath[0]['victims']) if breath else '-':>7}"
              f" {pct(ranged[1], ranged[0]):>7}/{pct(ranged[2], ranged[0]):<7}"
              f" {pct(heal[1], heal[0]):>6}/{pct(heal[2], heal[0])}")
    flight = airborne(trace)
    if flight:
        air_from, air_to = flight
        shares = reach_shares(trace, air_from, air_to)
        on = targeting_share(trace, air_from, air_to)
        steady = [guid for guid, share in on.items() if share >= TARGET_SHARE]
        print(f"  airborne {clock(air_from)} to {clock(air_to)}: ranged in reach"
              f" {pct(shares['ranged'][1], shares['ranged'][0])},"
              f" on her {pct(shares['ranged'][2], shares['ranged'][0])};"
              f" healers in reach {pct(shares['heal'][1], shares['heal'][0])}")
        print(f"  {len(on)} ranged or healer(s) ever targeted her up there, {len(steady)} for"
              f" {TARGET_SHARE * 100:.0f}% of their samples or more"
              + (": " + ", ".join(trace.name(guid) for guid in steady) if steady else ""))
        health = [row[5] for _, row in samples_of(trace).between(boss, air_from, air_to) if row[5] > 0]
        if health:
            print(f"  her health {health[0]:.2f}% -> {health[-1]:.2f}% while airborne")

    if balls:
        counts = [len(ball["victims"]) for ball in balls]
        print(f"\n  Fireball: {len(balls)} cast(s), {sum(counts)} hit(s), {statistics.mean(counts):.1f} a cast,"
              f" most {max(counts)}")
        print(f"  {'at':>9} {'target':14} {'role':6} {'hits':>4} {'damage':>8}")
        for ball in balls:
            print(f"  {clock(ball['t']):>9} {trace.name(ball['target'])[:14]:14} {trace.role(ball['target']):6}"
                  f" {len(ball['victims']):4} {ball['damage']:8,}")

    if deep:
        print(f"\n  Deep Breath: {len(deep)} cast(s), {sum(len(row['victims']) for row in deep)} hit(s)")
        for row in deep:
            victims = ", ".join(f"{trace.name(guid)}{f' x{count}' if count > 1 else ''}"
                                for guid, count in collections.Counter(row["victims"]).items()) or "nobody"
            print(f"    {clock(row['t']):>9} wp {row['waypoint']}  {victims}")
    lanes = sum(1 for rec in trace.of("haz") if rec.get("shape") == "lane" and rec.get("sp") in LANE_SPELLS)
    if lanes:
        print(f"  {lanes} breath lane hazard row(s)")

    casts, stray = novas(trace)
    if casts or stray:
        outcome = collections.Counter(row["outcome"] for row in casts)
        print(f"\n  Blast Nova from {len(guids_of_entry(trace, NPC_LAIR_GUARD))} Lair Guard(s) named:"
              f" {len(casts)} cast(s) seen, {outcome['landed']} landed, {outcome['kicked']} kicked,"
              f" {outcome['no hit']} neither" + (f"; {len(stray)} more landed with no cast row" if stray else ""))
        for row in sorted(casts + stray, key=lambda row: row["t"]):
            if "outcome" not in row:
                print(f"    {clock(row['t']):>9} landed  {len(row['victims']):3} hit(s)  no cast row")
                continue
            kicked = ", ".join(f"{trace.name(caster)} {trace.spells.get(spell) or spell}"
                               for _, caster, spell in row["kicks"]) or "-"
            print(f"    {clock(row['t']):>9} {row['outcome']:7} {len(row['victims']):3} hit(s)  kicked by {kicked}")
        kills = nova_kills(trace)
        if kills:
            print("  killed: " + ", ".join(f"{trace.name(death.get('g'))} {clock(death['t'])}" for death in kills))

    roles = holder_spans(trace, P2ROLE_KEY)
    if roles:
        tally = collections.Counter(spans[-1][0] for spans in roles.values() if spans)
        print("  " + P2ROLE_KEY + ": " + ", ".join(f"{name} {count}" for name, count in tally.most_common()))
    spots = holder_spans(trace, P2SPOT_KEY)
    if spots:
        stranded = sum(1 for spans in spots.values() if any(value == "none" for value, _, _ in spans))
        print(f"  {P2SPOT_KEY}: {len(spots)} bot(s), {stranded} ever had none")
    dodges = dodge_bots(trace)
    if dodges:
        print(f"  {DODGE_KEY}, bots per reason: "
              + ", ".join(f"{reason} {count}" for reason, count in dodges.most_common()))
    moves = collections.Counter(rec.get("by") for rec in trace.of("move")
                                if rec.get("ok") and rec.get("by") in (AIR_SPOT, DODGE))
    if moves:
        print("  accepted moves: " + ", ".join(f"{name} {count}" for name, count in moves.most_common()))


def roars(trace: Trace) -> list[dict]:
    """Each Bellowing Roar: who it feared, Eruption's hits after it, and who stood by a fissure."""
    roster = roster_guids(trace)
    samples = samples_of(trace)
    fears = [rec for rec in trace.of("aura") if rec.get("sp") == BELLOWING_ROAR and not rec.get("r")
             and rec.get("d") in roster]
    eruptions = [rec for rec in trace.of("dmg") if rec.get("sp") in ERUPTION]
    out = []
    for rec in her_casts(trace, (BELLOWING_ROAR,)):
        near = set()
        for guid in roster:
            mine = samples.row(guid, rec["t"], FRESH_MS)
            if mine and mine[5] > 0 and any(dist2(mine[1:3], spot) <= FISSURE_CLEARANCE for spot in FISSURES):
                near.add(guid)
        out.append({
            "t": rec["t"],
            "feared": sorted({fear.get("d") for fear in fears if rec["t"] <= fear["t"] <= rec["t"] + FEAR_MS}),
            "erupted": [hit.get("d") for hit in eruptions if rec["t"] <= hit["t"] <= rec["t"] + ERUPTION_MS],
            "near": near,
        })
    return out


def after_touchdown(trace: Trace) -> dict | None:
    """Her victims from touch-down: the first, when the main tank first held her, and how far he
    stood from `ONYXIA_P3_PICKUP` as she landed."""
    touch = touchdown_time(trace)
    if touch is None:
        return None
    tank = main_tank(trace)
    changes = [(when, victim) for when, victim in victim_changes(trace) if when >= touch and victim]
    mine = samples_of(trace).row(tank, touch, FRESH_MS) if tank else None
    return {
        "touch": touch,
        "pickup_gap": dist2(mine[1:3], P3_PICKUP) if mine and mine[5] > 0 else None,
        "first": changes[0] if changes else None,
        "tank_at": next((when for when, victim in changes if victim == tank), None),
        "losses": [loss for loss in aggro_losses(trace, tank) if loss["start"] >= touch],
    }


def show_p3(trace: Trace) -> None:
    print("PHASE 3")
    boss = boss_guid(trace)
    if boss is None:
        print("  Onyxia was never named")
        return
    landed = after_touchdown(trace)
    roared = roars(trace)
    if landed is None and not roared:
        health = [row[5] for _, row in samples_of(trace).between(boss, 0, pull_end(trace)) if row[5] > 0]
        floor = f", her health floor {min(health):.2f}%" if health else ""
        print(f"  never reached phase 3{floor}")
        return

    if landed:
        tank = main_tank(trace)
        first = landed["first"]
        who = (f"{trace.name(first[1])} ({trace.role(first[1])}) at {clock(first[0])}" if first else "nobody")
        held = (f"{(landed['tank_at'] - landed['touch']) / 1000:+.1f} s" if landed["tank_at"] is not None
                else "never")
        print(f"  touched down {clock(landed['touch'])}; first victim {who}; {trace.name(tank)} held her {held}")
        if landed["pickup_gap"] is not None:
            print(f"  {trace.name(tank)} stood {landed['pickup_gap']:.1f} yd from ONYXIA_P3_PICKUP as she landed")
        for loss in landed["losses"]:
            print(f"    lost to {trace.name(loss['guid'])} ({trace.role(loss['guid'])}) at {clock(loss['start'])}"
                  f" for {(loss['stop'] - loss['start']) / 1000:.1f} s")

    if roared:
        print(f"\n  {'roar':>9} {'feared':>6} {'eruption':>8} {'by fissure':>10}  feared")
        for row in roared:
            names = ", ".join(trace.name(guid) for guid in row["feared"][:8])
            more = f" and {len(row['feared']) - 8} more" if len(row["feared"]) > 8 else ""
            print(f"  {clock(row['t']):>9} {len(row['feared']):6} {len(row['erupted']):8} {len(row['near']):10}"
                  f"  {names or '-'}{more}")
        hits = collections.Counter(group_of(trace, guid) for row in roared for guid in row["erupted"])
        if hits:
            print("  Eruption hits by group: "
                  + "  ".join(f"{group} {hits[group]}" for group in GROUPS if hits[group]))
        print(f"  by fissure is members within {FISSURE_CLEARANCE} yd of a fissure at the roar")
    wards = collections.Counter(rec.get("sp") for rec in trace.of("cast")
                                if rec.get("sp") in (FEAR_WARD, TREMOR_TOTEM) and not rec.get("tr"))
    if wards:
        print("  anti-fear casts: " + ", ".join(f"{trace.spell(spell)} x{count}" for spell, count in wards.items()))
    acted = sum(1 for rec in trace.of("act") if rec.get("a") == ANTI_FEAR and rec.get("vd") == "OK")
    guard = sum(1 for rec in trace.of("veto") if rec.get("m") == TOTEM_GUARD)
    if acted or guard:
        print(f"  {ANTI_FEAR} OK {acted}, {TOTEM_GUARD} vetoes {guard}")


def show_banner(trace: Trace) -> None:
    end = pull_end(trace)
    outcome = trace.of("end")[-1].get("out", "?") if trace.of("end") else "no end record"
    encounter = encounter_of(trace)
    print(f"{encounter} {size_label(trace)}  {outcome} at {clock(end)}  {len(combat_deaths(trace))} death(s)")

    # the probe check matches keys to the encounter, so on another boss it would pass on nothing
    if encounter != ENCOUNTER:
        print("  not an Onyxia pull, every section below reads empty")
        return

    humans = sorted(trace.humans & roster_guids(trace), key=trace.name)
    if humans:
        print("humans: " + ", ".join(f"{trace.name(guid)} ({trace.role(guid)})" for guid in humans))
    gone = missing_probes(trace)
    if not gone:
        print("all onyxia.* probes present")
        return
    if not any(key.startswith(ENCOUNTER + ".") for key in emitted_keys(trace)):
        print(f"probes absent from this trace: all {len(gone)} onyxia.* keys, so every section reads raw records")
        return
    print(f"probes absent from this trace: {', '.join(gone)}")


SECTIONS = (
    ("phase", "phase spans, from onyxia.phase or her health and height", show_phase),
    ("tank", "her victims, aggro losses, the tank spot, Wing Buffet drift, tank branches", show_tank),
    ("threat", "first hit on her per bot against the pull and the tank's hold, lust", show_threat),
    ("layout", "bearing and range off her facing by role, cone and tail hits by role", show_layout),
    ("eggs", "egg whelps against cave whelps, who stood at the eggs, the egg pit", show_eggs),
    ("air", "each waypoint visit, Fireball victims, Deep Breath, Blast Nova", show_air),
    ("p3", "Bellowing Roar fears, Eruption hits, aggro after touch-down", show_p3),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
