#!/usr/bin/env python3
"""Score a Lord Jaraxxus pull from a RaidObs trace: Fel Fireball, Nether Power, Incinerate Flesh,
Legion Flame, the adds.

    lord_jaraxxus.py <file>               every section
    lord_jaraxxus.py <file> --boss        Fel Fireball landed or kicked and by whom, Fel Lightning chains
    lord_jaraxxus.py <file> --nether      Nether Power stacks over the pull, the casts that stripped them
    lord_jaraxxus.py <file> --incinerate  every Incinerate Flesh: heals on it, how it ended, Burning Inferno
    lord_jaraxxus.py <file> --flame       every Legion Flame carrier's walk and spacing, who stood in fire
    lord_jaraxxus.py <file> --adds        each add's life and victims, focus and add tank changes, the Kiss

What the generic views miss here, and what this reads instead:

- **Every difficulty id of a row counts.** ToC remaps only in the client DBC, so a 25H pull logs
  66965 for Fel Fireball, never 66532.
- **His own auras are never recorded** (`NoteAura` wants a player), so Nether Power has no `aura`
  row. `jaraxxus.netherpower` carries his stacks, and a Spellsteal that took one shows as Nether
  Power landing on the mage.
- **`jaraxxus.interrupter` is `1` only inside a Fel Fireball cast**: every bot rewrites it each tick
  he lives, so it drops to `0` as the cast ends. `--boss` names who held `1` inside each.
- **`jaraxxus.flame` is change-only per bot**: a carrier taking `carrier` on every leg writes one
  row, so branch counts are switches, not legs. Legs are the avoid action's accepted moves. A
  carrier holding him writes `tank` and clears the flames only, so the raid and boss spacing of
  a window with nothing but `tank` is not judged.
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
from raidobs.geometry import dist2, frames, guids_of_entry, radius  # noqa: E402
from raidobs.probes import emitted_keys, holder_spans, latch_spans, silent_keys  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, death_records, notes, roster_guids  # noqa: E402

ENCOUNTER = "lord-jaraxxus"
DIFFICULTIES = ("10N", "25N", "10H", "25H")

NPC_JARAXXUS = 34780
NPC_LEGION_FLAME = 34784
NPC_INFERNAL_VOLCANO = 34813
NPC_FELFLAME_INFERNAL = 34815
NPC_NETHER_PORTAL = 34825
NPC_MISTRESS_OF_PAIN = 34826
# kill order, which is also the table order
ADD_KINDS = (
    (NPC_NETHER_PORTAL, "portal"),
    (NPC_INFERNAL_VOLCANO, "volcano"),
    (NPC_MISTRESS_OF_PAIN, "mistress"),
    (NPC_FELFLAME_INFERNAL, "infernal"),
)

# spelldifficulty rows, 10N/25N/10H/25H
FEL_FIREBALL = (66532, 66963, 66964, 66965)
FEL_LIGHTNING = (66528, 67029, 67030, 67031)
INCINERATE_FLESH = (66237, 67049, 67050, 67051)
BURNING_INFERNO = (66242, 67059, 67060, 67061)
NETHER_POWER = (66228, 67106, 67107, 67108)
LEGION_FLAME = (66197, 68123, 68124, 68125)
LEGION_FLAME_TRAIL = (66199, 68126, 68127, 68128)
LEGION_FLAME_TICK = (66877, 67070, 67071, 67072)
MISTRESS_KISS = (66334, 67905, 67906, 67907)
MISTRESS_KISS_PUNISH = (66359, 67073, 67074, 67075)
MISTRESS_KISS_CAST = (66336, 67076, 67077, 67078)

# Kick, Pummel, Shield Bash, Mind Freeze, Counterspell, Wind Shear, Spell Lock, every rank
INTERRUPTS = frozenset({1766, 1767, 1768, 1769, 38768, 6552, 6554, 72, 1671, 1672, 29704, 47528, 2139, 57994,
                        19244, 19647})
# Spellsteal, Purge, Dispel Magic
NETHER_REMOVERS = frozenset({30449, 370, 8012, 527, 988})

# per difficulty, 10N/25N/10H/25H
FIREBALL_CAST_MS = (2500, 2000, 2500, 2000)
INCINERATE_MS = (15000, 15000, 12000, 12000)
NETHER_MAX = (5, 10, 5, 10)
CHAIN_CAP = (3, 5, 3, 5)
KISS_MS = 15000
BURNING_INFERNO_MS = 5000

NETHER_KEY = "jaraxxus.netherpower"
FOCUS_KEY = "jaraxxus.focus"
ADDTANK_KEY = "jaraxxus.addtank"
INTERRUPTER_KEY = "jaraxxus.interrupter"
FLAME_KEY = "jaraxxus.flame"
AVOID = "jaraxxus avoid legion flame"
KISS_HOLD = "jaraxxus kiss cast hold"

# Read from the source so a retune shows up here without a second edit.
FLAME_RADIUS = radius("JARAXXUS_LEGION_FLAME_RADIUS")
RAID_CLEAR = radius("JARAXXUS_FLAME_CARRIER_RAID_CLEAR")
BOSS_CLEAR = radius("JARAXXUS_FLAME_CARRIER_BOSS_CLEAR")

# Fel Fireball flies at 25 yd/s, so its hit trails the cast end by well under this. Its DoT shares the
# spell id, but the next cast starts 10 s or more after the last, past every tick.
LAND_SLACK_MS = 1000
# every jump of the chain lands with the cast
CHAIN_MS = 2000
# an aura removed this close to its full duration ran out
EXPIRE_SLACK_MS = 1000
# Legion Flame hands over to its trail aura in the same tick
MERGE_MS = 1000
# how long after a removal cast a lower stack count is still its doing
DROP_MS = 1500
# a sample older than this no longer places the unit
FRESH_MS = 1000
SHOWN = 12


def pull_end(trace: Trace) -> int:
    ends = trace.of("end")
    if ends:
        return ends[-1]["t"]
    return trace.records[-1].get("t", 0) if trace.records else 0


def difficulty(trace: Trace) -> int:
    diff = trace.header.get("diff", 0)
    return diff if isinstance(diff, int) and 0 <= diff < len(DIFFICULTIES) else 0


def boss_guids(trace: Trace) -> set[int]:
    return guids_of_entry(trace, NPC_JARAXXUS)


def as_guid(text) -> int:
    text = str(text)
    return int(text) if text.isdigit() else 0


def death_times(trace: Trace) -> dict[int, list[int]]:
    # every death, resets too: any of them ends an aura
    out: dict[int, list[int]] = collections.defaultdict(list)
    for death in death_records(trace):
        out[death.get("g")].append(death["t"])
    return out


def share(values, test) -> float | None:
    return sum(1 for value in values if test(value)) / len(values) if values else None


class Samples:
    """Every snapshot row by guid, so a view can ask where a unit stood at a time without rescanning."""

    def __init__(self, trace: Trace):
        self.rows: dict[int, tuple[list[int], list[list]]] = {}
        for snap in frames(trace):
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


def aura_windows(trace: Trace, spells, merge_ms: int = 0) -> list[dict]:
    """Per target, apply to remove of any id in `spells`, in time order. One target's windows that
    overlap or sit within `merge_ms` join; one still up when the file ends runs to the end, `open`."""
    end = pull_end(trace)
    opened: dict[tuple[int, int], tuple[int, int]] = {}
    spans: dict[int, list[list]] = collections.defaultdict(list)
    for rec in trace.of("aura"):
        spell = rec.get("sp")
        if spell not in spells:
            continue
        key = (rec.get("d", 0), spell)
        if rec.get("r"):
            if key in opened:
                start, dur = opened.pop(key)
                spans[key[0]].append([start, rec["t"], dur, False])
        elif key not in opened:
            opened[key] = (rec["t"], rec.get("dur", 0))
    for (guid, _), (start, dur) in opened.items():
        spans[guid].append([start, end, dur, True])

    out = []
    for guid, rows in spans.items():
        rows.sort()
        merged = [rows[0]]
        for row in rows[1:]:
            last = merged[-1]
            if row[0] > last[1] + merge_ms:
                merged.append(row)
            elif row[1] >= last[1]:
                last[1], last[3] = row[1], row[3]
        out += [{"guid": guid, "start": start, "stop": stop, "dur": dur, "open": still}
                for start, stop, dur, still in merged]
    return sorted(out, key=lambda window: (window["start"], window["guid"]))


def ending(window: dict, full_ms: int, deaths: dict[int, list[int]]) -> str:
    """`open`, `died`, `expired`, or `removed` for an aura stripped before its time."""
    if window["open"]:
        return "open"
    deaths = deaths.get(window["guid"], ())
    if any(window["start"] <= when <= window["stop"] + EXPIRE_SLACK_MS for when in deaths):
        return "died"
    if window["stop"] - window["start"] >= (window["dur"] or full_ms) - EXPIRE_SLACK_MS:
        return "expired"
    return "removed"


def missing_probes(trace: Trace) -> list[str]:
    return [key for key, _, _ in silent_keys(emitted_keys(trace), encounter_of(trace))]


def show_banner(trace: Trace) -> None:
    end = pull_end(trace)
    outcome = trace.of("end")[-1].get("out", "?") if trace.of("end") else "no end record"
    encounter = encounter_of(trace)
    print(f"{encounter} {DIFFICULTIES[difficulty(trace)]}  {outcome} at {clock(end)}"
          f"  {len(combat_deaths(trace))} death(s)")

    # the probe check matches keys to the encounter, so on another boss it would pass on nothing
    if encounter != ENCOUNTER:
        print("  not a Jaraxxus pull, every section below reads empty")
        return

    gone = missing_probes(trace)
    if not gone:
        print("all jaraxxus.* probes present")
        return
    print(f"probes absent from this trace: {', '.join(gone)}")
    if NETHER_KEY in gone:
        print(f"  {NETHER_KEY} is written on every bot's Nether Power check, so none of them ran it")


def fel_fireballs(trace: Trace) -> list[dict]:
    """Each Fel Fireball he started: whether it hit, the interrupts aimed at him inside the cast, and
    who held the interrupter latch while it was up."""
    bosses = boss_guids(trace)
    fallback = FIREBALL_CAST_MS[difficulty(trace)]
    hits = sorted(rec["t"] for rec in trace.of("dmg") if rec.get("sp") in FEL_FIREBALL)
    kicks = [rec for rec in trace.of("cast") if rec.get("tgt") in bosses and rec.get("sp") in INTERRUPTS]
    latch = holder_spans(trace, INTERRUPTER_KEY, pull_end(trace))

    out = []
    for rec in trace.of("cast"):
        if rec.get("s") not in bosses or rec.get("sp") not in FEL_FIREBALL or rec.get("tr"):
            continue
        start = rec["t"]
        stop = start + (rec.get("ct") or fallback)
        index = bisect.bisect_left(hits, start)
        landed = index < len(hits) and hits[index] <= stop + LAND_SLACK_MS
        inside = [(kick["t"], kick.get("s"), kick.get("sp")) for kick in kicks if start <= kick["t"] < stop]
        latched = [guid for guid, spans in latch.items()
                   if any(held == "1" and low < stop and high > start for held, low, high in spans)]
        out.append({
            "start": start,
            "stop": stop,
            "outcome": "landed" if landed else "kicked" if inside else "no hit",
            "kicks": inside,
            "latched": sorted(latched),
        })
    return out


def fel_lightning(trace: Trace) -> tuple[list[dict], int]:
    """Each Fel Lightning he cast with the members its chain hit, and how many hits matched no cast."""
    bosses = boss_guids(trace)
    casts = sorted(rec["t"] for rec in trace.of("cast")
                   if rec.get("s") in bosses and rec.get("sp") in FEL_LIGHTNING)
    out = [{"t": when, "victims": [], "damage": 0} for when in casts]
    stray = 0
    for rec in trace.of("dmg"):
        if rec.get("sp") not in FEL_LIGHTNING:
            continue
        index = bisect.bisect_right(casts, rec["t"]) - 1
        if index < 0 or rec["t"] - casts[index] > CHAIN_MS:
            stray += 1
            continue
        out[index]["victims"].append(rec.get("d"))
        out[index]["damage"] += rec.get("a", 0)
    return out, stray


def show_boss(trace: Trace) -> None:
    print("BOSS")
    bosses = boss_guids(trace)
    if not bosses:
        print("  Lord Jaraxxus was never named")
        return

    casts = collections.Counter(rec.get("sp") for rec in trace.of("cast")
                                if rec.get("s") in bosses and not rec.get("tr"))
    for spell, count in casts.most_common():
        print(f"  {count:4}  {trace.spell(spell)}")

    rows = fel_fireballs(trace)
    outcomes = collections.Counter(row["outcome"] for row in rows)
    print(f"\n  Fel Fireball: {len(rows)} cast(s), {outcomes['landed']} landed, {outcomes['kicked']} kicked,"
          f" {outcomes['no hit']} neither")
    if rows:
        print(f"  {'at':>9} {'outcome':8} {'kicked by':26} latched")
        for row in rows:
            kicked = ", ".join(f"{trace.name(caster)} {trace.spells.get(spell) or trace.spell(spell)}"
                               for _, caster, spell in row["kicks"]) or "-"
            latched = ", ".join(trace.name(guid) for guid in row["latched"]) or "nobody"
            print(f"  {clock(row['start']):>9} {row['outcome']:8} {kicked[:26]:26} {latched}")
    wasted = sum(len(row["kicks"]) for row in rows if row["outcome"] == "landed")
    if wasted:
        print(f"  {wasted} interrupt(s) fell inside a cast that still landed")
    unlatched = sum(1 for row in rows if not row["latched"])
    if unlatched:
        print(f"  {unlatched} cast(s) with nobody latched as interrupter")

    lightning, stray = fel_lightning(trace)
    hits = [len(row["victims"]) for row in lightning]
    if lightning:
        print(f"\n  Fel Lightning: {len(lightning)} cast(s), {sum(hits)} hit(s),"
              f" {statistics.mean(hits):.1f} a cast against a chain of {CHAIN_CAP[difficulty(trace)]}")
        print("  hits per cast: " + ", ".join(f"{count} x{times}"
                                             for count, times in sorted(collections.Counter(hits).items())))
    else:
        print("\n  no Fel Lightning cast")
    if stray:
        print(f"  {stray} Fel Lightning hit(s) matched no cast")

    samples = Samples(trace)
    health = [row[5] for guid in bosses for _, row in samples.between(guid, 0, pull_end(trace))]
    if health:
        print(f"  boss health floor {min(health):.2f}%")


def nether_timeline(trace: Trace) -> list[tuple[int, int, int]]:
    """`(stacks, start, stop)` off `jaraxxus.netherpower`, clipped to the pull."""
    end = pull_end(trace)
    out = []
    for held, start, stop in latch_spans(trace, NETHER_KEY, end):
        try:
            stacks = int(held)
        except ValueError:
            continue
        start, stop = max(start, 0), min(stop, end)
        if stop > start:
            out.append((stacks, start, stop))
    return out


def stacks_at(timeline, when: int) -> int:
    return next((stacks for stacks, start, stop in timeline if start <= when < stop), 0)


def nether_uptime(trace: Trace) -> tuple[int, int, int]:
    """ms with 1+ stacks, ms at the difficulty's full count, and that count."""
    cap = NETHER_MAX[difficulty(trace)]
    timeline = nether_timeline(trace)
    up = sum(stop - start for stacks, start, stop in timeline if stacks > 0)
    full = sum(stop - start for stacks, start, stop in timeline if stacks >= cap)
    return up, full, cap


def nether_removals(trace: Trace) -> dict[int, dict]:
    """Per caster, Spellsteal, Purge and Dispel Magic cast at him: how many, how many while he had
    stacks, and how many the stack count fell right after."""
    bosses = boss_guids(trace)
    timeline = nether_timeline(trace)
    out: dict[int, dict] = {}
    for rec in trace.of("cast"):
        if rec.get("sp") not in NETHER_REMOVERS or rec.get("tgt") not in bosses or rec.get("tr"):
            continue
        row = out.setdefault(rec.get("s"), {"casts": 0, "up": 0, "dropped": 0,
                                            "spells": collections.Counter()})
        row["casts"] += 1
        row["spells"][rec["sp"]] += 1
        before = stacks_at(timeline, rec["t"])
        if not before:
            continue
        row["up"] += 1
        if any(rec["t"] < start <= rec["t"] + DROP_MS and stacks < before for stacks, start, _ in timeline):
            row["dropped"] += 1
    return out


def nether_steals(trace: Trace) -> dict[int, int]:
    """Nether Power windows on raid members, which only a Spellsteal puts there."""
    roster = roster_guids(trace)
    return dict(collections.Counter(window["guid"] for window in aura_windows(trace, NETHER_POWER)
                                    if window["guid"] in roster))


def show_nether(trace: Trace) -> None:
    print("NETHER POWER")
    bosses = boss_guids(trace)
    casts = [rec["t"] for rec in trace.of("cast") if rec.get("s") in bosses and rec.get("sp") in NETHER_POWER]
    print(f"  {len(casts)} cast(s) by him" + (": " + ", ".join(clock(when) for when in casts[:SHOWN])
                                              if casts else ""))

    timeline = nether_timeline(trace)
    if timeline:
        up, full, cap = nether_uptime(trace)
        print(f"  stacks up {up / 1000:.1f} s, at the full {cap} {full / 1000:.1f} s")
        steps = [f"{clock(start)} {stacks}" for stacks, start, _ in timeline]
        print("  " + " -> ".join(steps[:SHOWN])
              + (f" and {len(steps) - SHOWN} more" if len(steps) > SHOWN else ""))
    else:
        print(f"  {NETHER_KEY} never written, so his stacks are unknown")

    removals = nether_removals(trace)
    if removals:
        print(f"\n  {'caster':14} {'role':6} {'casts':>5} {'up':>4} {'took':>5}  spells")
        for guid, row in sorted(removals.items(), key=lambda item: -item[1]["casts"]):
            spells = ", ".join(f"{trace.spell(spell)} x{count}"
                               for spell, count in row["spells"].most_common())
            print(f"  {trace.name(guid)[:14]:14} {trace.role(guid):6} {row['casts']:5} {row['up']:4}"
                  f" {row['dropped']:5}  {spells}")
        print("  up is casts made while he had stacks, took is those the count fell within"
              f" {DROP_MS / 1000:.1f} s of")
    else:
        print("  nobody cast Spellsteal, Purge or Dispel Magic at him")

    steals = nether_steals(trace)
    if steals:
        print("  Nether Power landed on: " + ", ".join(f"{trace.name(guid)} x{count}"
                                                      for guid, count in steals.items()))


def incinerate_windows(trace: Trace) -> list[dict]:
    """Each Incinerate Flesh: its target, how it ended, the heals landed on the target inside it, and
    the Burning Inferno damage right after it."""
    full = INCINERATE_MS[difficulty(trace)]
    deaths = death_times(trace)
    heals = trace.of("heal")
    inferno = [rec for rec in trace.of("dmg") if rec.get("sp") in BURNING_INFERNO]

    out = []
    for window in aura_windows(trace, INCINERATE_FLESH):
        guid, start, stop = window["guid"], window["start"], window["stop"]
        landed = [rec for rec in heals if rec.get("d") == guid and start <= rec["t"] <= stop]
        after = [rec for rec in inferno if stop <= rec["t"] <= stop + BURNING_INFERNO_MS + LAND_SLACK_MS]
        ended = ending(window, full, deaths)
        out.append({
            "guid": guid,
            "start": start,
            "stop": stop,
            "ended": "healed" if ended == "removed" else ended,
            "heals": len(landed),
            "healed": sum(rec.get("a", 0) for rec in landed),
            "healers": len({rec.get("s") for rec in landed}),
            "inferno": sum(rec.get("a", 0) for rec in after),
            "inferno_hits": len(after),
        })
    return out


def show_incinerate(trace: Trace) -> None:
    print("INCINERATE FLESH")
    rows = incinerate_windows(trace)
    if not rows:
        print("  nobody carried it")
        return

    print(f"  {'at':>9} {'target':14} {'role':6} {'ended':8} {'len':>5} {'heals':>5} {'amount':>9}"
          f" {'healers':>7} {'inferno':>9}")
    for row in rows:
        print(f"  {clock(row['start']):>9} {trace.name(row['guid'])[:14]:14} {trace.role(row['guid']):6}"
              f" {row['ended']:8} {(row['stop'] - row['start']) / 1000:5.1f} {row['heals']:5}"
              f" {row['healed']:9,} {row['healers']:7} {row['inferno']:9,}")
    ended = collections.Counter(row["ended"] for row in rows)
    print(f"\n  {ended['healed']} healed through, {ended['expired']} ran out, {ended['died']} died with it;"
          f" Burning Inferno {sum(row['inferno'] for row in rows):,} over"
          f" {sum(row['inferno_hits'] for row in rows)} hit(s)")
    print("  inferno is its damage in the 6 s after the window closed")


def carrier_windows(trace: Trace, samples: Samples | None = None, raid_clear: float = RAID_CLEAR,
                    boss_clear: float = BOSS_CLEAR) -> list[dict]:
    """Each Legion Flame carrier, debuff and trail as one window: distance walked, legs issued, the
    nearest other member and the boss over the trail, and the `jaraxxus.flame` switches inside it."""
    samples = samples or Samples(trace)
    roster = roster_guids(trace)
    bosses = boss_guids(trace)
    branches = notes(trace, FLAME_KEY)
    legs = [rec for rec in trace.of("move") if rec.get("by") == AVOID and rec.get("ok")]

    out = []
    for window in aura_windows(trace, LEGION_FLAME + LEGION_FLAME_TRAIL, MERGE_MS):
        guid, start, stop = window["guid"], window["start"], window["stop"]
        taken = collections.Counter(str(rec.get("txt", "")) for rec in branches
                                    if rec.get("g") == guid and start <= rec["t"] <= stop)
        track = [(when, row) for when, row in samples.between(guid, start, stop) if row[5] > 0]
        near, him = [], []
        for when, row in track:
            others = (samples.row(ally, when, FRESH_MS) for ally in roster if ally != guid)
            gaps = [dist2(row[1:3], other[1:3]) for other in others if other and other[5] > 0]
            if gaps:
                near.append(min(gaps))
            spots = (samples.row(boss, when, FRESH_MS) for boss in bosses)
            gaps = [dist2(row[1:3], spot[1:3]) for spot in spots if spot and spot[5] > 0]
            if gaps:
                him.append(min(gaps))
        out.append({
            "guid": guid,
            "start": start,
            "stop": stop,
            "walked": sum(dist2(a[1][1:3], b[1][1:3]) for a, b in zip(track, track[1:])),
            "legs": sum(1 for rec in legs if rec.get("g") == guid and start <= rec["t"] <= stop),
            "near_min": min(near) if near else None,
            "near_share": share(near, lambda gap: gap < raid_clear),
            "boss_min": min(him) if him else None,
            "boss_share": share(him, lambda gap: gap < boss_clear),
            "branches": taken,
            # a tank carrier drags him along, so the spacing it never kept is no verdict
            "tank": set(taken) == {"tank"},
        })
    return out


def flame_branches(trace: Trace) -> collections.Counter:
    return collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, FLAME_KEY))


def fire_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per bot, the Legion Flame ticks it took, how many while it carried, and how far the nearest
    sampled flame stood at each tick."""
    samples = samples or Samples(trace)
    flames = guids_of_entry(trace, NPC_LEGION_FLAME)
    carrying: dict[int, list[tuple[int, int]]] = collections.defaultdict(list)
    for window in aura_windows(trace, LEGION_FLAME + LEGION_FLAME_TRAIL, MERGE_MS):
        carrying[window["guid"]].append((window["start"], window["stop"]))

    rows: dict[int, dict] = {}
    for rec in trace.of("dmg"):
        if rec.get("sp") not in LEGION_FLAME_TICK:
            continue
        guid = rec.get("d")
        row = rows.setdefault(guid, {"guid": guid, "hits": 0, "damage": 0, "carrying": 0, "gaps": []})
        row["hits"] += 1
        row["damage"] += rec.get("a", 0)
        if any(start <= rec["t"] <= stop for start, stop in carrying.get(guid, ())):
            row["carrying"] += 1
        mine = samples.row(guid, rec["t"], FRESH_MS)
        spots = (samples.row(flame, rec["t"], FRESH_MS) for flame in flames)
        gaps = [dist2(mine[1:3], spot[1:3]) for spot in spots if spot] if mine else []
        if gaps:
            row["gaps"].append(min(gaps))
    return sorted(rows.values(), key=lambda row: -row["damage"])


def show_flame(trace: Trace) -> None:
    print("LEGION FLAME")
    samples = Samples(trace)
    rows = carrier_windows(trace, samples)
    if rows:
        print(f"  {'at':>9} {'carrier':14} {'role':6} {'len':>5} {'walked':>6} {'legs':>4}"
              f" {'near min':>8} {'<' + format(RAID_CLEAR, '.0f'):>5} {'boss min':>8}"
              f" {'<' + format(BOSS_CLEAR, '.0f'):>5}  branches")

        def gap(value):
            return "-" if value is None else f"{value:.1f}"

        def pct(value):
            return "-" if value is None else f"{value * 100:.0f}%"

        for row in rows:
            taken = ", ".join(f"{name} {count}" for name, count in row["branches"].most_common()) or "-"
            near = "-" if row["tank"] else pct(row["near_share"])
            boss = "-" if row["tank"] else pct(row["boss_share"])
            print(f"  {clock(row['start']):>9} {trace.name(row['guid'])[:14]:14} {trace.role(row['guid']):6}"
                  f" {(row['stop'] - row['start']) / 1000:5.1f} {row['walked']:6.1f} {row['legs']:4}"
                  f" {gap(row['near_min']):>8} {near:>5} {gap(row['boss_min']):>8}"
                  f" {boss:>5}  {taken}")
        print(f"  near is the nearest other member, boss is him; a carrier's spot keeps {RAID_CLEAR:.0f}"
              f" and {BOSS_CLEAR:.0f} yd,")
        print("  the % columns are samples inside those, - for a tank carrier, which keeps clear of flames only")
    else:
        print("  nobody carried it")

    taken = flame_branches(trace)
    if taken:
        print(f"  {FLAME_KEY} switches: "
              + ", ".join(f"{name} {count}" for name, count in taken.most_common()))

    fire = fire_rows(trace, samples)
    if not fire:
        print("  nobody took a flame tick")
        return
    print(f"\n  {'stood in fire':14} {'role':6} {'hits':>5} {'damage':>9} {'carrying':>8} {'flame yd':>8}")
    for row in fire:
        spot = f"{statistics.median(row['gaps']):.1f}" if row["gaps"] else "-"
        print(f"  {trace.name(row['guid'])[:14]:14} {trace.role(row['guid']):6} {row['hits']:5}"
              f" {row['damage']:9,} {row['carrying']:8} {spot:>8}")
    print(f"  flame yd is the median distance to the nearest sampled flame at a tick; a tick lands"
          f" inside {FLAME_RADIUS:.0f} yd")


def add_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per add unit in kill order: first and last sample, health at the last, and the share of its
    living samples with a victim that is not a tank."""
    samples = samples or Samples(trace)
    out = []
    for entry, kind in ADD_KINDS:
        for guid in sorted(guids_of_entry(trace, entry)):
            found = samples.rows.get(guid)
            if not found:
                out.append({"guid": guid, "kind": kind, "first": None, "last": None, "hp": None,
                            "share": None})
                continue
            stamps, rows = found
            victims = [row[7] for row in rows if row[5] > 0 and len(row) > 7 and row[7]]
            out.append({
                "guid": guid,
                "kind": kind,
                "first": stamps[0],
                "last": stamps[-1],
                "hp": rows[-1][5],
                "share": share(victims, lambda victim: trace.role(victim) != "tank"),
            })
    return out


def focus_changes(trace: Trace) -> list[tuple[int, int]]:
    """`(t, add)` at each change of the raid's focus add, 0 for none."""
    return [(start, as_guid(held)) for held, start, _ in latch_spans(trace, FOCUS_KEY)]


def addtank_changes(trace: Trace) -> dict[int, list[tuple[int, int]]]:
    """Per assist tank, `(t, add)` at each change of the add it holds, 0 for none."""
    return {tank: [(start, as_guid(held)) for held, start, _ in spans]
            for tank, spans in holder_spans(trace, ADDTANK_KEY).items()}


def kiss_windows(trace: Trace) -> list[dict]:
    """Each Mistress' Kiss: its target, how it ended, the punish damage, and the kiss cast hold vetoes
    on the target inside it."""
    deaths = death_times(trace)
    punish = [rec for rec in trace.of("dmg") if rec.get("sp") in MISTRESS_KISS_PUNISH]
    vetoes = [rec for rec in trace.of("veto") if rec.get("m") == KISS_HOLD]

    out = []
    for window in aura_windows(trace, MISTRESS_KISS):
        guid, start, stop = window["guid"], window["start"], window["stop"]
        hit = [rec for rec in punish if rec.get("d") == guid and start <= rec["t"] <= stop + EXPIRE_SLACK_MS]
        out.append({
            "guid": guid,
            "start": start,
            "stop": stop,
            "ended": "punished" if hit else ending(window, KISS_MS, deaths),
            "punish": sum(rec.get("a", 0) for rec in hit),
            "vetoes": sum(1 for rec in vetoes if rec.get("g") == guid and start <= rec["t"] <= stop),
        })
    return out


def kiss_punishes(trace: Trace) -> tuple[dict[int, tuple[int, int]], int]:
    """Per victim, `(hits, damage)` of the kiss punish, and how many hits fell outside every kiss."""
    windows = aura_windows(trace, MISTRESS_KISS)
    out: dict[int, list[int]] = collections.defaultdict(lambda: [0, 0])
    stray = 0
    for rec in trace.of("dmg"):
        if rec.get("sp") not in MISTRESS_KISS_PUNISH:
            continue
        guid = rec.get("d")
        out[guid][0] += 1
        out[guid][1] += rec.get("a", 0)
        if not any(window["guid"] == guid and window["start"] <= rec["t"] <= window["stop"] + EXPIRE_SLACK_MS
                   for window in windows):
            stray += 1
    return {guid: (hits, damage) for guid, (hits, damage) in out.items()}, stray


def show_adds(trace: Trace) -> None:
    print("ADDS")
    rows = add_rows(trace)
    if rows:
        print(f"  {'kind':8} {'unit':20} {'first':>9} {'last':>9} {'hp end':>6} {'on non-tank':>11}")
        for row in rows:
            first = clock(row["first"]) if row["first"] is not None else "-"
            last = clock(row["last"]) if row["last"] is not None else "-"
            health = f"{row['hp']:.0f}%" if row["hp"] is not None else "-"
            held = f"{row['share'] * 100:.0f}%" if row["share"] is not None else "-"
            print(f"  {row['kind']:8} {trace.name(row['guid'])[:20]:20} {first:>9} {last:>9} {health:>6}"
                  f" {held:>11}")
        print("  on non-tank is the share of its living samples whose victim was not a tank")
    else:
        print("  no add was named")

    def named(guid):
        return trace.name(guid) if guid else "none"

    changes = focus_changes(trace)
    if changes:
        steps = [f"{clock(when)} {named(guid)}" for when, guid in changes]
        print(f"\n  {FOCUS_KEY}: " + " -> ".join(steps[:SHOWN])
              + (f" and {len(steps) - SHOWN} more" if len(steps) > SHOWN else ""))
    for tank, held in sorted(addtank_changes(trace).items(), key=lambda item: trace.name(item[0])):
        steps = [f"{clock(when)} {named(guid)}" for when, guid in held]
        print(f"  {ADDTANK_KEY} {trace.name(tank)}: " + " -> ".join(steps[:SHOWN])
              + (f" and {len(steps) - SHOWN} more" if len(steps) > SHOWN else ""))

    casts = sum(1 for rec in trace.of("cast") if rec.get("sp") in MISTRESS_KISS_CAST)
    kisses = kiss_windows(trace)
    punished, stray = kiss_punishes(trace)
    if not kisses and not casts and not punished:
        return
    print(f"\n  Mistress' Kiss: {casts} cast(s), {len(kisses)} landed")
    if kisses:
        print(f"  {'at':>9} {'target':14} {'role':6} {'ended':8} {'len':>5} {'punish':>8} {'vetoes':>6}")
        for row in kisses:
            print(f"  {clock(row['start']):>9} {trace.name(row['guid'])[:14]:14} {trace.role(row['guid']):6}"
                  f" {row['ended']:8} {(row['stop'] - row['start']) / 1000:5.1f} {row['punish']:8,}"
                  f" {row['vetoes']:6}")
        print("  vetoes is the kiss cast hold zeroing a cast on the target while kissed")
    if stray:
        print(f"  {stray} punish hit(s) outside every kiss window")


SECTIONS = (
    ("boss", "Fel Fireball landed or kicked and by whom, Fel Lightning hits per cast", show_boss),
    ("nether", "Nether Power stacks and the casts that stripped them", show_nether),
    ("incinerate", "Incinerate Flesh windows, heals on the target, Burning Inferno after", show_incinerate),
    ("flame", "Legion Flame carriers' walk and spacing, who stood in fire", show_flame),
    ("adds", "add lifetimes and victims, focus and add tank changes, Mistress' Kiss", show_adds),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
