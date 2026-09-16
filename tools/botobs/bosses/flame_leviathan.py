#!/usr/bin/env python3
"""Score a Flame Leviathan pull from a RaidObs trace.

    flame_leviathan.py <file>            every section
    flame_leviathan.py <file> --hulls    why the vehicles died, crews on foot, drive branches, Pursued
    flame_leviathan.py <file> --pyrite   Blue Pyrite per demolisher: stacks, barrel decisions, losses, crates
    flame_leviathan.py <file> --ram      Battering Ram exposure
    flame_leviathan.py <file> --fury     Hodir's Fury: chase, fuse, strike, and the dodge scan
    flame_leviathan.py <file> --inferno  Mimiron's Inferno trail, measured to each hull's reach
    flame_leviathan.py <file> --adds     Freya's Ward adds, and whether the world DB lets them live
    flame_leviathan.py <file> --corners  corner containment, one row per ward wave
    flame_leviathan.py <file> --vents    Flame Vents channels and interrupts

**Battering Ram (62376)** is cast `me->CastSpell(me->GetVictim(), ...)` when the boss is
`IsWithinCombatRange(victim, 15.0f)`, and its ImplicitTargetA is 53 = TARGET_DEST_TARGET_ENEMY with
EffectRadiusIndex 20. It is therefore a 25 yd sphere centred on the **pursued vehicle**, not a cone
off the boss's front. `--ram` scores every other vehicle against that sphere, the same test the module
backs off on (`FlameLeviathanInBatteringRamBlast`), with the pursued vehicle read from `fl.pursued`.

**Hodir's Fury** follows a random target at 12 yd/s and commits only once that target has stopped:
`FollowMovementGenerator` informs on a finished spline with the target within 0.5 yd. It then stuns
itself, summons the strike NPC 33212 overhead 5 s later, and 62297 lands ~1.1 s after that - a 10 yd
blast with an undispellable 60 s stun, flat from the hull's centre. It stays stunned 5 s more before
it retargets. `--fury` splits the time hulls spent inside the dodge scan by what the reticle was doing.

**Mimiron's Inferno** is not one circle. `npc_mimirons_inferno` (33369) walks a waypoint path and
every 2 s summons NPC 33370, each of which burns for 30 s - a line of about fifteen 9 yd patches. The
patches are dynamic objects (62910), which is how `snap.hz` sees them, and `DynObjAura::FillTargetMap`
adds both object sizes: a siege engine (7.7) burns out to 17.1 yd from a patch centre.

**Freya's Ward** spawns four wards 30 s in, at the arena corners, each firing a wave every 29 s.
`npc_freya_ward_summon` keeps each add until it dies and zone-engages it. That binding is world DB
update 2026_09_10_03; without it the adds run SmartAI and despawn at their summon duration, 3 s for a
Ward of Life and 10 s for a Lasher. `--adds` and `--corners` detect that and say so.

**Blue Pyrite (68605)** is one stack per landed Hurl Pyrite Barrel, 10 s, 10 stacks, each landing
resetting the duration. `fl.barrel` names the driver's decision: `burst`, `refresh`, `hold`, `dry` (no
energy), `fail` (the core refused the cast: range, facing, LOS) or `not boss`. `--pyrite` blames every
stack that falls to 0 on the worst decision in the 10 s before it, and counts barrels and crate credits
off the demolisher's v13 power column. Every Grab Crate hit credits +25 until the crate despawns
1300 ms later, repeats included.

**Flame Vents (62396)** is a 10 s self-channel every 20 s that ticks 63847 eleven times. Only
Electroshock stops it (`boss_flame_leviathan.cpp` breaks the channel on spell 62522 hitting him), so
a channel with fewer than eleven ticks is an interrupt. `--vents` counts those, reads the `fl.vent`
notes, and names channels nobody fired at.

Things this file will not tell you, each of which has already fooled a reading of these traces:

- **The snapshot cast column only catches spells with a cast time.** Ram, Mortar and Fire Cannon are
  instant, so they never appear in it.
- **`dmg` rows before v13 are written for roster players only**, so on those traces damage to a hull
  cannot be attributed to a spell. The hull-attrition figures come from `snap` health deltas, which
  is why they are shares and rates per 5 s; v13 adds the spell breakdown beside them.
- **Riders are not at their vehicle's exact coordinates.** A turret gunner sits 0.20 yd off its
  hull, so crews are matched to hulls by proximity, never by equal positions.
- **Pre-fix traces flap `fl.pursued`.** A bot beyond SightDistance used to reset the shared state, so
  the same hull drops out for a few hundred ms and comes back. Spans are merged across that.
"""
from __future__ import annotations

import bisect
import collections
import math
import pathlib
import statistics
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs import geometry  # noqa: E402
from raidobs.cli import run_sections  # noqa: E402
from raidobs.probes import holder_spans, latch_spans  # noqa: E402
from raidobs.trace import Trace, clock, death_records, roster_guids  # noqa: E402

BOSS_ENTRY = 33113
RETICLE_ENTRY = 33108           # Hodir's Fury's reticle
FURY_STRIKE_ENTRY = 33212       # summoned overhead when the fuse runs out
HAMMER_ENTRIES = {33364, 33365}

SIEGE, DEMOLISHER, CHOPPER = 33060, 33109, 33062
# creature_model_info.CombatReach, which is what WorldObject::GetObjectSize returns.
VEHICLE_SIZE = {SIEGE: 7.7, DEMOLISHER: 2.25, CHOPPER: 1.0}
VEHICLE_NAME = {SIEGE: "siege", DEMOLISHER: "demolisher", CHOPPER: "chopper"}
DEFAULT_OBJECT_SIZE = 0.389     # DEFAULT_WORLD_OBJECT_SIZE, a dynamic object's own size

BOSS_REACH = 15.0
RAM_RADIUS = 25.0
RAM_CAST_RANGE = 15.0           # boss_flame_leviathan.cpp: IsWithinCombatRange(victim, 15.0f)
FURY_RADIUS = geometry.radius("ULDUAR_FL_FURY_RADIUS")
HAMMER_RADIUS = geometry.radius("ULDUAR_FL_HAMMER_RADIUS")
INFERNO_RADIUS = geometry.radius("ULDUAR_FL_INFERNO_RADIUS")
HAZARD_MARGIN = geometry.radius("ULDUAR_FL_TOWER_HAZARD_MARGIN")
FURY_ARMED_MS = 6500            # ULDUAR_FL_FURY_ARMED_MS
FURY_STRIKE_WINDOW_MS = 4000    # how long after 33212 appears a 62297 row still belongs to it
SPELL_FURY_STUN = 62297

ADD_ENTRIES = {33387: "Writhing Lasher", 34275: "Ward of Life"}
# 62947 -> 33387 DurationIndex 1 and 62907 -> 34275 DurationIndex 27. An add gone at full health after
# this long timed out, which it only does without npc_freya_ward_summon.
ADD_SUMMON_MS = {33387: 10000, 34275: 3000}
ADD_TIMEOUT_SLACK_MS = 700
LASH_SPELL = 65062

VENT_TICK = 63847               # SPELL_FLAME_VENTS_TRIGGER, one cast per tick of the 62396 channel
VENT_TICKS_FULL = 11            # what a channel that runs its whole 10 s emits
VENT_GAP_MS = 4000              # ticks are ~1 s apart, so a longer gap is a new channel

INFERNO_SPELL = 62910           # the ground fire, a dynamic object rather than a creature

GRAB_CRATE = 62482
CRATE_CREDIT_SPELL = 62496      # cast once per Grab Crate hit, repeats on the same crate included
CRATE_DESPAWN_MS = 1300         # spell_vehicle_grab_pyrite's despawn delay
CRATE_GRAB_CEILING = 75
CRATE_CREDIT = 25
BARREL_RANGE = 70.0
BARREL_COST = 5
BARREL_STATES = ("burst", "refresh", "hold", "dry", "fail", "not boss")
LOSS_WINDOW_MS = 10000          # Blue Pyrite's duration: whatever stopped the refresh happened inside it

DRIVE_ACTION = "flame leviathan drive"
VENT_ACTION = "flame leviathan interrupt vents"
RUSH_SPEED = 20.0               # yd/s: no hull drives this fast, so a faster step is a Steam Rush
PURSUED_FLAP_MS = 1500

# The four NPC_FREYA_WARD_TARGET spawn points, boss_flame_leviathan.cpp SummonTowerHelpers.
ARENA_CORNERS = [(159.4, 64.1), (382.9, 74.0), (374.0, -141.0), (157.7, -140.3)]
CORNER_STANDOFF = geometry.radius("ULDUAR_FL_CORNER_STANDOFF")
CORNER_HOLD_RADIUS = geometry.radius("ULDUAR_FL_CORNER_HOLD_RADIUS")
POSTED_YD = 15.0                # a siege hull this close to its post point is holding it
SPAWN_YD = 10.0                 # measured spawns land 0-7.6 yd from the corner point
WAVE_WINDOW_MS = 3000
KNOCKBACK_YD = 10.0             # an add moving this far inside a second was thrown

# Weapon bands off Spell.dbc, as (vehicle entry, min, max, label). A cone is scored on range alone,
# so the cone rows are an upper bound - facing is the action's problem, not this table's.
WEAPON_BANDS = [
    (SIEGE, 10.0, 70.0, "siege turret, Fire Cannon"),
    (SIEGE, 0.0, 15.0, "siege driver, Ram cone"),
    (DEMOLISHER, 0.0, 50.0, "demo gunner, Mortar"),
    (DEMOLISHER, 10.0, 70.0, "demo driver, Hurl Boulder"),
    (DEMOLISHER, 0.0, 15.0, "demo driver, Ram cone"),
    (CHOPPER, 0.0, 35.0, "chopper, Sonic Horn cone"),
]
SPLASH_YD = 20.0                # Fire Cannon and Hurl Boulder, EffectRadiusIndex 9
MELEE_YD = 8.0                  # close enough for Lash (0-5 yd) plus a snapshot's worth of travel
ATTRITION_WINDOW_MS = 5000
CREW_YD = 8.0


def pick(seq, quantile):
    return seq[min(int(len(seq) * quantile), len(seq) - 1)]


def post_point(index: int) -> tuple[float, float]:
    """FlameLeviathanCornerPostPoint: ULDUAR_FL_CORNER_STANDOFF in from the corner, toward the centre."""
    cx = sum(x for x, _ in ARENA_CORNERS) / len(ARENA_CORNERS)
    cy = sum(y for _, y in ARENA_CORNERS) / len(ARENA_CORNERS)
    x, y = ARENA_CORNERS[index]
    length = math.hypot(cx - x, cy - y) or 1.0
    return x + (cx - x) / length * CORNER_STANDOFF, y + (cy - y) / length * CORNER_STANDOFF


def inferno_reach(radius: float, entry: int) -> float:
    """How far from a patch centre a hull of this entry burns: both object sizes are added."""
    return radius + DEFAULT_OBJECT_SIZE + VEHICLE_SIZE.get(entry, 0.0)


class Frame:
    """One snapshot, split by what each row is."""

    def __init__(self, snap, ents, roster):
        self.t = snap["t"]
        self.boss = None        # (x, y, o, target)
        self.boss_hp = None
        self.hulls = {}         # guid -> (x, y, hp, entry, moving, power or None)
        self.bots = {}          # guid -> (x, y, hp)
        self.adds = {}          # guid -> (x, y, hp, target)
        self.reticles = {}      # guid -> (x, y)
        self.strikes = set()    # Hodir's Fury strike NPCs present
        self.hammers = []       # (x, y)
        self.fires = [(row[1], row[2], row[4] or INFERNO_RADIUS)
                      for row in (snap.get("hz") or []) if row[0] == INFERNO_SPELL]

        for row in snap.get("u", []):
            guid, entry = row[0], ents.get(row[0])
            # Columns 7 and 8 arrived in v8 and 12-13 in v13, so a short row is an older trace, not a
            # malformed one. Reading past its end takes down every view in this file at once.
            if entry == BOSS_ENTRY:
                self.boss = (row[1], row[2], row[4], row[7] if len(row) > 7 else 0)
                self.boss_hp = row[5]
            elif entry in VEHICLE_SIZE:
                self.hulls[guid] = (row[1], row[2], row[5], entry, row[8] if len(row) > 8 else 0,
                                    row[13] if len(row) > 13 else None)
            elif entry in ADD_ENTRIES:
                self.adds[guid] = (row[1], row[2], row[5], row[7] if len(row) > 7 else 0)
            elif entry == RETICLE_ENTRY:
                self.reticles[guid] = (row[1], row[2])
            elif entry == FURY_STRIKE_ENTRY:
                self.strikes.add(guid)
            elif entry in HAMMER_ENTRIES:
                self.hammers.append((row[1], row[2]))
            elif guid in roster:
                self.bots[guid] = (row[1], row[2], row[5])


class Fight:
    """Everything the views share, built once per trace."""

    def __init__(self, trace: Trace):
        self.trace = trace
        self.roster = roster_guids(trace)
        self.frames = [frame for frame in (Frame(snap, trace.entries, self.roster)
                                           for snap in trace.of("snap") if snap["t"] >= 0) if frame.boss]
        self.times = [frame.t for frame in self.frames]
        self.end = self.times[-1] if self.times else 0
        self.deaths: dict[int, int] = {}
        for rec in death_records(trace):
            self.deaths.setdefault(rec["g"], rec["t"])
        self.pursued = pursued_spans(trace, self)
        self._crews = None
        self._drivers = None
        self._hull_life = None

    def frame_at(self, when: int, tol: int = 1500):
        """The last frame at or before `when`, if it is no older than `tol`."""
        index = bisect.bisect_right(self.times, when) - 1
        if index < 0 or when - self.times[index] > tol:
            return None
        return self.frames[index]

    def frame_after(self, when: int, tol: int = 400):
        """The first frame at or after `when`, if it is no later than `tol`."""
        index = bisect.bisect_left(self.times, when)
        if index >= len(self.frames) or self.times[index] - when > tol:
            return None
        return self.frames[index]

    def pursued_at(self, when: int):
        for guid, start, stop in self.pursued:
            if start <= when < stop:
                return guid
        return None

    def hull_life(self) -> dict[int, dict]:
        """guid -> {entry, first, gone, died}: `gone` is the first frame at 0 hp, or the last sighting."""
        if self._hull_life is None:
            life: dict[int, dict] = {}
            for frame in self.frames:
                for guid, (_x, _y, hp, entry, _mv, _pw) in frame.hulls.items():
                    cell = life.setdefault(guid, {"entry": entry, "first": frame.t, "gone": None, "last": frame.t})
                    cell["last"] = frame.t
                    if hp <= 0 and cell["gone"] is None:
                        cell["gone"] = frame.t
            for cell in life.values():
                cell["died"] = cell["gone"] is not None or cell["last"] < self.end - 1000
                cell["gone"] = cell["gone"] if cell["gone"] is not None else cell["last"]
            self._hull_life = life
        return self._hull_life

    def crews(self) -> dict[int, int]:
        """bot -> the hull it rode most in the first minute, by proximity."""
        if self._crews is None:
            seen: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
            for frame in self.frames:
                if frame.t > 60000:
                    break
                for bot, (bx, by, _hp) in frame.bots.items():
                    near = geometry.nearest((bx, by), {g: (h[0], h[1]) for g, h in frame.hulls.items() if h[2] > 0})
                    if near and near[1] <= CREW_YD:
                        seen[bot][near[0]] += 1
            self._crews = {bot: counts.most_common(1)[0][0] for bot, counts in seen.items() if counts}
        return self._crews

    def drivers(self) -> dict[int, int]:
        """hull -> the bot whose drive moves were issued from it."""
        if self._drivers is None:
            votes: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
            for rec in self.trace.of("move"):
                if rec.get("by") != DRIVE_ACTION:
                    continue
                frame = self.frame_at(rec["t"])
                bot = frame.bots.get(rec.get("g")) if frame else None
                if not bot:
                    continue
                near = geometry.nearest((bot[0], bot[1]), {g: (h[0], h[1]) for g, h in frame.hulls.items()})
                if near and near[1] <= CREW_YD:
                    votes[near[0]][rec["g"]] += 1
            self._drivers = {hull: counts.most_common(1)[0][0] for hull, counts in votes.items()}
        return self._drivers


def fight(trace: Trace) -> Fight:
    cached = getattr(trace, "_fl_fight", None)
    if cached is None:
        cached = Fight(trace)
        trace._fl_fight = cached
    return cached


def pursued_spans(trace: Trace, fl: Fight) -> list[tuple[int, int, int]]:
    """`(hull, start, stop)` per Pursued span, from `fl.pursued`, with the same hull coming back inside
    PURSUED_FLAP_MS merged into one span. Falls back to the boss's target column."""
    spans: list[list[int]] = []
    for held, start, stop in latch_spans(trace, "fl.pursued", fl.end):
        if not held.isdigit() or held == "0":
            continue
        guid = int(held)
        if spans and spans[-1][0] == guid and start - spans[-1][2] <= PURSUED_FLAP_MS:
            spans[-1][2] = stop
        else:
            spans.append([guid, start, stop])
    if spans:
        return [tuple(span) for span in spans]

    for frame in fl.frames:
        target = frame.boss[3]
        if target not in frame.hulls:
            continue
        if spans and spans[-1][0] == target and frame.t - spans[-1][2] <= PURSUED_FLAP_MS:
            spans[-1][2] = frame.t
        else:
            spans.append([target, frame.t, frame.t])
    return [tuple(span) for span in spans]


def value_at(spans, when: int, default=None):
    for held, start, stop in spans or ():
        if start <= when < stop:
            return held
    return default


def edge_gap(hull, boss) -> float:
    return math.hypot(hull[0] - boss[0], hull[1] - boss[1]) - BOSS_REACH - VEHICLE_SIZE[hull[3]]


# ---------------------------------------------------------------------------------------------------
# --hulls
# ---------------------------------------------------------------------------------------------------

CAUSES = ("Mimiron's Inferno", "Thorim's Hammer", "Battering Ram, pursued", "Battering Ram, sphere",
          "adds within 8 yd", "Flame Vents", "nothing positional (Missile Barrage, turrets)")


def vent_channels(trace: Trace):
    """Flame Vents channels as (start, end, ticks). Electroshock is the only thing that ends one early."""
    ticks = sorted(rec["t"] for rec in trace.of("cast") if rec.get("sp") == VENT_TICK)
    out, cur = [], []
    for t in ticks:
        if cur and t - cur[-1] > VENT_GAP_MS:
            out.append((cur[0], cur[-1], len(cur)))
            cur = []
        cur.append(t)
    if cur:
        out.append((cur[0], cur[-1], len(cur)))
    return out


def hull_losses(fl: Fight):
    """Hull health lost per snapshot interval, attributed to the first matching cause.

    Returns (by cause, by hull and cause, hull-seconds by cause), all in health percentage points.
    """
    channels = vent_channels(fl.trace)
    by_cause: collections.Counter = collections.Counter()
    by_hull: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
    seconds: collections.Counter = collections.Counter()
    for prev, frame in zip(fl.frames, fl.frames[1:]):
        dt = frame.t - prev.t
        if not 0 < dt <= 1500:
            continue
        venting = any(start - 1000 <= frame.t <= stop + 1000 for start, stop, _ in channels)
        pursued = fl.pursued_at(frame.t)
        victim = prev.hulls.get(pursued)
        boss = prev.boss
        for guid, now in frame.hulls.items():
            was = prev.hulls.get(guid)
            if not was or was[2] <= 0:
                continue
            drop = max(0.0, was[2] - now[2])
            x, y, _hp, entry = was[:4]
            size = VEHICLE_SIZE[entry]
            in_ram_range = victim and math.hypot(victim[0] - boss[0], victim[1] - boss[1]) <= (
                BOSS_REACH + RAM_CAST_RANGE + VEHICLE_SIZE[victim[3]] + 3)
            if any(math.hypot(x - fx, y - fy) <= inferno_reach(r, entry) for fx, fy, r in prev.fires):
                cause = CAUSES[0]
            elif any(math.hypot(x - hx, y - hy) <= HAMMER_RADIUS + 3 for hx, hy in prev.hammers):
                cause = CAUSES[1]
            elif guid == pursued and in_ram_range:
                cause = CAUSES[2]
            elif victim and in_ram_range and math.hypot(x - victim[0], y - victim[1]) <= RAM_RADIUS + size:
                cause = CAUSES[3]
            elif any(math.hypot(x - a[0], y - a[1]) <= MELEE_YD + size for a in prev.adds.values() if a[2] > 0):
                cause = CAUSES[4]
            elif venting:
                cause = CAUSES[5]
            else:
                cause = CAUSES[6]
            by_cause[cause] += drop
            by_hull[guid][cause] += drop
            seconds[cause] += dt / 1000.0
    return by_cause, by_hull, seconds


def show_hulls(trace: Trace) -> int:
    fl = fight(trace)
    life = fl.hull_life()
    print("Hulls: what took their health, who rode them, and how each Pursued escape went\n")
    if not life:
        print("  no vehicle rows in this trace")
        return 0

    by_cause, by_hull, seconds = hull_losses(fl)
    total = sum(by_cause.values())
    died = sum(1 for cell in life.values() if cell["died"])
    print(f"  hulls seen {len(life)}, died {died}; hull health lost {total:.0f} points ({total / 100:.1f} hulls)")
    print("\n  share of hull health lost, first matching cause per snapshot interval:")
    for cause, lost in by_cause.most_common():
        rate = 5 * lost / seconds[cause] if seconds[cause] else 0.0
        print(f"     {cause:44s} {100 * lost / total:5.1f}%   {rate:5.2f} pts/5 s over {seconds[cause]:5.0f} hull-s")

    # ---- by spell, off the v13 hull dmg rows ------------------------------------------------------
    maxhp = trace.maxhp
    hits = [rec for rec in trace.of("dmg") if rec.get("d") in life]
    if hits:
        spent = sum(by_hull[guid][cause] * maxhp.get(guid, 0) / 100.0
                    for guid in by_hull for cause in by_hull[guid])
        by_spell = collections.Counter()
        for rec in hits:
            by_spell[rec.get("sp")] += rec.get("a", 0)
        logged = sum(by_spell.values())
        cover = f"{100 * logged / spent:.0f}% of the health lost" if spent else "hull max health unknown"
        print(f"\n  hull damage by spell ({len(hits)} dmg rows, {logged:,} damage, {cover}):")
        for spell, amount in by_spell.most_common(10):
            print(f"     {trace.spell(spell):40s} {100 * amount / logged:5.1f}%")
    else:
        print("\n  no hull dmg rows: a pre-v13 trace, so the causes above are positional only")

    # ---- per hull ---------------------------------------------------------------------------------
    crews = fl.crews()
    riders: dict[int, list[int]] = collections.defaultdict(list)
    for bot, hull in crews.items():
        riders[hull].append(bot)
    print(f"\n  {'hull':>6s} {'class':10s} {'gone':>9s}  {'top causes':52s} crew")
    for guid, cell in sorted(life.items(), key=lambda kv: kv[1]["gone"]):
        top = ", ".join(f"{cause.split(',')[0].split(' (')[0]} {lost:.0f}" for cause, lost in by_hull[guid].most_common(2))
        gone = clock(cell["gone"]) if cell["died"] else "alive"
        crew = " ".join(trace.name(bot) for bot in sorted(riders[guid], key=trace.name))
        print(f"  {guid & 0xffffffff:6d} {VEHICLE_NAME[cell['entry']]:10s} {gone:>9s}  {top:52s} {crew}")

    # ---- crews on foot ----------------------------------------------------------------------------
    delays, aboard = [], 0
    for bot, hull in crews.items():
        death = fl.deaths.get(bot)
        if death is None or bot in trace.humans:
            continue
        last_on = None
        for frame in fl.frames:
            if frame.t > death:
                break
            spot, ride = frame.bots.get(bot), frame.hulls.get(hull)
            if spot and ride and ride[2] > 0 and math.hypot(spot[0] - ride[0], spot[1] - ride[1]) <= CREW_YD:
                last_on = frame.t
        if last_on is None:
            continue
        delay = (death - last_on) / 1000.0
        if delay <= 0.5:
            aboard += 1
        else:
            delays.append(delay)
    if delays or aboard:
        delays.sort()
        middle = f", median {statistics.median(delays):.1f} s after leaving the hull" if delays else ""
        print(f"\n  bot deaths: {aboard} aboard, {len(delays)} on foot{middle}")

    show_drive_branches(fl)
    show_pursued(fl)
    return 0


def show_drive_branches(fl: Fight) -> None:
    trace = fl.trace
    spans = holder_spans(trace, "fl.drive", fl.end)
    if not spans:
        print("\n  no fl.drive notes: a trace from before the probe")
        return
    print("\n  drive branch share per driver (fl.drive, to the bot's death):")
    for bot, track in sorted(spans.items(), key=lambda kv: trace.name(kv[0])):
        stop = min(fl.deaths.get(bot, fl.end), fl.end)
        held = collections.Counter()
        for value, start, end in track:
            if start < stop:
                held[value] += min(end, stop) - start
        total = sum(held.values())
        if total <= 0:
            continue
        shares = "  ".join(f"{value} {100 * ms / total:.0f}%" for value, ms in held.most_common())
        print(f"     {trace.name(bot):14s} {shares}")


def show_pursued(fl: Fight) -> None:
    trace = fl.trace
    if not fl.pursued:
        print("\n  no Pursued spans")
        return
    drivers = fl.drivers()
    moves = collections.defaultdict(list)
    for rec in trace.of("move"):
        if rec.get("by") == DRIVE_ACTION:
            moves[rec["g"]].append(rec)
    shocks = collections.defaultdict(list)
    for rec in trace.of("act"):
        if rec.get("a") == VENT_ACTION and rec.get("vd") == "OK":
            shocks[rec["g"]].append(rec["t"])

    print("\n  Pursued spans (hull health, first accepted kite move, first Steam Rush, Electroshocks by")
    print("  its driver, gap from the hull's edge to his at +0/+2/+4 s):")
    close_losses = []
    for hull, start, stop in fl.pursued:
        track = [(f.t, f.hulls[hull], f.boss) for f in fl.frames if start <= f.t <= stop and hull in f.hulls]
        if len(track) < 2 or stop - start < 1500:
            continue
        driver = drivers.get(hull)
        accepted = next((m["t"] for m in moves.get(driver, []) if start <= m["t"] <= stop and m.get("ok") == 1), None)
        rush = None
        for (t0, a, _), (t1, b, _) in zip(track, track[1:]) if track[0][1][3] == SIEGE else ():
            dt = (t1 - t0) / 1000.0
            if 0 < dt <= 0.4 and math.hypot(b[0] - a[0], b[1] - a[1]) / dt > RUSH_SPEED:
                rush = t0
                break
        fired = sum(1 for t in shocks.get(driver, []) if start <= t <= stop)
        gaps = []
        for offset in (0, 2000, 4000):
            frame = fl.frame_after(start + offset)
            ride = frame.hulls.get(hull) if frame and frame.t <= stop else None
            gaps.append(f"{edge_gap(ride, frame.boss):6.1f}" if ride else "     -")
        hp_in, hp_out = track[0][1][2], track[-1][1][2]
        entry = track[0][1][3]
        first_gap = edge_gap(track[0][1], track[0][2])
        if first_gap <= 40:
            close_losses.append(hp_in - hp_out)
        take = lambda when: f"+{(when - start) / 1000:.1f}s" if when is not None else "none"  # noqa: E731
        print(f"     {VEHICLE_NAME[entry]:10s} {hull & 0xffffffff:6d} {clock(start):>9s} {(stop - start) / 1000:5.1f}s"
              f"  hp {hp_in:5.1f}->{hp_out:5.1f}  kite {take(accepted):>7s}  rush {take(rush):>7s}"
              f"  shocks {fired}  gap {' '.join(gaps)}  {trace.name(driver) if driver else '-'}")
    if close_losses:
        close_losses.sort()
        print(f"     started within 40 yd of his edge: {len(close_losses)} spans, hull health lost median "
              f"{statistics.median(close_losses):.0f} pts, range {close_losses[0]:.0f}-{close_losses[-1]:.0f}")


# ---------------------------------------------------------------------------------------------------
# --pyrite
# ---------------------------------------------------------------------------------------------------

def crate_duplicates(casts: list[dict], despawn_ms: int = CRATE_DESPAWN_MS) -> dict[str, int]:
    """A Grab Crate ledger from `cast` rows: every grab after the first on the same crate, split by
    whether the same gunner or another one made it, and whether it fell inside the despawn delay."""
    ledger = {"casts": len(casts), "crates": 0, "same": 0, "cross": 0, "later": 0}
    by_crate: dict[int, list[dict]] = collections.defaultdict(list)
    for rec in sorted(casts, key=lambda rec: rec["t"]):
        by_crate[rec.get("tgt")].append(rec)
    ledger["crates"] = len(by_crate)
    for grabs in by_crate.values():
        first = grabs[0]
        for grab in grabs[1:]:
            if grab["t"] - first["t"] > despawn_ms:
                ledger["later"] += 1
            elif grab.get("s") == first.get("s"):
                ledger["same"] += 1
            else:
                ledger["cross"] += 1
    return ledger


def energy_steps(track: list[tuple[int, float]]) -> tuple[int, int]:
    """`(barrels, credits)` off a demolisher's power track `[(t, energy)]`. Nothing else spends its
    energy and nothing but a crate refills it, so a drop is barrels at 5 each and a rise is a credit,
    counted once per 25 because the cap at 100 can clip one."""
    barrels = credits = 0
    for (_, before), (_, after) in zip(track, track[1:]):
        delta = after - before
        if delta <= -1.0:
            barrels += max(1, round(-delta / BARREL_COST))
        elif delta >= 1.0:
            credits += max(1, math.ceil(delta / CRATE_CREDIT - 1e-6))
    return barrels, credits


def stack_losses(pyrite, barrel, window_ms: int = LOSS_WINDOW_MS) -> list[tuple[int, int, int, str | None]]:
    """`(t, stacks, landed, cause)` for every fall of `fl.pyrite` spans to 0, `landed` being when the
    stack that fell was reached. The cause is the worst `fl.barrel` state held in the window before the
    fall: `dry`, then `fail`, then `not boss`. Anything else means the refresh landed too late or never
    ran (`late`). None when no `fl.barrel` state covers the window."""
    out = []
    previous, reached = 0, 0
    for value, start, _stop in pyrite:
        count = int(value) if value.isdigit() else 0
        if count == 0 and previous:
            seen = {held for held, begin, stop in barrel or () if begin < start and stop > start - window_ms}
            cause = None
            if seen:
                cause = next((state for state in ("dry", "fail", "not boss") if state in seen), "late")
            out.append((start, previous, reached, cause))
        previous, reached = count, start
    return out


def span_shares(spans, start: int, stop: int) -> dict[str, float]:
    """Share of `[start, stop)` each value held, clipped to the window."""
    held: dict[str, float] = collections.Counter()
    for value, begin, end in spans or ():
        overlap = min(end, stop) - max(begin, start)
        if overlap > 0:
            held[value] += overlap
    return {value: amount / (stop - start) for value, amount in held.items()} if stop > start else {}


def show_pyrite(trace: Trace) -> int:
    fl = fight(trace)
    life = fl.hull_life()
    crews = fl.crews()
    print("Blue Pyrite: stacks per demolisher driver, what stopped them, and what the crates were worth\n")

    stacks = holder_spans(trace, "fl.pyrite", fl.end)
    decisions = holder_spans(trace, "fl.barrel", fl.end)
    if not stacks:
        print("  no fl.pyrite notes in this trace")
    else:
        print(f"  {'driver':14s} {'held':>19s} {'mean':>5s} {'at 10':>6s} {'at 0':>5s} {'worst 0':>8s}"
              f" {'d(boss)':>8s} {'>70 yd':>7s}")
    windows: dict[int, tuple[int, int]] = {}
    areas: dict[int, float] = {}
    for bot, track in sorted(stacks.items(), key=lambda kv: trace.name(kv[0])):
        hull = crews.get(bot)
        stop = min(fl.deaths.get(bot, fl.end), life[hull]["gone"] if hull in life else fl.end, fl.end)
        start = track[0][1]
        windows[bot] = (start, stop)
        span = (stop - start) / 1000.0
        if span <= 0:
            continue
        area = at10 = at0 = worst = run = 0.0
        for value, begin, end in track:
            if begin >= stop:
                break
            seconds = (min(end, stop) - begin) / 1000.0
            count = int(value) if value.isdigit() else 0
            area += count * seconds
            at10 += seconds if count == 10 else 0.0
            if count == 0:
                at0 += seconds
                run += seconds
                worst = max(worst, run)
            else:
                run = 0.0
        areas[bot] = area
        ranges = [math.hypot(f.hulls[hull][0] - f.boss[0], f.hulls[hull][1] - f.boss[1])
                  for f in fl.frames if start <= f.t <= stop and hull in f.hulls]
        far = f"{100 * sum(1 for d in ranges if d > BARREL_RANGE) / len(ranges):6.0f}%" if ranges else "      -"
        middle = f"{statistics.median(ranges):8.0f}" if ranges else "       -"
        print(f"  {trace.name(bot):14s} {clock(start):>9s}-{clock(stop):>9s} {area / span:5.1f} {100 * at10 / span:5.0f}%"
              f" {100 * at0 / span:4.0f}% {worst:7.1f}s {middle} {far}")

    if stacks:
        print("\n  barrel decisions (fl.barrel, share of the driver's time) and energy (v13 power column):")
        print(f"  {'driver':14s}" + "".join(f" {state:>8s}" for state in BARREL_STATES)
              + f" {'barrels/min':>12s} {'energy/100 stack-s':>19s} {'credits':>8s}")
        for bot in sorted(windows, key=trace.name):
            start, stop = windows[bot]
            if stop <= start:
                continue
            hull = crews.get(bot)
            shares = span_shares(decisions.get(bot), start, stop)
            cells = "".join(f" {100 * shares[state]:7.0f}%" if state in shares else f" {'-':>8s}"
                            for state in BARREL_STATES)
            power = [(f.t, f.hulls[hull][5]) for f in fl.frames
                     if start <= f.t <= stop and hull in f.hulls and f.hulls[hull][5] is not None]
            if power:
                barrels, credits = energy_steps(power)
                rate = f"{barrels / ((stop - start) / 60000.0):12.1f}"
                spend = (f"{100 * barrels * BARREL_COST / areas[bot]:19.1f}" if areas.get(bot)
                         else f"{'-':>19s}")
                gained = f"{credits:8d}"
            else:
                rate, spend, gained = f"{'pre-v13':>12s}", f"{'-':>19s}", f"{'-':>8s}"
            print(f"  {trace.name(bot):14s}{cells} {rate} {spend} {gained}")

        # Below 10 the last landing is when the barrels stopped, and the hull often drifts out of range
        # only afterwards. A full stack's last refresh does not show, so that one reads the whole window.
        print(f"\n  stacks lost (fl.pyrite fell to 0), blamed on fl.barrel in the {LOSS_WINDOW_MS // 1000} s before;"
              " d(boss) when the barrels stopped:")
        causes: collections.Counter = collections.Counter()
        for bot in sorted(windows, key=trace.name):
            start, stop = windows[bot]
            hull = crews.get(bot)
            for when, count, landed, cause in stack_losses(stacks[bot], decisions.get(bot)):
                if when > stop:
                    break
                since = landed if count < 10 else when - LOSS_WINDOW_MS
                until = landed + 1000 if count < 10 else when
                ranges = [math.hypot(f.hulls[hull][0] - f.boss[0], f.hulls[hull][1] - f.boss[1])
                          for f in fl.frames if since <= f.t <= until and hull in f.hulls]
                distance = statistics.median(ranges) if ranges else None
                if cause is None:
                    cause = ">70 yd, no probe" if distance is not None and distance > BARREL_RANGE else "unknown, no probe"
                causes[cause] += 1
                shown = f"{distance:5.0f} yd" if distance is not None else "     - "
                print(f"     {trace.name(bot):14s} {clock(when):>9s}  from {count:2d}  {cause:18s} d(boss) {shown}")
        if causes:
            print("     by cause: " + ", ".join(f"{cause} {count}" for cause, count in causes.most_common()))
        else:
            print("     none")

    if stacks and fl.frames:
        print("\n  fleet stacks against boss health per 30 s:")
        for window in range(0, fl.end // 30000 + 1):
            begin, end = window * 30000, (window + 1) * 30000
            samples = []
            for when in range(begin, min(end, fl.end), 1000):
                samples.append(sum(int(value_at(track, when, "0")) if value_at(track, when, "0").isdigit() else 0
                                   for bot, track in stacks.items() if when < windows.get(bot, (0, fl.end))[1]))
            health = [f.boss_hp for f in fl.frames if begin <= f.t < end and f.boss_hp is not None]
            # A wipe resets him to full inside the last window, which is not negative damage.
            if samples and len(health) > 1 and health[-1] <= health[0]:
                print(f"     {begin // 1000:4d}s  stacks {sum(samples) / len(samples):5.1f}"
                      f"   boss {2 * (health[0] - health[-1]):5.2f} %/min")

    casts = [rec for rec in trace.of("cast") if rec.get("sp") == GRAB_CRATE and rec.get("s") not in trace.humans]
    print()
    if not casts:
        print("  no bot Grab Crate casts")
        return 0
    ledger = crate_duplicates(casts)
    repeats = ledger["same"] + ledger["cross"]
    credits = sum(1 for rec in trace.of("cast") if rec.get("sp") == CRATE_CREDIT_SPELL and rec.get("s") not in trace.humans)
    print(f"  Grab Crate: {ledger['casts']} bot casts on {ledger['crates']} crates, {credits} credits;"
          f" {repeats} repeats inside the {CRATE_DESPAWN_MS} ms despawn, each credited"
          f" (same gunner {ledger['same']}, another {ledger['cross']}); {ledger['later']} later")

    energy = []
    for rec in casts:
        hull = crews.get(rec.get("s"))
        frame = fl.frame_at(rec["t"])
        ride = frame.hulls.get(hull) if frame else None
        if ride and ride[5] is not None:
            energy.append(ride[5])
    if energy:
        energy.sort()
        over = sum(1 for value in energy if value > CRATE_GRAB_CEILING)
        print(f"  demolisher energy at the grab: median {statistics.median(energy):.0f}%,"
              f" above the {CRATE_GRAB_CEILING} ceiling {over} of {len(energy)}")
    else:
        print("  no power column: a pre-v13 trace, so energy at the grab is unknown")
    return 0


# ---------------------------------------------------------------------------------------------------
# --ram
# ---------------------------------------------------------------------------------------------------

def show_ram(trace: Trace) -> int:
    fl = fight(trace)
    drivers = fl.drivers()
    stations = holder_spans(trace, "fl.station", fl.end)
    real = collections.Counter()
    per_role = collections.defaultdict(collections.Counter)
    lead_to_victim, lead_to_boss = [], []

    for frame in fl.frames:
        victim = frame.hulls.get(fl.pursued_at(frame.t))
        if not victim:
            continue
        bx, by = frame.boss[0], frame.boss[1]
        # He cannot fire from further out than his own cast test allows.
        if math.hypot(victim[0] - bx, victim[1] - by) > BOSS_REACH + RAM_CAST_RANGE + VEHICLE_SIZE[victim[3]]:
            continue
        for guid, (x, y, hp, entry, _mv, _pw) in frame.hulls.items():
            if hp <= 0 or (x, y) == (victim[0], victim[1]):
                continue
            role = value_at(stations.get(drivers.get(guid)), frame.t) or VEHICLE_NAME[entry]
            # FlameLeviathanInBatteringRamBlast: the vehicle's own size counts.
            danger = math.hypot(x - victim[0], y - victim[1]) <= RAM_RADIUS + VEHICLE_SIZE[entry]
            real[danger] += 1
            per_role[role][danger] += 1
            if role == "tar-lead":
                lead_to_victim.append(math.hypot(x - victim[0], y - victim[1]))
                lead_to_boss.append(math.hypot(x - bx, y - by))

    exposed, scored = real[True], real[True] + real[False]
    print("Battering Ram: a 25 yd sphere on the pursued vehicle\n")
    print(f"  vehicle-frames scored (boss in range to fire) : {scored}")
    if scored:
        print(f"  still inside the blast                        : {exposed} ({100 * exposed / scored:.1f}%)")
    print("\n  by station:")
    print(f"     {'station':12s} {'frames':>9s} {'in blast':>10s}")
    for role in sorted(per_role):
        inside, total = per_role[role][True], per_role[role][True] + per_role[role][False]
        print(f"     {role:12s} {total:9d} {inside:7d} ({100 * inside / total:.1f}%)")
    if lead_to_victim:
        lead_to_victim.sort()
        lead_to_boss.sort()
        inside = sum(1 for d in lead_to_victim if d <= RAM_RADIUS)
        print(f"\n  lead chopper ({len(lead_to_victim)} frames):")
        print(f"     to the boss    p25 {pick(lead_to_boss, .25):5.1f}  median {pick(lead_to_boss, .5):5.1f}"
              f"  p75 {pick(lead_to_boss, .75):5.1f}")
        print(f"     to the victim  p25 {pick(lead_to_victim, .25):5.1f}  median {pick(lead_to_victim, .5):5.1f}"
              f"  p75 {pick(lead_to_victim, .75):5.1f}")
        print(f"     inside the blast: {inside}/{len(lead_to_victim)} = {100 * inside / len(lead_to_victim):.1f}%")
    return 0


# ---------------------------------------------------------------------------------------------------
# --fury
# ---------------------------------------------------------------------------------------------------

STILL_STEP_YD = 0.3


def reticle_phases(track: list[tuple[int, float, float]]) -> list[tuple[bool, int, int, float, float]]:
    """`(still, start, stop, x, y)` runs over one reticle's samples. A step under STILL_STEP_YD between
    samples is still; `x, y` is where the run began."""
    phases: list[list] = []
    for (t0, x0, y0), (t1, x1, y1) in zip(track, track[1:]):
        still = math.hypot(x1 - x0, y1 - y0) < STILL_STEP_YD
        if phases and phases[-1][0] == still:
            phases[-1][2] = t1
        else:
            phases.append([still, t0, t1, x0, y0])
    return [tuple(phase) for phase in phases]


def show_fury(trace: Trace) -> int:
    fl = fight(trace)
    tracks = collections.defaultdict(list)
    strikes: dict[int, int] = {}
    for frame in fl.frames:
        for guid, (x, y) in frame.reticles.items():
            tracks[guid].append((frame.t, x, y))
        for guid in frame.strikes:
            strikes.setdefault(guid, frame.t)

    print("Hodir's Fury: chases a target at 12 yd/s, commits once it stops, strikes 6 s later\n")
    if not tracks:
        print("  no reticle in this trace - the Frost tower was down")
        return 0

    phases = {guid: reticle_phases(track) for guid, track in tracks.items()}
    speeds = []
    for track in tracks.values():
        for (t0, x0, y0), (t1, x1, y1) in zip(track, track[1:]):
            dt = (t1 - t0) / 1000.0
            if 0 < dt <= 0.5 and math.hypot(x1 - x0, y1 - y0) / dt > 1.0:
                speeds.append(math.hypot(x1 - x0, y1 - y0) / dt)
    if speeds:
        speeds.sort()
        print(f"  chase speed: median {statistics.median(speeds):.1f} yd/s, p90 {pick(speeds, .9):.1f}")

    # ---- fuse and strike ------------------------------------------------------------------------
    stun_rows = sorted(rec["t"] for rec in trace.of("dmg", "aura")
                       if rec.get("sp") == SPELL_FURY_STUN and not rec.get("r"))
    fuses, flights, caught = [], [], 0
    for when in sorted(strikes.values()):
        stop = None
        for guid, runs in phases.items():
            for still, start, end, _x, _y in runs:
                if still and start <= when <= end + 500:
                    stop = start if stop is None or start > stop else stop
        if stop is not None:
            fuses.append((when - stop) / 1000.0)
        landed = next((t for t in stun_rows if 0 <= t - when <= FURY_STRIKE_WINDOW_MS), None)
        if landed is not None:
            flights.append(landed - when)
        frame = fl.frame_at(when + (landed - when if landed is not None else 1100), 400)
        spot = None
        for guid, track in tracks.items():
            near = [p for p in track if abs(p[0] - when) <= 500]
            if near:
                spot = (near[0][1], near[0][2])
        if frame and spot:
            caught += sum(1 for h in frame.hulls.values()
                          if h[2] > 0 and math.hypot(h[0] - spot[0], h[1] - spot[1]) <= FURY_RADIUS)
    print(f"  strikes: {len(strikes)}")
    if fuses:
        fuses.sort()
        print(f"     stop to strike NPC : median {statistics.median(fuses):.1f} s, {fuses[0]:.1f}-{fuses[-1]:.1f}")
    if flights:
        flights.sort()
        print(f"     strike NPC to 62297: median {statistics.median(flights):.0f} ms, {flights[0]}-{flights[-1]}")
    print(f"     hulls within {FURY_RADIUS:.0f} yd when it landed: {caught}")

    # ---- the dodge scan -------------------------------------------------------------------------
    def state(guid: int, when: int) -> str:
        for still, start, end, _x, _y in phases[guid]:
            if start <= when <= end:
                if not still:
                    return "chasing"
                return "armed" if when - start <= FURY_ARMED_MS else "spent"
        return "chasing"

    scan = FURY_RADIUS + HAZARD_MARGIN
    frames_by_class = collections.Counter()
    inside = collections.Counter()
    for frame in fl.frames:
        for guid, (x, y, hp, entry, _mv, _pw) in frame.hulls.items():
            if hp <= 0:
                continue
            frames_by_class[entry] += 1
            for reticle, (rx, ry) in frame.reticles.items():
                if math.hypot(x - rx, y - ry) < scan:
                    inside[entry, state(reticle, frame.t)] += 1
                    break
    print(f"\n  hull-frames with a reticle inside the {scan:.0f} yd scan, by what the reticle was doing:")
    print(f"     {'class':10s} {'chasing':>8s} {'armed':>8s} {'spent':>8s}   (share of that class's frames)")
    for entry, name in VEHICLE_NAME.items():
        total = frames_by_class[entry]
        if total:
            cells = "".join(f"{100 * inside[entry, kind] / total:7.1f}%" for kind in ("chasing", "armed", "spent"))
            print(f"     {name:10s} {cells}")
    armed = sum(inside[key] for key in inside if key[1] == "armed")
    scanned = sum(inside.values())
    if scanned:
        print(f"     only 'armed' can hurt: {100 * (scanned - armed) / scanned:.0f}% of scan time was against a"
              f" reticle that could not")

    marks = holder_spans(trace, "fl.fury", fl.end)
    if marks:
        armings = [(end - start) for track in marks.values() for value, start, end in track if value == "1"]
        if armings:
            print(f"\n  fl.fury: {len(armings)} armings, median {statistics.median(armings) / 1000:.1f} s armed")
    return 0


# ---------------------------------------------------------------------------------------------------
# --inferno
# ---------------------------------------------------------------------------------------------------

def show_inferno(trace: Trace) -> int:
    fl = fight(trace)
    bands = [(-1e9, 0.0, "inside its reach"), (0.0, 8.0, "0-8 yd past it"),
             (8.0, 25.0, "8-25 yd"), (25.0, 60.0, "25-60 yd"), (60.0, 1e9, "over 60 yd")]
    lost = collections.defaultdict(float)
    secs = collections.defaultdict(float)
    by_zone = collections.defaultdict(float)
    inside = collections.Counter()
    concurrent = []
    total_lost = 0.0

    def gap(hull, fires):
        return min((math.hypot(hull[0] - fx, hull[1] - fy) - inferno_reach(r, hull[3]) for fx, fy, r in fires),
                   default=math.inf)

    for prev, frame in zip([None] + fl.frames, fl.frames):
        concurrent.append(len(frame.fires))
        for hull in frame.hulls.values():
            if hull[2] > 0:
                inside[VEHICLE_NAME[hull[3]], gap(hull, frame.fires) < 0] += 1
        if prev is None or not 0 < frame.t - prev.t <= 3000:
            continue
        dt = (frame.t - prev.t) / 1000.0
        for guid, now in frame.hulls.items():
            was = prev.hulls.get(guid)
            if not was or was[2] <= 0 or now[2] <= 0:
                continue
            drop = max(0.0, was[2] - now[2])
            distance = gap(was, prev.fires)
            for low, high, label in bands:
                if low <= distance < high:
                    lost[label] += drop
                    secs[label] += dt
                    break
            total_lost += drop
            by_zone["inside" if distance < 0 else ("near" if distance < 8 else "clear")] += drop

    print("Mimiron's Inferno: a walking head drops a 9 yd patch every 2 s, each burning 30 s\n")
    if not concurrent or not max(concurrent):
        print("  no Inferno patches in this trace - the Flame tower was down, or it is a pre-v8 file")
        return 0
    concurrent.sort()
    print(f"  patches on the ground at once: median {pick(concurrent, .5)}  max {concurrent[-1]}")
    print("  reach is the patch radius plus both object sizes: chopper 10.4 yd, demolisher 11.6, siege 17.1")

    print("\n  hull frames inside a patch's reach:")
    for name in VEHICLE_NAME.values():
        hot, cold = inside[name, True], inside[name, False]
        if hot + cold:
            print(f"     {name:12s} {hot:6d} / {hot + cold:6d}  ({100 * hot / (hot + cold):.1f}%)")

    print("\n  hull health lost per 5 s, by distance past the nearest patch's reach:")
    for _, _, label in bands:
        if secs[label] > 1:
            print(f"     {label:18s} {5 * lost[label] / secs[label]:6.2f}   ({secs[label]:.0f} hull-seconds)")
    if total_lost:
        print("\n  share of all hull health lost:")
        for key, label in (("inside", "inside the reach"), ("near", "within 8 yd of it"), ("clear", "clear of it")):
            print(f"     {label:18s} {100 * by_zone[key] / total_lost:5.1f}%")
    return 0


# ---------------------------------------------------------------------------------------------------
# --adds
# ---------------------------------------------------------------------------------------------------

def add_tracks(fl: Fight) -> dict[int, list[tuple]]:
    """add guid -> [(t, x, y, hp, target)]"""
    tracks: dict[int, list[tuple]] = collections.defaultdict(list)
    for frame in fl.frames:
        for guid, (x, y, hp, target) in frame.adds.items():
            tracks[guid].append((frame.t, x, y, hp, target))
    return tracks


def add_fates(tracks: dict[int, list[tuple]], entries: dict[int, int], end: int) -> dict[str, list[int]]:
    """Which adds died, which were still up at the end, and which vanished at full health - split into
    the ones gone after exactly their summon duration (`timeout`) and the rest (`vanished`)."""
    fates: dict[str, list[int]] = {"died": [], "alive": [], "timeout": [], "vanished": []}
    for guid, track in tracks.items():
        first, last = track[0], track[-1]
        if last[0] >= end - 300:
            fates["alive"].append(guid)
        elif last[3] < 90:
            fates["died"].append(guid)
        elif abs(last[0] - first[0] - ADD_SUMMON_MS.get(entries.get(guid), -10 ** 9)) <= ADD_TIMEOUT_SLACK_MS:
            fates["timeout"].append(guid)
        else:
            fates["vanished"].append(guid)
    return fates


def timed_out(fl: Fight) -> str | None:
    """The DB diagnosis, when most adds left at full health after exactly their summon duration."""
    tracks = add_tracks(fl)
    if not tracks:
        return None
    fates = add_fates(tracks, fl.trace.entries, fl.end)
    if len(fates["timeout"]) * 2 < len(tracks):
        return None
    return (f"{len(fates['timeout'])} of {len(tracks)} adds vanished at full health after their summon duration"
            " (3 s Ward of Life, 10 s Lasher).\n  The world DB lacks 2026_09_10_03.sql, so npc_freya_ward_summon is"
            " not bound and the adds run SmartAI.\n  Apply the pending world updates; add handling in this pull"
            " cannot be scored.")


def show_adds(trace: Trace) -> int:
    fl = fight(trace)
    tracks = add_tracks(fl)
    print("Freya's Ward: four wards at the corners, a wave every 29 s\n")
    if not tracks:
        print("  no adds in this trace - the Tower of Life was down.")
        return 0

    fates = add_fates(tracks, trace.entries, fl.end)
    spans = sorted((track[-1][0] - track[0][0]) / 1000.0 for track in tracks.values())
    print(f"  adds seen {len(tracks)}: died {len(fates['died'])}, alive at the end {len(fates['alive'])},"
          f" gone at full health {len(fates['timeout']) + len(fates['vanished'])}"
          f" ({len(fates['timeout'])} exactly at their summon duration)")
    print(f"  lifespan: median {statistics.median(spans):.1f} s, p25 {pick(spans, .25):.1f}, p75 {pick(spans, .75):.1f}")
    diagnosis = timed_out(fl)
    if diagnosis:
        print(f"\n  {diagnosis}")
        return 0

    # ---- where they spawn, and whether they stay there -------------------------------------------
    corners = collections.Counter()
    wander = []
    for track in tracks.values():
        x0, y0 = track[0][1], track[0][2]
        corners[geometry.nearest((x0, y0), enumerate(ARENA_CORNERS))[0]] += 1
        wander.append(max(math.hypot(p[1] - x0, p[2] - y0) for p in track))
    wander.sort()
    print(f"\n  spawn corner: " + "  ".join(f"{i}:{corners[i]}" for i in range(len(ARENA_CORNERS))))
    print(f"  travelled from spawn: median {pick(wander, .5):.0f} yd  p90 {pick(wander, .9):.0f}  max {wander[-1]:.0f}")
    for band in (10, 30, 80):
        near = sum(1 for w in wander if w <= band)
        print(f"     never left {band:2d} yd of it: {near}/{len(wander)} ({100 * near / len(wander):.0f}%)")

    # ---- population, and how fast one dies -------------------------------------------------------
    buckets = collections.defaultdict(int)
    for frame in fl.frames:
        alive = sum(1 for add in frame.adds.values() if add[2] > 0)
        buckets[frame.t // 20000] = max(buckets[frame.t // 20000], alive)
    print("\n  most alive at once, per 20 s:")
    cells = [f"{k * 20:3d}s:{v}" for k, v in sorted(buckets.items())]
    for start in range(0, len(cells), 10):
        print("     " + "  ".join(cells[start:start + 10]))
    rates = sorted(r for r in ((t[0][3] - t[-1][3]) / ((t[-1][0] - t[0][0]) / 1000.0)
                               for t in tracks.values() if len(t) >= 8 and t[-1][0] - t[0][0] > 5000) if r > 0)
    if rates:
        print(f"  health lost per second: median {pick(rates, .5):.2f}%  ->  median time to kill one add"
              f" {100 / pick(rates, .5):.0f} s")

    # ---- what could have shot them, without anyone moving ----------------------------------------
    in_band = collections.Counter()
    reachable = add_frames = 0
    clump = []
    for frame in fl.frames:
        live = [(a[0], a[1]) for a in frame.adds.values() if a[2] > 0]
        for ax, ay in live:
            add_frames += 1
            clump.append(sum(1 for bx, by in live if math.hypot(ax - bx, ay - by) <= SPLASH_YD))
            covered = False
            for entry, low, high, label in WEAPON_BANDS:
                near = [math.hypot(ax - h[0], ay - h[1]) for h in frame.hulls.values() if h[3] == entry and h[2] > 0]
                if near and low <= min(near) <= high:
                    in_band[label] += 1
                    covered = True
            reachable += covered
    if add_frames:
        print(f"\n  add-frames in a weapon band with nobody moving ({add_frames} scored):")
        for _entry, _low, _high, label in WEAPON_BANDS:
            print(f"     {label:28s} {in_band[label]:6d}  {100 * in_band[label] / add_frames:5.1f}%")
        print(f"     {'>> reachable by something':28s} {reachable:6d}  {100 * reachable / add_frames:5.1f}%")
        clump.sort()
        print(f"  adds inside one {SPLASH_YD:.0f} yd splash: median {pick(clump, .5)}  p75 {pick(clump, .75)}"
              f"  max {clump[-1]}")

    # ---- and what share of the raid's damage taken is theirs? ------------------------------------
    # Players only: from v13 the hulls have dmg rows too, and they would swamp the share.
    taken = collections.Counter()
    for rec in trace.of("dmg"):
        if rec.get("d") in fl.roster:
            taken[rec.get("sp")] += rec.get("a", 0)
    total = sum(taken.values())
    if total:
        print(f"\n  Lash {LASH_SPELL} share of raid damage taken: "
              f"{100 * taken[LASH_SPELL] / total:.1f}% ({taken[LASH_SPELL]:,} of {total:,})")
    return 0


# ---------------------------------------------------------------------------------------------------
# --corners
# ---------------------------------------------------------------------------------------------------

def ward_waves(tracks: dict[int, list[tuple]]) -> list[tuple[int, int, list[int]]]:
    """`(corner, first seen, adds)` per wave: adds first seen within SPAWN_YD of a corner, grouped while
    they appear inside WAVE_WINDOW_MS of the wave's first."""
    spawned = []
    for guid, track in tracks.items():
        t0, x0, y0 = track[0][:3]
        near = geometry.nearest((x0, y0), enumerate(ARENA_CORNERS))
        if near and near[1] <= SPAWN_YD:
            spawned.append((t0, near[0], guid))
    waves: list[list] = []
    for t0, corner, guid in sorted(spawned):
        wave = next((w for w in waves if w[0] == corner and t0 - w[1] <= WAVE_WINDOW_MS), None)
        if wave:
            wave[2].append(guid)
        else:
            waves.append([corner, t0, [guid]])
    return [tuple(wave) for wave in waves]


def show_corners(trace: Trace) -> int:
    fl = fight(trace)
    tracks = add_tracks(fl)
    print("Corner containment: each ward wave against the siege engine posted at that corner\n")
    posts = [post_point(i) for i in range(len(ARENA_CORNERS))]

    # ---- arrival at each post after engage -------------------------------------------------------
    arrived: dict[int, tuple[int, int]] = {}
    for frame in fl.frames:
        for guid, (x, y, hp, entry, _mv, _pw) in frame.hulls.items():
            if entry != SIEGE or hp <= 0:
                continue
            for index, (px, py) in enumerate(posts):
                if index not in arrived and math.hypot(x - px, y - py) <= POSTED_YD:
                    arrived[index] = (frame.t, guid)
    print("  first siege engine within 15 yd of each post:")
    for index in range(len(posts)):
        when = arrived.get(index)
        print(f"     post {index}: " + (f"{clock(when[0])} ({when[1] & 0xffffffff})" if when else "never"))

    if not tracks:
        print("\n  no adds in this trace - the Tower of Life was down.")
        return 0
    diagnosis = timed_out(fl)
    if diagnosis:
        print(f"\n  {diagnosis}")

    drives = holder_spans(trace, "fl.drive", fl.end)
    drivers = fl.drivers()
    print(f"\n  {'wave':>9s} {'post':>4s} {'adds':>4s} {'engine':>7s} {'driving':>8s} {'held':>5s} {'thrown':>6s}"
          f" {'travel':>7s} {'left':>5s} {'kill':>6s} {'hull lost':>9s}")
    manned = first_on_post = total_adds = left_manned = 0
    for corner, start, adds in ward_waves(tracks):
        px, py = posts[corner]
        frame = fl.frame_at(start)
        engine = None
        if frame:
            near = geometry.nearest((px, py), {g: (h[0], h[1]) for g, h in frame.hulls.items()
                                               if h[3] == SIEGE and h[2] > 0})
            engine = near[0] if near and near[1] <= POSTED_YD else None
        driving = value_at(drives.get(drivers.get(engine)), start, "-") if engine else "-"
        held = thrown = left = 0
        travel, kills = 0.0, []
        for guid in adds:
            track = tracks[guid]
            victim = next((p[4] for p in track if p[4]), 0)
            held += engine is not None and victim == engine
            for a, b in zip(track, track[1:]):
                if b[0] - a[0] <= 1000 and math.hypot(b[1] - a[1], b[2] - a[2]) > KNOCKBACK_YD:
                    thrown += 1
            travel = max(travel, max(math.hypot(p[1] - track[0][1], p[2] - track[0][2]) for p in track))
            left += any(math.hypot(p[1] - px, p[2] - py) > CORNER_HOLD_RADIUS for p in track)
            if track[-1][3] < 90 and track[-1][0] < fl.end - 300:
                kills.append((track[-1][0] - track[0][0]) / 1000.0)
        lost = "-"
        if engine is not None:
            later = fl.frame_at(min(start + 29000, fl.end))
            before, after = frame.hulls[engine][2], later.hulls.get(engine, (0, 0, 0.0))[2] if later else 0.0
            lost = f"{before - after:.0f}"
        total_adds += len(adds)
        if engine is not None:
            manned += 1
            first_on_post += held
            left_manned += left
        kill = f"{statistics.median(kills):.0f}s" if kills else "-"
        print(f"  {clock(start):>9s} {corner:4d} {len(adds):4d} {engine & 0xffffffff if engine else '-':>7}"
              f" {driving:>8s} {held:5d} {thrown:6d} {travel:6.0f}y {left:5d} {kill:>6s} {lost:>9s}")

    waves = len(ward_waves(tracks))
    print(f"\n  waves with an engine on the post: {manned} of {waves}")
    manned_adds = sum(len(adds) for corner, start, adds in ward_waves(tracks)
                      if fl.frame_at(start) and any(h[3] == SIEGE and h[2] > 0 and
                                                    math.hypot(h[0] - posts[corner][0], h[1] - posts[corner][1]) <= POSTED_YD
                                                    for h in fl.frame_at(start).hulls.values()))
    if manned_adds:
        print(f"  adds at a manned post whose first victim was that engine: {first_on_post} of {manned_adds}")
        print(f"  adds at a manned post that left {CORNER_HOLD_RADIUS:.0f} yd of it: {left_manned} of {manned_adds}")
    return 0


# ---------------------------------------------------------------------------------------------------
# --vents
# ---------------------------------------------------------------------------------------------------

def show_vents(trace: Trace) -> int:
    fl = fight(trace)
    channels = vent_channels(trace)
    print("Flame Vents: a 10 s channel every 20 s, 11 damage ticks if nothing stops it\n")
    if not channels:
        print("  no 63847 ticks in this trace")
        return 0

    ends = trace.of("end")
    pull_end = ends[-1]["t"] if ends else channels[-1][1] + VENT_GAP_MS + 1
    # A channel the pull ended under is short for a reason that is not an interrupt.
    scored = [c for c in channels if pull_end - c[0] >= VENT_TICKS_FULL * 1000]
    short = [c for c in scored if c[2] < VENT_TICKS_FULL]
    held = sum(c[1] - c[0] for c in channels) / 1000.0
    print(f"  channels seen    : {len(channels)}, {held:.0f} s of channel out of {pull_end / 1000.0:.0f} s")
    print(f"  ticks per channel: {[c[2] for c in channels]}")
    print(f"  cut short        : {len(short)} of {len(scored)} that had room to finish")

    shots = [rec for rec in trace.of("note") if rec.get("k") == "fl.vent"]
    if not shots:
        print("  Electroshock     : no fl.vent notes - nothing fired, or the trace predates the probe")
    else:
        hit = sum(1 for rec in shots if rec.get("txt") == "hit")
        print(f"  Electroshock     : {len(shots)} cast, {hit} stopped the channel")
        unclaimed = []
        for start, stop, ticks in scored:
            if ticks < VENT_TICKS_FULL or any(start - 1500 <= rec["t"] <= stop + 1000 for rec in shots):
                continue
            frame = fl.frame_at(start)
            sieges = [math.hypot(h[0] - frame.boss[0], h[1] - frame.boss[1])
                      for h in frame.hulls.values() if h[3] == SIEGE and h[2] > 0] if frame else []
            if sieges:
                unclaimed.append((start, ticks, len(sieges), min(sieges)))
        print(f"  ran full with nobody firing and a siege engine alive: {len(unclaimed)}")
        for start, ticks, alive, nearest in unclaimed:
            print(f"     {clock(start):>9s}  {ticks:2d} ticks  {alive} alive, nearest {nearest:.1f} yd from his centre")

    # Same window shape as the add attrition, so the two numbers can be read side by side.
    windows = collections.defaultdict(lambda: [None, None, 0, 0])
    for frame in fl.frames:
        under = any(a - 500 <= frame.t <= b + 1500 for a, b, _ in channels)
        for guid, hull in frame.hulls.items():
            cell = windows[(guid, frame.t // ATTRITION_WINDOW_MS)]
            if cell[0] is None:
                cell[0] = hull[2]
            cell[1] = hull[2]
            cell[2] += 1 if under else 0
            cell[3] += 1
    inside, outside = [], []
    for first, last, under, samples in windows.values():
        if samples >= 5:
            (inside if under * 2 >= samples else outside).append(first - last)
    if inside and outside:
        print(f"\n  hull health lost per {ATTRITION_WINDOW_MS // 1000} s:")
        print(f"     while it is channelling: {sum(inside) / len(inside):5.2f}%   ({len(inside)} windows)")
        print(f"     otherwise              : {sum(outside) / len(outside):5.2f}%   ({len(outside)} windows)")
    return 0


SECTIONS = (
    ("hulls", "why the vehicles died, crews on foot, drive branches, Pursued escapes", show_hulls),
    ("pyrite", "Blue Pyrite stacks per demolisher, and the crate ledger", show_pyrite),
    ("ram", "Battering Ram exposure only", show_ram),
    ("fury", "Hodir's Fury only", show_fury),
    ("inferno", "Mimiron's Inferno trail only", show_inferno),
    ("adds", "Freya's Ward adds only", show_adds),
    ("corners", "corner containment per ward wave", show_corners),
    ("vents", "Flame Vents channels and interrupts only", show_vents),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS)


if __name__ == "__main__":
    sys.exit(main())
