#!/usr/bin/env python3
"""Score a Twin Val'kyr pull from a RaidObs trace: essences, Touch, Pact, orbs, tanks and targets.

    val_kyr_twins.py <file>             every section
    val_kyr_twins.py <file> --essence   time without an essence, swaps by reason, who ate each Vortex
    val_kyr_twins.py <file> --touch     every Touch: how long it stayed up, what it cost the raid
    val_kyr_twins.py <file> --pact      every Twin's Pact: shield, break, kicks, outcome, duty, lust
    val_kyr_twins.py <file> --orbs      Unleashed hits taken or absorbed, tv.orb rules, Powering Up
    val_kyr_twins.py <file> --tanks     who each twin hit, taunts, where the twins stood
    val_kyr_twins.py <file> --targets   how long each DPS bot hit the twin its colour sends it to

Ids are listed for 10N, 25N, 10H and 25H: the Spell constructor remaps every cast to the map's
difficulty, so rows carry that difficulty's id. Interrupts, taunts and lust are matched by the spell
record's name instead, which covers every rank, the felhunter's Spell Lock and both factions' lust.

What the generic views miss here, and what this reads instead:

- **An essence taken before the trace opened has no apply row.** Aura rows start with the session
  and the raid picks its colours before the pull. A bot's first essence removal names the colour it
  held until then. A bot with no essence row at all is placed by the Surges, which skip a holder of
  their own colour's essence (`ExcludeTargetAuraSpell`), so a bot hit by one Surge only holds the
  other colour.
- **Creature auras and heals are never recorded**, so the Pact shields exist only as `tv.shield`,
  and a Pact that went through shows as a jump in the twins' shared health.
- **Touch rows carry the full amount.** The script deals the damage from before absorbs, so `ab` on a
  Touch row is shield spent, not damage prevented.
- **`tv.interrupt` is a per-bot change-only latch.** Every living bot restates it in each Pact's
  unshielded window, `none` without a ready kick, but a dead bot never does, so a bot's duty is cut
  at its death. Duty is only read inside a Pact's unshielded window.
- **A lone tank sends every DPS bot to Fjola** outside a Pact. The reader takes a `tv.tank` of `both`
  for that, so a lone human tank goes unnoticed.
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
from raidobs.geometry import anchor, dist2, frames, guids_of_entry  # noqa: E402
from raidobs.probes import emitted_keys, holder_spans, latch_spans, silent_keys  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, notes, roster_guids  # noqa: E402
from raidobs.validity import DIFFICULTY  # noqa: E402

ENCOUNTER = "val-kyr-twins"

NPC_FJOLA = 34497
NPC_EYDIS = 34496

LIGHT = "light"
DARK = "dark"
NONE = "none"
UNKNOWN = "?"

# 10N, 25N, 10H, 25H
LIGHT_ESSENCE = (65686, 67222, 67223, 67224)
DARK_ESSENCE = (65684, 67176, 67177, 67178)
LIGHT_SURGE = (65767, 67274, 67275, 67276)
DARK_SURGE = (65769, 67265, 67266, 67267)
LIGHT_VORTEX = (66046, 67206, 67207, 67208)
DARK_VORTEX = (66058, 67182, 67183, 67184)
LIGHT_VORTEX_DAMAGE = (66048, 67203, 67204, 67205)
DARK_VORTEX_DAMAGE = (66059, 67155, 67156, 67157)
LIGHT_TOUCH = (65950, 67296, 67297, 67298)
DARK_TOUCH = (66001, 67281, 67282, 67283)
LIGHT_PACT = (65876, 67306, 67307, 67308)
DARK_PACT = (65875, 67303, 67304, 67305)
UNLEASHED_LIGHT = (65795, 67238, 67239, 67240)
UNLEASHED_DARK = (65808, 67172, 67173, 67174)
POWERING_UP = (67590, 67602, 67603, 67604)
EMPOWERED_LIGHT = (65748, 67216, 67217, 67218)
EMPOWERED_DARK = (65724, 67213, 67214, 67215)


def by_colour(light, dark) -> dict[int, str]:
    return {**{spell: LIGHT for spell in light}, **{spell: DARK for spell in dark}}


ESSENCE = by_colour(LIGHT_ESSENCE, DARK_ESSENCE)
SURGE = by_colour(LIGHT_SURGE, DARK_SURGE)
VORTEX = by_colour(LIGHT_VORTEX, DARK_VORTEX)
VORTEX_DAMAGE = by_colour(LIGHT_VORTEX_DAMAGE, DARK_VORTEX_DAMAGE)
TOUCH = by_colour(LIGHT_TOUCH, DARK_TOUCH)
PACT = by_colour(LIGHT_PACT, DARK_PACT)
UNLEASHED = by_colour(UNLEASHED_LIGHT, UNLEASHED_DARK)
EMPOWERED = by_colour(EMPOWERED_LIGHT, EMPOWERED_DARK)

# tv.shield is a TwinColour written as its raw number
SHIELD_CODE = {LIGHT: "1", DARK: "2"}

INTERRUPT_NAMES = {"kick", "pummel", "shield bash", "counterspell", "wind shear", "mind freeze",
                   "spell lock"}
TAUNT_NAMES = {"taunt", "growl", "hand of reckoning", "dark command"}
LUST_NAMES = {"bloodlust", "heroism"}

ORB_DODGE = "twin valkyr dodge orb"

ARENA_CENTER = anchor("ARENA_CENTER")

# heroic casts in 6 s; the cast row's own ct wins when it has one
VORTEX_CAST_MS = 8000
VORTEX_CHANNEL_MS = 5000
VORTEX_SLACK_MS = 1000
PACT_CAST_MS = 15000
# a Pact gone from the casting column a second short of its cast time was cut, not finished
PACT_EARLY_MS = 1000
# the shield and the Pact go out in one call, but tv.shield is read on a bot's tick
SHIELD_LEAD_MS = 1000
# Pact heals 20% (normal) or 50% (heroic); raid damage never lifts shared health this far in 2 s
HEAL_JUMP_PCT = 5.0
HEAL_LOOK_MS = 2000
# the strip is written at once and the essence replacing it on the next update, a tick apart
SWAP_TICK_MS = 250
# an essence taken this close to a Touch dropping is the swap that ended it
SWAP_MATCH_MS = 500
TOUCH_DURATION_MS = 20000
# one application can reach the aura hook twice in a tick
DUPLICATE_AURA_MS = 100
# one snapshot gap longer than this is a hole in the trace, not time spent anywhere
MAX_STEP_MS = 2000
START = -(1 << 62)

SILENT_WHY = {
    "tv.shield": "no twin helper ran with a bot on this map (the value restates on every trace open)",
    "tv.interrupt": "written by every living bot while a twin casts an unshielded Pact, so no shield broke",
    "tv.orb": "written only when a bot looks for a dodge spot",
    "tv.tank": "no bot held a tank role",
}


def pull_end(trace: Trace) -> int:
    ends = trace.of("end")
    if ends:
        return ends[-1]["t"]
    return trace.records[-1].get("t", 0) if trace.records else 0


def named(trace: Trace, spell, names: set[str]) -> bool:
    return trace.spells.get(spell, "").lower() in names


def share(part: float, whole: float) -> str:
    return f"{part * 100.0 / whole:.1f}%" if whole else "-"


class Samples:
    """Every snapshot row by guid, so a view can ask for a row at a time without rescanning."""

    def __init__(self, trace: Trace):
        self.stamps: list[int] = []
        self.rows: dict[int, tuple[list[int], list[list]]] = {}
        for snap in frames(trace):
            when = snap["t"]
            self.stamps.append(when)
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

    def exact(self, guid: int, when: int):
        """The guid's row in the snapshot taken at `when`, None when that one missed it. A despawned
        twin's last row would otherwise stand in for every sample after it."""
        found = self.rows.get(guid)
        if not found:
            return None
        index = bisect.bisect_right(found[0], when)
        return found[1][index - 1] if index and found[0][index - 1] == when else None

    def track(self, guid: int, low: int, high: int) -> list[tuple[int, list]]:
        """`(t, row)` for each of the guid's rows with `low <= t <= high`."""
        found = self.rows.get(guid)
        if not found:
            return []
        start = bisect.bisect_left(found[0], low)
        stop = bisect.bisect_right(found[0], high)
        return list(zip(found[0][start:stop], found[1][start:stop]))

    def steps(self, low: int, high: int):
        """`(t, weight)` per snapshot in `[low, high)`, weighted by the gap to the next one."""
        for index, when in enumerate(self.stamps):
            if when < low or when >= high:
                continue
            nxt = self.stamps[index + 1] if index + 1 < len(self.stamps) else when
            yield when, min(nxt - when, MAX_STEP_MS)


def twin_guids(trace: Trace, samples: Samples) -> dict[str, int | None]:
    """Fjola and Eydis, each the guid of her entry sampled most. Every attempt summons a fresh pair,
    so an earlier one can sit in the pre-roll."""
    out: dict[str, int | None] = {}
    for name, entry in (("fjola", NPC_FJOLA), ("eydis", NPC_EYDIS)):
        seen = [(len(samples.rows.get(guid, ((), ()))[0]), -guid, guid)
                for guid in guids_of_entry(trace, entry)]
        out[name] = max(seen)[2] if seen else None
    return out


def note_track(trace: Trace, key: str) -> dict[int, tuple[list[int], list[str]]]:
    """Per bot, `(times, values)` of a per-bot key in record order."""
    out: dict[int, tuple[list[int], list[str]]] = {}
    for rec in notes(trace, key):
        times, values = out.setdefault(rec.get("g", 0), ([], []))
        times.append(rec["t"])
        values.append(str(rec.get("txt", "")))
    return out


def death_times(trace: Trace) -> dict[int, list[int]]:
    out: dict[int, list[int]] = collections.defaultdict(list)
    for rec in combat_deaths(trace):
        out[rec.get("g")].append(rec["t"])
    return out


def cut_at_death(spans: list[tuple[str, int, int]], deaths: list[int]) -> list[tuple[str, int, int]]:
    """Each span ended at the bot's first death after it opened. A dead bot never ticks, so its last
    note would otherwise run on through later windows."""
    out = []
    for value, low, high in spans:
        died = min((when for when in deaths if when >= low), default=None)
        out.append((value, low, high if died is None else min(high, died)))
    return out


def value_at(track, when: int):
    if not track:
        return None
    index = bisect.bisect_right(track[0], when)
    return track[1][index - 1] if index else None


def essence_timelines(trace: Trace) -> dict[int, tuple[list[int], list[str]]]:
    """Per roster bot, `(times, colours)` at each change of the essence it held, from START.

    The colour held before the first row is the first row's own when that row is a removal, and none
    when it is an apply (a holder's strip would come first). A bot with no row never swapped, so the
    Surges place it for the whole pull, and nothing places a bot neither Surge touched: `?`.
    """
    events: dict[int, list[tuple[int, int, str]]] = collections.defaultdict(list)
    for rec in trace.of("aura"):
        colour = ESSENCE.get(rec.get("sp"))
        if colour:
            events[rec.get("d")].append((rec["t"], 0 if rec.get("r") else 1, colour))

    surged: dict[int, set[str]] = collections.defaultdict(set)
    for rec in trace.of("dmg"):
        colour = SURGE.get(rec.get("sp"))
        if colour and rec["t"] >= 0:
            surged[rec.get("d")].add(colour)

    out = {}
    for guid in roster_guids(trace):
        mine = sorted(events.get(guid, []))
        hit = surged.get(guid, set())
        if mine:
            initial = mine[0][2] if mine[0][1] == 0 else NONE
        elif len(hit) == 2:
            initial = NONE
        elif LIGHT in hit:
            initial = DARK
        elif DARK in hit:
            initial = LIGHT
        else:
            initial = UNKNOWN

        times, colours = [START], [initial]
        for when, applied, colour in mine:
            if applied:
                now = colour
            elif colour == colours[-1]:
                now = NONE
            else:
                continue
            if now == colours[-1]:
                continue
            if colours[-1] == NONE and len(times) > 1 and when - times[-1] <= SWAP_TICK_MS:
                times.pop()
                colours.pop()
                if now == colours[-1]:
                    continue
            times.append(when)
            colours.append(now)
        out[guid] = (times, colours)
    return out


def colour_at(timeline, when: int) -> str:
    times, colours = timeline
    return colours[bisect.bisect_right(times, when) - 1]


# ------------------------------------------------------------------------------------------ essence

def essence_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per roster bot: its colour at the pull and at the end, living combat time with no essence, and
    each swap counted under the `tv.essence` reason it wanted at the time (`other` when the colour it
    took is not the one it wanted, `-` with no note yet)."""
    samples = samples or Samples(trace)
    end = pull_end(trace)
    timelines = essence_timelines(trace)
    wanted = note_track(trace, "tv.essence")

    out = []
    for guid in sorted(roster_guids(trace), key=trace.name):
        timeline = timelines[guid]
        times, colours = timeline
        swaps: collections.Counter = collections.Counter()
        for when, colour in zip(times[1:], colours[1:]):
            if colour == NONE:
                continue
            want = value_at(wanted.get(guid), when)
            if want is None:
                swaps["-"] += 1
                continue
            reason, _, want_colour = want.partition(":")
            swaps[reason if want_colour == colour else "other"] += 1

        bare = None
        if colours != [UNKNOWN]:
            bare = 0
            for when, weight in samples.steps(0, end):
                row = samples.exact(guid, when)
                if row and row[5] > 0 and colour_at(timeline, when) == NONE:
                    bare += weight
        out.append({
            "guid": guid,
            "start": colour_at(timeline, 0),
            "end": colour_at(timeline, end),
            "bare": bare,
            "swaps": swaps,
        })
    return out


def vortexes(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Every Vortex cast: how many living bots held its colour when the channel opened, of those
    whose colour is known, and each bot's unabsorbed hits and damage from it."""
    samples = samples or Samples(trace)
    roster = roster_guids(trace)
    timelines = essence_timelines(trace)
    hits = [rec for rec in trace.of("dmg")
            if rec.get("sp") in VORTEX_DAMAGE and rec.get("a", 0) > 0 and rec.get("d") in roster]

    out = []
    for rec in trace.of("cast"):
        colour = VORTEX.get(rec.get("sp"))
        if not colour or rec.get("tr"):
            continue
        start = rec["t"]
        channel = start + (rec.get("ct") or VORTEX_CAST_MS)
        stop = channel + VORTEX_CHANNEL_MS + VORTEX_SLACK_MS

        matched = known = 0
        for guid in roster:
            row = samples.row(guid, channel)
            held = colour_at(timelines[guid], channel)
            if not row or row[5] <= 0 or held == UNKNOWN:
                continue
            known += 1
            matched += held == colour

        taken: collections.Counter = collections.Counter()
        damage: collections.Counter = collections.Counter()
        for hit in hits:
            if VORTEX_DAMAGE[hit["sp"]] == colour and start <= hit["t"] <= stop:
                taken[hit["d"]] += 1
                damage[hit["d"]] += hit["a"]
        out.append({"t": start, "caster": rec.get("s"), "colour": colour, "matched": matched,
                    "known": known, "hits": taken, "damage": damage})
    return out


def show_essence(trace: Trace) -> None:
    print("ESSENCE")
    samples = Samples(trace)
    every = essence_rows(trace, samples)
    rows = [row for row in every if row["bare"] is not None]
    if not rows:
        print("  no essence or Surge rows")
        return

    reasons = ["touch", "vortex", "shield", "base", "other", "-"]
    print(f"  {'bot':14} {'role':6} {'at pull':7} {'at end':6} {'no ess':>7}"
          + "".join(f" {reason:>6}" for reason in reasons))
    for row in rows:
        print(f"  {trace.name(row['guid'])[:14]:14} {trace.role(row['guid']):6} {row['start']:7}"
              f" {row['end']:6} {row['bare'] / 1000:6.1f}s"
              + "".join(f" {row['swaps'].get(reason, 0):6}" for reason in reasons))
    unplaced = [row["guid"] for row in every if row["bare"] is None]
    if unplaced:
        print(f"  no essence row and no Surge hit, colour unknown: {', '.join(map(trace.name, unplaced))}")
    print("  no ess is living time from the pull on holding neither essence; swaps sit under the"
          " tv.essence reason held when the essence landed")

    print("\n  VORTEX")
    casts = vortexes(trace, samples)
    if not casts:
        print("  no Vortex cast")
        return
    for cast in casts:
        by_role: dict[str, list[int]] = collections.defaultdict(lambda: [0, 0, 0])
        for guid, count in cast["hits"].items():
            tally = by_role[trace.role(guid)]
            tally[0] += 1
            tally[1] += count
            tally[2] += cast["damage"][guid]
        eaten = "  ".join(f"{role} {bots} bot(s) {count} hit(s) {damage:,}"
                          for role, (bots, count, damage) in sorted(by_role.items())) or "nobody"
        names = ", ".join(trace.name(guid) for guid in sorted(cast["hits"], key=trace.name))
        print(f"  {clock(cast['t']):>9} {cast['colour']:5} {trace.name(cast['caster'])[:16]:16}"
              f" matched {cast['matched']}/{cast['known']}  unabsorbed: {eaten}"
              + (f"  ({names})" if names else ""))


# -------------------------------------------------------------------------------------------- touch

def touches(trace: Trace) -> list[dict]:
    """Every Touch aura on a player, apply to removal, with what ended it and the raid's Touch damage
    of that colour meanwhile. A Touch already up when the trace opened has no start and is skipped."""
    end = pull_end(trace)
    opened: dict[tuple[int, int], int] = {}
    spans = []
    for rec in trace.of("aura"):
        if rec.get("sp") not in TOUCH:
            continue
        key = (rec.get("d"), rec["sp"])
        if rec.get("r"):
            if key in opened:
                spans.append((key, opened.pop(key), rec["t"]))
        elif key not in opened:
            opened[key] = rec["t"]
    spans += [(key, start, None) for key, start in opened.items()]

    taken = collections.defaultdict(list)
    for rec in trace.of("aura"):
        if rec.get("sp") in ESSENCE and not rec.get("r"):
            taken[(rec.get("d"), ESSENCE[rec["sp"]])].append(rec["t"])
    deaths = death_times(trace)
    ticks = [rec for rec in trace.of("dmg") if rec.get("sp") in TOUCH]

    out = []
    for (guid, spell), start, stop in sorted(spans, key=lambda span: span[1]):
        colour = TOUCH[spell]
        close = end if stop is None else stop
        if stop is None:
            ended = "pull end"
        elif any(abs(when - stop) <= SWAP_MATCH_MS for when in taken[(guid, colour)]):
            ended = "swap"
        elif any(abs(when - stop) <= SWAP_MATCH_MS for when in deaths[guid]):
            ended = "death"
        else:
            ended = "expired" if stop - start >= TOUCH_DURATION_MS - SWAP_MATCH_MS else "stripped"
        mine = [rec for rec in ticks if TOUCH[rec["sp"]] == colour and start <= rec["t"] <= close]
        out.append({
            "t": start,
            "guid": guid,
            "colour": colour,
            "held": close - start,
            "ended": ended,
            "ticks": len(mine),
            "victims": len({rec.get("d") for rec in mine}),
            "damage": sum(rec.get("a", 0) for rec in mine),
            "spent": sum(rec.get("ab", 0) for rec in mine),
        })
    return out


def show_touch(trace: Trace) -> None:
    print("TOUCH")
    rows = touches(trace)
    if not rows:
        heroic = trace.header.get("diff") in (2, 3)
        print("  nobody was touched" + ("" if heroic else " (heroic only)"))
        return

    print(f"  {'at':>9} {'touched':14} {'role':6} {'colour':6} {'held':>6} {'ended':8} {'ticks':>5}"
          f" {'victims':>7} {'damage':>9} {'ab spent':>9}")
    for row in rows:
        print(f"  {clock(row['t']):>9} {trace.name(row['guid'])[:14]:14} {trace.role(row['guid']):6}"
              f" {row['colour']:6} {row['held'] / 1000:5.1f}s {row['ended']:8} {row['ticks']:5}"
              f" {row['victims']:7} {row['damage']:9,} {row['spent']:9,}")
    total = sum(row["damage"] for row in rows)
    print(f"\n  {total:,} raid damage from {len(rows)} Touch(es), median held"
          f" {statistics.median(row['held'] for row in rows) / 1000:.1f} s; the script deals the"
          " amount before absorbs, so ab spent is shield used up, not damage saved")


# --------------------------------------------------------------------------------------------- pact

def pact_end(samples: Samples, twin: int, start: int, length: int) -> tuple[int | None, bool]:
    """When the twin's casting column let go of the Pact, and whether it ever showed it. None with
    True means it still showed the Pact when her rows ran out."""
    seen = False
    for when, row in samples.track(twin, start + 1, start + length + HEAL_LOOK_MS):
        if (row[10] if len(row) > 10 else 0) in PACT:
            seen = True
        elif seen:
            return when, True
    return None, seen


def health_jump(samples: Samples, twin: int, stop: int) -> float | None:
    before = samples.row(twin, stop - 1)
    after = [row[5] for _, row in samples.track(twin, stop, stop + HEAL_LOOK_MS)]
    if not before or not after:
        return None
    return max(after) - before[5]


def pacts(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Every Twin's Pact cast. `down` before `close` is a broken shield, and only then does anything
    count as landing after the break."""
    samples = samples or Samples(trace)
    end = pull_end(trace)
    shields = latch_spans(trace, "tv.shield", end)
    deaths = death_times(trace)
    duty = {guid: cut_at_death(spans, deaths.get(guid, []))
            for guid, spans in holder_spans(trace, "tv.interrupt", end).items()}
    casts = [rec for rec in trace.of("cast") if not rec.get("tr")]

    out = []
    for rec in casts:
        colour = PACT.get(rec.get("sp"))
        if not colour:
            continue
        twin, start = rec.get("s"), rec["t"]
        length = rec.get("ct") or PACT_CAST_MS
        stop, seen = pact_end(samples, twin, start, length)
        close = stop if stop is not None else min(start + length, end)

        up = down = None
        for value, low, high in shields:
            if value == SHIELD_CODE[colour] and start - SHIELD_LEAD_MS <= low <= start + length:
                up, down = low, high
                break
        broke = down is not None and down < close

        jump = health_jump(samples, twin, stop) if stop is not None else None
        last = samples.row(twin, stop) if stop is not None else None
        if not seen:
            outcome = "unsampled"
        elif stop is None:
            outcome = "open"
        elif last and last[5] <= 0:
            outcome = "died"
        elif jump is not None and jump >= HEAL_JUMP_PCT:
            outcome = "healed"
        elif stop - start < length - PACT_EARLY_MS:
            outcome = "kicked"
        else:
            outcome = "full"

        kicks = [cast for cast in casts if cast.get("tgt") == twin and start <= cast["t"] <= close
                 and named(trace, cast.get("sp"), INTERRUPT_NAMES)]
        out.append({
            "t": start,
            "twin": twin,
            "colour": colour,
            "close": close,
            "up": up,
            "down": down,
            "broke": broke,
            "outcome": outcome,
            "jump": jump,
            "shielded": [cast for cast in kicks if not broke or cast["t"] < down],
            "after": [cast for cast in kicks if broke and cast["t"] >= down],
            "duty": sorted({guid for guid, spans in duty.items() for value, low, high in spans
                            if broke and value == "duty" and low < close and high > down}),
            "lust": [cast for cast in casts if start <= cast["t"] <= close
                     and named(trace, cast.get("sp"), LUST_NAMES)],
        })
    return out


def show_pact(trace: Trace) -> None:
    print("TWIN'S PACT")
    rows = pacts(trace)
    if not rows:
        print("  no Pact cast")
        return

    def rel(when, start):
        return "-" if when is None else f"{(when - start) / 1000:+.1f}"

    print(f"  {'at':>9} {'twin':16} {'up':>5} {'down':>6} {'cast':>5} {'outcome':9} {'hp':>6}"
          f" {'kicks on shield':>15} {'after':>5}  duty / lust")
    for row in rows:
        jump = "-" if row["jump"] is None else f"{row['jump']:+.1f}"
        down = rel(row["down"], row["t"]) + ("" if row["broke"] else "*")
        after = ", ".join(trace.name(cast.get("s")) for cast in row["after"])
        lust = ", ".join(f"{trace.name(cast.get('s'))} {rel(cast['t'], row['t'])}" for cast in row["lust"])
        print(f"  {clock(row['t']):>9} {trace.name(row['twin'])[:16]:16} {rel(row['up'], row['t']):>5}"
              f" {down:>6} {(row['close'] - row['t']) / 1000:5.1f} {row['outcome']:9} {jump:>6}"
              f" {len(row['shielded']):15} {len(row['after']):5}"
              f"  duty {', '.join(map(trace.name, row['duty'])) or 'nobody'}"
              + (f"; kicked by {after}" if after else "") + (f"; lust {lust}" if lust else ""))

    outcomes = collections.Counter(row["outcome"] for row in rows)
    broken = [row["down"] - row["t"] for row in rows if row["broke"]]
    print("\n  " + ", ".join(f"{count} {outcome}" for outcome, count in outcomes.most_common())
          + f"; {len(broken)} shield(s) broke"
          + (f", median {statistics.median(broken) / 1000:.1f} s into the cast" if broken else ""))
    print(f"  kicks on the shield {sum(len(row['shielded']) for row in rows)} (wasted),"
          f" after the break {sum(len(row['after']) for row in rows)}")
    windows = [(row["t"], row["close"]) for row in rows]
    for cast in trace.of("cast"):
        if not cast.get("tr") and named(trace, cast.get("sp"), LUST_NAMES):
            inside = any(low <= cast["t"] <= high for low, high in windows)
            print(f"  {trace.spell(cast['sp'])} by {trace.name(cast.get('s'))} at {clock(cast['t'])}"
                  f" {'inside a Pact' if inside else 'outside every Pact'}")
    print("  up/down are s from the cast off tv.shield, * a shield that outlasted the cast;"
          " hp is shared health's jump after it")


# --------------------------------------------------------------------------------------------- orbs

def orb_rows(trace: Trace) -> list[dict]:
    """Per bot: Unleashed hits taken (`a` > 0) and absorbed, the damage taken, peak Powering Up
    stacks, Empowered applications, accepted orb dodges."""
    roster = roster_guids(trace)
    rows: dict[int, dict] = collections.defaultdict(lambda: {"taken": 0, "absorbed": 0, "damage": 0,
                                                             "peak": 0, "empowered": 0, "dodges": 0})
    for rec in trace.of("dmg"):
        if rec.get("sp") in UNLEASHED and rec.get("d") in roster:
            row = rows[rec["d"]]
            if rec.get("a", 0) > 0:
                row["taken"] += 1
                row["damage"] += rec["a"]
            else:
                row["absorbed"] += 1

    last: dict[tuple[int, int], int] = {}
    for rec in trace.of("aura"):
        guid = rec.get("d")
        if guid not in roster or rec.get("r"):
            continue
        if rec.get("sp") in POWERING_UP:
            rows[guid]["peak"] = max(rows[guid]["peak"], rec.get("st", 1))
        elif rec.get("sp") in EMPOWERED:
            key = (guid, rec["sp"])
            if key not in last or rec["t"] - last[key] > DUPLICATE_AURA_MS:
                rows[guid]["empowered"] += 1
            last[key] = rec["t"]

    for rec in trace.of("move"):
        if rec.get("by") == ORB_DODGE and rec.get("ok") and rec.get("g") in roster:
            rows[rec["g"]]["dodges"] += 1
    return [{"guid": guid, **row} for guid, row in sorted(rows.items(), key=lambda item: trace.name(item[0]))]


def orb_rules(trace: Trace) -> dict[str, set[int]]:
    """Per `tv.orb` rule, the bots that ever held it. The key is change-only, so its rows count rule
    switches, not dodges; the `move` rows count those."""
    out: dict[str, set[int]] = collections.defaultdict(set)
    for rec in notes(trace, "tv.orb"):
        out[str(rec.get("txt", ""))].add(rec.get("g", 0))
    return out


def show_orbs(trace: Trace) -> None:
    print("ORBS")
    rows = orb_rows(trace)
    rules = orb_rules(trace)
    if not rows and not rules:
        print("  no Unleashed hit, Powering Up, Empowered or tv.orb row")
        return

    if rows:
        print(f"  {'bot':14} {'role':6} {'taken':>5} {'absorbed':>8} {'damage':>9} {'peak PU':>7}"
              f" {'empowered':>9} {'dodges':>6}")
        for row in rows:
            print(f"  {trace.name(row['guid'])[:14]:14} {trace.role(row['guid']):6} {row['taken']:5}"
                  f" {row['absorbed']:8} {row['damage']:9,} {row['peak']:7} {row['empowered']:9}"
                  f" {row['dodges']:6}")
        by_role: dict[str, list[int]] = collections.defaultdict(lambda: [0, 0])
        for row in rows:
            by_role[trace.role(row["guid"])][0] += row["taken"]
            by_role[trace.role(row["guid"])][1] += row["absorbed"]
        print("\n  Unleashed taken/absorbed: " + "  ".join(
            f"{role} {taken}/{absorbed}" for role, (taken, absorbed) in sorted(by_role.items())))
    print("  bots per tv.orb rule (none: found no spot): "
          + (", ".join(f"{rule} {len(bots)}" for rule, bots in sorted(rules.items())) or "no row"))


# -------------------------------------------------------------------------------------------- tanks

def assigned(spans: dict[int, list[tuple[str, int, int]]], name: str, when: int) -> set[int]:
    """Tanks whose `tv.tank` put them on this twin at `when`."""
    return {guid for guid, track in spans.items()
            for value, low, high in track if value in (name, "both") and low <= when < high}


def tank_rows(trace: Trace, samples: Samples | None = None) -> tuple[list[dict], list[float]]:
    """Per twin: time her victim was her `tv.tank` tank or any tank, of her living sampled time; her
    taunts by caster; her distance from ARENA_CENTER. Then the twins' distance apart."""
    samples = samples or Samples(trace)
    end = pull_end(trace)
    twins = twin_guids(trace, samples)
    spans = holder_spans(trace, "tv.tank", end)
    casts = [rec for rec in trace.of("cast") if not rec.get("tr")]

    out = []
    for name, twin in twins.items():
        if twin is None:
            continue
        alive = mine = tanked = 0
        radii = []
        for when, weight in samples.steps(0, end):
            row = samples.exact(twin, when)
            if not row or row[5] <= 0:
                continue
            victim = row[7] if len(row) > 7 else 0
            alive += weight
            mine += weight if victim in assigned(spans, name, when) else 0
            tanked += weight if trace.role(victim) == "tank" else 0
            radii.append(dist2(row[1:3], ARENA_CENTER))
        if not alive:
            continue
        taunts = collections.Counter(cast.get("s") for cast in casts if cast.get("tgt") == twin
                                     and named(trace, cast.get("sp"), TAUNT_NAMES))
        out.append({"name": name, "guid": twin, "alive": alive, "mine": mine, "tanked": tanked,
                    "taunts": taunts, "centre": statistics.median(radii)})

    apart = []
    if twins["fjola"] is not None and twins["eydis"] is not None:
        for when, _ in samples.steps(0, end):
            first, second = samples.exact(twins["fjola"], when), samples.exact(twins["eydis"], when)
            if first and second and first[5] > 0 and second[5] > 0:
                apart.append(dist2(first[1:3], second[1:3]))
    return out, apart


def show_tanks(trace: Trace) -> None:
    print("TANKS")
    rows, apart = tank_rows(trace)
    if not rows:
        print("  Fjola and Eydis were never sampled alive")
        return

    for row in rows:
        taunts = ", ".join(f"{trace.name(guid)} {count}" for guid, count in row["taunts"].most_common())
        print(f"  {trace.name(row['guid'])[:16]:16} on her tv.tank {share(row['mine'], row['alive']):>6}"
              f"  on a tank {share(row['tanked'], row['alive']):>6}"
              f"  from ARENA_CENTER median {row['centre']:.1f} yd"
              f"  taunts {sum(row['taunts'].values())}" + (f" ({taunts})" if taunts else ""))
    if apart:
        print(f"  twins apart: median {statistics.median(apart):.1f} yd, max {max(apart):.1f}")

    held = {guid: values[-1] for guid, (_, values) in note_track(trace, "tv.tank").items()}
    print("  tv.tank at the end: " + (", ".join(f"{trace.name(guid)} {value}" for guid, value
                                               in sorted(held.items(), key=lambda item: trace.name(item[0])))
                                     or "never written"))


# ------------------------------------------------------------------------------------------ targets

def pact_windows(trace: Trace, samples: Samples) -> list[tuple[int, int, int]]:
    return [(row["t"], row["close"], row["twin"]) for row in pacts(trace, samples)]


def lone_tank_windows(trace: Trace, end: int) -> list[tuple[int, int]]:
    """While a living tank's `tv.tank` reads `both`."""
    deaths = death_times(trace)
    return [(low, high) for guid, spans in holder_spans(trace, "tv.tank", end).items()
            for value, low, high in cut_at_death(spans, deaths.get(guid, [])) if value == "both"]


def target_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per DPS bot, living sampled time on the twin the rule names (the Pact twin during a Pact, else
    Fjola under a lone tank, else Eydis for a Light bot and Fjola for Dark or none), on the other
    twin, or on neither, off its own `snap.u[7]`. Time with its colour unknown counts nowhere."""
    samples = samples or Samples(trace)
    end = pull_end(trace)
    twins = twin_guids(trace, samples)
    fjola, eydis = twins["fjola"], twins["eydis"]
    if fjola is None or eydis is None:
        return []
    windows = pact_windows(trace, samples)
    lone = lone_tank_windows(trace, end)
    timelines = essence_timelines(trace)

    out = []
    for guid in sorted(roster_guids(trace), key=trace.name):
        if trace.role(guid) not in ("melee", "ranged"):
            continue
        tally = {"right": 0, "wrong": 0, "neither": 0, "unknown": 0}
        for when, weight in samples.steps(0, end):
            row = samples.exact(guid, when)
            if not row or row[5] <= 0:
                continue
            want = next((twin for low, high, twin in windows if low <= when < high), None)
            if want is None and any(low <= when < high for low, high in lone):
                want = fjola
            if want is None:
                held = colour_at(timelines[guid], when)
                want = None if held == UNKNOWN else eydis if held == LIGHT else fjola
            if want is None:
                tally["unknown"] += weight
                continue
            target = row[7] if len(row) > 7 else 0
            tally["right" if target == want else "wrong" if target in (fjola, eydis) else "neither"] += weight
        out.append({"guid": guid, **tally})
    return out


def show_targets(trace: Trace) -> None:
    print("TARGETS")
    rows = target_rows(trace)
    if not rows:
        print("  no DPS bot, or Fjola and Eydis were never both named")
        return

    print(f"  {'bot':14} {'role':6} {'rule twin':>9} {'other twin':>10} {'neither':>7} {'unknown':>7}")
    for row in rows:
        known = row["right"] + row["wrong"] + row["neither"]
        print(f"  {trace.name(row['guid'])[:14]:14} {trace.role(row['guid']):6}"
              f" {share(row['right'], known):>9} {share(row['wrong'], known):>10}"
              f" {share(row['neither'], known):>7} {row['unknown'] / 1000:6.1f}s")
    right = sum(row["right"] for row in rows)
    known = sum(row["right"] + row["wrong"] + row["neither"] for row in rows)
    print(f"\n  on the rule twin {share(right, known)} of DPS time; without a lone tank the wrong one"
          " halves a bot's damage and the right one adds 50%")


# ------------------------------------------------------------------------------------------- banner

def missing_probes(trace: Trace) -> list[str]:
    return [key for key, _, _ in silent_keys(emitted_keys(trace), encounter_of(trace))]


def show_banner(trace: Trace) -> None:
    end = pull_end(trace)
    outcome = trace.of("end")[-1].get("out", "?") if trace.of("end") else "no end record"
    encounter = encounter_of(trace)
    difficulty = DIFFICULTY.get(trace.header.get("diff"), "difficulty ?")
    print(f"{encounter}  {difficulty}  {outcome} at {clock(end)}  {len(combat_deaths(trace))} death(s)")

    # the probe check matches keys to the encounter, so on another boss it would pass on nothing
    if encounter != ENCOUNTER:
        print("  not a Twin Val'kyr pull, every section below reads empty")
        return

    gone = missing_probes(trace)
    if not gone:
        print("all tv.* probes present")
        return
    print(f"probes absent from this trace: {', '.join(gone)}")
    for key in gone:
        if key in SILENT_WHY:
            print(f"  {key}: {SILENT_WHY[key]}")


SECTIONS = (
    ("essence", "time without an essence, swaps by reason, Vortex damage by role", show_essence),
    ("touch", "every Touch, how long it stayed up and the raid damage meanwhile", show_touch),
    ("pact", "every Pact: shield, break, kicks, outcome, interrupt duty, lust", show_pact),
    ("orbs", "Unleashed hits taken or absorbed, tv.orb rules, Powering Up, Empowered", show_orbs),
    ("tanks", "each twin's victim against her tv.tank, taunts, distances", show_tanks),
    ("targets", "each DPS bot's time on the twin its colour names", show_targets),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
