#!/usr/bin/env python3
"""Score a Hodir pull from a RaidObs trace: pace, where he was held, Singed, buffs, Storm Cloud, churn, ice blocks,
deaths, tanks."""
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
from raidobs.geometry import at, frames, guids_of_entry, track  # noqa: E402
from raidobs.probes import emitted_keys, silent_keys  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, notes, roster_guids  # noqa: E402

NPC_HODIR = 32845
NPC_ICICLE_SMALL = 33169
NPC_HELPER_BLOCK = 32938
NPC_TRAPPED_BLOCK = 32926

SPELL_FLASH_FREEZE = 61968
SPELL_STARLIGHT = 62807
SPELL_TOASTY_FIRE = 62821
SPELL_SINGED = 65280
SPELL_STORM_POWER = (63711, 65134)
# The cloud a carrier holds, 10 and 25; each Storm Power it hands out spends one of its charges.
SPELL_STORM_CLOUD = (65123, 65133)
# Storm Power pulses this far round the carrier's own feet.
STORM_PULSE_YD = 3.0
# Inside this of the rally a receiver stops walking (ULDUAR_HODIR_STORM_CLOUD_COLLECT_RELEASE).
STORM_COLLECT_RELEASE_YD = 4.0
# Inside this a carrier stops walking back to its rally (ULDUAR_HODIR_STORM_CLOUD_COLLECT_PARK).
STORM_PARK_YD = 2.0
# A cloud removed with less than this left ran its time out rather than its charges.
STORM_CLOUD_EXPIRY_MS = 500
SPELL_BITING_COLD = 62039
SPELL_BITING_COLD_DAMAGE = 62188
SPELL_ICE_SHARDS = 62457
SPELL_BLOODLUST = 2825
SPELL_HEROISM = 32182
# The aura he casts on himself, 10 and 25: each lasts 20 s and opens 15 s after a Flash Freeze starts.
SPELL_FROZEN_BLOWS = (62478, 63512)
FROZEN_BLOWS_MS = 20000
# His miss of the hard mode: the 65272 timer ticks at 3:00 after the engage and he casts this on the cache.
SPELL_SHATTER_CHEST = 62501
# The swing's own damage, 10 and 25, which lands only on whoever he is hitting.
SPELL_FROZEN_BLOWS_SWING = (62867, 63511)
# The tank buttons NextTankDefensive offers, by the cast row's spell. Last Stand's cast is a dummy that
# puts up 12976, which is the row a death's auras carry.
TANK_DEFENSIVES = {
    12975: "last stand", 871: "shield wall", 498: "divine protection", 22812: "barkskin",
    22842: "frenzied regeneration", 61336: "survival instincts", 48707: "anti magic shell",
    55233: "vampiric blood", 48792: "icebound fortitude",
}
TANK_DEFENSIVE_AURAS = {12976 if spell == 12975 else spell: name for spell, name in TANK_DEFENSIVES.items()}
TANK_DEFENSIVE_NODE = "hodir tank defensive action"
# What killed a raider, by the spell of the killing blow. Biting Cold's tick is cast by the victim on itself.
DEATH_SPELLS = {
    62867: "Frozen Blows swing", 63511: "Frozen Blows swing",
    64544: "Frozen Blows raid tick", 64545: "Frozen Blows raid tick",
    62188: "Biting Cold",
    62469: "Freeze",
    62457: "Ice Shards", 65370: "Ice Shards",
    62226: "Flash Freeze",
}

DODGE = "hodir icicle dodge action"
SHELTER = "hodir move snowpacked icicle"
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
# The cast ends 9 s in: targets are picked and the fires wiped there, the blocks spawn a second later.
FREEZE_LANDS_MS = 9000
FIRE_MIN_LIFE_MS = 500
UNDO_WINDOW_MS = 5000
FLIP_WINDOW_MS = 5000

# The hold and the stands, as UldEncounter_Hodir.h has them.
CENTRE = (1998.0, -235.5)
HOLD_LEASH = 25.0
FIRE_RADIUS = 11.0
FIRE_STAND_RADIUS = 8.0
STARLIGHT_STAND_RADIUS = 1.5
BUFF_WALK = 15.0
# What a bot with neither Starlight nor a fire walks; the short one is for a bot that already holds one.
BUFF_WALK_UNBUFFED = 30.0
BAND = (15.0, 35.0)
# His melee range against a raider: combat reaches 7.5 and 1.5 plus 4/3. The code reads it live.
HODIR_MELEE_RANGE = 7.5 + 1.5 + 4.0 / 3.0
# A Starlight stand may come closer than the band's inner edge, as far in as his melee range.
STARLIGHT_BAND = (HODIR_MELEE_RANGE, 35.0)
# Melee this close to him are the pack a fire at his feet is meant to cover.
MELEE_NEAR_YD = 10.0
# Inside this one a melee bot is actually swinging, so it is the uptime number the pack share needs
# beside it: a high share of a pack that is not there says nothing.
MELEE_REACH_YD = 8.0
ROLE_TAGS = (("melee", "m"), ("ranged", "r"), ("heal", "h"), ("tank", "t"))
# Centre of the ranged ring on builds before the ring was removed, for reading their traces.
LEGACY_RING_ANCHOR = (1986.56, -257.11)
# He normally stops within 1-2 yd of the point, so past this he is parked off it.
OFF_POINT_YD = 5.0
# A stalled walk: still this long, this far short of where the move was sent, before any newer move.
STALL_MS = 1000
STALL_SHORT_YD = 1.5
STALL_SETTLE_MS = 300
# Where the shed arms outside Starlight, and inside it.
BITING_COLD_ARM = 4
BITING_COLD_ARM_IN_STARLIGHT = 5
# Both arm points drop this much while he carries Frozen Blows.
BITING_COLD_ARM_FROZEN_BLOWS_DROP = 1
SHED = "hodir biting cold shed"
# Where a shed stops (ULDUAR_HODIR_BITING_COLD_SHED_FLOOR).
BITING_COLD_SHED_FLOOR = 1
# OK verdicts of the shed this close together are one shed, and it ends this long after its last one at
# the latest: a hop launched on the last verdict stays up across the next two ticks.
SHED_GAP_MS = 3000
SHED_TAIL_MS = 2500
# Movers that run while the position node stands down; a dodge isn't one, it re-reads the anchor.
PROBE_PAUSES = (SHED, SHELTER, "hodir collect storm power", "hodir spread storm cloud")
# A shed leg this close to where the bot already stands goes nowhere: MoveTo calls it there or a dup.
OWN_SPOT_YD = 0.5
# An instant still costs the global cooldown, so the breaker split counts it as this much casting.
CAST_FLOOR_MS = 1000
# A breaker this far from its block is past any spell's reach of it.
BLOCK_FAR_YD = 35.0
POSITION = "hodir raid position action"
# A sample counts as moved by the last accepted move this recent.
MOVER_WINDOW_MS = 4000


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


def union_ms(spans: list[tuple[int, int]], low: int, high: int) -> int:
    """Time inside at least one span, clipped to `[low, high)`."""
    total = 0
    reach = low
    for start, stop in sorted(spans):
        start, stop = max(start, reach), min(stop, high)
        if stop > start:
            total += stop - start
            reach = stop
    return total


def fire_in_leash(x: float, y: float) -> bool:
    """A fire he is worth being dragged onto: inside HOLD_LEASH of the centre."""
    return math.hypot(x - CENTRE[0], y - CENTRE[1]) <= HOLD_LEASH


def stand_point(bot: tuple[float, float], zone: tuple[float, float],
                radius: float) -> tuple[float, float]:
    """Where a bot stands to hold `zone`: the point of it nearest the bot, capped at `radius` from the
    centre. A bot already inside keeps the spot it is on, which is what the encounter derives."""
    dx, dy = bot[0] - zone[0], bot[1] - zone[1]
    gap = math.hypot(dx, dy)
    if gap <= radius or not gap:
        return bot
    return zone[0] + dx / gap * radius, zone[1] + dy / gap * radius


def in_band(point: tuple[float, float], boss: tuple[float, float], band: tuple[float, float] = BAND) -> bool:
    gap = math.hypot(point[0] - boss[0], point[1] - boss[1])
    return band[0] <= gap <= band[1]


def usable_within(bot: tuple[float, float], boss: tuple[float, float],
                  zones: list[tuple[float, float]], radius: float, walk: float = BUFF_WALK,
                  band: tuple[float, float] = BAND) -> float | None:
    """The walk to the nearest stand among `zones` that is inside `band` and no further than `walk`,
    or None when there is none - what the encounter would have offered this bot."""
    best = None
    for zone in zones:
        stand = stand_point(bot, zone, radius)
        gap = math.hypot(stand[0] - bot[0], stand[1] - bot[1])
        if gap > walk or not in_band(stand, boss, band):
            continue
        best = gap if best is None else min(best, gap)
    return best


def in_legal_fire(spot: tuple[float, float], boss: tuple[float, float], fires: list[tuple[float, float]]) -> bool:
    """In a fire and inside the caster band from him: what a ranged fire stand is for. The fire he is
    held on fails the band, so it never counts."""
    return in_band(spot, boss) and any(inside_circle(spot, fire, FIRE_RADIUS) for fire in fires)


def both_runs(first: list[tuple[int, str]], second: list[tuple[int, str]], low: int, high: int,
              want_first: str, want_second: str) -> list[tuple[int, int]]:
    """Merged `(start, stop)` spans in `[low, high)` where one bot's `first` latch reads `want_first`
    while its `second` reads `want_second`."""
    spans = []
    for a_low, a_high, a_value in latch_windows(first, low, high):
        if a_value != want_first:
            continue
        for b_low, b_high, b_value in latch_windows(second, low, high):
            start, stop = max(a_low, b_low), min(a_high, b_high)
            if b_value == want_second and stop > start:
                spans.append((start, stop))
    merged: list[tuple[int, int]] = []
    for start, stop in sorted(spans):
        if merged and start <= merged[-1][1]:
            merged[-1] = (merged[-1][0], max(merged[-1][1], stop))
        else:
            merged.append((start, stop))
    return merged


def storm_carries(rows: list[tuple[int, int, int, bool, int]]) -> list[dict]:
    """One carry per Storm Cloud from `(t, carrier, charges, removed, ms left)` aura rows in time order:
    `start`, `end`, `carrier` and `spent`, True when the charges ran out with time left on the cloud,
    False when it ran out of time with charges left, None when the trace ended first. A fresh cloud is
    an apply on a carrier with none open, or charges rising on an open one."""
    carries: list[dict] = []
    open_on: dict[int, dict] = {}
    for when, carrier, charges, removed, left in rows:
        carry = open_on.get(carrier)
        if removed:
            if carry:
                carry["end"], carry["spent"] = when, left > STORM_CLOUD_EXPIRY_MS
                del open_on[carrier]
            continue
        if carry and charges <= carry["charges"]:
            carry["charges"] = charges
            continue
        if carry:
            carry["end"], carry["spent"] = when, False
        carry = {"start": when, "end": None, "carrier": carrier, "charges": charges, "spent": None}
        carries.append(carry)
        open_on[carrier] = carry
    return carries


def run_until_paused(low: int, high: int, pauses: list[int], spans: list[tuple[int, int]]) -> int:
    """Where a probe run over `[low, high)` stops counting: the first pause time in it, or a span
    open at or opening after `low`, else `high`."""
    cuts = [when for when in pauses if low <= when < high]
    cuts += [max(low, start) for start, stop in spans if start < high and stop > low]
    return min(cuts, default=high)


def starlight_rule(value: str) -> str:
    """The rule a hodir.starlight note names, without its zone. A held stand standing down keeps its
    `held`, so it reads apart from a sweep that found nothing."""
    words = value.split(" ")
    return " ".join(words[:2]) if words[0] == "held" else words[0]


def breaker_cast_split(casts: list[tuple[int, int | None]], own: int, blocks: set[int],
                       boss: int | None) -> collections.Counter:
    """Casting ms per place it went, from `(cast time, target)` pairs a breaker started while its
    block lived: `own`, `other blocks`, `Hodir`, `ground/self` (no target) or `other`."""
    out: collections.Counter = collections.Counter()
    for ms, target in casts:
        if target == own:
            where = "own"
        elif not target:
            where = "ground/self"
        elif target in blocks:
            where = "other blocks"
        elif target == boss:
            where = "Hodir"
        else:
            where = "other"
        out[where] += max(ms, CAST_FLOOR_MS)
    return out


def breaker_motion(samples: list[tuple[bool, str, float | None]], far: float = BLOCK_FAR_YD) -> tuple[float, float, float]:
    """`(moving %, moved by the position node %, over `far` from the block %)` over `(moving, last mover,
    gap to the block)` samples of ranged bots shooting a block."""
    if not samples:
        return 0.0, 0.0, 0.0
    moving = sum(1 for move, _, _ in samples if move)
    by_position = sum(1 for move, mover, _ in samples if move and mover == POSITION)
    distant = sum(1 for _, _, gap in samples if gap is not None and gap > far)
    return tuple(count / len(samples) * 100 for count in (moving, by_position, distant))


def reach_shares(walks: list[float | None], budgets: tuple[float, ...]) -> tuple[float, tuple[float, ...]]:
    """`(share of samples where a legal stand existed at all, share inside each budget)`, both over every
    sample: the share a bot could reach only means something beside the share there was to reach."""
    if not walks:
        return 0.0, tuple(0.0 for _ in budgets)
    live = [walk for walk in walks if walk is not None]
    return (len(live) / len(walks) * 100,
            tuple(sum(1 for walk in live if walk <= budget) / len(walks) * 100 for budget in budgets))


def dps_split(samples: list[tuple[int, float]], lit: list[tuple[int, int]], max_hp: int, end: int,
              step_cap: int = 2000) -> dict[str, tuple[float, int]]:
    """Boss dps over `[0, end)` split by whether a fire burned anywhere, as `{"fire": (dps, ms)}`.
    Samples further apart than `step_cap` are a hole in the trace and count for neither."""
    acc = {"fire": [0.0, 0], "none": [0.0, 0]}
    for (when, health), (nxt, after) in zip(samples, samples[1:]):
        span = min(nxt, end) - when
        if when < 0 or when >= end or span <= 0 or nxt - when > step_cap:
            continue
        key = "fire" if any(start <= when < stop for start, stop in lit) else "none"
        acc[key][0] += max(0.0, health - after) / 100 * max_hp
        acc[key][1] += span
    return {key: (damage / (ms / 1000), ms) for key, (damage, ms) in acc.items() if ms}


def inside_circle(point: tuple[float, float], centre: tuple[float, float], radius: float) -> bool:
    return math.hypot(point[0] - centre[0], point[1] - centre[1]) <= radius


def gap_profile(gaps: list[float], limit: float) -> tuple[float, float]:
    """`(median, share at or inside limit as a percentage)` for a list of distances."""
    if not gaps:
        return 0.0, 0.0
    return statistics.median(gaps), sum(1 for gap in gaps if gap <= limit) / len(gaps) * 100


def role_split(roles: collections.Counter) -> str:
    """`m12 r3 h1` for a role tally, always in that order, skipping the roles with none."""
    return " ".join(f"{tag}{roles[name]}" for name, tag in ROLE_TAGS if roles[name])


def ramp_ms(curve: list[tuple[int, int, int]], since: int, cap: int = SINGED_CAP) -> int | None:
    """How long after `since` the stack first reached `cap`, or None if it never did."""
    return next((proc - since for proc, stacks, _ in curve if proc >= since and stacks >= cap), None)


def stacks_ms(curve: list[tuple[int, int, int]], low: int, high: int, floor: int, step: int = 500) -> int:
    """Time inside `[low, high)` at `floor` stacks or more."""
    return sum(step for when in range(low, high, step) if stacks_at(curve, when) >= floor)


def dark_ms(curve: list[tuple[int, int, int]], fires: list[tuple[int, int]], low: int, high: int,
            step: int = 500) -> int:
    """Time with no stacks at all while a fire was burning somewhere - the hole that costs the most,
    because a fire was there to stand in and nobody did."""
    return sum(step for when in range(low, high, step)
               if not stacks_at(curve, when) and any(start <= when <= stop for start, stop in fires))


def off_point_ms(samples: list[tuple[int, float, float]], points: list[tuple[int, tuple[float, float]]],
                 low: int, high: int, gap: float = OFF_POINT_YD) -> tuple[int, int]:
    """`(ms off, ms sampled)` inside `[low, high)`. Each sample counts until the next, against the hold
    point in force when it was taken."""
    off = total = 0
    for (when, x, y), (after, _, _) in zip(samples, samples[1:]):
        start, stop = max(when, low), min(after, high)
        if stop <= start:
            continue
        point = None
        for mark, value in points:
            if mark > when:
                break
            point = value
        if point is None:
            continue
        total += stop - start
        if math.hypot(x - point[0], y - point[1]) > gap:
            off += stop - start
    return off, total


def stalled_walks(moves: list[tuple[int, float, float, str]], rows: list[tuple[int, float, float, int]],
                  movers=(DODGE, SHELTER)) -> collections.Counter:
    """Accepted `movers` moves after which the bot stood still `STALL_MS`+, over `STALL_SHORT_YD` short of
    where it was sent, before any newer accepted move. `moves` and `rows` are one bot's, in time order."""
    out = collections.Counter()
    times = [row[0] for row in rows]
    for index, (when, x, y, by) in enumerate(moves):
        if by not in movers:
            continue
        stop = moves[index + 1][0] if index + 1 < len(moves) else when + UNDO_WINDOW_MS
        since = None
        for t, px, py, moving in rows[bisect.bisect_left(times, when + STALL_SETTLE_MS):]:
            if t >= stop:
                break
            if not moving and math.hypot(px - x, py - y) > STALL_SHORT_YD:
                since = t if since is None else since
                if t - since >= STALL_MS:
                    out[by] += 1
                    break
            else:
                since = None
    return out


def after_landing(times: list[int], casts: list[int], cast_ms: int = FREEZE_LANDS_MS) -> int:
    """How many of `times` fall outside every Flash Freeze cast."""
    return sum(1 for when in times if not any(cast <= when <= cast + cast_ms + 100 for cast in casts))


def stacks_before(changes: list[tuple[int, int]], when: int) -> int:
    held = 0
    for mark, stacks in changes:
        if mark > when:
            break
        held = stacks
    return held


def band_ms(changes: list[tuple[int, int]], low: int, high: int, floor: int) -> int:
    """Time at `floor` stacks or more inside `[low, high)`."""
    total = 0
    for (mark, stacks), (after, _) in zip(changes, changes[1:] + [(high, 0)]):
        if stacks >= floor:
            total += max(0, min(after, high) - max(mark, low))
    return total


def still_armed_ms(samples: list[tuple[int, int]], changes: list[tuple[int, int]], fires: list[tuple[int, int]],
                   stars: list[tuple[int, int]], low: int, high: int, step_cap: int = 2000,
                   blows: list[tuple[int, int]] = ()) -> int:
    """Time in `[low, high)` one bot stood still out of a fire at the shed's arm point or more, one higher
    in Starlight and one lower inside `blows`, the Frozen Blows windows. `samples` are `(t, moving)`, each
    lasting to the next; a step over `step_cap` is a hole."""
    total = 0
    for (when, moving), (nxt, _) in zip(samples, samples[1:]):
        if moving or when < low or when >= high or nxt - when > step_cap:
            continue
        if any(start <= when < stop for start, stop in fires):
            continue
        arm = BITING_COLD_ARM_IN_STARLIGHT if any(start <= when < stop for start, stop in stars) else BITING_COLD_ARM
        if any(start <= when < stop for start, stop in blows):
            arm -= BITING_COLD_ARM_FROZEN_BLOWS_DROP
        if stacks_before(changes, when) >= arm:
            total += min(nxt, high) - when
    return total


def own_spot_legs(moves: list[tuple[int, float, float]], rows: list[tuple[int, float, float, int]],
                  within: float = OWN_SPOT_YD) -> int:
    """How many of one bot's shed moves, accepted or refused, point within `within` of where its
    latest sample has it. Only samples taken standing still count: a walking bot's lags where it is."""
    times = [row[0] for row in rows]
    count = 0
    for when, x, y in moves:
        index = bisect.bisect_right(times, when) - 1
        if index < 0:
            continue
        _, px, py, moving = rows[index]
        if not moving and math.hypot(x - px, y - py) < within:
            count += 1
    return count


def shed_spans(oks: list[int], changes: list[tuple[int, int]], gap: int = SHED_GAP_MS,
               floor: int = BITING_COLD_SHED_FLOOR, tail: int = SHED_TAIL_MS) -> list[tuple[int, int]]:
    """One bot's sheds as `(start, stop)`: runs of the shed's OK verdicts `oks` no more than `gap` apart,
    each stopping where its stacks first fall to `floor` or below, but no later than `tail` past its last
    verdict: a shed cut short (a fire, the freeze) only reaches the floor later by other means."""
    runs: list[list[int]] = []
    for when in sorted(oks):
        if runs and when - runs[-1][1] <= gap:
            runs[-1][1] = when
        else:
            runs.append([when, when])
    out = []
    for start, last in runs:
        stop = last + tail
        for when, stacks in changes:
            if start < when <= stop and stacks <= floor:
                stop = when
                break
        out.append((start, stop))
    return out


def stacks_shed(changes: list[tuple[int, int]], low: int, high: int) -> int:
    """Stacks that came off in `(low, high]`, the aura's removal included."""
    shed = 0
    held = stacks_before(changes, low)
    for when, stacks in changes:
        if low < when <= high:
            shed += max(0, held - stacks)
            held = stacks
    return shed


def kept_near(seg: list[tuple[int, float, float]], other: list[tuple[int, float, float]],
              radius: float) -> float | None:
    """Share of one bot's samples `seg` within `radius` of `other`'s latest sample at or before each, or None
    when the first one isn't: only a bot that started there can lose it. Both are `(t, x, y)` in time order."""
    stamps = [row[0] for row in other]

    def near(when: int, x: float, y: float) -> bool:
        index = bisect.bisect_right(stamps, when) - 1
        return index >= 0 and math.hypot(x - other[index][1], y - other[index][2]) <= radius

    if not seg or not near(*seg[0]):
        return None
    return sum(1 for row in seg if near(*row)) / len(seg)


def rare_cache(outcome: str, end: int, shatter: int | None) -> str:
    """Whether the pull was a hard-mode kill. The cache needs no toggle: he shatters it with 62501 when
    the timer he cast at the engage ticks, so only a kill with no such cast before it counts."""
    if shatter is not None:
        return f"Rare Cache shattered at {clock(shatter)} (Shatter Chest {SPELL_SHATTER_CHEST}): not a hard-mode kill"
    if end > DEADLINE_MS:
        return f"Rare Cache unread: {outcome} at {clock(end)}, past the timer, but no Shatter Chest cast in the trace"
    if outcome == "kill":
        return (f"Rare Cache kept: kill at {clock(end)}, {(DEADLINE_MS - end) / 1000:.1f}s inside the"
                f" {clock(DEADLINE_MS)[:4]} timer, a hard-mode kill")
    return f"Rare Cache never decided: {outcome} at {clock(end)}, before the timer"


def cold_stacks_at_death(death: dict, settle_ms: int = 1500) -> int:
    """Biting Cold stacks on a raider as it died. The core strips auras a few ms before the death hook,
    so a row removed just before still counts; one that fell off earlier is a shed, not the death."""
    held = 0
    for row in death.get("auras") or []:
        if len(row) < 6 or row[0] != SPELL_BITING_COLD:
            continue
        if row[5] == -1 or row[5] >= death["t"] - settle_ms:
            held = max(held, row[1])
    return held


def killing_spell(death: dict) -> int:
    """The spell of the blow that killed, 0 for a white swing. Two hits can share the killing ms, a
    swing and its Frozen Blows proc, so the one matching the blow's amount wins."""
    rewind = sorted(death.get("rewind") or [], key=lambda row: row[0])
    if not rewind:
        return 0
    last = [row for row in rewind if row[0] == rewind[-1][0]]
    amount = (death.get("blow") or [None, None])[1]
    match = [row for row in last if len(row) > 3 and row[3] == amount]
    return (match or last)[-1][2]


def death_mechanic(death: dict) -> str:
    spell = killing_spell(death)
    if spell in DEATH_SPELLS:
        return DEATH_SPELLS[spell]
    return "Hodir melee" if spell == 0 else f"spell {spell}"


def button_up_at_death(death: dict, settle_ms: int = 1500) -> str | None:
    """The tank button still up as it died, or None. Same settle as the Biting Cold count."""
    for row in death.get("auras") or []:
        if len(row) >= 6 and row[0] in TANK_DEFENSIVE_AURAS and (row[5] == -1 or row[5] >= death["t"] - settle_ms):
            return TANK_DEFENSIVE_AURAS[row[0]]
    return None


def victim_stints(samples: list[tuple[int, int]], low: int, high: int) -> list[tuple[int, int, int]]:
    """`(start, stop, target)` runs of his target over `[low, high)`, each ending where the next begins.
    Samples are ~250 ms apart, so a taunt answered at once shows as a run of one sample."""
    runs: list[list[int]] = []
    for when, target in samples:
        if not low <= when < high or (runs and runs[-1][2] == target):
            continue
        if runs:
            runs[-1][1] = when
        runs.append([when, high, target])
    return [(start, stop, target) for start, stop, target in runs]


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


def frozen_blows_windows(trace: Trace, boss: int | None) -> list[tuple[int, int]]:
    """`(start, stop)` of each Frozen Blows aura. The stop runs half a second past the 20 s, since the
    last raid tick lands on the aura's final millisecond."""
    return [(rec["t"], rec["t"] + FROZEN_BLOWS_MS + 500) for rec in trace.of("cast")
            if rec.get("sp") in SPELL_FROZEN_BLOWS and rec.get("s") == boss]


def shatter_cast(trace: Trace, boss: int | None) -> int | None:
    casts = [rec["t"] for rec in trace.of("cast") if rec.get("sp") == SPELL_SHATTER_CHEST and rec.get("s") == boss]
    return min(casts) if casts else None


def tank_holder(trace: Trace) -> int | None:
    """The bot writing hodir.tankhold. Before the hold was per instance only the main tank wrote it,
    and after it every tank and ranged bot does, so the busiest writer is the main tank either way."""
    counts = collections.Counter(rec["g"] for rec in notes(trace, "hodir.tankhold"))
    tanks = [guid for guid in counts if trace.role(guid) == "tank"]
    return max(tanks, key=counts.get) if tanks else (counts.most_common(1)[0][0] if counts else None)


def hold_points(trace: Trace) -> list[tuple[int, tuple[float, float]]]:
    """`(t, point)` for where he was held, every writer's note merged in time order: one latch per instance.
    Older traces carry `hodir.centre`, the ring centre, shifted back here by the ring's offset."""
    out = []
    for rec in notes(trace, "hodir.hold"):
        parts = str(rec.get("txt", "")).split(",")
        try:
            out.append((rec["t"], (float(parts[0]), float(parts[1]))))
        except (ValueError, IndexError):
            continue
    if out:
        return sorted(out)

    for rec in notes(trace, "hodir.centre"):
        parts = str(rec.get("txt", "")).split(",")
        try:
            out.append((rec["t"], (float(parts[0]) - LEGACY_RING_ANCHOR[0] + CENTRE[0],
                                   float(parts[1]) - LEGACY_RING_ANCHOR[1] + CENTRE[1])))
        except (ValueError, IndexError):
            continue
    return sorted(out)


def held_fire_coverage(trace: Trace, boss: int, points: list[tuple[int, tuple[float, float]]],
                       windows: list[tuple[int, int, str]]) -> dict[int, tuple[int, int]]:
    """`(inside, near, live)` per window start: melee bots and pets in the held fire, within MELEE_NEAR_YD
    of him, and alive. The second share says whether the pack was even there to be covered."""
    roles = {guid: trace.role(guid) for guid in roster_guids(trace)}
    out = {low: [0, 0, 0] for low, _, _ in windows}
    for snap in frames(trace):
        window = next((low for low, high, _ in windows if low <= snap["t"] < high), None)
        if window is None:
            continue
        rows = {row[0]: row for row in snap.get("u", [])}
        him = rows.get(boss)
        fires = [(row[1], row[2]) for row in snap.get("hz", []) if row[0] == SPELL_TOASTY_FIRE]
        if not him or not fires:
            continue

        point = None
        for mark, value in points:
            if mark > snap["t"]:
                break
            point = value
        aim = point if point else (him[1], him[2])
        held = min(fires, key=lambda fire: math.hypot(fire[0] - aim[0], fire[1] - aim[1]))

        for guid, row in rows.items():
            owner = trace.owners.get(guid)
            if not owner and roles.get(guid) != "melee":
                continue
            if len(row) < 6 or row[5] <= 0:
                continue
            out[window][2] += 1
            if not inside_circle((row[1], row[2]), (him[1], him[2]), MELEE_NEAR_YD):
                continue
            out[window][1] += 1
            out[window][0] += inside_circle((row[1], row[2]), held, FIRE_RADIUS)
    return {low: tuple(counts) for low, counts in out.items()}


def melee_gaps(trace: Trace, boss: int, end: int) -> list[float]:
    """Distance to him for every live melee bot sample up to `end`. Pets are left out: they follow
    their owner, so counting them hides how far the bots themselves stood."""
    roles = {guid: trace.role(guid) for guid in bots(trace)}
    dead = first_death(trace)
    out = []
    for snap in frames(trace):
        if not 0 <= snap["t"] < end:
            continue
        rows = {row[0]: row for row in snap.get("u", [])}
        him = rows.get(boss)
        if not him:
            continue
        for guid, row in rows.items():
            if roles.get(guid) != "melee" or len(row) < 6 or row[5] <= 0:
                continue
            if snap["t"] >= dead.get(guid, end):
                continue
            out.append(math.hypot(row[1] - him[1], row[2] - him[2]))
    return out


def stack_changes(trace: Trace) -> dict[int, list[tuple[int, int]]]:
    """Biting Cold stacks per raider, as `(t, stacks)` in time order, 0 once it comes off."""
    out: dict[int, list[tuple[int, int]]] = collections.defaultdict(list)
    roster = roster_guids(trace)
    for rec in trace.of("aura"):
        if rec.get("sp") == SPELL_BITING_COLD and rec.get("d") in roster:
            out[rec["d"]].append((rec["t"], 0 if rec.get("r") else rec.get("st", 1)))
    return out


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
    print(rare_cache(outcome, fight_end(trace), shatter_cast(trace, boss_guid(trace))))


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
    windows = latch_windows(marks, 0, end)
    points = hold_points(trace)
    coverage = held_fire_coverage(trace, boss, points,
                                  [w for w in windows if w[2] not in ("corner", "centre", "offfloor")])
    print(f"  as {trace.name(holder)} read it, 0-{clock(end)[:4]}")
    for low, high, value in windows:
        kind = value if value in ("corner", "centre", "offfloor") else "fire"
        inside = [row for row in rows if low <= row[0] <= high]
        if len(inside) < 2:
            continue
        path = path_length([(row[1], row[2]) for row in inside])
        seconds = (inside[-1][0] - inside[0][0]) / 1000
        dps = (inside[0][3] - inside[-1][3]) / 100 * max_hp / seconds if seconds else 0.0
        held = sum(1 for row in inside if row[4] == holder) / len(inside)
        pack = ""
        if low in coverage and coverage[low][1]:
            covers, near, live = coverage[low]
            pack = (f"  pack {near / live * 100:.0f}% on him,"
                    f" {covers / near * 100:.0f}% of those in the fire")
        print(f"  {kind:7} {clock(low)}-{clock(high)} {(high - low) / 1000:5.1f}s  path {path:5.1f} yd"
              f"  {trace.name(holder)} holds {held * 100:3.0f}%  boss dps {dps / 1000:4.0f}k{pack}")
        totals[kind][0] += high - low
        totals[kind][1] += path
        totals[kind][2] += dps * (high - low)
    for kind, (ms, path, weighted) in totals.items():
        if ms:
            print(f"  total {kind:7} {ms / 1000:5.0f}s  {path:5.0f} yd, {path / (ms / 1000):.2f} yd/s"
                  f"  boss dps {weighted / ms / 1000:.0f}k")

    near_total = sum(near for _, near, _ in coverage.values())
    if near_total:
        covers_total = sum(covers for covers, _, _ in coverage.values())
        live_total = sum(live for _, _, live in coverage.values())
        print(f"  melee and pets: {near_total}/{live_total} ="
              f" {near_total / live_total * 100:.0f}% stood within {MELEE_NEAR_YD:.0f} yd of him,"
              f" and {covers_total} = {covers_total / near_total * 100:.0f}% of those"
              f" were inside the held fire")

    gaps = melee_gaps(trace, boss, end)
    if gaps:
        median, reach = gap_profile(gaps, MELEE_REACH_YD)
        print(f"  melee bots: gap to him p50 {median:.1f} yd, inside {MELEE_REACH_YD:.0f} yd"
              f" {reach:.0f}% of {len(gaps)} samples")

    if points:
        samples = [(row[0], row[1], row[2]) for row in rows]
        off: dict[str, list[int]] = collections.defaultdict(lambda: [0, 0])
        for low, high, value in windows:
            kind = value if value in ("corner", "centre", "offfloor") else "fire"
            away, sampled = off_point_ms(samples, points, low, high)
            off[kind][0] += away
            off[kind][1] += sampled
        print(f"  Hodir more than {OFF_POINT_YD:.0f} yd off the hold point: " + ", ".join(
            f"{kind} {away / 1000:.0f}s of {sampled / 1000:.0f}s" for kind, (away, sampled) in off.items()))

    burned = [zone for zone in zones_of(trace, SPELL_TOASTY_FIRE) if zone["t1"] - zone["t0"] >= FIRE_MIN_LIFE_MS]
    alive = union_ms([(zone["t0"], zone["t1"]) for zone in burned], 0, end)
    reach = union_ms([(zone["t0"], zone["t1"]) for zone in burned if fire_in_leash(zone["x"], zone["y"])], 0, end)
    print(f"  fire 0-{clock(end)[:4]}: alive {alive / 1000:.0f}s, inside the {HOLD_LEASH:.0f} yd leash"
          f" {reach / 1000:.0f}s, held {totals['fire'][0] / 1000 if 'fire' in totals else 0:.0f}s")

    # Keyed on the fire rather than on hodir.tankhold: the windows above say where he was parked, this
    # says what a fire burning anywhere was worth, which is the number the whole fight turns on.
    split = dps_split([(row[0], row[3]) for row in rows],
                      [(zone["t0"], zone["t1"]) for zone in burned], max_hp, end)
    if split:
        print("  boss dps " + ", ".join(
            f"{'with a fire burning' if key == 'fire' else 'with none'}"
            f" {dps / 1000:.0f}k over {ms / 1000:.0f}s" for key, (dps, ms) in sorted(split.items())))

    firsts = fire_starts(trace)
    print(f"\n  first fire after the pull {clock(min(firsts)) if firsts else '-'}")
    roster = roster_guids(trace)
    lust = sorted((rec["t"], rec["sp"]) for rec in trace.of("cast")
                  if rec.get("sp") in (SPELL_BLOODLUST, SPELL_HEROISM) and rec.get("s") in roster and rec["t"] >= 0)
    print(f"  lust {'Heroism' if lust[0][1] == SPELL_HEROISM else 'Bloodlust'} {clock(lust[0][0])}"
          if lust else "  lust none")
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

    firsts = fire_starts(trace)
    if firsts:
        ramp = ramp_ms(curve, min(firsts))
        print(f"  first fire {clock(min(firsts))[:7]}, {SINGED_CAP} stacks"
              f" {f'{ramp / 1000:.1f}s later' if ramp is not None else 'never'}")

    burned = [zone for zone in zones_of(trace, SPELL_TOASTY_FIRE) if zone["t1"] - zone["t0"] >= FIRE_MIN_LIFE_MS]
    fires = [(zone["t0"], zone["t1"]) for zone in burned]
    print(f"  0-{clock(stop)[:4]} at {SINGED_CAP} stacks {stacks_ms(curve, 0, stop, SINGED_CAP) / 1000:.0f}s,"
          f" at none with a fire burning {dark_ms(curve, fires, 0, stop) / 1000:.0f}s")


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

    boss = boss_guid(trace)
    casters = [guid for guid in bots(trace) if trace.role(guid) in ("ranged", "heal")]
    if boss is not None and casters:
        walks: dict[tuple[str, str], list[float | None]] = collections.defaultdict(list)
        for snap in frames(trace):
            if not 0 <= snap["t"] < end:
                continue
            rows = {row[0]: row for row in snap.get("u", [])}
            him = rows.get(boss)
            if not him:
                continue
            stars = [(row[1], row[2]) for row in snap.get("hz", []) if row[0] == SPELL_STARLIGHT]
            fires = [(row[1], row[2]) for row in snap.get("hz", []) if row[0] == SPELL_TOASTY_FIRE]
            for guid in casters:
                row = rows.get(guid)
                if not row or len(row) < 6 or row[5] <= 0:
                    continue
                spot, aim = (row[1], row[2]), (him[1], him[2])
                for name, here, radius, band in (("Starlight", stars, STARLIGHT_STAND_RADIUS, STARLIGHT_BAND),
                                                 ("fire", fires, FIRE_STAND_RADIUS, BAND)):
                    walks[(trace.role(guid), name)].append(
                        usable_within(spot, aim, here, radius, walk=math.inf, band=band))
        budgets = (BUFF_WALK, BUFF_WALK_UNBUFFED)
        for (role, name), got in sorted(walks.items()):
            there, within = reach_shares(got, budgets)
            print(f"  {role:6} a legal {name} stand existed on {there:.1f}% of samples, " + ", ".join(
                f"inside {budget:.0f} yd {share:.1f}%" for budget, share in zip(budgets, within)))

    rules = collections.Counter()
    marks: dict[int, list[tuple[int, str]]] = collections.defaultdict(list)
    for rec in notes(trace, "hodir.starlight"):
        rule = starlight_rule(str(rec.get("txt", "")))
        rules[rule] += 1
        marks[rec["g"]].append((rec["t"], rule))
    print("  hodir.starlight " + ", ".join(f"{rule} {count}" for rule, count in rules.most_common()))

    # Time rather than notes: a held stand can sit on one note for most of a zone's minute.
    held: collections.Counter = collections.Counter()
    lit: collections.Counter = collections.Counter()
    for guid in casters:
        for low, high, rule in latch_windows(sorted(marks.get(guid, [])), 0, min(end, dead.get(guid, end))):
            held[rule] += high - low
            lit[rule] += covered(spans["starlight"].get(guid, []), low, high)
    whole = sum(held.values())
    if whole:
        print("  caster time by Starlight rule: " + ", ".join(
            f"{rule} {ms / whole * 100:.0f}% (starlit {lit[rule] / ms * 100:.0f}%)" for rule, ms in held.most_common()))

    stands = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "hodir.stand"))
    if stands:
        print("  hodir.stand " + ", ".join(f"{kind} {count}" for kind, count in stands.most_common()))

    # The fire probe goes stale while a bot holds Starlight, so a held fire only counts with no stand.
    # Both probes also go stale while the position node stands down, so a run stops at a shed or
    # shelter move, a Storm Cloud walk, or a carry.
    fire_marks: dict[int, list[tuple[int, str]]] = collections.defaultdict(list)
    for rec in notes(trace, "hodir.firestand"):
        fire_marks[rec["g"]].append((rec["t"], starlight_rule(str(rec.get("txt", "")))))
    stand_marks: dict[int, list[tuple[int, str]]] = collections.defaultdict(list)
    for rec in notes(trace, "hodir.stand"):
        stand_marks[rec["g"]].append((rec["t"], str(rec.get("txt", ""))))
    if fire_marks:
        pauses: dict[int, list[int]] = collections.defaultdict(list)
        for rec in trace.of("act"):
            if rec.get("vd") == "OK" and rec.get("a") in PROBE_PAUSES:
                pauses[rec["g"]].append(rec["t"])
        clouds = aura_spans(trace, SPELL_STORM_CLOUD)
        runs = []
        for guid in casters:
            for low, high in both_runs(sorted(fire_marks.get(guid, [])), sorted(stand_marks.get(guid, [])), 0,
                                       min(end, dead.get(guid, end)), "held noreach", "none"):
                stop = run_until_paused(low, high, pauses.get(guid, []), clouds.get(guid, []))
                if stop > low:
                    runs.append(stop - low)
        print(f"  fire latch held out of the band with no stand: {len(runs)} runs, {sum(runs) / 1000:.0f} bot-seconds,"
              f" longest {max(runs, default=0) / 1000:.1f}s")

    boss = boss_guid(trace)
    starts = fire_starts(trace)
    landings = [cast + FREEZE_LANDS_MS for cast in freeze_casts(trace, boss)] if boss is not None else []
    if boss is not None and starts:
        first_window = (starts[0], next((landing for landing in landings if landing > starts[0]), end))
        ranged_bots = {guid for guid in bots(trace) if trace.role(guid) == "ranged"}
        shares = []
        for low, high in (first_window, (0, min(end, DEADLINE_MS))):
            hits = samples = 0
            for snap in frames(trace):
                if not low <= snap["t"] < high:
                    continue
                rows = {row[0]: row for row in snap.get("u", [])}
                him = rows.get(boss)
                fires = [(row[1], row[2]) for row in snap.get("hz", []) if row[0] == SPELL_TOASTY_FIRE]
                if not him:
                    continue
                for guid, row in rows.items():
                    if guid not in ranged_bots or len(row) < 6 or row[5] <= 0:
                        continue
                    samples += 1
                    hits += in_legal_fire((row[1], row[2]), (him[1], him[2]), fires)
            shares.append(hits / max(1, samples) * 100)
        print(f"  ranged in a fire 15-35 yd from him: first fire to the first landing"
              f" ({clock(first_window[0])[:4]}-{clock(first_window[1])[:4]}) {shares[0]:.0f}%,"
              f" 0-{clock(min(end, DEADLINE_MS))[:4]} {shares[1]:.0f}%")

    cold = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_BITING_COLD_DAMAGE and 0 <= rec["t"] < end]
    taken = sum(rec.get("a", 0) for rec in trace.of("dmg") if 0 <= rec["t"] < end)
    peak = max((rec.get("st", 0) for rec in trace.of("aura") if rec.get("sp") == SPELL_BITING_COLD and rec["t"] < end),
               default=0)
    shards = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_ICE_SHARDS and 0 <= rec["t"] < end]
    cold_total = sum(rec.get("a", 0) for rec in cold)
    print(f"  Biting Cold {cold_total:,} ({cold_total / max(1, taken) * 100:.1f}% of damage taken), peak {peak} stacks;"
          f" Ice Shards {len(shards)} hits for {sum(rec.get('a', 0) for rec in shards):,}")

    changes = stack_changes(trace)
    team = bots(trace)
    stop = min(end, DEADLINE_MS)
    early = [rec for rec in cold if rec["t"] < stop]
    early_total = sum(rec.get("a", 0) for rec in early)
    # The stack a tick was dealt at is the one standing just before it: the tick and its own stack
    # change share a millisecond.
    at_arm = sum(rec.get("a", 0) for rec in early
                 if stacks_before(changes.get(rec.get("d"), []), rec["t"] - 5) >= BITING_COLD_ARM)
    seconds = sum(band_ms(rows, 0, stop, BITING_COLD_ARM) for guid, rows in changes.items() if guid in team) / 1000
    print(f"  Biting Cold 0-{clock(stop)[:4]} {early_total:,}, {at_arm / max(1, early_total) * 100:.0f}% of it at"
          f" {BITING_COLD_ARM}+ stacks; bots spent {seconds:.0f} bot-seconds at {BITING_COLD_ARM}+")

    # At the arm point a bot should be walking or in a fire. Standing there means the shed isn't
    # getting it anywhere, which the stack totals above can't tell apart from a shed doing its job.
    fires = aura_spans(trace, [SPELL_TOASTY_FIRE])
    stars = aura_spans(trace, [SPELL_STARLIGHT])
    blows = frozen_blows_windows(trace, boss_guid(trace))
    still = {guid: still_armed_ms(rows, changes.get(guid, []), fires.get(guid, []), stars.get(guid, []), 0, stop,
                                  blows=blows)
             for guid, rows in track(trace, team, ("t", "moving")).items()}
    worst = sorted(((ms, guid) for guid, ms in still.items() if ms), reverse=True)[:3]
    print(f"  standing still at the arm point out of a fire: {sum(still.values()) / 1000:.0f} bot-seconds"
          + "".join(f"{'; ' if index == 0 else ', '}{trace.name(guid)} {ms / 1000:.1f}s"
                    for index, (ms, guid) in enumerate(worst)))

    boss = boss_guid(trace)
    gains = []
    for cast in freeze_casts(trace, boss) if boss is not None else []:
        worst = 0
        for guid, rows in changes.items():
            if guid not in team:
                continue
            before = stacks_before(rows, cast)
            during = [stacks for when, stacks in rows if cast <= when <= cast + FREEZE_LANDS_MS + 100]
            worst = max(worst, max(during + [before]) - before)
        gains.append(f"{clock(cast)[:4]} +{worst}")
    if gains:
        print("  most stacks a bot gained over a freeze cast: " + ", ".join(gains))


def show_storm(trace: Trace) -> None:
    print("STORM")
    end = fight_end(trace)
    roster = roster_guids(trace)
    rows = [(rec["t"], rec.get("d", 0), rec.get("st", 1), bool(rec.get("r")), rec.get("dur", 0))
            for rec in trace.of("aura") if rec.get("sp") in SPELL_STORM_CLOUD and rec.get("d") in roster]
    carries = [carry for carry in storm_carries(rows) if 0 <= carry["start"] < end]
    if not carries:
        print("  no Storm Cloud carried")
        return

    powered = [rec for rec in trace.of("aura")
               if rec.get("sp") in SPELL_STORM_POWER and not rec.get("r") and 0 <= rec["t"] < end]
    roles = collections.Counter(trace.role(rec.get("d")) for rec in powered)
    carriers = collections.Counter(trace.role(carry["carrier"]) for carry in carries)
    print(f"  {len(carries)} clouds, carried by {role_split(carriers)}; {len(powered)} Storm Power,"
          f" {len(powered) / len(carries):.1f} a cloud: {role_split(roles)}")

    spent = [(carry["end"] - carry["start"]) / 1000 for carry in carries if carry["spent"]]
    expired = sum(1 for carry in carries if carry["spent"] is False)
    print(f"  charges spent out on {len(spent)} (median {statistics.median(spent) if spent else 0:.1f}s),"
          f" ran out of time with charges left on {expired}")

    # A receiver inside the collect release is done walking, so it gets the buff only if the carrier
    # stands within the pulse of it. The carrier's rally comes from its own hodir.stormcloud note.
    rallies: dict[int, list[tuple[int, tuple[float, float]]]] = collections.defaultdict(list)
    for rec in notes(trace, "hodir.stormcloud"):
        parts = str(rec.get("txt", "")).split(" ")[-1].split(",")
        try:
            rallies[rec["g"]].append((rec["t"], (float(parts[0]), float(parts[1]))))
        except (ValueError, IndexError):
            continue
    holding = aura_spans(trace, SPELL_STORM_POWER)
    dealers = {guid for guid in bots(trace) if trace.role(guid) in ("melee", "ranged")}
    parked = inside = held = on_rally = 0
    for snap in frames(trace):
        when = snap["t"]
        live = [carry for carry in carries if carry["start"] <= when <= (carry["end"] or end)]
        if not live or not 0 <= when < end:
            continue
        carrier = min(carry["carrier"] for carry in live)
        marks = [point for mark, point in rallies.get(carrier, []) if mark <= when]
        rows = {row[0]: row for row in snap.get("u", [])}
        him = rows.get(carrier)
        if not marks or not him:
            continue
        held += 1
        on_rally += inside_circle((him[1], him[2]), marks[-1], STORM_PARK_YD)
        for guid in dealers - {carrier}:
            row = rows.get(guid)
            if not row or row[5] <= 0 or covered(holding.get(guid, []), when, when + 1):
                continue
            if inside_circle((row[1], row[2]), marks[-1], STORM_COLLECT_RELEASE_YD):
                parked += 1
                inside += inside_circle((row[1], row[2]), (him[1], him[2]), STORM_PULSE_YD)
    if held:
        print(f"  carrier within {STORM_PARK_YD:.0f} yd of its rally {on_rally / held * 100:.0f}% of {held} samples")
    if parked:
        print(f"  damage dealers parked at the rally without Storm Power: inside the carrier's"
              f" {STORM_PULSE_YD:.0f} yd {inside / parked * 100:.0f}% of {parked} samples")


def show_shed(trace: Trace) -> None:
    print("SHED (per role: stacks shed a second, Biting Cold ticks used, how far the shed took bots off their spot)")
    end = fight_end(trace)
    boss = boss_guid(trace)
    team = bots(trace)
    oks: dict[int, list[int]] = collections.defaultdict(list)
    for rec in trace.of("act"):
        if rec.get("a") == SHED and rec.get("vd") == "OK" and rec.get("g") in team and 0 <= rec["t"] < end:
            oks[rec["g"]].append(rec["t"])
    if not oks:
        print("  no shed")
        return

    ticks: dict[int, list[int]] = collections.defaultdict(list)
    for rec in trace.of("dmg"):
        if rec.get("sp") == SPELL_BITING_COLD_DAMAGE and rec.get("d") in team:
            ticks[rec["d"]].append(rec["t"])
    moves = collections.Counter()
    for rec in trace.of("move"):
        if rec.get("by") == SHED and rec.get("ok") and rec.get("g") in team and 0 <= rec["t"] < end:
            moves[(trace.role(rec["g"]), rec.get("k", "?"))] += 1
    cuts = collections.Counter(trace.role(rec["g"]) for rec in notes(trace, "hodir.shuttle")
                               if rec.get("txt") == "cut" and rec["t"] < end)

    changes = stack_changes(trace)
    samples = track(trace, team, ("t", "x", "y", "moving"))
    him = [(row[0], row[1], row[2]) for row in boss_rows(trace, boss)] if boss is not None else []
    stars = aura_spans(trace, [SPELL_STARLIGHT])
    clouds = aura_spans(trace, SPELL_STORM_CLOUD)
    stats: dict[str, dict[str, list]] = collections.defaultdict(lambda: collections.defaultdict(list))

    for guid, times in oks.items():
        role = trace.role(guid)
        rows = samples.get(guid, [])
        stamps = [row[0] for row in rows]
        for start, stop in shed_spans(times, changes.get(guid, [])):
            window = rows[bisect.bisect_left(stamps, start):bisect.bisect_right(stamps, stop)]
            seg = [(row[0], row[1], row[2]) for row in window]
            if len(seg) < 2:
                continue
            table = stats[role]
            table["seconds"].append((stop - start) / 1000)
            table["shed"].append(stacks_shed(changes.get(guid, []), start, stop))
            table["ticks"].append(sum(1 for when in ticks.get(guid, []) if start < when <= stop))
            table["moving"].append(sum(1 for row in window if row[3]) / len(window))
            origin = (seg[0][1], seg[0][2])
            table["offset"].append(max(math.hypot(x - origin[0], y - origin[1]) for _, x, y in seg))
            if covered(stars.get(guid, []), start, start + 1):
                table["starlight"].append(covered(stars.get(guid, []), start, stop) / max(1, stop - start))
            if role == "melee":
                kept = kept_near(seg, him, MELEE_REACH_YD)
                if kept is not None:
                    table["reach"].append(kept)
            carriers = [carrier for carrier, spans in clouds.items()
                        if carrier != guid and covered(spans, start, start + 1)]
            if carriers:
                kept = kept_near(seg, [(row[0], row[1], row[2]) for row in samples.get(carriers[0], [])],
                                 STORM_PULSE_YD)
                if kept is not None:
                    table["pulse"].append(kept)

    for role in ("ranged", "melee", "heal", "tank"):
        table = stats.get(role)
        if not table:
            continue
        seconds = sum(table["seconds"])
        shed = sum(table["shed"])
        ticked = sum(table["ticks"])
        offsets = sorted(table["offset"])
        print(f"  {role:6} {len(offsets):3} sheds, {shed} stacks in {seconds:.0f}s = {shed / max(1e-9, seconds):.2f}/s,"
              f" {shed * 2 / max(1, ticked) * 100:.0f}% of {ticked} ticks used (2 a stack),"
              f" moving {statistics.mean(table['moving']) * 100:.0f}% of samples;"
              f" off the spot p50 {statistics.median(offsets):.1f} p90 {offsets[int(0.9 * (len(offsets) - 1))]:.1f} yd;"
              f" hops {moves[(role, 'jump')]}, walked legs {moves[(role, 'point')]}, hops cut {cuts[role]}")
        for name, started in (("starlight", "in Starlight"), ("reach", "in melee reach of him"),
                              ("pulse", "inside a Storm Cloud pulse")):
            if table[name]:
                print(f"         {len(table[name])} started {started}, still there"
                      f" {statistics.mean(table[name]) * 100:.0f}% of the shed")


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
    # Which roles each mover walked. A role showing up under a node that was never meant to move it is
    # the whole symptom of a gate that stopped gating, and nothing else here says it out loud.
    by_role: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
    for rec in moves:
        by_action[rec.get("by", "?")]["ok" if rec.get("ok") else rec.get("r", "?")] += 1
        if rec.get("ok"):
            by_role[rec.get("by", "?")][trace.role(rec["g"])] += 1
    print(f"  {accepted} accepted moves")
    for action, counts in sorted(by_action.items(), key=lambda kv: -sum(kv[1].values()))[:10]:
        rest = " ".join(f"{reason}={count}" for reason, count in counts.most_common() if reason != "ok")
        print(f"    {action:34} {counts['ok']:5} ok ({counts['ok'] / max(1, accepted) * 100:4.1f}%)"
              f"  {role_split(by_role[action]):22} {rest}")

    boss = boss_guid(trace)
    casts = freeze_casts(trace, boss) if boss is not None else []
    sheltering = [rec["t"] for rec in moves if rec.get("ok") and rec.get("by") == SHELTER]
    print(f"  {after_landing(sheltering, casts)} of {len(sheltering)} shelter moves came after the freeze landed")

    issued: dict[int, list[tuple[int, float, float, str]]] = collections.defaultdict(list)
    for rec in moves:
        if rec.get("ok") and rec.get("x") is not None:
            issued[rec["g"]].append((rec["t"], rec["x"], rec["y"], rec.get("by", "?")))
    samples = track(trace, team, ("t", "x", "y", "moving"))
    stalls = collections.Counter()
    for guid, sequence in issued.items():
        stalls.update(stalled_walks(sequence, samples.get(guid, [])))
    print(f"  stalled walks (still {STALL_MS / 1000:.0f} s+, over {STALL_SHORT_YD} yd short): "
          + (", ".join(f"{action} {count}" for action, count in stalls.most_common()) or "none"))

    # Refused moves too: a leg onto the bot's own spot never gets accepted, so the stall count above
    # can't see it.
    legs: dict[int, list[tuple[int, float, float]]] = collections.defaultdict(list)
    for rec in moves:
        if rec.get("by") == SHED and rec.get("x") is not None and rec.get("k") != "jump":
            legs[rec["g"]].append((rec["t"], rec["x"], rec["y"]))
    nowhere = sum(own_spot_legs(sequence, samples.get(guid, [])) for guid, sequence in legs.items())
    print(f"  shed legs sent to where the bot already stood (under {OWN_SPOT_YD} yd): {nowhere} of"
          f" {sum(len(sequence) for sequence in legs.values())}")

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


def dps_target_windows(trace: Trace) -> dict[int, list[tuple[int, int, int]]]:
    """Per bot, `(low, high, target)` for each window hodir.dpstarget named a guid. Humans are left
    out: nothing assigns them a target."""
    marks: dict[int, list[tuple[int, str]]] = collections.defaultdict(list)
    for rec in notes(trace, "hodir.dpstarget"):
        if rec["g"] not in trace.humans:
            marks[rec["g"]].append((rec["t"], str(rec.get("txt", "")).split(" ")[-1]))

    end = pull_end(trace)
    out: dict[int, list[tuple[int, int, int]]] = collections.defaultdict(list)
    for guid, rows in marks.items():
        rows.sort()
        for index, (when, value) in enumerate(rows):
            if value.isdigit():
                out[guid].append((when, rows[index + 1][0] if index + 1 < len(rows) else end, int(value)))
    return out


def block_target_spans(trace: Trace, blocks: set[int]) -> dict[int, list[tuple[int, int]]]:
    """Per bot, the windows hodir.dpstarget named one of `blocks` - the time it was shooting ice
    instead of Hodir."""
    return {guid: [(low, high) for low, high, target in windows if target in blocks]
            for guid, windows in dps_target_windows(trace).items()}


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
    health = boss_rows(trace, boss) if boss is not None else []
    targets = dps_target_windows(trace)
    started: dict[int, list[dict]] = collections.defaultdict(list)
    for rec in trace.of("cast"):
        if not rec.get("tr") and rec.get("s") in targets and rec.get("t", -1) >= 0:
            started[rec["s"]].append(rec)
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
        hp = health_at(health, spawned)
        at = f", Hodir {hp:.1f}%" if hp is not None else ""
        print(f"  {label}: {len(batch)} blocks from {clock(spawned)}{at}, next fire {fire}")

        # Only the mage block holds up the next fire, so bot-time on ice after the last one is down is
        # time the boss could have had. Split out because the two are worth arguing about separately.
        spans = block_target_spans(trace, {guid for guid, _ in batch})
        mages = [rows[-1][0] for guid, rows in batch if kinds.get(guid) == "mage"]
        stop = min(high, end)
        on_ice = sum(covered(rows, spawned, stop) for rows in spans.values())
        after = sum(covered(rows, max(mages), stop) for rows in spans.values()) if mages else 0
        if on_ice:
            tail = (f", {after / 1000:.0f}s of it after the last mage was free" if mages
                    else " (no mage block named in this trace)")
            print(f"    bots on ice {on_ice / 1000:.0f}s{tail}")

        # Every second a mage breaker casts elsewhere is a second later the wave's fire comes back.
        lives = {guid: (rows[0][0], rows[-1][0]) for guid, rows in batch if kinds.get(guid) == "mage"}
        split: collections.Counter = collections.Counter()
        for guid, windows in targets.items():
            for start, stop, block in windows:
                if block not in lives:
                    continue
                born, died = lives[block]
                split += breaker_cast_split(
                    [(rec.get("ct", 0), rec.get("tgt")) for rec in started[guid]
                     if max(start, born) <= rec["t"] < min(stop, died)],
                    block, set(blocks), boss)
        if split:
            own = split.pop("own", 0)
            off = sum(split.values())
            where = ": " + ", ".join(f"{place} {ms / 1000:.1f}" for place, ms in split.most_common()) if off else ""
            print(f"    mage breakers cast on their block {own / 1000:.1f}s, elsewhere {off / 1000:.1f}s"
                  f" ({off / (own + off) * 100:.0f}%){where}")

        for guid, rows in sorted(batch, key=lambda item: item[1][-1][0]):
            life = (rows[-1][0] - rows[0][0]) / 1000
            print(f"    {kinds.get(guid, '?'):6} {life:5.1f}s  died {clock(rows[-1][0])}")

    # Ranged shooting ice, read off the snapshot target: a breaker walked off its block deals nothing
    # to it, and the wave holds up the next fire for as long as the block lives.
    ice = guids_of_entry(trace, NPC_HELPER_BLOCK) | guids_of_entry(trace, NPC_TRAPPED_BLOCK)
    movers: dict[int, list[tuple[int, str]]] = collections.defaultdict(list)
    for rec in trace.of("move"):
        if rec.get("ok") == 1:
            movers[rec["g"]].append((rec["t"], str(rec.get("by", ""))))
    stamps = {guid: [when for when, _ in rows] for guid, rows in movers.items()}
    samples: list[tuple[bool, str, float | None]] = []
    dealt = collections.Counter()
    last: dict[int, tuple[int, int, int]] = {}
    for snap in frames(trace):
        when = snap["t"]
        rows = {row[0]: row for row in snap.get("u", [])}
        for guid, row in rows.items():
            if trace.role(guid) != "ranged" or guid in trace.humans or len(row) <= 11:
                continue
            if 0 <= when < end and guid in last and row[5] > 0:
                place = "ice" if last[guid][2] in ice else ("boss" if last[guid][2] == boss else "")
                if place:
                    dealt[place] += row[11] - last[guid][1]
                    dealt[place + " ms"] += when - last[guid][0]
            last[guid] = (when, row[11], row[7])
            if not 0 <= when < end or row[5] <= 0 or row[7] not in ice:
                continue
            index = bisect.bisect_right(stamps.get(guid, []), when) - 1
            mover = movers[guid][index][1] if index >= 0 and when - movers[guid][index][0] < MOVER_WINDOW_MS else ""
            block = rows.get(row[7])
            gap = math.hypot(row[1] - block[1], row[2] - block[2]) if block else None
            samples.append((bool(row[8]), mover, gap))
    if samples:
        moving, by_position, distant = breaker_motion(samples)
        rate = {place: dealt[place] / max(1, dealt[place + " ms"]) for place in ("ice", "boss")}
        print(f"  ranged shooting ice {dealt['ice ms'] / 1000:.0f} bot-seconds, {rate['ice']:.1f}k a second against"
              f" {rate['boss']:.1f}k on Hodir; moving {moving:.0f}% of it, walked by the position node {by_position:.0f}%,"
              f" over {BLOCK_FAR_YD:.0f} yd from the block {distant:.0f}%")


def show_deaths(trace: Trace) -> None:
    print("DEATHS")
    deaths = combat_deaths(trace)
    if not deaths:
        print("  none")
        return
    blows = frozen_blows_windows(trace, boss_guid(trace))
    inside = [d for d in deaths if any(start <= d["t"] < stop for start, stop in blows)]
    counts = collections.Counter(death_mechanic(d) for d in deaths)
    print("  " + ", ".join(f"{name} {n}" for name, n in counts.most_common())
          + f"; {len(inside)} of {len(deaths)} inside a Frozen Blows window")
    for death in deaths:
        where = "in Frozen Blows" if death in inside else "outside"
        print(f"  {clock(death['t'])}  {trace.name(death['g']):10} {trace.role(death['g']):6}"
              f" {death_mechanic(death):22} Biting Cold x{cold_stacks_at_death(death)}  {where}")


def show_tanks(trace: Trace) -> None:
    print("TANKS (per Frozen Blows window: time he spent on each tank, swings landed, buttons and who cast them)")
    boss = boss_guid(trace)
    tanks = sorted(guid for guid in roster_guids(trace) if trace.role(guid) == "tank")
    if boss is None or not tanks:
        print("  no Hodir or no tanks in the trace")
        return
    end = fight_end(trace)
    blows = [(start, min(stop, end)) for start, stop in frozen_blows_windows(trace, boss) if start < end]
    targets = [(row[0], row[4]) for row in boss_rows(trace, boss)]
    swings = [rec for rec in trace.of("dmg") if rec.get("sp") in SPELL_FROZEN_BLOWS_SWING and rec.get("d") in tanks]
    # The verdict of the action that cast it lands on the cast's millisecond or the next one.
    acted = {(rec["g"], rec["t"]): rec["a"] for rec in trace.of("act") if rec.get("vd") == "OK"}
    casts = [rec for rec in trace.of("cast")
             if rec.get("sp") in TANK_DEFENSIVES and rec.get("s") in tanks and rec["t"] < end]
    probes = [rec for rec in notes(trace, "hodir.defensive") if rec.get("g") in tanks]

    def button(rec: dict) -> str:
        by = "node" if TANK_DEFENSIVE_NODE in (acted.get((rec["s"], rec["t"])), acted.get((rec["s"], rec["t"] + 1))) \
            else "class"
        return f"{TANK_DEFENSIVES[rec['sp']]} {clock(rec['t'])[:7]} {by}"

    for start, stop in blows:
        stints = victim_stints(targets, start, stop)
        print(f"  {clock(start)[:7]}")
        for tank in tanks:
            mine = [(low, high) for low, high, target in stints if target == tank]
            hits = [rec.get("a", 0) for rec in swings if rec["d"] == tank and start <= rec["t"] < stop]
            spent = [button(rec) for rec in casts if rec["s"] == tank and start <= rec["t"] < stop]
            seen = [rec["txt"] for rec in probes if rec["g"] == tank and start <= rec["t"] < stop
                    and rec["txt"] not in ("idle", "wait")]
            print(f"    {trace.name(tank):10} held {sum(high - low for low, high in mine) / 1000:4.1f}s in"
                  f" {len(mine)} stint(s), {len(hits)} swing(s) {sum(hits):,}; "
                  + (", ".join(spent) or "no button")
                  + (f"  [hodir.defensive: {', '.join(dict.fromkeys(seen))}]" if seen else ""))

    outside = [button(rec) + f" ({trace.name(rec['s'])})" for rec in casts
               if not any(start <= rec["t"] < stop for start, stop in blows)]
    print("  outside every window: " + (", ".join(outside) or "none"))
    for death in combat_deaths(trace):
        if death["g"] not in tanks:
            continue
        where = "in Frozen Blows" if any(start <= death["t"] < stop for start, stop in blows) else "outside"
        print(f"  died {clock(death['t'])} {trace.name(death['g']):10} {death_mechanic(death):20} {where},"
              f" button up: {button_up_at_death(death) or 'none'}")


SECTIONS = (
    ("pace", "health, boss dps per 15 s, 0-3:00 against the cache pace", show_pace),
    ("hold", "hodir.tankhold windows and the fire gaps after each freeze", show_hold),
    ("singed", "Singed on the boss by caster kind, modelled stacks", show_singed),
    ("buffs", "Starlight, fire, Storm Power, Biting Cold by role", show_buffs),
    ("storm", "Storm Cloud carries, Storm Power per cloud, collect walks against the carrier", show_storm),
    ("shed", "Biting Cold sheds by role: stacks a second, ticks used, hops, distance off the spot", show_shed),
    ("churn", "walking undone, A-B-A flips, moves by action, dodge walk-backs", show_churn),
    ("blocks", "helper ice blocks per freeze and the next fire", show_blocks),
    ("deaths", "each death's killing spell, Biting Cold stacks and Frozen Blows window", show_deaths),
    ("tanks", "per Frozen Blows window: his time on each tank, swings, defensives cast", show_tanks),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
