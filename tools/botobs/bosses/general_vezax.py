#!/usr/bin/env python3
"""Score a General Vezax pull from a RaidObs trace: the mark, Shadow Crash, fields, mana, vapors."""
from __future__ import annotations

import bisect
import collections
import math
import pathlib
import statistics
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.geometry import anchor, dist2, first_seen, frames, guids_of_entry, radius  # noqa: E402
from raidobs.probes import emitted_keys, silent_keys  # noqa: E402
from raidobs.space import threat_share  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, notes, roster_guids  # noqa: E402

NPC_VEZAX = 33271
NPC_SARONITE_VAPORS = 33488
NPC_SARONITE_ANIMUS = 33524

SPELL_MARK_OF_THE_FACELESS = 63276
SPELL_MARK_LEECH = 63278
SPELL_SHADOW_CRASH = 62660
SPELL_SHADOW_CRASH_IMPACT = 62659
SPELL_SEARING_FLAMES = 62661
SPELL_SURGE_OF_DARKNESS = 62662
SPELL_SUMMON_VAPORS = 63081
SPELL_FIELD = 63277
CRASH_SPELLS = (SPELL_SHADOW_CRASH, SPELL_SHADOW_CRASH_IMPACT, SPELL_FIELD)
# The half of the field carrying the -70% mana cost, linked off 63277 and not always behind it.
SPELL_FIELD_COST = 65269
SPELL_LIFE_TAP = 57946
SPELL_LIFE_TAP_GLYPH = 63321
SPELL_MISDIRECTION = 34477
SPELL_TRICKS = 57934
REDIRECT_SPELLS = (SPELL_MISDIRECTION, SPELL_TRICKS)
# Heroism, Bloodlust
LUST_SPELLS = (32182, 2825)
SPELL_BEACON = 53563
BEACON_COPIES = (53652, 53653, 53654)
SPELL_SACRED_SHIELD = 53601
TANK_BUFFS_HOLD = "vezax tank buffs on the holder"

DODGE = "vezax shadow crash dodge action"
SOAK = "vezax shadow crash soak action"
HOLD = "vezax hold cast outside field multiplier"
GUARD = "vezax target guard multiplier"
LIFE_TAP_VETO = "vezax suppress life tap multiplier"
ANIMUS_SWITCH = "vezax saronite animus action"

# Read from the source so a retune shows up here without a second edit.
CAMP_RADIUS = radius("ULDUAR_VEZAX_CAMP_RADIUS")
SOAK_MAX_TRAVEL = radius("ULDUAR_VEZAX_SHADOW_CRASH_SOAK_MAX_TRAVEL")
IMPACT_RADIUS = radius("ULDUAR_VEZAX_SHADOW_CRASH_IMPACT_RADIUS")
WALK_CLEARANCE = radius("ULDUAR_VEZAX_SHADOW_CRASH_WALK_CLEARANCE")
LEECH_RADIUS = radius("ULDUAR_VEZAX_MARK_LEECH_RADIUS")
ARENA_RADIUS = radius("ULDUAR_VEZAX_ARENA_RADIUS")
ARC_ORIENTATION = radius("ULDUAR_VEZAX_ARC_ORIENTATION")
MARK_SIDE_OFFSET = radius("ULDUAR_VEZAX_MARK_SIDE_OFFSET")
MARK_SPOT_TOLERANCE = radius("ULDUAR_VEZAX_MARK_SPOT_TOLERANCE")
ANCHOR = anchor("ULDUAR_VEZAX_ANCHOR")

# Engine figures, not source constants: the mark's 15 yd of GetDistance and reach spell's 28.5, each
# plus his combat reach and the player's 1.5. Traces from before core update 2026_11_06_01 ran on a
# reach of 8.0.
COMBAT_REACH = 12.6
BAND_FLOOR = 15.0 + COMBAT_REACH + 1.5
BAND_CEILING = 28.5 + COMBAT_REACH + 1.5
SURGE_MS = 10000
SURGE_SCALE = 1.35
SURGE_FLOOR = 15.0 + COMBAT_REACH * SURGE_SCALE + 1.5
ANIMUS_COMBAT_REACH = 7.0
ANIMUS_REACH_CEILING = 28.5 + ANIMUS_COMBAT_REACH + 1.5
REACH_SPELL = "reach spell"
RUN_SPEED = 7.0

# ULDUAR_VEZAX_GROUP_SLOTS, a product of two uint8 constants the float parser cannot read: slots
# [0,10) are group L and [10,20) group R.
GROUP_SLOTS = 10

MARK_WINDOW_MS = 10000
# the last leech tick lands ~1.6 s after the aura drops
LEECH_SLACK_MS = 2000
EARLY_MS = 4000
NEAREST_FROM_MS = 2000
# one snapshot gap longer than this is a hole in the trace, not time spent anywhere
MAX_STEP_MS = 2000
# Past the 30 s redirect cooldown, so one spent before the spawn still shows
REDIRECT_LOOKBACK_MS = 45000
ANIMUS_VICTIMS_SHOWN = 12
# Under four fifths of the sampled roster alive the raid is wiping, and cast and mana figures past it
# measure the wipe, not the fight.
COLLAPSE_ALIVE_SHARE = 0.8
# A dead creature leaves the snapshots at a sliver of health; one still above this went missing instead.
KILL_LAST_HEALTH_PCT = 5.0


def pull_end(trace: Trace) -> int:
    ends = trace.of("end")
    if ends:
        return ends[-1]["t"]
    return trace.records[-1].get("t", 0) if trace.records else 0


def boss_guid(trace: Trace) -> int | None:
    guids = sorted(guids_of_entry(trace, NPC_VEZAX))
    return guids[0] if guids else None


def slots(trace: Trace) -> dict[int, int]:
    """First `vezax.slot` per bot, the assignment it held for the pull. First rather than latest: an
    erase is written as "0", which is also a real slot."""
    out: dict[int, int] = {}
    for rec in notes(trace, "vezax.slot"):
        try:
            out.setdefault(rec["g"], int(rec.get("txt", "")))
        except ValueError:
            continue
    return out


def blocks(trace: Trace) -> dict[int, str]:
    """Latest `vezax.block` per bot, else its slot's group: the block is only written inside the gate,
    and `vezax.slot` goes silent on some older traces."""
    out = {guid: ("R" if slot >= GROUP_SLOTS else "L") for guid, slot in slots(trace).items()}
    out.update({rec["g"]: str(rec.get("txt", "")) for rec in notes(trace, "vezax.block")})
    return out


def camp(trace: Trace) -> set[int]:
    return {guid for guid, block in blocks(trace).items() if block in ("L", "R")}


def latch_spans_for(trace: Trace, key: str, value: str) -> dict[int, list[tuple[int, int]]]:
    """Per bot, where a per-bot latch held a value. probes.latch_spans pools every bot's rows."""
    end = pull_end(trace)
    marks: dict[int, list[tuple[int, str]]] = collections.defaultdict(list)
    for rec in notes(trace, key):
        marks[rec["g"]].append((rec["t"], str(rec.get("txt", ""))))

    out: dict[int, list[tuple[int, int]]] = collections.defaultdict(list)
    for guid, rows in marks.items():
        for index, (when, held) in enumerate(rows):
            stop = rows[index + 1][0] if index + 1 < len(rows) else end
            if held == value:
                out[guid].append((when, stop))
    return out


def formation_spans(trace: Trace) -> dict[int, list[tuple[int, int]]]:
    out: dict[int, list[tuple[int, int]]] = collections.defaultdict(list)
    for value in ("on", "kept"):
        for guid, spans in latch_spans_for(trace, "vezax.formation", value).items():
            out[guid].extend(spans)
    for spans in out.values():
        spans.sort()
    return out


def aura_spans(trace: Trace, spell: int) -> dict[int, list[tuple[int, int]]]:
    """Per target, apply to remove. An aura still up when the file ends runs to the end."""
    end = pull_end(trace)
    opened: dict[int, int] = {}
    out: dict[int, list[tuple[int, int]]] = collections.defaultdict(list)
    for rec in trace.of("aura"):
        if rec.get("sp") != spell:
            continue
        guid = rec.get("d", 0)
        if rec.get("r"):
            if guid in opened:
                out[guid].append((opened.pop(guid), rec["t"]))
        elif guid not in opened:
            opened[guid] = rec["t"]
    for guid, start in opened.items():
        out[guid].append((start, end))
    return out


def inside(spans, when: int) -> bool:
    return any(start <= when < stop for start, stop in spans)


def covered(spans, low: int, high: int) -> int:
    return sum(max(0, min(stop, high) - max(start, low)) for start, stop in spans)


class Samples:
    """Every snapshot row by guid, plus the fields each snapshot swept, so a view can ask for a
    position, health or mana at a time without rescanning."""

    def __init__(self, trace: Trace):
        self.stamps: list[int] = []
        self.fields: list[list[tuple[float, float, float]]] = []
        self.rows: dict[int, tuple[list[int], list[list]]] = {}
        for snap in frames(trace):
            when = snap["t"]
            self.stamps.append(when)
            self.fields.append([(hz[1], hz[2], hz[4]) for hz in snap.get("hz", [])
                                if len(hz) >= 5 and hz[0] == SPELL_FIELD])
            for row in snap.get("u", []):
                stamps, rows = self.rows.setdefault(row[0], ([], []))
                stamps.append(when)
                rows.append(row)

    def row(self, guid: int, when: int):
        """The guid's last row at or before `when`, or None."""
        found = self.rows.get(guid)
        if not found:
            return None
        index = bisect.bisect_right(found[0], when)
        return found[1][index - 1] if index else None

    def between(self, guid: int, low: int, high: int) -> list[list]:
        found = self.rows.get(guid)
        if not found:
            return []
        start = bisect.bisect_left(found[0], low)
        stop = bisect.bisect_right(found[0], high)
        return found[1][start:stop]

    def fields_at(self, when: int) -> list[tuple[float, float, float]]:
        index = bisect.bisect_right(self.stamps, when)
        return self.fields[index - 1] if index else []

    def steps(self, low: int = 0, high: int = 1 << 62):
        """`(index, t, weight)` per snapshot in range, weighted by the gap to the next one."""
        for index, when in enumerate(self.stamps):
            if when < low or when >= high:
                continue
            nxt = self.stamps[index + 1] if index + 1 < len(self.stamps) else when
            yield index, when, min(nxt - when, MAX_STEP_MS)


def pending_impacts(trace: Trace) -> list[tuple[int, int, float, float]]:
    """`(cast, impact, x, y)` per Shadow Crash, from the in-flight hazard row."""
    return [(rec["t"], rec["t"] + rec.get("ttl", 0), rec["x"], rec["y"])
            for rec in trace.of("haz") if rec.get("sp") == SPELL_SHADOW_CRASH_IMPACT]


def field_in_reach(samples: Samples, impacts, spot, when: int) -> bool:
    """A live field within soak travel that no missile is about to land on, which is the test the
    soak itself applies."""
    for fx, fy, _ in samples.fields_at(when):
        if dist2(spot, (fx, fy)) > SOAK_MAX_TRAVEL:
            continue
        buried = any(cast <= when < impact and dist2((fx, fy), (ix, iy)) <= IMPACT_RADIUS
                     for cast, impact, ix, iy in impacts)
        if not buried:
            return True
    return False


def in_any_field(samples: Samples, spot, when: int) -> bool:
    return any(dist2(spot, (fx, fy)) <= fr for fx, fy, fr in samples.fields_at(when))


def missing_probes(trace: Trace) -> list[str]:
    return [key for key, _, _ in silent_keys(emitted_keys(trace), encounter_of(trace))]


def show_banner(trace: Trace) -> None:
    end = pull_end(trace)
    outcome = trace.of("end")[-1].get("out", "?") if trace.of("end") else "no end record"
    encounter = encounter_of(trace)
    print(f"{encounter}  {outcome} at {clock(end)}  {len(combat_deaths(trace))} death(s)")

    # the probe check matches keys to the encounter, so on another boss it would pass on nothing
    if encounter != "vezax":
        print("  not a Vezax pull, every section below reads empty")
        return

    gone = missing_probes(trace)
    if not gone:
        print("all vezax.* probes present")
        return
    print(f"probes absent from this trace: {', '.join(gone)}")
    if "vezax.slot" in gone and "vezax.block" not in gone:
        print("  vezax.slot silent with vezax.block present: the slot state carried over from an")
        print("  earlier pull, so nothing was assigned in this one")


def searing_flames(trace: Trace, boss: int) -> tuple[list[int], list[int], list[dict]]:
    """Every 62661 cast, the ones that did damage, and every hit. A kicked cast leaves no damage row."""
    hits = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_SEARING_FLAMES]
    starts = sorted(rec["t"] for rec in trace.of("cast")
                    if rec.get("s") == boss and rec.get("sp") == SPELL_SEARING_FLAMES)
    landed = sorted({starts[bisect.bisect_right(starts, rec["t"]) - 1]
                     for rec in hits if starts and rec["t"] >= starts[0]})
    return starts, landed, hits


def show_boss(trace: Trace) -> None:
    print("BOSS")
    boss = boss_guid(trace)
    if boss is None:
        print("  General Vezax was never sampled")
        return

    casts = collections.Counter(rec.get("sp") for rec in trace.of("cast") if rec.get("s") == boss)
    for spell, count in casts.most_common():
        print(f"  {count:4}  {trace.spell(spell)}")

    starts, landed, hits = searing_flames(trace, boss)
    print(f"\n  Searing Flames landed {len(landed)} of {len(starts)} cast(s) for"
          f" {sum(r.get('a', 0) for r in hits):,} over {len(hits)} hit(s)")
    if landed and len(landed) < len(starts):
        print(f"  landed at {', '.join(clock(when) for when in landed)};"
              f" the other {len(starts) - len(landed)} did no damage")

    held = sorted({trace.name(rec["g"]) for rec in notes(trace, "vezax.interrupter")
                   if rec.get("txt") == "1"})
    print(f"  ever held the interrupt: {', '.join(held) if held else 'nobody'}")

    samples = Samples(trace)
    health = [row[5] for row in samples.between(boss, 0, pull_end(trace))]
    if health:
        print(f"  boss health floor {min(health):.2f}%")


def boss_health_rate(samples: Samples, boss: int, low: int, high: int) -> float | None:
    """Health percent per second over a span, positive when he gained."""
    first, last = samples.row(boss, low), samples.row(boss, high)
    if not first or not last or high <= low:
        return None
    return (last[5] - first[5]) / ((high - low) / 1000.0)


def far_pool_minimum(trace: Trace) -> int:
    """How many living players past BAND_FLOOR the core wants before it marks one of them; short of that
    it marks anyone nearer, tanks and melee included. Difficulty 1 and 3 are the 25-man ones."""
    return 9 if trace.header.get("diff") in (1, 3) else 4


def far_at(samples: Samples, roster, boss: int, when: int, floor: float = BAND_FLOOR) -> int | None:
    """Living roster members past the floor from him, measured 3D like his GetDistance."""
    centre = samples.row(boss, when)
    if not centre:
        return None
    rows = (samples.row(guid, when) for guid in roster)
    return sum(1 for row in rows if row and row[5] > 0 and math.dist(row[1:4], centre[1:4]) > floor)


def surge_starts(trace: Trace, boss: int | None) -> list[int]:
    return sorted(rec["t"] + rec.get("ct", 0) for rec in trace.of("cast")
                  if boss is not None and rec.get("s") == boss and rec.get("sp") == SPELL_SURGE_OF_DARKNESS)


def in_surge(starts: list[int], when: int) -> bool:
    return any(start <= when <= start + SURGE_MS for start in starts)


def mark_windows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    samples = samples or Samples(trace)
    roster = roster_guids(trace)
    boss = boss_guid(trace)
    block = blocks(trace)
    marks = notes(trace, "vezax.mark")
    dodges = [rec for rec in trace.of("move") if rec.get("by") == DODGE]
    leech = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_MARK_LEECH]
    surges = surge_starts(trace, boss)

    windows = []
    for guid, spans in sorted(aura_spans(trace, SPELL_MARK_OF_THE_FACELESS).items()):
        for start, stop in spans:
            stop = min(stop, start + MARK_WINDOW_MS)
            surge = in_surge(surges, start)
            ticks = [rec for rec in leech if start <= rec["t"] <= stop + LEECH_SLACK_MS]
            branch = [str(rec.get("txt")) for rec in marks
                      if rec.get("g") == guid and start <= rec["t"] <= stop]

            nearest = []
            for _, when, _ in samples.steps(start + NEAREST_FROM_MS, stop):
                mine = samples.row(guid, when)
                if not mine:
                    continue
                others = (samples.row(ally, when) for ally in roster if ally != guid)
                gaps = [dist2(mine[1:3], other[1:3]) for other in others if other and other[5] > 0]
                if gaps:
                    nearest.append(min(gaps))

            windows.append({
                "start": start,
                "stop": stop,
                "guid": guid,
                "block": block.get(guid, "-"),
                "branch": branch[-1] if branch else None,
                # The aura lands ~0.2 s after his pick, so this can be one off at the edge.
                "far": far_at(samples, roster, boss, start, SURGE_FLOOR if surge else BAND_FLOOR) if boss else None,
                "surge": surge,
                "ticks": len(ticks),
                "damage": sum(rec.get("a", 0) for rec in ticks),
                "early": sum(1 for rec in ticks if rec["t"] - start < EARLY_MS),
                "victims": collections.Counter(trace.role(rec.get("d")) for rec in ticks),
                "nearest_min": min(nearest) if nearest else None,
                "nearest_median": statistics.median(nearest) if nearest else None,
                "dodgers": len({rec["g"] for rec in dodges if start <= rec["t"] <= stop}),
                "rate": boss_health_rate(samples, boss, start, stop) if boss else None,
            })
    return sorted(windows, key=lambda window: window["start"])


def show_mark(trace: Trace) -> None:
    print("MARK OF THE FACELESS")
    samples = Samples(trace)
    windows = mark_windows(trace, samples)
    if not windows:
        print("  nobody was marked")
        return

    need = far_pool_minimum(trace)
    print(f"  {'at':>9} {'marked':14} {'role':6} {'blk':4} {'branch':9} {'far':>5} {'surge':>5} {'ticks':>5}"
          f" {'damage':>9} {'0-4s':>5} {'near min/med':>13} {'dodgers':>7} {'boss %/s':>8}")
    for window in windows:
        near = ("-" if window["nearest_min"] is None
                else f"{window['nearest_min']:.1f}/{window['nearest_median']:.1f}")
        rate = "-" if window["rate"] is None else f"{window['rate']:+.3f}"
        far = "-" if window["far"] is None else f"{window['far']}/{need}"
        surge = "yes" if window["surge"] else "-"
        print(f"  {clock(window['start']):>9} {trace.name(window['guid'])[:14]:14}"
              f" {trace.role(window['guid']):6} {window['block']:4} {window['branch'] or 'unchanged':9}"
              f" {far:>5} {surge:>5} {window['ticks']:5} {window['damage']:9,} {window['early']:5} {near:>13}"
              f" {window['dodgers']:7} {rate:>8}")

    ticks = sum(window["ticks"] for window in windows)
    damage = sum(window["damage"] for window in windows)
    early = sum(window["early"] for window in windows)
    victims = sum((window["victims"] for window in windows), collections.Counter())
    print(f"\n  {damage:,} leech over {ticks} tick(s), {early} in the first 4 s and {ticks - early} after")
    if ticks:
        print("  on: " + "  ".join(f"{role} {count * 100.0 / ticks:.0f}%"
                                  for role, count in victims.most_common()))
    stray = sum(1 for rec in trace.of("dmg") if rec.get("sp") == SPELL_MARK_LEECH) - ticks
    if stray:
        print(f"  {stray} leech tick(s) fell outside every window")

    boss = boss_guid(trace)
    if boss is None:
        return
    end = pull_end(trace)
    spans = [(window["start"], window["stop"]) for window in windows]
    gaps, cursor = [], 0
    for start, stop in spans:
        if start > cursor:
            gaps.append((cursor, start))
        cursor = max(cursor, stop)
    gaps.append((cursor, end))

    def weighted(parts):
        total = sum(stop - start for start, stop in parts)
        rates = [(boss_health_rate(samples, boss, start, stop), stop - start) for start, stop in parts]
        rates = [(rate, span) for rate, span in rates if rate is not None]
        return sum(rate * span for rate, span in rates) / total if total and rates else None

    inside_rate, outside_rate = weighted(spans), weighted(gaps)
    if inside_rate is not None and outside_rate is not None:
        print(f"  boss health inside windows {inside_rate:+.3f} %/s, outside {outside_rate:+.3f} %/s")


def walk_meets_impact(impact, start, dest) -> bool:
    if dist2(dest, impact) <= WALK_CLEARANCE:
        return True
    dx, dy = dest[0] - start[0], dest[1] - start[1]
    length = dx * dx + dy * dy
    along = 0.0
    if length:
        along = min(1.0, max(0.0, ((impact[0] - start[0]) * dx + (impact[1] - start[1]) * dy) / length))
    nearest = dist2(impact, (start[0] + dx * along, start[1] + dy * along))
    return nearest <= WALK_CLEARANCE and nearest < dist2(start, impact)


def blast_exit_walk(start, dest, impact) -> float:
    dx, dy = dest[0] - start[0], dest[1] - start[1]
    length = math.hypot(dx, dy)
    if not length:
        return math.inf if dist2(start, impact) < IMPACT_RADIUS else 0.0
    fx, fy = start[0] - impact[0], start[1] - impact[1]
    along = (fx * dx + fy * dy) / length
    disc = along * along - (fx * fx + fy * fy - IMPACT_RADIUS * IMPACT_RADIUS)
    if disc <= 0:
        return 0.0
    enter, leave = -along - math.sqrt(disc), -along + math.sqrt(disc)
    if leave <= 0 or enter >= length:
        return 0.0
    return math.inf if leave > length else leave


def crashes(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Every Shadow Crash. `masked` is what else he cast while the missile flew: on a build that read
    the impact off his current spell, that cast took the slot and the crash landed unseen."""
    samples = samples or Samples(trace)
    roster = roster_guids(trace)
    block = blocks(trace)
    boss = boss_guid(trace)
    dodges = [rec for rec in trace.of("move") if rec.get("by") == DODGE]
    walks = [rec for rec in trace.of("move")
             if rec.get("ok") and rec.get("by") != DODGE and rec.get("x") is not None]
    hits = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_SHADOW_CRASH_IMPACT]
    his = sorted((rec["t"], rec.get("sp")) for rec in trace.of("cast")
                 if rec.get("s") == boss and not rec.get("tr") and rec.get("sp") not in CRASH_SPELLS)

    out = []
    for cast, impact, x, y in pending_impacts(trace):
        target, best = None, None
        for guid in roster:
            row = samples.row(guid, cast)
            if not row or row[5] <= 0:
                continue
            gap = dist2(row[1:3], (x, y))
            if best is None or gap < best:
                target, best = guid, gap

        dodged: dict[str, set[int]] = {"L": set(), "R": set()}
        for rec in dodges:
            if cast <= rec["t"] <= impact + 1000 and block.get(rec["g"]) in dodged:
                dodged[block[rec["g"]]].add(rec["g"])

        walk_ins = []
        for rec in walks:
            if not cast <= rec["t"] <= impact:
                continue
            here = samples.row(rec["g"], rec["t"])
            if here and walk_meets_impact((x, y), here[1:3], (rec["x"], rec["y"])):
                walk_ins.append(rec)

        late = []
        for rec in dodges:
            if not (cast <= rec["t"] <= impact and rec.get("ok") and not rec.get("r") and rec.get("x") is not None):
                continue
            here = samples.row(rec["g"], rec["t"])
            if here and (blast_exit_walk(here[1:3], (rec["x"], rec["y"]), (x, y)) / RUN_SPEED
                         > (impact - rec["t"]) / 1000.0):
                late.append(rec)

        out.append({
            "cast": cast,
            "impact": impact,
            "target": target,
            "block": block.get(target, "-"),
            "dodged": dodged,
            "hits": [rec for rec in hits if impact - 500 <= rec["t"] <= impact + 1500],
            "masked": [spell for when, spell in his if cast < when < impact],
            "walk_ins": walk_ins,
            "late": late,
        })
    return out


def evictions(trace: Trace, samples: Samples) -> tuple[int, int]:
    """Accepted dodge moves issued while standing in a field, and how many of those were aimed
    outside every live field."""
    fields = aura_spans(trace, SPELL_FIELD)
    issued = left = 0
    for rec in trace.of("move"):
        if rec.get("by") != DODGE or not rec.get("ok") or rec.get("x") is None:
            continue
        if not inside(fields.get(rec["g"], []), rec["t"]):
            continue
        issued += 1
        if not in_any_field(samples, (rec["x"], rec["y"]), rec["t"]):
            left += 1
    return issued, left


def show_crash(trace: Trace) -> None:
    print("SHADOW CRASH")
    samples = Samples(trace)
    rows = crashes(trace, samples)
    if not rows:
        print("  no Shadow Crash hazard rows")
        return

    size = {name: sum(1 for block in blocks(trace).values() if block == name) for name in ("L", "R")}
    print(f"  {'impact':>9} {'target':14} {'blk':4} {'L dodged':>9} {'R dodged':>9} {'his cast in flight':22}"
          f" {'in':>3} {'late':>4} {'hits':>5}")
    for row in rows:
        victims = ", ".join(trace.name(rec.get("d")) for rec in row["hits"])
        masked = ", ".join(trace.spell(spell).rsplit(" ", 1)[0] for spell in row["masked"]) or "-"
        print(f"  {clock(row['impact']):>9} {trace.name(row['target'])[:14]:14} {row['block']:4}"
              f" {len(row['dodged']['L']):4} of {size['L']:<2} {len(row['dodged']['R']):4} of {size['R']:<2}"
              f" {masked[:22]:22} {len({rec['g'] for rec in row['walk_ins']}):3}"
              f" {len({rec['g'] for rec in row['late']}):4} {len(row['hits']):5}  {victims}")

    both = sum(1 for row in rows if row["dodged"]["L"] and row["dodged"]["R"])
    mean = statistics.mean(len(row["dodged"]["L"]) + len(row["dodged"]["R"]) for row in rows)
    print(f"\n  {both} of {len(rows)} crashes moved both blocks, {mean:.1f} of {size['L'] + size['R']}"
          " camp members dodging on average")
    masked = [row for row in rows if row["masked"]]
    print(f"  {len(masked)} of {len(rows)} crashes had another cast of his in flight:"
          f" {sum(len(row['hits']) for row in masked)} hit(s), against"
          f" {sum(len(row['hits']) for row in rows if not row['masked'])} for the rest")
    movers = collections.Counter(rec.get("by") for row in rows for rec in row["walk_ins"])
    walked_hit = sum(1 for row in rows for rec in row["hits"]
                     if rec.get("d") in {walk["g"] for walk in row["walk_ins"]})
    print(f"  {sum(movers.values())} walk(s) toward a landing impact by: "
          + (", ".join(f"{by} {count}" for by, count in movers.most_common()) or "-")
          + f"; {walked_hit} of {sum(len(row['hits']) for row in rows)} hit(s) on a bot that took one")
    late = [(row, {rec["g"] for rec in row["late"]}) for row in rows]
    print(f"  {sum(len(bots) for _, bots in late)} bot(s) whose dodge could not leave the blast by the landing at"
          f" {RUN_SPEED:.0f} yd/s, {sum(1 for row, bots in late for rec in row['hits'] if rec.get('d') in bots)}"
          " of them hit")
    issued, left = evictions(trace, samples)
    if issued:
        print(f"  {left} of {issued} dodges issued from inside a field were aimed outside every field")


def block_sign(block: str) -> int:
    return 1 if block == "R" else -1


def across(start, dest) -> float:
    bearing = ARC_ORIENTATION + math.pi / 2
    return (dest[0] - start[0]) * math.cos(bearing) + (dest[1] - start[1]) * math.sin(bearing)


def arc_spot(boss, block: str) -> tuple[float, float]:
    bearing = ARC_ORIENTATION + block_sign(block) * MARK_SIDE_OFFSET / CAMP_RADIUS
    return boss[0] + math.cos(bearing) * CAMP_RADIUS, boss[1] + math.sin(bearing) * CAMP_RADIUS


def crash_phase(samples: Samples, animus: int | None, spawn: int | None, when: int) -> str:
    if animus is None or spawn is None or when < spawn:
        return "pre"
    rows = samples.between(animus, when - MAX_STEP_MS, when)
    return "animus" if rows and rows[-1][5] > 0 else "post"


def mark_crashes(trace: Trace, samples: Samples | None = None) -> list[dict]:
    samples = samples or Samples(trace)
    boss = boss_guid(trace)
    if boss is None:
        return []
    block = blocks(trace)
    animus = animus_guid(trace)
    spawn = first_seen(trace, [animus]) if animus else None
    rows = crashes(trace, samples)
    leech = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_MARK_LEECH]
    dodges = [rec for rec in trace.of("move")
              if rec.get("by") == DODGE and rec.get("x") is not None
              and (rec.get("ok") and not rec.get("r") or rec.get("r") == "dup")]

    def first_dodge(guid: int, low: int, high: int):
        return next((rec for rec in dodges if rec["g"] == guid and low <= rec["t"] <= high), None)

    out = []
    for window in mark_windows(trace, samples):
        marked, side = window["guid"], window["block"]
        crash = next((row for row in rows if window["start"] <= row["cast"] < window["stop"]), None)
        if side not in ("L", "R") or crash is None:
            continue
        cast, impact = crash["cast"], crash["impact"]
        ticks = [rec for rec in leech if window["start"] <= rec["t"] <= window["stop"] + LEECH_SLACK_MS]
        group = [rec for rec in ticks if block.get(rec.get("d")) == side]

        walk = None
        own = first_dodge(marked, cast, impact)
        him = samples.row(boss, own["t"]) if own else None
        if him:
            spot = arc_spot(him[1:3], side)
            walk = "spot" if dist2((own["x"], own["y"]), spot) <= 2 * MARK_SPOT_TOLERANCE else "off spot"

        ways: collections.Counter = collections.Counter()
        for guid, mate in block.items():
            if mate != side or guid == marked:
                continue
            rec = first_dodge(guid, cast, impact)
            here = samples.row(guid, rec["t"]) if rec else None
            if here:
                step = across(here[1:3], (rec["x"], rec["y"]))
                ways["with" if step * block_sign(side) > 0 else "away"] += 1

        out.append({
            "start": window["start"],
            "guid": marked,
            "block": side,
            "phase": crash_phase(samples, animus, spawn, cast),
            "cast": cast,
            "on": "it" if crash["target"] == marked else crash["block"],
            "ticks": len(ticks),
            "group": len(group),
            "group_after": sum(1 for rec in group if rec["t"] >= cast),
            "walk": walk,
            "with": ways["with"],
            "away": ways["away"],
            "rate": window["rate"],
        })
    return out


def show_mark_crash(trace: Trace) -> None:
    print("MARK AND CRASH")
    rows = mark_crashes(trace)
    if not rows:
        print("  no crash cast inside a camp member's mark")
        return

    print(f"  {'mark':>9} {'marked':14} {'blk':4} {'phase':6} {'crash':>6} {'on':3} {'ticks':>5} {'group':>5}"
          f" {'after':>5} {'marked walk':12} {'group with/away':>15} {'boss %/s':>8}")
    for row in rows:
        rate = "-" if row["rate"] is None else f"{row['rate']:+.3f}"
        print(f"  {clock(row['start']):>9} {trace.name(row['guid'])[:14]:14} {row['block']:4} {row['phase']:6}"
              f" {(row['cast'] - row['start']) / 1000:+5.1f}s {row['on']:3} {row['ticks']:5} {row['group']:5}"
              f" {row['group_after']:5} {row['walk'] or 'no dodge':12} {row['with']:>9}/{row['away']:<5} {rate:>8}")

    print("\n  crash is its cast after the mark, on the block it was aimed at or `it` for the marked bot")
    print("  group is the leech ticks on the marked bot's block, after those from the cast on")
    print("  marked walk is its first dodge in the flight, `spot` when it ends within"
          f" {2 * MARK_SPOT_TOLERANCE:.0f} yd of its arc spot; a `dup` row counts, the bot already walking there")
    print("  with/away count its blockmates whose first dodge in the flight went toward its arc side or the"
          " other way")


def field_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    samples = samples or Samples(trace)
    end = pull_end(trace)
    impacts = pending_impacts(trace)
    field = aura_spans(trace, SPELL_FIELD)
    cost = aura_spans(trace, SPELL_FIELD_COST)
    formation = formation_spans(trace)
    block = blocks(trace)
    casts = [rec for rec in trace.of("cast") if not rec.get("tr") and 0 <= rec["t"] <= end]
    moves = [rec for rec in trace.of("move") if rec.get("by") == SOAK]
    holds = collections.Counter(rec.get("g") for rec in trace.of("veto") if rec.get("m") == HOLD)

    out = []
    for guid in sorted(camp(trace), key=trace.name):
        mine = [rec for rec in casts if rec.get("s") == guid]
        outside = [rec for rec in mine if not inside(field.get(guid, []), rec["t"])]

        reach_casts = 0
        for rec in outside:
            row = samples.row(guid, rec["t"])
            if row and field_in_reach(samples, impacts, row[1:3], rec["t"]):
                reach_casts += 1

        away = reach = 0
        for _, when, weight in samples.steps(0, end):
            if not inside(formation.get(guid, []), when) or inside(field.get(guid, []), when):
                continue
            row = samples.row(guid, when)
            if not row or row[5] <= 0:
                continue
            away += weight
            if field_in_reach(samples, impacts, row[1:3], when):
                reach += weight

        soak = collections.Counter(("ok" if rec.get("ok") else rec.get("r") or "refused")
                                   for rec in moves if rec.get("g") == guid)
        up = covered(field.get(guid, []), 0, end)
        up_cost = covered(cost.get(guid, []), 0, end)
        out.append({
            "guid": guid,
            "block": block.get(guid, "-"),
            "field": up,
            "cost": up_cost,
            "shortfall": 1.0 - up_cost / up if up else None,
            "casts": len(mine),
            "in_cost": sum(1 for rec in mine if inside(cost.get(guid, []), rec["t"])),
            "outside": len(outside),
            "reach_casts": reach_casts,
            "away": away,
            "reach": reach,
            "soak": soak,
            "holds": holds.get(guid, 0),
        })
    return out


def show_field(trace: Trace) -> None:
    print("SHADOW CRASH FIELD")
    end = pull_end(trace)
    rows = field_rows(trace)
    if not rows:
        print("  no camp: nobody ever held an L or R slot")
        return

    print(f"  {'bot':14} {'role':6} {'blk':4} {'63277':>6} {'65269':>6} {'short':>6} {'casts':>6}"
          f" {'in 65269':>8} {'out/reach':>10} {'away reach':>10} {'soak ok/dup/wait':>17} {'held':>6}")
    for row in rows:
        short = "-" if row["shortfall"] is None else f"{row['shortfall'] * 100:.0f}%"
        share = f"{row['in_cost'] * 100.0 / row['casts']:.0f}%" if row["casts"] else "-"
        away = f"{row['reach'] * 100.0 / row['away']:.0f}%" if row["away"] else "-"
        soak = f"{row['soak']['ok']}/{row['soak']['dup']}/{row['soak']['wait']}"
        print(f"  {trace.name(row['guid'])[:14]:14} {trace.role(row['guid']):6} {row['block']:4}"
              f" {row['field'] * 100.0 / end:5.0f}% {row['cost'] * 100.0 / end:5.0f}% {short:>6}"
              f" {row['casts']:6} {share:>8} {row['outside']:4}/{row['reach_casts']:<5} {away:>10}"
              f" {soak:>17} {row['holds']:6}")

    away = sum(row["away"] for row in rows)
    reach = sum(row["reach"] for row in rows)
    outside = sum(row["outside"] for row in rows)
    reach_casts = sum(row["reach_casts"] for row in rows)
    soak = sum((row["soak"] for row in rows), collections.Counter())
    print(f"\n  out of a field: a clear one within {SOAK_MAX_TRAVEL:.0f} yd for"
          f" {reach * 100.0 / away if away else 0:.0f}% of the time and {reach_casts} of {outside} casts")
    print(f"  soak moves ok {soak['ok']}, dup {soak['dup']}, wait {soak['wait']}"
          f"; cast hold vetoes {sum(row['holds'] for row in rows)}")
    print("  63277/65269 are uptime over the pull, short is how far 65269 trails 63277, out/reach is"
          " casts made outside a field / of those with one in reach")


def mana_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    samples = samples or Samples(trace)
    end = pull_end(trace)
    taps = [rec for rec in trace.of("cast") if rec.get("sp") == SPELL_LIFE_TAP and not rec.get("tr")]
    glyph = collections.Counter(rec.get("d") for rec in trace.of("aura")
                                if rec.get("sp") == SPELL_LIFE_TAP_GLYPH and not rec.get("r"))
    vetoes = collections.Counter(rec.get("g") for rec in trace.of("veto")
                                 if rec.get("m") == LIFE_TAP_VETO)

    out = []
    for guid in sorted(roster_guids(trace), key=trace.name):
        rows = samples.between(guid, 0, end)
        if len(rows) < 2 or max(row[6] for row in rows) <= 0:
            continue
        stamps = samples.rows[guid][0]
        start = bisect.bisect_left(stamps, 0)

        def mana_at(when):
            row = samples.row(guid, when)
            return row[6] if row and when <= end else None

        low = next((stamps[start + i] for i, row in enumerate(rows) if row[6] < 5.0), None)
        empty = next((stamps[start + i] for i, row in enumerate(rows) if row[6] <= 0.5), None)

        mine = [rec for rec in taps if rec.get("s") == guid]
        rises = 0
        for rec in mine:
            before = samples.row(guid, rec["t"])
            after = samples.between(guid, rec["t"] + 1, rec["t"] + 2000)
            if before and after and max(row[6] for row in after) - before[6] >= 3.0:
                rises += 1

        out.append({
            "guid": guid,
            "at": [mana_at(when) for when in (60000, 120000, 180000)],
            "end": rows[-1][6],
            "low": low,
            "empty": empty,
            "taps": len(mine),
            "rises": rises,
            "glyph": glyph.get(guid, 0),
            "vetoes": vetoes.get(guid, 0),
        })
    return out


def show_mana(trace: Trace) -> None:
    print("MANA")
    rows = mana_rows(trace)
    if not rows:
        print("  nobody with mana was sampled")
        return

    def pct(value):
        return "-" if value is None else f"{value:.0f}"

    print(f"  {'bot':14} {'role':6} {'1:00':>5} {'2:00':>5} {'3:00':>5} {'end':>5} {'<5% at':>9}"
          f" {'0% at':>9} {'taps':>5} {'rises':>6} {'glyph':>6} {'vetoes':>7}")
    for row in rows:
        print(f"  {trace.name(row['guid'])[:14]:14} {trace.role(row['guid']):6}"
              + "".join(f" {pct(value):>5}" for value in row["at"])
              + f" {pct(row['end']):>5} {clock(row['low']) if row['low'] is not None else '-':>9}"
              f" {clock(row['empty']) if row['empty'] is not None else '-':>9} {row['taps']:5}"
              f" {row['rises']:6} {row['glyph']:6} {row['vetoes']:7}")
    print("\n  rises is taps followed within 2 s by a gain of 3+ mana points; Aura of Despair makes"
          " that 0 here, against +15 to +22 a tap on Hodir")


def vapor_share(trace: Trace, samples: Samples | None = None) -> dict[int, float]:
    """Share of each bot's living samples spent targeting a vapor."""
    samples = samples or Samples(trace)
    vapors = guids_of_entry(trace, NPC_SARONITE_VAPORS)
    end = pull_end(trace)
    out = {}
    for guid in roster_guids(trace):
        alive = [row for row in samples.between(guid, 0, end) if row[5] > 0 and len(row) > 7]
        if alive:
            out[guid] = sum(1 for row in alive if row[7] in vapors) / len(alive)
    return out


def vapor_hits(trace: Trace) -> dict[tuple[int, int], list[int]]:
    """When each (caster, spell) landed on a vapor, from the cast rows aimed at one."""
    vapors = guids_of_entry(trace, NPC_SARONITE_VAPORS)
    out: dict[tuple[int, int], list[int]] = collections.defaultdict(list)
    for rec in trace.of("cast"):
        if rec.get("tgt") in vapors:
            out[(rec.get("s"), rec.get("sp"))].append(rec["t"])
    return dict(out)


def first_values(trace: Trace, key: str) -> dict[str, int]:
    """The first time any bot's `key` read each value."""
    out: dict[str, int] = {}
    for rec in notes(trace, key):
        out.setdefault(str(rec.get("txt", "")), rec["t"])
    return out


def hard_mode_changes(trace: Trace) -> dict[str, int]:
    return first_values(trace, "vezax.hardmode")


def hold_changes(trace: Trace) -> dict[str, int]:
    """`on` is the non-tank stop line, `tank` the tanks' one, `off` the release."""
    return first_values(trace, "vezax.hold")


def show_vapors(trace: Trace) -> None:
    print("SARONITE VAPORS")
    boss = boss_guid(trace)
    summons = [rec["t"] for rec in trace.of("cast")
               if rec.get("sp") == SPELL_SUMMON_VAPORS and rec.get("s") == boss]
    vapors = guids_of_entry(trace, NPC_SARONITE_VAPORS)
    print(f"  {len(summons)} summon(s){': ' + ', '.join(clock(t) for t in summons) if summons else ''}"
          f"; {len(vapors)} vapor(s) named")

    shares = sorted(((share, guid) for guid, share in vapor_share(trace).items() if share > 0),
                    reverse=True)
    if shares:
        print("  targeting a vapor, share of living samples:")
        for share, guid in shares[:8]:
            print(f"    {trace.name(guid)[:14]:14} {trace.role(guid):6} {share * 100:5.1f}%")
    else:
        print("  nobody targeted a vapor")

    drops = collections.Counter(rec["g"] for rec in notes(trace, "vezax.target") if rec.get("txt") == "vapor")
    if drops:
        print("  vezax.target = vapor: " + ", ".join(f"{trace.name(g)} {n}" for g, n in drops.most_common()))
    guard = collections.Counter(rec.get("a") for rec in trace.of("veto") if rec.get("m") == GUARD)
    if guard:
        print("  target guard vetoes: " + ", ".join(f"{action} {n}" for action, n in guard.most_common()))

    hits = vapor_hits(trace)
    if hits:
        print("  cast at a vapor:")
        for (caster, spell), times in sorted(hits.items(), key=lambda item: item[1][0]):
            print(f"    {trace.name(caster)[:14]:14} {trace.spell(spell)[:24]:24} x{len(times):<3}"
                  f" {clock(times[0])} to {clock(times[-1])}")

    changes = hard_mode_changes(trace)
    if changes:
        print("  vezax.hardmode: " + ", ".join(f"{value} at {clock(when)}" for value, when
                                             in sorted(changes.items(), key=lambda item: item[1])))

    samples = Samples(trace)

    def health_at(when: int) -> str:
        row = samples.row(boss, when)
        return f" ({row[5]:.1f}%)" if row else ""

    holds = hold_changes(trace)
    if holds:
        print("  vezax.hold: " + ", ".join(f"{value} at {clock(when)}{health_at(when)}" for value, when
                                         in sorted(holds.items(), key=lambda item: item[1])))

    animus = guids_of_entry(trace, NPC_SARONITE_ANIMUS)
    if animus:
        seen = first_seen(trace, animus)
        where = f" at {clock(seen)}{health_at(seen)}" if seen is not None else ""
        print(f"  Saronite Animus spawned{where}: hard mode held")
    else:
        print(f"  no Saronite Animus after {len(summons)} summon(s)"
              + (": a vapor died, or the sixth summon never came" if len(summons) >= 6 else ""))


def animus_guid(trace: Trace) -> int | None:
    guids = sorted(guids_of_entry(trace, NPC_SARONITE_ANIMUS))
    return guids[0] if guids else None


def animus_victims(trace: Trace, samples: Samples | None = None) -> list[tuple[int, int]]:
    """`(t, victim)` at each change of the Animus's victim, off `snap.u[7]`. It can't be taunted, so
    this is the whole story of who held it."""
    animus = animus_guid(trace)
    found = (samples or Samples(trace)).rows.get(animus) if animus else None
    if not found:
        return []
    out: list[tuple[int, int]] = []
    for when, row in zip(*found):
        victim = row[7] if len(row) > 7 else 0
        if row[5] > 0 and (not out or out[-1][1] != victim):
            out.append((when, victim))
    return out


def animus_redirects(trace: Trace, spawn: int) -> list[dict]:
    """Misdirection and Tricks cast from the lookback before the spawn on. A negative `rel` is one
    spent before it came, which is the cooldown it didn't have."""
    return [{"t": rec["t"], "rel": rec["t"] - spawn, "caster": rec.get("s"), "spell": rec.get("sp"),
             "target": rec.get("tgt")}
            for rec in trace.of("cast")
            if rec.get("sp") in REDIRECT_SPELLS and not rec.get("tr")
            and rec["t"] >= spawn - REDIRECT_LOOKBACK_MS]


def animus_switches(trace: Trace, spawn: int) -> dict[int, int]:
    """Per bot, ms from the spawn to its first switch onto the Animus."""
    out: dict[int, int] = {}
    for rec in trace.of("act"):
        if rec.get("a") == ANIMUS_SWITCH and rec.get("vd") == "OK" and rec["t"] >= spawn:
            out.setdefault(rec["g"], rec["t"] - spawn)
    return out


def animus_window(trace: Trace, samples: Samples, spawn: int) -> tuple[int, str]:
    """`(t, why)` where the Animus fight stops being worth measuring: `kill` when it leaves the
    snapshots dying, `collapse` once under four fifths of the sampled roster lives, else the pull's
    `end`."""
    end = pull_end(trace)
    stamps, rows = samples.rows[animus_guid(trace)]
    gone = stamps[-1] if stamps[-1] < end - MAX_STEP_MS and rows[-1][5] < KILL_LAST_HEALTH_PCT else None
    roster = [guid for guid in roster_guids(trace) if guid in samples.rows]
    for _, when, _ in samples.steps(spawn, end):
        if gone is not None and when > gone:
            return gone, "kill"
        alive = sum(1 for guid in roster if (row := samples.row(guid, when)) and row[5] > 0)
        if alive < COLLAPSE_ALIVE_SHARE * len(roster):
            return when, "collapse"
    return (gone, "kill") if gone is not None else (end, "end")


def animus_casters(trace: Trace, samples: Samples | None = None) -> dict | None:
    """The Animus fight from the spawn to `animus_window`: its health, the raid's damage, and per
    ranged and healer bot what it cast, how much of it from a field, damage dealt, mana and cast holds,
    all and those with the Animus as its sampled target, the share of its living time spent past reach
    spell's ceiling on the Animus and the reach spell walks it took. None when it never spawned."""
    animus = animus_guid(trace)
    spawn = first_seen(trace, [animus]) if animus else None
    if spawn is None:
        return None
    samples = samples or Samples(trace)
    stop, why = animus_window(trace, samples, spawn)
    field = aura_spans(trace, SPELL_FIELD)
    casts = [rec for rec in trace.of("cast") if not rec.get("tr") and spawn <= rec["t"] <= stop]
    holds = [rec for rec in trace.of("veto") if rec.get("m") == HOLD and spawn <= rec["t"] <= stop]
    walks = [rec for rec in trace.of("move")
             if rec.get("by") == REACH_SPELL and not rec.get("r") and spawn <= rec["t"] <= stop]
    dodges = [rec for rec in trace.of("move")
              if rec.get("by") == DODGE and rec.get("ok") and not rec.get("r") and rec.get("x") is not None
              and spawn <= rec["t"] <= stop]

    def dodges_past(guid: int) -> tuple[int, int]:
        mine = [rec for rec in dodges if rec.get("g") == guid]
        past = 0
        for rec in mine:
            target = samples.row(animus, rec["t"])
            if target and math.dist((rec["x"], rec["y"]), target[1:3]) > ANIMUS_REACH_CEILING:
                past += 1
        return len(mine), past

    def past_reach(guid: int) -> float | None:
        alive = past = 0
        for _, when, weight in samples.steps(spawn, stop):
            row = samples.row(guid, when)
            target = samples.row(animus, when)
            if not row or row[5] <= 0 or not target:
                continue
            alive += weight
            if math.dist(row[1:4], target[1:4]) > ANIMUS_REACH_CEILING:
                past += weight
        return past / alive if alive else None

    def dealt(guid: int, when: int) -> float:
        row = samples.row(guid, when)
        return row[11] if row and len(row) > 11 else 0.0

    def on_animus(guid: int, when: int) -> bool:
        row = samples.row(guid, when)
        return bool(row) and len(row) > 7 and row[7] == animus

    def mana(guid: int, when: int) -> float | None:
        """At the bot's last living sample by `when`: the collapse tick itself reads the dead as 0."""
        found = samples.rows.get(guid)
        if not found:
            return None
        index = bisect.bisect_right(found[0], when)
        living = next((row for row in reversed(found[1][:index]) if row[5] > 0), None)
        return living[6] if living else None

    rows = []
    for guid in sorted(roster_guids(trace), key=lambda guid: (trace.role(guid), trace.name(guid))):
        at_spawn = samples.row(guid, spawn)
        if trace.role(guid) not in ("ranged", "heal") or not at_spawn or at_spawn[5] <= 0:
            continue
        mine = [rec for rec in casts if rec.get("s") == guid]
        held = [rec for rec in holds if rec.get("g") == guid]
        rows.append({
            "guid": guid,
            "casts": len(mine),
            "on_animus": sum(1 for rec in mine if rec.get("tgt") == animus),
            "outside": sum(1 for rec in mine if not inside(field.get(guid, []), rec["t"])),
            "field": covered(field.get(guid, []), spawn, stop),
            "dealt": dealt(guid, stop) - dealt(guid, spawn),
            "mana": (mana(guid, spawn), mana(guid, stop)),
            "holds": len(held),
            "held_on": sum(1 for rec in held if on_animus(guid, rec["t"])),
            "past_reach": past_reach(guid),
            "reach_walks": sum(1 for rec in walks if rec.get("g") == guid),
            "dodges": dodges_past(guid),
        })

    health = (samples.row(animus, spawn)[5], samples.row(animus, stop)[5])
    return {
        "spawn": spawn,
        "stop": stop,
        "why": why,
        "health": (health[0], 0.0 if why == "kill" else health[1]),
        "raid": sum(dealt(guid, stop) - dealt(guid, spawn) for guid in roster_guids(trace)),
        "rows": rows,
    }


def probe_states(trace: Trace, key: str, spawn: int) -> dict[str, set[int]]:
    out: dict[str, set[int]] = collections.defaultdict(set)
    for rec in notes(trace, key):
        if rec["t"] >= spawn:
            out[str(rec.get("txt", ""))].add(rec["g"])
    return dict(sorted(out.items()))


def reach_states(trace: Trace, spawn: int) -> dict[str, set[int]]:
    return probe_states(trace, "vezax.reach", spawn)


def show_animus(trace: Trace) -> None:
    print("SARONITE ANIMUS")
    animus = animus_guid(trace)
    spawn = first_seen(trace, [animus]) if animus else None
    if spawn is None:
        print("  never spawned")
        return

    samples = Samples(trace)
    rows = samples.rows[animus][1]
    print(f"  spawned {clock(spawn)}, {rows[-1][5]:.1f}% at the end")

    victims = animus_victims(trace, samples)
    tank = next(((when, guid) for when, guid in victims if trace.role(guid) == "tank"), None)
    print("  first on a tank: " + (f"{clock(tank[0])} (+{(tank[0] - spawn) / 1000:.1f} s), {trace.name(tank[1])}"
                                   if tank else "never"))
    held, _ = threat_share(trace, {animus}, lambda when: when >= spawn)
    total = sum(held.values())
    if total:
        print("  time on target: " + "  ".join(f"{role} {span * 100.0 / total:.1f}%"
                                              for role, span in held.most_common()))
    shown = victims[:ANIMUS_VICTIMS_SHOWN]
    print("  victims: " + ", ".join(f"{clock(when)} {trace.name(guid)}" for when, guid in shown)
          + (f" and {len(victims) - len(shown)} more" if len(victims) > len(shown) else ""))

    redirects = animus_redirects(trace, spawn)
    print("  redirects:" + ("" if redirects else " none"))
    for row in redirects:
        print(f"    {clock(row['t'])} {row['rel'] / 1000:+6.1f} s  {trace.name(row['caster'])[:14]:14}"
              f" {trace.spell(row['spell'])[:26]:26} on {trace.name(row['target'])} ({trace.role(row['target'])})")

    by_role: dict[str, list[int]] = collections.defaultdict(list)
    for guid, delay in animus_switches(trace, spawn).items():
        by_role[trace.role(guid)].append(delay)
    if by_role:
        print("  switched on after the spawn: " + "  ".join(
            f"{role} {min(delays) / 1000:.1f}-{max(delays) / 1000:.1f} s ({len(delays)})"
            for role, delays in sorted(by_role.items())))

    lust = [rec for rec in trace.of("cast") if rec.get("sp") in LUST_SPELLS and not rec.get("tr")]
    for rec in lust:
        print(f"  {trace.spell(rec['sp'])} by {trace.name(rec.get('s'))} at {clock(rec['t'])}"
              f" ({(rec['t'] - spawn) / 1000:+.1f} s)")
    if not lust:
        print("  no lust cast")

    show_animus_casters(trace, animus_casters(trace, samples))

    for key in ("vezax.reach", "vezax.dodge"):
        states = probe_states(trace, key, spawn)
        print(f"  {key} after the spawn: " + (", ".join(f"{value} {len(bots)} bot(s)"
                                                       for value, bots in states.items()) or "never written"))

    deaths = [death for death in combat_deaths(trace) if death["t"] >= spawn]
    if deaths:
        print(f"  {len(deaths)} death(s) after the spawn:")
        for death in deaths:
            blow = death.get("blow") or [0, 0]
            print(f"    {clock(death['t'])} {trace.name(death.get('g'))[:14]:14} {trace.role(death.get('g')):6}"
                  f" {trace.name(death.get('killer'))} {blow[1] if len(blow) > 1 else ''}")


def show_animus_casters(trace: Trace, fight: dict | None) -> None:
    if not fight:
        return
    span = (fight["stop"] - fight["spawn"]) / 1000.0
    if span <= 0:
        return
    start, left = fight["health"]
    rate = (start - left) / span
    if fight["why"] == "kill":
        pace = f"killed in {span:.0f} s"
    else:
        pace = f"a kill takes ~{start / rate:.0f} s" if rate > 0 else "no progress"
    rows = fight["rows"]
    ranged = sum(row["dealt"] for row in rows if trace.role(row["guid"]) == "ranged")
    print(f"  spawn to {fight['why']} at {clock(fight['stop'])} ({span:.1f} s): Animus {start:.1f}% to"
          f" {left:.1f}%, {rate:.2f} %/s ({pace}); raid {fight['raid'] / span / 1000:.0f}k/s,"
          f" ranged {ranged / span / 1000:.0f}k/s")

    def pct(value):
        return "-" if value is None else f"{value:.0f}"

    print(f"    {'bot':14} {'role':6} {'casts/min':>9} {'on it':>6} {'outside':>8} {'field':>6} {'dealt/s':>8}"
          f" {'mana':>9} {'held':>7} {'past':>5} {'walks':>5} {'dodges':>7}")
    for row in rows:
        mana = f"{pct(row['mana'][0])}>{pct(row['mana'][1])}"
        held = f"{row['holds']}/{row['held_on']}"
        past = "-" if row["past_reach"] is None else f"{row['past_reach']:.0%}"
        dodges = f"{row['dodges'][1]}/{row['dodges'][0]}"
        print(f"    {trace.name(row['guid'])[:14]:14} {trace.role(row['guid']):6}"
              f" {row['casts'] * 60.0 / span:9.1f} {row['on_animus']:6} {row['outside']:8}"
              f" {row['field'] / 1000.0 / span:6.0%} {row['dealt'] / span:8.0f} {mana:>9} {held:>7}"
              f" {past:>5} {row['reach_walks']:5} {dodges:>7}")
    print("    casts are starts, outside is those made out of a field, mana is at the spawn and the stop,"
          " held is cast hold vetoes, all/with the Animus targeted")
    print(f"    past is living time over {ANIMUS_REACH_CEILING:.1f} yd from the Animus, where reach spell"
          " fires; walks are its accepted moves; dodges are accepted Shadow Crash dodges ending past it/all")


def main_tank(trace: Trace, samples: Samples | None = None) -> int | None:
    boss = boss_guid(trace)
    found = (samples or Samples(trace)).rows.get(boss) if boss else None
    if not found:
        return None
    held: collections.Counter = collections.Counter()
    stamps, rows = found
    for index in range(len(rows) - 1):
        row = rows[index]
        if row[5] > 0 and len(row) > 7 and trace.role(row[7]) == "tank":
            held[row[7]] += min(stamps[index + 1] - stamps[index], MAX_STEP_MS)
    return held.most_common(1)[0][0] if held else None


def tank_buffs(trace: Trace, samples: Samples | None = None) -> dict | None:
    samples = samples or Samples(trace)
    tank = main_tank(trace, samples)
    if tank is None:
        return None
    end = pull_end(trace)
    death = next((rec["t"] for rec in combat_deaths(trace) if rec.get("g") == tank), end)
    animus = animus_guid(trace)
    spawn = first_seen(trace, [animus]) if animus else None
    beacon = aura_spans(trace, SPELL_BEACON)

    def shares(low: int, high: int) -> tuple[float, float] | None:
        if high <= low:
            return None
        mine = covered(beacon.get(tank, []), low, high)
        other = sum(covered(spans, low, high) for guid, spans in beacon.items()
                    if guid != tank and trace.role(guid) == "tank")
        return mine / (high - low), other / (high - low)

    split = min(spawn, death) if spawn is not None else death
    copies = collections.Counter()
    for rec in trace.of("heal"):
        if rec.get("sp") in BEACON_COPIES and rec["t"] < death:
            copies["him" if rec.get("d") == tank else "others"] += rec.get("a", 0) - rec.get("oh", 0)
            copies["cast"] += rec.get("a", 0)
    shields = collections.Counter(
        "him" if rec.get("tgt") == tank else trace.role(rec.get("tgt"))
        for rec in trace.of("cast") if rec.get("sp") == SPELL_SACRED_SHIELD and not rec.get("tr"))
    return {
        "tank": tank,
        "death": death if death < end else None,
        "before": shares(0, split),
        "after": shares(spawn, death) if spawn is not None else None,
        "copies": copies,
        "shields": shields,
        "beacons": sum(1 for rec in trace.of("cast") if rec.get("sp") == SPELL_BEACON and not rec.get("tr")),
        "holds": sum(1 for rec in trace.of("veto") if rec.get("m") == TANK_BUFFS_HOLD),
    }


def show_tank(trace: Trace) -> None:
    print("MAIN TANK")
    buffs = tank_buffs(trace)
    if not buffs:
        print("  no tank held him")
        return
    death = clock(buffs["death"]) if buffs["death"] is not None else "never"
    print(f"  {trace.name(buffs['tank'])}, the tank he hit longest; first death {death}")
    if not buffs["beacons"]:
        print("  no Beacon of Light cast")
    else:
        for label, share in (("before the Animus", buffs["before"]), ("after it", buffs["after"])):
            if share:
                print(f"  Beacon of Light while he lived, {label}: on him {share[0]:.0%},"
                      f" on another tank {share[1]:.0%}")
        copies = buffs["copies"]
        print(f"  beacon copies {copies['cast']:,} cast, {copies['him']:,} landed on him and"
              f" {copies['others']:,} on others, overheal aside, to his first death")
    shields = buffs["shields"]
    print("  Sacred Shield casts: " + (", ".join(f"{key} {count}" for key, count in shields.most_common())
                                       or "none"))
    print(f"  {TANK_BUFFS_HOLD} vetoes {buffs['holds']}")


def band_rows(trace: Trace, samples: Samples | None = None) -> tuple[list[float], list[dict]]:
    samples = samples or Samples(trace)
    boss = boss_guid(trace)
    end = pull_end(trace)
    if boss is None:
        return [], []

    drift = [dist2(row[1:3], ANCHOR) for row in samples.between(boss, 0, end)]
    formation = formation_spans(trace)
    kept = latch_spans_for(trace, "vezax.formation", "kept")
    block = blocks(trace)
    out = []
    for guid in sorted(camp(trace), key=trace.name):
        radii = []
        joined = min((start for start, _ in formation.get(guid, [])), default=None)
        locked = 0
        for _, when, weight in samples.steps(0, end):
            row, him = samples.row(guid, when), samples.row(boss, when)
            living = row and him and row[5] > 0 and him[5] > 0
            if not inside(formation.get(guid, []), when):
                if living and joined is not None and when >= joined:
                    locked += weight
                continue
            if living:
                radii.append(dist2(row[1:3], him[1:3]))
        # Kept even with no radii: a slotted bot that never got inside the gate is the finding.
        on = sum(max(0, min(stop, end) - max(start, 0)) for start, stop in formation.get(guid, []))
        held = sum(max(0, min(stop, end) - max(start, 0)) for start, stop in kept.get(guid, []))
        out.append({
            "guid": guid,
            "block": block.get(guid, "-"),
            "on": on / end if end > 0 else 0.0,
            "kept": held / end if end > 0 else 0.0,
            "locked": locked,
            "median": statistics.median(radii) if radii else None,
            "inside": sum(1 for r in radii if r < BAND_FLOOR) / len(radii) if radii else None,
            "past": sum(1 for r in radii if r > BAND_CEILING) / len(radii) if radii else None,
        })
    return drift, out


def show_band(trace: Trace) -> None:
    print("BAND")
    drift, rows = band_rows(trace)
    if not drift:
        print("  General Vezax was never sampled")
        return

    print(f"  boss from ULDUAR_VEZAX_ANCHOR: median {statistics.median(drift):.1f} yd, max {max(drift):.1f}")
    print("  so postmortem --from/--band measure the camp against a point he has walked away from")
    if not rows:
        print("  no camp member in the trace")
        return

    print(f"\n  {'bot':14} {'role':6} {'blk':4} {'on':>5} {'kept':>5} {'out s':>6} {'median':>7}"
          f" {'<' + format(BAND_FLOOR, '.1f'):>7} {'>' + format(BAND_CEILING, '.1f'):>7}")
    for row in rows:
        figures = (f"{'-':>7} {'-':>7} {'-':>7}" if row["median"] is None else
                   f"{row['median']:7.1f} {row['inside'] * 100:6.1f}% {row['past'] * 100:6.1f}%")
        print(f"  {trace.name(row['guid'])[:14]:14} {trace.role(row['guid']):6} {row['block']:4}"
              f" {row['on'] * 100:4.0f}% {row['kept'] * 100:4.0f}% {row['locked'] / 1000:6.1f} {figures}")
    never = sum(1 for row in rows if row["median"] is None)
    if never:
        print(f"\n  {never} of {len(rows)} never inside the formation gate:"
              " no slot, no dodge, no movement guard")
    print(f"\n  on is the share of the pull the formation gate was open, kept the share the fight's latch held"
          f" it open past {ARENA_RADIUS:.0f} yd, out s the living time shut out after joining; radius from the"
          f" boss where he stood, gate open only; slots sit at {CAMP_RADIUS:.1f}")


SECTIONS = (
    ("boss", "what he cast, Searing Flames, interrupters", show_boss),
    ("mark", "Mark of the Faceless windows", show_mark),
    ("crash", "Shadow Crash impacts, who dodged, his cast in flight, walks toward it", show_crash),
    ("markcrash", "a crash cast inside a camp mark: leech on the marked bot's block, which way it and the"
     " block dodged", show_mark_crash),
    ("field", "field uptime, in-field casts, soak, cast hold", show_field),
    ("mana", "mana, Life Tap and what it returned", show_mana),
    ("vapors", "vapor targeting, hits and the Animus", show_vapors),
    ("animus", "who held the Animus, redirects, lust, its pace and casters, deaths", show_animus),
    ("tank", "where Beacon of Light and Sacred Shield sat against his main tank", show_tank),
    ("band", "each camp member's distance from the boss", show_band),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
