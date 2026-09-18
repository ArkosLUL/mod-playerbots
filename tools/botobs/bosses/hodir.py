#!/usr/bin/env python3
"""Score a Hodir pull from a RaidObs trace: pace, where he was held, Singed, buffs, churn, ice blocks.

    hodir.py <file>            every section
    hodir.py <file> --pace     health at each 30 s, boss dps per 15 s, 0-3:00 against the cache pace
    hodir.py <file> --hold     hodir.tankhold windows: his path and the boss dps in each; fire gaps
    hodir.py <file> --singed   65280 on the boss by caster kind, and the stack count that implies
    hodir.py <file> --buffs    Starlight, Toasty Fire, Storm Power, Biting Cold by role; Starlight zones
    hodir.py <file> --churn    walking undone in 5 s, A-B-A flips, moves by action, dodge walk-backs
    hodir.py <file> --blocks   helper ice blocks per Flash Freeze, by the helper inside them

What the generic views get wrong here, and what this reads instead:

- **An aura on a creature is never recorded** (`ObsSession::Tracks` wants a player), so Singed on
  Hodir has no `aura` row at all. Each proc is a triggered `cast` of 65280 with `tgt` = the boss,
  and the stack count is modelled from those: +1 a proc, 25 at most, gone 25 s after the last.
- **Toasty Fires and Starlight zones are never unit rows** that anything samples as such; they are
  `snap.hz` rows (62821, 62807), clustered here into zones by spot.
- **Boss dps comes off his own health column**, not `dealt`, which also counts ice blocks.
- **The helper inside a block is not in the trace.** Helpers are friendly and never sampled, so the
  kind comes from `hodir.dpstarget`, which writes it in front of the block guid from the centre-hold
  build on. Older traces read `?`.
"""
from __future__ import annotations

import collections
import math
import pathlib
import statistics
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.geometry import at, frames, guids_of_entry, track  # noqa: E402
from raidobs.probes import emitted_keys, silent_keys  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, notes, roster_guids  # noqa: E402

NPC_HODIR = 32845
NPC_ICICLE_SMALL = 33169
NPC_HELPER_BLOCK = 32938

SPELL_FLASH_FREEZE = 61968
SPELL_STARLIGHT = 62807
SPELL_TOASTY_FIRE = 62821
SPELL_SINGED = 65280
SPELL_STORM_POWER = (63711, 65134)
SPELL_BITING_COLD = 62039
SPELL_BITING_COLD_DAMAGE = 62188
SPELL_ICE_SHARDS = 62457

DODGE = "hodir icicle dodge action"
# The movers that can walk a bot straight back into the pool it just dodged out of.
WALK_BACKS = ("reach melee", "set behind", "hodir collect storm power", "reach spell",
              "hodir raid position action", "hodir biting cold shed")

# The Rare Cache shatters at 3:00 (SPELL_SHATTER_CHEST_TIMER), so that is the pace that matters.
DEADLINE_MS = 180000
SINGED_CAP = 25
SINGED_MS = 25000
# A small icicle detonates 3.7 s after it appears and splashes 4 yd; the dodge fires at 4.5.
ICICLE_LIVE_MS = 3700
ICICLE_TRIGGER = 4.5
# Fires are wiped when the freeze lands, about 9 s after the cast starts.
FREEZE_LANDS_MS = 9000
FIRE_MIN_LIFE_MS = 500
UNDO_WINDOW_MS = 5000
FLIP_WINDOW_MS = 5000


# Pure pieces, kept free of the trace so the tests can hand them numbers.

def singed_curve(proc_times: list[int], cap: int = SINGED_CAP, duration: int = SINGED_MS) -> list[tuple[int, int, int]]:
    """`(t, stacks, expires)` per proc. A proc after the last one expired starts again from one."""
    curve = []
    stacks = 0
    expires = -1
    for when in sorted(proc_times):
        if when > expires:
            stacks = 0
        stacks = min(cap, stacks + 1)
        expires = when + duration
        curve.append((when, stacks, expires))
    return curve


def stacks_at(curve: list[tuple[int, int, int]], when: int) -> int:
    held = 0
    for proc, stacks, expires in curve:
        if proc > when:
            break
        held = stacks if when <= expires else 0
    return held


def mean_stacks(curve: list[tuple[int, int, int]], low: int, high: int, step: int = 500) -> float:
    samples = [stacks_at(curve, when) for when in range(low, high, step)]
    return sum(samples) / len(samples) if samples else 0.0


def latch_windows(marks: list[tuple[int, str]], low: int, high: int) -> list[tuple[int, int, str]]:
    """Consecutive `(start, stop, value)` spans of one bot's latch, clipped to `[low, high)`."""
    out = []
    for index, (when, value) in enumerate(marks):
        stop = marks[index + 1][0] if index + 1 < len(marks) else high
        start, stop = max(low, when), min(high, stop)
        if stop > start:
            out.append((start, stop, value))
    return out


def path_length(points: list[tuple[float, float]]) -> float:
    return sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(points, points[1:]))


def walked_and_undone(rows: list[tuple[int, float, float]], window: int = UNDO_WINDOW_MS) -> tuple[float, float]:
    """Yards walked and yards undone, window by window: a path that ends where it started is all
    undone, a straight one none of it."""
    walked = undone = 0.0
    start = 0
    while start < len(rows) - 1:
        stop = start
        while stop + 1 < len(rows) and rows[stop + 1][0] - rows[start][0] <= window:
            stop += 1
        if stop == start:
            start += 1
            continue
        path = path_length([(row[1], row[2]) for row in rows[start:stop + 1]])
        net = math.hypot(rows[stop][1] - rows[start][1], rows[stop][2] - rows[start][2])
        walked += path
        undone += path - net
        start = stop
    return walked, undone


def aba_flips(sequence: list[tuple[int, str]], within: int = FLIP_WINDOW_MS) -> list[tuple[tuple[str, str], int]]:
    """`(pair, round trip ms)` for every A-B-A inside `within`, repeats of one action collapsed."""
    collapsed = []
    for when, action in sequence:
        if not collapsed or collapsed[-1][1] != action:
            collapsed.append((when, action))
    out = []
    for (t0, a0), (_, a1), (t2, a2) in zip(collapsed, collapsed[1:], collapsed[2:]):
        if a0 == a2 and t2 - t0 < within:
            out.append((tuple(sorted((a0, a1))), t2 - t0))
    return out


def cluster_zones(rows: list[tuple[int, float, float, float]], gap_ms: int = 2000) -> list[dict]:
    """`snap.hz` rows of one spell as zones. A zone never moves, so a row within half a yard of one
    seen in the last two seconds is the same zone."""
    zones: list[dict] = []
    for when, x, y, radius in rows:
        zone = next((z for z in zones if abs(z["x"] - x) < 0.5 and abs(z["y"] - y) < 0.5
                     and when - z["t1"] < gap_ms), None)
        if zone is None:
            zones.append({"x": x, "y": y, "r": radius, "t0": when, "t1": when})
        else:
            zone["t1"] = when
    return zones


def covered(spans: list[tuple[int, int]], low: int, high: int) -> int:
    return sum(max(0, min(stop, high) - max(start, low)) for start, stop in spans)


# Trace readers.

def pull_end(trace: Trace) -> int:
    ends = trace.of("end")
    if ends:
        return ends[-1]["t"]
    return trace.records[-1].get("t", 0) if trace.records else 0


def fight_end(trace: Trace) -> int:
    """The pull's end, or the wipe command if one came first: nothing after it is the strategy."""
    wiped = [rec["t"] for rec in trace.of("death") if rec.get("cause") == "reset"]
    return min([pull_end(trace)] + wiped)


def boss_guid(trace: Trace) -> int | None:
    guids = sorted(guids_of_entry(trace, NPC_HODIR))
    return guids[0] if guids else None


def boss_rows(trace: Trace, boss: int) -> list[tuple[int, float, float, float, int]]:
    """`(t, x, y, hp%, target)` for every sample of him."""
    out = []
    for snap in frames(trace):
        for row in snap.get("u", []):
            if row[0] == boss and len(row) > 7:
                out.append((snap["t"], row[1], row[2], row[5], row[7]))
    return out


def health_at(rows, when: int) -> float | None:
    held = None
    for row in rows:
        if row[0] > when:
            break
        held = row[3]
    return held


def bots(trace: Trace) -> set[int]:
    return {guid for guid in roster_guids(trace) if guid not in trace.humans}


def first_death(trace: Trace) -> dict[int, int]:
    out: dict[int, int] = {}
    for rec in trace.of("death"):
        out.setdefault(rec["g"], rec["t"])
    return out


def aura_spans(trace: Trace, spells) -> dict[int, list[tuple[int, int]]]:
    spells = set(spells)
    end = pull_end(trace)
    opened: dict[tuple[int, int], int] = {}
    out: dict[int, list[tuple[int, int]]] = collections.defaultdict(list)
    for rec in trace.of("aura"):
        if rec.get("sp") not in spells:
            continue
        key = (rec.get("d", 0), rec["sp"])
        if rec.get("r"):
            if key in opened:
                out[key[0]].append((opened.pop(key), rec["t"]))
        else:
            opened.setdefault(key, rec["t"])
    for (guid, _), start in opened.items():
        out[guid].append((start, end))
    return out


def zones_of(trace: Trace, spell: int) -> list[dict]:
    rows = [(snap["t"], row[1], row[2], row[4]) for snap in frames(trace)
            for row in snap.get("hz", []) if row[0] == spell]
    return cluster_zones(rows)


def fire_starts(trace: Trace) -> list[int]:
    """When each Toasty Fire first showed, sorted. A mage can finish a cast the tick the freeze lands,
    which leaves a fire seen once and wiped at once; those never burned, so they don't count."""
    return sorted(zone["t0"] for zone in zones_of(trace, SPELL_TOASTY_FIRE)
                  if zone["t0"] >= 0 and zone["t1"] - zone["t0"] >= FIRE_MIN_LIFE_MS)


def freeze_casts(trace: Trace, boss: int) -> list[int]:
    return [rec["t"] for rec in trace.of("cast") if rec.get("sp") == SPELL_FLASH_FREEZE and rec.get("s") == boss]


def tank_holder(trace: Trace) -> int | None:
    """The bot writing hodir.tankhold. Before the hold was per instance only the main tank wrote it,
    and after it every tank and ranged bot does, so the busiest writer is the main tank either way."""
    counts = collections.Counter(rec["g"] for rec in notes(trace, "hodir.tankhold"))
    tanks = [guid for guid in counts if trace.role(guid) == "tank"]
    return max(tanks, key=counts.get) if tanks else (counts.most_common(1)[0][0] if counts else None)


def missing_probes(trace: Trace) -> list[str]:
    return [key for key, _, _ in silent_keys(emitted_keys(trace), encounter_of(trace))]


def show_banner(trace: Trace) -> None:
    end = pull_end(trace)
    outcome = trace.of("end")[-1].get("out", "?") if trace.of("end") else "no end record"
    encounter = encounter_of(trace)
    humans = ", ".join(f"{trace.name(guid)} ({trace.role(guid)})" for guid in sorted(trace.humans)) or "none"
    print(f"{encounter}  {outcome} at {clock(end)}  {len(combat_deaths(trace))} death(s)  humans: {humans}")
    if encounter != "hodir":
        print("  not a Hodir pull, every section below reads empty")
        return
    gone = missing_probes(trace)
    print(f"probes absent from this trace: {', '.join(gone)}" if gone else "all hodir.* probes present")


def show_pace(trace: Trace) -> None:
    print("PACE")
    boss = boss_guid(trace)
    if boss is None:
        print("  Hodir was never sampled")
        return
    rows = boss_rows(trace, boss)
    max_hp = trace.maxhp.get(boss)
    end = fight_end(trace)
    marks = [m for m in range(30000, DEADLINE_MS + 1, 30000) if m <= end] + [end]
    print("  health " + "  ".join(f"{clock(m)[:4]} {health_at(rows, m):.1f}%" for m in marks
                                  if health_at(rows, m) is not None))
    if not max_hp:
        return

    buckets = []
    for low in range(0, end, 15000):
        high = min(low + 15000, end)
        start, stop = health_at(rows, low), health_at(rows, high)
        if start is not None and stop is not None and high > low:
            buckets.append((low, (start - stop) / 100 * max_hp / ((high - low) / 1000)))
    print("  boss dps per 15 s " + " ".join(f"{clock(low)[:4]}:{dps / 1000:.0f}k" for low, dps in buckets))

    stop = min(end, DEADLINE_MS)
    start_hp, stop_hp = health_at(rows, 0) or 100.0, health_at(rows, stop)
    if stop_hp is not None and stop > 0:
        dps = (start_hp - stop_hp) / 100 * max_hp / (stop / 1000)
        print(f"  0-{clock(stop)[:4]} {dps:,.0f} dps on the boss; the 3:00 cache wants {max_hp / 180:,.0f}")


def show_hold(trace: Trace) -> None:
    print("HOLD")
    boss = boss_guid(trace)
    holder = tank_holder(trace)
    if boss is None or holder is None:
        print("  no hodir.tankhold in this trace")
        return
    rows = boss_rows(trace, boss)
    max_hp = trace.maxhp.get(boss, 0)
    end = min(fight_end(trace), DEADLINE_MS)
    marks = [(rec["t"], str(rec.get("txt", ""))) for rec in notes(trace, "hodir.tankhold") if rec["g"] == holder]
    totals: dict[str, list[float]] = collections.defaultdict(lambda: [0.0, 0.0, 0.0])
    print(f"  as {trace.name(holder)} read it, 0-{clock(end)[:4]}")
    for low, high, value in latch_windows(marks, 0, end):
        kind = value if value in ("corner", "centre", "offfloor") else "fire"
        inside = [row for row in rows if low <= row[0] <= high]
        if len(inside) < 2:
            continue
        path = path_length([(row[1], row[2]) for row in inside])
        seconds = (inside[-1][0] - inside[0][0]) / 1000
        dps = (inside[0][3] - inside[-1][3]) / 100 * max_hp / seconds if seconds else 0.0
        held = sum(1 for row in inside if row[4] == holder) / len(inside)
        print(f"  {kind:7} {clock(low)}-{clock(high)} {(high - low) / 1000:5.1f}s  path {path:5.1f} yd"
              f"  {trace.name(holder)} holds {held * 100:3.0f}%  boss dps {dps / 1000:4.0f}k")
        totals[kind][0] += high - low
        totals[kind][1] += path
        totals[kind][2] += dps * (high - low)
    for kind, (ms, path, weighted) in totals.items():
        if ms:
            print(f"  total {kind:7} {ms / 1000:5.0f}s  {path:5.0f} yd, {path / (ms / 1000):.2f} yd/s"
                  f"  boss dps {weighted / ms / 1000:.0f}k")

    firsts = fire_starts(trace)
    print(f"\n  first fire after the pull {clock(min(firsts)) if firsts else '-'}")
    for cast in freeze_casts(trace, boss):
        lands = cast + FREEZE_LANDS_MS
        after = [when for when in firsts if when > lands]
        gap = f"{(min(after) - lands) / 1000:.1f}s" if after else "none before the end"
        print(f"  Flash Freeze {clock(cast)}, lands ~{clock(lands)}, next fire {gap}")


def show_singed(trace: Trace) -> None:
    print("SINGED")
    boss = boss_guid(trace)
    if boss is None:
        print("  Hodir was never sampled")
        return
    procs = [rec for rec in trace.of("cast")
             if rec.get("sp") == SPELL_SINGED and rec.get("tgt") == boss and rec["t"] >= 0]
    kinds = collections.Counter()
    for rec in procs:
        caster = rec.get("s", 0)
        owner = trace.owners.get(caster)
        if caster >> 32 == 0:
            kinds[trace.role(caster)] += 1
        elif owner:
            kinds["pet"] += 1
        else:
            kinds["creature"] += 1
    print(f"  {len(procs)} procs on him: " + ", ".join(f"{kind} {count}" for kind, count in kinds.most_common()))

    curve = singed_curve([rec["t"] for rec in procs])
    end = fight_end(trace)
    print("  mean stacks per 15 s " + " ".join(
        f"{clock(low)[:4]}:{mean_stacks(curve, low, min(low + 15000, end)):4.1f}" for low in range(0, end, 15000)))
    stop = min(end, DEADLINE_MS)
    mean = mean_stacks(curve, 0, stop)
    print(f"  0-{clock(stop)[:4]} mean {mean:.1f} stacks, +{2 * mean:.0f}% magic damage taken")


def show_buffs(trace: Trace) -> None:
    print("BUFFS")
    end = fight_end(trace)
    dead = first_death(trace)
    spans = {
        "starlight": aura_spans(trace, [SPELL_STARLIGHT]),
        "fire": aura_spans(trace, [SPELL_TOASTY_FIRE]),
        "stormpower": aura_spans(trace, SPELL_STORM_POWER),
        "bitingcold": aura_spans(trace, [SPELL_BITING_COLD]),
    }
    by_role: dict[str, dict[str, list[float]]] = collections.defaultdict(lambda: collections.defaultdict(list))
    for guid in bots(trace):
        alive = min(end, dead.get(guid, end))
        if alive <= 0:
            continue
        for name, table in spans.items():
            by_role[trace.role(guid)][name].append(covered(table.get(guid, []), 0, alive) / alive)
    for role in ("ranged", "melee", "heal", "tank"):
        table = by_role.get(role)
        if table:
            print(f"  {role:6} n={len(table['fire']):2}  " + "  ".join(
                f"{name} {statistics.mean(values) * 100:5.1f}%" for name, values in table.items()))

    ranged = sorted(((covered(spans["starlight"].get(guid, []), 0, min(end, dead.get(guid, end)))
                      / max(1, min(end, dead.get(guid, end))), trace.name(guid))
                     for guid in bots(trace) if trace.role(guid) == "ranged"), reverse=True)
    print("  ranged Starlight " + ", ".join(f"{name} {share * 100:.0f}%" for share, name in ranged))

    zones = [zone for zone in zones_of(trace, SPELL_STARLIGHT) if zone["t1"] >= 0]
    up = sum(1 for when in range(0, end, 500) if any(z["t0"] <= when <= z["t1"] for z in zones))
    life = statistics.mean([(z["t1"] - z["t0"]) / 1000 for z in zones]) if zones else 0.0
    print(f"  {len(zones)} Starlight zones, one up {up * 500 / max(1, end) * 100:.0f}% of the pull, mean life {life:.1f}s")

    rules = collections.Counter()
    for rec in notes(trace, "hodir.starlight"):
        words = str(rec.get("txt", "")).split(" ")
        rules[" ".join(words[:2]) if words[0] == "held" else words[0]] += 1
    print("  hodir.starlight " + ", ".join(f"{rule} {count}" for rule, count in rules.most_common()))

    cold = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_BITING_COLD_DAMAGE and 0 <= rec["t"] < end]
    taken = sum(rec.get("a", 0) for rec in trace.of("dmg") if 0 <= rec["t"] < end)
    peak = max((rec.get("st", 0) for rec in trace.of("aura") if rec.get("sp") == SPELL_BITING_COLD and rec["t"] < end),
               default=0)
    shards = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_ICE_SHARDS and 0 <= rec["t"] < end]
    cold_total = sum(rec.get("a", 0) for rec in cold)
    print(f"  Biting Cold {cold_total:,} ({cold_total / max(1, taken) * 100:.1f}% of damage taken), peak {peak} stacks;"
          f" Ice Shards {len(shards)} hits for {sum(rec.get('a', 0) for rec in shards):,}")


def show_churn(trace: Trace) -> None:
    print("CHURN")
    end = fight_end(trace)
    dead = first_death(trace)
    team = bots(trace)
    walked = undone = 0.0
    for guid, rows in track(trace, team, ("t", "x", "y")).items():
        rows = [row for row in rows if 0 <= row[0] < min(end, dead.get(guid, end))]
        yards, back = walked_and_undone(rows)
        walked += yards
        undone += back
    print(f"  walked {walked:,.0f} yd, {undone / max(1.0, walked) * 100:.1f}% undone within {UNDO_WINDOW_MS // 1000} s")

    sequences: dict[int, list[tuple[int, str]]] = collections.defaultdict(list)
    for rec in trace.of("act"):
        if rec.get("vd") == "OK" and rec.get("g") in team and 0 <= rec["t"] < end:
            sequences[rec["g"]].append((rec["t"], rec["a"]))
    flips = [flip for sequence in sequences.values() for flip in aba_flips(sequence)]
    per_min = len(flips) / max(1e-9, end / 60000)
    print(f"  {len(flips)} A-B-A flips of an OK verdict under {FLIP_WINDOW_MS // 1000} s, {per_min:.0f}/min")
    pairs = collections.Counter(pair for pair, _ in flips)
    trips = collections.defaultdict(list)
    for pair, ms in flips:
        trips[pair].append(ms)
    for pair, count in pairs.most_common(8):
        print(f"    {count:4}  {statistics.median(trips[pair]):5.0f} ms  {pair[0]} <-> {pair[1]}")

    moves = [rec for rec in trace.of("move") if rec.get("g") in team and 0 <= rec["t"] < end]
    accepted = sum(1 for rec in moves if rec.get("ok"))
    by_action: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
    for rec in moves:
        by_action[rec.get("by", "?")]["ok" if rec.get("ok") else rec.get("r", "?")] += 1
    print(f"  {accepted} accepted moves")
    for action, counts in sorted(by_action.items(), key=lambda kv: -sum(kv[1].values()))[:10]:
        rest = " ".join(f"{reason}={count}" for reason, count in counts.most_common() if reason != "ok")
        print(f"    {action:34} {counts['ok']:5} ok ({counts['ok'] / max(1, accepted) * 100:4.1f}%)  {rest}")

    icicles = []
    for guid, rows in track(trace, guids_of_entry(trace, NPC_ICICLE_SMALL), ("t", "x", "y")).items():
        if rows and 0 <= rows[0][0] < end:
            icicles.append(rows[0])
    dodges = sum(1 for rec in moves if rec.get("by") == DODGE and rec.get("ok"))
    print(f"  {dodges} accepted dodge moves over {len(icicles)} small icicles, {dodges / max(1, len(icicles)):.1f} each")

    per_bot: dict[int, list[dict]] = collections.defaultdict(list)
    for rec in moves:
        if rec.get("ok"):
            per_bot[rec["g"]].append(rec)
    after = collections.Counter()
    into = collections.Counter()
    for sequence in per_bot.values():
        for first, second in zip(sequence, sequence[1:]):
            if first.get("by") != DODGE or second.get("by") not in WALK_BACKS:
                continue
            if second["t"] - first["t"] >= ICICLE_LIVE_MS:
                continue
            after[second["by"]] += 1
            live = [icicle for icicle in icicles if 0 <= second["t"] - icicle[0] <= ICICLE_LIVE_MS]
            if second.get("x") is not None and any(
                    math.hypot(second["x"] - icicle[1], second["y"] - icicle[2]) <= ICICLE_TRIGGER for icicle in live):
                into[second["by"]] += 1
    print(f"  the move right after a dodge, and how many ended within {ICICLE_TRIGGER} yd of a live icicle")
    for action, count in after.most_common():
        print(f"    {action:34} {count:4}  {into[action]:4} into a live pool")


def show_blocks(trace: Trace) -> None:
    print("BLOCKS")
    boss = boss_guid(trace)
    end = fight_end(trace)
    kinds: dict[int, str] = {}
    for rec in notes(trace, "hodir.dpstarget"):
        words = str(rec.get("txt", "")).split(" ")
        if len(words) == 2 and words[1].isdigit():
            kinds[int(words[1])] = words[0]
    blocks = track(trace, guids_of_entry(trace, NPC_HELPER_BLOCK), ("t", "x", "y"))
    fires = fire_starts(trace)
    casts = [-1] + (freeze_casts(trace, boss) if boss is not None else [])
    for index, cast in enumerate(casts):
        low = max(0, cast)
        high = casts[index + 1] if index + 1 < len(casts) else end
        batch = [(guid, rows) for guid, rows in blocks.items() if rows and low <= rows[0][0] < high]
        if not batch:
            continue
        label = "the pull" if cast < 0 else f"Flash Freeze {clock(cast)}"
        spawned = min(rows[0][0] for _, rows in batch)
        next_fire = next((when for when in fires if when > spawned), None)
        fire = f"{(next_fire - spawned) / 1000:.1f}s later" if next_fire else "none"
        print(f"  {label}: {len(batch)} blocks from {clock(spawned)}, next fire {fire}")
        for guid, rows in sorted(batch, key=lambda item: item[1][-1][0]):
            life = (rows[-1][0] - rows[0][0]) / 1000
            print(f"    {kinds.get(guid, '?'):6} {life:5.1f}s  died {clock(rows[-1][0])}")


SECTIONS = (
    ("pace", "health, boss dps per 15 s, 0-3:00 against the cache pace", show_pace),
    ("hold", "hodir.tankhold windows and the fire gaps after each freeze", show_hold),
    ("singed", "Singed on the boss by caster kind, modelled stacks", show_singed),
    ("buffs", "Starlight, fire, Storm Power, Biting Cold by role", show_buffs),
    ("churn", "walking undone, A-B-A flips, moves by action, dodge walk-backs", show_churn),
    ("blocks", "helper ice blocks per freeze and the next fire", show_blocks),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
