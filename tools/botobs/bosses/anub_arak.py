#!/usr/bin/env python3
"""Score an Anub'arak pull from a RaidObs trace: phases, the landing, the spike, spheres, burrowers.

    anub_arak.py <file>              every section
    anub_arak.py <file> --phases     anub.phase spans, his health at each edge, deaths per phase
    anub_arak.py <file> --pull       who was locked out or never landed, and stage 9 to the pull
    anub_arak.py <file> --spike      every Pursued by Anub'arak window: kiter, kite branch, outcome
    anub_arak.py <file> --spheres    flying spheres and patches over time, patches the spike used
    anub_arak.py <file> --burrowers  each burrower: life, submerges, time on Permafrost, Shadow Strike
    anub_arak.py <file> --threat     who he hit in each surfaced window, time to a tank, taunts
    anub_arak.py <file> --swarm      phase 3: Leeching Swarm per bot, lust, Penetrating Cold, deaths

What the generic views get wrong here, and what this reads instead:

- **Two units are called Anub'arak.** The Pursuing Spike (34660) carries his name and the boss flag,
  so anything keyed on the name or the flag can pick the spike. He is keyed on entry 34564 here.
- **The `anub-arak` slug is also Azjol-Nerub's boss**, so the banner checks map 649 as well.
- **One entry, two states.** A flying Frost Sphere hovers at z 155.67 and a patch sits at 142.7, and
  neither carries Permafrost itself, so spheres are split by height and Permafrost is read off
  `snap.hz`.
- **Creature auras are never recorded**, so a burrower's submerge only shows as its health going back
  to full, and its time on Permafrost as its distance from a Permafrost centre.
- **`anub.kite`, `anub.dodge`, `anub.sphere` and the other derived keys are per bot and change only**:
  a bot that takes the same branch twice writes one row. `anub.kite` goes back to `none` when the mark
  moves on, so a window never opens on the branch the last one ended with.
"""
from __future__ import annotations

import bisect
import collections
import pathlib
import statistics
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of, map_of  # noqa: E402
from raidobs.geometry import anchor, dist2, frames, guids_of_entry  # noqa: E402
from raidobs.probes import emitted_keys, latch_spans, silent_keys  # noqa: E402
from raidobs.space import threat_share  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, notes, roster_guids  # noqa: E402

MAP_TOC = 649
DIFFICULTIES = {0: "10N", 1: "25N", 2: "10H", 3: "25H"}
HEROIC = (2, 3)

NPC_ANUBARAK = 34564
NPC_FROST_SPHERE = 34606
NPC_BURROWER = 34607

SPELL_MARK = 67574
SPELL_SHADOW_STRIKE = 66134
SPELL_LEECHING_SWARM_DMG = 66240
# 10N, 25N, 10H, 25H
IMPALE = (65919, 67858, 67859, 67860)
PERMAFROST = (66193, 67855, 67856, 67857)
PENETRATING_COLD = (66013, 67700, 68509, 68510)
LEECHING_SWARM = (66118, 67630, 68646, 68647)
# Bloodlust, Heroism
LUST_SPELLS = (2825, 32182)
# what CastClassTaunt casts
TAUNT_SPELLS = {355: "Taunt", 62124: "Hand of Reckoning", 56222: "Dark Command", 6795: "Growl"}

PHASE_LABELS = {0: "none", 1: "P1", 2: "P2", 3: "P3"}

# Read from the source so a retune shows up here without a second edit.
ROOM_CENTER = anchor("ANUBARAK_ROOM_CENTER")
# Permafrost's target search adds only the target's size. A burrower's display has no combat reach, so
# it gets the core's 0.389 default, not the 1.5 of ANUBARAK_PERMAFROST_SLOW_REACH's player.
BURROWER_PERMAFROST_REACH = 6.0 + 0.389

# Not constants in the module. The Web Door is GO 195485 and shuts at the engage. Before the floor
# breaks the raid stands near z 395, the pit floor is 142. Spheres fly at 155.67 and land at 142.7.
WEB_DOOR_X = 661.6
LANDED_Z = 200.0
SPHERE_FLYING_Z = 150.0
SPHERE_PATCH_Z = 146.0

# the spike's fail despawns the patch 1.5 s after the mark comes off
CONSUMED_MS = 3000
# the spike despawns 2 s before he turns selectable, so a mark ended by the emerge lands near that edge
EMERGE_SLACK_MS = 3000
IMPALE_SLACK_MS = 500
SUBMERGE_GAP_MS = 5000
NEAR_FULL = 95.0
SUBMERGE_JUMP = 15.0
SHADOW_STRIKE_CAST_MS = 8000
LAND_SLACK_MS = 1000
# a row this far after the asked time still answers it when nothing was sampled before
NEAR_MS = 2000
# one snapshot gap longer than this is a hole in the trace, not time spent anywhere
MAX_STEP_MS = 2000
TIMELINE_SHOWN = 30


def pull_end(trace: Trace) -> int:
    ends = trace.of("end")
    if ends:
        return ends[-1]["t"]
    return trace.records[-1].get("t", 0) if trace.records else 0


def boss_guid(trace: Trace) -> int | None:
    guids = sorted(guids_of_entry(trace, NPC_ANUBARAK))
    return guids[0] if guids else None


def is_heroic(trace: Trace) -> bool:
    return trace.header.get("diff") in HEROIC


def short(trace: Trace, guid: int) -> str:
    """A creature's name repeats across every burrower and sphere, so those print by counter."""
    if guid in trace.roles:
        return trace.name(guid)
    return f"#{guid & 0xFFFFFFFF}"


def per_bot(trace: Trace, key: str) -> dict[int, list[tuple[int, str]]]:
    out: dict[int, list[tuple[int, str]]] = collections.defaultdict(list)
    for rec in notes(trace, key):
        out[rec.get("g", 0)].append((rec["t"], str(rec.get("txt", ""))))
    return out


def value_counts(trace: Trace, key: str) -> dict[int, collections.Counter]:
    """Rows per value per bot. A derived key writes one row per change, so this counts entries."""
    return {guid: collections.Counter(value for _, value in rows)
            for guid, rows in per_bot(trace, key).items()}


def show_counts(trace: Trace, key: str) -> None:
    counts = value_counts(trace, key)
    if not counts:
        print(f"  {key}: no rows")
        return
    print(f"  {key}: " + "; ".join(
        f"{trace.name(guid)} " + ", ".join(f"{value} {n}" for value, n in counter.most_common())
        for guid, counter in sorted(counts.items(), key=lambda item: trace.name(item[0]))))


def aura_spans(trace: Trace, spells) -> dict[int, list[tuple[int, int, bool]]]:
    """Per target, `(apply, remove, closed)`. One still up when the file ends runs to the end, open."""
    end = pull_end(trace)
    opened: dict[int, int] = {}
    out: dict[int, list[tuple[int, int, bool]]] = collections.defaultdict(list)
    for rec in trace.of("aura"):
        if rec.get("sp") not in spells:
            continue
        guid = rec.get("d", 0)
        if rec.get("r"):
            if guid in opened:
                out[guid].append((opened.pop(guid), rec["t"], True))
        elif guid not in opened:
            opened[guid] = rec["t"]
    for guid, start in opened.items():
        out[guid].append((start, max(start, end), False))
    return out


class Samples:
    """Snapshot rows by guid, plus each snapshot's Permafrost centres and sphere counts."""

    def __init__(self, trace: Trace):
        spheres = guids_of_entry(trace, NPC_FROST_SPHERE)
        self.stamps: list[int] = []
        self.frost: list[list[tuple[float, float]]] = []
        self.spheres: list[tuple[int, int, int]] = []
        self.rows: dict[int, tuple[list[int], list[list]]] = {}
        for snap in frames(trace):
            when = snap["t"]
            self.stamps.append(when)
            frost = [(hz[1], hz[2]) for hz in snap.get("hz", []) if len(hz) >= 3 and hz[0] in PERMAFROST]
            self.frost.append(frost)
            flying = patches = 0
            for row in snap.get("u", []):
                stamps, rows = self.rows.setdefault(row[0], ([], []))
                stamps.append(when)
                rows.append(row)
                if row[0] in spheres and len(row) > 3:
                    if row[3] > SPHERE_FLYING_Z:
                        flying += 1
                    elif row[3] < SPHERE_PATCH_Z:
                        patches += 1
            self.spheres.append((flying, patches, len(frost)))

    def track(self, guid) -> tuple[list[int], list[list]]:
        return self.rows.get(guid, ([], []))

    def row(self, guid, when: int):
        """The last row at or before `when`, else the first within NEAR_MS after it."""
        stamps, rows = self.track(guid)
        index = bisect.bisect_right(stamps, when)
        if index:
            return rows[index - 1]
        if stamps and stamps[0] - when <= NEAR_MS:
            return rows[0]
        return None

    def recent(self, guid, when: int):
        """The last row at or before `when`, only if it is no older than one snapshot gap."""
        stamps, rows = self.track(guid)
        index = bisect.bisect_right(stamps, when)
        if index and when - stamps[index - 1] <= MAX_STEP_MS:
            return rows[index - 1]
        return None

    def between(self, guid, low: int, high: int) -> list[list]:
        stamps, rows = self.track(guid)
        return rows[bisect.bisect_left(stamps, low):bisect.bisect_right(stamps, high)]

    def index_at(self, when: int) -> int:
        return bisect.bisect_right(self.stamps, when) - 1

    def frost_at(self, when: int) -> list[tuple[float, float]]:
        index = self.index_at(when)
        return self.frost[index] if index >= 0 else []


def health(samples: Samples, guid: int | None, when: int) -> float | None:
    row = samples.row(guid, when) if guid else None
    return row[5] if row else None


def missing_probes(trace: Trace) -> list[str]:
    return [key for key, _, _ in silent_keys(emitted_keys(trace), encounter_of(trace))]


def is_toc_anubarak(trace: Trace) -> bool:
    return (encounter_of(trace) == "anub-arak"
            and trace.header.get("map", map_of(trace.path)) == MAP_TOC)


def show_banner(trace: Trace) -> None:
    end = pull_end(trace)
    outcome = trace.of("end")[-1].get("out", "?") if trace.of("end") else "no end record"
    difficulty = DIFFICULTIES.get(trace.header.get("diff"), "?")
    print(f"{encounter_of(trace)} {difficulty}  {outcome} at {clock(end)}"
          f"  {len(combat_deaths(trace))} death(s)")

    # the probe check matches keys to the encounter, so on another boss it would pass on nothing
    if not is_toc_anubarak(trace):
        print("  not a Trial of the Crusader Anub'arak pull, every section below reads empty")
        return

    gone = missing_probes(trace)
    print("all anub.* and toc.* probes present" if not gone
          else f"probes absent from this trace: {', '.join(gone)}")


# --------------------------------------------------------------------------------------------- phases

def phase_spans(trace: Trace) -> list[tuple[int, int, int, str]]:
    """`(start, stop, phase, label)` per stretch `anub.phase` held one value. A phase that comes round
    again gets a letter, P1b, P2b, so each span keeps its own deaths."""
    seen: collections.Counter = collections.Counter()
    out = []
    for value, start, stop in latch_spans(trace, "anub.phase", pull_end(trace)):
        try:
            phase = int(value)
        except ValueError:
            continue
        label = PHASE_LABELS.get(phase, value)
        seen[label] += 1
        if phase and seen[label] > 1:
            label += chr(ord("a") + seen[label] - 1)
        out.append((start, max(start, stop), phase, label))
    return out


def phase_at(spans, when: int) -> str:
    for start, stop, _, label in spans:
        if start <= when < stop:
            return label
    if spans and when >= spans[-1][1]:
        return spans[-1][3]
    return "pre"


def phase_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    samples = samples or Samples(trace)
    boss = boss_guid(trace)
    spans = phase_spans(trace)
    dead: dict[str, list[dict]] = collections.defaultdict(list)
    for rec in combat_deaths(trace):
        dead[phase_at(spans, rec["t"])].append(rec)
    return [{"label": label, "phase": phase, "start": start, "stop": stop,
             "hp": health(samples, boss, start), "deaths": dead.get(label, [])}
            for start, stop, phase, label in spans]


def show_phases(trace: Trace) -> None:
    print("PHASES")
    samples = Samples(trace)
    rows = phase_rows(trace, samples)
    if not rows:
        print("  no anub.phase rows")
        return

    for row in rows:
        hp = "-" if row["hp"] is None else f"{row['hp']:5.1f}%"
        print(f"  {row['label']:5} {clock(row['start']):>9} .. {clock(row['stop']):>9}"
              f"  {(row['stop'] - row['start']) / 1000:6.1f} s  boss {hp:>6}  {len(row['deaths']):2} dead")
        for rec in row["deaths"]:
            blow = rec.get("blow") or [0, 0]
            print(f"          {clock(rec['t'])}  {trace.name(rec['g'])[:14]:14} {trace.role(rec['g']):6}"
                  f"  by {trace.name(rec.get('killer'))} ({blow[1] if len(blow) > 1 else 0})")

    submerges = sum(1 for row in rows if row["phase"] == 2)
    final = health(samples, boss_guid(trace), pull_end(trace))
    print(f"\n  {submerges} submerge(s)"
          + ("" if final is None else f", boss {final:.1f}% at {clock(pull_end(trace))}"))


# ----------------------------------------------------------------------------------------------- pull

def stage_nine(trace: Trace) -> tuple[int | None, bool]:
    """When `toc.progress` first read 9, and whether another stage came before it in this trace. The
    container restates its value when a trace opens, so a 9 with nothing before it may be older."""
    rows = sorted(notes(trace, "toc.progress"), key=lambda rec: rec["t"])
    for index, rec in enumerate(rows):
        if str(rec.get("txt", "")) == "9":
            return rec["t"], index > 0
    return None, False


def pull_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Roster members west of the Web Door or above the pit at the pull, and when each first stood in
    the room after it. Nobody sampled near the pull is listed too."""
    # the door and the pit heights only mean anything in this room
    if not is_toc_anubarak(trace):
        return []
    samples = samples or Samples(trace)
    out = []
    for guid in sorted(roster_guids(trace), key=trace.name):
        row = samples.row(guid, 0)
        if row is None:
            out.append({"guid": guid, "why": "unsampled", "spot": None, "arrived": None})
            continue
        locked, high = row[1] < WEB_DOOR_X, row[3] > LANDED_Z
        if not (locked or high):
            continue
        stamps, rows = samples.track(guid)
        start = bisect.bisect_left(stamps, 0)
        arrived = next((stamps[i] for i in range(start, len(stamps))
                        if rows[i][1] >= WEB_DOOR_X and rows[i][3] <= LANDED_Z), None)
        out.append({"guid": guid, "why": "locked out" if locked else "not landed",
                    "spot": tuple(row[1:4]), "arrived": arrived})
    return out


def show_pull(trace: Trace) -> None:
    print("PULL")
    if not is_toc_anubarak(trace):
        print("  not an Anub'arak pull")
        return

    when, edge = stage_nine(trace)
    if when is None:
        print("  no toc.progress 9 in this trace")
    elif edge:
        print(f"  stage 9 at {clock(when)}, {-when / 1000:.1f} s before the pull")
    else:
        print(f"  toc.progress read 9 on its first row at {clock(when)}: a restate, the floor broke"
              " before the trace opened")

    rows = pull_rows(trace)
    if not rows:
        print("  everyone stood in the pit at the pull")
        return
    print(f"  west of x {WEB_DOOR_X} or above z {LANDED_Z:.0f} at the pull:")
    for row in rows:
        spot = "-" if row["spot"] is None else ", ".join(f"{value:.1f}" for value in row["spot"])
        came = "never in the room" if row["arrived"] is None else f"in at {clock(row['arrived'])}"
        print(f"    {trace.name(row['guid'])[:14]:14} {trace.role(row['guid']):6} {row['why']:10}"
              f" {spot:>22}  {came if row['spot'] else ''}")


# ---------------------------------------------------------------------------------------------- spike

def mark_spans(trace: Trace) -> list[tuple[int, int, int, bool]]:
    """`(guid, start, stop, closed)` per Pursued by Anub'arak window on the roster, in time order."""
    roster = roster_guids(trace)
    return sorted(((guid, start, stop, closed)
                   for guid, spans in aura_spans(trace, (SPELL_MARK,)).items() if guid in roster
                   for start, stop, closed in spans), key=lambda span: span[1])


def patch_lives(trace: Trace, samples: Samples) -> dict[int, tuple[int, int]]:
    """Per sphere that ever sat as a patch, `(landed, last seen)`."""
    out = {}
    for guid in guids_of_entry(trace, NPC_FROST_SPHERE):
        stamps, rows = samples.track(guid)
        low = [when for when, row in zip(stamps, rows) if len(row) > 3 and row[3] < SPHERE_PATCH_Z]
        if low:
            out[guid] = (low[0], stamps[-1])
    return out


def consumed_patches(trace: Trace, samples: Samples) -> dict[int, tuple[int, int]]:
    """Patch guid to `(last seen, mark end)` for every patch gone within CONSUMED_MS of a mark ending.
    One still sampled in the last snapshot never went."""
    last = samples.stamps[-1] if samples.stamps else 0
    stops = [(stop, guid) for guid, _, stop, closed in mark_spans(trace) if closed]
    out = {}
    for patch, (_, seen) in patch_lives(trace, samples).items():
        if seen >= last:
            continue
        near = [stop for stop, _ in stops if abs(seen - stop) <= CONSUMED_MS]
        if near:
            out[patch] = (seen, min(near, key=lambda stop: abs(seen - stop)))
    return out


def branches_in(rows: list[tuple[int, str]], start: int, stop: int) -> list[str]:
    """The branch held when a window opened, then every change inside it. A carried `none` is the
    close of an earlier window, and a row at `stop` is this one's close."""
    held = [value for when, value in rows if when <= start]
    out = [value for value in held[-1:] if value != "none"]
    for when, value in rows:
        if start < when < stop and (not out or out[-1] != value):
            out.append(value)
    return out


def mark_windows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    samples = samples or Samples(trace)
    impales = [rec for rec in trace.of("dmg") if rec.get("sp") in IMPALE]
    deaths = combat_deaths(trace)
    kite = per_bot(trace, "anub.kite")
    used = {stop for _, stop in consumed_patches(trace, samples).values()}
    emerges = [stop for _, stop, phase, _ in phase_spans(trace) if phase == 2]

    out = []
    for guid, start, stop, closed in mark_spans(trace):
        hits = [rec for rec in impales
                if rec.get("d") == guid and start <= rec["t"] <= stop + IMPALE_SLACK_MS]
        died = next((rec["t"] for rec in deaths
                     if rec.get("g") == guid and start <= rec["t"] <= stop + IMPALE_SLACK_MS), None)
        consumed = closed and stop in used
        if died is not None:
            outcome = "death"
        elif hits:
            outcome = "impale"
        elif not closed:
            outcome = "open"
        elif not consumed and any(abs(stop - edge) <= EMERGE_SLACK_MS for edge in emerges):
            outcome = "emerge"
        else:
            # nothing hit the kiter and it lived, so a patch or a falling sphere took the spike
            outcome = "patch"
        out.append({"guid": guid, "start": start, "stop": stop,
                    "branches": branches_in(kite.get(guid, []), start, stop),
                    "impales": len(hits), "damage": sum(rec.get("a", 0) for rec in hits),
                    "outcome": outcome, "consumed": consumed, "died": died})
    return out


def stray_impales(trace: Trace, windows: list[dict]) -> list[dict]:
    """Impale hits on anyone but the kiter of the window they landed in."""
    def kiter_hit(rec):
        return any(window["guid"] == rec.get("d")
                   and window["start"] <= rec["t"] <= window["stop"] + IMPALE_SLACK_MS for window in windows)

    return [rec for rec in trace.of("dmg") if rec.get("sp") in IMPALE and not kiter_hit(rec)]


def show_spike(trace: Trace) -> None:
    print("PURSUING SPIKE")
    windows = mark_windows(trace)
    if not windows:
        print("  nobody carried Pursued by Anub'arak")
    else:
        print(f"  {'at':>9} {'kiter':14} {'role':6} {'secs':>5} {'kite branches':24} {'outcome':7}"
              f" {'impales':>7} {'damage':>8}")
        for window in windows:
            branches = ">".join(window["branches"]) or "-"
            print(f"  {clock(window['start']):>9} {trace.name(window['guid'])[:14]:14}"
                  f" {trace.role(window['guid']):6} {(window['stop'] - window['start']) / 1000:5.1f}"
                  f" {branches[:24]:24} {window['outcome']:7} {window['impales']:7} {window['damage']:8,}"
                  + ("  patch seen going" if window["consumed"] else ""))
        outcomes = collections.Counter(window["outcome"] for window in windows)
        print(f"\n  {len(windows)} window(s): "
              + ", ".join(f"{name} {n}" for name, n in outcomes.most_common()))

    stray = stray_impales(trace, windows)
    if stray:
        hit = collections.Counter(rec.get("d") for rec in stray)
        print(f"  Impale on anyone else: {len(stray)} hit(s) for {sum(rec.get('a', 0) for rec in stray):,}: "
              + ", ".join(f"{trace.name(guid)} ({trace.role(guid)}) {n}" for guid, n in hit.most_common()))
    else:
        print("  Impale hit nobody but a kiter")
    show_counts(trace, "anub.dodge")


# -------------------------------------------------------------------------------------------- spheres

def sphere_timeline(samples: Samples) -> list[tuple[int, int, int, int]]:
    """`(t, flying, patches, permafrost)` at every snapshot where one of the three changed."""
    out: list[tuple[int, int, int, int]] = []
    for when, counts in zip(samples.stamps, samples.spheres):
        if not out or out[-1][1:] != counts:
            out.append((when, *counts))
    return out


def flying_at(samples: Samples, when: int) -> int | None:
    index = samples.index_at(when)
    return samples.spheres[index][0] if index >= 0 else None


def spheres_left(trace: Trace, samples: Samples) -> list[tuple[str, int, int | None]]:
    """Flying spheres at each submerge, at phase 3 and at the end: heroic never replaces one."""
    out = [(label, start, flying_at(samples, start))
           for start, _, phase, label in phase_spans(trace) if phase == 2 or label == "P3"]
    if samples.stamps:
        out.append(("end", samples.stamps[-1], samples.spheres[-1][0]))
    return out


def sphere_shots(trace: Trace) -> collections.Counter:
    """Casts per caster aimed at a sphere."""
    spheres = guids_of_entry(trace, NPC_FROST_SPHERE)
    return collections.Counter(rec.get("s") for rec in trace.of("cast")
                               if rec.get("tgt") in spheres and not rec.get("tr"))


def show_spheres(trace: Trace) -> None:
    print("FROST SPHERES")
    samples = Samples(trace)
    spheres = guids_of_entry(trace, NPC_FROST_SPHERE)
    if not spheres:
        print("  no Frost Sphere named")
        return

    lives = patch_lives(trace, samples)
    used = consumed_patches(trace, samples)
    print(f"  {len(spheres)} sphere(s) named, {len(lives)} ever a patch, {len(used)} used by the spike")
    timeline = [row for row in sphere_timeline(samples) if any(row[1:])]
    print(f"  {'at':>9} {'flying':>6} {'patches':>7} {'permafrost':>10}")
    for when, flying, patches, frost in timeline[:TIMELINE_SHOWN]:
        print(f"  {clock(when):>9} {flying:6} {patches:7} {frost:10}")
    if len(timeline) > TIMELINE_SHOWN:
        print(f"  ... {len(timeline) - TIMELINE_SHOWN} more change(s)")

    for patch, (seen, stop) in sorted(used.items(), key=lambda item: item[1]):
        print(f"  patch {short(trace, patch)} last seen {clock(seen)}, a mark ended {clock(stop)}")
    if is_heroic(trace):
        print("  heroic, no respawn, flying left: "
              + ", ".join(f"{label} {clock(when)} {'-' if n is None else n}"
                          for label, when, n in spheres_left(trace, samples)))
    show_counts(trace, "anub.sphere")
    shots = sphere_shots(trace)
    if shots:
        print("  cast at a sphere: "
              + ", ".join(f"{trace.name(guid)} {n}" for guid, n in shots.most_common()))


# ------------------------------------------------------------------------------------------ burrowers

def returned(before: tuple[int, float], after: tuple[int, float]) -> bool:
    """A submerge, read off two samples: gone at least SUBMERGE_GAP_MS and back near full, or a jump."""
    (then, low), (now, high) = before, after
    if now - then >= SUBMERGE_GAP_MS and high >= NEAR_FULL and high > low:
        return True
    return high - low >= SUBMERGE_JUMP


def on_permafrost(samples: Samples, row, when: int) -> bool:
    return any(dist2(row[1:3], centre) <= BURROWER_PERMAFROST_REACH for centre in samples.frost_at(when))


def burrower_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    samples = samples or Samples(trace)
    strikes = [rec for rec in trace.of("cast") if rec.get("sp") == SPELL_SHADOW_STRIKE]
    hits = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_SHADOW_STRIKE]
    out = []
    for guid in sorted(guids_of_entry(trace, NPC_BURROWER)):
        stamps, rows = samples.track(guid)
        if not stamps:
            continue
        points = [(when, row[5]) for when, row in zip(stamps, rows)]
        mine = [rec for rec in strikes if rec.get("s") == guid]
        landed = sum(1 for rec in mine if any(
            hit.get("s") == guid
            and rec["t"] <= hit["t"] <= rec["t"] + (rec.get("ct") or SHADOW_STRIKE_CAST_MS) + LAND_SLACK_MS
            for hit in hits))
        out.append({
            "guid": guid,
            "first": stamps[0],
            "last": stamps[-1],
            "low": min(hp for _, hp in points),
            "end": points[-1][1],
            "submerges": sum(1 for index in range(1, len(points))
                             if returned(points[index - 1], points[index])),
            "frost": sum(1 for when, row in zip(stamps, rows)
                         if on_permafrost(samples, row, when)) / len(rows),
            "strikes": len(mine),
            "landed": landed,
        })
    return out


def show_burrowers(trace: Trace) -> None:
    print("NERUBIAN BURROWERS")
    rows = burrower_rows(trace)
    if not rows:
        print("  no burrower sampled")
    else:
        print(f"  {'burrower':10} {'first':>9} {'last':>9} {'low':>6} {'end':>6} {'submerged':>9}"
              f" {'on frost':>8} {'strikes':>7} {'landed':>6}")
        for row in rows:
            print(f"  {short(trace, row['guid']):10} {clock(row['first']):>9} {clock(row['last']):>9}"
                  f" {row['low']:5.1f}% {row['end']:5.1f}% {row['submerges']:9} {row['frost'] * 100:7.0f}%"
                  f" {row['strikes']:7} {row['landed']:6}")
        print(f"\n  on frost is the share of its samples within {BURROWER_PERMAFROST_REACH:.1f} yd of a"
              " Permafrost centre\n  a submerge is its health going back to full")

    held = sorted({trace.name(guid) for guid, counts in value_counts(trace, "anub.interrupter").items()
                   if counts.get("1")})
    print(f"  ever held Shadow Strike duty: {', '.join(held) if held else 'nobody'}")


# --------------------------------------------------------------------------------------------- threat

def surfaced_windows(spans) -> list[dict]:
    """Each P1 or P3 span, and what came before it: the pull, an emerge, or the swarm starting."""
    out = []
    for index, (start, stop, phase, label) in enumerate(spans):
        if phase not in (1, 3):
            continue
        before = spans[index - 1][2] if index else 0
        kind = "emerge" if before == 2 else ("swarm" if before == 1 else "pull")
        out.append({"label": label, "kind": kind, "start": start, "stop": stop})
    return out


def held_at(spans, when: int) -> str | None:
    return next((value for value, start, stop in spans if start <= when < stop), None)


def drag_anchor(latches, samples: Samples, when: int) -> tuple[float, float]:
    """Where the main tank drags him: between both latched tank patches, else the room centre."""
    spots = []
    for spans in latches:
        value = held_at(spans, when)
        row = samples.recent(int(value), when) if value and value.isdigit() and value != "0" else None
        if row and row[3] < SPHERE_PATCH_Z:
            spots.append(row[1:3])
    if len(spots) == 2:
        return (spots[0][0] + spots[1][0]) / 2.0, (spots[0][1] + spots[1][1]) / 2.0
    return ROOM_CENTER[0], ROOM_CENTER[1]


def threat_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    samples = samples or Samples(trace)
    boss = boss_guid(trace)
    if boss is None:
        return []
    taunts = [rec for rec in trace.of("cast")
              if rec.get("sp") in TAUNT_SPELLS and rec.get("tgt") == boss and not rec.get("tr")]
    end = pull_end(trace)
    latches = [latch_spans(trace, key, end) for key in ("anub.patch0", "anub.patch1")]
    stamps, rows = samples.track(boss)

    out = []
    for window in surfaced_windows(phase_spans(trace)):
        start, stop = window["start"], window["stop"]
        held, _ = threat_share(trace, {boss}, lambda when, low=start, high=stop: low <= when < high)
        inside = [(when, row) for when, row in zip(stamps, rows) if start <= when < stop]
        tank = next((when for when, row in inside if len(row) > 7 and trace.role(row[7]) == "tank"), None)
        spread = [dist2(row[1:3], drag_anchor(latches, samples, when)) for when, row in inside]
        out.append({**window,
                    "held": held,
                    "to_tank": None if tank is None else tank - start,
                    "taunts": [rec for rec in taunts if start <= rec["t"] < stop],
                    "off_anchor": statistics.median(spread) if spread else None})
    return out


def show_threat(trace: Trace) -> None:
    print("THREAT")
    rows = threat_rows(trace)
    if not rows:
        print("  no surfaced window: Anub'arak never sampled or no anub.phase rows")
        return

    for row in rows:
        total = sum(row["held"].values())
        share = "never sampled"
        if total:
            share = "  ".join(f"{role} {span * 100.0 / total:.0f}%" for role, span in row["held"].most_common())
        to_tank = "never" if row["to_tank"] is None else f"{row['to_tank'] / 1000:.1f} s"
        anchor_gap = "-" if row["off_anchor"] is None else f"{row['off_anchor']:.1f} yd"
        print(f"  {row['label']:5} {row['kind']:6} {clock(row['start']):>9}"
              f" {(row['stop'] - row['start']) / 1000:6.1f} s  to a tank {to_tank:>7}"
              f"  off anchor {anchor_gap:>8}  {share}")
        for rec in row["taunts"]:
            print(f"          taunt {clock(rec['t'])} {trace.name(rec.get('s'))[:14]:14}"
                  f" {TAUNT_SPELLS[rec['sp']]}")

    emerges = [row["to_tank"] for row in rows if row["kind"] == "emerge"]
    if emerges:
        known = [ms for ms in emerges if ms is not None]
        print(f"\n  {len(emerges)} emerge(s), {len(emerges) - len(known)} never reached a tank"
              + (f", the rest in {min(known) / 1000:.1f}-{max(known) / 1000:.1f} s" if known else ""))
    print("  off anchor is his median distance from the midpoint of the latched tank patches,"
          " else the room centre")
    show_counts(trace, "anub.pickup")


# ---------------------------------------------------------------------------------------------- swarm

def swarm_span(trace: Trace, spans=None) -> tuple[int, int, str] | None:
    """Phase 3 from `anub.phase`, else from the first Leeching Swarm aura or tick on the raid."""
    spans = phase_spans(trace) if spans is None else spans
    for start, stop, phase, _ in spans:
        if phase == 3:
            return start, stop, "anub.phase"
    starts = [rec["t"] for rec in trace.of("aura") if rec.get("sp") in LEECHING_SWARM and not rec.get("r")]
    starts += [rec["t"] for rec in trace.of("dmg") if rec.get("sp") == SPELL_LEECHING_SWARM_DMG]
    if starts:
        return min(starts), max(min(starts), pull_end(trace)), "Leeching Swarm"
    return None


def swarm_damage(trace: Trace) -> dict[int, tuple[int, int]]:
    """Per victim, `(ticks, damage)` of 66240."""
    out: dict[int, list[int]] = collections.defaultdict(lambda: [0, 0])
    for rec in trace.of("dmg"):
        if rec.get("sp") == SPELL_LEECHING_SWARM_DMG:
            out[rec.get("d")][0] += 1
            out[rec.get("d")][1] += rec.get("a", 0)
    return {guid: (ticks, damage) for guid, (ticks, damage) in out.items()}


def lust_casts(trace: Trace, start: int) -> list[dict]:
    return [{"t": rec["t"], "rel": rec["t"] - start, "caster": rec.get("s"), "spell": rec.get("sp")}
            for rec in trace.of("cast") if rec.get("sp") in LUST_SPELLS and not rec.get("tr")]


def cold_windows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Every Penetrating Cold on the roster, with the heals it drew and how low the carrier got."""
    samples = samples or Samples(trace)
    roster = roster_guids(trace)
    heals = trace.of("heal")
    deaths = combat_deaths(trace)
    spans = phase_spans(trace)
    out = []
    for guid, windows in aura_spans(trace, PENETRATING_COLD).items():
        if guid not in roster:
            continue
        for start, stop, _ in windows:
            mine = [rec for rec in heals if rec.get("d") == guid and start <= rec["t"] <= stop]
            hp = [row[5] for row in samples.between(guid, start, stop)]
            out.append({
                "guid": guid,
                "start": start,
                "stop": stop,
                "phase": phase_at(spans, start),
                "heals": len(mine),
                "amount": sum(rec.get("a", 0) for rec in mine),
                "overheal": sum(rec.get("oh", 0) for rec in mine),
                "healers": {rec.get("s") for rec in mine},
                "low": min(hp) if hp else None,
                "died": any(rec.get("g") == guid and start <= rec["t"] <= stop for rec in deaths),
            })
    return sorted(out, key=lambda row: row["start"])


def show_swarm(trace: Trace) -> None:
    print("PHASE 3")
    span = swarm_span(trace)
    if span is None:
        print("  never reached: no phase 3 in anub.phase and no Leeching Swarm on the raid")
    else:
        start, stop, source = span
        print(f"  from {clock(start)} for {(stop - start) / 1000:.1f} s ({source})")

        damage = swarm_damage(trace)
        ticks = sum(n for n, _ in damage.values())
        print(f"  Leeching Swarm {SPELL_LEECHING_SWARM_DMG}: {sum(a for _, a in damage.values()):,}"
              f" over {ticks} tick(s)")
        for guid, (n, amount) in sorted(damage.items(), key=lambda item: -item[1][1]):
            print(f"    {trace.name(guid)[:14]:14} {trace.role(guid):6} {n:4} {amount:10,}")

        lust = lust_casts(trace, start)
        for row in lust:
            print(f"  {trace.spell(row['spell'])} by {trace.name(row['caster'])} at {clock(row['t'])}"
                  f" ({row['rel'] / 1000:+.1f} s from phase 3)")
        if not lust:
            print("  no lust cast")

    windows = cold_windows(trace)
    if windows:
        print(f"\n  Penetrating Cold, whole pull: {len(windows)} carrier window(s),"
              f" {sum(row['heals'] for row in windows)} heal(s) onto them")
        print(f"    {'at':>9} {'phase':5} {'carrier':14} {'role':6} {'secs':>5} {'heals':>5} {'amount':>8}"
              f" {'over':>7} {'low':>6}")
        for row in windows:
            low = "-" if row["low"] is None else f"{row['low']:5.1f}%"
            print(f"    {clock(row['start']):>9} {row['phase']:5} {trace.name(row['guid'])[:14]:14}"
                  f" {trace.role(row['guid']):6} {(row['stop'] - row['start']) / 1000:5.1f} {row['heals']:5}"
                  f" {row['amount']:8,} {row['overheal']:7,} {low:>6}" + ("  died" if row["died"] else ""))
    else:
        print("  no Penetrating Cold on the raid")
    show_counts(trace, "anub.defensive")

    if span is not None:
        dead = [rec for rec in combat_deaths(trace) if rec["t"] >= span[0]]
        print(f"  {len(dead)} death(s) in phase 3" + (":" if dead else ""))
        for rec in dead:
            print(f"    {clock(rec['t'])} {trace.name(rec.get('g'))[:14]:14} {trace.role(rec.get('g')):6}"
                  f" by {trace.name(rec.get('killer'))}")


SECTIONS = (
    ("phases", "anub.phase spans, his health at each edge, deaths per phase", show_phases),
    ("pull", "locked out past the Web Door or never landed, stage 9 to the pull", show_pull),
    ("spike", "every Pursued by Anub'arak window, kite branches and outcome, Impale on others", show_spike),
    ("spheres", "flying spheres and patches over time, patches the spike used, shooters", show_spheres),
    ("burrowers", "each burrower: life, submerges, time on Permafrost, Shadow Strike", show_burrowers),
    ("threat", "his target by role per surfaced window, time to a tank, taunts", show_threat),
    ("swarm", "phase 3: Leeching Swarm, lust, Penetrating Cold heals, defensives, deaths", show_swarm),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
