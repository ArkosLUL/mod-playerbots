#!/usr/bin/env python3
"""Score a General Vezax pull from a RaidObs trace: the mark, Shadow Crash, fields, mana, vapors.

    general_vezax.py <file>            every section
    general_vezax.py <file> --boss     what he cast, Searing Flames, interrupters
    general_vezax.py <file> --mark     every Mark of the Faceless window: leech, escape, boss health
    general_vezax.py <file> --crash    every Shadow Crash: who it hit, which blocks dodged
    general_vezax.py <file> --field    field uptime, casts inside one, the soak and the cast hold
    general_vezax.py <file> --mana     mana over the pull, Life Tap and what it returned
    general_vezax.py <file> --vapors   who targeted a vapor, and whether the Animus came
    general_vezax.py <file> --band     each camp member's distance from the boss, not the anchor

What the generic views get wrong here, and what this reads instead:

- **The camp is boss-relative and he walks.** `postmortem.py --from`/`--band`/`--moves` measure from
  a fixed point, `ULDUAR_VEZAX_ANCHOR` or his spot at the pull, and a camp member's radius from
  either is off by a median 8 to 16 yd against the real one, wider than the band. Everything here
  measures from where he stands at each sample.
- **Saronite Vapors are never sampled**, so nothing says where one was. Targeting is read off the
  bot's own target column against the vapor guids the unit rows name.
- **Heals on a creature are never recorded** (`ObsSession::Tracks` wants a player), so the mark's
  heal-back only shows as his health rate inside a window against outside one.
- **`vezax.mark` is a per-bot change-only latch**: a bot that takes the same branch twice writes one
  row. Windows come from the 63276 aura; the latch only names the branch where it changed.
- **`vezax.slot` goes silent on the pull after a wipe** on any build where the state reset needed the
  boss dead. The assignment carried over, so that pull cannot say who held which slot.
"""
from __future__ import annotations

import bisect
import collections
import pathlib
import statistics
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.geometry import anchor, dist2, first_seen, frames, guids_of_entry, radius  # noqa: E402
from raidobs.probes import emitted_keys, silent_keys  # noqa: E402
from raidobs.trace import Trace, clock, notes, roster_guids  # noqa: E402

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
# The half of the field carrying the -70% mana cost, linked off 63277 and not always behind it.
SPELL_FIELD_COST = 65269
SPELL_LIFE_TAP = 57946
SPELL_LIFE_TAP_GLYPH = 63321

DODGE = "vezax shadow crash dodge action"
SOAK = "vezax shadow crash soak action"
HOLD = "vezax hold cast outside field multiplier"
GUARD = "vezax target guard multiplier"
LIFE_TAP_VETO = "vezax suppress life tap multiplier"

# Read from the source so a retune shows up here without a second edit.
CAMP_RADIUS = radius("ULDUAR_VEZAX_CAMP_RADIUS")
SOAK_MAX_TRAVEL = radius("ULDUAR_VEZAX_SHADOW_CRASH_SOAK_MAX_TRAVEL")
IMPACT_RADIUS = radius("ULDUAR_VEZAX_SHADOW_CRASH_IMPACT_RADIUS")
LEECH_RADIUS = radius("ULDUAR_VEZAX_MARK_LEECH_RADIUS")
ANCHOR = anchor("ULDUAR_VEZAX_ANCHOR")

# Not constants in the source, they come out of the engine. Floor: the boss script picks a mark
# target past 15 yd of GetDistance, which takes off his 8.0 reach and the player's 1.5. Ceiling:
# ReachTargetAction's AiPlayerbot.SpellDistance of 28.5 plus the same two reaches.
BAND_FLOOR = 15.0 + 8.0 + 1.5
BAND_CEILING = 28.5 + 8.0 + 1.5

MARK_WINDOW_MS = 10000
# the last leech tick lands ~1.6 s after the aura drops
LEECH_SLACK_MS = 2000
EARLY_MS = 4000
NEAREST_FROM_MS = 2000
# one snapshot gap longer than this is a hole in the trace, not time spent anywhere
MAX_STEP_MS = 2000


def pull_end(trace: Trace) -> int:
    ends = trace.of("end")
    if ends:
        return ends[-1]["t"]
    return trace.records[-1].get("t", 0) if trace.records else 0


def boss_guid(trace: Trace) -> int | None:
    guids = sorted(guids_of_entry(trace, NPC_VEZAX))
    return guids[0] if guids else None


def blocks(trace: Trace) -> dict[int, str]:
    """Latest `vezax.block` per bot. It re-emits every pull, unlike `vezax.slot`."""
    return {rec["g"]: str(rec.get("txt", "")) for rec in notes(trace, "vezax.block")}


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
    print(f"{encounter}  {outcome} at {clock(end)}  {len(trace.of('death'))} death(s)")

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


def show_boss(trace: Trace) -> None:
    print("BOSS")
    boss = boss_guid(trace)
    if boss is None:
        print("  General Vezax was never sampled")
        return

    casts = collections.Counter(rec.get("sp") for rec in trace.of("cast") if rec.get("s") == boss)
    for spell, count in casts.most_common():
        print(f"  {count:4}  {trace.spell(spell)}")

    flames = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_SEARING_FLAMES]
    print(f"\n  Searing Flames landed {len(flames)} hit(s) for {sum(r.get('a', 0) for r in flames):,}"
          f" across {casts.get(SPELL_SEARING_FLAMES, 0)} cast(s)")

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


def mark_windows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    samples = samples or Samples(trace)
    roster = roster_guids(trace)
    boss = boss_guid(trace)
    block = blocks(trace)
    marks = notes(trace, "vezax.mark")
    dodges = [rec for rec in trace.of("move") if rec.get("by") == DODGE]
    leech = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_MARK_LEECH]

    windows = []
    for guid, spans in sorted(aura_spans(trace, SPELL_MARK_OF_THE_FACELESS).items()):
        for start, stop in spans:
            stop = min(stop, start + MARK_WINDOW_MS)
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

    print(f"  {'at':>9} {'marked':14} {'role':6} {'blk':4} {'branch':9} {'ticks':>5} {'damage':>9}"
          f" {'0-4s':>5} {'near min/med':>13} {'dodgers':>7} {'boss %/s':>8}")
    for window in windows:
        near = ("-" if window["nearest_min"] is None
                else f"{window['nearest_min']:.1f}/{window['nearest_median']:.1f}")
        rate = "-" if window["rate"] is None else f"{window['rate']:+.3f}"
        print(f"  {clock(window['start']):>9} {trace.name(window['guid'])[:14]:14}"
              f" {trace.role(window['guid']):6} {window['block']:4} {window['branch'] or 'unchanged':9}"
              f" {window['ticks']:5} {window['damage']:9,} {window['early']:5} {near:>13}"
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


def crashes(trace: Trace, samples: Samples | None = None) -> list[dict]:
    samples = samples or Samples(trace)
    roster = roster_guids(trace)
    block = blocks(trace)
    dodges = [rec for rec in trace.of("move") if rec.get("by") == DODGE]
    hits = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_SHADOW_CRASH_IMPACT]

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

        out.append({
            "cast": cast,
            "impact": impact,
            "target": target,
            "block": block.get(target, "-"),
            "dodged": dodged,
            "hits": [rec for rec in hits if impact - 500 <= rec["t"] <= impact + 1500],
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
    print(f"  {'impact':>9} {'target':14} {'blk':4} {'L dodged':>9} {'R dodged':>9} {'hits':>5}")
    for row in rows:
        victims = ", ".join(trace.name(rec.get("d")) for rec in row["hits"])
        print(f"  {clock(row['impact']):>9} {trace.name(row['target'])[:14]:14} {row['block']:4}"
              f" {len(row['dodged']['L']):4} of {size['L']:<2} {len(row['dodged']['R']):4} of {size['R']:<2}"
              f" {len(row['hits']):5}  {victims}")

    both = sum(1 for row in rows if row["dodged"]["L"] and row["dodged"]["R"])
    mean = statistics.mean(len(row["dodged"]["L"]) + len(row["dodged"]["R"]) for row in rows)
    print(f"\n  {both} of {len(rows)} crashes moved both blocks, {mean:.1f} of {size['L'] + size['R']}"
          " camp members dodging on average")
    issued, left = evictions(trace, samples)
    if issued:
        print(f"  {left} of {issued} dodges issued from inside a field were aimed outside every field")


def field_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    samples = samples or Samples(trace)
    end = pull_end(trace)
    impacts = pending_impacts(trace)
    field = aura_spans(trace, SPELL_FIELD)
    cost = aura_spans(trace, SPELL_FIELD_COST)
    formation = latch_spans_for(trace, "vezax.formation", "on")
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

    animus = guids_of_entry(trace, NPC_SARONITE_ANIMUS)
    if animus:
        seen = first_seen(trace, animus)
        print(f"  Saronite Animus spawned{' at ' + clock(seen) if seen is not None else ''}: hard mode held")
    else:
        print(f"  no Saronite Animus after {len(summons)} summon(s)"
              + (": a vapor died, or the sixth summon never came" if len(summons) >= 6 else ""))


def band_rows(trace: Trace, samples: Samples | None = None) -> tuple[list[float], list[dict]]:
    samples = samples or Samples(trace)
    boss = boss_guid(trace)
    end = pull_end(trace)
    if boss is None:
        return [], []

    drift = [dist2(row[1:3], ANCHOR) for row in samples.between(boss, 0, end)]
    formation = latch_spans_for(trace, "vezax.formation", "on")
    block = blocks(trace)
    out = []
    for guid in sorted(camp(trace), key=trace.name):
        radii = []
        for _, when, _ in samples.steps(0, end):
            if not inside(formation.get(guid, []), when):
                continue
            row, him = samples.row(guid, when), samples.row(boss, when)
            if row and him and row[5] > 0:
                radii.append(dist2(row[1:3], him[1:3]))
        if radii:
            out.append({
                "guid": guid,
                "block": block.get(guid, "-"),
                "median": statistics.median(radii),
                "inside": sum(1 for r in radii if r < BAND_FLOOR) / len(radii),
                "past": sum(1 for r in radii if r > BAND_CEILING) / len(radii),
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
        print("  no camp member ever had the formation on")
        return

    print(f"\n  {'bot':14} {'role':6} {'blk':4} {'median':>7} {'<' + format(BAND_FLOOR, '.1f'):>7}"
          f" {'>' + format(BAND_CEILING, '.0f'):>7}")
    for row in rows:
        print(f"  {trace.name(row['guid'])[:14]:14} {trace.role(row['guid']):6} {row['block']:4}"
              f" {row['median']:7.1f} {row['inside'] * 100:6.1f}% {row['past'] * 100:6.1f}%")
    print(f"\n  radius from the boss where he stood, formation on only; slots sit at {CAMP_RADIUS:.1f}")


SECTIONS = (
    ("boss", "what he cast, Searing Flames, interrupters", show_boss),
    ("mark", "Mark of the Faceless windows", show_mark),
    ("crash", "Shadow Crash impacts and who dodged", show_crash),
    ("field", "field uptime, in-field casts, soak, cast hold", show_field),
    ("mana", "mana, Life Tap and what it returned", show_mana),
    ("vapors", "vapor targeting and the Animus", show_vapors),
    ("band", "each camp member's distance from the boss", show_band),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
