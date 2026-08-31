#!/usr/bin/env python3
"""Read a RaidObs trace and explain what happened.

A trace is NDJSON: one record per line, `t` in milliseconds relative to the pull (negative during the
pre-roll). Schema and field meanings live in docs/systems/observability.md.

    postmortem.py <file>                 session summary + one block per death
    postmortem.py <file> --death N       full rewind for one death
    postmortem.py <file> --bot NAME      that bot's timeline
    postmortem.py <file> --track NAME    position track, with distance to each boss
    postmortem.py <file> --notes [KEY]   pull/phase/note/end records, optionally one key prefix
    postmortem.py <file> --stalls [MS]   held still while still asking to move - i.e. stuck
    postmortem.py <file> --clump [YARDS] how stacked the raid was, largest group in one circle
"""
from __future__ import annotations

import argparse
import json
import math
import pathlib
import sys
from collections import defaultdict

SUPPORTED_SCHEMA = 8

# Old traces stay readable: every addition through v6 is a new field or a new record, so an older file
# only loses the detail those carry. v7 gave an existing column a -1 sentinel, but what it replaces was
# nonsense in older files too, so one render serves both. v8 appends to the end of a snapshot row and
# adds an optional cast field, so a pre-v8 row is just a short one.
READABLE_SCHEMAS = (4, 5, 6, 7, 8)


def clock(ms: int) -> str:
    sign = "-" if ms < 0 else ""
    ms = abs(int(ms))
    return f"{sign}{ms // 60000:d}:{(ms % 60000) / 1000:06.3f}"


class Trace:
    def __init__(self, path: pathlib.Path):
        self.path = path
        self.header: dict = {}
        self.records: list[dict] = []
        self.names: dict[int, str] = {}
        # creature_template entry per guid. Names repeat across unrelated creatures and change with
        # locale, so anything keying off "which creature is this" wants the entry instead.
        self.entries: dict[int, int] = {}
        self.spells: dict[int, str] = {}
        self.roles: dict[int, str] = {}
        self.humans: set[int] = set()
        self.bosses: set[int] = set()
        self.truncated = False
        self._load()

    def _load(self) -> None:
        bad = 0
        with self.path.open("r", encoding="utf-8", errors="replace") as fh:
            for line in fh:
                line = line.strip()
                if not line:
                    continue
                try:
                    rec = json.loads(line)
                except json.JSONDecodeError:
                    # A trace truncated by a crash or the size cap ends mid-line; everything before it
                    # is still good, so keep it rather than failing the whole read.
                    bad += 1
                    continue

                if rec.get("e") == "hdr":
                    self.header = rec
                    for member in rec.get("roster", []):
                        self.names[member["g"]] = member["n"]
                        self.roles[member["g"]] = member.get("r", "?")
                        if member.get("h"):
                            self.humans.add(member["g"])
                    continue

                if rec.get("e") == "unit":
                    self.names[rec["g"]] = rec.get("n", "?")
                    if rec.get("en"):
                        self.entries[rec["g"]] = rec["en"]
                    # Players carry a role; a creature does not. Somebody who zoned in after the
                    # header was written only ever appears here.
                    if "r" in rec:
                        self.roles[rec["g"]] = rec["r"]
                    if rec.get("h"):
                        self.humans.add(rec["g"])
                    if rec.get("b"):
                        self.bosses.add(rec["g"])
                    continue

                if rec.get("e") == "spell":
                    self.spells[rec["sp"]] = rec.get("n", "?")
                    continue

                if rec.get("e") == "truncated":
                    self.truncated = True
                    continue

                self.records.append(rec)

        # Verdicts are written when the engine pass that produced them ends, so their line order can
        # trail their own timestamps by a tick. Sorting once here keeps every view chronological.
        self.records.sort(key=lambda r: r.get("t", 0))

        if bad:
            print(f"note: skipped {bad} unparsable line(s) (trace likely cut short)", file=sys.stderr)

        version = self.header.get("v")
        if version is not None and version not in READABLE_SCHEMAS:
            print(
                f"warning: trace schema v{version}, this tool understands v{SUPPORTED_SCHEMA}",
                file=sys.stderr,
            )

    def name(self, guid) -> str:
        if not guid:
            return "-"
        if guid in self.names:
            return self.names[guid]
        # A guid is a type tag in the high half plus the counter. Printing the bare counter would make
        # a creature and a player that share one look like the same unit.
        return f"#{guid >> 32}:{guid & 0xFFFFFFFF}"

    def spell(self, spell_id) -> str:
        """A spell as a reader recognises it. v4 traces carry no names, so those stay bare numbers."""
        if not spell_id:
            return "melee"
        name = self.spells.get(spell_id)
        return f"{name} {spell_id}" if name else f"spell {spell_id}"

    def of(self, *events: str) -> list[dict]:
        return [r for r in self.records if r.get("e") in events]

    def guid_by_name(self, needle: str) -> int | None:
        needle = needle.lower()
        for guid, name in self.names.items():
            if name.lower() == needle:
                return guid
        for guid, name in self.names.items():
            if needle in name.lower():
                return guid
        return None


# Unit::Kill strips a dying unit's auras through the same hook the recorder listens on, so an aura
# held right up to the death is stamped with the death's own timestamp. Anything inside this window
# was still on when the bot fell, not something that had worn off.
STRIP_AT_DEATH_MS = 250


def is_debuff(row: list) -> bool:
    """Whether this hurt.

    v5 carries the answer as a 7th column, straight off the spell. v4 has to guess from the caster's
    guid tag, which is wrong both ways - totems and pets buff from a creature guid, and Biting Cold is
    applied to the player by the player - so it is only the fallback.
    """
    if len(row) >= 7:
        return not row[6]

    caster = row[3]
    return not caster or (caster >> 32) != 0


def aura_line(trace: Trace, row: list, death_t: int) -> str:
    spell, stacks, duration, caster, applied, removed = row[:6]

    # -1 means the recorder never saw the apply, which is every raid buff cast before the pull. Older
    # traces wrote -startMs there instead; both are unknown, and a made-up duration is worse than none.
    held = f"{'?':>6}" if applied < 0 else f"{(death_t - applied) / 1000:5.1f}s"
    left = "-" if duration < 0 else f"{duration / 1000:.1f}s left"
    if removed < 0 or death_t - removed <= STRIP_AT_DEATH_MS:
        gone = ""
    else:
        gone = f", fell off {(death_t - removed) / 1000:.1f}s before"
    return f"{trace.spell(spell)} x{stacks:<3} {left:>10}  held {held}  from {trace.name(caster)}{gone}"


def held_to_death(row: list, death_t: int) -> bool:
    removed = row[5]
    return removed < 0 or death_t - removed <= STRIP_AT_DEATH_MS


def act_row(row: list) -> tuple:
    """One `death.acts` row as (firstT, lastT, action, relevance, verdict, repeats).

    v3 wrote one row per verdict with no span and no repeat count; reading both shapes is what lets a
    v4 run be compared against the trace that motivated it.
    """
    if len(row) >= 6:
        return tuple(row[:6])

    first, action, relevance, verdict = row[:4]
    return first, first, action, relevance, verdict, 1


# How close an impact marker or a hazard creature has to be to count as "on top of the bot". A
# zero-radius hazard row is a point, not an area, so containment cannot be tested against it.
MARKER_PROXIMITY_YD = 5.0


def snapshot_before(trace: Trace, when: int) -> dict | None:
    sample = None
    for snap in trace.of("snap"):
        if snap["t"] > when:
            break
        sample = snap
    return sample


def hazards_at(trace: Trace, death: dict) -> tuple[list, list, list, list]:
    """What was on the spot the bot died on, from the last sample before it.

    Four answers, because one rule does not cover them. `covering` is the classic containment test.
    `markers` exists because the boss dynobjects that matter carry no radius at all - Hodir's three
    Icicle spells have a zero-radius DBC row, so CalcRadius has nothing to return - which leaves the
    row a point saying where something landed. `units` is the sweep, and on Hodir it is the real
    answer: the thing that kills is a creature, not a dynobject. `friendly` is the zones the bot was
    inside, because "not standing in the fire that sheds the stacks" is a diagnosis too.
    """
    sample = snapshot_before(trace, death["t"])
    if not sample:
        return [], [], [], []

    where = (death.get("x"), death.get("y"))
    covering, markers, friendly = [], [], []
    for row in sample.get("hz", []):
        if len(row) < 6:
            continue

        spell, hx, hy, _hz, radius, foe = row[:6]
        gap = math.dist(where, (hx, hy))

        if not foe:
            if radius and gap <= radius:
                friendly.append((spell, gap, radius))
            continue

        if radius:
            if gap <= radius:
                covering.append((spell, gap, radius))
        elif gap <= MARKER_PROXIMITY_YD:
            markers.append((spell, gap, radius))

    roster = set(trace.roles)
    units = []
    for row in sample.get("u", []):
        guid = row[0]
        if guid in roster or guid in trace.humans:
            continue

        gap = math.dist(where, (row[1], row[2]))
        if gap <= MARKER_PROXIMITY_YD:
            units.append((guid, gap))

    for group in (covering, markers, friendly, units):
        group.sort(key=lambda h: h[1])

    return covering, markers, units, friendly


# The only two reasons that mean the movement layer turned a destination down. "dup" and "wait" are the
# ordinary state while a bot walks to a destination its action re-offers every tick, and "there" means
# it is already standing on it.
REFUSALS = {"blocked", "nopath"}


def note_text(trace: Trace, rec: dict) -> str:
    """A note's payload is free text, so a guid written into one arrives as a bare number.

    Joined here rather than at the recorder because the unit records are already in the file and only
    the join is missing, the same way every other guid in the schema is resolved on read. Substitutes
    only when the whole payload is a guid the trace knows, so counters like hodir.slot and coordinate
    strings like hodir.anchor are left alone.
    """
    txt = str(rec.get("txt", ""))
    body = txt.strip()
    if body.isdigit() and int(body) in trace.names:
        return trace.name(int(body))
    return txt


def move_line(trace: Trace, rec: dict) -> str:
    reason = rec.get("r", "")
    if not reason:
        status = "ok"
    elif reason in REFUSALS:
        status = f"REFUSED: {reason}"
    else:
        status = reason

    # The gate yields only to a strictly higher priority, so a "wait" is a contest and the holder is
    # the other half of it. Absent before v6, and absent on Follow and Chase, which never face the gate.
    priority = rec.get("pr")
    if priority:
        status = f"{status}, {priority}"
    elif priority == "":
        status = f"{status}, no priority"

    holder = rec.get("hpr")
    if holder:
        status = f"{status}, held by {holder} {rec.get('hms', 0) / 1000:.1f}s"

    where = f"({rec['x']}, {rec['y']}, {rec['z']})"
    target = rec.get("tgt")
    if target:
        where = f"{trace.name(target)} at {where}"

    return f"{rec.get('k', 'point'):<6} -> {where} by '{rec['by']}' [{status}]"


def summarise(trace: Trace) -> None:
    hdr = trace.header
    print(f"trace   {trace.path.name}")
    print(f"boss    {hdr.get('boss', '?')}  map {hdr.get('map')} instance {hdr.get('inst')} diff {hdr.get('diff')}")

    roster = hdr.get("roster", [])
    by_role: dict[str, int] = defaultdict(int)
    for member in roster:
        by_role[member.get("r", "?")] += 1
    comp = " ".join(f"{n}x{role}" for role, n in sorted(by_role.items()))
    print(f"raid    {len(roster)} members ({comp}), {len(trace.humans)} human")

    pulls = trace.of("pull")
    ends = trace.of("end")
    if pulls:
        print(f"pull    {pulls[0].get('boss')} via {pulls[0].get('src')}")
    if ends:
        print(f"outcome {ends[0].get('out')} at {clock(ends[0]['t'])}")
    else:
        print("outcome (no end record - trace cut short)")

    snaps = trace.of("snap")
    if snaps:
        print(f"span    {clock(snaps[0]['t'])} .. {clock(snaps[-1]['t'])}  ({len(snaps)} snapshots)")

    if trace.truncated:
        print("        *** trace hit the size cap and stopped early ***")

    deaths = trace.of("death")
    print(f"deaths  {len(deaths)}")
    print()

    for index, death in enumerate(deaths):
        death_block(trace, death, index, brief=True)


def damage_summary(trace: Trace, rewind: list[list]) -> list[tuple[str, int, int]]:
    totals: dict[tuple[int, int], list[int]] = defaultdict(lambda: [0, 0])
    for _, source, spell, amount in rewind:
        entry = totals[(source, spell)]
        entry[0] += amount
        entry[1] += 1

    rows = []
    for (source, spell), (amount, hits) in totals.items():
        rows.append((f"{trace.name(source)} {trace.spell(spell)}", amount, hits))

    rows.sort(key=lambda r: -r[1])
    return rows


def death_block(trace: Trace, death: dict, index: int, brief: bool) -> None:
    guid = death["g"]
    print(f"[{index}] {trace.name(guid)} ({trace.roles.get(guid, '?')}) died at {clock(death['t'])}")
    print(f"     killed by {trace.name(death.get('killer'))}")
    print(f"     at ({death.get('x')}, {death.get('y')}, {death.get('z')})")

    # First, because it is the answer far more often than anything else in the block: the core strips
    # a dying unit's auras before the death hook, so v3 traces could not show this at all. Held-to-death
    # ahead of worn-off, then most recent first - the one that killed the bot is rarely the oldest.
    auras = death.get("auras") or []
    debuffs = sorted(
        (a for a in auras if len(a) >= 6 and is_debuff(a)),
        key=lambda a: (not held_to_death(a, death["t"]), -a[4]),
    )
    if debuffs:
        shown = debuffs[: 5 if brief else 40]
        count = f"{len(debuffs)}" if len(shown) == len(debuffs) else f"{len(shown)} of {len(debuffs)} shown"
        print(f"     debuffs ({count}):")
        for row in shown:
            print(f"       {aura_line(trace, row, death['t'])}")

    covering, markers, units, friendly = hazards_at(trace, death)
    if covering:
        where = ", ".join(f"{trace.spell(sp)} ({gap:.1f} of {rad:.1f} yd)" for sp, gap, rad in covering)
        print(f"     STOOD IN {where}")
    if markers:
        where = ", ".join(f"{trace.spell(sp)} at {gap:.1f} yd" for sp, gap, _rad in markers)
        print(f"     NEAR     {where}")
    if units:
        where = ", ".join(f"{trace.name(g)} at {gap:.1f} yd" for g, gap in units)
        print(f"     ON TOP   {where}")
    if friendly and not brief:
        where = ", ".join(f"{trace.spell(sp)} ({gap:.1f} of {rad:.1f} yd)" for sp, gap, rad in friendly)
        print(f"     inside   {where}")

    dist = death.get("dist") or {}
    if dist:
        near = " ".join(f"{trace.name(int(g))}={d}" for g, d in sorted(dist.items(), key=lambda kv: kv[1]))
        print(f"     range    {near}")

    # The rewind holds the last 15 seconds and nothing older, so an empty one is a statement rather
    # than a gap - and it is exactly the case the killing blow below is there to answer.
    rewind = death.get("rewind") or []
    if rewind:
        # Sorted here rather than trusted: v5 writes the ring chronologically, v4 wrote it by amount,
        # and the question this line answers is what landed last.
        for when, source, spell, amount in sorted(rewind, key=lambda row: row[0])[-3:]:
            print(f"     hit at {clock(when)}  {amount:>8}  {trace.name(source)} {trace.spell(spell)}")

        rows = damage_summary(trace, rewind)
        total = sum(r[1] for r in rows)
        print(f"     took {total} over {len(rewind)} hits:")
        for label, amount, hits in rows[: 3 if brief else 20]:
            share = 100.0 * amount / total if total else 0.0
            print(f"       {amount:>8}  {share:5.1f}%  {hits:>3}x  {label}")
    else:
        print("     took no damage in the rewind window")

    blow = death.get("blow")
    hplast = death.get("hplast")
    if blow or hplast:
        parts = []
        if hplast:
            parts.append(f"{hplast[0]}% at {clock(hplast[1])}")
        parts.append(f"final blow {blow[1]} from {trace.name(blow[0])}" if blow else "no final blow recorded")
        print(f"     health   {', '.join(parts)}")

    move = death.get("lastmove")
    if move:
        state = "arrived" if move.get("arrived") else "STILL WALKING"
        print(f"     moving  -> ({move['x']}, {move['y']}, {move['z']}) by '{move['by']}' [{state}]")

    # One row per verdict per distinct engine pass, carrying how many passes running produced it.
    acts = [act_row(row) for row in death.get("acts") or []]
    if acts:
        last = acts[-1]
        print(f"     doing   '{last[2]}' rel {last[3]} -> {last[4]}")
        if not brief:
            print(f"     last {len(acts)} verdict rows:")
            for first, last_ms, action, rel, verdict, repeats in acts:
                span = clock(first) if repeats == 1 else f"{clock(first)}..{clock(last_ms)}"
                run = "" if repeats == 1 else f" x{repeats}"
                print(f"       {span:>21}  {verdict:<14}{run:<6} rel {rel:<8} {action}")

    buffs = [a for a in auras if len(a) >= 6 and not is_debuff(a)]
    if buffs and not brief:
        print(f"     buffs held: {', '.join(f'{trace.spell(row[0])} x{row[1]}' for row in buffs)}")

    print()


def show_death(trace: Trace, index: int) -> int:
    deaths = trace.of("death")
    if index < 0 or index >= len(deaths):
        print(f"no death {index} (trace has {len(deaths)})", file=sys.stderr)
        return 1

    death_block(trace, deaths[index], index, brief=False)

    # Vetoes rarely coincide with a death by accident: they are the mechanism by which a bot that
    # "should have moved" did not.
    guid = deaths[index]["g"]
    start = deaths[index]["t"] - 10000
    vetoes = [r for r in trace.of("veto") if r["g"] == guid and start <= r["t"] <= deaths[index]["t"]]
    if vetoes:
        print("     vetoes in the last 10s:")
        for veto in vetoes:
            print(f"       {clock(veto['t'])}  {veto['m']} killed {veto['a']}")
        print()

    return 0


def show_bot(trace: Trace, name: str) -> int:
    guid = trace.guid_by_name(name)
    if guid is None:
        print(f"no unit matching '{name}'", file=sys.stderr)
        return 1

    print(f"timeline for {trace.name(guid)} ({trace.roles.get(guid, '?')})")
    for rec in trace.records:
        event = rec.get("e")
        stamp = clock(rec["t"])
        if event == "act" and rec.get("g") == guid:
            print(f"{stamp}  act    {rec['vd']:<10} rel {rec['rel']:<8} {rec['a']}")
        elif event == "veto" and rec.get("g") == guid:
            print(f"{stamp}  VETO   {rec['m']} killed {rec['a']}")
        elif event == "move" and rec.get("g") == guid:
            print(f"{stamp}  move   {move_line(trace, rec)}")
        elif event == "note" and rec.get("g") == guid:
            print(f"{stamp}  note   {rec['k']} = {note_text(trace, rec)}")
        elif event == "dmg" and rec.get("d") == guid:
            print(f"{stamp}  dmg    {rec['a']:>7} from {trace.name(rec['s'])} {trace.spell(rec['sp'])} -> {rec['hp']}%")
        elif event == "heal" and rec.get("d") == guid:
            print(f"{stamp}  heal   {rec['a']:>7} from {trace.name(rec['s'])} {trace.spell(rec['sp'])} -> {rec['hp']}%")
        elif event == "abs" and rec.get("d") == guid:
            print(f"{stamp}  absorb {rec['a']:>7} by {trace.name(rec['s'])} shield {trace.spell(rec['sp'])}")
        elif event == "aura" and rec.get("d") == guid:
            verb = "lost" if rec.get("r") else "got"
            stacks = f" x{rec['st']}" if "st" in rec else ""
            print(f"{stamp}  aura   {verb} {trace.spell(rec['sp'])}{stacks} from {trace.name(rec['s'])}")
        elif event == "death" and rec.get("g") == guid:
            print(f"{stamp}  DIED   killed by {trace.name(rec.get('killer'))}")

    return 0


def show_track(trace: Trace, name: str) -> int:
    guid = trace.guid_by_name(name)
    if guid is None:
        print(f"no unit matching '{name}'", file=sys.stderr)
        return 1

    print(f"track for {trace.name(guid)}  (step = distance moved since previous sample)")
    header = "time      x        y        z       hp    step   " + "  ".join(
        f"d:{trace.name(b)[:10]}" for b in sorted(trace.bosses)
    )
    print(header)

    previous = None
    for snap in trace.of("snap"):
        rows = {row[0]: row for row in snap.get("u", [])}
        row = rows.get(guid)
        if not row:
            continue

        _, x, y, z, _o, hp = row[0], row[1], row[2], row[3], row[4], row[5]
        step = 0.0 if previous is None else math.dist((x, y, z), previous)
        previous = (x, y, z)

        dists = []
        for boss in sorted(trace.bosses):
            brow = rows.get(boss)
            dists.append(f"{math.dist((x, y, z), (brow[1], brow[2], brow[3])):8.1f}" if brow else "       -")

        print(f"{clock(snap['t']):>9} {x:8.1f} {y:8.1f} {z:7.1f} {hp:5.1f} {step:6.2f}  " + " ".join(dists))

    return 0


def show_notes(trace: Trace, prefix: str | None = None) -> int:
    for rec in trace.records:
        event = rec.get("e")
        # A note key is prefixed with the encounter that owns it, so one raid's latches can be read
        # without the others - Iron Assembly legitimately spans three bosses under one slug.
        if prefix and event == "note" and not rec.get("k", "").startswith(prefix):
            continue

        if event in ("pull", "end"):
            detail = rec.get("boss") or rec.get("out")
            print(f"{clock(rec['t']):>9}  {event.upper():<6} {detail}")
        elif event == "note":
            print(
                f"{clock(rec['t']):>9}  note   {trace.name(rec.get('g')):<16} {rec['k']} = {note_text(trace, rec)}"
            )
        elif event == "haz":
            shape = rec.get("shape", "?")
            # Shape fields differ per shape, so print whatever the probe attached rather than a
            # fixed set: "side" for a lane, "rad" for a circle, "lead"/"sweep"/"rate" for a sweep.
            extra = " ".join(
                f"{k}={v}"
                for k, v in rec.items()
                if k not in ("t", "e", "sp", "shape", "x", "y", "z", "ttl")
            )
            place = f"({rec.get('x')}, {rec.get('y')}, {rec.get('z')})"
            print(f"{clock(rec['t']):>9}  haz    {trace.spell(rec['sp'])} {shape} at {place} ttl {rec.get('ttl')} {extra}")

    return 0


ANSWERED_MOVE_YD = 5.0


def roster_guids(trace: Trace) -> set:
    return {member["g"] for member in trace.header.get("roster", [])}


def position_runs(track: list, tol: float, min_ms: int) -> list:
    """Maximal windows in which the unit never left a `tol`-yard disc."""
    runs = []
    index, count = 0, len(track)
    while index < count:
        end = index + 1
        x0, y0 = track[index][1], track[index][2]
        while end < count and math.hypot(track[end][1] - x0, track[end][2] - y0) <= tol:
            end += 1
        span = track[end - 1][0] - track[index][0]
        if span >= min_ms:
            runs.append((track[index][0], track[end - 1][0], x0, y0))
        index = end if end > index + 1 else index + 1
    return runs


def show_stalls(trace: Trace, min_ms: int) -> int:
    """Ordered to move and didn't.

    A bot that has *arrived* is also motionless, so standing still is not the signal on its own. The
    discriminator is that a mover which parks returns before it ever calls MoveTo and so leaves no
    `move` record at all: a stationary window that still contains accepted moves is one where the
    engine asked for a walk and the unit ignored it. That is what a spline-less POINT generator looks
    like from outside - PointMovementGenerator::DoInitialize returns without launching one while the
    unit is in UNIT_STATE_NOT_MOVE, so `ok` means "MovePoint was called", never "the unit moved".
    """
    roster = roster_guids(trace)
    dead_at: dict = {}
    for rec in trace.of("death"):
        dead_at.setdefault(rec["g"], rec["t"])

    tracks = defaultdict(list)
    for snap in trace.of("snap"):
        if snap["t"] < 0:
            continue
        for row in snap.get("u", []):
            if row[0] in roster:
                tracks[row[0]].append((snap["t"], row[1], row[2], row[5]))

    moves = defaultdict(list)
    for rec in trace.of("move"):
        if rec.get("ok"):
            moves[rec.get("g")].append(rec)

    print(f"stationary windows of at least {min_ms / 1000:.0f}s that still contain accepted moves\n")
    total = 0
    for guid, track in sorted(tracks.items(), key=lambda kv: trace.name(kv[0])):
        for start, end, x0, y0 in position_runs(track, 1.0, min_ms):
            if guid in dead_at and end > dead_at[guid]:
                continue
            issued = [m for m in moves[guid] if start <= m["t"] <= end]
            if not issued:
                continue
            # A goal only a few yards out is indistinguishable from having arrived: every mover has an
            # arrival deadband, and a walk that ends inside it looks identical to one never taken.
            furthest = max(math.hypot(m["x"] - x0, m["y"] - y0) for m in issued)
            if furthest <= ANSWERED_MOVE_YD:
                continue
            total += end - start
            owners = sorted({m.get("by") or "?" for m in issued})
            print(
                f"{trace.name(guid):<16} {clock(start):>9} -> {clock(end):>9}"
                f"  {(end - start) / 1000:6.1f}s at ({x0:7.1f},{y0:7.1f})"
                f"  {len(issued)} move(s) accepted, furthest goal {furthest:.0f} yd"
            )
            print(f"{'':16} {'':9}    {'':9}  wanted by: {', '.join(owners)}")

    print(f"\ntotal: {total / 1000:.0f}s")
    return 0


def show_clump(trace: Trace, radius: float) -> int:
    """How much of the pull had the raid stacked inside one AoE.

    Counts distinct *positions*, not bodies: passengers share their vehicle's coordinates exactly, so
    five riders in one siege engine are one thing an area spell can hit, not five.
    """
    roster = roster_guids(trace)
    dead_at: dict = {}
    for rec in trace.of("death"):
        dead_at.setdefault(rec["g"], rec["t"])

    histogram: dict = defaultdict(int)
    for snap in trace.of("snap"):
        if snap["t"] < 0:
            continue
        spots = {
            (round(row[1], 1), round(row[2], 1))
            for row in snap.get("u", [])
            if row[0] in roster and not (row[0] in dead_at and snap["t"] >= dead_at[row[0]])
        }
        if len(spots) < 2:
            continue
        biggest = max(
            sum(1 for x, y in spots if math.hypot(x - cx, y - cy) <= radius) for cx, cy in spots
        )
        histogram[biggest] += 1

    frames = sum(histogram.values())
    if not frames:
        print("no snapshots with two or more live positions")
        return 0

    print(f"most distinct positions inside one {radius:.0f} yd circle, per snapshot\n")
    running = 0
    for size in sorted(histogram, reverse=True):
        running += histogram[size]
        print(
            f"  {size:3d} together  {histogram[size]:6d} frames  {100 * histogram[size] / frames:5.1f}%"
            f"     >= {size}: {100 * running / frames:5.1f}% of the pull"
        )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Explain a RaidObs trace.")
    parser.add_argument("file", type=pathlib.Path)
    parser.add_argument("--death", type=int, help="full detail for one death, by index")
    parser.add_argument("--bot", help="timeline for one bot")
    parser.add_argument("--track", help="position track for one bot")
    parser.add_argument(
        "--notes",
        nargs="?",
        const="",
        metavar="KEY",
        help="pull/note/hazard/end records only; pass a key prefix such as hodir. to narrow it",
    )
    parser.add_argument(
        "--stalls",
        nargs="?",
        type=int,
        const=6000,
        metavar="MS",
        help="windows where a bot held station while still issuing accepted moves (default 6000ms)",
    )
    parser.add_argument(
        "--clump",
        nargs="?",
        type=float,
        const=10.0,
        metavar="YARDS",
        help="how stacked the raid was, as the largest group inside one circle (default 10 yd)",
    )
    args = parser.parse_args()

    if not args.file.is_file():
        print(f"no such trace: {args.file}", file=sys.stderr)
        return 1

    trace = Trace(args.file)

    if args.death is not None:
        return show_death(trace, args.death)
    if args.bot:
        return show_bot(trace, args.bot)
    if args.track:
        return show_track(trace, args.track)
    if args.notes is not None:
        return show_notes(trace, args.notes or None)
    if args.stalls is not None:
        return show_stalls(trace, args.stalls)
    if args.clump is not None:
        return show_clump(trace, args.clump)

    summarise(trace)
    return 0


if __name__ == "__main__":
    sys.exit(main())
