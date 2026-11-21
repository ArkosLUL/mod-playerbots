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
from raidobs.trace import Trace, clock, combat_deaths  # noqa: E402

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
CLASS_HUNTER = 3

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
    ("deaths", "deaths by mechanic", show_deaths),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
