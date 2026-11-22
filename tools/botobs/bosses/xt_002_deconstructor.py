#!/usr/bin/env python3
"""Read an XT-002 pull from a RaidObs trace: Heart windows, add spawns per pile, Scrapyard trash pulls, deaths.

    xt_002_deconstructor.py <file>            every section
    xt_002_deconstructor.py <file> --heart    each Heart window and whether Heartbreak landed

What the generic views miss here:

- **A kill is not a hard mode kill.** Hard mode needs the Heart dead inside a window. If it survives
  all three, XT still dies to the damage it passes on, so the kill counts with `hardmode` on in the
  header and Heartbreak never landed.
- **Hunters can't shoot the Heart from a ring slot.** Its 22.5 yd combat reach puts their shot minimum at
  30.3 yd, so each window prints the hunters' damage on it against the ranged median.
- **Adds come from the pile, not from XT.** A pile only summons while XT stands outside 90 yd plus
  both combat reaches, so where the tank holds him decides which piles spawn. In the 25-man traces
  piles up to 105 yd off stayed quiet and 108 yd or more summoned, hence the distances per window.
- **The west piles sit inside the Scrapyard trash.** A Pummeller engaging there calls assistance
  (10 yd, centre to centre, 2 s delay) and its pack follows it onto the Pummeller's target.
- **A Life Spark that can't path to its victim is immune and never resets** in a raid map, while its
  Static Charged keeps hitting the whole raid. Sparks the raid can reach die in 5-17 s, so the sparks
  view lists the ones that lived longer, with the carrier they spawned on and the `xt002.evading` notes.
- **A tank carrier only walks out after Heartbreak, and only holding neither XT nor a Pummeller.** The
  sparks view lists each spark a tank dropped with its distance from the Searing Light spot.
- **Tank duties follow whoever XT is on**, since nothing can taunt him back. The tanks view lists who XT
  was on and how far that tank stood off the tank spot, each tank's `xt002.tank` duty notes, and the
  taunts still cast at him.
- **So do the healers' tank buffs.** The class nodes put Beacon, Sacred Shield, Earth Shield and the priest's
  shield and Renew on the group's main tank or, for a paladin, whichever tank dropped lowest. The tanks view
  prints the `xt002.buffs` tank they followed and, over the long holds, how much of the time each buff sat
  on the holder against the other tank.
- **A dps bot that pulls XT keeps him** until a tank's threat passes its own, so dps hold their damage on him
  near the holder's threat. The tanks view sums the time XT spent on non-tanks and the `xt002.brake` holds.
- **A Gravity Bomb carrier walks out past every healer's reach**, and the bomb's 14.5k (25-man) with the tantrum
  after it killed carriers at 36-85%. The nearest bot healer gets sent (`xt002.bombheal`); the bombs view lists
  each carrier that went out of range, who was sent, how close it got, its heals on the carrier and when it was
  back on its slot.
"""
from __future__ import annotations

import collections
import pathlib
import re
import statistics
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.geometry import anchor, dist2, frames, nearest, radius  # noqa: E402
from raidobs.probes import latch_spans, resolve_guids  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, notes  # noqa: E402

ENCOUNTER = "xt-002"

NPC_XT002 = 33293
NPC_HEART = 33329
NPC_SCRAPBOT = 33343
NPC_PUMMELLER = 33344
NPC_BOOMBOT = 33346
NPC_VOID_ZONE = 34001
NPC_LIFE_SPARK = 34004
ADDS = {NPC_SCRAPBOT: "Scrapbot", NPC_PUMMELLER: "Pummeller", NPC_BOOMBOT: "Boombot"}
# Parts Recovery Technician, XR-949 Salvagebot, XD-175 Compactobot, XB-488 Disposalbot, Salvagebot Sawblade
TRASH = {34267, 34269, 34271, 34273, 34288}

SPELL_HEART_PHASE = 37751
SPELL_STAND = 37752
HEARTBREAK = (64193, 65737)
SEARING_LIGHT = (63018, 65121)
GRAVITY_BOMB = (63024, 64234)
# Playerbots.HealDistance: the reach a bot healer heals from, and past it a carrier gets a healer sent out
HEAL_RANGE = 38.5
# a carrier dead this soon after its bomb came off, with the bomb's hit in its rewind, died to it; the tantrum
# often lands the last tick a couple of seconds later
BOMB_DEATH_MS = 5000
# the point at the end of an `xt002.slot` note, "<slot>/<of> x,y,z"
SLOT_POINT = re.compile(r"(-?\d+(?:\.\d+)?),(-?\d+(?:\.\d+)?),-?\d+(?:\.\d+)?$")
SEARING_LIGHT_SPOT = "ULDUAR_XT002_SEARING_LIGHT_SPOT"
TANK_SPOT = "ULDUAR_XT002_MAINTANK_SPOT"
CLASS_HUNTER = 3

# Taunt, Growl, Dark Command, Hand of Reckoning, Death Grip, Challenging Shout, Challenging Roar, Mocking Blow
TAUNTS = {355, 6795, 56222, 62124, 49576, 1161, 5209, 694}
# cast on the ally XT is hitting, not on XT
RIGHTEOUS_DEFENSE = 31789
# walking XT from where he turned to the tank spot takes a holder up to this long
SETTLE_MS = 15000
# The ranks a level-80 healer casts. Beacon, Sacred Shield and Earth Shield each have a heal or absorb proc
# under the same name, so a name match would count those.
TANK_BUFFS = {
    "Beacon of Light": {53563},
    "Sacred Shield": {53601},
    "Earth Shield": {49284},
    "Power Word: Shield": {48065, 48066},
    "Renew": {48067, 48068},
}

# 25-man hard mode traces: sparks the raid could hit died in 5-17 s, ones it couldn't lived 39-90 s
SPARK_LONG_MS = 20000
# a Searing Light removal this close to a spark's first sample is the one that dropped it
SPARK_CARRIER_MS = 2000
# bots still on an evading spark this long after the first note never let go of it
SPARK_DROP_MS = 2000

# core data/sql/updates/db_world/2026_07_04_03.sql
PILES = {
    "NE": (897.908, 67.0764), "SE": (898.099, -88.9115),
    "SW": (793.096, -95.158), "NW": (792.646, 65.3854),
}
ASSIST_RADIUS = 10.0
ASSIST_LOOKBACK_MS = 4000
# trash in one pack engages on the same tick, so anything inside this reads as one pull
PACK_WINDOW_MS = 1000


def mine(trace: Trace) -> bool:
    return encounter_of(trace) == ENCOUNTER


def entry(trace: Trace, guid) -> int | None:
    return trace.entries.get(guid)


def pile_of(point) -> str:
    return nearest(point, PILES)[0]


def xt_frames(trace: Trace):
    """(t, x, y) for XT, sample by sample."""
    out = []
    for snap in frames(trace):
        for row in snap.get("u", []):
            if entry(trace, row[0]) == NPC_XT002:
                out.append((snap["t"], row[1], row[2]))
    return out


def heartbreak_at(trace: Trace) -> int | None:
    for rec in trace.of("cast", "aura"):
        if rec.get("sp") in HEARTBREAK and not rec.get("r"):
            return rec["t"]
    return None


def heart_windows(trace: Trace) -> list[tuple[int, float, float]]:
    """(start, stop, lowest Heart hp %) per window, from XT's own submerge and stand casts.

    The window shuts on the stand, not on the next submerge: the hidden Heart still takes splash in between,
    and that hp is gone by the next window's full heal.
    """
    casts = [rec for rec in trace.of("cast") if entry(trace, rec.get("s")) == NPC_XT002]
    starts = [rec["t"] for rec in casts if rec.get("sp") == SPELL_HEART_PHASE]
    stands = [rec["t"] for rec in casts if rec.get("sp") == SPELL_STAND]
    out = []
    for index, start in enumerate(starts):
        stop = starts[index + 1] if index + 1 < len(starts) else float("inf")
        stop = min([t for t in stands if t > start] + [stop])
        low = 100.0
        for snap in frames(trace):
            if start <= snap["t"] < stop:
                for row in snap.get("u", []):
                    if entry(trace, row[0]) == NPC_HEART:
                        low = min(low, row[5])
        out.append((start, stop, low))
    return out


def heart_damage(trace: Trace, start: float, stop: float) -> collections.Counter:
    """Damage per roster member while it had the Heart targeted, pets folded in, from the `dealt` column."""
    roster = {member["g"] for member in trace.header.get("roster", [])}
    before, out = {}, collections.Counter()
    for snap in frames(trace):
        for row in snap.get("u", []):
            if row[0] not in roster or len(row) < 12:
                continue
            if row[0] in before and start <= snap["t"] <= stop and entry(trace, row[7]) == NPC_HEART:
                out[row[0]] += row[11] - before[row[0]]
            before[row[0]] = row[11]
    return out


def hunters_line(trace: Trace, start: float, stop: float) -> str | None:
    """Hunters' Heart damage against the other ranged, the check on whether they could shoot it."""
    damage = heart_damage(trace, start, stop)
    classes = {member["g"]: member.get("c") for member in trace.header.get("roster", [])}
    ranged = [guid for guid in classes if trace.role(guid) == "ranged"]
    hunters = sorted((damage[guid] for guid in ranged if classes[guid] == CLASS_HUNTER), reverse=True)
    others = [damage[guid] for guid in ranged if classes[guid] != CLASS_HUNTER]
    if not hunters or not others:
        return None
    shown = ", ".join(f"{value / 1000:.0f}k" for value in hunters)
    return f"    hunters on it {shown} against a ranged median {statistics.median(others) / 1000:.0f}k"


def show_banner(trace: Trace) -> None:
    print(f"XT-002  {trace.path.name}")
    if not mine(trace):
        print("  not an XT-002 pull, every section below reads empty")
        return
    ends = trace.of("end")
    out = ends[-1].get("out", "?") if ends else "?"
    broke = heartbreak_at(trace)
    mode = f"Heartbreak at {clock(broke)}" if broke is not None else "no Heartbreak, normal mode"
    print(f"  outcome {out}, {mode}")
    held = [f for f in xt_frames(trace) if f[0] >= 30000]
    if held:
        print(f"  XT at ({held[0][1]:.0f}, {held[0][2]:.0f}) from 0:30")


def pile_distances(spot) -> str:
    return " ".join(f"{name} {dist2(spot, pile):.0f}" for name, pile in PILES.items())


def show_heart(trace: Trace) -> None:
    print("HEART")
    if not mine(trace):
        return
    windows = heart_windows(trace)
    if not windows:
        print("  no heart phase")
    spots = xt_frames(trace)
    for start, stop, low in windows:
        line = f"  window at {clock(start)}  lowest {low:.0f}%"
        spot = next(((x, y) for t, x, y in spots if t >= start), None)
        if spot:
            line += f"  XT at ({spot[0]:.0f}, {spot[1]:.0f}), piles {pile_distances(spot)} yd"
        print(line)
        hunters = hunters_line(trace, start, stop)
        if hunters:
            print(hunters)
    broke = heartbreak_at(trace)
    print(f"  Heartbreak at {clock(broke)}" if broke is not None else "  Heart survived every window")


def first_rows(trace: Trace, entries) -> tuple[dict, dict]:
    """Per guid of these entries: first sample (t, x, y) and first sample holding a target."""
    seen, targeted = {}, {}
    for snap in frames(trace):
        for row in snap.get("u", []):
            if entry(trace, row[0]) not in entries:
                continue
            seen.setdefault(row[0], (snap["t"], row[1], row[2]))
            if row[7] and row[0] not in targeted:
                targeted[row[0]] = (snap["t"], row[1], row[2], row[7])
    return seen, targeted


def show_adds(trace: Trace) -> None:
    print("ADDS")
    if not mine(trace):
        return
    seen, _ = first_rows(trace, ADDS)
    per = collections.defaultdict(collections.Counter)
    first = {}
    for guid, (t, x, y) in seen.items():
        pile = pile_of((x, y))
        per[pile][ADDS[entry(trace, guid)]] += 1
        first[pile] = min(first.get(pile, t), t)
    if not per:
        print("  none seen")
    for pile in sorted(per):
        counts = ", ".join(f"{n} {name}" for name, n in sorted(per[pile].items()))
        print(f"  {pile} pile from {clock(first[pile])}: {counts}")


def trash_pulls(trace: Trace) -> list[dict]:
    """Each time Scrapyard trash engaged: when, how many, on whom, and the Pummeller that called it."""
    _, trash = first_rows(trace, TRASH)
    _, pummellers = first_rows(trace, {NPC_PUMMELLER})
    homes, _ = first_rows(trace, TRASH)
    pulls = []
    for guid, (t, x, y, target) in sorted(trash.items(), key=lambda item: item[1][0]):
        if pulls and t - pulls[-1]["t"] <= PACK_WINDOW_MS:
            pulls[-1]["n"] += 1
            continue
        caller = None
        for pguid, (pt, px, py, _) in pummellers.items():
            if not 0 <= t - pt <= ASSIST_LOOKBACK_MS:
                continue
            gap = min(dist2((px, py), (hx, hy)) for _, hx, hy in homes.values())
            if gap <= ASSIST_RADIUS + 2.0:
                caller = (pt, pile_of((px, py)), gap)
        pulls.append({"t": t, "n": 1, "target": target, "pile": pile_of((x, y)), "caller": caller})
    return pulls


def show_trash(trace: Trace) -> None:
    print("SCRAPYARD TRASH")
    if not mine(trace):
        return
    pulls = trash_pulls(trace)
    if not pulls:
        print("  never engaged")
    for pull in pulls:
        line = f"  {clock(pull['t'])}  {pull['n']} at the {pull['pile']} pile on {trace.name(pull['target'])}"
        if pull["caller"]:
            pt, pile, gap = pull["caller"]
            line += f", after a {pile} Pummeller engaged at {clock(pt)} {gap:.1f} yd from the pack"
        print(line)
    killed = sum(1 for d in combat_deaths(trace) if entry(trace, d.get("killer")) in TRASH)
    print(f"  deaths to trash: {killed}")


def sparks(trace: Trace) -> list[dict]:
    """Per Life Spark: first and last sample, spawn spot, the carrier it dropped from, and the most roster
    members targeting it in one snapshot. A spark leaves the snapshots when it dies."""
    roster = {member["g"] for member in trace.header.get("roster", [])}
    out = {}
    for snap in frames(trace):
        on = collections.Counter()
        for row in snap.get("u", []):
            if entry(trace, row[0]) == NPC_LIFE_SPARK and row[5] > 0:
                spark = out.setdefault(row[0], {"g": row[0], "first": snap["t"], "spot": (row[1], row[2]),
                                                "most": 0})
                spark["last"] = snap["t"]
            elif row[0] in roster and len(row) > 7 and entry(trace, row[7]) == NPC_LIFE_SPARK:
                on[row[7]] += 1
        for guid, count in on.items():
            if guid in out:
                out[guid]["most"] = max(out[guid]["most"], count)
    removals = [rec for rec in trace.of("aura") if rec.get("sp") in SEARING_LIGHT and rec.get("r")]
    for spark in out.values():
        near = [rec for rec in removals if abs(rec["t"] - spark["first"]) <= SPARK_CARRIER_MS]
        spark["carrier"] = min(near, key=lambda rec: abs(rec["t"] - spark["first"]))["d"] if near else None
    return sorted(out.values(), key=lambda spark: spark["first"])


def evading_from(trace: Trace) -> dict:
    """Spark guid -> (first `xt002.evading` note naming it, first later note from a bot that named it
    and no longer does or None, how many bots named it). The notes only change, so the first bot to
    drop the spark marks it hittable again."""
    first, until, bots = {}, {}, collections.defaultdict(set)
    named = collections.defaultdict(set)
    for rec in notes(trace, "xt002.evading"):
        now = {int(token) for token in rec.get("txt", "").split() if token.isdigit()}
        for guid in named[rec.get("g")] - now:
            until.setdefault(guid, rec["t"])
        for guid in now:
            first.setdefault(guid, rec["t"])
            bots[guid].add(rec.get("g"))
        named[rec.get("g")] = now
    return {guid: (t, until.get(guid), len(bots[guid])) for guid, t in first.items()}


def held_between(trace: Trace, guid, since: int, until) -> int:
    """Most roster members targeting this unit in one snapshot from `since` until `until` (None: the end).
    Bounded, because bots rightly go back to a spark once it can be hit."""
    roster = {member["g"] for member in trace.header.get("roster", [])}
    most = 0
    for snap in frames(trace):
        if snap["t"] >= since and (until is None or snap["t"] < until):
            most = max(most, sum(1 for row in snap.get("u", []) if row[0] in roster and len(row) > 7
                                 and row[7] == guid))
    return most


def show_sparks(trace: Trace) -> None:
    print("LIFE SPARKS")
    if not mine(trace):
        return
    rows = sparks(trace)
    if not rows:
        print("  none")
        return
    ends = trace.of("end")
    end = ends[-1]["t"] if ends else None
    peak = max(sum(1 for other in rows if other["first"] <= spark["first"] <= other["last"]) for spark in rows)
    lives = sorted(spark["last"] - spark["first"] for spark in rows)
    print(f"  {len(rows)} sparks, at most {peak} up at once, median life {statistics.median(lives) / 1000:.0f} s")
    evading = evading_from(trace)
    for spark in rows:
        life = spark["last"] - spark["first"]
        if life < SPARK_LONG_MS and spark["g"] not in evading:
            continue
        carrier = spark["carrier"]
        source = f"on {trace.name(carrier)} ({trace.role(carrier)})" if carrier else "on an unknown carrier"
        up = ", still up at the end" if end is not None and end - spark["last"] <= SPARK_CARRIER_MS else ""
        print(f"  {clock(spark['first'])} {source} at ({spark['spot'][0]:.0f}, {spark['spot'][1]:.0f}):"
              f" lived {life / 1000:.0f} s{up}, most bots on it at once {spark['most']}")
        if spark["g"] in evading:
            t, until, bots = evading[spark["g"]]
            held = held_between(trace, spark["g"], t + SPARK_DROP_MS, until)
            span = f"{clock(t)} to {clock(until)}" if until is not None else f"{clock(t)} on"
            print(f"    evading {span} (bots noting it {bots}),"
                  f" most bots still on it {SPARK_DROP_MS // 1000} s in {held}")
    spot = anchor(SEARING_LIGHT_SPOT)
    for spark in rows:
        carrier = spark["carrier"]
        if carrier and trace.role(carrier) == "tank":
            print(f"  tank carrier {trace.name(carrier)} at {clock(spark['first'])}: spark"
                  f" {dist2(spark['spot'], spot):.0f} yd off the Searing Light spot,"
                  f" lived {(spark['last'] - spark['first']) / 1000:.0f} s")


def holders(trace: Trace) -> list[dict]:
    """Runs of XT's victim, snapshot by snapshot: `t`, `g` (0 while he attacks nobody), `until`, and the
    victim's own spots during the run."""
    runs = []
    for snap in frames(trace):
        rows = {row[0]: row for row in snap.get("u", [])}
        xt = next((row for guid, row in rows.items() if entry(trace, guid) == NPC_XT002 and len(row) > 7), None)
        if not xt:
            continue
        if not runs or runs[-1]["g"] != xt[7]:
            runs.append({"t": snap["t"], "g": xt[7], "spots": []})
        runs[-1]["last"] = snap["t"]
        victim = rows.get(xt[7])
        if victim:
            runs[-1]["spots"].append((snap["t"], victim[1], victim[2]))
    for run, after in zip(runs, runs[1:] + [None]):
        run["until"] = after["t"] if after else run["last"]
    return runs


def taunts_at_xt(trace: Trace, runs: list[dict]) -> collections.Counter:
    """Taunts tanks cast at XT, per (tank, spell name). Righteous Defense counts when its ally is XT's victim."""
    xt = {guid for guid, en in trace.entries.items() if en == NPC_XT002}
    out = collections.Counter()
    for rec in trace.of("cast"):
        if trace.role(rec.get("s")) != "tank":
            continue
        spell = rec.get("sp")
        if spell == RIGHTEOUS_DEFENSE:
            victim = next((run["g"] for run in reversed(runs) if run["t"] <= rec["t"]), 0)
            hit = victim and rec.get("tgt") == victim
        else:
            hit = spell in TAUNTS and rec.get("tgt") in xt
        if hit:
            out[(rec["s"], trace.spells.get(spell) or f"spell {spell}")] += 1
    return out


def buff_spans(trace: Trace) -> dict[tuple[str, int], list[tuple[int, float]]]:
    """(buff name, tank guid) -> merged [(start, stop)] a healer's copy of that tank buff sat on that tank. Tracked
    per caster, since two priests' Renews come and go apart."""
    by_id = {sp: name for name, ids in TANK_BUFFS.items() for sp in ids}
    ends = trace.of("end")
    end = ends[-1]["t"] if ends else float("inf")
    raw, since = collections.defaultdict(list), {}
    for rec in trace.of("aura"):
        name = by_id.get(rec.get("sp"))
        if not name or trace.role(rec.get("d")) != "tank" or trace.role(rec.get("s")) != "heal":
            continue
        key = (name, rec["d"], rec["s"])
        if not rec.get("r"):
            since.setdefault(key, rec["t"])
        elif key in since:
            raw[key[:2]].append((since.pop(key), rec["t"]))
    for key, start in since.items():
        raw[key[:2]].append((start, end))
    spans = {}
    for key, pieces in raw.items():
        merged = []
        for start, stop in sorted(pieces):
            if merged and start <= merged[-1][1]:
                merged[-1] = (merged[-1][0], max(merged[-1][1], stop))
            else:
                merged.append((start, stop))
        spans[key] = merged
    return spans


def overlap(spans: list[tuple[int, float]], start: int, stop: int) -> float:
    return sum(max(0.0, min(b, stop) - max(a, start)) for a, b in spans)


def buffs_line(trace: Trace, runs: list[dict]) -> str | None:
    """For each healer tank buff cast in the pull, its share of the tank holds of SETTLE_MS or more spent on the
    holder, then on the other tank."""
    holds = [run for run in runs
             if run["g"] and trace.role(run["g"]) == "tank" and run["until"] - run["t"] >= SETTLE_MS]
    spans = buff_spans(trace)
    total = sum(run["until"] - run["t"] for run in holds)
    if not total or not spans:
        return None
    shares = []
    for name in TANK_BUFFS:
        if not any(buff == name for buff, _ in spans):
            continue
        on = off = 0.0
        for (buff, tank), tank_spans in spans.items():
            if buff != name:
                continue
            for run in holds:
                held = overlap(tank_spans, run["t"], run["until"])
                if tank == run["g"]:
                    on += held
                else:
                    off += held
        shares.append(f"{name} {100 * on / total:.0f}/{100 * off / total:.0f}%")
    return (f"  tank buffs over {total / 1000:.0f} s of holds of {SETTLE_MS // 1000} s or more, % on the holder/"
            f"the other tank: {', '.join(shares)}")


def brake_spans(trace: Trace) -> dict[int, list[tuple[int, int]]]:
    """Per dps bot, the (start, stop) spans its `xt002.brake` note held its damage off XT. One still on at the
    end runs to it."""
    ends = trace.of("end")
    since, spans = {}, collections.defaultdict(list)
    for rec in notes(trace, "xt002.brake"):
        guid = rec.get("g")
        if rec.get("txt") == "1":
            since.setdefault(guid, rec["t"])
        elif guid in since:
            spans[guid].append((since.pop(guid), rec["t"]))
    for guid, start in since.items():
        spans[guid].append((start, ends[-1]["t"] if ends else start))
    return spans


def brake_line(trace: Trace) -> str | None:
    spans = brake_spans(trace)
    if not spans:
        return None
    total = sum(stop - start for pieces in spans.values() for start, stop in pieces)
    guid, (start, stop) = max(((guid, piece) for guid, pieces in spans.items() for piece in pieces),
                              key=lambda item: item[1][1] - item[1][0])
    return (f"  threat brake: {len(spans)} dps bots held, {total / 1000:.0f} s in all, longest"
            f" {(stop - start) / 1000:.1f} s ({trace.name(guid)} from {clock(start)})")


def show_tanks(trace: Trace) -> None:
    print("TANKS")
    if not mine(trace):
        return
    runs = holders(trace)
    if not runs:
        print("  XT never sampled")
    spot = anchor(TANK_SPOT)
    for run in runs:
        who = f"{trace.name(run['g'])} ({trace.role(run['g'])})" if run["g"] else "nobody"
        line = f"  {clock(run['t'])}  {who} for {(run['until'] - run['t']) / 1000:.1f} s"
        settled = [dist2((x, y), spot) for t, x, y in run["spots"] if t >= run["t"] + SETTLE_MS]
        if run["g"] and trace.role(run["g"]) == "tank" and settled:
            line += f", {statistics.median(settled):.0f} yd off the tank spot from {SETTLE_MS // 1000} s in"
        print(line)
    stolen = [run for run in runs if run["g"] and trace.role(run["g"]) != "tank"]
    if runs:
        held = sum(run["until"] - run["t"] for run in stolen)
        print(f"  XT on a non-tank: {held / 1000:.1f} s over {len(stolen)} runs" if stolen
              else "  XT on a non-tank: never")
    duties = collections.defaultdict(list)
    for rec in notes(trace, "xt002.tank"):
        duties[rec.get("g")].append(f"{rec.get('txt')} {clock(rec['t'])}")
    for guid, changes in duties.items():
        print(f"  {trace.name(guid)}: {', '.join(changes)}")
    taunts = taunts_at_xt(trace, runs)
    shown = ", ".join(f"{trace.name(g)} {name} {n}" for (g, name), n in taunts.most_common())
    print(f"  taunts at XT: {sum(taunts.values())}" + (f" ({shown})" if shown else ""))
    brake = brake_line(trace)
    if brake:
        print(brake)
    moved = [f"{resolve_guids(trace, value)} {clock(start)}" for value, start, _ in latch_spans(trace, "xt002.buffs")]
    if moved:
        print(f"  healer buff tank: {', '.join(moved)}")
    line = buffs_line(trace, runs)
    if line:
        print(line)


def gravity_bombs(trace: Trace) -> list[dict]:
    """One row per Gravity Bomb on a roster member: `g` the carrier, `t` and `until` the aura's span."""
    roster = {member["g"] for member in trace.header.get("roster", [])}
    out, since = [], {}
    for rec in trace.of("aura"):
        if rec.get("sp") not in GRAVITY_BOMB or rec.get("d") not in roster:
            continue
        if not rec.get("r"):
            since.setdefault(rec["d"], rec["t"])
        elif rec["d"] in since:
            out.append({"g": rec["d"], "t": since.pop(rec["d"]), "until": rec["t"]})
    return sorted(out, key=lambda bomb: bomb["t"])


def sent_healers(trace: Trace) -> dict[int, list[tuple[int, int]]]:
    """Carrier guid -> [(t, healer guid)] from `xt002.bombheal`; healer 0 is the latch let go."""
    out = collections.defaultdict(list)
    for rec in notes(trace, "xt002.bombheal"):
        txt = str(rec.get("txt", ""))
        if txt.isdigit():
            out[rec.get("g")].append((rec["t"], int(txt)))
    return out


def last_slot(trace: Trace, guid, before: int):
    """The bot's `xt002.slot` point as last noted before `before`, or None."""
    spot = None
    for rec in notes(trace, "xt002.slot"):
        if rec.get("g") != guid or rec["t"] > before:
            continue
        found = SLOT_POINT.search(str(rec.get("txt", "")))
        if found:
            spot = (float(found.group(1)), float(found.group(2)))
    return spot


def bomb_row(trace: Trace, bomb: dict, healers: set, sent: dict, deaths: list[dict]) -> dict:
    """What one bomb did: when the carrier left every healer's reach, who was sent, how close that healer got,
    the heals it landed, the carrier's hp at expiry and whether the expiry killed it."""
    carrier, start, stop = bomb["g"], bomb["t"], bomb["until"]
    picks = [(t, healer) for t, healer in sent.get(carrier, []) if start <= t <= stop and healer]
    healer = picks[0][1] if picks else None
    row = {**bomb, "out": None, "healer": healer, "sent": picks[0][0] if picks else None, "closest": None,
           "at_expiry": None, "hp": None, "nearest": None, "heals": 0}
    for snap in frames(trace):
        if snap["t"] < start:
            continue
        rows = {unit[0]: unit for unit in snap.get("u", [])}
        spot = rows.get(carrier)
        if snap["t"] > stop or not spot:
            break
        gaps = {guid: dist2(spot[1:3], unit[1:3]) for guid, unit in rows.items()
                if guid in healers and unit[5] > 0}
        if carrier in healers:
            gaps[carrier] = 0.0
        if gaps and min(gaps.values()) > HEAL_RANGE and row["out"] is None:
            row["out"] = snap["t"]
        if healer in gaps and snap["t"] >= row["sent"]:
            row["closest"] = gaps[healer] if row["closest"] is None else min(row["closest"], gaps[healer])
            row["at_expiry"] = gaps[healer]
        row["hp"] = spot[5]
        row["nearest"] = min(gaps.values()) if gaps else None
    if healer:
        row["heals"] = sum(1 for rec in trace.of("heal")
                           if rec.get("s") == healer and rec.get("d") == carrier and start <= rec["t"] <= stop)
    # a carrier walking back through its own fresh puddle dies to Consumption, not to the bomb
    row["killed"] = any(death["g"] == carrier and stop - 500 <= death["t"] <= stop + BOMB_DEATH_MS
                        and entry(trace, death.get("killer")) != NPC_VOID_ZONE
                        and any(hit[2] in GRAVITY_BOMB for hit in death.get("rewind") or [])
                        for death in deaths)
    return row


def back_on_slot(trace: Trace, healer, since: int):
    """ms after `since` the healer first stood within the slot tolerance of its `xt002.slot` point, or None."""
    slot = last_slot(trace, healer, since)
    if not slot:
        return None
    tolerance = radius("ULDUAR_XT002_RANGED_SPOT_TOLERANCE")
    for snap in frames(trace):
        if snap["t"] < since:
            continue
        spot = next((unit for unit in snap.get("u", []) if unit[0] == healer), None)
        if spot and dist2(spot[1:3], slot) <= tolerance:
            return snap["t"] - since
    return None


def show_bombs(trace: Trace) -> None:
    print("GRAVITY BOMBS")
    if not mine(trace):
        return
    bombs = gravity_bombs(trace)
    if not bombs:
        print("  none")
        return
    healers = {member["g"] for member in trace.header.get("roster", []) if member.get("r") == "heal"}
    sent, deaths = sent_healers(trace), combat_deaths(trace)
    rows = [bomb_row(trace, bomb, healers, sent, deaths) for bomb in bombs]
    out = sum(1 for row in rows if row["out"] is not None)
    killed = sum(1 for row in rows if row["killed"])
    print(f"  {len(rows)} bombs, {out} carriers out of every healer's {HEAL_RANGE:g} yd, {killed} killed by their own")
    pull = radius("ULDUAR_XT002_GRAVITY_BOMB_PULL_RADIUS")
    for row in rows:
        if row["out"] is None and not row["healer"] and not row["killed"]:
            continue
        line = f"  {clock(row['t'])} {trace.name(row['g'])} ({trace.role(row['g'])})"
        if row["out"] is not None:
            line += f" out of range from {clock(row['out'])}"
        if row["healer"]:
            line += f", {trace.name(row['healer'])} sent {clock(row['sent'])}"
            # no distance when the healer died or the latch came in the bomb's last snapshot gap
            if row["closest"] is not None:
                line += (f", closest {row['closest']:.0f} yd{' (inside the pull)' if row['closest'] < pull else ''},"
                         f" {row['at_expiry']:.0f} yd at expiry")
            line += f", {row['heals']} heals from it"
            back = back_on_slot(trace, row["healer"], row["until"])
            line += f", back on its slot {back / 1000:.0f} s after" if back is not None else ", never back on its slot"
        elif row["out"] is not None:
            line += ", nobody sent"
        if row["nearest"] is not None:
            line += f", nearest healer {row['nearest']:.0f} yd at expiry"
        if row["hp"] is not None:
            line += f", carrier {row['hp']:.0f}% at expiry"
        if row["killed"]:
            line += ", killed"
        print(line)


def mechanic(trace: Trace, death: dict) -> str:
    killer = entry(trace, death.get("killer"))
    rewind = death.get("rewind") or []
    spell_id = rewind[-1][2] if rewind else 0
    spell = (trace.spells.get(spell_id) or f"spell {spell_id}") if spell_id else ""
    if killer in TRASH:
        return "Scrapyard trash"
    if killer in ADDS:
        return ADDS[killer]
    if killer == NPC_VOID_ZONE:
        return "Void Zone"
    if killer == NPC_LIFE_SPARK:
        return "Life Spark"
    if killer == NPC_XT002 and spell:
        return spell
    if killer == NPC_XT002:
        return "XT melee on a tank" if trace.role(death["g"]) == "tank" else "XT melee on a non-tank"
    return f"{trace.name(death.get('killer'))} {spell}".strip()


def show_deaths(trace: Trace) -> None:
    print("DEATHS BY MECHANIC")
    if not mine(trace):
        return
    counts = collections.Counter(mechanic(trace, d) for d in combat_deaths(trace))
    if not counts:
        print("  none")
    for name, n in counts.most_common():
        print(f"  {n:3d}  {name}")


SECTIONS = (
    ("heart", "each Heart window and whether Heartbreak landed", show_heart),
    ("adds", "adds per toy pile", show_adds),
    ("trash", "Scrapyard trash pulls and what called them", show_trash),
    ("sparks", "Life Sparks that lived long, their carrier and evade notes", show_sparks),
    ("tanks", "who XT was on and how far off the tank spot, tank duty notes, taunts at him", show_tanks),
    ("bombs", "Gravity Bomb carriers out of heal range and the healer sent to them", show_bombs),
    ("deaths", "deaths by mechanic", show_deaths),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
