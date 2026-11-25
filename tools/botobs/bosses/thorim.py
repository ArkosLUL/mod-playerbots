#!/usr/bin/env python3
"""Read a Thorim pull from a RaidObs trace: deaths, and the phase 2 measures the tank spots are judged on.

    thorim.py <file>           every section
    thorim.py <file> --deaths  deaths by mechanic, phase 1 and phase 2 apart
    thorim.py <file> --phase2  Chain Lightning, tank deaths, his cannot-reach flag and stretches held without
                               losing health, how long he ran loose before a tank first held him, the
                               opening trade and how far it dragged him, Thorim's
                               spot and facing, tank trades, the waiting tank, chain bridges, melee
                               ring and camp spacing, hazards on each tank
    thorim.py <file> --blizzard  each Sif Blizzard against the melee ring: slot points it covered, who
                                 stood in it and how long, ticks taken, step-outs (how many landed on
                                 the bunny's path ahead or under a zone soon after) and the walk back
    thorim.py <file> --chains    each Chain Lightning hop by hop: which groups (tank, camp, a ring slot, a
                                 slot stepped out of Blizzard or walking back from it) it crossed between
                                 and how far, who died after which crossing
    thorim.py <file> --shelters  each Lightning Charge against the camp: who walked to a shelter row or a
                                 cone step, how far, who got there, who the cone hit (walking or not) and
                                 which of Sif's slows were on them
    thorim.py <file> --ringcone  each Lightning Charge against the melee ring: who it hit and what they were
                                 doing (unable to move, walking, held out of it, standing, past melee reach),
                                 and how many ring walks were held for crossing the lit cone
    thorim.py <file> --colossus  each Runic Colossus Smash against the gauntlet healers: how far off his facing
                                 and his target each stood, the move that put it there, who it hit or killed
    thorim.py <file> --chainwait each orb 1 or 2 against the camp: shelter walks held until Chain Lightning
                                 landed, released by the landing or the cap, the chain's camp hops, and what the
                                 chain and the cone did to the waiters
"""
from __future__ import annotations

import collections
import math
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.geometry import NEAREST, anchor, at, dist2, frames, radius  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, notes  # noqa: E402

ENCOUNTER = "thorim"

NPC_THORIM = 32865
NPC_SIF = 33196
CHAIN_LIGHTNING = (62131, 64390)
LIGHTNING_CHARGE = 62466
UNBALANCING_STRIKE = 62130
BLIZZARD = (62576, 62602)
FROST_NOVA = (62597, 62605)
SPELL_NAMES = {62131: "Chain Lightning", 64390: "Chain Lightning", 62466: "Lightning Charge",
               62130: "Unbalancing Strike", 62576: "Blizzard", 62602: "Blizzard", 62597: "Frost Nova",
               62605: "Frost Nova", 62580: "Frostbolt Volley", 62604: "Frostbolt Volley",
               62583: "Frostbolt", 62601: "Frostbolt"}

FLOOR_Z = 429.6
TANK_SPOT = anchor("ULDUAR_THORIM_PHASE2_TANK_SPOT")
CHAIN_JUMP = 8.0
MELEE_EDGE = 9.0
RING_REACH = 12.0
SETTLED_MS = 15000
UPTIME_END_MS = 100000
CAST_GAP_MS = 400
TRADE_LOOK_MS = 3000
SWAP_WAIT_MS = 15000
OPENING_MS = 30000
DRAG_LOOK_MS = 8000
FACING_OFF_DEG = 20.0
STACKED_YD = 2.0
STALL_MS = 3000
STALL_GAP_MS = 1000
STALL_HP = 0.03
RING_MOVER = "thorim phase 2 positioning action"
UNREACH_MOVER = "thorim unreachable step action"
BLIZZARD_MOVER = "thorim sif blizzard action"
RINGSPOT = re.compile(r"slot (\d+)")
RINGSPOT_SOLVED = re.compile(r"slot (\d+) bearing (-?\d+) solved (-?\d+)")
NPC_SIF_BLIZZARD = 32879
BLIZZARD_REACH = 9.8
BLIZZARD_ZONE_MS = 10000
SIF_BLIZZARD_PATH = ((2108.7, -280.04), (2104.6, -268.5), (2104.3, -256.3), (2109.3, -246.4), (2117.9, -238.6),
                     (2128.8, -232.1), (2151.9, -237.5), (2164.9, -256.3), (2161.5, -280.0))
STEP_OVERRUN_MS = 8000
BUNNY_FRESH_MS = 1000
RING_RADIUS = radius("ULDUAR_THORIM_MELEE_RING_RADIUS")
RING_BLIZZARD_CLEARANCE = radius("ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE")
LUST = (2825, 32182)
LUST_MS = 40000
CHAIN_DEATH_MS = 1500
CHAIN_POSITION_MS = 1000
CHAIN_STEP_BACK_MS = 3000
SLOWS = {62580: "Volley", 62604: "Volley", 62583: "Frostbolt", 62601: "Frostbolt", 62576: "Blizzard",
         62602: "Blizzard", 62597: "Frost Nova", 62605: "Frost Nova"}
LIT_GAP_MS = 3000
CONE_LEAD_MS = (3000, 7000)
SHELTER_LEAD_MS = 200
SHELTER_ARRIVED = 2.0
SLOW_LOOK_MS = 4000
RING_CONE_HOLD = "thorim.ringconehold"
RING_HIT_KINDS = ("blocked", "walking", "held", "standing")
NPC_RUNIC_COLOSSUS = 32872
COLOSSUS_SMASH = 62339
SMASH_LAND_MS = 3500
SMASH_LOOK = 20.0
SMASH_ON_TARGET = 6.0
NPC_THUNDER_ORB = 33378
THUNDER_ORB_SPOTS = ((2145.50, -222.62), (2164.20, -233.47), (2164.55, -293.00), (2105.04, -292.56), (2092.95, -263.00),
                     (2104.94, -233.44), (2124.30, -222.60))
ORB_MATCH = 4.0
CHAIN_WAIT_ORBS = (1, 2)
SHELTER_ORB = re.compile(r"slot \d+ orb (\d+)( wait| step)?$")
WAIT_LOOK_MS = 4800
CHAIN_LOOK_MS = (-1000, 3000)
CAMP_CHAIN_BRIDGE = 4
RELEASE_SLACK_MS = 500


def mine(trace: Trace) -> bool:
    return encounter_of(trace) == ENCOUNTER


def thorim_guid(trace: Trace):
    for guid, entry in trace.entries.items():
        if entry == NPC_THORIM:
            return guid
    return next((guid for guid, name in trace.names.items() if name == "Thorim"), None)


def phase2_start(trace: Trace) -> int | None:
    for note in notes(trace, "thorim.p2start"):
        if str(note.get("txt")) not in ("", "0"):
            return note["t"]
    return None


def hard_mode(trace: Trace) -> bool:
    return any(str(note.get("txt")) == "1" for note in notes(trace, "thorim.sifjoined"))


def tanks_of(trace: Trace) -> set:
    return {guid for guid, role in trace.roles.items() if role == "tank"}


def blow_spell(death: dict) -> int | None:
    blow = death.get("blow") or []
    if len(blow) < 2:
        return None
    rows = [row for row in death.get("rewind") or [] if len(row) >= 4 and row[1] == blow[0] and row[3] == blow[1]]
    return rows[-1][2] if rows else None


def mechanic(trace: Trace, death: dict) -> str:
    spell = blow_spell(death)
    killer = (death.get("blow") or [death.get("killer")])[0]
    if spell in SPELL_NAMES:
        return SPELL_NAMES[spell]
    if trace.entries.get(killer) == NPC_THORIM:
        return "Thorim melee on a tank" if trace.role(death["g"]) == "tank" else "Thorim melee on a non-tank"
    name = trace.name(killer)
    if spell:
        return f"{name} {trace.spells.get(spell) or f'spell {spell}'}"
    return f"{name} melee"


def show_banner(trace: Trace) -> None:
    print(f"Thorim  {trace.path.name}")
    if not mine(trace):
        print("  not a Thorim pull, every section below reads empty")
        return
    ends = trace.of("end")
    out = ends[-1].get("out", "?") if ends else "?"
    when = f" at {clock(ends[-1]['t'])}" if ends else ""
    start = phase2_start(trace)
    phase = f"phase 2 at {clock(start)}, {'hard' if hard_mode(trace) else 'normal'} mode" if start is not None \
        else "phase 2 never started"
    print(f"  outcome {out}{when}, {len(combat_deaths(trace))} death(s), {phase}")


def show_deaths(trace: Trace) -> None:
    print("DEATHS BY MECHANIC")
    if not mine(trace):
        return
    start = phase2_start(trace)
    counts = collections.Counter()
    for death in combat_deaths(trace):
        phase = "phase 2" if start is not None and death["t"] >= start else "phase 1"
        counts[(phase, mechanic(trace, death))] += 1
    if not counts:
        print("  none")
    for (phase, name), n in sorted(counts.items(), key=lambda item: (item[0][0], -item[1], item[0][1])):
        print(f"  {phase}  {n:3d}  {name}")


def angle_off(first: float, second: float) -> float:
    return abs(math.degrees((first - second + math.pi) % (2 * math.pi) - math.pi))


def bearing(source, target) -> float:
    return math.atan2(target[1] - source[1], target[0] - source[0])


def quantile(values, fraction: float):
    ordered = sorted(values)
    return ordered[int(fraction * (len(ordered) - 1))] if ordered else None


def yd(value) -> str:
    return "-" if value is None else f"{value:.1f}"


def share(hits: int, total: int) -> str:
    return f"{100.0 * hits / total:.1f}%" if total else "-"


def segment_gap(point, start, end) -> float:
    dx, dy = end[0] - start[0], end[1] - start[1]
    length = dx * dx + dy * dy
    along = 0.0 if not length else max(0.0, min(1.0, ((point[0] - start[0]) * dx + (point[1] - start[1]) * dy)
                                                / length))
    return dist2(point, (start[0] + dx * along, start[1] + dy * along))


def path_ahead_gap(point, bunny) -> float:
    legs = list(zip(SIF_BLIZZARD_PATH, SIF_BLIZZARD_PATH[1:]))
    leg = min(range(len(legs)), key=lambda index: segment_gap(bunny, *legs[index]))
    ahead = [(bunny, legs[leg][1])] + legs[leg + 1:]
    return min(segment_gap(point, start, end) for start, end in ahead)


def chain_casts(trace: Trace, start: int, players: set) -> list[list[dict]]:
    rows = [rec for rec in trace.of("dmg")
            if rec.get("sp") in CHAIN_LIGHTNING and rec["t"] >= start and rec.get("d") in players]
    casts: list[list[dict]] = []
    for rec in rows:
        if casts and rec["t"] - casts[-1][-1]["t"] <= CAST_GAP_MS:
            casts[-1].append(rec)
        else:
            casts.append([rec])
    return casts


class Phase2:
    def __init__(self, trace: Trace, start: int):
        self.trace = trace
        self.start = start
        self.boss = thorim_guid(trace)
        self.tanks = tanks_of(trace)
        self.players = set(trace.roles)
        self.pets = set(trace.owners)
        self.slot_events = sorted(
            (note["t"], note.get("g"), int(found.group(1)))
            for note in notes(trace, "thorim.ringspot")
            if (found := RINGSPOT.match(str(note.get("txt")))))
        self.victims: list[tuple[int, int, tuple]] = []
        self.end = start
        self._walk()

    def slots_at(self, when: int) -> dict:
        slots: dict = {}
        for t, guid, slot in self.slot_events:
            if t > when:
                break
            slots[guid] = slot
        return slots

    def _walk(self) -> None:
        counters = collections.Counter()
        lists = collections.defaultdict(list)
        self.waiting_offsets: list[tuple[float, float]] = []
        event = 0
        slots: dict = {}
        previous_t = None
        boss_hp = []
        for snap in frames(self.trace):
            when = snap["t"]
            if when < self.start:
                continue
            while event < len(self.slot_events) and self.slot_events[event][0] <= when:
                slots[self.slot_events[event][1]] = self.slot_events[event][2]
                event += 1
            rows = {row[0]: row for row in snap.get("u", [])}
            boss = rows.get(self.boss)
            if not boss or boss[5] <= 0 or boss[3] > FLOOR_Z:
                previous_t = when
                continue
            self.end = when
            boss_hp.append((when, boss[5]))
            victim = boss[7] if len(boss) > 7 else 0
            self.victims.append((when, victim, (boss[1], boss[2])))
            floor = {guid: row for guid, row in rows.items()
                     if guid != self.boss and row[5] > 0 and row[3] <= FLOOR_Z}
            tanks_up = [guid for guid in floor if guid in self.tanks]
            if previous_t is not None and tanks_up and victim not in self.tanks:
                counters["loose_ms"] += when - previous_t
            previous_t = when
            if when < self.start + SETTLED_MS or victim not in self.tanks:
                continue
            spot = (boss[1], boss[2])
            counters["held"] += 1
            lists["boss_off_spot"].append(dist2(spot, TANK_SPOT))
            facing = angle_off(boss[4], bearing(spot, TANK_SPOT))
            lists["facing"].append(facing)
            counters["facing_off"] += facing > FACING_OFF_DEG

            ring = {guid: row for guid, row in floor.items()
                    if guid in slots and guid in self.players and dist2(row[1:3], spot) <= RING_REACH}
            camp = {guid: row for guid, row in floor.items()
                    if guid in self.players and guid not in slots and guid not in self.tanks}
            bodies = {guid: row for guid, row in floor.items()
                      if guid not in self.tanks and (guid in self.players or guid in self.pets)}
            holder = floor.get(victim)
            if holder and any(dist2(row[1:3], holder[1:3]) <= CHAIN_JUMP for row in ring.values()):
                counters["holder_ring"] += 1
            for guid in tanks_up:
                if guid == victim:
                    continue
                tank = floor[guid]
                counters["waiting"] += 1
                self.waiting_offsets.append((tank[1] - TANK_SPOT[0], tank[2] - TANK_SPOT[1]))
                counters["waiting_stacked"] += bool(holder) and dist2(tank[1:3], holder[1:3]) <= STACKED_YD
                gap = dist2(tank[1:3], spot)
                lists["waiting_boss"].append(gap)
                counters["waiting_out"] += gap > MELEE_EDGE
                near = [g for g, row in bodies.items() if dist2(row[1:3], tank[1:3]) <= CHAIN_JUMP]
                counters["waiting_body"] += bool(near)
                counters["waiting_ring"] += any(g in ring for g in near)
                counters["waiting_slot2"] += any(g in ring and slots.get(g) == 2 for g in near)
            if len(ring) >= 3:
                counters["ring_snaps"] += 1
                counters["ring_pair"] += any(
                    slots[a] != slots[b] and dist2(ra[1:3], rb[1:3]) <= CHAIN_JUMP
                    for a, ra in ring.items() for b, rb in ring.items() if a < b)
            for guid, row in ring.items():
                lists["ring_boss"].append(dist2(row[1:3], spot))
                counters["ring_bodies"] += 1
                counters["ring_front"] += angle_off(bearing(spot, row[1:3]), boss[4]) < 90.0
                if when <= self.start + UPTIME_END_MS:
                    counters["uptime_rows"] += 1
                    counters["uptime_on"] += len(row) > 7 and row[7] == self.boss
            for row in camp.values():
                lists["camp_boss"].append(dist2(row[1:3], spot))
            counters["camp_tank"] += any(
                dist2(row[1:3], floor[guid][1:3]) <= CHAIN_JUMP for row in camp.values() for guid in tanks_up)
        self.counters = counters
        self.lists = lists
        self.boss_hp = boss_hp

    def victim_at(self, when: int):
        found = None
        for t, victim, _ in self.victims:
            if t > when:
                break
            found = victim
        return found

    def trades(self) -> list[float]:
        moved = []
        for index in range(1, len(self.victims)):
            t, victim, spot = self.victims[index]
            _, before, before_spot = self.victims[index - 1]
            if victim == before or victim not in self.tanks or before not in self.tanks:
                continue
            later = [s for when, v, s in self.victims[index:] if when >= t + TRADE_LOOK_MS]
            if later:
                moved.append(dist2(before_spot, later[0]))
        return moved

    def swaps(self) -> tuple[list[float], int]:
        delays, unanswered = [], 0
        for rec in self.trace.of("aura"):
            if rec.get("sp") != UNBALANCING_STRIKE or rec.get("r") or rec["t"] < self.start:
                continue
            struck = rec.get("d")
            if struck not in self.tanks:
                continue
            answer = next((t for t, victim, _ in self.victims
                           if rec["t"] <= t <= rec["t"] + SWAP_WAIT_MS and victim in self.tanks and victim != struck),
                          None)
            if answer is None:
                unanswered += 1
            else:
                delays.append((answer - rec["t"]) / 1000.0)
        return delays, unanswered

    def opening(self) -> tuple[tuple[int, float] | None, float | None, float | None]:
        first = None
        previous = None
        for when, victim, _ in self.victims:
            if previous in self.tanks and victim in self.tanks and victim != previous:
                first = when
                break
            previous = victim
        taker_off = None
        if first is not None:
            taker = self.victim_at(first)
            for snap in frames(self.trace):
                if snap["t"] >= first:
                    break
                row = next((row for row in snap.get("u", []) if row[0] == taker), None)
                if row:
                    taker_off = dist2(row[1:3], TANK_SPOT)
        dragged = None
        if first is not None:
            dragged = max((dist2(spot, TANK_SPOT) for when, _, spot in self.victims
                           if first <= when <= first + DRAG_LOOK_MS), default=None)
        early = [(t, hp) for t, hp in self.boss_hp if t <= self.start + OPENING_MS]
        rate = None
        if len(early) >= 2 and early[-1][0] > early[0][0]:
            rate = (early[0][1] - early[-1][1]) / ((early[-1][0] - early[0][0]) / 1000.0)
        return ((first, taker_off) if first is not None else None), dragged, rate

    def pickup(self) -> tuple[int | None, int, int]:
        taken = next((when for when, victim, _ in self.victims if victim in self.tanks), None)
        until = taken if taken is not None else self.end
        swings = [rec for rec in self.trace.of("dmg")
                  if self.start <= rec["t"] <= until and rec.get("s") == self.boss and not rec.get("sp")
                  and rec.get("d") in self.players and rec.get("d") not in self.tanks]
        return taken, len(swings), sum(rec.get("a", 0) for rec in swings)

    def ring_moves_per_minute(self) -> float | None:
        ring = {guid for _, guid, _ in self.slot_events}
        minutes = (self.end - self.start - SETTLED_MS) / 60000.0
        if not ring or minutes <= 0:
            return None
        moves = sum(1 for rec in self.trace.of("move")
                    if rec.get("g") in ring and rec.get("ok") and rec.get("by") == RING_MOVER
                    and self.start + SETTLED_MS <= rec["t"] <= self.end)
        return moves / len(ring) / minutes

    def chain_casts(self) -> list[list[dict]]:
        return chain_casts(self.trace, self.start, self.players)

    def stalls(self) -> list[tuple[int, int, float, float]]:
        held = [(t, hp) for (t, hp), (_, victim, _) in zip(self.boss_hp, self.victims) if victim in self.tanks]
        found = []
        index = 0
        while index < len(held):
            begin, floor = held[index]
            top = floor
            last = index
            while (last + 1 < len(held) and held[last + 1][1] >= top - STALL_HP
                   and held[last + 1][0] - held[last][0] <= STALL_GAP_MS):
                last += 1
                top = max(top, held[last][1])
            if held[last][0] - begin >= STALL_MS:
                found.append((begin, held[last][0], floor, top))
                index = last + 1
            else:
                index += 1
        return found

    def unreach_spans(self) -> list[tuple[int, int, int]] | None:
        marks = notes(self.trace, "thorim.unreach")
        if not marks:
            return None
        spans = []
        for index, rec in enumerate(marks):
            flagged = int(rec.get("txt") or 0)
            stop = marks[index + 1]["t"] if index + 1 < len(marks) else self.end
            if flagged and stop > self.start:
                spans.append((max(rec["t"], self.start), stop, flagged))
        return spans

    def tank_hazards(self) -> dict:
        tally = collections.defaultdict(lambda: [0, 0])
        for rec in self.trace.of("dmg"):
            guid = rec.get("d")
            if rec["t"] < self.start or guid not in self.tanks:
                continue
            spell = rec.get("sp")
            if spell in BLIZZARD:
                kind = "Blizzard"
            elif spell == LIGHTNING_CHARGE:
                kind = "Lightning Charge"
            elif spell in FROST_NOVA:
                kind = "Frost Nova"
            else:
                continue
            who = "holder" if self.victim_at(rec["t"]) == guid else "waiting"
            tally[(kind, who)][0] += 1
            tally[(kind, who)][1] += rec.get("a", 0)
        return tally


def chain_line(phase: Phase2) -> str:
    trace = phase.trace
    casts = phase.chain_casts()
    hits = sum(len(cast) for cast in casts)
    on_tank = [rec for cast in casts for rec in cast if rec["d"] in phase.tanks]
    by_slot = collections.Counter()
    slot_damage = collections.Counter()
    crossings = 0
    for cast in casts:
        slots = phase.slots_at(cast[0]["t"])
        ordered = sorted(cast, key=lambda rec: rec.get("a", 0))
        for rec in ordered:
            slot = slots.get(rec["d"])
            if slot is not None and rec["d"] not in phase.tanks:
                by_slot[slot] += 1
                slot_damage[slot] += rec.get("a", 0)
        crossings += sum(1 for first, second in zip(ordered, ordered[1:])
                         if first["d"] in phase.tanks and second["d"] not in phase.tanks
                         and slots.get(second["d"]) is not None)
    deaths = sum(1 for death in combat_deaths(trace)
                 if death["t"] >= phase.start and blow_spell(death) in CHAIN_LIGHTNING)
    slot_text = "/".join(str(by_slot[slot]) for slot in range(3))
    return (f"casts {len(casts)}, hits on players {hits}; on tanks {len(on_tank)} "
            f"({sum(rec.get('a', 0) for rec in on_tank) // 1000}k), on slot 0/1/2 {slot_text} "
            f"(slot 2 {slot_damage[2] // 1000}k); tank-to-ring hops {crossings}; deaths {deaths}")


def tank_death_line(phase: Phase2) -> str:
    deaths = [death for death in combat_deaths(phase.trace)
              if death["t"] >= phase.start and death["g"] in phase.tanks]
    if not deaths:
        return "none"
    kinds = collections.Counter(mechanic(phase.trace, death) for death in deaths)
    first = (deaths[0]["t"] - phase.start) / 1000.0
    return f"{len(deaths)}, first at +{first:.1f} s: " + ", ".join(f"{name} {n}" for name, n in kinds.most_common())


def untouchable_line(phase: Phase2) -> str:
    stalls = phase.stalls()
    stall_text = (f"held without losing health {len(stalls)}x, "
                  f"{sum(stop - begin for begin, stop, _, _ in stalls) / 1000.0:.1f} s"
                  + "".join(f"; +{(begin - phase.start) / 1000.0:.1f}..+{(stop - phase.start) / 1000.0:.1f} s "
                            f"{low:.1f}% to max {high:.1f}%" for begin, stop, low, high in stalls))
    spans = phase.unreach_spans()
    if spans is None:
        return f"no thorim.unreach probe (a build before it); {stall_text}"
    steps = sum(1 for rec in phase.trace.of("move")
                if rec.get("ok") and rec.get("by") == UNREACH_MOVER and phase.start <= rec["t"] <= phase.end)
    total = sum(stop - begin for begin, stop, _ in spans) / 1000.0
    longest = max(((stop - begin) / 1000.0 for begin, stop, _ in spans), default=0.0)
    named = collections.Counter(phase.trace.name(guid) for _, _, guid in spans)
    who = ", ".join(f"{name} {n}" for name, n in named.most_common(3)) or "nobody"
    return (f"cannot-reach flag {len(spans)}x, {total:.1f} s, longest {longest:.1f} s ({who}); "
            f"step moves {steps}; {stall_text}")


def opening_line(phase: Phase2) -> str:
    trade, dragged, rate = phase.opening()
    trade_text = "no tank-to-tank trade"
    if trade:
        first, taker_off = trade
        trade_text = (f"first trade +{(first - phase.start) / 1000.0:.1f} s, the taker {yd(taker_off)} yd off the "
                      f"tank spot, Thorim up to {yd(dragged)} yd off it in the next {DRAG_LOOK_MS // 1000} s")
    rate_text = "-" if rate is None else f"{rate:.2f}"
    return f"{trade_text}; {rate_text} %/s over the first {OPENING_MS // 1000} s"


def pickup_line(phase: Phase2) -> str:
    taken, swings, damage = phase.pickup()
    taken_text = "no tank on him" if taken is None else f"a tank on him at +{(taken - phase.start) / 1000.0:.1f} s"
    return (f"{taken_text}; before it Thorim's melee on non-tanks {damage // 1000}k from {swings} "
            f"swing{'' if swings == 1 else 's'}")


def show_phase2(trace: Trace) -> None:
    print("PHASE 2")
    if not mine(trace):
        return
    start = phase2_start(trace)
    if start is None:
        print("  phase 2 never started")
        return
    phase = Phase2(trace, start)
    c, lists = phase.counters, phase.lists
    print(f"  from {clock(start)}, {'hard' if hard_mode(trace) else 'normal'} mode, "
          f"{(phase.end - start) / 1000.0:.1f} s on the floor; settled snapshots with a tank holding him {c['held']}")
    print(f"  chain lightning   {chain_line(phase)}")
    print(f"  tank deaths       {tank_death_line(phase)}")
    print(f"  untouchable       {untouchable_line(phase)}")
    print(f"  pickup            {pickup_line(phase)}")
    print(f"  opening           {opening_line(phase)}")
    print(f"  thorim spot       off the tank spot p50 {yd(quantile(lists['boss_off_spot'], .5))} "
          f"p90 {yd(quantile(lists['boss_off_spot'], .9))} yd; facing off the tank-spot axis "
          f"p50 {yd(quantile(lists['facing'], .5))} p90 {yd(quantile(lists['facing'], .9))} deg, "
          f"over {FACING_OFF_DEG:.0f} deg {share(c['facing_off'], c['held'])}")
    moved = phase.trades()
    delays, unanswered = phase.swaps()
    print(f"  trades            {len(moved)} tank-to-tank, Thorim moved p50 {yd(quantile(moved, .5))} "
          f"p90 {yd(quantile(moved, .9))} yd across one; strike to swap p50 {yd(quantile(delays, .5))} "
          f"max {yd(max(delays) if delays else None)} s, {unanswered} unanswered; "
          f"on a non-tank with a tank up {c['loose_ms'] / 1000.0:.1f} s")
    offsets = phase.waiting_offsets
    offset = (f"({quantile([o[0] for o in offsets], .5):+.1f}, {quantile([o[1] for o in offsets], .5):+.1f})"
              if offsets else "-")
    moves = collections.Counter()
    for rec in trace.of("move"):
        guid = rec.get("g")
        if rec["t"] < start + SETTLED_MS or guid not in phase.tanks or not rec.get("ok"):
            continue
        if phase.victim_at(rec["t"]) in (guid, None):
            continue
        moves[rec.get("by") or "?"] += 1
    move_text = ", ".join(f"{name} {n}" for name, n in moves.most_common(4)) or "none"
    print(f"  waiting tank      offset from the tank spot {offset} yd, on the holder "
          f"{share(c['waiting_stacked'], c['waiting'])}, from Thorim "
          f"p50 {yd(quantile(lists['waiting_boss'], .5))} p90 {yd(quantile(lists['waiting_boss'], .9))} yd, "
          f"past {MELEE_EDGE:.1f} yd {share(c['waiting_out'], c['waiting'])}; moves: {move_text}")
    print(f"  bridges           waiting tank within {CHAIN_JUMP:.0f} yd of a non-tank body "
          f"{share(c['waiting_body'], c['waiting'])}, of a ring body {share(c['waiting_ring'], c['waiting'])}, "
          f"of slot 2 {share(c['waiting_slot2'], c['waiting'])}; holder of a ring body "
          f"{share(c['holder_ring'], c['held'])}")
    print(f"  melee ring        off Thorim p50 {yd(quantile(lists['ring_boss'], .5))} yd, in his front arc "
          f"{share(c['ring_front'], c['ring_bodies'])}, two slots within {CHAIN_JUMP:.0f} yd "
          f"{share(c['ring_pair'], c['ring_snaps'])}, on Thorim to +{UPTIME_END_MS // 1000} s "
          f"{share(c['uptime_on'], c['uptime_rows'])}, moves per ring bot a minute "
          f"{yd(phase.ring_moves_per_minute())}")
    print(f"  camp              off Thorim p50 {yd(quantile(lists['camp_boss'], .5))} yd, "
          f"a camp body within {CHAIN_JUMP:.0f} yd of a tank {share(c['camp_tank'], c['held'])}")
    hazards = phase.tank_hazards()
    parts = []
    for kind in ("Blizzard", "Lightning Charge", "Frost Nova"):
        holder, waiting = hazards[(kind, "holder")], hazards[(kind, "waiting")]
        parts.append(f"{kind} {holder[0]} ({holder[1] // 1000}k) / {waiting[0]} ({waiting[1] // 1000}k)")
    print(f"  tank hazards      holder / waiting: {', '.join(parts)}")
    if len(phase.boss_hp) >= 2:
        (t0, hp0), (t1, hp1) = phase.boss_hp[0], phase.boss_hp[-1]
        rate = (hp0 - hp1) / ((t1 - t0) / 1000.0) if t1 > t0 else 0.0
        print(f"  thorim health     {hp0:.1f}% to {hp1:.1f}%, {rate:.2f} %/s")


class RingBlizzard:
    def __init__(self, trace: Trace, start: int):
        self.trace = trace
        self.start = start
        self.boss = thorim_guid(trace)
        self.ring = collections.defaultdict(list)
        for note in notes(trace, "thorim.ringspot"):
            if found := RINGSPOT_SOLVED.match(str(note.get("txt"))):
                self.ring[note.get("g")].append(
                    (note["t"], int(found.group(1)), math.radians(int(found.group(3)))))
        self.steps = collections.defaultdict(list)
        for note in notes(trace, "thorim.ringstep"):
            self.steps[note.get("g")].append((note["t"], str(note.get("txt")) == "1"))
        self.probed = bool(self.steps)
        self.hits = [rec for rec in trace.of("dmg") if rec.get("sp") in BLIZZARD and rec.get("d") in self.ring]
        self.bunny_track: list[tuple[int, float, float]] = []
        self.zone_track: list[tuple[int, list]] = []
        self.casts = self._casts()

    def _casts(self) -> list[dict]:
        bunnies = {guid for guid, entry in self.trace.entries.items() if entry == NPC_SIF_BLIZZARD}
        seen: dict = {}
        for snap in frames(self.trace):
            if snap["t"] < self.start:
                continue
            zones = [(zone[1], zone[2]) for zone in snap.get("hz", []) if zone[0] in BLIZZARD]
            if zones:
                self.zone_track.append((snap["t"], zones))
            for row in snap.get("u", []):
                if row[0] in bunnies:
                    self.bunny_track.append((snap["t"], row[1], row[2]))
                    first, _ = seen.get(row[0], (snap["t"], snap["t"]))
                    seen[row[0]] = (first, snap["t"])
        spans = sorted(seen.values())
        casts = []
        for index, (first, last) in enumerate(spans):
            end = last + BLIZZARD_ZONE_MS
            if index + 1 < len(spans):
                end = min(end, spans[index + 1][0])
            casts.append({"begin": first, "end": end})
        return casts

    @staticmethod
    def _latest(events: list, when: int):
        found = None
        for event in events:
            if event[0] > when:
                break
            found = event
        return found

    def stepped(self, guid, when: int) -> bool:
        event = self._latest(self.steps.get(guid, []), when)
        return bool(event and event[1])

    def read(self, cast: dict) -> dict:
        begin, end = cast["begin"], cast["end"]
        covered = collections.Counter()
        in_zone = collections.Counter()
        stays = collections.defaultdict(list)
        inside_since: dict = {}
        stepped_ms = 0
        out_ms = 0
        previous = None
        snaps = [snap for snap in frames(self.trace) if begin <= snap["t"] < end]
        for snap in snaps:
            when = snap["t"]
            dt = min(when - previous, 1000) if previous is not None else 0
            previous = when
            rows = {row[0]: row for row in snap.get("u", [])}
            boss = rows.get(self.boss)
            if not boss or boss[5] <= 0 or boss[3] > FLOOR_Z:
                continue
            spot = (boss[1], boss[2])
            zones = [(zone[1], zone[2]) for zone in snap.get("hz", []) if zone[0] in BLIZZARD]
            marks = zones + [(row[1], row[2]) for guid, row in rows.items()
                             if self.trace.entries.get(guid) == NPC_SIF_BLIZZARD]
            points = {}
            for guid, events in self.ring.items():
                event = self._latest(events, when)
                if event:
                    points[event[1]] = (spot[0] + RING_RADIUS * math.cos(event[2]),
                                        spot[1] + RING_RADIUS * math.sin(event[2]))
            for slot, point in points.items():
                if any(dist2(point, mark) < RING_BLIZZARD_CLEARANCE for mark in marks):
                    covered[slot] += dt
            for guid in self.ring:
                row = rows.get(guid)
                alive = row is not None and row[5] > 0 and row[3] <= FLOOR_Z
                inside = alive and any(dist2(row[1:3], zone) <= BLIZZARD_REACH for zone in zones)
                if inside:
                    in_zone[guid] += dt
                    inside_since[guid] = inside_since.get(guid, 0) + dt
                elif guid in inside_since:
                    stays[guid].append(inside_since.pop(guid))
                if alive and self.stepped(guid, when):
                    stepped_ms += dt
                    out_ms += dt if dist2(row[1:3], spot) > MELEE_EDGE else 0
        for guid, stay in inside_since.items():
            stays[guid].append(stay)

        ticks = collections.Counter()
        damage = collections.Counter()
        for rec in self.hits:
            if begin <= rec["t"] < end:
                ticks[rec["d"]] += 1
                damage[rec["d"]] += rec.get("a", 0)
        step_moves = [rec for rec in self.trace.of("move")
                      if begin <= rec["t"] < end and rec.get("ok") and rec.get("by") == BLIZZARD_MOVER
                      and rec.get("g") in self.ring]
        deaths = sum(1 for death in combat_deaths(self.trace)
                     if begin <= death["t"] < end and death["g"] in self.ring
                     and blow_spell(death) in BLIZZARD)
        walks = []
        walk_ticks = 0
        for guid, events in self.steps.items():
            for when, on in events:
                if on or not begin <= when < end or guid not in self.ring:
                    continue
                back = self._back_in_reach(guid, when)
                if back is None:
                    continue
                walks.append((back - when) / 1000.0)
                walk_ticks += sum(1 for rec in self.hits if rec["d"] == guid and when <= rec["t"] <= back)
        return {"covered": covered, "in_zone": in_zone, "stays": stays, "ticks": ticks, "damage": damage,
                "steps": len(step_moves), "onto_path": sum(1 for rec in step_moves if self.onto_path(rec)),
                "overrun": sum(1 for rec in step_moves if self.overrun(rec)), "deaths": deaths,
                "stepped_ms": stepped_ms, "out_ms": out_ms, "walks": walks, "walk_ticks": walk_ticks}

    def onto_path(self, move: dict) -> bool:
        bunny = self._latest(self.bunny_track, move["t"])
        if "x" not in move or not bunny or move["t"] - bunny[0] > BUNNY_FRESH_MS:
            return False
        return path_ahead_gap((move["x"], move["y"]), bunny[1:]) < RING_BLIZZARD_CLEARANCE

    def overrun(self, move: dict) -> bool:
        if "x" not in move:
            return False
        spot = (move["x"], move["y"])
        return any(dist2(spot, zone) <= BLIZZARD_REACH for when, zones in self.zone_track
                   if move["t"] <= when <= move["t"] + STEP_OVERRUN_MS for zone in zones)

    def _back_in_reach(self, guid, when: int):
        for snap in frames(self.trace):
            if snap["t"] < when:
                continue
            rows = {row[0]: row for row in snap.get("u", [])}
            boss, row = rows.get(self.boss), rows.get(guid)
            if not boss or not row or row[5] <= 0:
                return None
            if dist2(row[1:3], boss[1:3]) <= MELEE_EDGE:
                return snap["t"]
        return None


def step_points(read: dict) -> str:
    return (f"{read['onto_path']} onto the bunny's path ahead, {read['overrun']} under a zone within "
            f"{STEP_OVERRUN_MS // 1000} s")


def ring_blizzard_line(blizzard: RingBlizzard, label: str, read: dict) -> str:
    trace = blizzard.trace
    covered = ", ".join(f"{slot} {ms / 1000.0:.1f} s" for slot, ms in sorted(read["covered"].items())) or "none"
    who = []
    for guid, ms in sorted(read["in_zone"].items(), key=lambda item: -item[1]):
        stays = read["stays"].get(guid, [])
        who.append(f"{trace.name(guid)} {ms / 1000.0:.1f} s ({len(stays)}x, longest "
                   f"{max(stays, default=0) / 1000.0:.1f} s, {read['ticks'][guid]} ticks "
                   f"{read['damage'][guid] // 1000}k)")
    hit_only = [guid for guid in read["ticks"] if guid not in read["in_zone"]]
    who += [f"{trace.name(guid)} 0.0 s ({read['ticks'][guid]} ticks {read['damage'][guid] // 1000}k)"
            for guid in hit_only]
    if blizzard.probed:
        walks = read["walks"]
        steps = (f"steps {read['steps']} ({step_points(read)}), stepped {read['stepped_ms'] / 1000.0:.1f} bot-s, "
                 f"out of reach {read['out_ms'] / 1000.0:.1f} of them; walk back p50 {yd(quantile(walks, .5))} "
                 f"max {yd(max(walks) if walks else None)} s, {read['walk_ticks']} ticks on the way")
    else:
        steps = "no thorim.ringstep probe (a build before it)"
    return (f"{label}: slot points covered {covered}; in it {', '.join(who) or 'nobody'}; "
            f"deaths {read['deaths']}; {steps}")


def show_blizzard(trace: Trace) -> None:
    print("RING BLIZZARD")
    if not mine(trace):
        return
    start = phase2_start(trace)
    if start is None:
        print("  phase 2 never started")
        return
    blizzard = RingBlizzard(trace, start)
    if not blizzard.casts:
        print("  no Sif Blizzard in phase 2")
        return
    total = {"covered": collections.Counter(), "in_zone": collections.Counter(),
             "stays": collections.defaultdict(list), "ticks": collections.Counter(),
             "damage": collections.Counter(), "steps": 0, "onto_path": 0, "overrun": 0, "deaths": 0,
             "stepped_ms": 0, "out_ms": 0, "walks": [], "walk_ticks": 0}
    for index, cast in enumerate(blizzard.casts, 1):
        read = blizzard.read(cast)
        label = (f"blizzard {index} +{(cast['begin'] - start) / 1000.0:.1f}..+{(cast['end'] - start) / 1000.0:.1f} s")
        print(f"  {ring_blizzard_line(blizzard, label, read)}")
        for key in ("covered", "in_zone", "ticks", "damage"):
            total[key].update(read[key])
        for guid, stays in read["stays"].items():
            total["stays"][guid] += stays
        for key in ("steps", "onto_path", "overrun", "deaths", "stepped_ms", "out_ms", "walk_ticks"):
            total[key] += read[key]
        total["walks"] += read["walks"]
    stays = [stay / 1000.0 for values in total["stays"].values() for stay in values]
    print(f"  all: ring melee in a zone {sum(total['in_zone'].values()) / 1000.0:.1f} bot-s, stays p50 "
          f"{yd(quantile(stays, .5))} max {yd(max(stays) if stays else None)} s, "
          f"{sum(total['ticks'].values())} ticks {sum(total['damage'].values()) // 1000}k, deaths {total['deaths']}"
          + (f"; steps {total['steps']} ({step_points(total)}), stepped {total['stepped_ms'] / 1000.0:.1f} bot-s, "
             f"out of reach "
             f"{total['out_ms'] / 1000.0:.1f}; walk back p50 {yd(quantile(total['walks'], .5))} max "
             f"{yd(max(total['walks']) if total['walks'] else None)} s, {total['walk_ticks']} ticks on the way"
             if blizzard.probed else ""))


class ChainBridges:
    def __init__(self, trace: Trace, start: int):
        self.trace = trace
        self.start = start
        self.tanks = tanks_of(trace)
        self.casts = chain_casts(trace, start, set(trace.roles))
        self.slots = collections.defaultdict(list)
        for note in notes(trace, "thorim.ringspot"):
            if found := RINGSPOT.match(str(note.get("txt"))):
                self.slots[note.get("g")].append((note["t"], int(found.group(1))))
        self.steps = collections.defaultdict(list)
        for note in notes(trace, "thorim.ringstep"):
            self.steps[note.get("g")].append((note["t"], str(note.get("txt")) == "1"))
        self.lust = collections.defaultdict(list)
        for rec in trace.of("aura"):
            if rec.get("sp") not in LUST:
                continue
            spans = self.lust[rec.get("d")]
            if not rec.get("r"):
                spans.append((rec["t"], rec["t"] + (rec.get("dur") or LUST_MS)))
            elif spans and spans[-1][1] > rec["t"]:
                spans[-1] = (spans[-1][0], rec["t"])
        self.died = collections.defaultdict(list)
        for death in combat_deaths(trace):
            if blow_spell(death) in CHAIN_LIGHTNING:
                self.died[death["g"]].append(death["t"])

    def group(self, guid, when: int) -> tuple[str, bool]:
        if guid in self.tanks:
            return "tank", False
        slot = RingBlizzard._latest(self.slots.get(guid, []), when)
        if slot is None:
            return "camp", False
        step = RingBlizzard._latest(self.steps.get(guid, []), when)
        return f"slot {slot[1]}", bool(step and (step[1] or when - step[0] <= CHAIN_STEP_BACK_MS))

    def lusted(self, guid, when: int) -> bool:
        return any(begin <= when <= end for begin, end in self.lust.get(guid, []))

    def gap(self, first, second, when: int):
        a = at(self.trace, first, when, NEAREST, CHAIN_POSITION_MS)
        b = at(self.trace, second, when, NEAREST, CHAIN_POSITION_MS)
        return dist2(a, b) if a and b else None

    def read(self, cast: list[dict]) -> list[dict]:
        segments: list[dict] = []
        for rec in cast:
            guid, when = rec["d"], rec["t"]
            group, stepped = self.group(guid, when)
            dead = any(when <= t <= when + CHAIN_DEATH_MS for t in self.died.get(guid, []))
            if not segments or segments[-1]["group"] != group:
                previous = segments[-1] if segments else None
                segments.append({
                    "group": group, "hits": 0, "stepped": False, "dead": 0,
                    "from": previous["group"] if previous else None,
                    "gap": self.gap(previous["last"], guid, when) if previous else None,
                    "lust": bool(previous) and self.lusted(previous["last"], when) and self.lusted(guid, when)})
            segment = segments[-1]
            segment["hits"] += 1
            segment["stepped"] |= stepped
            segment["dead"] += dead
            segment["last"] = guid
        return segments


def group_kind(group: str) -> str:
    return "slot" if group.startswith("slot") else group


def crossing_kind(segments: list[dict], index: int) -> str:
    return f"{group_kind(segments[index]['from'])} to {group_kind(segments[index]['group'])}"


def segment_text(segment: dict) -> str:
    name = segment["group"] + (" stepped" if segment["stepped"] else "")
    text = f"{name} x{segment['hits']}" + (f" {segment['dead']} dead" if segment["dead"] else "")
    if segment["from"] is None:
        return text
    gap = "?" if segment["gap"] is None else f"{segment['gap']:.1f}"
    return f"{gap} yd{' (lust)' if segment['lust'] else ''} to {text}"


def show_chains(trace: Trace) -> None:
    print("CHAIN LIGHTNING")
    if not mine(trace):
        return
    start = phase2_start(trace)
    if start is None:
        print("  phase 2 never started")
        return
    bridges = ChainBridges(trace, start)
    if not bridges.casts:
        print("  no Chain Lightning on a player")
        return
    first_tank = next((death["t"] for death in combat_deaths(trace)
                       if death["t"] >= start and death["g"] in bridges.tanks), None)
    crossings = collections.Counter()
    stepped_crossings = collections.Counter()
    deaths = collections.Counter()
    stepped_deaths = collections.Counter()
    early = 0
    for cast in bridges.casts:
        segments = bridges.read(cast)
        dead = sum(segment["dead"] for segment in segments)
        early += dead if first_tank is None or cast[0]["t"] < first_tank else 0
        print(f"  +{(cast[0]['t'] - start) / 1000.0:.1f} s: {len(cast)} hit{'' if len(cast) == 1 else 's'}, "
              f"{dead} dead; "
              + ", ".join(segment_text(segment) for segment in segments))
        deaths["in the seed's group"] += segments[0]["dead"]
        for index in range(1, len(segments)):
            kind = crossing_kind(segments, index)
            crossings[kind] += 1
            deaths[f"after {kind}"] += segments[index]["dead"]
            if segments[index]["stepped"] or segments[index - 1]["stepped"]:
                stepped_crossings[kind] += 1
                stepped_deaths[kind] += segments[index]["dead"]
    hits = sum(len(cast) for cast in bridges.casts)

    def counted(counter: collections.Counter) -> str:
        return ", ".join(f"{name} {n}" for name, n in counter.most_common() if n) or "none"

    print(f"  all: casts {len(bridges.casts)}, hits {hits}, deaths {sum(deaths.values())}, {early} before the first "
          f"tank death ({counted(deaths)}; through a stepped slot: {counted(stepped_deaths)}); crossings "
          f"{counted(crossings)}; through a stepped slot: {counted(stepped_crossings)}")


def cone_casts(trace: Trace, start: int) -> list[int]:
    casts: list[int] = []
    for rec in trace.of("dmg"):
        if rec.get("sp") == LIGHTNING_CHARGE and rec["t"] >= start and (not casts or rec["t"] - casts[-1] > CAST_GAP_MS):
            casts.append(rec["t"])
    return casts


def lit_orbs(trace: Trace, start: int) -> list[int]:
    lit: list[int] = []
    for note in notes(trace, "thorim.lightningorb"):
        if note["t"] >= start and str(note.get("txt")) not in ("", "0") and (not lit or note["t"] - lit[-1] > LIT_GAP_MS):
            lit.append(note["t"])
    return lit


class ShelterWalks:
    def __init__(self, trace: Trace, start: int):
        self.trace = trace
        self.start = start
        self.camp = {note.get("g") for note in notes(trace, "thorim.rangedslot")}
        self.shelters = [note for note in notes(trace, "thorim.shelter") if " orb " in str(note.get("txt"))]
        self.moves = [rec for rec in trace.of("move") if rec.get("by") == RING_MOVER and "x" in rec]
        self.hits = [rec for rec in trace.of("dmg") if rec.get("sp") == LIGHTNING_CHARGE]
        self.slows = [rec for rec in trace.of("aura") if rec.get("sp") in SLOWS and not rec.get("r")]
        self.deaths = [death for death in combat_deaths(trace) if blow_spell(death) == LIGHTNING_CHARGE]

    def read(self, lit: int, cast: int) -> dict:
        walkers: dict = {}
        for note in self.shelters:
            if lit - SHELTER_LEAD_MS <= note["t"] <= cast:
                walker = walkers.setdefault(note.get("g"), {"step": False})
                walker["step"] |= str(note.get("txt")).endswith(" step")
        hit = {rec["d"] for rec in self.hits if abs(rec["t"] - cast) <= CAST_GAP_MS}
        dead = {death["g"] for death in self.deaths if abs(death["t"] - cast) <= CHAIN_DEATH_MS}
        for guid, walker in walkers.items():
            heading = [rec for rec in self.moves if rec.get("g") == guid and lit - SHELTER_LEAD_MS <= rec["t"] <= cast]
            begin = at(self.trace, guid, lit, NEAREST, CHAIN_POSITION_MS)
            end = at(self.trace, guid, cast, NEAREST, CHAIN_POSITION_MS)
            target = (heading[-1]["x"], heading[-1]["y"]) if heading else None
            walker["walk"] = dist2(begin, target) if begin and target else None
            walker["left"] = dist2(end, target) if end and target else None
            walker["hit"] = guid in hit
            walker["dead"] = guid in dead
            walker["slowed"] = sorted({SLOWS[rec["sp"]] for rec in self.slows
                                       if rec.get("d") == guid and lit - SLOW_LOOK_MS <= rec["t"] <= cast})
        return {"walkers": walkers, "off_walk": sorted(guid for guid in hit if guid in self.camp and guid not in walkers),
                "dead_off_walk": sum(1 for guid in dead if guid in self.camp and guid not in walkers)}


def shelter_walk_text(trace: Trace, guid, walker: dict) -> str:
    there = walker["left"] is not None and walker["left"] <= SHELTER_ARRIVED
    where = "there" if there else f"short {yd(walker['left'])}"
    return (f"{trace.name(guid)} {'step' if walker['step'] else 'row'} {yd(walker['walk'])} yd {where}"
            + (f" slowed {', '.join(walker['slowed'])}" if walker["slowed"] else "")
            + (" dead" if walker["dead"] else ""))


def cone_windows(trace: Trace, start: int) -> list[tuple[int, int]]:
    casts = cone_casts(trace, start)
    windows: list[tuple[int, int]] = []
    for when in lit_orbs(trace, start):
        cast = next((t for t in casts if CONE_LEAD_MS[0] <= t - when <= CONE_LEAD_MS[1]), None)
        if cast is not None:
            windows.append((when, cast))
    return windows


def first_tank_death(trace: Trace, start: int) -> int | None:
    return next((death["t"] for death in combat_deaths(trace)
                 if death["t"] >= start and death["g"] in tanks_of(trace)), None)


def show_shelters(trace: Trace) -> None:
    print("CAMP SHELTERS")
    if not mine(trace):
        return
    start = phase2_start(trace)
    if start is None:
        print("  phase 2 never started")
        return
    if not lit_orbs(trace, start):
        print("  no thorim.lightningorb probe or no lit orb in phase 2")
        return
    walks = ShelterWalks(trace, start)
    first_tank = first_tank_death(trace, start)
    total = collections.Counter()
    for when, cast in cone_windows(trace, start):
        read = walks.read(when, cast)
        walkers = read["walkers"]
        there = sum(1 for walker in walkers.values()
                    if walker["left"] is not None and walker["left"] <= SHELTER_ARRIVED)
        hit = [guid for guid, walker in walkers.items() if walker["hit"]]
        early = first_tank is None or cast < first_tank
        counts = {"walks": len(walkers), "steps": sum(walker["step"] for walker in walkers.values()), "there": there,
                  "hit": len(hit), "dead": sum(walker["dead"] for walker in walkers.values()),
                  "slowed hit": sum(1 for guid in hit if walkers[guid]["slowed"]),
                  "off a walk": len(read["off_walk"]), "dead off a walk": read["dead_off_walk"]}
        for key, n in counts.items():
            total[key] += n
            if early:
                total["early " + key] += n
        print(f"  +{(when - start) / 1000.0:.1f} s: walks {counts['walks']} (steps {counts['steps']}), there "
              f"{there}, hit {counts['hit']}, dead {counts['dead']}; camp hit off a walk {counts['off a walk']}"
              + "".join(f"; {shelter_walk_text(trace, guid, walkers[guid])}" for guid in hit))
    print(f"  all: walks {total['walks']} (steps {total['steps']}), there {total['there']}, hit {total['hit']} "
          f"({total['slowed hit']} slowed), dead {total['dead']}; camp hit off a walk {total['off a walk']}, dead "
          f"{total['dead off a walk']}; before the first tank death walks {total['early walks']} (steps "
          f"{total['early steps']}), hit {total['early hit']}, dead {total['early dead']}")


class RingCone:
    def __init__(self, trace: Trace):
        self.trace = trace
        self.boss = thorim_guid(trace)
        self.ring = {note.get("g") for note in notes(trace, "thorim.ringspot")}
        probes = notes(trace, RING_CONE_HOLD)
        self.probed = bool(probes)
        self.holds = [note for note in probes if str(note.get("txt")) == "hold"]
        self.moves = [rec for rec in trace.of("move") if rec.get("g") in self.ring]
        self.hits = [rec for rec in trace.of("dmg") if rec.get("sp") == LIGHTNING_CHARGE and rec.get("d") in self.ring]
        self.deaths = {death["g"]: death["t"] for death in combat_deaths(trace)
                       if blow_spell(death) == LIGHTNING_CHARGE}

    def read(self, lit: int, cast: int) -> dict:
        held = {note.get("g") for note in self.holds if lit - SHELTER_LEAD_MS <= note["t"] <= cast}
        boss = at(self.trace, self.boss, cast, NEAREST, CHAIN_POSITION_MS)
        hits: dict = {}
        for guid in sorted({rec["d"] for rec in self.hits if abs(rec["t"] - cast) <= CAST_GAP_MS}):
            window = [rec for rec in self.moves if rec.get("g") == guid and lit - SHELTER_LEAD_MS <= rec["t"] <= cast]
            walks = [rec for rec in window if rec.get("ok")]
            if any(rec.get("r") == "blocked" for rec in window):
                kind = "blocked"
            elif walks:
                kind = "walking"
            elif guid in held:
                kind = "held"
            else:
                kind = "standing"
            where = at(self.trace, guid, cast, NEAREST, CHAIN_POSITION_MS)
            hits[guid] = {"kind": kind, "mover": walks[-1].get("by") if walks else None,
                          "far": bool(where and boss and dist2(where, boss) > RING_REACH),
                          "dead": abs(self.deaths.get(guid, -CHAIN_DEATH_MS * 10) - cast) <= CHAIN_DEATH_MS}
        return {"held": held, "hits": hits}


def ring_hit_text(trace: Trace, guid, hit: dict) -> str:
    return (f"{trace.name(guid)} {hit['kind']}" + (f" ({hit['mover']})" if hit["mover"] else "")
            + (", past reach" if hit["far"] else "") + (", dead" if hit["dead"] else ""))


def show_ringcone(trace: Trace) -> None:
    print("RING CONE")
    if not mine(trace):
        return
    start = phase2_start(trace)
    if start is None:
        print("  phase 2 never started")
        return
    if not lit_orbs(trace, start):
        print("  no thorim.lightningorb probe or no lit orb in phase 2")
        return
    cone = RingCone(trace)
    first_tank = first_tank_death(trace, start)
    total = collections.Counter()
    for when, cast in cone_windows(trace, start):
        read = cone.read(when, cast)
        hits, held = read["hits"], read["held"]
        dead = sum(hit["dead"] for hit in hits.values())
        held_hit = sum(1 for guid in held if guid in hits)
        early = first_tank is None or cast < first_tank
        for key, n in (("hit", len(hits)), ("dead", dead), ("held walks", len(held)), ("held walks hit", held_hit),
                       ("far", sum(hit["far"] for hit in hits.values())),
                       *((kind, sum(hit["kind"] == kind for hit in hits.values())) for kind in RING_HIT_KINDS)):
            total[key] += n
            if early:
                total["early " + key] += n
        if not hits and not held:
            continue
        held_text = f"held {len(held)} ({held_hit} hit)" if cone.probed else "held -"
        print(f"  +{(when - start) / 1000:.1f} s: ring hit {len(hits)}, dead {dead}; {held_text}"
              + "".join(f"; {ring_hit_text(trace, guid, hit)}" for guid, hit in hits.items()))
    kinds = ", ".join(f"{kind} {total[kind]}" for kind in RING_HIT_KINDS)
    held_text = (f"held walks {total['held walks']}, {total['held walks hit']} hit" if cone.probed
                 else "no thorim.ringconehold probe (a build before it)")
    print(f"  all: ring hit {total['hit']} ({kinds}; past reach {total['far']}), dead {total['dead']}; {held_text}; "
          f"before the first tank death hit {total['early hit']}, dead {total['early dead']}")


def thunder_orb_index(trace: Trace, guid, when: int) -> int | None:
    where = at(trace, guid, when, NEAREST) if guid else None
    if not where:
        return None
    return next((index for index, spot in enumerate(THUNDER_ORB_SPOTS) if dist2(where, spot) <= ORB_MATCH), None)


class ChainWaits:
    def __init__(self, trace: Trace, start: int):
        self.trace = trace
        self.camp = {note.get("g") for note in notes(trace, "thorim.rangedslot")}
        self.orbs = {note["t"]: int(note["txt"]) for note in notes(trace, "thorim.lightningorb")
                     if str(note.get("txt")).isdigit() and str(note.get("txt")) != "0"}
        self.shelters = notes(trace, "thorim.shelter")
        self.chains = chain_casts(trace, start, set(trace.roles))
        self.cones = [rec for rec in trace.of("dmg") if rec.get("sp") == LIGHTNING_CHARGE]
        self.deaths = [(death["g"], death["t"], blow_spell(death)) for death in combat_deaths(trace)]

    def orb_of(self, lit: int) -> int | None:
        index = thunder_orb_index(self.trace, self.orbs.get(lit), lit)
        if index is not None:
            return index
        for note in self.shelters:
            if lit - SHELTER_LEAD_MS <= note["t"] <= lit + WAIT_LOOK_MS:
                if found := SHELTER_ORB.match(str(note.get("txt"))):
                    return int(found.group(1))
        return None

    def died(self, guid, when: int, spells) -> bool:
        return any(g == guid and spell in spells and abs(t - when) <= CHAIN_DEATH_MS for g, t, spell in self.deaths)

    def read(self, lit: int) -> dict:
        waits: dict = {}
        for note in self.shelters:
            if not lit - SHELTER_LEAD_MS <= note["t"] <= lit + WAIT_LOOK_MS:
                continue
            text = str(note.get("txt"))
            if text.endswith(" wait"):
                waits.setdefault(note["g"], {"from": note["t"], "until": None, "step": False})
            elif note["g"] in waits and waits[note["g"]]["until"] is None:
                waits[note["g"]].update(until=note["t"], step=text.endswith(" step"))
        chain = next((cast for cast in self.chains if CHAIN_LOOK_MS[0] <= cast[0]["t"] - lit <= CHAIN_LOOK_MS[1]),
                     [])
        land = chain[0]["t"] if chain else None
        hops = [index + 1 for index, rec in enumerate(chain) if rec["d"] in self.camp]
        out = {"waits": waits, "camp_hops": hops,
               "camp_dead": sum(self.died(rec["d"], rec["t"], CHAIN_LIGHTNING) for rec in chain if rec["d"] in self.camp),
               "landed": 0, "standing": 0, "walking": 0, "chain_dead": 0, "cone_hit": 0, "cone_dead": 0, "steps": 0,
               "lengths": []}
        hit_at = {rec["d"]: rec["t"] for rec in chain}
        for guid, wait in waits.items():
            until = wait["until"]
            if until is not None:
                out["lengths"].append(until - wait["from"])
            out["landed"] += land is not None and wait["from"] <= land and (until is None or
                                                                             land <= until + RELEASE_SLACK_MS)
            if guid in hit_at:
                out["standing" if until is None or hit_at[guid] <= until else "walking"] += 1
                out["chain_dead"] += self.died(guid, hit_at[guid], CHAIN_LIGHTNING)
            cone = next((rec["t"] for rec in self.cones
                         if rec["d"] == guid and CONE_LEAD_MS[0] <= rec["t"] - lit <= CONE_LEAD_MS[1]), None)
            if cone is not None:
                out["cone_hit"] += 1
                out["cone_dead"] += self.died(guid, cone, (LIGHTNING_CHARGE,))
            out["steps"] += wait["step"]
        return out


def show_chainwait(trace: Trace) -> None:
    print("CHAIN WAITS")
    if not mine(trace):
        return
    start = phase2_start(trace)
    if start is None:
        print("  phase 2 never started")
        return
    lits = lit_orbs(trace, start)
    if not lits:
        print("  no thorim.lightningorb probe or no lit orb in phase 2")
        return
    waits = ChainWaits(trace, start)
    first_tank = first_tank_death(trace, start)
    total = collections.Counter()
    lengths: list[int] = []
    for lit in lits:
        orb = waits.orb_of(lit)
        if orb not in CHAIN_WAIT_ORBS:
            continue
        read = waits.read(lit)
        n = len(read["waits"])
        early = first_tank is None or lit < first_tank
        lengths += read["lengths"]
        for key, value in (("lit", 1), ("bridged", len(read["camp_hops"]) >= CAMP_CHAIN_BRIDGE),
                           ("camp dead", read["camp_dead"]), ("waits", n), ("landed", read["landed"]),
                           ("standing", read["standing"]), ("walking", read["walking"]),
                           ("chain dead", read["chain_dead"]), ("cone hit", read["cone_hit"]),
                           ("cone dead", read["cone_dead"]), ("steps", read["steps"])):
            total[key] += value
            if early:
                total["early " + key] += value
        if not n and not read["camp_hops"]:
            continue
        hops = ", ".join(str(hop) for hop in read["camp_hops"]) or "-"
        waited = (f" (released by the landing {read['landed']}, by the cap {n - read['landed']}); waiters hit by the "
                  f"chain standing {read['standing']} / walking {read['walking']}, dead {read['chain_dead']}; by the "
                  f"cone {read['cone_hit']}, dead {read['cone_dead']}; steps {read['steps']}") if n else ""
        print(f"  +{(lit - start) / 1000.0:.1f} s orb {orb}: chain camp hops {hops}, {read['camp_dead']} dead; "
              f"waits {n}{waited}")
    print(f"  all: orbs 1-2 lit {total['lit']}, chains with {CAMP_CHAIN_BRIDGE}+ camp hits {total['bridged']}, camp "
          f"chain deaths {total['camp dead']} ({total['early camp dead']} before the first tank death); waits "
          f"{total['waits']} (released by the landing {total['landed']}, by the cap "
          f"{total['waits'] - total['landed']}), wait p50 {yd(quantile([v / 1000.0 for v in lengths], .5))} max "
          f"{yd(max(lengths) / 1000.0 if lengths else None)} s; waiters hit by the chain standing {total['standing']} / "
          f"walking {total['walking']}, dead {total['chain dead']}; by the cone {total['cone hit']}, dead "
          f"{total['cone dead']} ({total['early cone dead']} before the first tank death); steps after a wait "
          f"{total['steps']}")


def rows_before(trace: Trace, when: int) -> dict:
    last = None
    for snap in frames(trace):
        if snap["t"] > when:
            break
        last = snap
    return {row[0]: row for row in last.get("u", [])} if last else {}


def smash_healer_text(trace: Trace, guid, read: dict) -> str:
    landed = ", dead" if read["dead"] else (f", hit {read['hit'] / 1000:.0f}k" if read["hit"] else "")
    return (f"{trace.name(guid)} {yd(read['away'])} yd {read['off']:.0f} deg, {yd(read['from_target'])} yd off his "
            f"target{landed} ({read['mover']})")


def show_colossus(trace: Trace) -> None:
    print("COLOSSUS SMASH")
    if not mine(trace):
        return
    colossi = {guid for guid, entry in trace.entries.items() if entry == NPC_RUNIC_COLOSSUS}
    casts = [rec for rec in trace.of("cast") if rec.get("sp") == COLOSSUS_SMASH and rec.get("s") in colossi]
    if not casts:
        print("  no Smash cast")
        return
    healers = {guid for guid, role in trace.roles.items() if role == "heal"}
    hits = [rec for rec in trace.of("dmg") if rec.get("sp") == COLOSSUS_SMASH]
    deaths = [death for death in combat_deaths(trace) if blow_spell(death) == COLOSSUS_SMASH]
    moves = [rec for rec in trace.of("move") if rec.get("g") in healers and rec.get("ok")]
    total = collections.Counter()
    for cast in casts:
        rows = rows_before(trace, cast["t"])
        colossus = rows.get(cast["s"])
        if not colossus:
            continue
        target = colossus[7] if len(colossus) > 7 else 0
        target_row = rows.get(target)
        landed = {rec["d"]: rec["a"] for rec in hits if cast["t"] <= rec["t"] <= cast["t"] + SMASH_LAND_MS}
        dead = {death["g"] for death in deaths if cast["t"] <= death["t"] <= cast["t"] + SMASH_LAND_MS}
        reads = {}
        for guid in sorted(healers):
            row = rows.get(guid)
            if not row or row[5] <= 0 or dist2(row[1:3], colossus[1:3]) > SMASH_LOOK:
                continue
            last = [rec for rec in moves if rec["g"] == guid and rec["t"] <= cast["t"]]
            reads[guid] = {"away": dist2(row[1:3], colossus[1:3]),
                           "off": angle_off(bearing(colossus[1:3], row[1:3]), colossus[4]),
                           "from_target": dist2(row[1:3], target_row[1:3]) if target_row else None,
                           "hit": landed.get(guid), "dead": guid in dead,
                           "mover": (last[-1].get("by") or "-") if last else "-"}
        total["casts"] += 1
        total["near"] += len(reads)
        total["on target"] += sum(1 for read in reads.values()
                                  if read["from_target"] is not None and read["from_target"] <= SMASH_ON_TARGET)
        total["hit"] += sum(1 for read in reads.values() if read["hit"])
        total["dead"] += sum(read["dead"] for read in reads.values())
        on = f"on {trace.name(target)} ({trace.role(target)})" if target else "on nobody"
        print(f"  {clock(cast['t'])} {on}: " + ("; ".join(smash_healer_text(trace, guid, read)
                                                         for guid, read in reads.items()) or "no healer in reach"))
    print(f"  all: casts {total['casts']}, healers within {SMASH_LOOK:.0f} yd {total['near']}, within "
          f"{SMASH_ON_TARGET:.0f} yd of his target {total['on target']}, hit {total['hit']}, dead {total['dead']}")


SECTIONS = (
    ("deaths", "deaths by mechanic, phase 1 and phase 2 apart", show_deaths),
    ("phase2", "the phase 2 measures the tank spots are judged on", show_phase2),
    ("blizzard", "each Sif Blizzard against the melee ring: who stood in it, ticks, steps, the walk back",
     show_blizzard),
    ("chains", "each Chain Lightning hop by hop: the groups it crossed, how far, who died after which crossing",
     show_chains),
    ("shelters", "each Lightning Charge against the camp: who walked to a shelter row or a cone step, who got "
     "there, who the cone hit and what slowed them", show_shelters),
    ("ringcone", "each Lightning Charge against the melee ring: who it hit and what they were doing, and the ring "
     "walks held for crossing the lit cone", show_ringcone),
    ("colossus", "each Runic Colossus Smash against the gauntlet healers: how far off his facing and his target "
     "each stood, the move that put it there, who it hit", show_colossus),
    ("chainwait", "each orb 1 or 2 against the camp: shelter walks held for Chain Lightning, how they were released, "
     "the chain's camp hops, and what the chain and the cone did to the waiters", show_chainwait),
)
SUMMARY = ("deaths", "phase2")


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner, SUMMARY)


if __name__ == "__main__":
    sys.exit(main())
