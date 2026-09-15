#!/usr/bin/env python3
"""Score a Yogg-Saron pull from a RaidObs trace: phases, clouds, portals, the brain room, Crush.

    yogg_saron.py <file>            every section
    yogg_saron.py <file> --phases   phase timeline and what happened before the pull
    yogg_saron.py <file> --clouds   cloud-orbit exposure per role
    yogg_saron.py <file> --portals  portal waves, assignments and who got down
    yogg_saron.py <file> --brain    brain-room occupancy, the Brain's health, skull exposure
    yogg_saron.py <file> --crush    Crush, the body's knockback and Death Rays, per role
    yogg_saron.py <file> --sanity   Sanity minima and where they went

The fight has three rooms, five mechanics that kill and nothing in the world to sweep for two of
them, so most of what matters here is in `note` rows rather than in the snapshot:

- **Sara is FACTION_FRIENDLY for all of phase 1**, and the snapshot sweep keeps only units hostile to
  the anchor, so her health bar - which *is* phase 1's progress - is never sampled. `yogg.phase` is
  the only phase timeline a trace carries.
- **The body's knockback has no world object.** 64022 is an infinite self aura on Yogg triggering
  64020 every second, a 14 yd knock back that works out to a 13.26 yd horizontal ring on the floor.
  It reaches the file only as a `haz` circle, which nothing tests a death against.
- **Crush is a cone with no dynamic object either**, and its facing tracks whoever hit the tentacle
  last. `yogg.crush` is the per-bot read and `haz` carries the geometry to check it against.
- **A disguised Influence Tentacle keeps its own health while showing the disguise's entry**, because
  Creature::UpdateEntry preserves current health. A Suit of Armor at 6% is not a damaged Suit of
  Armor, it is a full-health tentacle.
- **Portals are creatures (34072), not gameobjects**, one use each, despawning after 25 s, and only
  RAID_MODE(4, 10) of the ten spots ever spawn. They are not hostile, so the snapshot sweep - which
  keeps only units hostile to the anchor - never samples one. `yogg.wave` is the only record that a
  wave happened at all, and the spot table below is the only record of where they were.

`ObsValue` and `ObsGuidMap` emit on change only, so a key going quiet means "unchanged", never
"never happened". A key missing from the whole trace is a different thing and is reported as such:
that means the probe is not reaching the recorder, not that the mechanic never fired.
"""
from __future__ import annotations

import argparse
import collections
import math
import pathlib
import statistics
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from analysis import roster_guids  # noqa: E402
from obstrace import Trace, clock  # noqa: E402

NPC_GUARDIAN = 33136
NPC_YOGG_SARON = 33288
NPC_BRAIN = 33890
NPC_INFLUENCE_TENTACLE = 33943

SPELL_SANITY = 63050
SPELL_INDUCE_MADNESS = 64059
SPELL_LUNATIC_GAZE_SKULL = 64168
SPELL_GRIM_REPRISAL = 64039
SPELL_CRUSH_CONE = 64147
SPELL_KNOCK_BACK = 64020
SPELL_SHADOW_NOVA_SARA = 65719

# A Guardian's death nova is 15 yd (62714 and 65209 both carry radius index 18) and the ranged station
# is 21.5, so a Guardian dying more than 6.5 yd from the middle catches the whole raid rather than the
# melee pile. Measured: novas at 2.2-4.0 yd hit 9-10 players, novas at 8.5-14.8 yd hit 22-24.
NOVA_RADIUS = 15.0
RANGED_STATION = 21.5

# 64022 on Yogg triggers a 14 yd knock back every second, and his model sits 4.5 yd above the
# floor, so what lands on the floor is a 13.26 yd ring. Nothing in the world can be swept for it.
KNOCKBACK_RADIUS = 13.3

BODY = (1980.28, -25.5868)

# boss_yoggsaron.cpp yoggPortalLoc, in table order. AddPortals spawns RAID_MODE(4, 10) of them, so a
# 10-man pull only ever gets the first four.
PORTAL_SPOTS = [
    (1964.60, -42.71), (1986.94, -46.21), (1989.50, -6.70), (1965.52, -8.09), (2000.84, -25.40),
    (1960.22, -26.14), (1976.30, -47.83), (1997.69, -37.46), (1998.07, -13.36), (1976.99, -3.96),
]

# Every key this file reads. A key in source and absent from every trace of its own boss means the
# recorder is dropping it - flame_leviathan.py has read fl.station for a year and never once got a
# row, because an early return shadows it.
PROBE_KEYS = (
    "yogg.phase", "yogg.engaged", "yogg.room", "yogg.roomstate", "yogg.cloudreach", "yogg.knockback",
    "yogg.crush", "yogg.deathray", "yogg.wave", "yogg.portal", "yogg.portalslot", "yogg.brainteam",
    "yogg.skull", "yogg.exit", "yogg.handover",
)

PHASE_NAMES = {0: "idle", 1: "phase 1", 2: "phase 2", 3: "phase 3"}


def notes(trace: Trace, key: str) -> list[dict]:
    return [rec for rec in trace.of("note") if rec.get("k") == key]


def role_of(trace: Trace, guid: int) -> str:
    return trace.roles.get(guid, "?")


def tracks(trace: Trace, guids: set[int]) -> dict[int, list[tuple[int, float, float]]]:
    """Snapshot positions per guid, in time order."""
    out: dict[int, list[tuple[int, float, float]]] = collections.defaultdict(list)
    for snap in trace.of("snap"):
        for row in snap.get("u", []):
            if row[0] in guids:
                out[row[0]].append((snap["t"], row[1], row[2]))
    return out


def first_seen(trace: Trace, guids: set[int]) -> int | None:
    for snap in trace.of("snap"):
        for row in snap.get("u", []):
            if row[0] in guids:
                return snap["t"]
    return None


def guids_of_entry(trace: Trace, entry: int) -> set[int]:
    return {guid for guid, en in trace.entries.items() if en == entry}


def phase_spans(trace: Trace) -> list[tuple[int, int, int]]:
    """(phase, start, end) over the whole file, from the change-only yogg.phase stream."""
    marks = sorted((rec["t"], int(rec.get("txt", 0))) for rec in notes(trace, "yogg.phase"))
    if not marks:
        return []

    end = trace.records[-1].get("t", marks[-1][0]) if trace.records else marks[-1][0]
    spans = []
    for index, (start, phase) in enumerate(marks):
        stop = marks[index + 1][0] if index + 1 < len(marks) else end
        spans.append((phase, start, stop))
    return spans


def missing_probes(trace: Trace) -> list[str]:
    present = {rec.get("k", "") for rec in trace.of("note")}
    return [key for key in PROBE_KEYS if key not in present]


def show_banner(trace: Trace) -> None:
    gone = missing_probes(trace)
    if gone:
        print(f"probes absent from this trace: {', '.join(gone)}")
        print("  a key declared in source and absent from every row means the recorder is dropping it")
    else:
        print("all yogg.* probes present")


def show_phases(trace: Trace) -> None:
    print("PHASES")
    spans = phase_spans(trace)
    if not spans:
        print("  no yogg.phase rows - either a pre-probe trace, or nothing ever read the encounter")
        return

    for phase, start, stop in spans:
        print(f"  {PHASE_NAMES.get(phase, phase):8} {clock(start):>9} -> {clock(stop):>9}"
              f"  ({(stop - start) / 1000.0:6.1f} s)")

    # How late the raid noticed. t=0 is the `pull src=bossstate` row, which the recorder writes when
    # the boss state goes IN_PROGRESS - that is inside Sara's InitFight, so t=0 is the pull itself.
    # First damage is 20-30 s later and is not a pull marker: reading it as one is what hid a build
    # whose encounter read did not open until 24 s in.
    engaged = [rec["t"] for rec in notes(trace, "yogg.engaged") if rec.get("txt") == "engaged"]
    if engaged:
        late = min(engaged)
        note = "" if late < 2000 else "   <- nothing yogg-specific ran until here"
        print()
        print(f"  gate opens              : {clock(late)}, {late / 1000.0:.1f} s after the pull{note}")

    # Phase 1 only. Guardians come back in phase 3, but the station they have to die clear of is a
    # phase 1 thing and the ring geometry below means nothing once it is gone.
    p1_end = next((stop for phase, _, stop in spans if phase == 1), None)

    guardians = guids_of_entry(trace, NPC_GUARDIAN)
    born = {guid: pts[0][0] for guid, pts in tracks(trace, guardians).items()}
    if born:
        early = [t for t in born.values() if t < 0]
        in_p1 = [t for t in born.values() if 0 <= t <= (p1_end or 0)]
        print(f"  Guardians in phase 1    : {len(in_p1)}, {len(early)} of them before the pull")

    # Where a Guardian died decides who its nova hits, and it is the only phase 1 number that moves
    # the back line's damage. Creature deaths are not in the `death` stream, which is roster only, so
    # the nova it casts at Sara on the way out is the record that it died at all.
    novas = [(rec["t"], rec.get("s")) for rec in trace.of("cast")
             if rec.get("sp") == SPELL_SHADOW_NOVA_SARA and rec["t"] <= (p1_end or 0)]
    where = tracks(trace, guardians)
    radii = []
    for when, guid in novas:
        pts = [p for p in where.get(guid, []) if p[0] <= when]
        if pts:
            radii.append(math.dist((pts[-1][1], pts[-1][2]), BODY))
    if radii:
        wide = sum(1 for r in radii if r > RANGED_STATION - NOVA_RADIUS)
        print(f"  Guardian deaths         : {len(radii)}, median {statistics.median(radii):.1f} yd "
              f"from the middle")
        print(f"    reaching the {RANGED_STATION:.1f} yd station (over "
              f"{RANGED_STATION - NOVA_RADIUS:.1f} yd out): {wide} of {len(radii)}")

    show_handover(trace)


def show_handover(trace: Trace) -> None:
    """Sara hits 0 and Yogg is summoned invisible in the same tick; 18 s of transformation dialogue
    later ACTION_YOGG_SARON_APPEAR lights the knockback ring and summons the Brain together. Melee and
    the tank are leashed to the middle right through it, so the question is who was standing in the
    ring when it appeared."""
    down = first_seen(trace, guids_of_entry(trace, NPC_YOGG_SARON))
    ring = first_seen(trace, guids_of_entry(trace, NPC_BRAIN))
    if down is None or ring is None or ring <= down:
        return

    print(f"  handover                : {clock(down)} -> {clock(ring)} "
          f"({(ring - down) / 1000.0:.1f} s)")

    roster = roster_guids(trace)
    standing: dict[int, float] = {}
    for snap in trace.of("snap"):
        if abs(snap["t"] - ring) > 400:
            continue
        for row in snap.get("u", []):
            if row[0] in roster:
                standing.setdefault(row[0], math.dist((row[1], row[2]), BODY))

    inside: dict[str, list[bool]] = collections.defaultdict(list)
    for guid, far in standing.items():
        inside[role_of(trace, guid)].append(far <= KNOCKBACK_RADIUS)
    if inside:
        parts = ", ".join(f"{role} {sum(v)} of {len(v)}"
                          for role, v in sorted(inside.items()) if v)
        print(f"    in the {KNOCKBACK_RADIUS} yd ring when it lit: {parts}")

    held = collections.Counter(rec.get("txt") for rec in notes(trace, "yogg.handover"))
    if held:
        print(f"    yogg.handover         : {dict(held)}")


def show_clouds(trace: Trace) -> None:
    print("CLOUD ORBITS")
    rows = notes(trace, "yogg.cloudreach")
    if not rows:
        print("  no yogg.cloudreach rows")
        return

    # The stream is change-only, so a bot's reading holds until its next row. Weighting by the span
    # it held for is the only honest way to turn it into a share of the phase.
    held: dict[int, tuple[int, str]] = {}
    time_in: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
    end = trace.records[-1].get("t", 0) if trace.records else 0
    for rec in sorted(rows, key=lambda r: r["t"]):
        guid, now, value = rec.get("g", 0), rec["t"], str(rec.get("txt", ""))
        if guid in held:
            was_t, was = held[guid]
            time_in[role_of(trace, guid)][was] += now - was_t
        held[guid] = (now, value)
    for guid, (was_t, was) in held.items():
        time_in[role_of(trace, guid)][was] += max(0, end - was_t)

    for role in sorted(time_in):
        total = sum(time_in[role].values()) or 1
        parts = sorted(time_in[role].items(), key=lambda kv: -kv[1])[:4]
        shares = "  ".join(f"{value}: {span * 100.0 / total:4.1f}%" for value, span in parts)
        print(f"  {role:7} {shares}")

    dodges = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.p1dodge"))
    if dodges:
        print(f"\n  phase 1 dodge reasons : {dict(dodges)}")
    station = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.p1station"))
    if station:
        print(f"  station re-issues     : {dict(station)}")


def show_portals(trace: Trace) -> None:
    print("PORTAL WAVES")
    waves = [(rec["t"], int(rec.get("txt", 0))) for rec in notes(trace, "yogg.wave")]
    if waves:
        for when, ordinal in waves:
            print(f"  wave {ordinal:<2} opened at {clock(when)}")
    else:
        print("  no yogg.wave rows - no wave was ever seen, or this trace predates the clock")

    team = sorted({rec.get("g", 0) for rec in notes(trace, "yogg.brainteam") if rec.get("txt") == "1"})
    if team:
        roles = collections.Counter(role_of(trace, guid) for guid in team)
        names = ", ".join(trace.name(guid) for guid in team)
        print(f"\n  brain team ({len(team)}): {dict(roles)}")
        print(f"    {names}")

    # What the assignment cost. By group index it sent bots a median of 41-44 yd when the nearest
    # spot was 12-15, so assigned-against-nearest is the number that says whether it is fixed.
    assigned, nearest = [], []
    for rec in notes(trace, "yogg.portalslot"):
        slot = int(rec.get("txt", 0))
        where = position_at(trace, rec.get("g", 0), rec["t"])
        if not where or not 0 <= slot < len(PORTAL_SPOTS):
            continue
        assigned.append(math.dist(where, PORTAL_SPOTS[slot]))
        nearest.append(min(math.dist(where, spot) for spot in PORTAL_SPOTS))
    if assigned:
        print(f"\n  walk to the assigned spot: median {median(assigned):.1f} yd"
              f", nearest spot was {median(nearest):.1f} yd  ({len(assigned)} assignments)")

    states = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.portal"))
    if states:
        print(f"  portal intent rows   : {dict(states)}")

    # Who actually got down. The room is the only proof: the portal creature is not hostile, so no
    # snapshot samples one, and a click that failed leaves nothing else behind.
    arrived = {rec.get("g", 0) for rec in notes(trace, "yogg.room")
               if str(rec.get("txt", "")) in ("stormwind", "icecrown", "chamber", "brain")}
    if team:
        print(f"  reached the brain level: {len(arrived & set(team))} of {len(team)} on the team"
              f" ({len(arrived)} bots in all)")


def position_at(trace: Trace, guid: int, when: int):
    best = None
    for snap in trace.of("snap"):
        if snap["t"] > when:
            break
        for row in snap.get("u", []):
            if row[0] == guid:
                best = (row[1], row[2])
    return best


def median(values: list[float]) -> float:
    ordered = sorted(values)
    mid = len(ordered) // 2
    return ordered[mid] if len(ordered) % 2 else (ordered[mid - 1] + ordered[mid]) / 2.0


def show_brain(trace: Trace) -> None:
    print("BRAIN ROOM")

    rooms = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.room"))
    if rooms:
        print(f"  room reads       : {dict(rooms)}")

    # The deadlock: a bot that never walks in has no target, the tentacle lives, the door stays shut.
    states = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.roomstate"))
    if states:
        print(f"  room states      : {dict(states)}")
    else:
        print("  no yogg.roomstate rows - nothing was ever in an illusion room")

    brains = guids_of_entry(trace, NPC_BRAIN)
    if brains:
        track = []
        for snap in trace.of("snap"):
            for row in snap.get("u", []):
                if row[0] in brains:
                    track.append((snap["t"], row[5]))
        if track:
            print(f"  Brain health     : {track[0][1]:.1f}% at {clock(track[0][0])}"
                  f" -> {track[-1][1]:.1f}% at {clock(track[-1][0])}"
                  f", low {min(hp for _, hp in track):.1f}%")
    else:
        print("  the Brain was never sampled")

    tentacles = guids_of_entry(trace, NPC_INFLUENCE_TENTACLE)
    if tentacles:
        kills = sum(1 for rec in trace.of("death") if rec.get("g") in tentacles)
        print(f"  Influence Tentacles seen: {len(tentacles)}, {kills} died")

    gaze = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_LUNATIC_GAZE_SKULL]
    if gaze:
        total = sum(rec.get("a", 0) for rec in gaze)
        print(f"  Lunatic Gaze     : {len(gaze)} hits for {total:,} damage")
    skulls = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.skull"))
    if skulls:
        print(f"  skulls in arc    : {dict(skulls)}")

    reprisal = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_GRIM_REPRISAL]
    if reprisal:
        total = sum(rec.get("a", 0) for rec in reprisal)
        print(f"  Grim Reprisal    : {len(reprisal)} hits for {total:,} self-inflicted")

    madness = [rec for rec in trace.of("dmg") + trace.of("aura") if rec.get("sp") == SPELL_INDUCE_MADNESS]
    exits = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.exit"))
    if exits:
        print(f"  exit decisions   : {dict(exits)}")
    if madness:
        print(f"  Induce Madness   : {len(madness)} rows")


def show_crush(trace: Trace) -> None:
    print("CRUSH, KNOCKBACK AND DEATH RAYS")

    for key, label in (("yogg.crush", "Crush"), ("yogg.knockback", "body ring"),
                       ("yogg.deathray", "Death Ray")):
        rows = notes(trace, key)
        if not rows:
            print(f"  {label:10}: no {key} rows")
            continue
        per_role: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
        for rec in rows:
            per_role[role_of(trace, rec.get("g", 0))][str(rec.get("txt", ""))] += 1
        print(f"  {label}:")
        for role in sorted(per_role):
            total = sum(per_role[role].values()) or 1
            shares = "  ".join(f"{value}: {count * 100.0 / total:4.1f}%"
                               for value, count in sorted(per_role[role].items()))
            print(f"    {role:7} {shares}   ({total} transitions)")

    for spell, label in ((SPELL_CRUSH_CONE, "Crush 64147"), (SPELL_KNOCK_BACK, "Knock Back 64020")):
        hits = [rec for rec in trace.of("dmg") if rec.get("sp") == spell]
        if hits:
            total = sum(rec.get("a", 0) for rec in hits)
            killed = sum(1 for rec in hits if rec.get("ok", 0) > 0)
            print(f"  {label}: {len(hits)} hits, {total:,} damage, {killed} killing blows")

    deaths = trace.of("death")
    if deaths:
        by_role = collections.Counter(role_of(trace, rec.get("g", 0)) for rec in deaths)
        print(f"  deaths by role: {dict(by_role)}")

    lanes = [rec for rec in trace.of("haz") if rec.get("shape") == "wedge"]
    circles = [rec for rec in trace.of("haz") if rec.get("sp") == SPELL_KNOCK_BACK]
    print(f"  haz rows: {len(lanes)} Crush wedges, {len(circles)} body rings")


def show_sanity(trace: Trace) -> None:
    print("SANITY")
    rows = [rec for rec in trace.of("aura") if rec.get("sp") == SPELL_SANITY]
    if not rows:
        print("  no 63050 rows - Sanity is seeded once at the pull and never re-applied,"
              " so an empty read here means the pull never started")
        return

    low: dict[int, int] = {}
    for rec in rows:
        # A removal row carries no stack count. Counting it as zero reads every bot as Insane.
        if rec.get("r"):
            continue
        guid, stacks = rec.get("d", 0), rec.get("st", 0)
        low[guid] = min(low.get(guid, stacks), stacks)

    for guid in sorted(low, key=lambda g: low[g]):
        print(f"  {trace.name(guid):18} {role_of(trace, guid):7} low {low[guid]:3}")

    insane = [guid for guid, stacks in low.items() if stacks <= 0]
    if insane:
        print(f"  went Insane: {', '.join(trace.name(g) for g in insane)}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("file", type=pathlib.Path)
    parser.add_argument("--phases", action="store_true", help="phase timeline and the pre-pull window")
    parser.add_argument("--clouds", action="store_true", help="cloud-orbit exposure per role")
    parser.add_argument("--portals", action="store_true", help="portal waves and assignments")
    parser.add_argument("--brain", action="store_true", help="brain room, the Brain, skulls")
    parser.add_argument("--crush", action="store_true", help="Crush, knockback and Death Rays")
    parser.add_argument("--sanity", action="store_true", help="Sanity minima")
    args = parser.parse_args()

    if not args.file.is_file():
        print(f"no such trace: {args.file}", file=sys.stderr)
        return 1

    trace = Trace(args.file)
    picked = (args.phases, args.clouds, args.portals, args.brain, args.crush, args.sanity)
    every = not any(picked)

    show_banner(trace)
    for wanted, section in zip(picked, (show_phases, show_clouds, show_portals, show_brain,
                                        show_crush, show_sanity)):
        if wanted or every:
            print()
            section(trace)
    return 0


if __name__ == "__main__":
    sys.exit(main())
