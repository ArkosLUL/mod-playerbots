#!/usr/bin/env python3
"""Score a Yogg-Saron pull from a RaidObs trace: phases, clouds, portals, the brain room, Crush.

    yogg_saron.py <file>            every section
    yogg_saron.py <file> --phases   phase timeline and what happened before the pull
    yogg_saron.py <file> --clouds   cloud-orbit exposure per role, and who summoned each Guardian
    yogg_saron.py <file> --threat   who the Guardians were on, redirects, and taunt aim
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
from geometry import at, first_seen, guids_of_entry  # noqa: E402
from geometry import track as tracks  # noqa: E402
from obstrace import Trace, clock  # noqa: E402
from probes import emitted_keys, latch_spans, silent_keys  # noqa: E402
from space import show_share, threat_share  # noqa: E402
from views import IDLE_MS, idle_windows, veto_counts  # noqa: E402

NPC_GUARDIAN = 33136
NPC_YOGG_SARON = 33288
NPC_BRAIN = 33890
NPC_INFLUENCE_TENTACLE = 33943
NPC_LAUGHING_SKULL = 33990
NPC_CRUSHER_TENTACLE = 33966

# The six entries Creature::UpdateEntry disguises an Influence Tentacle as, picked by where it spawned
# rather than by which illusion is running. A tentacle wears one from the moment it is summoned and
# only reverts to 33943 once something damages it, so counting 33943 alone counts revealed tentacles,
# never the room.
NPC_TENTACLE_DISGUISES = (33433, 33567, 33716, 33717, 33718, 33719, 33720)

SPELL_SANITY = 63050
SPELL_INSANE = 63120
SPELL_INDUCE_MADNESS = 64059
SPELL_LUNATIC_GAZE_SKULL = 64168
SPELL_GRIM_REPRISAL = 64039
SPELL_CRUSH_CONE = 64147
SPELL_KNOCK_BACK = 64020
SPELL_SHADOW_NOVA_SARA = 65719
SPELL_SQUEEZE = 64126
SPELL_BRAIN_LINK = 63802
SPELL_BRAIN_LINK_DAMAGE = 63803
SPELL_BRAIN_LINK_OK = 63804
# What a bot casts on itself the moment it takes a portal. The portal creature is not hostile so no
# snapshot ever samples one, and a click that failed leaves nothing behind - this is the only proof a
# portal was actually used.
SPELL_ILLUSION_ROOM = 63988
SPELL_HAND_OF_PROTECTION = (10278, 5599, 1022)

# A Guardian's death nova is 15 yd (62714 and 65209 both carry radius index 18) and the ranged station
# is 21.5, so a Guardian dying more than 6.5 yd from the middle catches the whole raid rather than the
# melee pile. Measured: novas at 2.2-4.0 yd hit 9-10 players, novas at 8.5-14.8 yd hit 22-24.
NOVA_RADIUS = 15.0
RANGED_STATION = 21.5
P1_LEASH = 6.5

# 65719 is a flat 25,000 against Sara, so what the phase costs is her health divided by it. Never
# assume that number: mod-dungeon-scale scales raid boss health and the result moves with raid size -
# 240,000 at 25 raiders, 237,500 at 24, against the 199,999 of her spawn row. Ten kills, not eight.
# She is FACTION_FRIENDLY and no snapshot ever samples her, but the `unit` row carries her max health.
NPC_SARA = 33134
SARA_NOVA_DAMAGE = 25000

# The clouds. Six of them, one per orbit, circling the middle at a constant 3.0 yd/s; radii are the
# midpoints of a four-pull spread that never drifted. A cloud summons on any player within 8.5 yd -
# SelectNearbyTarget's 6 goes through _IsWithinDist, which adds both combat reaches - and the Guardian
# appears 10 s later, because 63031 is a 10 s aura rather than the instant summon the C++ reads as.
#
# InformCloud, Sara's own 20-18-16-14-12-10 s timer, skips any cloud within 20 yd of her, so the
# innermost orbit can only ever have been triggered by a player standing on it. That is what makes
# attribution possible at all.
NPC_OMINOUS_CLOUD = 33292
CLOUD_ORBITS = (11.6, 21.4, 31.2, 41.0, 50.9, 60.8)
CLOUD_SUMMON_REACH = 8.5
CLOUD_SUMMON_DELAY_MS = 10000
INFORM_CLOUD_MIN_RANGE = 20.0

# Threat redirects and the single-target taunts, for --threat. Righteous Defense is left out: it is
# aimed at the raid member being hit rather than at a Guardian, so it cannot be ranked against one.
REDIRECT_SPELLS = {34477: "Misdirection", 57934: "Tricks of the Trade"}
TAUNT_SPELLS = {355: "Taunt", 62124: "Hand of Reckoning", 56222: "Dark Command", 2649: "Growl"}

# GetCombatReach of both ends plus 4/3, floored at NOMINAL_MELEE_RANGE and widened again by movement
# leeway, so the real threshold moves. Kept generous because the question this answers is "what was in
# there at all": every Crush in one pull had a pet at 5.5 yd and no player nearer than 12.
CRUSHER_MELEE_RANGE = 10.0

# 64167 on a Laughing Skull triggers 64168 at 30 yd, and the module's node uses the same number.
LAUGHING_SKULL_RADIUS = 30.0

# TEMPSUMMON_TIMED_DESPAWN on every portal, so the whole window a wave offers is this long.
PORTAL_DESPAWN_MS = 25000

# A creature that stops being sampled below this was almost certainly killed; one that vanishes near
# full health was despawned by the phase. Nothing records a creature death, so this is the line.
TENTACLE_SPENT_PCT = 20.0

# 64022 on Yogg triggers a 14 yd knock back every second, and his model sits 4.5 yd above the
# floor, so what lands on the floor is a 13.26 yd ring. Nothing in the world can be swept for it.
KNOCKBACK_RADIUS = 13.3

# The boss platform floor samples between 324.8 and 325.6, and a thrown bot peaks at 329-331, so this
# pair separates standing from flying without catching the ramp at the rim.
FLOOR_Z = 325.6
KNOCKBACK_ARC_Z = 2.0

BODY = (1980.28, -25.5868)

# Brain Link ticks 63803 and -2 Sanity on both ends past this, nothing inside it. Only the owner
# carries 63802; the partner is only visible as the second raider taking 63803 in the same window.
BRAIN_LINK_RANGE = 20.0

# The boss platform floor is z 325-330 and every illusion room is z 236-244, so this separates them.
BRAIN_LEVEL_Z = 300.0

# boss_yoggsaron.cpp yoggPortalLoc, in table order. AddPortals spawns RAID_MODE(4, 10) of them, so a
# 10-man pull only ever gets the first four.
PORTAL_SPOTS = [
    (1964.60, -42.71), (1986.94, -46.21), (1989.50, -6.70), (1965.52, -8.09), (2000.84, -25.40),
    (1960.22, -26.14), (1976.30, -47.83), (1997.69, -37.46), (1998.07, -13.36), (1976.99, -3.96),
]

PHASE_NAMES = {0: "idle", 1: "phase 1", 2: "phase 2", 3: "phase 3"}


def notes(trace: Trace, key: str) -> list[dict]:
    return [rec for rec in trace.of("note") if rec.get("k") == key]


def sara_kills_needed(trace: Trace) -> int | None:
    """Guardian deaths inside 15 yd the phase costs, from Sara's own max health."""
    for guid in guids_of_entry(trace, NPC_SARA):
        maxhp = trace.maxhp.get(guid)
        if maxhp:
            return math.ceil(maxhp / SARA_NOVA_DAMAGE)
    return None


def phase_spans(trace: Trace) -> list[tuple[int, int, int]]:
    """(phase, start, end) over the whole file, from the change-only yogg.phase stream."""
    end = trace.records[-1].get("t", 0) if trace.records else 0
    return [(int(value or 0), start, stop)
            for value, start, stop in latch_spans(trace, "yogg.phase", end)]


def missing_probes(trace: Trace) -> list[str]:
    """`yogg.*` keys the source declares and this pull never emitted. Named outright rather than
    resolved off the trace: a pull still filed under the map resolves to no boss, and a check keyed
    on that matches no key at all and reports every one present."""
    return [key for key, _, _ in silent_keys(emitted_keys(trace), "yogg-saron")]


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

        # The only number that says how close the phase came. A kill outside the nova's reach of Sara
        # does nothing at all for it, so 13 deaths can still be 9 kills.
        counted = sum(1 for r in radii if r <= NOVA_RADIUS)
        needed = sara_kills_needed(trace)
        if needed:
            short = needed - counted
            verdict = "enough" if short <= 0 else f"{short} short"
            print(f"    counting for Sara (inside {NOVA_RADIUS:.0f} yd): {counted} of the {needed}"
                  f" she needed - {verdict}")
        else:
            print(f"    counting for Sara (inside {NOVA_RADIUS:.0f} yd): {counted}"
                  f"  (no Sara unit row, so her health is unknown)")

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
        inside[trace.role(guid)].append(far <= KNOCKBACK_RADIUS)
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
            time_in[trace.role(guid)][was] += now - was_t
        held[guid] = (now, value)
    for guid, (was_t, was) in held.items():
        time_in[trace.role(guid)][was] += max(0, end - was_t)

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
    taunts = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.p1taunt"))
    if taunts:
        print(f"  phase 1 taunts        : {dict(taunts)}  (counts = still inside 15 yd of Sara)")

    show_cloud_spawns(trace)


def show_cloud_spawns(trace: Trace) -> None:
    """Which Guardians the raid summoned by standing on a cloud, and which Sara's timer sent.

    Every Guardian appears at the cloud that summoned it, so the cloud is read off the spawn position
    rather than guessed. Who marked it is read 10 s earlier, because 63031 is a 10 s aura and the
    Guardian arrives at the end of it - attributing one without rewinding blames whoever happens to be
    standing there when it lands.
    """
    frames = [(snap["t"], {row[0]: (row[1], row[2]) for row in snap.get("u", [])})
              for snap in trace.of("snap")]
    if not frames:
        print("\n  no snapshots, so nothing to attribute")
        return

    guardians = guids_of_entry(trace, NPC_GUARDIAN)
    clouds = guids_of_entry(trace, NPC_OMINOUS_CLOUD)
    roster = roster_guids(trace)
    if not clouds:
        print("\n  no Ominous Cloud was ever sampled, so spawns cannot be attributed")
        return

    def orbit_of(radius: float) -> int:
        for index, ring in enumerate(CLOUD_ORBITS, 1):
            if abs(radius - ring) < 1.5:
                return index
        return 0

    def frame_at(when: int):
        return min(frames, key=lambda frame: abs(frame[0] - when))

    cloud_orbit = {}
    for guid in clouds:
        for _, positions in frames:
            if guid in positions:
                cloud_orbit[guid] = orbit_of(math.dist(positions[guid], BODY))
                break

    born: dict[int, tuple[int, tuple[float, float]]] = {}
    for when, positions in frames:
        for guid in positions:
            if guid in guardians and guid not in born:
                born[guid] = (when, positions[guid])

    print("\nCLOUD SPAWNS")
    per_orbit = collections.Counter()
    contact = collections.Counter()
    detail = []
    for guid, (when, where) in sorted(born.items(), key=lambda kv: kv[1][0]):
        _, positions = frame_at(when)
        near = sorted((math.dist(positions[cloud], where), cloud)
                      for cloud in clouds if cloud in positions)
        if not near or near[0][0] > 4.0:
            detail.append((when, 0, []))
            continue

        gap, cloud = near[0]
        orbit = cloud_orbit.get(cloud, 0)
        per_orbit[orbit] += 1

        _, marked = frame_at(when - CLOUD_SUMMON_DELAY_MS)
        who = []
        if cloud in marked:
            who = sorted((round(math.dist(marked[guid2], marked[cloud]), 1), trace.name(guid2),
                          trace.role(guid2))
                         for guid2 in roster
                         if guid2 in marked
                         and math.dist(marked[guid2], marked[cloud]) <= CLOUD_SUMMON_REACH)
        if who:
            contact[orbit] += 1
        detail.append((when, orbit, who))

    for when, orbit, who in detail:
        names = ", ".join(f"{name}/{role} {gap}" for gap, name, role in who) or "-"
        ring = f"orbit{orbit}" if orbit else "no cloud"
        print(f"  {clock(when):>9}  {ring:<9}  {names}")

    # The innermost orbit is the load-bearing one: InformCloud skips every cloud within 20 yd of Sara,
    # so Sara's timer can never pick it and every Guardian off it was summoned by a player.
    inner = [index for index, ring in enumerate(CLOUD_ORBITS, 1) if ring < INFORM_CLOUD_MIN_RANGE]
    ours = sum(count for orbit, count in per_orbit.items() if orbit in inner)
    ours += sum(count for orbit, count in contact.items() if orbit not in inner)
    total = sum(per_orbit.values())
    print(f"\n  by orbit: {dict(sorted(per_orbit.items()))}   of {len(born)} Guardians")
    print(f"  the raid's own feet: {ours} of {total}"
          f"   (every orbit {'/'.join(str(i) for i in inner)} spawn, plus any other with a raider"
          f" inside {CLOUD_SUMMON_REACH} yd at the mark)")
    print(f"  Sara's timer: {total - ours}"
          f"   (20-18-16-14-12-10 s, and it can only pick a cloud over"
          f" {INFORM_CLOUD_MIN_RANGE:.0f} yd out)")


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
        roles = collections.Counter(trace.role(guid) for guid in team)
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
        print(f"\n  walk to the assigned spot: median {statistics.median(assigned):.1f} yd"
              f", nearest spot was {statistics.median(nearest):.1f} yd  ({len(assigned)} assignments)")

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

    show_portal_drop(trace, waves)

    show_rooms(trace, waves)


def show_portal_drop(trace: Trace, waves: list[tuple[int, int]]) -> None:
    """How many of the wave's portals were taken, against how many bots were told to take one.
    AddPortals spawns RAID_MODE(4, 10) of them per wave and each is one use, so the gap between
    assigned and gone is bots that walked to a spot somebody else had already used."""
    if not waves:
        return

    assignments: dict[int, dict[int, int]] = {}
    for rec in notes(trace, "yogg.portalslot"):
        assignments.setdefault(rec["t"], {})[rec.get("g", 0)] = int(rec.get("txt", 0))

    print()
    for start, ordinal in waves:
        rounds = [when for when in assignments if when <= start]
        plan = assignments[max(rounds)] if rounds else {}
        stale = (start - max(rounds)) / 1000.0 if rounds else 0.0

        took = {rec.get("s") for rec in trace.of("cast")
                if rec.get("sp") == SPELL_ILLUSION_ROOM and start <= rec["t"] <= start + PORTAL_DESPAWN_MS}
        missed = [guid for guid in plan if guid not in took]

        print(f"  wave {ordinal}: {len(took)} of {len(PORTAL_SPOTS)} portals taken,"
              f" {len(plan)} bots assigned a spot, plan was {stale:.0f} s old")
        if missed:
            print(f"    assigned and never went: {', '.join(sorted(trace.name(guid) for guid in missed))}")


def show_rooms(trace: Trace, waves: list[tuple[int, int]]) -> None:
    """What each wave did with the room it landed in. A wave that never reaches `fighting` is a
    room the raid stood in rather than cleared, and the gap between arriving and the first cast at a
    tentacle is where that goes wrong: every tentacle is disguised on arrival, so a bot with none in
    sight has nothing to reveal one with and the room reads as already clear."""
    if not waves:
        return

    tentacles = set()
    for entry in (NPC_INFLUENCE_TENTACLE,) + NPC_TENTACLE_DISGUISES:
        tentacles |= guids_of_entry(trace, entry)

    ends = [start for start, _ in waves[1:]] + [trace.records[-1].get("t", 0) if trace.records else 0]
    print()
    for (start, ordinal), stop in zip(waves, ends):
        window = [rec for rec in notes(trace, "yogg.room") if start <= rec["t"] <= stop]
        rooms = collections.Counter(str(rec.get("txt", "")) for rec in window
                                    if str(rec.get("txt", "")) in ("stormwind", "icecrown", "chamber"))
        if not rooms:
            print(f"  wave {ordinal}: nobody reached an illusion room")
            continue

        room, bots = rooms.most_common(1)[0]
        entered = min(rec["t"] for rec in window if str(rec.get("txt", "")) == room)

        states = {str(rec.get("txt", "")) for rec in notes(trace, "yogg.roomstate")
                  if start <= rec["t"] <= stop}
        engaged = [rec["t"] for rec in trace.of("cast")
                   if start <= rec["t"] <= stop and rec.get("tgt") in tentacles]

        if engaged:
            delay = f"{(min(engaged) - entered) / 1000.0:.1f} s to the first cast at a tentacle"
        else:
            delay = "NEVER cast at a tentacle"
        cleared = "cleared" if states & {"tobrain", "atbrain"} else "not cleared"
        print(f"  wave {ordinal} {room:10} {bots} bots, entered {clock(entered)}, {delay}, {cleared}")
        print(f"    states: {', '.join(sorted(states)) or 'none'}")

    show_frozen(trace)
    show_vetoes(trace)


def show_frozen(trace: Trace) -> None:
    """The longest a bot went holding a target and casting nothing at all. This is what a vetoed
    walk looks like from outside: somewhere to be, no way to get there, and nothing moving the bot
    instead. One pull left five of six bots in a room doing it for fifty seconds while the tentacle
    they were pointed at sat at 87%."""
    frozen = idle_windows(trace, IDLE_MS)
    if not frozen:
        print(f"\n  nobody held a target for {IDLE_MS / 1000:.0f} s without casting")
        return

    print(f"\n  held a target and cast nothing for over {IDLE_MS / 1000:.0f} s:")
    for window in sorted(frozen, key=lambda w: (w["quiet"], w["start"], w["guid"]), reverse=True)[:8]:
        guid = window["guid"]
        print(f"    {trace.name(guid):14} {trace.role(guid):6} {window['quiet'] / 1000.0:5.1f} s"
              f" from {clock(window['start'])}")


def show_vetoes(trace: Trace) -> None:
    """Which multiplier zeroed which action, and how often. A veto is cheap to write and easy to
    get wrong: one that zeroes a walk with nothing walking in its place is a bot standing still, and
    this says so long before the room view does."""
    rows = veto_counts(trace)
    if not rows:
        return

    print("\n  multiplier vetoes:")
    for (multiplier, action), count in rows.most_common(10):
        print(f"    {count:5}  {multiplier} -> {action}")


def position_at(trace: Trace, guid: int, when: int):
    """Ground position only: everything here measures across a flat platform."""
    spot = at(trace, guid, when)
    return (spot[0], spot[1]) if spot else None


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
        # Nothing in the trace says a creature died: the death stream is roster only, and so is dmg,
        # so counting deaths there always reads zero. The snapshot does carry the last
        # health each one was sampled at before it stopped appearing - a few percent for one the raid
        # killed, near full for one the phase despawned - so that is what gets reported.
        last = [rows[-1][1] for rows in tracks(trace, tentacles, ("t", "hp")).values() if rows]
        spent = sum(1 for hp in last if hp <= TENTACLE_SPENT_PCT)
        print(f"  Influence Tentacles seen: {len(tentacles)}, {spent} last seen under "
              f"{TENTACLE_SPENT_PCT:.0f}% health and gone")

    gaze = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_LUNATIC_GAZE_SKULL]
    if gaze:
        total = sum(rec.get("a", 0) for rec in gaze)
        print(f"  Lunatic Gaze     : {len(gaze)} hits for {total:,} damage")
    skulls = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.skull"))
    if skulls:
        near = []
        skull_guids = guids_of_entry(trace, NPC_LAUGHING_SKULL)
        for rec in notes(trace, "yogg.skull"):
            here = position_at(trace, rec.get("g", 0), rec["t"])
            seen = [position_at(trace, guid, rec["t"]) for guid in skull_guids]
            far = [math.dist(here, spot) for spot in seen if here and spot]
            if far:
                near.append(min(far))
        tail = ""
        if near:
            out_of_range = sum(1 for gap in near if gap > LAUGHING_SKULL_RADIUS)
            tail = (f", nearest skull median {statistics.median(near):.1f} yd,"
                    f" {out_of_range} of {len(near)} flips with none inside {LAUGHING_SKULL_RADIUS:.0f}")
        print(f"  skulls in arc    : {dict(skulls)}{tail}")

    reach = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.tentacle"))
    if reach:
        print(f"  tentacle reach   : {dict(reach)}")

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

    # Induce Madness leaves no damage row: it strips all 100 Sanity from whoever is still below the
    # platform when its 60 s cast ends, and Insane kills them when the charm falls off a minute later.
    # So the Insane aura is the record of who failed to get out, and the only one there is.
    caught = sorted({rec.get("d", 0) for rec in trace.of("aura")
                     if rec.get("sp") == SPELL_INSANE and not rec.get("r")},
                    key=lambda guid: trace.name(guid))
    if caught:
        names = ", ".join(f"{trace.name(guid)}({trace.role(guid)})" for guid in caught)
        print(f"  caught by it     : {len(caught)} went Insane - {names}")


def aura_windows(trace: Trace, spell: int) -> list[tuple[int, int, int]]:
    """(guid, applied, removed) per aura window, closed at the last record when it never lifts."""
    end = trace.records[-1].get("t", 0) if trace.records else 0
    open_at: dict[int, int] = {}
    out: list[tuple[int, int, int]] = []
    for rec in sorted((r for r in trace.of("aura") if r.get("sp") == spell), key=lambda r: r["t"]):
        guid = rec.get("d", 0)
        if not rec.get("r"):
            open_at.setdefault(guid, rec["t"])
        elif guid in open_at:
            out.append((guid, open_at.pop(guid), rec["t"]))
    out.extend((guid, start, end) for guid, start in open_at.items())
    return sorted(out, key=lambda w: w[1])


def show_phase2(trace: Trace) -> None:
    """Everything phase 2 is decided by that is not the brain room: who gets held by a Constrictor and
    for how long, whether Brain Linked pairs ever close, and how often two nodes trade a bot."""
    print("PHASE 2")

    roster = roster_guids(trace)
    squeeze = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_SQUEEZE]
    grabs = aura_windows(trace, SPELL_SQUEEZE)
    rescues = [rec for rec in trace.of("cast") if rec.get("sp") in SPELL_HAND_OF_PROTECTION]

    if grabs:
        total = sum(rec.get("a", 0) for rec in squeeze)
        longest = max((stop - start) / 1000.0 for _, start, stop in grabs)
        print(f"  Constrictor grabs  : {len(grabs)}, {total:,} damage, longest {longest:.1f} s")
        for guid, start, stop in grabs:
            took = sum(rec.get("a", 0) for rec in squeeze
                       if rec.get("d") == guid and start <= rec["t"] <= stop + 500)
            freed = any(rec.get("tgt") == guid and start <= rec["t"] <= stop + 500 for rec in rescues)
            print(f"    {trace.name(guid):14} {trace.role(guid):6} {clock(start)} -> {clock(stop)}"
                  f"  ({(stop - start) / 1000.0:5.1f} s) {took:>7,}"
                  f"  {'Hand of Protection' if freed else 'rode it out'}")
        print(f"    rescues cast     : {len(rescues)}")
    else:
        print("  no Squeeze rows - nothing was ever grabbed")

    links = aura_windows(trace, SPELL_BRAIN_LINK)
    link_dmg = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_BRAIN_LINK_DAMAGE]
    if links:
        print(f"  Brain Link         : {len(links)} links, "
              f"{sum(rec.get('a', 0) for rec in link_dmg):,} damage")
        for guid, start, stop in links:
            # The aura holds the partner's GUID privately, but it casts 63803 (apart) or 63804
            # (together) on that partner every second, so the cast names the pair exactly. Damage rows
            # only name it while the two are already too far apart, which is the half that matters
            # least.
            partner = next((rec.get("tgt") for rec in trace.of("cast")
                            if rec.get("sp") in (SPELL_BRAIN_LINK_DAMAGE, SPELL_BRAIN_LINK_OK)
                            and rec.get("s") == guid and rec.get("tgt") not in (None, 0, guid)
                            and start <= rec["t"] <= stop), None)
            if partner is None:
                print(f"    {clock(start)} -> {clock(stop)}  {trace.name(guid):14}"
                      f"  never ticked, so the pair stayed inside {BRAIN_LINK_RANGE:.0f} yd")
                continue

            gaps = []
            for snap in trace.of("snap"):
                if not start <= snap["t"] <= stop:
                    continue
                seen = {row[0]: (row[1], row[2]) for row in snap.get("u", [])
                        if row[0] in (guid, partner)}
                if len(seen) == 2:
                    gaps.append(math.dist(seen[guid], seen[partner]))
            if not gaps:
                continue
            over = sum(1 for gap in gaps if gap > BRAIN_LINK_RANGE)
            print(f"    {clock(start)} -> {clock(stop)}  {trace.name(guid)}({trace.role(guid)})"
                  f" + {trace.name(partner)}({trace.role(partner)})"
                  f"  gap median {statistics.median(gaps):.1f} yd, {over} of {len(gaps)} samples over"
                  f" {BRAIN_LINK_RANGE:.0f}")
    else:
        print("  no 63802 rows - nothing was ever linked")

    show_handovers(trace)


def show_handovers(trace: Trace) -> None:
    """Two nodes trading a bot. A successful move handed to a different node inside two seconds is one
    of them undoing the other, and it is what a raid strategy with no movement guard looks like."""
    spans = [(start, stop) for phase, start, stop in phase_spans(trace) if phase == 2]
    if not spans:
        return

    roster = roster_guids(trace)
    last: dict[int, tuple[int, str]] = {}
    per_bot: dict[int, int] = collections.Counter()
    pairs: dict[tuple[str, str], int] = collections.Counter()
    issued: dict[str, int] = collections.Counter()
    for rec in trace.of("move"):
        if not any(start <= rec["t"] <= stop for start, stop in spans):
            continue
        guid, node = rec.get("g", 0), str(rec.get("by", "?"))
        if guid not in roster:
            continue
        issued[node] += 1
        if not rec.get("ok"):
            continue
        before = last.get(guid)
        last[guid] = (rec["t"], node)
        if before and before[1] != node and rec["t"] - before[0] <= 2000:
            per_bot[guid] += 1
            pairs[tuple(sorted((before[1], node)))] += 1

    if not per_bot:
        return

    by_role: dict[str, list[int]] = collections.defaultdict(list)
    for guid, count in per_bot.items():
        by_role[trace.role(guid)].append(count)
    parts = ", ".join(f"{role} {sum(v) / len(v):.0f}/bot" for role, v in sorted(by_role.items()))
    print(f"  node handovers     : {sum(per_bot.values())} in phase 2 ({parts})")
    for (one, two), count in pairs.most_common(3):
        print(f"    {one} <-> {two}: {count}")
    for node, count in issued.most_common(4):
        print(f"    issued {count:5} by {node}")


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
            per_role[trace.role(rec.get("g", 0))][str(rec.get("txt", ""))] += 1
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
        by_role = collections.Counter(trace.role(rec.get("g", 0)) for rec in deaths)
        print(f"  deaths by role: {dict(by_role)}")

    lanes = [rec for rec in trace.of("haz") if rec.get("shape") == "wedge"]
    circles = [rec for rec in trace.of("haz") if rec.get("sp") == SPELL_KNOCK_BACK]
    print(f"  haz rows: {len(lanes)} Crush wedges, {len(circles)} body rings")

    guard = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.petguard"))
    if guard:
        print(f"  pet guard        : {dict(guard)}")

    detour = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "yogg.detour"))
    if detour:
        print(f"  body detour      : {dict(detour)}")

    show_launches(trace)
    show_crush_melee(trace)


def show_launches(trace: Trace) -> None:
    """Who the body actually threw, and what walked them in. 64020 deals no damage, so a launch
    only shows up as the arc: on the floor one sample, well above it the next. The distance that
    matters is the one at the last grounded sample, not the first airborne one, which is already a
    third of a second into the flight.

    The walk is the other half. Every other read of the ring asks where the bot is standing, so the
    one thing none of them can catch is a walk whose destination is the body - which is what
    ReachCombatTo produces for a target on the far side, since it shortens its path to halfway."""
    roster = roster_guids(trace)
    casts = sorted(rec["t"] for rec in trace.of("cast") if rec.get("sp") == SPELL_KNOCK_BACK)
    if not casts:
        return

    tracked: dict[int, list[tuple]] = collections.defaultdict(list)
    for snap in trace.of("snap"):
        for row in snap.get("u", []):
            if row[0] in roster:
                tracked[row[0]].append((snap["t"], row[1], row[2], row[3]))

    walks = [rec for rec in trace.of("move") if rec.get("g") in roster]

    launches = []
    for guid, rows in tracked.items():
        for before, after in zip(rows, rows[1:]):
            if not (before[3] <= FLOOR_Z and after[3] > FLOOR_Z + KNOCKBACK_ARC_Z):
                continue
            if not any(before[0] <= when <= after[0] + 100 for when in casts):
                continue
            # The last walk before the throw that was aimed into the ring, which is the one that put
            # the bot there. Not simply the last walk: by the time the pulse lands a dodge node has
            # usually already issued the walk back out, and naming that one blames the rescue.
            blame = None
            for rec in walks:
                if rec.get("g") != guid or rec["t"] > after[0]:
                    continue
                if math.dist((rec.get("x", 0), rec.get("y", 0)), BODY) <= KNOCKBACK_RADIUS:
                    blame = rec
            aimed = math.dist((blame.get("x", 0), blame.get("y", 0)), BODY) if blame else -1.0
            launches.append((after[0], guid, math.dist((before[1], before[2]), BODY),
                             blame.get("by", "?") if blame else "nothing walked it in", aimed))

    if not launches:
        print("  nothing was thrown by the body ring")
        return

    print(f"  thrown by the body ring: {len(launches)}")
    for when, guid, flat, by, aimed in sorted(launches)[:10]:
        where = f" -> {aimed:.1f} yd out" if aimed >= 0 else ""
        print(f"    {clock(when):>10} {trace.name(guid):14} {trace.role(guid):6}"
              f" from {flat:5.1f} yd  walked in by {by}{where}")


def show_crush_melee(trace: Trace) -> None:
    """What stood inside a Crusher's melee range. Crush is a 100% proc on the tentacle's own
    white swing and UpdateAI will not swing at a victim out of melee range, so whatever is in there is
    what fires every cone the raid eats - and a pet counts."""
    crushers = guids_of_entry(trace, NPC_CRUSHER_TENTACLE)
    if not crushers:
        return

    roster = roster_guids(trace)
    closest: dict[int, float] = {}
    for snap in trace.of("snap"):
        spots = {row[0]: (row[1], row[2], row[5]) for row in snap.get("u", [])}
        live = [spot for guid, spot in spots.items() if guid in crushers and spot[2] > 0]
        if not live:
            continue
        for guid, spot in spots.items():
            if guid not in roster and guid not in trace.owners:
                continue
            gap = min(math.dist((spot[0], spot[1]), (other[0], other[1])) for other in live)
            closest[guid] = min(closest.get(guid, gap), gap)

    inside = sorted((gap, guid) for guid, gap in closest.items() if gap <= CRUSHER_MELEE_RANGE)
    if not inside:
        print(f"  nothing came inside {CRUSHER_MELEE_RANGE:.0f} yd of a Crusher")
        return

    # Standing there is half of it; being pointed at it is what keeps a pet coming back, since PetAI
    # re-picks from its own attacker and its owner's victim the moment it has no target of its own.
    aimed: dict[int, int] = collections.Counter()
    for snap in trace.of("snap"):
        for row in snap.get("u", []):
            if len(row) > 7 and row[7] in crushers and row[0] in trace.owners:
                aimed[row[0]] += 1

    print(f"  inside {CRUSHER_MELEE_RANGE:.0f} yd of a Crusher ({len(inside)}):")
    for gap, guid in inside[:8]:
        kind = trace.role(guid) if guid in roster else "pet"
        print(f"    {trace.name(guid):28} {kind:6} {gap:5.1f} yd")

    if aimed:
        print("  pets with a Crusher targeted:")
        for guid, count in aimed.most_common(6):
            print(f"    {trace.name(guid):28} {count:5} samples")


# What each source takes off a 25-man Sanity bar, from spell_yogg_saron_sanity_reduce. Induce Madness
# is not in here: it leaves no damage or aura row of its own, so the Insane aura is the only record
# that it landed, and what it takes is the whole bar.
SANITY_COSTS = {65301: 12, 63830: 3, 63881: 3, 63803: 2, 64168: 2, 64164: 4}
SANITY_NAMES = {65301: "Psychosis", 63830: "Malady of the Mind", 63881: "Malady trigger",
                63803: "Brain Link", 64168: "Lunatic Gaze (skull)", 64164: "Lunatic Gaze (Yogg)"}


def show_sanity_sources(trace: Trace) -> None:
    """Where the Sanity went, reconstructed from the spells that take it. Deduplicated on
    (time, spell, target) because one landed hit can leave both a dmg and an aura row."""
    roster = roster_guids(trace)
    lost: dict[int, int] = collections.Counter()
    seen: set[tuple[int, int, int]] = set()
    for rec in trace.of("dmg", "aura"):
        spell, victim = rec.get("sp"), rec.get("d")
        if spell not in SANITY_COSTS or victim not in roster:
            continue
        if rec.get("e") == "aura" and rec.get("r"):
            continue
        key = (rec["t"], spell, victim)
        if key in seen:
            continue
        seen.add(key)
        lost[spell] += SANITY_COSTS[spell]

    insane = sorted({rec.get("d", 0) for rec in trace.of("aura")
                     if rec.get("sp") == SPELL_INSANE and not rec.get("r")})
    madness = len(insane) * 100

    total = sum(lost.values()) + madness
    if not total:
        return

    print("  where it went:")
    rows = [("Induce Madness", madness)] if madness else []
    rows += [(SANITY_NAMES[spell], stacks) for spell, stacks in lost.items()]
    for label, stacks in sorted(rows, key=lambda row: -row[1]):
        print(f"    {label:22} {stacks:5}  {stacks * 100.0 / total:4.1f}%")
    print(f"    {'total':22} {total:5}")
    if insane:
        print(f"    Induce Madness is {len(insane)} bots x 100, every one still below the platform"
              f" when the cast ended")


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
        print(f"  {trace.name(guid):18} {trace.role(guid):7} low {low[guid]:3}")

    insane = [guid for guid, stacks in low.items() if stacks <= 0]
    if insane:
        print(f"  went Insane: {', '.join(trace.name(g) for g in insane)}")

    show_sanity_sources(trace)


def show_threat(trace: Trace) -> None:
    """Who the Guardians were actually beating on, and what the raid spent trying to change that.

    The answer is not "nothing was tried": Misdirection goes to the tank and the tank taunts. It is
    that one tank's taunt budget is roughly one cast per Guardian death, against a room where every
    Guardian carries its own threat table from SetInCombatWithZone, so the question worth asking is
    whether the budget was spent on the Guardian whose death location decides the phase.
    """
    print("THREAT")
    spans = phase_spans(trace)
    p1_end = next((stop for phase, _, stop in spans if phase == 1), None)
    if p1_end is None:
        print("  no phase 1 in this trace")
        return

    guardians = guids_of_entry(trace, NPC_GUARDIAN)
    roster = roster_guids(trace)

    held, _ = threat_share(trace, guardians, lambda when: 0 <= when <= p1_end)
    if sum(held.values()):
        show_share(held, "Guardian time on target")
    else:
        print("  no live Guardian was ever sampled with a target")

    redirects = [rec for rec in trace.of("cast")
                 if rec.get("sp") in REDIRECT_SPELLS and 0 <= rec["t"] <= p1_end]
    if redirects:
        print(f"\n  redirects: {len(redirects)}")
        for rec in redirects:
            target = rec.get("tgt", 0)
            role = trace.role(target) if target in roster else "?"
            print(f"    {clock(rec['t']):>9}  {REDIRECT_SPELLS[rec['sp']]:<20}"
                  f" {trace.name(rec.get('s', 0)):<14} -> {trace.name(target)} ({role})")

    # A taunt is only worth its cooldown if it lands on the Guardian the raid is killing, which is the
    # lowest-health one. Ranking it against the living Guardians is the whole measurement.
    taunts = [rec for rec in trace.of("cast")
              if rec.get("sp") in TAUNT_SPELLS and 0 <= rec["t"] <= p1_end
              and rec.get("s") in roster]
    if not taunts:
        print("\n  no single-target taunt was cast in phase 1")
        return

    frames = [snap for snap in trace.of("snap") if 0 <= snap["t"] <= p1_end]

    print(f"\n  taunts: {len(taunts)}   (rank 1 = the lowest-health Guardian, the one the raid is on)")
    on_focus = 0
    for rec in taunts:
        target = rec.get("tgt", 0)
        snap = min(frames, key=lambda s: abs(s["t"] - rec["t"])) if frames else None
        live = sorted((row[5], row[0]) for row in (snap.get("u", []) if snap else [])
                      if row[0] in guardians and row[5] > 0)
        rank = next((i + 1 for i, (_, guid) in enumerate(live) if guid == target), 0)
        if rank == 1:
            on_focus += 1
        where = position_at(trace, target, rec["t"])
        radius = f"{math.dist(where, BODY):5.1f}" if where else "    ?"
        print(f"    {clock(rec['t']):>9}  {TAUNT_SPELLS[rec['sp']]:<18}"
              f" rank {rank or '?'}/{len(live):<2}  target {radius} yd out")
    print(f"    landed on the focus Guardian: {on_focus} of {len(taunts)}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("file", type=pathlib.Path)
    parser.add_argument("--phases", action="store_true", help="phase timeline and the pre-pull window")
    parser.add_argument("--clouds", action="store_true", help="cloud-orbit exposure per role")
    parser.add_argument("--portals", action="store_true", help="portal waves and assignments")
    parser.add_argument("--phase2", action="store_true", help="Constrictor, Brain Link, node handovers")
    parser.add_argument("--brain", action="store_true", help="brain room, the Brain, skulls")
    parser.add_argument("--crush", action="store_true", help="Crush, knockback and Death Rays")
    parser.add_argument("--sanity", action="store_true", help="Sanity minima")
    parser.add_argument("--threat", action="store_true", help="who the Guardians were on, and taunts")
    args = parser.parse_args()

    if not args.file.is_file():
        print(f"no such trace: {args.file}", file=sys.stderr)
        return 1

    trace = Trace(args.file)
    picked = (args.phases, args.clouds, args.threat, args.portals, args.phase2, args.brain,
              args.crush, args.sanity)
    every = not any(picked)

    show_banner(trace)
    for wanted, section in zip(picked, (show_phases, show_clouds, show_threat, show_portals,
                                        show_phase2, show_brain, show_crush, show_sanity)):
        if wanted or every:
            print()
            section(trace)
    return 0


if __name__ == "__main__":
    sys.exit(main())
