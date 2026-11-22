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
"""
from __future__ import annotations

import collections
import pathlib
import statistics
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.geometry import dist2, frames, nearest  # noqa: E402
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
CLASS_HUNTER = 3

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
    """Spark guid -> (first `xt002.evading` note naming it, how many bots named it)."""
    first, bots = {}, collections.defaultdict(set)
    for rec in notes(trace, "xt002.evading"):
        for token in rec.get("txt", "").split():
            if token.isdigit():
                first.setdefault(int(token), rec["t"])
                bots[int(token)].add(rec.get("g"))
    return {guid: (t, len(bots[guid])) for guid, t in first.items()}


def held_after(trace: Trace, guid, since: int) -> int:
    """Most roster members still targeting this unit in one snapshot from `since` on."""
    roster = {member["g"] for member in trace.header.get("roster", [])}
    most = 0
    for snap in frames(trace):
        if snap["t"] >= since:
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
            t, bots = evading[spark["g"]]
            held = held_after(trace, spark["g"], t + SPARK_DROP_MS)
            print(f"    evading from {clock(t)} (bots noting it {bots}),"
                  f" most bots still on it {SPARK_DROP_MS // 1000} s later {held}")


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
    ("deaths", "deaths by mechanic", show_deaths),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
