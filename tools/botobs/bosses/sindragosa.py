#!/usr/bin/env python3
"""Read a Sindragosa pull from a RaidObs trace: her phases, every bot duty, and the drakes before her.

    sindragosa.py <file>              every section
    sindragosa.py <file> --phases     sg.phase: ground, air, last
    sindragosa.py <file> --tanks      tank roles, the breath count, swaps, externals, the brake, tank buttons
    sindragosa.py <file> --stands     per bot: the ground stands it held
    sindragosa.py <file> --cold       each Blistering Cold: who ran where, whom it hit
    sindragosa.py <file> --unchained  each Unchained Magic carrier: its rules, peak Instability
    sindragosa.py <file> --chilled    per player: Chilled to the Bone stops and peak stacks
    sindragosa.py <file> --air        each air phase: beacon spots, bombs, hides per bomb, tomb hold
    sindragosa.py <file> --last       the last phase: beacon spots, Mystic Buffet hides and peaks
    sindragosa.py <file> --targets    per bot: the resolver rules it was handed
    sindragosa.py <file> --drakes     Rimefang and Spinestalker: tanks, flights, brake, dodges, buttons
    sindragosa.py <file> --cheats     this encounter's cheat uses

What the generic views miss here:

- **A `sindragosa` trace is a drakes pull or a boss pull.** Rimefang and Spinestalker have no boss
  state, so their pull is marked under her slug and carries `sg.drake.*`; hers carries `sg.phase`.
  The banner names the silent keys of the kind the trace is, never `sg.gauntlet.*` (her gauntlet's)
  nor `sg.group`, `sg.tomb`, `sg.freedfallback` and `sg.flaredblue`, which no code writes.
- **`sg.phase` is her script's own state**, per bot: `air` from takeoff to touchdown, `last` at 35%
  or under on the ground, `ground` otherwise.
- **`sg.tank` is a role per tank**: `1` the holder at her tank stand, `2` free at the melee stand,
  `0` off. The roles flip at each last-phase swap, and `sg.swap` is the free tank's steps through it,
  `missed` when she stayed on the holder a second past its taunt. `sg.breath` counts her breaths, the
  swap's clock. `sg.cover` is the external a healer owes a holder no swap relieves before a last-phase
  breath, at 5 Mystic Buffet stacks or more.
- **`sg.brake` is `1` while braked**, past 90% of the holder's threat until under 80%;
  `sg.drake.brake` the same against the drake's holder, for a tank holding no drake all fight and for
  anyone else while Rimefang is up (`sg.drake.air` `1`). `sg.drake.tank` is the drake a tank holds:
  `1` Rimefang, `2` Spinestalker.
- **`sg.beacon` is a beaconed bot's spot**: `1`-`6` at the stair foot in the air, `7` and `8` the
  last phase's A and B, `0` once it ends. `sg.bombs` counts an air phase's Frost Bomb markers and
  `sg.tombhold` is `1` while its tombs are held at 20%. `sg.hide` is `bomb <tomb>` behind a tomb from
  a marker, `buffet <tomb>` from Mystic Buffet.
- **The other per-bot keys note `none` when their duty ends**: `sg.stand` (`wait` while its walk would
  cut through a beacon's splash), `sg.cold`, `sg.unchained` (`spot N`, N a node of the heroic carrier
  grid), `sg.chilled`, `sg.splash` (a beacon dodge, `blocked` when no spot was clear), and the drakes'
  `sg.drake.stand` (` alt` on a ranged stand moved off a puddle) and `sg.drake.dodge`. A value leads
  with its rule, and this reader counts rules.
  `sg.target` and `sg.drake.target` lead with the resolver's rule, `none` counted as a rule; a tomb
  pick carries the tomb's guid counter, a drake pick the drake's name.
- **`sg.defensive` is the tank ladder's pick**, a cast name, `idle` outside a window
  (`sg.drake.defensive` on the drakes). `sg.humancall` and `sg.drake.humancall` are calls to a human
  tank.
- **Blistering Cold has no hazard circle**: her cast row times it and its `dmg` rows say whom it hit.
  Peak stacks come from `aura` rows: Instability 69766, Chilled to the Bone 70106, Mystic Buffet.
"""
from __future__ import annotations

import collections
import pathlib
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from bosses import icecrown_citadel as icc  # noqa: E402
from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.probes import emitted_keys, holder_spans, latch_spans, resolve_guids, silent_keys  # noqa: E402
from raidobs.trace import Trace, clock, notes, roster_guids  # noqa: E402

ENCOUNTER = "sindragosa"
TITLE = "Sindragosa"
PHASE = "sg.phase"
DRAKE_PREFIX = "sg.drake."
GAUNTLET_PREFIX = "sg.gauntlet."
# IccInstanceState members that no code writes
UNUSED_KEYS = frozenset({"sg.group", "sg.tomb", "sg.freedfallback", "sg.flaredblue"})
BOSS = "boss"
DRAKES = "drakes"
OFF = ("none", "0")
EVER = (-(1 << 62), 1 << 62)

FROST_BREATH = (69649, 71056, 71057, 71058, 73061, 73062, 73063, 73064)
BLISTERING_COLD = (70123, 71047, 71048, 71049)
UNCHAINED_MAGIC = 69762
INSTABILITY = 69766
BACKLASH = (69770, 71044, 71045, 71046)
CHILLED_TO_THE_BONE = 70106
MYSTIC_BUFFET = (70127, 72528, 72529, 72530)
ICY_BLAST = (71377, 71378)
ICY_BLAST_PUDDLE = (71380, 71381)
COLD_CAST_MS = 5000
COLD_LAND_SLACK_MS = 1000
# Instability runs 5 s past a carrier's last cast, and Backlash fires when it drops
INSTABILITY_MS = 5000
MERGE_GAP_MS = 500
LIST_LIMIT = 8
TANK_ROLES = {"1": "holder", "2": "free", "0": "off"}
DRAKE_NAMES = {"1": "Rimefang", "2": "Spinestalker", "0": "off"}
LAST_SPOTS = {"7": "A", "8": "B"}
SPLASH_DODGE = "dodge"
DEFENSIVE_OFF = "idle"
# a button counts for a window from this long before it opens until this long after it closes
WINDOW_LEAD_MS = 1500
WINDOW_TAIL_MS = 1500
# cast names, any rank
TANK_BUTTONS = frozenset({"Last Stand", "Shield Wall", "Divine Protection", "Barkskin", "Frenzied Regeneration",
                          "Survival Instincts", "Anti-Magic Shell", "Vampiric Blood", "Icebound Fortitude"})


def mine(trace: Trace) -> bool:
    return encounter_of(trace) == ENCOUNTER


def refuse(trace: Trace, title: str) -> bool:
    print(title)
    if mine(trace):
        return False
    print(f"  not a {TITLE} pull")
    return True


def pull_kinds(trace: Trace) -> set[str]:
    """`boss` when the trace carries sg.phase, `drakes` when it carries any sg.drake.* key."""
    emitted = emitted_keys(trace)
    kinds = set()
    if PHASE in emitted:
        kinds.add(BOSS)
    if any(key.startswith(DRAKE_PREFIX) for key in emitted):
        kinds.add(DRAKES)
    return kinds


def key_kinds(key: str) -> frozenset[str]:
    """The pulls a declared key belongs to: a raid-wide key to both, the gauntlet's and the unused ones
    to neither."""
    if not key.startswith("sg."):
        return frozenset({BOSS, DRAKES})
    if key.startswith(GAUNTLET_PREFIX) or key in UNUSED_KEYS:
        return frozenset()
    return frozenset({DRAKES if key.startswith(DRAKE_PREFIX) else BOSS})


def missing_probes(trace: Trace) -> list[str]:
    kinds = pull_kinds(trace)
    return [key for key, _, _ in silent_keys(emitted_keys(trace), ENCOUNTER) if key_kinds(key) & kinds]


def show_banner(trace: Trace) -> None:
    icc.show_banner(trace)
    if not icc.is_icc(trace):
        return
    if not mine(trace):
        print(f"  not a {TITLE} pull, every section below reads empty")
        return
    kinds = pull_kinds(trace)
    if not kinds:
        print(f"neither {PHASE} nor any {DRAKE_PREFIX}* key written, so no probe is called absent")
        return
    gone = missing_probes(trace)
    found = f"probes absent from this trace: {', '.join(gone)}" if gone else "all its sg.* probes present"
    print(f"{' and '.join(sorted(kinds))} pull, {found}")


def off(value: str) -> bool:
    return value in OFF


def spans_of(trace: Trace, key: str) -> dict[int, list[tuple[str, int, int]]]:
    return holder_spans(trace, key, icc.pull_end(trace))


def rule_counts(trace: Trace, key: str, skip=OFF) -> dict[int, collections.Counter]:
    """Per guid: how many spans each rule of `key`, a value's first word, started."""
    return {guid: collections.Counter(value.split(" ", 1)[0] for value, _, _ in spans if value not in skip)
            for guid, spans in spans_of(trace, key).items()}


def counted(counts: collections.Counter) -> str:
    return ", ".join(f"{key} {counts[key]}" for key in sorted(counts)) or "-"


def joined(values: list[str], sep: str) -> str:
    """`values` joined, cut after LIST_LIMIT with a count of the rest."""
    rest = len(values) - LIST_LIMIT
    return (sep.join(values[:LIST_LIMIT]) or "-") + (f" ... {rest} more" if rest > 0 else "")


def held_values(spans: list[tuple[str, int, int]]) -> tuple[list[str], int]:
    """The distinct values held in order, `none` and `0` left out, and how often one went straight to
    another."""
    held: list[str] = []
    moves = 0
    last = None
    for value, _, _ in spans:
        if off(value):
            last = None
            continue
        if value not in held:
            held.append(value)
        moves += last is not None and last != value
        last = value
    return held, moves


def braked(spans: list[tuple[str, int, int]]) -> tuple[int, int]:
    """How often a bot was braked, and for how long in all."""
    runs = [stop - start for value, start, stop in spans if value == "1"]
    return len(runs), sum(runs)


def notes_inside(trace: Trace, key: str, start: int, stop: int) -> dict[int, list[str]]:
    """Per guid: the values it noted under `key` inside the window, repeats folded, `none` and `0`
    left out."""
    out: dict[int, list[str]] = {}
    for rec in notes(trace, key):
        value = str(rec.get("txt", ""))
        if not start <= rec["t"] <= stop or off(value):
            continue
        held = out.setdefault(rec["g"], [])
        if not held or held[-1] != value:
            held.append(value)
    return out


def aura_windows(trace: Trace, spells) -> list[tuple[int, int, int]]:
    """`(player, start, stop)` per stretch the player held any of `spells`, stretches less than half a
    second apart merged, the last one running to the pull's end."""
    end = icc.pull_end(trace)
    held: dict[int, list[list[int]]] = {}
    open_at: dict[tuple[int, int], int] = {}
    for rec in trace.of("aura"):
        if rec.get("sp") not in spells:
            continue
        key = (rec["d"], rec["sp"])
        if not rec.get("r"):
            open_at.setdefault(key, rec["t"])
        elif key in open_at:
            held.setdefault(rec["d"], []).append([open_at.pop(key), rec["t"]])
    for (player, _), start in open_at.items():
        held.setdefault(player, []).append([start, end])

    windows = []
    for player, spans in held.items():
        merged: list[list[int]] = []
        for start, stop in sorted(spans):
            if merged and start - merged[-1][1] <= MERGE_GAP_MS:
                merged[-1][1] = max(merged[-1][1], stop)
            else:
                merged.append([start, stop])
        windows += [(player, start, stop) for start, stop in merged]
    return sorted(windows, key=lambda window: (window[1], window[0]))


def peak_stacks(trace: Trace, spells, start: int, stop: int) -> dict[int, int]:
    """Per player: the most stacks of `spells` an `aura` row gave it inside the window."""
    peaks: dict[int, int] = {}
    for rec in trace.of("aura"):
        if rec.get("sp") in spells and not rec.get("r") and start <= rec["t"] <= stop:
            peaks[rec["d"]] = max(peaks.get(rec["d"], 0), rec.get("st") or 1)
    return peaks


def hits(trace: Trace, spells, start: int = EVER[0], stop: int = EVER[1]) -> collections.Counter:
    """Per roster player: the `dmg` rows of `spells` it took inside the window."""
    roster = roster_guids(trace)
    return collections.Counter(rec["d"] for rec in trace.of("dmg")
                               if rec.get("sp") in spells and rec.get("d") in roster and start <= rec["t"] <= stop)


def casts_of(trace: Trace, spells) -> list[dict]:
    return [rec for rec in trace.of("cast") if rec.get("sp") in spells]


def casts_named(trace: Trace, names: frozenset) -> list[tuple[int, int, str]]:
    """`(t, guid, spell name)` for every roster cast of one of `names`, once per ms."""
    roster = roster_guids(trace)
    named = ((rec["t"], rec.get("s"), trace.spells.get(rec.get("sp"), "")) for rec in trace.of("cast")
             if rec.get("s") in roster)
    return sorted({cast for cast in named if cast[2] in names})


def tank_windows(trace: Trace, key: str) -> list[tuple[int, int, int, list[str]]]:
    """`(start, stop, tank, values)` per run of non-idle `key` values of one tank."""
    end = icc.pull_end(trace)
    out = []
    for guid, spans in holder_spans(trace, key, end).items():
        run: list[tuple[str, int, int]] = []
        for span in spans + [(DEFENSIVE_OFF, end, end)]:
            if span[0] != DEFENSIVE_OFF:
                run.append(span)
            elif run:
                out.append((run[0][1], run[-1][2], guid, [value for value, _, _ in run]))
                run = []
    return sorted(out)


def show_buttons(trace: Trace, key: str, call_key: str) -> None:
    windows = tank_windows(trace, key)
    if windows:
        print(f"  {key}, each window: the tank, how long, what it offered and cast")
    else:
        print(f"  {key} never offered a button")
    casts = casts_named(trace, TANK_BUTTONS)
    for start, stop, tank, values in windows:
        low, high = start - WINDOW_LEAD_MS, stop + WINDOW_TAIL_MS
        spent = [cast for cast in casts if cast[1] == tank and low <= cast[0] <= high]
        offered = ", ".join(dict.fromkeys(values))
        cast = ", ".join(f"{name} {(when - start) / 1000:+.1f}s" for when, _, name in spent) or "none"
        print(f"    {clock(start)}  {trace.name(tank):<10} {clock(stop - start):>9}  offered {offered}; cast {cast}")
    calls = notes(trace, call_key)
    if calls:
        print("  calls to a human tank  " + ", ".join(f"{resolve_guids(trace, str(rec['txt']))} {clock(rec['t'])}"
                                                    for rec in calls))


def show_brakes(trace: Trace, key: str) -> None:
    rows = spans_of(trace, key)
    if not rows:
        print(f"  {key} never written")
        return
    print(f"  {key} per bot: times braked, time held")
    for guid in sorted(rows, key=trace.name):
        times, held = braked(rows[guid])
        print(f"    {trace.name(guid)}  {times}, {clock(held)}")


def stand_lines(trace: Trace, key: str) -> list[str]:
    rows = spans_of(trace, key)
    lines = []
    for guid in sorted(rows, key=trace.name):
        stands, moves = held_values(rows[guid])
        lines.append(f"    {trace.name(guid)}  {joined(stands, '/')} (moved {moves})")
    return lines


def rule_lines(trace: Trace, key: str, skip=OFF) -> list[str]:
    rows = rule_counts(trace, key, skip)
    return [f"    {trace.name(guid)}  {counted(rows[guid])}" for guid in sorted(rows, key=trace.name)]


def show_phases(trace: Trace) -> None:
    if refuse(trace, "PHASES"):
        return
    steps = icc.phase_steps(trace)
    if not steps:
        print(f"  {PHASE} never written")
        return
    print(f"  {PHASE}, each step from the first bot to note it\n")
    print(f"    {'from':>9}  {'held':>9}  phase")
    for value, start, stop in steps:
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {value}")


def breaths(trace: Trace) -> int:
    """How often sg.breath went up: once per breath her script cast."""
    count = 0
    last = 0
    for value, _, _ in latch_spans(trace, "sg.breath"):
        now = int(value) if value.isdigit() else 0
        count += now > last
        last = now
    return count


def show_tanks(trace: Trace) -> None:
    if refuse(trace, "TANKS"):
        return
    roles = spans_of(trace, "sg.tank")
    if roles:
        print("  sg.tank per tank: its role, from when")
        for guid in sorted(roles, key=trace.name):
            held = ", ".join(f"{TANK_ROLES.get(value, value)} {clock(start)}" for value, start, _ in roles[guid])
            print(f"    {trace.name(guid)}  {held}")
    else:
        print("  sg.tank never written")
    cast = len(casts_of(trace, FROST_BREATH))
    print(f"  sg.breath counted {breaths(trace)} breath(s), her Frost Breath casts {cast}")
    swaps = spans_of(trace, "sg.swap")
    if swaps:
        print("  sg.swap per bot: each step, from when")
        for guid in sorted(swaps, key=trace.name):
            steps = [f"{resolve_guids(trace, value)} {clock(start)}"
                     for value, start, _ in swaps[guid] if not off(value)]
            print(f"    {trace.name(guid)}  {joined(steps, ' -> ')}")
    else:
        print("  sg.swap never written")
    covers = {guid: [f"{value} {clock(start)}" for value, start, _ in spans if not off(value)]
              for guid, spans in spans_of(trace, "sg.cover").items()}
    covers = {guid: steps for guid, steps in covers.items() if steps}
    if covers:
        print("  sg.cover per healer: each external it owed the holder, from when")
        for guid in sorted(covers, key=trace.name):
            print(f"    {trace.name(guid)}  {joined(covers[guid], ', ')}")
    else:
        print("  sg.cover never owed an external")
    show_brakes(trace, "sg.brake")
    show_buttons(trace, "sg.defensive", "sg.humancall")


def show_stands(trace: Trace) -> None:
    if refuse(trace, "STANDS"):
        return
    lines = stand_lines(trace, "sg.stand")
    if not lines:
        print("  sg.stand never written")
        return
    print("  per bot: the stands sg.stand held in order (moves between two)\n")
    print("\n".join(lines))


def cold_rows(trace: Trace) -> list[dict]:
    """Per Blistering Cold cast: `start`, `land`, the `runs` sg.cold noted until it lands, and `hit`, the
    roster players its `dmg` rows name."""
    rows = []
    for rec in casts_of(trace, BLISTERING_COLD):
        start = rec["t"]
        land = start + (rec.get("ct") or COLD_CAST_MS)
        rows.append({"start": start, "land": land, "runs": notes_inside(trace, "sg.cold", start, land),
                     "hit": hits(trace, BLISTERING_COLD, start, land + COLD_LAND_SLACK_MS)})
    return rows


def show_cold(trace: Trace) -> None:
    if refuse(trace, "COLD"):
        return
    rows = cold_rows(trace)
    if not rows:
        print("  no Blistering Cold cast")
        return
    print(f"  {len(rows)} Blistering Cold cast(s): the runs sg.cold noted, the players it hit\n")
    for row in rows:
        runs = ", ".join(f"{trace.name(guid)} {' -> '.join(values)}"
                         for guid, values in sorted(row["runs"].items(), key=lambda item: trace.name(item[0])))
        print(f"    {clock(row['start']):>9}  {runs or 'nobody ran'}  hit {icc.ranked(trace, row['hit']) or 'nobody'}")


def unchained_rows(trace: Trace) -> list[dict]:
    """Per Unchained Magic window: `carrier`, `start`, `stop`, the `rules` sg.unchained noted until its
    Instability ran out, and the `peak` Instability stacks."""
    rows = []
    for carrier, start, stop in aura_windows(trace, (UNCHAINED_MAGIC,)):
        tail = stop + INSTABILITY_MS
        rows.append({"carrier": carrier, "start": start, "stop": stop,
                     "rules": notes_inside(trace, "sg.unchained", start, tail).get(carrier, []),
                     "peak": peak_stacks(trace, (INSTABILITY,), start, tail).get(carrier, 0)})
    return rows


def backlash_splash(trace: Trace, rows: list[dict]) -> collections.Counter:
    """Backlash hits on players outside every window of their own, so another carrier's splash."""
    carrying: dict[int, list[tuple[int, int]]] = {}
    for row in rows:
        carrying.setdefault(row["carrier"], []).append((row["start"], row["stop"] + INSTABILITY_MS))
    roster = roster_guids(trace)
    return collections.Counter(
        rec["d"] for rec in trace.of("dmg")
        if rec.get("sp") in BACKLASH and rec.get("d") in roster
        and not any(low <= rec["t"] <= high for low, high in carrying.get(rec["d"], [])))


def show_unchained(trace: Trace) -> None:
    if refuse(trace, "UNCHAINED"):
        return
    rows = unchained_rows(trace)
    if not rows:
        print("  no Unchained Magic landed")
        return
    print(f"  {len(rows)} Unchained Magic window(s): the carrier, the rules sg.unchained noted, peak Instability\n")
    for row in rows:
        print(f"    {clock(row['start']):>9}  {clock(row['stop'] - row['start']):>9}  "
              f"{icc.who(trace, row['carrier'])}  {' -> '.join(row['rules']) or '-'}  peak {row['peak']}")
    print(f"  Backlash hits on non-carriers: {icc.ranked(trace, backlash_splash(trace, rows)) or 'none'}")


def show_chilled(trace: Trace) -> None:
    if refuse(trace, "CHILLED"):
        return
    stops = rule_counts(trace, "sg.chilled")
    peaks = peak_stacks(trace, (CHILLED_TO_THE_BONE,), *EVER)
    players = set(stops) | set(peaks)
    if not players:
        print("  no Chilled to the Bone and no sg.chilled note")
        return
    print("  per player: sg.chilled stops, peak Chilled to the Bone stacks\n")
    for guid in sorted(players, key=trace.name):
        print(f"    {icc.who(trace, guid)}  stops {sum(stops.get(guid, {}).values())}  peak {peaks.get(guid, 0)}")


def air_rows(trace: Trace) -> list[dict]:
    """Per air phase: `start`, `stop`, `beacons` as (bot, spot), `dodges` per bot, `bombs` as (count,
    from, hides, tombs), hides counted until the next bomb, and `held`, the sg.tombhold spans over it."""
    end = icc.pull_end(trace)
    beacons = spans_of(trace, "sg.beacon")
    splashes = spans_of(trace, "sg.splash")
    hides = [(start, value.partition(" ")[2]) for spans in spans_of(trace, "sg.hide").values()
             for value, start, _ in spans if value.startswith("bomb ")]
    marks = [(int(value), start) for value, start, _ in latch_spans(trace, "sg.bombs", end)
             if value.isdigit() and value != "0"]
    holds = [(start, stop) for value, start, stop in latch_spans(trace, "sg.tombhold", end) if value == "1"]
    rows = []
    for phase, start, stop in icc.phase_steps(trace):
        if phase != "air":
            continue
        spots = sorted(((guid, spot) for guid, spans in beacons.items() for spot, at, _ in spans
                        if start <= at < stop and not off(spot)),
                       key=lambda pair: (int(pair[1]) if pair[1].isdigit() else 255, trace.name(pair[0])))
        dodges = collections.Counter(guid for guid, spans in splashes.items() for value, at, _ in spans
                                     if start <= at < stop and value == SPLASH_DODGE)
        inside = [(count, at) for count, at in marks if start <= at < stop]
        bombs = []
        for index, (count, at) in enumerate(inside):
            until = inside[index + 1][1] if index + 1 < len(inside) else stop
            tombs = [tomb for when, tomb in hides if at <= when < until]
            bombs.append((count, at, len(tombs), len(set(tombs))))
        held = [(low, high) for low, high in holds if low < stop and high > start]
        rows.append({"start": start, "stop": stop, "beacons": spots, "dodges": dodges, "bombs": bombs, "held": held})
    return rows


def show_air(trace: Trace) -> None:
    if refuse(trace, "AIR"):
        return
    rows = air_rows(trace)
    if not rows:
        print(f"  no air phase in {PHASE}")
        return
    print(f"  {len(rows)} air phase(s): beacon spots, beacon dodges, bombs and the hides behind tombs, tomb hold")
    for row in rows:
        print(f"\n  {clock(row['start'])}  held {clock(row['stop'] - row['start'])}")
        print("    beacons  " + (", ".join(f"{trace.name(guid)} {spot}" for guid, spot in row["beacons"]) or "none"))
        print(f"    beacon dodges  {icc.ranked(trace, row['dodges']) or 'none'}")
        if not row["bombs"]:
            print("    bombs  none counted")
        for count, at, picks, tombs in row["bombs"]:
            print(f"    bomb {count}  {clock(at)}  hides {picks}, tombs {tombs}")
        held = ", ".join(f"{clock(low)} for {clock(high - low)}" for low, high in row["held"])
        print(f"    tombs held  {held or 'never'}")


def show_last(trace: Trace) -> None:
    if refuse(trace, "LAST"):
        return
    start = next((at for phase, at, _ in icc.phase_steps(trace) if phase == "last"), None)
    if start is None:
        print(f"  no last phase in {PHASE}")
        return
    beacons = sorted((at, guid, spot) for guid, spans in spans_of(trace, "sg.beacon").items()
                     for spot, at, _ in spans if at >= start and not off(spot))
    hides = {guid: [(at, until) for value, at, until in spans if at >= start and value.startswith("buffet ")]
             for guid, spans in spans_of(trace, "sg.hide").items()}
    peaks = peak_stacks(trace, MYSTIC_BUFFET, start, icc.pull_end(trace))
    print(f"  last phase from {clock(start)}: beacon spots, Mystic Buffet hides, peak Buffet stacks\n")
    print("    beacons  " + (", ".join(f"{trace.name(guid)} {LAST_SPOTS.get(spot, spot)} {clock(at)}"
                                       for at, guid, spot in beacons) or "none"))
    for guid in sorted({guid for guid, rows in hides.items() if rows} | set(peaks), key=trace.name):
        rows = hides.get(guid, [])
        hidden = sum(until - at for at, until in rows)
        print(f"    {icc.who(trace, guid)}  hides {len(rows)}, {clock(hidden)} hidden  peak {peaks.get(guid, 0)}")


def show_targets(trace: Trace) -> None:
    if refuse(trace, "TARGETS"):
        return
    lines = rule_lines(trace, "sg.target", skip=())
    if not lines:
        print("  sg.target never written")
        return
    print("  per bot: spans of each resolver rule\n")
    print("\n".join(lines))


def show_drakes(trace: Trace) -> None:
    if refuse(trace, "DRAKES"):
        return
    if DRAKES not in pull_kinds(trace):
        print(f"  no {DRAKE_PREFIX}* key written: not a drakes pull")
        return
    tanks = spans_of(trace, "sg.drake.tank")
    if tanks:
        print("  sg.drake.tank per tank: the drake it holds, from when")
        for guid in sorted(tanks, key=trace.name):
            held = ", ".join(f"{DRAKE_NAMES.get(value, value)} {clock(start)}" for value, start, _ in tanks[guid])
            print(f"    {trace.name(guid)}  {held}")
    else:
        print("  sg.drake.tank never written")

    flights = [(start, stop) for value, start, stop in latch_spans(trace, "sg.drake.air", icc.pull_end(trace))
               if value == "1"]
    if flights:
        print(f"  Rimefang up {len(flights)} time(s): "
              + ", ".join(f"{clock(start)} for {clock(stop - start)}" for start, stop in flights))
    else:
        print("  sg.drake.air never noted Rimefang up")
    show_brakes(trace, "sg.drake.brake")

    dodges = rule_counts(trace, "sg.drake.dodge")
    taken = {"blast": hits(trace, ICY_BLAST), "puddle": hits(trace, ICY_BLAST_PUDDLE)}
    players = set(dodges) | {guid for counter in taken.values() for guid in counter}
    if players:
        print("  per player: sg.drake.dodge rules, then Icy Blast and puddle hits")
        for guid in sorted(players, key=trace.name):
            hit = ", ".join(f"{label} {counter[guid]}" for label, counter in taken.items() if counter[guid]) or "-"
            print(f"    {icc.who(trace, guid)}  dodges {counted(dodges.get(guid, collections.Counter()))}  hit {hit}")
    else:
        print("  no sg.drake.dodge note and no Icy Blast hit")

    for key, title, lines in (
            ("sg.drake.stand", "the stands held in order (moves between two)", stand_lines(trace, "sg.drake.stand")),
            ("sg.drake.target", "spans of each resolver rule", rule_lines(trace, "sg.drake.target", skip=()))):
        if not lines:
            print(f"  {key} never written")
            continue
        print(f"  {key} per bot: {title}")
        print("\n".join(lines))
    show_buttons(trace, "sg.drake.defensive", "sg.drake.humancall")


def show_cheats(trace: Trace) -> None:
    if refuse(trace, "CHEATS"):
        return
    if (trace.header.get("v") or 0) < icc.CHEAT_SCHEMA:
        print(f"  schema v{trace.header.get('v') or 0} predates the cheat record (v{icc.CHEAT_SCHEMA})")
        return
    rows = icc.cheat_rows(trace)
    own = [row for row in rows if row["own"]]
    if not own:
        print("  no cheat of this encounter fired")
    for row in own:
        print(f"  {row['key']}  {row['n']} use(s) from {clock(row['first'])}")
        print(f"    by  {icc.ranked(trace, row['bots'])}")
        print(f"    on  {icc.named(trace, row['targets'])}")
    others = sum(row["n"] for row in rows if not row["own"])
    if others:
        print(f"  {others} use(s) under other encounters' keys: icecrown_citadel.py --cheats")


SECTIONS = (
    ("phases", "sg.phase: ground, air, last", show_phases),
    ("tanks", "tank roles, the breath count, swaps, externals, the brake, tank buttons", show_tanks),
    ("stands", "per bot: the ground stands it held", show_stands),
    ("cold", "each Blistering Cold: who ran where, whom it hit", show_cold),
    ("unchained", "each Unchained Magic carrier: its rules, peak Instability", show_unchained),
    ("chilled", "per player: Chilled to the Bone stops and peak stacks", show_chilled),
    ("air", "each air phase: beacon spots, bombs, hides per bomb, tomb hold", show_air),
    ("last", "the last phase: beacon spots, Mystic Buffet hides and peaks", show_last),
    ("targets", "per bot: the resolver rules it was handed", show_targets),
    ("drakes", "Rimefang and Spinestalker: tanks, flights, brake, dodges, stands, targets, buttons", show_drakes),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
