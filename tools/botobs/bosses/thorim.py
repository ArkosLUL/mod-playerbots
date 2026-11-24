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
                                 stood in it and how long, ticks taken, step-outs and the walk back
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
from raidobs.geometry import anchor, dist2, frames, radius  # noqa: E402
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
RING_RADIUS = radius("ULDUAR_THORIM_MELEE_RING_RADIUS")
RING_BLIZZARD_CLEARANCE = radius("ULDUAR_THORIM_RING_BLIZZARD_CLEARANCE")


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
        rows = [rec for rec in self.trace.of("dmg")
                if rec.get("sp") in CHAIN_LIGHTNING and rec["t"] >= self.start and rec.get("d") in self.players]
        casts: list[list[dict]] = []
        for rec in rows:
            if casts and rec["t"] - casts[-1][-1]["t"] <= CAST_GAP_MS:
                casts[-1].append(rec)
            else:
                casts.append([rec])
        return casts

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
        self.casts = self._casts()

    def _casts(self) -> list[dict]:
        bunnies = {guid for guid, entry in self.trace.entries.items() if entry == NPC_SIF_BLIZZARD}
        seen: dict = {}
        for snap in frames(self.trace):
            if snap["t"] < self.start:
                continue
            for row in snap.get("u", []):
                if row[0] in bunnies:
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
        steps = sum(1 for rec in self.trace.of("move")
                    if begin <= rec["t"] < end and rec.get("ok") and rec.get("by") == BLIZZARD_MOVER
                    and rec.get("g") in self.ring)
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
                "steps": steps, "deaths": deaths, "stepped_ms": stepped_ms, "out_ms": out_ms, "walks": walks,
                "walk_ticks": walk_ticks}

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
        steps = (f"steps {read['steps']}, stepped {read['stepped_ms'] / 1000.0:.1f} bot-s, out of reach "
                 f"{read['out_ms'] / 1000.0:.1f} of them; walk back p50 {yd(quantile(walks, .5))} "
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
             "damage": collections.Counter(), "steps": 0, "deaths": 0, "stepped_ms": 0, "out_ms": 0,
             "walks": [], "walk_ticks": 0}
    for index, cast in enumerate(blizzard.casts, 1):
        read = blizzard.read(cast)
        label = (f"blizzard {index} +{(cast['begin'] - start) / 1000.0:.1f}..+{(cast['end'] - start) / 1000.0:.1f} s")
        print(f"  {ring_blizzard_line(blizzard, label, read)}")
        for key in ("covered", "in_zone", "ticks", "damage"):
            total[key].update(read[key])
        for guid, stays in read["stays"].items():
            total["stays"][guid] += stays
        for key in ("steps", "deaths", "stepped_ms", "out_ms", "walk_ticks"):
            total[key] += read[key]
        total["walks"] += read["walks"]
    stays = [stay / 1000.0 for values in total["stays"].values() for stay in values]
    print(f"  all: ring melee in a zone {sum(total['in_zone'].values()) / 1000.0:.1f} bot-s, stays p50 "
          f"{yd(quantile(stays, .5))} max {yd(max(stays) if stays else None)} s, "
          f"{sum(total['ticks'].values())} ticks {sum(total['damage'].values()) // 1000}k, deaths {total['deaths']}"
          + (f"; steps {total['steps']}, stepped {total['stepped_ms'] / 1000.0:.1f} bot-s, out of reach "
             f"{total['out_ms'] / 1000.0:.1f}; walk back p50 {yd(quantile(total['walks'], .5))} max "
             f"{yd(max(total['walks']) if total['walks'] else None)} s, {total['walk_ticks']} ticks on the way"
             if blizzard.probed else ""))


SECTIONS = (
    ("deaths", "deaths by mechanic, phase 1 and phase 2 apart", show_deaths),
    ("phase2", "the phase 2 measures the tank spots are judged on", show_phase2),
    ("blizzard", "each Sif Blizzard against the melee ring: who stood in it, ticks, steps, the walk back",
     show_blizzard),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
