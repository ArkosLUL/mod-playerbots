#!/usr/bin/env python3
"""Score an Anub'arak pull from a RaidObs trace: phases, the landing, the spike, spheres, burrowers.

    anub_arak.py <file>              every section
    anub_arak.py <file> --phases     anub.phase spans, his health at each edge, deaths per phase
    anub_arak.py <file> --pull       who was locked out or never landed, and stage 9 to the pull
    anub_arak.py <file> --spike      every Pursued by Anub'arak window: kiter, kite branch, outcome, protection,
                                     time in each kite stage, the kiter's boosts, its own immunities and
                                     whether anub.protect asked for them, a kiter marked inside a patch's slow,
                                     the tank patch it spent, one spent in the run stage flagged, the spike's
                                     closest approach to each latched tank patch; at the mark's end the spike's
                                     distance from the gather centre and any unlatched patch alive, a death or
                                     own immunity within 20 yd of the gather flagged; the spike's speed and the
                                     kiter's at each Hand of Protection and own immunity, one on a spike no
                                     faster flagged, and the seconds the kiter stood still while marked; a walk
                                     flip-flopping between two move targets; the nearest Hand of Protection-ready
                                     paladin as the spike first came within 12 yd; a Feign Death dropped at once;
                                     per submerge the living players within 8, 12 and 20 yd of where he went
                                     down, by role, and the first kiter's distance from the spike at its mark
    anub_arak.py <file> --spheres    spheres over time, patch supply at each mark, sphere duty and its shots,
                                     each dead sphere's killer and landing against its side's spot, Death Grips
                                     against the DK's spot at the cast, whether the grip duty held to kill and
                                     a gripped sphere back up, each side's first tank patch, each anub.spot0
                                     and anub.spot1 change; per submerge the spheres spent and the tank patches
                                     gone against the kite allowance, each with its kiter, and when that side's
                                     patch came back
    anub_arak.py <file> --burrowers  each burrower: life, submerges, Permafrost, Shadow Strike, its exit, side
                                     and pickup, DPS focus, DPS casts at any other burrower; whether it died
                                     inside its first 30 s up; per wave, those deaths, when the focus came due,
                                     and each time it stopped with the focus alive; each focus pick against its
                                     tank and the patches, each burrower's distance from the patch centre while
                                     on a side tank; each submerge where it went under: the nearest patch, the
                                     patches alive, its side tank alive and how far, its target; each one's
                                     opener and the three players nearest its exit at the spawn
    anub_arak.py <file> --frost      seconds carrying Permafrost per group and bot, he up and he down, and every
                                     stint over 3 s with the bot's target at its start
    anub_arak.py <file> --threat     per surfaced window: his target, his drag legs, taunts, threat cap, the
                                     main tank's time in Permafrost, his submerge spot against the tank patches
    anub_arak.py <file> --swarm      phase 3: Leeching Swarm per bot, lust, Penetrating Cold, deaths
    anub_arak.py <file> --healers    per healer and phase its median distance to the main tank and each side
                                     tank, the seconds over 35 yd from a side tank a burrower was on, follow
                                     acts in the pull, its engine passes, anub.hstand; per side tank the seconds
                                     alive with no healer within 40 yd; raid healer engine in the coverage
    anub_arak.py <file> --tanks      Expose Weakness peak per burrower wave per side tank, each tank defensive
                                     cast with the stack then, from its cast row, each tank death with the damage
                                     taken and the heals landed in the 12 s before
    anub_arak.py <file> --scarabs    the scarabs alive 0, 10, 20 and 30 s after each emerge with their targets,
                                     the main tank's Mandibles stack at the emerge, scarab lives per phase, and
                                     per phase each player's Mandibles peak stack and tick damage
    anub_arak.py <file> --redirects  each Misdirection and Tricks of the Trade: caster, recipient, the caster's
                                     target, and a burrower's target 1 to 8 s after; totals per recipient
    anub_arak.py <file> --pursuit    pure pursuit replayed against the real spike per kite: end and patch-pass
                                     errors 3 and 6 s on, a p90 patch-pass error over 1.5 yd flagged

The banner names the probes the pull never wrote, a side left without a side tank, and a human on one.

What the generic views get wrong here, and what this reads instead:

- **Two units are called Anub'arak.** The Pursuing Spike (34660) carries his name and the boss flag,
  so anything keyed on the name or the flag can pick the spike. He is keyed on entry 34564 here.
- **The `anub-arak` slug is also Azjol-Nerub's boss**, so the banner checks map 649 as well.
- **One entry, two states.** A flying Frost Sphere hovers at z 155.67 and a patch sits at 142.7, and
  neither carries Permafrost itself, so spheres are split by height and Permafrost is read off
  `snap.hz`. A low sphere with no Permafrost centre on it is still falling: it casts Permafrost
  1.5 s after it drops.
- **Creature auras and deaths are never recorded**, so a burrower's submerge only shows as its health
  going back to full, which the core sets as it emerges, its time on Permafrost as its distance from a
  Permafrost centre, and its death as its last sample before the trace ends.
- **`anub.kite`, `anub.dodge`, `anub.sphere` and the other derived keys are per bot and change only**:
  a bot that takes the same branch twice writes one row. `anub.kite` goes back to `none` when the mark
  moves on, so a window never opens on the branch the last one ended with. `anub.gather` never does,
  so a bot still reads `hold` while it kites, after it dies and into the next submerge: the gather
  centre counts only living bots off the mark whose hold began in this submerge.
- **`anub.focus` is one raid-wide latch**, so the `g` on its rows is whichever bot's read moved it, and
  whether the DPS followed it is read off their snapshot targets, not off `anub.dps`. So are
  `anub.side0`, `anub.side1`, `anub.wave`, `anub.focusdue`, `anub.grip`, `anub.allow`, and `anub.spot0`
  and `anub.spot1`: each side's tank spot as `x,y,z`, its latched patch while it has one, else where the
  open side wants one; z is left out here. `anub.bside` is per burrower, its `g` the burrower, and
  `anub.hstand` per healer, `walk` or `hold`.
- **No row carries a speed.** The spike's speed and the kiter's are the median of their moving
  snapshot steps around the moment, and the spike's speed auras are creature auras, never recorded.
- **No `act` row names its engine.** `cov` counts each engine's passes per bot from the session's open,
  so the non-combat engine's passes while he is in combat are a pull total, not a timeline.
- **A sphere's killer is rarely a hit.** `dmg` rows only land on the roster, so the killer is the last
  `dmg` row on the sphere when there is one, else the last cast aimed at it. The killing blow makes it
  unselectable and it casts Permafrost 1.5 s later, so its death is timed off the patch, not its
  height: a gripped sphere sits low while it's still alive.
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

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of, map_of  # noqa: E402
from raidobs.geometry import anchor, dist2, frames, guids_of_entry, radius  # noqa: E402
from raidobs.probes import emitted_keys, holder_spans, latch_spans, silent_keys  # noqa: E402
from raidobs.space import threat_share  # noqa: E402
from raidobs.trace import COVERAGE_COLUMNS, Trace, clock, combat_deaths, notes, roster_guids  # noqa: E402

MAP_TOC = 649
DIFFICULTIES = {0: "10N", 1: "25N", 2: "10H", 3: "25H"}
HEROIC = (2, 3)

NPC_ANUBARAK = 34564
NPC_FROST_SPHERE = 34606
NPC_BURROWER = 34607
NPC_SPIKE = 34660
NPC_SCARAB = 34605
ENCOUNTER_NPCS = (NPC_ANUBARAK, NPC_FROST_SPHERE, NPC_BURROWER, NPC_SPIKE, NPC_SCARAB)

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
SPELL_RIGHTEOUS_DEFENSE = 31789
# what CastClassTaunt casts, plus Righteous Defense, which targets the friend he is hitting
TAUNT_SPELLS = {355: "Taunt", 62124: "Hand of Reckoning", 56222: "Dark Command", 6795: "Growl",
                SPELL_RIGHTEOUS_DEFENSE: "Righteous Defense"}
HAND_OF_PROTECTION = (1022, 5599, 10278)
SPELL_FEIGN_DEATH = 5384
KITER_OWN = {642: "Divine Shield", 45438: "Ice Block", SPELL_FEIGN_DEATH: "Feign Death", 26889: "Vanish"}
SPELL_FORBEARANCE = 25771
CLASS_PALADIN = 2
# The stem casts these as the kiter's last resort, once its anub.protect reads self
LAST_RESORT_LEAD_MS = 2000
# 55004 is the Nitro Boosts use, 54861 the speed it gives
KITE_BOOSTS = {11305: "Sprint", 33357: "Dash", 55004: "Nitro Boosts", 54861: "Nitro Boosts", 1953: "Blink",
               48020: "Demonic Circle: Teleport"}
NITRO_SPEED = 54861
# one boost read twice, a use and its aura or a cast and its triggered copy
BOOST_PAIR_MS = 1000
SPELL_DEATH_GRIP = 49576
THREAT_CAP = "anubarak threat cap"
# 10-man and 25-man; 25H wears 67847
EXPOSE_WEAKNESS = (67721, 67847)
# ANUBARAK_SIDE_DEFENSIVE_STACKS, a uint32 radius() doesn't read
DEFENSIVE_STACKS = 5
MAGIC_ONLY_DEFENSIVES = ("anti magic shell", "cloak of shadows")
# NextTankDefensive's tank rows, named the way anub.defensive writes them
TANK_DEFENSIVES = {12975: "last stand", 871: "shield wall", 498: "divine protection", 22812: "barkskin",
                   22842: "frenzied regeneration", 61336: "survival instincts", 48707: "anti magic shell",
                   55233: "vampiric blood", 48792: "icebound fortitude"}
# 10N, 25N, 10H, 25H
MANDIBLES = (65775, 67861, 67862, 67863)
REDIRECTS = {34477: "Misdirection", 57934: "Tricks of the Trade"}

# Hand-kept, since no dmg or cast row says whether a spell ticks or splashes: level 80 ranks of what
# bots aim at a sphere.
PERIODIC = {47813, 47864, 47811, 47843, 47867, 48468, 48463, 48125, 48160, 48300, 48156, 49001, 60053, 53352,
            63672, 49233, 47857, 47855, 42846, 55360, 47836, 48819, 49800, 48672, 48676, 47465, 42926}
AREA = {53190, 53195, 42940, 47820, 47823, 58434, 48467, 49938, 51411, 49941, 49048, 49271, 42921, 42926,
        51723, 47520, 47502, 48562, 62078, 48817, 53385, 59159, 61657, 47836, 55360, 48819, 1680, 53595,
        47897, 61290, 42950, 42945, 46924}
# aimed at spheres in real pulls but deal no damage, so never the killer
NO_DAMAGE = {SPELL_DEATH_GRIP, 53338, 47865}

PHASE_LABELS = {0: "none", 1: "P1", 2: "P2", 3: "P3"}

FOCUS = "anub.focus"
DPS_ROLES = ("melee", "ranged")
SIDE_TANKS_NEEDED = {0: 1, 1: 2, 2: 1, 3: 2}
GRIP_STATES = {"0": "none", "1": "gripping", "2": "shooting"}
# A side this long without a tank patch while he's up with burrowers out: its pair can submerge to full
UNPATCHED_FLAG_MS = 15000

# Every key a section reads, so the banner flags one this trace lacks even while the source doesn't
# declare it in a shape the probe parse finds.
READ_KEYS = ("toc.progress", "anub.phase", "anub.patch0", "anub.patch1", "anub.sphere", "anub.kite",
             "anub.dodge", "anub.protect", "anub.interrupter", "anub.pickup", "anub.defensive", "anub.focus",
             "anub.dps", "anub.side0", "anub.side1", "anub.wave", "anub.focusdue", "anub.grip", "anub.allow",
             "anub.bside", "anub.gripstep", "anub.sidetank", "anub.kitestage", "anub.gather", "anub.spot0",
             "anub.spot1", "anub.hstand")
SPOT_KEYS = ("anub.spot0", "anub.spot1")

# Read from the source so a retune shows up here without a second edit.
ROOM_CENTER = anchor("ANUBARAK_ROOM_CENTER")
KITE_PROTECT_RANGE = radius("ANUBARAK_KITE_PROTECT_RANGE")
TANK_PATCH_OFFSET = radius("ANUBARAK_TANK_PATCH_OFFSET")
PERMAFROST_SLOW_REACH = radius("ANUBARAK_PERMAFROST_SLOW_REACH")
GRIP_LANDED = radius("ANUBARAK_GRIP_LANDED")
GRIP_LANDED_HEIGHT = radius("ANUBARAK_GRIP_LANDED_HEIGHT")
# once one side has a patch, the other side's spot sits the two spots' gap off it in y
SIDE_SPOT_GAP = 2.0 * TANK_PATCH_OFFSET
# ANUBARAK_GRIP_JUDGE_MS, a uint32 radius() doesn't read
GRIP_JUDGE_MS = 3000
# The second of ANUBARAK_SUBMERGE_CLEARANCES, an array radius() doesn't read: a submerge spot nearer a
# tank patch is a fallback
SUBMERGE_PATCH_CLEARANCE = 25.0
SPIKE_PATCH_REACH = radius("ANUBARAK_SPIKE_PATCH_REACH")
FOCUS_SETTLE = radius("ANUBARAK_FOCUS_SETTLE")
FOCUS_LOW_PCT = radius("ANUBARAK_FOCUS_OFF_FROST_PCT")
# ANUBARAK_KITE_GATHER_CLEARANCE, a sum radius() doesn't read: a kite ending nearer hands the spike to the
# gather
KITE_END_GATHER_YD = 20.0
# Permafrost's target search adds only the target's size. A burrower's display has no combat reach, so
# it gets the core's 0.389 default, not the 1.5 of ANUBARAK_PERMAFROST_SLOW_REACH's player.
BURROWER_PERMAFROST_REACH = 6.0 + 0.389

# Not constants in the module. The Web Door is GO 195485 and shuts at the engage. Before the floor
# breaks the raid stands near z 395, the pit floor is 142. Spheres fly at 155.67 and land at 142.7.
WEB_DOOR_X = 661.6
LANDED_Z = 200.0
SPHERE_FLYING_Z = 150.0
SPHERE_PATCH_Z = 146.0
# a patch's Permafrost dynobject sits on the sphere itself
PATCH_MATCH_YD = 1.0
# (x, y, side) per burrow exit: the north pair feeds side 0, the south pair side 1
BURROW_EXITS = ((740.2, 193.4, 0), (694.5, 185.4, 0), (732.0, 83.4, 1), (694.9, 102.5, 1))
# the killing blow sets the sphere falling and it casts Permafrost this long after
PERMAFROST_DELAY_MS = 1500
# a wave's burrowers spawn in one tick and waves come 45 s apart
WAVE_SPLIT_MS = 10000

LEG_MIN_YD = 2.0
REVERSAL_DEGREES = 120.0

# the spike's fail despawns the patch 1.5 s after the mark comes off
CONSUMED_MS = 3000
# the spike despawns 2 s before he turns selectable, so a mark ended by the emerge lands near that edge
EMERGE_SLACK_MS = 3000
IMPALE_SLACK_MS = 500
# Divine Shield, Ice Block, Feign Death and Vanish hand the mark on at once, so one cast this close
# before the mark comes off ended it
IMMUNE_END_MS = 1000
SUBMERGE_GAP_MS = 5000
# npc_nerubian_burrowerAI runs its first submerge check 30 s after it spawns or emerges
BURROWER_SAFE_MS = 30000
NEAR_FULL = 95.0
SUBMERGE_JUMP = 15.0
SHADOW_STRIKE_CAST_MS = 8000
LAND_SLACK_MS = 1000
# a row this far after the asked time still answers it when nothing was sampled before
NEAR_MS = 2000
# one snapshot gap longer than this is a hole in the trace, not time spent anywhere
MAX_STEP_MS = 2000
TIMELINE_SHOWN = 30
# a Permafrost stint longer than this is listed on its own
FROST_STINT_MS = 3000
FROST_GROUPS = ("main tank", "side tank", "tank", "melee", "ranged", "heal")

# A kiter that moved less than STALL_YD over a STALL_MS stretch of its mark stood still
STALL_YD = 1.5
STALL_MS = 1000
# A snapshot step slower than this is a stop or a turn, not the unit's speed. Steps read 0.1 yd/s either
# side of the true speed, so a spike within SPEED_SLACK of the kiter counts as no faster.
MOVING_YD_S = 1.0
SPEED_SLACK = 0.2
CROWD_YD = (8.0, 12.0, 20.0)
CROWD_ROLES = ("tank", "heal", "melee", "ranged")
# a kiter's move targets jumping FLIP_YD or more and back FLIP_COUNT times inside FLIP_MS: two walks
# cancelling each other out
FLIP_YD = 5.0
FLIP_COUNT = 3
FLIP_MS = 2000
# Feign Death breaks on move, so cast while running it comes off the same tick
FEIGN_DROP_MS = 100
HOP_RANGE = radius("ANUBARAK_HOP_RANGE")
# HoP cooldown with both ranks of Guardian's Favor, the shortest it gets. No row carries cooldowns, so a
# paladin reads ready this long after its last one.
HOP_COOLDOWN_MS = 180000
SCARAB_OFFSETS_MS = (0, 10000, 20000, 30000)
# when a redirect's burrower is read again for its target
REDIRECT_AFTER_MS = (1000, 2000, 3000, 5000, 8000)
OPENER_NEAREST = 3
# pursuit replay: a model spike every PURSUIT_STEP_MS of a chase, a snapshot gap this long ends the chase,
# patches within PURSUIT_PATCH_YD of the start judged
PURSUIT_HORIZONS_MS = (3000, 6000)
PURSUIT_STEP_MS = 500
PURSUIT_GAP_MS = 600
PURSUIT_PATCH_YD = 30.0
PURSUIT_FLAG_YD = 1.5
# a submerged burrower keeps its spot and its health until it comes back up full
FROZEN_YD = 0.3
# a submerged burrower targets nobody, so its victim is the last target it had this far back
VICTIM_LOOKBACK_MS = 5000
HEALER_FAR_YD = 35.0
HEALER_REACH_YD = 40.0
TANK_DEATH_LOOKBACK_MS = 12000
HEALER_ENGINE = "raid healer engine"
ENGINE_NAMES = {"c": "combat", "n": "non-combat", "d": "dead"}
# Every node of an engine lands in exactly one of these columns on every pass
PASS_COLUMNS = ("checks", "shared", "throttled", "minimal", "dead")


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


def unit_label(trace: Trace, guid) -> str:
    """A target as a reader recognises it: a roster name, a pet or a ghoul by name and owner, him, or a
    creature's kind and counter."""
    entry = trace.entries.get(guid)
    if not guid or guid in trace.roles or guid in trace.owners or entry is None:
        return trace.name(guid)
    if entry == NPC_ANUBARAK:
        return "Anub'arak"
    kind = {NPC_BURROWER: "burrower", NPC_FROST_SPHERE: "sphere", NPC_SPIKE: "spike"}.get(entry, trace.name(guid))
    return f"{kind} {short(trace, guid)}"


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
        self.sphere_rows: list[list[list]] = []
        self.rows: dict[int, tuple[list[int], list[list]]] = {}
        for snap in frames(trace):
            when = snap["t"]
            self.stamps.append(when)
            frost = [(hz[1], hz[2]) for hz in snap.get("hz", []) if len(hz) >= 3 and hz[0] in PERMAFROST]
            self.frost.append(frost)
            flying = patches = 0
            mine = []
            for row in snap.get("u", []):
                stamps, rows = self.rows.setdefault(row[0], ([], []))
                stamps.append(when)
                rows.append(row)
                if row[0] in spheres and len(row) > 3:
                    mine.append(row)
                    if row[3] > SPHERE_FLYING_Z:
                        flying += 1
                    elif row[3] < SPHERE_PATCH_Z:
                        patches += 1
            self.spheres.append((flying, patches, len(frost)))
            self.sphere_rows.append(mine)

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

    def sphere_kinds(self, when: int) -> dict[int, str]:
        """Each sphere in the snapshot at or before `when`: `flying`, `falling` or `patch`."""
        index = self.index_at(when)
        if index < 0:
            return {}
        out = {}
        for row in self.sphere_rows[index]:
            if row[3] > SPHERE_FLYING_Z:
                out[row[0]] = "flying"
            elif any(dist2(row[1:3], centre) <= PATCH_MATCH_YD for centre in self.frost[index]):
                out[row[0]] = "patch"
            else:
                out[row[0]] = "falling"
        return out


def health(samples: Samples, guid: int | None, when: int) -> float | None:
    row = samples.row(guid, when) if guid else None
    return row[5] if row else None


def patch_latches(trace: Trace) -> list[list[tuple[str, int, int]]]:
    """`anub.patch0` and `anub.patch1` spans, indexed by side."""
    end = pull_end(trace)
    return [latch_spans(trace, key, end) for key in ("anub.patch0", "anub.patch1")]


def latched_patch(spans, samples: Samples, when: int) -> tuple[float, float] | None:
    """Where the patch a side had latched at `when` sat, None with no latch or none sampled low."""
    guid = as_guid(held_at(spans, when))
    row = samples.recent(guid, when) if guid else None
    return (row[1], row[2]) if row and row[3] < SPHERE_PATCH_Z else None


def parse_spot(text) -> tuple[float, float] | None:
    """An `anub.spotN` value, `x,y,z` or `x,y`, as its ground point."""
    parts = str(text).split(",")
    if len(parts) not in (2, 3):
        return None
    try:
        return float(parts[0]), float(parts[1])
    except ValueError:
        return None


def spot_spans(trace: Trace) -> list[list[tuple[tuple[float, float] | None, int, int]]]:
    """`anub.spot0` and `anub.spot1` spans, indexed by side, each value parsed to its ground point."""
    end = pull_end(trace)
    return [[(parse_spot(value), start, stop) for value, start, stop in latch_spans(trace, key, end)]
            for key in SPOT_KEYS]


def side_spot(latches, samples: Samples, side: int, when: int, noted=None) -> tuple[float, float]:
    """Where a side's tank patch belongs: the `anub.spotN` it held at `when` when `noted` has one, else the
    room centre's y plus TANK_PATCH_OFFSET for side 0, minus for side 1, or SIDE_SPOT_GAP off the other
    side's patch toward this side once that one latched."""
    spot = held_at(noted[side], when) if noted else None
    if spot is not None:
        return spot
    sign = 1.0 if side == 0 else -1.0
    other = latched_patch(latches[1 - side], samples, when)
    if other is not None:
        return other[0], other[1] + sign * SIDE_SPOT_GAP
    return ROOM_CENTER[0], ROOM_CENTER[1] + sign * TANK_PATCH_OFFSET


def missing_probes(trace: Trace) -> list[str]:
    emitted = emitted_keys(trace)
    declared = {key for key, _, _ in silent_keys(emitted, encounter_of(trace))}
    return sorted(declared | {key for key in READ_KEYS if key not in emitted})


def side_tanks(trace: Trace) -> tuple[int, dict[int, list[int]]]:
    """Side tanks the raid size needs, and each side's holders in the order `anub.side0` and
    `anub.side1` latched them, humans included: a human side tank swaps sides with the bot one."""
    needed = SIDE_TANKS_NEEDED.get(trace.header.get("diff"), 2)
    latched = {}
    for side in (0, 1):
        holders: list[int] = []
        for rec in notes(trace, f"anub.side{side}"):
            guid = as_guid(rec.get("txt"))
            if guid and (not holders or holders[-1] != guid):
                holders.append(guid)
        if holders:
            latched[side] = holders
    return needed, latched


def covered(spans: list[tuple[int, int]], lo: int, hi: int) -> int:
    """Milliseconds of `[lo, hi)` inside the union of `spans`."""
    total = 0
    reach = lo
    for start, stop in sorted(spans):
        start, stop = max(start, reach), min(stop, hi)
        if start < stop:
            total += stop - start
            reach = stop
    return total


def unpatched_ms(trace: Trace, samples: Samples) -> dict[int, int]:
    """Per side with a side tank, how long he was up with a burrower alive and no tank patch latched."""
    lives = []
    for guid in guids_of_entry(trace, NPC_BURROWER):
        stamps, _ = samples.track(guid)
        if stamps:
            lives.append((stamps[0], stamps[-1]))
    up = [(start, stop) for start, stop, phase, _ in phase_spans(trace) if phase in (1, 3)]
    _, holders = side_tanks(trace)
    out = {}
    for side, spans in enumerate(patch_latches(trace)):
        if side not in holders:
            continue
        bare = [(start, stop) for value, start, stop in spans if not as_guid(value)]
        out[side] = sum(covered(lives, max(start, up_start), min(stop, up_stop))
                        for start, stop in bare for up_start, up_stop in up)
    return out


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

    # with neither key written the absent line above already says so
    if notes(trace, "anub.side0") or notes(trace, "anub.side1"):
        needed, latched = side_tanks(trace)
        human = any(guid in trace.humans for holders in latched.values() for guid in holders)
        if len(latched) < needed or human:
            def label(guid: int) -> str:
                return trace.name(guid) + (" (human)" if guid in trace.humans else "")

            print(f"side tanks latched: {len(latched)} of {needed} needed ("
                  + ", ".join(f"side {side} "
                              + (" > ".join(label(guid) for guid in latched[side]) if side in latched else "none")
                              for side in (0, 1)) + ")")

    if notes(trace, "anub.patch0") or notes(trace, "anub.patch1"):
        bare = {side: ms for side, ms in unpatched_ms(trace, Samples(trace)).items() if ms >= UNPATCHED_FLAG_MS}
        if bare:
            print("no tank patch with burrowers out while he was up: "
                  + ", ".join(f"side {side} {ms / 1000:.1f} s" for side, ms in sorted(bare.items())))


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


def spike_near(samples: Samples, spikes, guid: int, start: int, stop: int) -> int | None:
    """Milliseconds into the window the spike first came within KITE_PROTECT_RANGE of the kiter."""
    stamps, rows = samples.track(guid)
    for index in range(bisect.bisect_left(stamps, start), bisect.bisect_right(stamps, stop)):
        for spike in spikes:
            row = samples.recent(spike, stamps[index])
            if row and dist2(row[1:3], rows[index][1:3]) <= KITE_PROTECT_RANGE:
                return stamps[index] - start
    return None


def tank_patch_approach(samples: Samples, latches, spikes, start: int, stop: int) -> dict[int, float]:
    """Per side, the spike's closest approach in the window to the tank patch that side had latched at each
    sample. A spent patch stops being sampled, so its last read is the spike on top of it."""
    out: dict[int, float] = {}
    for spike in spikes:
        stamps, rows = samples.track(spike)
        for index in range(bisect.bisect_left(stamps, start), bisect.bisect_right(stamps, stop)):
            for side, spans in enumerate(latches):
                patch = latched_patch(spans, samples, stamps[index])
                if patch:
                    yards = dist2(rows[index][1:3], patch)
                    out[side] = min(out.get(side, yards), yards)
    return out


def stage_time(spans, start: int, stop: int) -> dict[str, int]:
    """Milliseconds per `anub.kitestage` value inside the window, `none` left out, in the order first held."""
    out: dict[str, int] = {}
    for value, low, high in spans:
        overlap = min(high, stop) - max(low, start)
        if value != "none" and overlap > 0:
            out[value] = out.get(value, 0) + overlap
    return out


def stage_name(trace: Trace, value: str) -> str:
    kind, _, spell = value.partition(":")
    if kind == "boost" and spell.isdigit():
        return f"boost {KITE_BOOSTS.get(int(spell)) or trace.spell(int(spell))}"
    return value


def boost_rows(trace: Trace) -> list[dict]:
    """Kite boost casts, triggered ones too, and Nitro Boosts' speed aura going on, as `{t, s, sp}`."""
    rows = [rec for rec in trace.of("cast") if rec.get("sp") in KITE_BOOSTS]
    rows += [{"t": rec["t"], "s": rec.get("d"), "sp": NITRO_SPEED} for rec in trace.of("aura")
             if rec.get("sp") == NITRO_SPEED and not rec.get("r")]
    return sorted(rows, key=lambda rec: rec["t"])


def kiter_boosts(rows: list[dict], guid: int, start: int, stop: int) -> list[dict]:
    """The kiter's boosts in the window, a boost read again within BOOST_PAIR_MS kept once."""
    out: list[dict] = []
    for rec in rows:
        if rec.get("s") != guid or not start <= rec["t"] <= stop:
            continue
        if out and KITE_BOOSTS[out[-1]["sp"]] == KITE_BOOSTS[rec["sp"]] and rec["t"] - out[-1]["t"] <= BOOST_PAIR_MS:
            continue
        out.append(rec)
    return out


def submerged_spans(trace: Trace) -> list[tuple[int, int]]:
    return [(start, stop) for start, stop, phase, _ in phase_spans(trace) if phase == 2]


def gather_centre(holds, samples: Samples, submerged, when: int, skip=frozenset()) -> tuple[float, float] | None:
    """The mean spot of the living bots whose `anub.gather` read `hold` at `when` while he was under, `skip`
    left out, None with nobody holding. The note never clears, so a hold that began before this submerge
    is stale."""
    under = next((start for start, stop in submerged if start <= when <= stop), None)
    if under is None:
        return None
    points = []
    for guid, spans in holds.items():
        began = next((start for value, start, stop in spans if value == "hold" and start <= when < stop), None)
        if guid in skip or began is None or began < under:
            continue
        row = samples.recent(guid, when)
        if row and row[5] > 0:
            points.append(row[1:3])
    if not points:
        return None
    return statistics.fmean(point[0] for point in points), statistics.fmean(point[1] for point in points)


def spike_at(samples: Samples, spikes, kiter: int, when: int):
    """The spike's last row at `when`, the one chasing `kiter` first should two be sampled."""
    rows = [row for row in (samples.recent(spike, when) for spike in spikes) if row]
    rows.sort(key=lambda row: len(row) <= 7 or row[7] != kiter)
    return rows[0] if rows else None


def unlatched_patches(samples: Samples, latches, when: int) -> list[int]:
    """Patches alive at `when` that no side held then."""
    held = {as_guid(held_at(spans, when)) for spans in latches}
    return sorted(guid for guid, kind in samples.sphere_kinds(when).items() if kind == "patch" and guid not in held)


def unit_speed(samples: Samples, guid: int, low: int, high: int) -> float | None:
    """The median yards per second over the unit's snapshot steps inside `[low, high]`, holes and steps
    under MOVING_YD_S left out; 0 for one sampled there that never moved, None for one never sampled."""
    stamps, rows = samples.track(guid)
    first, last = bisect.bisect_left(stamps, low), bisect.bisect_right(stamps, high)
    if first >= last:
        return None
    speeds = []
    for index in range(first + 1, last):
        span = stamps[index] - stamps[index - 1]
        if 0 < span <= MAX_STEP_MS:
            speed = dist2(rows[index][1:3], rows[index - 1][1:3]) * 1000.0 / span
            if speed >= MOVING_YD_S:
                speeds.append(speed)
    return statistics.median(speeds) if speeds else 0.0


def cover_speeds(samples: Samples, spikes, window: dict) -> list[dict]:
    """Each Hand of Protection on the kiter and each own immunity in the window, with the spike's speed over
    the second after it and the kiter's around it, else over the whole window. `slow` is a spike no faster
    than the kiter, so the cover went on while the kiter still outran it; None when either went unread."""
    guid, start, stop = window["guid"], window["start"], window["stop"]
    out = []
    for rec in sorted(window["hop"] + window["own"], key=lambda rec: rec["t"]):
        when = rec["t"]
        spike = spike_at(samples, spikes, guid, when) or spike_at(samples, spikes, guid, when + NEAR_MS)
        spike_speed = unit_speed(samples, spike[0], when - 500, when + 1000) if spike else None
        kiter_speed = unit_speed(samples, guid, when - 1000, when + 2000) or unit_speed(samples, guid, start, stop)
        slow = None
        if spike_speed is not None and kiter_speed:
            slow = spike_speed <= kiter_speed + SPEED_SLACK
        out.append({"rec": rec, "spike": spike_speed, "kiter": kiter_speed, "slow": slow})
    return out


def merged(spans: list[tuple[int, int]]) -> list[tuple[int, int]]:
    out: list[tuple[int, int]] = []
    for start, stop in sorted(spans):
        if out and start <= out[-1][1]:
            out[-1] = (out[-1][0], max(out[-1][1], stop))
        else:
            out.append((start, stop))
    return out


def stalled(samples: Samples, guid: int, start: int, stop: int) -> tuple[int, int]:
    """`(ms, longest)` of the window the kiter stood still: every STALL_MS stretch inside it, alive at both
    ends, over which it moved under STALL_YD, overlaps counted once."""
    stamps, rows = samples.track(guid)
    spans = []
    for index in range(bisect.bisect_left(stamps, start + STALL_MS), bisect.bisect_right(stamps, stop)):
        back = bisect.bisect_right(stamps, stamps[index] - STALL_MS) - 1
        if back < 0 or stamps[back] < start or stamps[index] - stamps[back] > STALL_MS + MAX_STEP_MS:
            continue
        if rows[index][5] > 0 and rows[back][5] > 0 and dist2(rows[index][1:3], rows[back][1:3]) < STALL_YD:
            spans.append((stamps[back], stamps[index]))
    runs = merged(spans)
    return covered(runs, start, stop), max((high - low for low, high in runs), default=0)


def flip_flops(moves: list[dict], start: int, stop: int) -> list[tuple[int, int, int]]:
    """`(first, last, reversals)` per stretch of the window the kiter's walk kept reversing. A jump is a move
    row FLIP_YD or more off the one before it, a reversal a jump heading back against the jump before it, and
    a stretch FLIP_COUNT or more reversals inside FLIP_MS, overlaps merged."""
    reversals = []
    last = jump = None
    for rec in moves:
        if not start <= rec["t"] <= stop:
            continue
        point = (rec["x"], rec["y"])
        if last is not None:
            step = (point[0] - last[0], point[1] - last[1])
            if math.hypot(*step) >= FLIP_YD:
                if jump is not None and step[0] * jump[0] + step[1] * jump[1] < 0:
                    reversals.append(rec["t"])
                jump = step
        last = point
    spans = [(reversals[index], reversals[index + FLIP_COUNT - 1])
             for index in range(len(reversals) - FLIP_COUNT + 1)
             if reversals[index + FLIP_COUNT - 1] - reversals[index] <= FLIP_MS]
    return [(low, high, sum(1 for when in reversals if low <= when <= high)) for low, high in merged(spans)]


def hop_readiness(trace: Trace) -> dict:
    """What `ready_paladin` reads: the bot paladins on the roster, each one's Hand of Protection casts, the
    marks and Forbearance."""
    paladins = {member["g"] for member in trace.header.get("roster", [])
                if member.get("c") == CLASS_PALADIN and not member.get("h")}
    casts: dict[int, list[int]] = collections.defaultdict(list)
    for rec in trace.of("cast"):
        if rec.get("sp") in HAND_OF_PROTECTION and not rec.get("tr") and rec.get("s") in paladins:
            casts[rec["s"]].append(rec["t"])
    return {"paladins": paladins, "casts": casts, "marks": mark_spans(trace),
            "forbearance": aura_spans(trace, (SPELL_FORBEARANCE,))}


def ready_paladin(samples: Samples, ready: dict, kiter: int, when: int) -> dict:
    """The nearest bot paladin that could Hand of Protection the kiter at `when`: alive, unmarked, its last one
    HOP_COOLDOWN_MS or more back. `guid` is None with none, and `forbearance` says the kiter carried it, which
    rules out every paladin."""
    forbearance = any(low <= when < high for low, high, _ in ready["forbearance"].get(kiter, []))
    out = {"t": when, "guid": None, "yards": None, "forbearance": forbearance}
    spot = samples.recent(kiter, when)
    if spot is None or forbearance:
        return out
    for guid in sorted(ready["paladins"] - {kiter}):
        row = samples.recent(guid, when)
        if row is None or row[5] <= 0:
            continue
        if any(marked == guid and low <= when < high for marked, low, high, _ in ready["marks"]):
            continue
        if any(when - HOP_COOLDOWN_MS < cast < when for cast in ready["casts"].get(guid, [])):
            continue
        gap = dist2(row[1:3], spot[1:3])
        if out["yards"] is None or gap < out["yards"]:
            out.update(guid=guid, yards=gap)
    return out


def mark_windows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    samples = samples or Samples(trace)
    impales = [rec for rec in trace.of("dmg") if rec.get("sp") in IMPALE]
    deaths = combat_deaths(trace)
    kite = per_bot(trace, "anub.kite")
    stages = holder_spans(trace, "anub.kitestage", pull_end(trace))
    holds = holder_spans(trace, "anub.gather", pull_end(trace))
    boosts = boost_rows(trace)
    used = {stop for _, stop in consumed_patches(trace, samples).values()}
    submerged = submerged_spans(trace)
    emerges = [stop for _, stop in submerged]
    casts = [rec for rec in trace.of("cast") if not rec.get("tr")]
    hops = [rec for rec in casts if rec.get("sp") in HAND_OF_PROTECTION]
    own = [rec for rec in casts if rec.get("sp") in KITER_OWN]
    protect = notes(trace, "anub.protect")
    spikes = guids_of_entry(trace, NPC_SPIKE)
    latches = patch_latches(trace)
    moves: dict[int, list[dict]] = collections.defaultdict(list)
    for rec in trace.of("move"):
        if rec.get("x") is not None and rec.get("y") is not None:
            moves[rec.get("g")].append(rec)
    feigns = aura_spans(trace, (SPELL_FEIGN_DEATH,))
    ready = hop_readiness(trace)

    out = []
    for guid, start, stop, closed in mark_spans(trace):
        hits = [rec for rec in impales
                if rec.get("d") == guid and start <= rec["t"] <= stop + IMPALE_SLACK_MS]
        died = next((rec["t"] for rec in deaths
                     if rec.get("g") == guid and start <= rec["t"] <= stop + IMPALE_SLACK_MS), None)
        consumed = closed and stop in used
        by_emerge = any(abs(stop - edge) <= EMERGE_SLACK_MS for edge in emerges)
        if died is not None:
            outcome = "death"
        elif hits:
            outcome = "impale"
        elif not closed:
            outcome = "open"
        elif not consumed and by_emerge:
            outcome = "emerge"
        else:
            # nothing hit the kiter and it lived, so a patch or a falling sphere took the spike
            outcome = "patch"
        immune = next((rec for rec in own if rec.get("s") == guid
                       and stop - IMMUNE_END_MS <= rec["t"] <= stop + IMPALE_SLACK_MS), None)
        ended = "death" if died is not None else None
        # the emerge despawns the spike, so a last-moment immunity there handed nothing on
        if ended is None and immune and closed and not by_emerge:
            ended = KITER_OWN[immune["sp"]]
        spike = spike_at(samples, spikes, guid, stop)
        centre = gather_centre(holds, samples, submerged, stop, {guid})
        to_gather = dist2(spike[1:3], centre) if spike and centre is not None else None
        stand = samples.row(guid, start)
        frost = [dist2(stand[1:3], point) for point in samples.frost_at(start)] if stand else []
        near = spike_near(samples, spikes, guid, start, stop)
        hands = [rec for rec in hops if rec.get("tgt") == guid and start <= rec["t"] <= stop]
        # read just before a hand that landed by then, its Forbearance would rule out every paladin
        asked = None if near is None else min([start + near] + [rec["t"] - 1 for rec in hands
                                                                  if rec["t"] <= start + near])
        out.append({"guid": guid, "start": start, "stop": stop,
                    "branches": branches_in(kite.get(guid, []), start, stop),
                    "impales": len(hits), "damage": sum(rec.get("a", 0) for rec in hits),
                    "outcome": outcome, "consumed": consumed, "died": died,
                    "near": near,
                    "escort": None if asked is None else ready_paladin(samples, ready, guid, asked),
                    "flips": flip_flops(moves.get(guid, []), start, stop),
                    "feign_drops": [low for low, high, done in feigns.get(guid, [])
                                    if done and start <= low <= stop + IMPALE_SLACK_MS
                                    and high - low <= FEIGN_DROP_MS],
                    "hop": hands,
                    "own": [rec for rec in own if rec.get("s") == guid and start <= rec["t"] <= stop],
                    "protect": [rec for rec in protect if start <= rec["t"] <= stop],
                    "stages": stage_time(stages.get(guid, []), start, stop),
                    "boosts": kiter_boosts(boosts, guid, start, stop),
                    "tank_near": tank_patch_approach(samples, latches, spikes, start, stop),
                    "ended": ended, "gather": centre, "to_gather": to_gather,
                    "kite_patches": unlatched_patches(samples, latches, stop),
                    "near_gather": ended is not None and to_gather is not None and to_gather <= KITE_END_GATHER_YD,
                    "frost_yd": min(frost) if frost else None,
                    "in_frost": bool(frost) and min(frost) <= PERMAFROST_SLOW_REACH,
                    "stalled": stalled(samples, guid, start, stop)})
        out[-1]["cover"] = cover_speeds(samples, spikes, out[-1])
    return out


def stray_impales(trace: Trace, windows: list[dict]) -> list[dict]:
    """Impale hits on anyone but the kiter of the window they landed in."""
    def kiter_hit(rec):
        return any(window["guid"] == rec.get("d")
                   and window["start"] <= rec["t"] <= window["stop"] + IMPALE_SLACK_MS for window in windows)

    return [rec for rec in trace.of("dmg") if rec.get("sp") in IMPALE and not kiter_hit(rec)]


def submerge_crowds(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per submerge: his last surfaced spot, the living roster within each CROWD_YD of it by role, and the
    first mark under him with the kiter's distance from the spike at the first snapshot of its mark."""
    samples = samples or Samples(trace)
    boss = boss_guid(trace)
    spikes = guids_of_entry(trace, NPC_SPIKE)
    marks = mark_spans(trace)
    roster = roster_guids(trace)
    out = []
    for start, stop, phase, label in phase_spans(trace):
        row = samples.row(boss, start - 1) if boss and phase == 2 else None
        if row is None:
            continue
        spot = (row[1], row[2])
        near = {yards: collections.Counter() for yards in CROWD_YD}
        for guid in roster:
            unit = samples.recent(guid, start - 1)
            if unit is None or unit[5] <= 0:
                continue
            gap = dist2(unit[1:3], spot)
            for yards in CROWD_YD:
                if gap <= yards:
                    near[yards][trace.role(guid)] += 1
        first = next((span for span in marks if start <= span[1] < stop), None)
        kiter = mark = spike_yd = None
        if first:
            kiter, mark = first[0], first[1]
            index = bisect.bisect_left(samples.stamps, mark)
            at = samples.stamps[index] if index < len(samples.stamps) else mark
            spike, unit = spike_at(samples, spikes, kiter, at), samples.recent(kiter, at)
            spike_yd = dist2(spike[1:3], unit[1:3]) if spike and unit else None
        out.append({"label": label, "start": start, "spot": spot, "near": near, "kiter": kiter, "mark": mark,
                    "spike_yd": spike_yd})
    return out


def run_spend(row: dict | None) -> bool:
    """A tank patch spent while its kiter ran or boosted, before the kite had come down to a patch."""
    stage = (row or {}).get("stage") or ""
    return stage == "run" or stage.startswith("boost")


def show_spike(trace: Trace) -> None:
    print("PURSUING SPIKE")
    samples = Samples(trace)
    windows = mark_windows(trace, samples)
    spends = {(row["kiter"], row["mark"]): row for row in spent_patches(trace, samples) if row["kiter"]}
    if not windows:
        print("  nobody carried Pursued by Anub'arak")
    else:
        print(f"  {'at':>9} {'kiter':14} {'role':6} {'secs':>5} {'kite branches':24} {'outcome':7}"
              f" {'impales':>7} {'damage':>8} {'near':>5}")
        for window in windows:
            branches = ">".join(window["branches"]) or "-"
            near = "-" if window["near"] is None else f"{window['near'] / 1000:.1f}"
            spend = spends.get((window["guid"], window["start"]))
            gone = (f"  tank patch {spend['side']} spent" if spend
                    else "  patch seen going" if window["consumed"] else "")
            if run_spend(spend):
                gone += f" in {stage_name(trace, spend['stage'])}"
            print(f"  {clock(window['start']):>9} {trace.name(window['guid'])[:14]:14}"
                  f" {trace.role(window['guid']):6} {(window['stop'] - window['start']) / 1000:5.1f}"
                  f" {branches[:24]:24} {window['outcome']:7} {window['impales']:7} {window['damage']:8,}"
                  f" {near:>5}" + gone)
            if window["in_frost"]:
                print(f"            marked {window['frost_yd']:.1f} yd from a Permafrost centre, inside its slow")
            cover = protection(trace, window)
            if cover:
                print(f"            {cover}")
            if window["cover"]:
                print(f"            speeds at the cover: {speed_line(window)}")
            escort = escort_line(trace, window)
            if escort:
                print(f"            {escort}")
            still, longest = window["stalled"]
            if still:
                print(f"            stood still {still / 1000:.1f} s while marked, longest {longest / 1000:.1f} s")
            for low, high, reversals in window["flips"]:
                print(f"            walk flip-flopped from {(low - window['start']) / 1000:+.1f} s for"
                      f" {(high - low) / 1000:.1f} s, {reversals} reversals")
            for low in window["feign_drops"]:
                print(f"            Feign Death dropped at once {(low - window['start']) / 1000:+.1f} s")
            run = kite_line(trace, window)
            if run:
                print(f"            {run}")
            approach = tank_line(window)
            if approach:
                print(f"            spike to tank patch: {approach}")
            print(f"            at the end: {end_line(trace, window)}")
        outcomes = collections.Counter(window["outcome"] for window in windows)
        print(f"\n  {len(windows)} window(s): "
              + ", ".join(f"{name} {n}" for name, n in outcomes.most_common()))
        reached = sum(1 for window in windows
                      if any(yards < SPIKE_PATCH_REACH for yards in window["tank_near"].values()))
        print(f"  spike within {SPIKE_PATCH_REACH} yd of a latched tank patch, its fail test's reach:"
              f" {reached} window(s)")
        near = [window for window in windows if window["near"] is not None]
        print(f"  near is the seconds in when the spike first came within {KITE_PROTECT_RANGE:.0f} yd of the"
              f" kiter: {len(near)} window(s), Hand of Protection in"
              f" {sum(1 for window in near if window['hop'])}, an own immunity in"
              f" {sum(1 for window in near if window['own'])}")
        stages: collections.Counter = collections.Counter()
        for window in windows:
            stages.update(window["stages"])
        if stages:
            print("  kitestage over every window: " + ", ".join(
                f"{stage_name(trace, value)} {ms / 1000:.1f} s" for value, ms in stages.most_common()))
        unasked_own = collections.Counter(KITER_OWN[rec["sp"]] for window in windows for rec in window["own"]
                                          if unasked(window, rec))
        print("  own immunity on a kiter anub.protect never asked for: "
              + (", ".join(f"{name} {n}" for name, n in unasked_own.most_common()) if unasked_own else "none"))
        flagged = [window for window in windows if window["near_gather"]]
        print(f"  kite ended by a death or an own immunity within {KITE_END_GATHER_YD:.0f} yd of the gather: "
              + (", ".join(f"{trace.name(window['guid'])} {clock(window['stop'])} {window['ended']}"
                           for window in flagged) or "none"))
        print("  the gather is the mean spot of the living bots holding anub.gather since he went under, the"
              " kiter left out")
        inside = [window for window in windows if window["in_frost"]]
        print(f"  marked within {PERMAFROST_SLOW_REACH} yd of a Permafrost centre: "
              + (", ".join(f"{trace.name(window['guid'])} {clock(window['start'])} {window['frost_yd']:.1f} yd"
                           for window in inside) or "none"))
        slow = [(window, row) for window in windows for row in window["cover"] if row["slow"]]
        print("  Hand of Protection or an own immunity on a spike no faster than the kiter: "
              + (", ".join(f"{trace.name(window['guid'])} {clock(row['rec']['t'])} {cover_name(row['rec'])}"
                           f" ({row['spike']:.1f} vs {row['kiter']:.1f} yd/s)" for window, row in slow) or "none"))
        still = [window for window in windows if window["stalled"][0]]
        print(f"  stood still while marked, under {STALL_YD} yd in {STALL_MS / 1000:.0f} s and alive: "
              + (", ".join(f"{trace.name(window['guid'])} {clock(window['start'])}"
                           f" {window['stalled'][0] / 1000:.1f} s" for window in still) or "none"))
        ran = [row for row in spends.values() if run_spend(row)]
        print("  tank patch spent while the kiter ran or boosted: "
              + (", ".join(f"{trace.name(row['kiter'])} {clock(row['gone'])} side {row['side']}"
                           f" {stage_name(trace, row['stage'])}" for row in ran) or "none"))
        print("  speeds are the median of the moving snapshot steps, the spike's over the second after the cover,"
              "\n  the kiter's around it")
        flipped = [window for window in windows if window["flips"]]
        print(f"  walk flip-flopped, move targets {FLIP_YD:.0f}+ yd apart reversing {FLIP_COUNT}+ times in"
              f" {FLIP_MS / 1000:.0f} s: " + (", ".join(
                  f"{trace.name(window['guid'])} {clock(window['start'])}"
                  f" from {(window['flips'][0][0] - window['start']) / 1000:+.1f} s" for window in flipped) or "none"))
        dropped = [(window, low) for window in windows for low in window["feign_drops"]]
        print(f"  Feign Death dropped within {FEIGN_DROP_MS} ms: " + (", ".join(
            f"{trace.name(window['guid'])} {clock(window['start'])} {(low - window['start']) / 1000:+.1f} s"
            for window, low in dropped) or "none"))
        neared = [window for window in windows if window["escort"]]
        far = [window for window in neared if window["escort"]["guid"] is None
               or window["escort"]["yards"] > HOP_RANGE]
        print(f"  nearest Hand of Protection-ready paladin as the spike came within {KITE_PROTECT_RANGE:.0f} yd:"
              f" within {HOP_RANGE:.0f} yd in {len(neared) - len(far)} of {len(neared)}; further or none: "
              + (", ".join(f"{trace.name(window['guid'])} {clock(window['start'])} ({escort_brief(trace, window)})"
                           for window in far) or "none"))
        print(f"  ready is a bot paladin alive and unmarked, its last Hand of Protection {HOP_COOLDOWN_MS // 1000} s"
              " or more back, the\n  shortest cooldown, with no Forbearance on the kiter")
    show_crowds(trace, samples)

    stray = stray_impales(trace, windows)
    if stray:
        hit = collections.Counter(rec.get("d") for rec in stray)
        print(f"  Impale on anyone else: {len(stray)} hit(s) for {sum(rec.get('a', 0) for rec in stray):,}: "
              + ", ".join(f"{trace.name(guid)} ({trace.role(guid)}) {n}" for guid, n in hit.most_common()))
    else:
        print("  Impale hit nobody but a kiter")
    show_counts(trace, "anub.dodge")
    show_counts(trace, "anub.protect")
    show_counts(trace, "anub.gather")


def unasked(window: dict, rec: dict) -> bool:
    """An own immunity with no `anub.protect` self from the kiter in the 2 s before it."""
    return not any(str(note.get("txt")) == "self" and note.get("g") == window["guid"]
                   and 0 <= rec["t"] - note["t"] <= LAST_RESORT_LEAD_MS for note in window["protect"])


def protection(trace: Trace, window: dict) -> str:
    """Hand of Protection on the kiter, its own immunities and the `anub.protect` duties in one window."""
    def rel(rec) -> str:
        return f"{(rec['t'] - window['start']) / 1000:+.1f} s"

    parts = [f"Hand of Protection by {trace.name(rec.get('s'))} {rel(rec)}" for rec in window["hop"]]
    parts += [f"{KITER_OWN[rec['sp']]} {rel(rec)}" + (" unasked" if unasked(window, rec) else "")
              for rec in window["own"]]
    parts += [f"anub.protect {trace.name(rec.get('g'))} {rec.get('txt')} {rel(rec)}"
              for rec in window["protect"] if str(rec.get("txt")) != "none"]
    return "; ".join(parts)


def cover_name(rec: dict) -> str:
    return KITER_OWN.get(rec.get("sp")) or "Hand of Protection"


def speed_line(window: dict) -> str:
    """The spike's speed and the kiter's at each cover in one window, a spike no faster flagged."""
    def speed(value) -> str:
        return "-" if value is None else f"{value:.1f} yd/s"

    return "; ".join(f"{cover_name(row['rec'])} {(row['rec']['t'] - window['start']) / 1000:+.1f} s: spike"
                     f" {speed(row['spike'])}, kiter {speed(row['kiter'])}" + (", no faster" if row["slow"] else "")
                     for row in window["cover"])


def escort_brief(trace: Trace, window: dict) -> str:
    """The nearest Hand of Protection-ready paladin as the spike first came within KITE_PROTECT_RANGE."""
    escort = window["escort"]
    if escort["forbearance"]:
        return "Forbearance on the kiter"
    if escort["guid"] is None:
        return "none ready"
    return f"{trace.name(escort['guid'])} {escort['yards']:.1f} yd"


def escort_line(trace: Trace, window: dict) -> str:
    if window["escort"] is None:
        return ""
    escort = window["escort"]
    far = escort["guid"] is not None and escort["yards"] > HOP_RANGE
    return (f"spike within {KITE_PROTECT_RANGE:.0f} yd +{window['near'] / 1000:.1f} s, nearest"
            f" Hand of Protection-ready paladin: {escort_brief(trace, window)}"
            + (f", past its {HOP_RANGE:.0f} yd reach" if far else ""))


def show_crowds(trace: Trace, samples: Samples) -> None:
    rows = submerge_crowds(trace, samples)
    if not rows:
        return
    print(f"\n  per submerge, the living players within {', '.join(f'{yards:.0f}' for yards in CROWD_YD)} yd of where"
          " he went down, by role,\n  and the first kiter's distance from the spike at its mark:")
    for row in rows:
        rings = "; ".join(f"{yards:.0f} yd {sum(counts.values())}"
                          + (" (" + ", ".join(f"{role} {counts[role]}" for role in CROWD_ROLES if counts[role]) + ")"
                             if counts else "")
                          for yards, counts in row["near"].items())
        first = "no mark"
        if row["kiter"] is not None:
            gap = "spike unsampled" if row["spike_yd"] is None else f"{row['spike_yd']:.1f} yd from the spike"
            first = f"first mark {trace.name(row['kiter'])} {clock(row['mark'])}, {gap}"
        print(f"    {row['label']:5} {clock(row['start']):>9} at {row['spot'][0]:.1f}, {row['spot'][1]:.1f}: {rings}"
              f"\n          {first}")


def tank_line(window: dict) -> str:
    """The spike's closest approach to each latched tank patch in one window."""
    return ", ".join(f"side {side} {yards:.1f} yd" + (" in reach" if yards < SPIKE_PATCH_REACH else "")
                     for side, yards in sorted(window["tank_near"].items()))


def end_line(trace: Trace, window: dict) -> str:
    """Where the mark's end left the spike against the gather, the kite patches alive then, and what ended it."""
    if window["gather"] is None:
        parts = ["nobody holding the gather"]
    elif window["to_gather"] is None:
        parts = ["spike unsampled"]
    else:
        parts = [f"spike {window['to_gather']:.1f} yd from the gather"]
    patches = ", ".join(short(trace, guid) for guid in window["kite_patches"])
    parts.append(f"kite patch {patches}" if patches else "no kite patch")
    if window["ended"]:
        parts.append(f"ended by {window['ended']}"
                     + (f" within {KITE_END_GATHER_YD:.0f} yd of the gather" if window["near_gather"] else ""))
    return "; ".join(parts)


def kite_line(trace: Trace, window: dict) -> str:
    """Time in each kite stage and the kiter's boosts in one window."""
    stages = ", ".join(f"{stage_name(trace, value)} {ms / 1000:.1f} s" for value, ms in window["stages"].items())
    boosts = ", ".join(f"{KITE_BOOSTS[rec['sp']]} {(rec['t'] - window['start']) / 1000:+.1f} s"
                       for rec in window["boosts"])
    return "; ".join(part for part in (stages and f"kitestage {stages}", boosts and f"boosts {boosts}") if part)


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


def supply_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per mark window, the spheres at the mark: patches split by whether a side latched them,
    flying and falling ones, and every bot that held a sphere duty during the window."""
    samples = samples or Samples(trace)
    latches = patch_latches(trace)
    duty = holder_spans(trace, "anub.sphere", pull_end(trace))
    out = []
    for window in mark_windows(trace, samples):
        start, stop = window["start"], window["stop"]
        kinds = samples.sphere_kinds(start)
        tank = {as_guid(held_at(spans, start)) for spans in latches} - {0}
        patches = [guid for guid, kind in kinds.items() if kind == "patch"]
        holders = sorted({(guid, label) for guid, spans in duty.items() for label, low, high in spans
                          if label != "none" and low < stop and high > start})
        out.append({"guid": window["guid"], "start": start, "outcome": window["outcome"],
                    "tank": sum(1 for guid in patches if guid in tank),
                    "other": sum(1 for guid in patches if guid not in tank),
                    "flying": sum(1 for kind in kinds.values() if kind == "flying"),
                    "falling": sum(1 for kind in kinds.values() if kind == "falling"),
                    "holders": holders})
    return out


def duty_spans(trace: Trace) -> list[dict]:
    """Each stretch one bot held one `anub.sphere` duty, with its casts at a sphere and its `drop target`
    rows meanwhile. The note names the duty, not the sphere, so a cast at any sphere counts."""
    spheres = guids_of_entry(trace, NPC_FROST_SPHERE)
    casts = [rec for rec in trace.of("cast") if rec.get("tgt") in spheres and not rec.get("tr")]
    drops = [rec for rec in trace.of("act") if rec.get("a") == "drop target"]
    out = []
    for guid, spans in holder_spans(trace, "anub.sphere", pull_end(trace)).items():
        for label, start, stop in spans:
            if label == "none":
                continue
            out.append({"guid": guid, "label": label, "start": start, "stop": stop,
                        "casts": sum(1 for rec in casts if rec.get("s") == guid and start <= rec["t"] < stop),
                        "drops": sum(1 for rec in drops if rec.get("g") == guid and start <= rec["t"] < stop)})
    return sorted(out, key=lambda row: (row["start"], row["guid"]))


def patch_formed(samples: Samples, guid: int) -> int | None:
    """When a Permafrost centre first sat on the sphere."""
    stamps, rows = samples.track(guid)
    for when, row in zip(stamps, rows):
        if row[3] < SPHERE_PATCH_Z and any(dist2(row[1:3], centre) <= PATCH_MATCH_YD
                                           for centre in samples.frost_at(when)):
            return when
    return None


def kill_time(samples: Samples, guid: int) -> tuple[int, list] | None:
    """When the sphere died and the row it lay on: PERMAFROST_DELAY_MS before its patch formed, else the
    first row of the low run the trace ended it in. None for one still flying."""
    formed = patch_formed(samples, guid)
    if formed is not None:
        return formed - PERMAFROST_DELAY_MS, samples.row(guid, formed)
    stamps, rows = samples.track(guid)
    if not rows or rows[-1][3] > SPHERE_FLYING_Z:
        return None
    index = len(rows) - 1
    while index and rows[index - 1][3] <= SPHERE_FLYING_Z:
        index -= 1
    return stamps[index], rows[-1]


def duty_side(label: str | None) -> int | None:
    """The side an `anub.sphere` duty serves: `0` and `grip0` side 0, `1` and `grip1` side 1."""
    side = (label or "").removeprefix("grip")
    return int(side) if side in ("0", "1") else None


def spell_shape(spell) -> tuple[str, ...]:
    return tuple(name for name, ids in (("periodic", PERIODIC), ("area", AREA)) if spell in ids)


def sphere_deaths(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Each sphere that died: when, what killed it, where it landed, and how far that is from its side's
    spot at the kill. Its side is the latch that named it, else the killer's sphere duty at the blow."""
    samples = samples or Samples(trace)
    spheres = guids_of_entry(trace, NPC_FROST_SPHERE)
    latches = patch_latches(trace)
    noted = spot_spans(trace)
    duty = holder_spans(trace, "anub.sphere", pull_end(trace))
    hits = [rec for rec in trace.of("dmg") if rec.get("d") in spheres]
    # a triggered cast is a proc or a debuff, but Starfall's stars and Explosive Shot's ticks come in
    # triggered too
    casts = [rec for rec in trace.of("cast") if rec.get("tgt") in spheres and rec.get("sp") not in NO_DAMAGE
             and (not rec.get("tr") or spell_shape(rec.get("sp")))]
    out = []
    for guid in sorted(spheres):
        death = kill_time(samples, guid)
        if death is None:
            continue
        killed, row = death
        source = "dmg"
        mine = [rec for rec in hits if rec["d"] == guid and rec["t"] <= killed]
        if not mine:
            source = "cast"
            mine = [rec for rec in casts if rec["tgt"] == guid and rec["t"] <= killed]
        blow = mine[-1] if mine else None
        caster = blow.get("s") if blow else None
        killer = trace.owners.get(caster, caster)
        held = held_at(duty.get(killer, []), blow["t"]) if blow else None
        named = [side for side, spans in enumerate(latches) if any(as_guid(value) == guid for value, _, _ in spans)]
        side = named[0] if named else duty_side(held)
        spot = None if side is None else side_spot(latches, samples, side, killed, noted)
        landed = (row[1], row[2])
        out.append({"guid": guid, "killed": killed, "landed": landed,
                    "killer": killer, "pet": caster if killer != caster else None,
                    "spell": blow.get("sp") if blow else None, "source": source if blow else None,
                    "shape": spell_shape(blow.get("sp")) if blow else (),
                    "duty": held, "side": side, "latched": bool(named),
                    "spot": spot, "off_spot": None if spot is None else dist2(landed, spot)})
    return sorted(out, key=lambda row: row["killed"])


def grip_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Each Death Grip at a sphere, and how long after it the sphere first sat within GRIP_LANDED yd and
    GRIP_LANDED_HEIGHT of where the DK stood at the cast, inside GRIP_JUDGE_MS: the grip pulls it to that
    spot, wherever the DK walks after. `closest` is its nearest ground gap meanwhile. `served` is the side
    its grip duty was for, `latched` the side whose `anub.patchN` later named it. `step` is the DK's first
    `anub.gripstep` of `kill` or `none` from the cast to its next grip and `step_ms` how long after the cast:
    `kill` held the duty to the kill, `none` dropped it. `rose` is when the sphere was first back above
    SPHERE_FLYING_Z after it came down, before the next grip at it."""
    samples = samples or Samples(trace)
    spheres = guids_of_entry(trace, NPC_FROST_SPHERE)
    duty = holder_spans(trace, "anub.sphere", pull_end(trace))
    steps = per_bot(trace, "anub.gripstep")
    latches = patch_latches(trace)
    casts = [rec for rec in trace.of("cast")
             if rec.get("sp") == SPELL_DEATH_GRIP and rec.get("tgt") in spheres and not rec.get("tr")]
    out = []
    for index, rec in enumerate(casts):
        dk, sphere, start = rec.get("s"), rec["tgt"], rec["t"]
        upto = next((other["t"] for other in casts[index + 1:] if other.get("s") == dk), None)
        again = next((other["t"] for other in casts[index + 1:] if other["tgt"] == sphere), None)
        step = next(((value, when - start) for when, value in steps.get(dk, [])
                     if when >= start and (upto is None or when < upto) and value in ("kill", "none")), (None, None))
        landed = closest = None
        stand = samples.recent(dk, start)
        stamps, rows = samples.track(sphere)
        judged = range(bisect.bisect_left(stamps, start), bisect.bisect_right(stamps, start + GRIP_JUDGE_MS))
        for index in judged if stand else ():
            gap = dist2(rows[index][1:3], stand[1:3])
            closest = gap if closest is None else min(closest, gap)
            if gap <= GRIP_LANDED and abs(rows[index][3] - stand[3]) <= GRIP_LANDED_HEIGHT:
                landed = stamps[index] - start
                break
        label = held_at(duty.get(dk, []), start)
        served = int(label[-1]) if label in ("grip0", "grip1") else None
        latched = next((side for side in (0, 1) for value, begin, _ in latches[side]
                        if begin >= start and as_guid(value) == sphere), None)
        out.append({"t": start, "dk": dk, "sphere": sphere, "duty": label, "landed": landed, "closest": closest,
                    "cast_at": (stand[1], stand[2]) if stand else None, "served": served, "latched": latched,
                    "step": step[0], "step_ms": step[1], "rose": back_up(samples, sphere, start, again)})
    return out


def back_up(samples: Samples, sphere: int, start: int, until: int | None = None) -> int | None:
    """Milliseconds from `start` to the sphere's first sample back above SPHERE_FLYING_Z once it had come
    below it, before `until`. A killed sphere falls to a patch and never rises."""
    stamps, rows = samples.track(sphere)
    low = False
    for index in range(bisect.bisect_left(stamps, start), len(stamps)):
        if until is not None and stamps[index] >= until:
            break
        if rows[index][3] <= SPHERE_FLYING_Z:
            low = True
        elif low:
            return stamps[index] - start
    return None


def last_grip(grips: list[dict], sphere: int, before: int, since: int | None = None) -> dict | None:
    """The last Death Grip at the sphere cast at or before `before`, and not before `since`."""
    return next((row for row in reversed(grips) if row["sphere"] == sphere and row["t"] <= before
                 and (since is None or row["t"] >= since)), None)


def first_patches(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per side, the first sphere its `anub.patchN` latched: when, the last Death Grip at it before then and its
    death row. `guid` is None for a side that never latched one."""
    samples = samples or Samples(trace)
    grips = grip_rows(trace, samples)
    deaths = {row["guid"]: row for row in sphere_deaths(trace, samples)}
    out = []
    for side, spans in enumerate(patch_latches(trace)):
        when, guid = next(((start, as_guid(value)) for value, start, _ in spans if as_guid(value)), (None, None))
        out.append({"side": side, "t": when, "guid": guid,
                    "grip": last_grip(grips, guid, when) if guid else None, "death": deaths.get(guid)})
    return out


def spent_patches(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Each tank patch a side held as it went while he was down: its side and submerge, its last sample, the
    mark that ended on it (else the one running then) with that kiter's last kite stage and the seconds
    into its window, and in `back` the side's next latch: when, a Death Grip at it since, its death row."""
    samples = samples or Samples(trace)
    end = pull_end(trace)
    latches = patch_latches(trace)
    lives = patch_lives(trace, samples)
    used = consumed_patches(trace, samples)
    last = samples.stamps[-1] if samples.stamps else 0
    spans = phase_spans(trace)
    marks = mark_spans(trace)
    stages = holder_spans(trace, "anub.kitestage", end)
    grips = grip_rows(trace, samples)
    deaths = {row["guid"]: row for row in sphere_deaths(trace, samples)}
    out = []
    for side, latch in enumerate(latches):
        for guid in dict.fromkeys(as_guid(value) for value, _, _ in latch):
            if not guid or guid not in lives or lives[guid][1] >= last:
                continue
            gone = lives[guid][1]
            # the latch clearing and the patch's last sample can land a tick apart, either way round
            if not any(as_guid(value) == guid and start <= gone < stop + NEAR_MS for value, start, stop in latch):
                continue
            under = next(((start, stop, label) for start, stop, phase, label in spans
                          if phase == 2 and start <= gone < stop), None)
            if under is None:
                continue
            row = {"side": side, "guid": guid, "gone": gone, "label": under[2], "start": under[0],
                   "emerge": under[1], "kiter": None, "mark": None, "into": None, "stage": None}
            if guid in used:
                hit = used[guid][1]
                mark = next((span for span in marks if span[3] and span[2] == hit), None)
            else:
                hit = gone
                mark = next((span for span in marks if span[1] <= gone <= span[2]), None)
            if mark:
                kiter, low, high, _ = mark
                held = [value for value, begin, stop in stages.get(kiter, [])
                        if value != "none" and begin < high and stop > low]
                row.update({"kiter": kiter, "mark": low, "into": hit - low, "stage": held[-1] if held else None})
            when, again = next(((start, as_guid(value)) for value, start, _ in latch
                                if start > gone and as_guid(value) not in (0, guid)), (None, None))
            row["back"] = None if again is None else {
                "t": when, "guid": again, "grip": last_grip(grips, again, when, gone), "death": deaths.get(again)}
            out.append(row)
    return sorted(out, key=lambda row: row["gone"])


def allowance_rows(trace: Trace, deaths: list[dict], spent: list[dict]) -> list[dict]:
    """Per submerge: the `anub.allow` written as he went under and when it next read 0 inside it, the spheres
    that died while he was down (`tank` the ones a side owned, `other` the rest), and the tank patches gone.
    `over` is heroic only, where the allowance is the tank patches the kiter may spend."""
    spans = latch_spans(trace, "anub.allow", pull_end(trace))
    heroic = is_heroic(trace)
    out = []
    for start, stop, phase, label in phase_spans(trace):
        if phase != 2:
            continue
        # a row from another bot's tick can trail the phase edge
        value = held_at(spans, min(start + NEAR_MS, stop - 1))
        allow = int(value) if value and value.isdigit() else None
        dropped = next((low for held, low, _ in spans if held == "0" and start + NEAR_MS < low < stop),
                       None) if allow else None
        made = [row for row in deaths if start <= row["killed"] < stop]
        mine = [row for row in spent if row["start"] == start]
        out.append({"label": label, "start": start, "allow": allow, "dropped": dropped,
                    "tank": sum(1 for row in made if row["side"] is not None),
                    "other": sum(1 for row in made if row["side"] is None), "spent": mine,
                    "over": heroic and allow is not None and len(mine) > allow})
    return out


def patch_source(trace: Trace, grip: dict | None, death: dict | None) -> str:
    """How a patch came down: a Death Grip at it and whether it landed, else shots; then its killer and its
    landing against its side's spot."""
    text = "from shots"
    if grip:
        verdict = "missed" if grip["landed"] is None else f"landed +{grip['landed'] / 1000:.1f} s"
        text = f"from a grip by {trace.name(grip['dk'])} at {clock(grip['t'])}, {verdict}"
    if death and death["killer"]:
        text += (f", killed by {trace.name(death['killer'])}" + ("'s pet" if death["pet"] else "")
                 + ("" if death["spell"] is None else f" {trace.spell(death['spell'])}"))
    if death and death["off_spot"] is not None:
        text += f", {death['off_spot']:.1f} yd off its spot"
    return text


def spend_line(trace: Trace, row: dict) -> str:
    head = f"side {row['side']} {short(trace, row['guid'])} gone {clock(row['gone'])}"
    if row["kiter"] is None:
        return head + ", nobody marked"
    stage = stage_name(trace, row["stage"]) if row["stage"] else "-"
    return head + f", {trace.name(row['kiter'])}'s mark {row['into'] / 1000:.1f} s in, kitestage {stage}"


def back_line(trace: Trace, row: dict) -> str:
    back = row["back"]
    if back is None:
        return "not back by the end"
    after = (back["t"] - row["emerge"]) / 1000
    edge = f"{after:.1f} s after the emerge" if after >= 0 else f"{-after:.1f} s before the emerge"
    return (f"back {clock(back['t'])}, {edge}: {short(trace, back['guid'])}"
            f" {patch_source(trace, back['grip'], back['death'])}")


def show_tank_patches(trace: Trace, samples: Samples) -> None:
    print("\n  first tank patch per side, at its anub.patchN latch:")
    for row in first_patches(trace, samples):
        if row["guid"] is None:
            print(f"    side {row['side']}  never latched one")
        else:
            print(f"    side {row['side']}  {clock(row['t']):>9}  {short(trace, row['guid'])}"
                  f" {patch_source(trace, row['grip'], row['death'])}")

    rows = allowance_rows(trace, sphere_deaths(trace, samples), spent_patches(trace, samples))
    if not rows:
        return
    print("\n  per submerge: anub.allow as he went under, the spheres that died while he was down (tank the ones a"
          "\n  side owned, other the rest) and the latched tank patches gone meanwhile, over the allowance in heroic"
          " flagged:")
    for row in rows:
        allow = "-" if row["allow"] is None else str(row["allow"])
        print(f"    {row['label']:5} {clock(row['start']):>9}  allowance {allow:>2}  tank {row['tank']}"
              f"  other {row['other']}  spent {len(row['spent'])}" + ("  over" if row["over"] else "")
              + ("" if row["dropped"] is None else f", allowance 0 from {clock(row['dropped'])}"))
        for spend in row["spent"]:
            print(f"      {spend_line(trace, spend)}\n        {back_line(trace, spend)}")
    print(f"  a patch is gone at its last sample; its mark is the one that ended within {CONSUMED_MS / 1000:.0f} s"
          " of it, else the one\n  running then, with the seconds into it and its last kitestage; back is that"
          " side's next anub.patchN latch")


def show_deaths(trace: Trace, samples: Samples) -> None:
    deaths = sphere_deaths(trace, samples)
    if not deaths:
        print("  no sphere died")
        return
    print("\n  each sphere that died, killed by its last dmg row, else the last cast at it before the kill:")
    print(f"  {'sphere':8} {'died':>9} {'killer':18} {'spell':26} {'from':4} {'shape':13} {'side':10}"
          f" {'off spot':>8}")
    for row in deaths:
        killer = "-"
        if row["killer"]:
            killer = trace.name(row["killer"]) + ("'s pet" if row["pet"] else "")
        spell = "-" if row["spell"] is None else trace.spell(row["spell"])
        side = row["duty"] if row["duty"] not in (None, "none") else "-"
        if row["side"] is not None:
            side = f"{row['side']}" + (" latched" if row["latched"] else "")
        off = "-" if row["off_spot"] is None else f"{row['off_spot']:.1f} yd"
        print(f"  {short(trace, row['guid']):8} {clock(row['killed']):>9} {killer[:18]:18} {spell[:26]:26}"
              f" {row['source'] or '-':4} {'+'.join(row['shape']) or '-':13} {side[:10]:10} {off:>8}")
    print(f"  died is {PERMAFROST_DELAY_MS / 1000:.1f} s before its patch formed; side is the latch that named"
          " it, else the killer's\n  sphere duty; off spot is how far it landed from that side's spot at the"
          f" kill: {TANK_PATCH_OFFSET:.0f} yd off the\n  room centre in y, or {SIDE_SPOT_GAP:.0f} yd off the"
          " other side's patch once that one latched,\n  or the anub.spotN the side held then where the trace"
          " has one")

    spheres = guids_of_entry(trace, NPC_FROST_SPHERE)
    shaped = collections.Counter((trace.owners.get(rec.get("s"), rec.get("s")), rec.get("sp"))
                                 for rec in trace.of("cast")
                                 if rec.get("tgt") in spheres and spell_shape(rec.get("sp")))
    print("  periodic or area spells aimed at a sphere: " + (", ".join(
        f"{trace.name(guid)} {trace.spell(spell)} {n}" for (guid, spell), n in shaped.most_common()) or "none"))


def grip_after(row: dict) -> str:
    """What the DK's grip duty did after the cast, and a sphere that floated back up."""
    parts = []
    if row["step"] == "kill":
        parts.append(f"duty held to kill +{row['step_ms'] / 1000:.1f} s")
    elif row["step"] == "none":
        parts.append(f"duty dropped to none +{row['step_ms'] / 1000:.1f} s, never at kill")
    if row["rose"] is not None:
        parts.append(f"sphere back above z {SPHERE_FLYING_Z:.0f} +{row['rose'] / 1000:.1f} s")
    return "; ".join(parts)


def spot_changes(trace: Trace) -> list[dict]:
    """Each `anub.spot0` and `anub.spot1` change in time order: the side, when, the spot, and how far it
    moved from that side's last one."""
    out = []
    for side, spans in enumerate(spot_spans(trace)):
        last = None
        for spot, start, _ in spans:
            out.append({"side": side, "t": start, "spot": spot,
                        "moved": dist2(spot, last) if spot and last else None})
            last = spot or last
    return sorted(out, key=lambda row: (row["t"], row["side"]))


def show_spots(trace: Trace) -> None:
    rows = spot_changes(trace)
    if not rows:
        print("  anub.spot0 and anub.spot1: no rows")
        return
    print("\n  tank spots from anub.spot0 and anub.spot1, each change:")
    for row in rows:
        spot = "unparsed" if row["spot"] is None else f"{row['spot'][0]:.1f}, {row['spot'][1]:.1f}"
        moved = "" if row["moved"] is None else f"  {row['moved']:.1f} yd on"
        print(f"    side {row['side']}  {clock(row['t']):>9}  {spot}{moved}")


def show_grips(trace: Trace, samples: Samples) -> None:
    grips = grip_rows(trace, samples)
    if not grips:
        print("  no Death Grip at a sphere")
    else:
        print(f"\n  Death Grip at a sphere, landed once it sat within {GRIP_LANDED:.0f} yd of the DK's spot at the"
              f" cast inside {GRIP_JUDGE_MS / 1000:.0f} s:")
        for row in grips:
            verdict = "missed, sphere unsampled"
            if row["cast_at"] is None:
                verdict = "unjudged, DK unsampled at the cast"
            elif row["landed"] is not None:
                verdict = f"landed +{row['landed'] / 1000:.1f} s"
            elif row["closest"] is not None:
                verdict = f"missed, closest {row['closest']:.1f} yd"
            if row["latched"] is not None:
                verdict += f", latched side {row['latched']}"
                if row["served"] is not None and row["latched"] != row["served"]:
                    verdict += f", not its grip's side {row['served']}"
            print(f"    {clock(row['t']):>9} {trace.name(row['dk'])[:14]:14} {row['duty'] or '-':6}"
                  f" {short(trace, row['sphere']):8} {verdict}")
            after = grip_after(row)
            if after:
                print(f"              {after}")
        dropped = [row for row in grips if row["step"] != "kill"]
        rose = [row for row in grips if row["rose"] is not None]
        print(f"  grip duty never at kill: {len(dropped)} of {len(grips)}; gripped sphere back above z"
              f" {SPHERE_FLYING_Z:.0f}: " + (", ".join(f"{short(trace, row['sphere'])} {clock(row['t'] + row['rose'])}"
                                                       for row in rose) or "none"))
    spans = latch_spans(trace, "anub.grip", pull_end(trace))
    if spans:
        print("  anub.grip: " + ", ".join(f"{GRIP_STATES.get(value, value)} from {clock(start)}"
                                          for value, start, _ in spans))
    show_counts(trace, "anub.gripstep")


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

    supply = supply_rows(trace, samples)
    if supply:
        print("\n  spheres at each mark, tank being the patches a side had latched and other the rest:")
        print(f"  {'at':>9} {'kiter':14} {'tank':>4} {'other':>5} {'flying':>6} {'falling':>7}"
              f" {'outcome':7} sphere duty in the window")
        for row in supply:
            holders = ", ".join(f"{trace.name(guid)} {label}" for guid, label in row["holders"]) or "-"
            print(f"  {clock(row['start']):>9} {trace.name(row['guid'])[:14]:14} {row['tank']:4} {row['other']:5}"
                  f" {row['flying']:6} {row['falling']:7} {row['outcome']:7} {holders}")

    spans = duty_spans(trace)
    if not spans:
        print("  nobody held a sphere duty")
    else:
        print(f"\n  {'duty from':>11} {'to':>9} {'holder':14} {'duty':5} {'casts':>5} {'drops':>5}")
        for row in spans[:TIMELINE_SHOWN]:
            print(f"  {clock(row['start']):>11} {clock(row['stop']):>9} {trace.name(row['guid'])[:14]:14}"
                  f" {row['label']:5} {row['casts']:5} {row['drops']:5}")
        if len(spans) > TIMELINE_SHOWN:
            print(f"  ... {len(spans) - TIMELINE_SHOWN} more span(s)")
        by_label: dict[str, list[dict]] = collections.defaultdict(list)
        for row in spans:
            by_label[row["label"]].append(row)
        print("  spans with a cast: " + ", ".join(
            f"{'side ' + label if label.isdigit() else label} {sum(1 for row in rows if row['casts'])} of {len(rows)}"
            for label, rows in sorted(by_label.items())))
        print("  casts are the holder's at any sphere; drops are its drop target rows, one per verdict change")

    show_deaths(trace, samples)
    show_grips(trace, samples)
    show_spots(trace)
    show_tank_patches(trace, samples)


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
    """Per burrower, its life and Shadow Strikes. `ups` is each time it came up: first sight, then each
    emerge. `died` is its last sample, None for one still sampled in the trace's last snapshot."""
    samples = samples or Samples(trace)
    strikes = [rec for rec in trace.of("cast") if rec.get("sp") == SPELL_SHADOW_STRIKE]
    hits = [rec for rec in trace.of("dmg") if rec.get("sp") == SPELL_SHADOW_STRIKE]
    last = samples.stamps[-1] if samples.stamps else 0
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
        emerges = [points[index][0] for index in range(1, len(points)) if returned(points[index - 1], points[index])]
        out.append({
            "guid": guid,
            "first": stamps[0],
            "last": stamps[-1],
            "low": min(hp for _, hp in points),
            "end": points[-1][1],
            "submerges": len(emerges),
            "ups": [stamps[0], *emerges],
            "died": stamps[-1] if stamps[-1] < last else None,
            "frost": sum(1 for when, row in zip(stamps, rows)
                         if on_permafrost(samples, row, when)) / len(rows),
            "strikes": len(mine),
            "landed": landed,
        })
    return out


def burrower_windows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per burrower, its wave, the last time it came up, whether that was an emerge, and whether it died
    inside BURROWER_SAFE_MS of it, before the core could submerge it. `inside` is None for one alive at
    the end."""
    samples = samples or Samples(trace)
    waves = wave_starts(trace, samples)
    out = []
    for row in burrower_rows(trace, samples):
        up, died = row["ups"][-1], row["died"]
        out.append({"guid": row["guid"], "wave": wave_of(waves, row["first"]), "up": up,
                    "emerged": len(row["ups"]) > 1, "died": died,
                    "inside": None if died is None else died - up <= BURROWER_SAFE_MS})
    return out


def as_guid(value) -> int:
    text = str(value).strip()
    return int(text) if text.isdigit() else 0


def focus_rows(trace: Trace) -> list[dict]:
    """Per burrower `anub.focus` held: the time it held it, and the share of living DPS bot samples
    (melee and ranged roles, humans left out) whose target it was meanwhile."""
    spans = [(as_guid(value), start, stop) for value, start, stop in latch_spans(trace, FOCUS, pull_end(trace))]
    starts = [start for _, start, _ in spans]
    dps = {guid for guid in roster_guids(trace) if trace.role(guid) in DPS_ROLES and guid not in trace.humans}

    held: collections.Counter = collections.Counter()
    for guid, start, stop in spans:
        if guid:
            held[guid] += stop - start

    on: collections.Counter = collections.Counter()
    total: collections.Counter = collections.Counter()
    for snap in frames(trace):
        index = bisect.bisect_right(starts, snap["t"]) - 1
        if index < 0:
            continue
        focus, _, stop = spans[index]
        if not focus or snap["t"] >= stop:
            continue
        for row in snap.get("u", []):
            if row[0] in dps and len(row) > 7 and row[5] > 0:
                total[focus] += 1
                on[focus] += row[7] == focus

    return [{"guid": guid, "held": held[guid], "share": on[guid] / total[guid] if total[guid] else None}
            for guid in sorted(held)]


def wave_starts(trace: Trace, samples: Samples | None = None) -> list[tuple[int, int]]:
    """`(wave, spawn)` per burrower wave from `anub.wave`, else from burrower spawns WAVE_SPLIT_MS apart."""
    waves = [(int(value), start) for value, start, _ in latch_spans(trace, "anub.wave", pull_end(trace))
             if value.isdigit() and int(value) > 0]
    if waves:
        return waves
    samples = samples or Samples(trace)
    spawns = sorted(samples.track(guid)[0][0] for guid in guids_of_entry(trace, NPC_BURROWER)
                    if samples.track(guid)[0])
    out: list[tuple[int, int]] = []
    for index, when in enumerate(spawns):
        if not index or when - spawns[index - 1] >= WAVE_SPLIT_MS:
            out.append((len(out) + 1, when))
    return out


def wave_of(waves: list[tuple[int, int]], spawn: int) -> int | None:
    return next((wave for wave, start in reversed(waves) if start <= spawn + NEAR_MS), None)


def owner_at(changes: list[tuple[int, int]], first: int, when: int) -> int:
    """The side an `anub.bside` history had at `when`, `first` before its first row."""
    return next((owner for start, owner in reversed(changes) if start <= when), first)


def pickup_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per burrower: its wave, exit and side at the spawn, every owner it had in turn, the time from its
    first sample to the first with any tank on it and with its owning side's tank on it then, and how
    far that side tank stood at the spawn from its side's patch, or from the side's spot before one
    latched. The side is the first `anub.bside`: a dead side tank's burrowers all move, so the last says
    little. `opener` is `(ms after the spawn, target)` for its first target that isn't one of the encounter's
    own creatures, since it spawns aimed at a sphere for a tick, and `nearest` the OPENER_NEAREST living
    roster players nearest its exit at the spawn, `(guid, yards)`."""
    samples = samples or Samples(trace)
    end = pull_end(trace)
    sides = [latch_spans(trace, f"anub.side{side}", end) for side in (0, 1)]
    latches = patch_latches(trace)
    noted = spot_spans(trace)
    owned = per_bot(trace, "anub.bside")
    waves = wave_starts(trace, samples)
    roster = roster_guids(trace)
    out = []
    for guid in sorted(guids_of_entry(trace, NPC_BURROWER)):
        stamps, rows = samples.track(guid)
        if not stamps:
            continue
        spawn = stamps[0]
        x, y, exit_side = min(BURROW_EXITS, key=lambda spot: dist2(spot, rows[0][1:3]))
        changes = [(when, int(value)) for when, value in owned.get(guid, []) if value in ("0", "1")]
        side = changes[0][1] if changes else exit_side
        owners = [owner for index, (_, owner) in enumerate(changes) if not index or owner != changes[index - 1][1]]
        targets = [(when, row[7]) for when, row in zip(stamps, rows) if len(row) > 7 and row[7]]
        to_tank = next((when - spawn for when, target in targets if trace.role(target) == "tank"), None)
        to_side = next((when - spawn for when, target in targets
                        if as_guid(held_at(sides[owner_at(changes, side, when)], when)) == target), None)
        tank = as_guid(held_at(sides[side], spawn))
        stand = samples.row(tank, spawn) if tank else None
        gap = kind = None
        if stand is not None:
            point = latched_patch(latches[side], samples, spawn)
            kind = "patch" if point else "spot"
            gap = dist2(stand[1:3], point or side_spot(latches, samples, side, spawn, noted))
        opener = next(((when - spawn, target) for when, target in targets
                       if trace.entries.get(target) not in ENCOUNTER_NPCS), None)
        players = [(member, samples.recent(member, spawn)) for member in roster]
        nearest = sorted(((member, dist2(row[1:3], (x, y))) for member, row in players if row and row[5] > 0),
                         key=lambda item: item[1])[:OPENER_NEAREST]
        out.append({"guid": guid, "spawn": spawn, "exit": (x, y), "exit_side": exit_side,
                    "wave": wave_of(waves, spawn),
                    "side": side, "probed": bool(changes), "owners": owners or [side], "to_tank": to_tank,
                    "to_side": to_side, "tank": tank or None, "gap": gap, "from": kind,
                    "opener": opener, "nearest": nearest})
    return out


def focus_due_rows(trace: Trace, waves: list[tuple[int, int]]) -> list[dict]:
    """Per wave, when `anub.focusdue` first read 1 from its spawn on, kept only when that came before the
    next wave's spawn."""
    due = [(start, stop) for value, start, stop in latch_spans(trace, "anub.focusdue", pull_end(trace))
           if value == "1"]
    out = []
    for index, (wave, spawn) in enumerate(waves):
        upto = waves[index + 1][1] if index + 1 < len(waves) else None
        when = next((max(start, spawn) for start, stop in due if stop > spawn), None)
        if when is not None and upto is not None and when >= upto:
            when = None
        out.append({"wave": wave, "spawn": spawn, "due": when, "next": upto})
    return out


def stray_burrower_casts(trace: Trace) -> list[dict]:
    """DPS bot casts aimed at a burrower other than the `anub.focus` held then, triggered ones and humans
    left out, and a cast landing while its target cast Shadow Strike, since interrupts may go anywhere."""
    burrowers = guids_of_entry(trace, NPC_BURROWER)
    dps = {guid for guid in roster_guids(trace) if trace.role(guid) in DPS_ROLES and guid not in trace.humans}
    focus = latch_spans(trace, FOCUS, pull_end(trace))
    strikes = [(rec.get("s"), rec["t"], rec["t"] + (rec.get("ct") or SHADOW_STRIKE_CAST_MS))
               for rec in trace.of("cast") if rec.get("sp") == SPELL_SHADOW_STRIKE]
    out = []
    for rec in trace.of("cast"):
        target = rec.get("tgt")
        if rec.get("tr") or rec.get("s") not in dps or target not in burrowers:
            continue
        if as_guid(held_at(focus, rec["t"])) == target:
            continue
        if any(source == target and low <= rec["t"] <= high for source, low, high in strikes):
            continue
        out.append(rec)
    return out


def focus_due_drops(trace: Trace, samples: Samples) -> list[dict]:
    """Each time `anub.focusdue` went from 1 to 0 with the `anub.focus` burrower still alive, and its
    health then."""
    end = pull_end(trace)
    focus = latch_spans(trace, FOCUS, end)
    out = []
    for value, _, stop in latch_spans(trace, "anub.focusdue", end):
        if value != "1" or stop >= end:
            continue
        guid = as_guid(held_at(focus, stop - 1))
        row = samples.recent(guid, stop) if guid else None
        if row and row[5] > 0:
            out.append({"t": stop, "guid": guid, "hp": row[5]})
    return out


def side_spans(trace: Trace) -> list[list[tuple[str, int, int]]]:
    """`anub.side0` and `anub.side1` spans, indexed by side."""
    end = pull_end(trace)
    return [latch_spans(trace, f"anub.side{side}", end) for side in (0, 1)]


def focus_picks(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per `anub.focus` pick of a burrower: its health, the milliseconds since it came up (first sight or its
    last emerge), its target, and how far it stood from the nearest living side tank and the nearest
    Permafrost centre. `settled` is a tank as its target within FOCUS_SETTLE of it."""
    samples = samples or Samples(trace)
    sides = side_spans(trace)
    holders = {as_guid(value) for spans in sides for value, _, _ in spans} - {0}
    ups = {row["guid"]: row["ups"] for row in burrower_rows(trace, samples)}
    out = []
    for value, start, _ in latch_spans(trace, FOCUS, pull_end(trace)):
        guid = as_guid(value)
        row = samples.row(guid, start) if guid else None
        if row is None:
            continue
        came = [when for when in ups.get(guid, []) if when <= start] or ups.get(guid, [start])[:1]
        target = row[7] if len(row) > 7 else 0
        victim = samples.recent(target, start) if target else None
        tanks = [samples.recent(tank, start) for tank in {as_guid(held_at(spans, start)) for spans in sides} - {0}]
        to_tank = [dist2(row[1:3], tank[1:3]) for tank in tanks if tank and tank[5] > 0]
        to_frost = [dist2(row[1:3], point) for point in samples.frost_at(start)]
        out.append({"t": start, "guid": guid, "hp": row[5], "up": max(0, start - came[-1]), "target": target,
                    "side_tank": min(to_tank) if to_tank else None, "patch": min(to_frost) if to_frost else None,
                    "settled": victim is not None and (trace.role(target) == "tank" or target in holders)
                    and dist2(row[1:3], victim[1:3]) <= FOCUS_SETTLE})
    return out


def held_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per burrower ever on a side tank, its target the tank holding a side then and within FOCUS_SETTLE of it:
    those samples, and its median distance from the patch that side had latched, over the ones with a patch.
    A side with none would read the other side's patch as the nearest."""
    samples = samples or Samples(trace)
    sides = side_spans(trace)
    latches = patch_latches(trace)
    out = []
    for guid in sorted(guids_of_entry(trace, NPC_BURROWER)):
        stamps, rows = samples.track(guid)
        held = 0
        gaps = []
        for when, row in zip(stamps, rows):
            target = row[7] if len(row) > 7 else 0
            side = next((side for side, spans in enumerate(sides)
                         if target and as_guid(held_at(spans, when)) == target), None)
            if side is None:
                continue
            tank = samples.recent(target, when)
            if tank is None or dist2(row[1:3], tank[1:3]) > FOCUS_SETTLE:
                continue
            held += 1
            patch = latched_patch(latches[side], samples, when)
            if patch:
                gaps.append(dist2(row[1:3], patch))
        if held:
            out.append({"guid": guid, "held": held, "gaps": gaps, "median": statistics.median(gaps) if gaps else None})
    return out


def burrower_submerges(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Each burrower submerge, read where it went under: the first sample of the still run, same spot and
    health, before it came back full. There its health, the nearest Permafrost centre and how many were
    alive, its `anub.bside` side, else its exit's, that side's tank, alive or not and how far, and its
    target, else the last it had within VICTIM_LOOKBACK_MS."""
    samples = samples or Samples(trace)
    sides = side_spans(trace)
    owned = per_bot(trace, "anub.bside")
    out = []
    for guid in sorted(guids_of_entry(trace, NPC_BURROWER)):
        stamps, rows = samples.track(guid)
        if not stamps:
            continue
        exit_side = min(BURROW_EXITS, key=lambda spot: dist2(spot, rows[0][1:3]))[2]
        changes = [(when, int(value)) for when, value in owned.get(guid, []) if value in ("0", "1")]
        for index in range(1, len(rows)):
            if not returned((stamps[index - 1], rows[index - 1][5]), (stamps[index], rows[index][5])):
                continue
            under = index - 1
            while (under and dist2(rows[under - 1][1:3], rows[under][1:3]) < FROZEN_YD
                   and rows[under - 1][5] == rows[under][5]):
                under -= 1
            when, row = stamps[under], rows[under]
            frost = [dist2(row[1:3], centre) for centre in samples.frost_at(when)]
            side = owner_at(changes, changes[0][1] if changes else exit_side, when)
            tank = as_guid(held_at(sides[side], when))
            stand = samples.recent(tank, when) if tank else None
            victim = next((rows[back][7] for back in range(under, -1, -1)
                           if when - stamps[back] <= VICTIM_LOOKBACK_MS and len(rows[back]) > 7 and rows[back][7]), 0)
            out.append({"guid": guid, "t": when, "back": stamps[index], "spot": (row[1], row[2]), "hp": row[5],
                        "frost": min(frost) if frost else None, "patches": len(frost), "side": side,
                        "tank": tank or None, "alive": bool(stand) and stand[5] > 0,
                        "tank_yd": dist2(row[1:3], stand[1:3]) if stand else None, "victim": victim})
    return sorted(out, key=lambda row: (row["t"], row["guid"]))


def submerge_reasons(row: dict) -> list[str]:
    """What let a burrower go under: no side tank, a dead one, no patch alive, or a target off its side tank."""
    out = []
    if row["tank"] is None:
        out.append("no side tank")
    elif not row["alive"]:
        out.append("side tank dead")
    if not row["patches"]:
        out.append("no patch")
    if row["tank"] is not None and row["victim"] != row["tank"]:
        out.append("off its side tank")
    return out


def side_tank_gather(trace: Trace) -> list[tuple[str, int, dict[int, int]]]:
    """Per submerge, the milliseconds each bot held `anub.sidetank` gather while he was down."""
    spans = holder_spans(trace, "anub.sidetank", pull_end(trace))
    out = []
    for start, stop, phase, label in phase_spans(trace):
        if phase == 2:
            held = {guid: covered([(low, high) for value, low, high in rows if value == "gather"], start, stop)
                    for guid, rows in spans.items()}
            out.append((label, start, {guid: ms for guid, ms in held.items() if ms}))
    return out


def show_focus_picks(trace: Trace, samples: Samples) -> None:
    picks = focus_picks(trace, samples)
    if picks:
        print("\n  each anub.focus pick: the burrower's health, seconds since it came up, target, and its distance to"
              "\n  the nearest living side tank and Permafrost centre:")
        print(f"  {'at':>9} {'focus':10} {'hp':>4} {'up':>6} {'target':14} {'side tank':>9} {'patch':>7}")
        for row in picks:
            tank = "-" if row["side_tank"] is None else f"{row['side_tank']:.1f} yd"
            patch = "-" if row["patch"] is None else f"{row['patch']:.1f} yd"
            early = not row["settled"] and row["hp"] >= FOCUS_LOW_PCT
            print(f"  {clock(row['t']):>9} {short(trace, row['guid']):10} {row['hp']:3.0f}% {row['up'] / 1000:4.1f} s"
                  f" {unit_label(trace, row['target'])[:14]:14} {tank:>9} {patch:>7}"
                  + ("  before its tank" if early else ""))
        early = sum(1 for row in picks if not row["settled"] and row["hp"] >= FOCUS_LOW_PCT)
        print(f"  {early} of {len(picks)} pick(s) before its target was a tank within {FOCUS_SETTLE:.0f} yd, at"
              f" {FOCUS_LOW_PCT:.0f}% or more health; up runs from its first sample or its last emerge")

    held = held_rows(trace, samples)
    if not held:
        print(f"  no burrower sampled on a side tank within {FOCUS_SETTLE:.0f} yd")
        return
    print(f"\n  on a side tank, its target the tank holding a side and within {FOCUS_SETTLE:.0f} yd: median distance"
          " from\n  that side's latched tank patch over those samples with one")
    print(f"  {'burrower':10} {'samples':>7} {'patched':>7} {'median':>7}")
    for row in held:
        median = "-" if row["median"] is None else f"{row['median']:.1f} yd"
        print(f"  {short(trace, row['guid']):10} {row['held']:7} {len(row['gaps']):7} {median:>7}")
    gaps = [gap for row in held for gap in row["gaps"]]
    if gaps:
        print(f"  all: median {statistics.median(gaps):.1f} yd over {len(gaps)} sample(s), a burrower's own reach on"
              f" the slow being {BURROWER_PERMAFROST_REACH:.1f} yd")


def show_burrower_windows(trace: Trace, samples: Samples) -> None:
    rows = burrower_windows(trace, samples)
    print(f"\n  dead inside {BURROWER_SAFE_MS / 1000:.0f} s of coming up, before the core's first submerge check:")
    print(f"  {'burrower':10} {'wave':>4} {'came up':>9} {'from':6} {'died':>9} {'after':>7} inside")
    tally: dict[int | None, collections.Counter] = collections.defaultdict(collections.Counter)
    for row in rows:
        died = "alive" if row["died"] is None else clock(row["died"])
        after = "-" if row["died"] is None else f"{(row['died'] - row['up']) / 1000:.1f} s"
        verdict = {None: "alive", True: "yes", False: "no"}[row["inside"]]
        tally[row["wave"]][verdict] += 1
        print(f"  {short(trace, row['guid']):10} {row['wave'] or '-':>4} {clock(row['up']):>9}"
              f" {'emerge' if row['emerged'] else 'sight':6} {died:>9} {after:>7}"
              f" {'-' if row['inside'] is None else verdict}")
    for wave, counts in sorted(tally.items(), key=lambda item: (item[0] is None, item[0] or 0)):
        print(f"  wave {wave or '-'}: {counts['yes']} of {sum(counts.values())} dead inside, {counts['no']} later,"
              f" {counts['alive']} alive")


def show_burrower_submerges(trace: Trace, samples: Samples) -> None:
    rows = burrower_submerges(trace, samples)
    if not rows:
        print("  no burrower submerged")
        return
    print("\n  each submerge, at the first sample of the still run before it came back full:")
    print(f"  {'burrower':10} {'under':>9} {'at':13} {'hp':>4} {'patch':>8} {'patches':>7} {'side':>4}"
          f" {'side tank':22} target")
    for row in rows:
        patch = "-" if row["frost"] is None else f"{row['frost']:.1f} yd"
        tank = "none"
        if row["tank"] is not None:
            state = "dead" if not row["alive"] else "-" if row["tank_yd"] is None else f"{row['tank_yd']:.1f} yd"
            tank = f"{trace.name(row['tank'])[:14]} {state}"
        why = ", ".join(submerge_reasons(row))
        print(f"  {short(trace, row['guid']):10} {clock(row['t']):>9} {row['spot'][0]:6.1f},{row['spot'][1]:6.1f}"
              f" {row['hp']:3.0f}% {patch:>8} {row['patches']:7} {row['side']:4} {tank:22}"
              f" {unit_label(trace, row['victim'])}" + (f"  {why}" if why else ""))
    up = sum(1 for row in rows if row["alive"] and row["patches"])
    print(f"  {up} of {len(rows)} went under with its side tank alive and a patch alive; patch is the nearest"
          " Permafrost\n  centre, patches the centres alive then, side its anub.bside owner, else its exit's")


def show_openers(trace: Trace, rows: list[dict]) -> None:
    print(f"\n  each burrower's opener, its first target off the encounter's own creatures, and the {OPENER_NEAREST}"
          " living\n  players nearest its exit at the spawn:")
    for row in rows:
        if row["opener"] is None:
            opener = "never on the raid"
        else:
            after, target = row["opener"]
            role = f" ({trace.role(target)})" if target in trace.roles else ""
            opener = f"{unit_label(trace, target)[:30]}{role} +{after / 1000:.1f} s"
        near = ", ".join(f"{trace.name(guid)} {trace.role(guid)} {yards:.0f}" for guid, yards in row["nearest"])
        print(f"  {short(trace, row['guid']):10} {row['wave'] or '-':>4} {opener:40} nearest {near or '-'}")
    untanked = [row for row in rows if row["opener"] and trace.role(row["opener"][1]) != "tank"]
    times = [row["to_tank"] for row in untanked if row["to_tank"] is not None]
    print(f"  opened on a non-tank: {len(untanked)} of {len(rows)}"
          + (f"; to a tank after it: median {statistics.median(times) / 1000:.1f} s, longest"
             f" {max(times) / 1000:.1f} s" if times else "")
          + (f", {len(untanked) - len(times)} never" if len(untanked) > len(times) else ""))


def show_pickups(trace: Trace, samples: Samples) -> None:
    def secs(ms):
        return "never" if ms is None else f"{ms / 1000:.1f} s"

    rows = pickup_rows(trace, samples)
    if not rows:
        return
    print(f"\n  {'burrower':10} {'wave':>4} {'exit':13} {'side':7} {'to tank':>7} {'to side':>7}"
          f" {'side tank':14} at the spawn")
    for row in rows:
        exit_name = f"{'N' if row['exit_side'] == 0 else 'S'} {row['exit'][0]:.1f},{row['exit'][1]:.1f}"
        tank = "-" if row["tank"] is None else trace.name(row["tank"])
        gap = "-" if row["gap"] is None else f"{row['gap']:.1f} yd off its {row['from']}"
        owners = ">".join(str(owner) for owner in row["owners"]) + ("" if row["probed"] else "*")
        print(f"  {short(trace, row['guid']):10} {row['wave'] or '-':>4} {exit_name:13}"
              f" {owners[:7]:7} {secs(row['to_tank']):>7}"
              f" {secs(row['to_side']):>7} {tank[:14]:14} {gap}")
    print("  side is its owner at the spawn, then each one after; to tank and to side run from its first"
          " sample\n  to the first with any tank, then its owning side's tank, on it; a * side is read off"
          " its exit,\n  with no anub.bside row; side tank is the spawn side's")
    show_openers(trace, rows)

    waves = wave_starts(trace, samples)
    if not notes(trace, "anub.wave"):
        print(f"  no anub.wave rows, so waves are burrower spawns {WAVE_SPLIT_MS / 1000:.0f} s or more apart")
    for row in focus_due_rows(trace, waves):
        if row["next"] is None:
            verdict = "focus never due" if row["due"] is None else f"focus due {clock(row['due'])}"
            verdict += ", the last wave"
        elif row["due"] is None:
            verdict = f"focus never due before wave {row['wave'] + 1} at {clock(row['next'])}"
        else:
            verdict = (f"focus due {clock(row['due'])}, {(row['next'] - row['due']) / 1000:.1f} s before"
                       f" wave {row['wave'] + 1} at {clock(row['next'])}")
        print(f"  wave {row['wave']} {clock(row['spawn'])}: {verdict}")
    show_counts(trace, "anub.sidetank")
    if any(str(rec.get("txt")) == "gather" for rec in notes(trace, "anub.sidetank")):
        print("  anub.sidetank gather while he was down: " + "; ".join(
            f"{label} " + (", ".join(f"{trace.name(guid)} {ms / 1000:.1f} s" for guid, ms in sorted(held.items()))
                           or "none")
            for label, _, held in side_tank_gather(trace)))


def show_burrowers(trace: Trace) -> None:
    print("NERUBIAN BURROWERS")
    samples = Samples(trace)
    rows = burrower_rows(trace, samples)
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
        show_burrower_windows(trace, samples)
        show_pickups(trace, samples)
        show_burrower_submerges(trace, samples)

    held = sorted({trace.name(guid) for guid, counts in value_counts(trace, "anub.interrupter").items()
                   if counts.get("1")})
    print(f"  ever held Shadow Strike duty: {', '.join(held) if held else 'nobody'}")

    focus = focus_rows(trace)
    if not focus:
        print("  anub.focus never held a burrower")
    else:
        print(f"\n  {'focus':10} {'held':>7} {'dps on it':>9}")
        for row in focus:
            share = "-" if row["share"] is None else f"{row['share'] * 100:.0f}%"
            print(f"  {short(trace, row['guid']):10} {row['held'] / 1000:6.1f}s {share:>9}")
        print("  dps on it is the share of living DPS bot samples whose target was the focus while it held")
    show_focus_picks(trace, samples)

    stray = stray_burrower_casts(trace)
    if stray:
        casters = collections.Counter(rec["s"] for rec in stray)
        spells = collections.Counter(rec["sp"] for rec in stray)
        print(f"  DPS casts at a burrower other than the focus: {len(stray)}, by "
              + ", ".join(f"{trace.name(guid)} {n}" for guid, n in casters.most_common())
              + "; most: " + ", ".join(f"{trace.spell(spell)} {n}" for spell, n in spells.most_common(5)))
    else:
        print("  no DPS cast at a burrower other than the focus")
    drops = focus_due_drops(trace, samples)
    print("  anub.focusdue dropped with the focus alive: "
          + (", ".join(f"{clock(row['t'])} {short(trace, row['guid'])} at {row['hp']:.0f}%" for row in drops)
             if drops else "never"))
    show_counts(trace, "anub.dps")
    if notes(trace, "anub.mstand"):
        show_counts(trace, "anub.mstand")


# ---------------------------------------------------------------------------------------------- frost

def frost_groups(trace: Trace, samples: Samples) -> dict[int, str]:
    """Each roster member's group: `side tank` for any holder of `anub.side0` or `anub.side1`, `main tank` for
    the other tank he targeted most, else its role."""
    _, holders = side_tanks(trace)
    sides = {guid for guids in holders.values() for guid in guids}
    _, rows = samples.track(boss_guid(trace))
    hit = collections.Counter(row[7] for row in rows
                              if len(row) > 7 and row[7] not in sides and trace.role(row[7]) == "tank")
    main = hit.most_common(1)[0][0] if hit else None
    return {guid: "side tank" if guid in sides else "main tank" if guid == main else trace.role(guid)
            for guid in roster_guids(trace)}


def frost_stints(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Each Permafrost stint on the roster: the bot and its group, the milliseconds of it while he was down,
    the phase it began in and the bot's target then. One still on when the trace ends runs to the end."""
    samples = samples or Samples(trace)
    groups = frost_groups(trace, samples)
    spans = phase_spans(trace)
    down = [(start, stop) for start, stop, phase, _ in spans if phase == 2]
    out = []
    for guid, stints in aura_spans(trace, PERMAFROST).items():
        if guid not in groups:
            continue
        for start, stop, closed in stints:
            row = samples.row(guid, start)
            out.append({"guid": guid, "group": groups[guid], "start": start, "stop": stop, "closed": closed,
                        "down": covered(down, start, stop), "phase": phase_at(spans, start),
                        "target": row[7] if row and len(row) > 7 else 0})
    return sorted(out, key=lambda row: (row["start"], row["guid"]))


def frost_totals(stints: list[dict]) -> dict[int, dict]:
    """Per bot: its group, milliseconds carrying Permafrost up and down, stint count and longest stint."""
    out: dict[int, dict] = {}
    for row in stints:
        bot = out.setdefault(row["guid"], {"group": row["group"], "up": 0, "down": 0, "stints": 0, "longest": 0})
        span = row["stop"] - row["start"]
        bot["up"] += span - row["down"]
        bot["down"] += row["down"]
        bot["stints"] += 1
        bot["longest"] = max(bot["longest"], span)
    return out


def show_frost(trace: Trace) -> None:
    print("PERMAFROST")
    stints = frost_stints(trace)
    if not stints:
        print("  nobody on the roster carried Permafrost")
    else:
        totals = frost_totals(stints)

        def rank(group: str) -> int:
            return FROST_GROUPS.index(group) if group in FROST_GROUPS else len(FROST_GROUPS)

        groups: dict[str, list[dict]] = collections.defaultdict(list)
        for guid, bot in totals.items():
            if guid not in trace.humans:
                groups[bot["group"]].append(bot)
        print(f"  seconds carrying it, bots only:\n  {'group':10} {'bots':>4} {'up':>7} {'down':>7} {'total':>7}")
        for group in sorted(groups, key=rank):
            up = sum(bot["up"] for bot in groups[group]) / 1000
            down = sum(bot["down"] for bot in groups[group]) / 1000
            print(f"  {group:10} {len(groups[group]):4} {up:7.1f} {down:7.1f} {up + down:7.1f}")

        print(f"\n  {'bot':14} {'group':10} {'up':>7} {'down':>7} {'total':>7} {'stints':>6} {'longest':>7}")
        for guid, bot in sorted(totals.items(), key=lambda item: (rank(item[1]["group"]),
                                                                    -(item[1]["up"] + item[1]["down"]))):
            print(f"  {trace.name(guid)[:14]:14} {bot['group']:10} {bot['up'] / 1000:7.1f} {bot['down'] / 1000:7.1f}"
                  f" {(bot['up'] + bot['down']) / 1000:7.1f} {bot['stints']:6} {bot['longest'] / 1000:7.1f}"
                  + ("  human" if guid in trace.humans else ""))

        long = [row for row in stints if row["stop"] - row["start"] > FROST_STINT_MS]
        if long:
            print(f"\n  stints over {FROST_STINT_MS / 1000:.0f} s:\n  {'at':>9} {'bot':14} {'group':10} {'secs':>7}"
                  f" {'phase':5} target at its start")
        else:
            print(f"\n  no stint over {FROST_STINT_MS / 1000:.0f} s")
        for row in long:
            print(f"  {clock(row['start']):>9} {trace.name(row['guid'])[:14]:14} {row['group']:10}"
                  f" {(row['stop'] - row['start']) / 1000:5.1f} s {row['phase']:5} {unit_label(trace, row['target'])}"
                  + ("  human" if row["guid"] in trace.humans else "")
                  + ("" if row["closed"] else "  on at the end"))
        print("  up is every moment outside a phase 2 anub.phase span; side tanks are every anub.side0 and"
              " anub.side1\n  holder, the main tank the other tank he targeted most")
    if notes(trace, "anub.leave"):
        show_counts(trace, "anub.leave")


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


def drag_anchor(latches, samples: Samples, when: int) -> tuple[float, float] | None:
    """Where the main tank drags him: between both latched tank patches. None with one or none, since
    then the tank holds him where it picked him up."""
    spots = [spot for spot in (latched_patch(spans, samples, when) for spans in latches) if spot]
    if len(spots) == 2:
        return (spots[0][0] + spots[1][0]) / 2.0, (spots[0][1] + spots[1][1]) / 2.0
    return None


def turn(before, after) -> float:
    """Degrees between two ground headings, 0 when either is no move at all."""
    first, second = math.hypot(*before), math.hypot(*after)
    if not first or not second:
        return 0.0
    cos = (before[0] * after[0] + before[1] * after[1]) / (first * second)
    return math.degrees(math.acos(max(-1.0, min(1.0, cos))))


def boss_legs(samples: Samples, boss: int, start: int, stop: int) -> list[dict]:
    """His walks of LEG_MIN_YD or more, `{start, stop, frm, to, yards}`. A walk is a run of samples with
    his moving flag set, from its first sample to the first still one after it, or to its last when the
    window or a snapshot hole cuts it. Yards are the straight line, and a turn inside a run stays one
    leg: the flag is his `isMoving()`, which the drag waits on."""
    stamps, rows = samples.track(boss)
    legs: list[dict] = []
    leg = None
    last = None
    for when, row in zip(stamps, rows):
        if not start <= when < stop:
            continue
        if leg is not None and when - last > MAX_STEP_MS:
            leg = None
        last = when
        moving = len(row) > 8 and bool(row[8])
        if leg is None:
            if not moving:
                continue
            leg = {"start": when, "frm": row[1:3]}
            legs.append(leg)
        leg["stop"], leg["to"] = when, row[1:3]
        if not moving:
            leg = None
    for leg in legs:
        leg["yards"] = dist2(leg["frm"], leg["to"])
    return [leg for leg in legs if leg["yards"] >= LEG_MIN_YD]


def leg_reversals(legs: list[dict]) -> int:
    """Legs heading more than REVERSAL_DEGREES off the one before."""
    headings = [(leg["to"][0] - leg["frm"][0], leg["to"][1] - leg["frm"][1]) for leg in legs]
    return sum(1 for before, after in zip(headings, headings[1:]) if turn(before, after) > REVERSAL_DEGREES)


def boss_taunts(trace: Trace, samples: Samples, boss: int) -> list[dict]:
    """Taunts cast on him, and Righteous Defense on the friend he was hitting at the time: it taunts that
    friend's attackers, so cast on anyone else it never reached him."""
    out = []
    for rec in trace.of("cast"):
        spell = rec.get("sp")
        if rec.get("tr") or spell not in TAUNT_SPELLS:
            continue
        if spell == SPELL_RIGHTEOUS_DEFENSE:
            row = samples.row(boss, rec["t"])
            if not row or len(row) <= 7 or row[7] != rec.get("tgt"):
                continue
        elif rec.get("tgt") != boss:
            continue
        out.append(rec)
    return out


def patch_share(samples: Samples, guid: int, start: int, stop: int) -> tuple[int, int]:
    """`(inside, living)`: the unit's living samples in the window, and those within
    PERMAFROST_SLOW_REACH of a Permafrost centre."""
    stamps, rows = samples.track(guid)
    inside = living = 0
    for index in range(bisect.bisect_left(stamps, start), bisect.bisect_left(stamps, stop)):
        row = rows[index]
        if row[5] <= 0:
            continue
        living += 1
        inside += any(dist2(row[1:3], centre) <= PERMAFROST_SLOW_REACH for centre in samples.frost_at(stamps[index]))
    return inside, living


def submerge_gaps(latches, samples: Samples, boss: int, edge: int) -> dict | None:
    """His last surfaced spot before the phase 2 edge, and its distance from each latched tank patch."""
    row = samples.row(boss, edge - 1)
    if row is None:
        return None
    gaps = {}
    for side, spans in enumerate(latches):
        point = latched_patch(spans, samples, edge - 1)
        if point is not None:
            gaps[side] = dist2(row[1:3], point)
    return {"spot": (row[1], row[2]), "patches": gaps}


def threat_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    samples = samples or Samples(trace)
    boss = boss_guid(trace)
    if boss is None:
        return []
    taunts = boss_taunts(trace, samples, boss)
    latches = patch_latches(trace)
    pickups = notes(trace, "anub.pickup")
    capped = [rec for rec in trace.of("veto") if rec.get("m") == THREAT_CAP]
    stamps, rows = samples.track(boss)
    spans = phase_spans(trace)
    submerges = {start for start, _, phase, _ in spans if phase == 2}

    out = []
    for window in surfaced_windows(spans):
        start, stop = window["start"], window["stop"]
        held, _ = threat_share(trace, {boss}, lambda when, low=start, high=stop: low <= when < high)
        inside = [(when, row) for when, row in zip(stamps, rows) if start <= when < stop]
        tank = next((when for when, row in inside if len(row) > 7 and trace.role(row[7]) == "tank"), None)
        tanks = collections.Counter(row[7] for _, row in inside if len(row) > 7 and trace.role(row[7]) == "tank")
        main = tanks.most_common(1)[0][0] if tanks else None
        in_patch, living = patch_share(samples, main, start, stop) if main else (0, 0)
        # from the first submerge leg on he's led to the submerge spot and stays there, and the tank's
        # rows read hold again once he arrives
        cutoff = next((rec["t"] for rec in pickups
                       if start <= rec["t"] < stop and str(rec.get("txt", "")) == "submerge"), stop)
        spread = []
        for when, row in inside:
            point = drag_anchor(latches, samples, when) if when < cutoff else None
            if point is not None:
                spread.append(dist2(row[1:3], point))
        legs = boss_legs(samples, boss, start, stop)
        out.append({**window,
                    "held": held,
                    "to_tank": None if tank is None else tank - start,
                    "taunts": [rec for rec in taunts if start <= rec["t"] < stop],
                    "off_anchor": statistics.median(spread) if spread else None,
                    "legs": len(legs),
                    "yards": sum(leg["yards"] for leg in legs),
                    "reversals": leg_reversals(legs),
                    "pickup": collections.Counter((rec.get("g", 0), str(rec.get("txt", "")))
                                                  for rec in pickups if start <= rec["t"] < stop),
                    "capped": collections.Counter(rec.get("g", 0) for rec in capped if start <= rec["t"] < stop),
                    "main": main, "in_patch": in_patch, "living": living,
                    "submerge": submerge_gaps(latches, samples, boss, stop) if stop in submerges else None})
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
        print(f"          {row['legs']} leg(s), {row['yards']:.0f} yd, {row['reversals']} reversal(s)")
        for rec in row["taunts"]:
            print(f"          taunt {clock(rec['t'])} {trace.name(rec.get('s'))[:14]:14}"
                  f" {'human' if rec.get('s') in trace.humans else 'bot':5} {TAUNT_SPELLS[rec['sp']]}")
        if row["taunts"]:
            humans = sum(1 for rec in row["taunts"] if rec.get("s") in trace.humans)
            print(f"          {len(row['taunts'])} taunt(s): bot {len(row['taunts']) - humans}, human {humans}")
        by_bot: dict[int, list[str]] = collections.defaultdict(list)
        for (guid, value), n in row["pickup"].most_common():
            by_bot[guid].append(f"{value} {n}")
        if by_bot:
            print("          anub.pickup: " + "; ".join(
                f"{trace.name(guid)} " + ", ".join(values) for guid, values in by_bot.items()))
        if row["capped"]:
            print(f"          {THREAT_CAP} vetoes: " + ", ".join(
                f"{trace.name(guid)} {n}" for guid, n in row["capped"].most_common()))
        if row["main"] is not None:
            share = f"{row['in_patch'] * 100 / row['living']:.0f}%" if row["living"] else "-"
            print(f"          main tank {trace.name(row['main'])} in a patch {share} of {row['living']} samples")
        if row["submerge"] is not None:
            spot = row["submerge"]["spot"]
            gaps = ", ".join(f"patch{side} {yards:.1f} yd" + (" under" if yards < SUBMERGE_PATCH_CLEARANCE else "")
                             for side, yards in sorted(row["submerge"]["patches"].items()))
            print(f"          submerged {clock(row['stop'])} at {spot[0]:.1f}, {spot[1]:.1f}:"
                  f" {gaps or 'no tank patch latched'}")

    emerges = [row["to_tank"] for row in rows if row["kind"] == "emerge"]
    if emerges:
        known = [ms for ms in emerges if ms is not None]
        print(f"\n  {len(emerges)} emerge(s), {len(emerges) - len(known)} never reached a tank"
              + (f", the rest in {min(known) / 1000:.1f}-{max(known) / 1000:.1f} s" if known else ""))
    living = sum(row["living"] for row in rows)
    if living:
        print(f"  main tank, the tank he targeted most in each window, in a patch (within"
              f" {PERMAFROST_SLOW_REACH:.1f} yd of a Permafrost\n  centre): {sum(row['in_patch'] for row in rows)}"
              f" of {living} surfaced samples; a submerge under {SUBMERGE_PATCH_CLEARANCE} yd from a tank"
              " patch reads under")
    print("  off anchor is his median distance from the midpoint of both latched tank patches, over the"
          " samples\n  that have both, before the window's first submerge leg; - when none does")
    print(f"  a leg is a run of samples flagged moving, {LEG_MIN_YD:.0f} yd or more start to stop; a reversal is"
          f" a leg\n  heading more than {REVERSAL_DEGREES:.0f} degrees off the one before;"
          f" {THREAT_CAP} vetoes are rows, one per verdict change")


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


# -------------------------------------------------------------------------------------------- healers

def healer_guids(trace: Trace) -> list[int]:
    return sorted((guid for guid in roster_guids(trace) if trace.role(guid) == "heal"), key=trace.name)


def main_tank(trace: Trace, samples: Samples) -> int | None:
    return next((guid for guid, group in frost_groups(trace, samples).items() if group == "main tank"), None)


def healer_rows(trace: Trace, samples: Samples | None = None) -> dict:
    """From the pull on, per living healer and phase its distances to the main tank and to whoever held each
    side (`gaps[(healer, label)]["main"|0|1]`), the milliseconds over HEALER_FAR_YD from a side tank a living
    burrower targeted (`far[(healer, side, label)]`), and per side the milliseconds its holder lived with no
    living healer within HEALER_REACH_YD (`bare[(side, tank, label)]`). A snapshot stands for the time since
    the one before, capped at MAX_STEP_MS."""
    samples = samples or Samples(trace)
    healers = healer_guids(trace)
    main = main_tank(trace, samples) if healers else None
    sides = side_spans(trace)
    spans = phase_spans(trace)
    burrowers = guids_of_entry(trace, NPC_BURROWER)
    gaps: dict = collections.defaultdict(lambda: collections.defaultdict(list))
    far: collections.Counter = collections.Counter()
    bare: collections.Counter = collections.Counter()
    labels: list[str] = []
    last = None
    for snap in frames(trace) if healers else ():
        when = snap["t"]
        step = 0 if last is None else min(when - last, MAX_STEP_MS)
        last = when
        if when < 0:
            continue
        units = {row[0]: row for row in snap.get("u", [])}
        label = phase_at(spans, when)
        if label not in labels:
            labels.append(label)
        tanks = {side: as_guid(held_at(sides[side], when)) for side in (0, 1)}
        held = {side: bool(tank) and any(row[0] in burrowers and row[5] > 0 and len(row) > 7 and row[7] == tank
                                         for row in units.values())
                for side, tank in tanks.items()}
        living = [units[guid] for guid in healers if guid in units and units[guid][5] > 0]
        for row in living:
            for key, tank in (("main", main), (0, tanks[0]), (1, tanks[1])):
                unit = units.get(tank) if tank else None
                if unit is None or unit[5] <= 0:
                    continue
                gap = dist2(row[1:3], unit[1:3])
                gaps[(row[0], label)][key].append(gap)
                if key != "main" and held[key] and gap > HEALER_FAR_YD:
                    far[(row[0], key, label)] += step
        for side, tank in tanks.items():
            unit = units.get(tank) if tank else None
            if unit is not None and unit[5] > 0 and not any(dist2(row[1:3], unit[1:3]) <= HEALER_REACH_YD
                                                            for row in living):
                bare[(side, tank, label)] += step
    return {"healers": healers, "main": main, "labels": labels, "gaps": gaps, "far": far, "bare": bare}


def healer_follows(trace: Trace, samples: Samples | None = None) -> dict[int, list[int]]:
    """Per healer, its `follow` act rows with an OK verdict while it lived, from the pull to the end. A verdict
    is written when it changes and again every 10 s it holds, so these are rows, not ticks."""
    samples = samples or Samples(trace)
    healers = set(healer_guids(trace))
    end = pull_end(trace)
    out: dict[int, list[int]] = collections.defaultdict(list)
    for rec in trace.of("act"):
        guid = rec.get("g")
        if guid not in healers or rec.get("a") != "follow" or rec.get("vd") != "OK" or not 0 <= rec["t"] <= end:
            continue
        row = samples.recent(guid, rec["t"])
        if row is None or row[5] > 0:
            out[guid].append(rec["t"])
    return dict(out)


def engine_passes(trace: Trace) -> dict[int, dict[str, int]]:
    """Per bot, the passes each engine ran from the session's open: every node lands in exactly one of
    PASS_COLUMNS on each pass, so their sum is the count. The most common sum is taken, because a node two
    strategies of one engine both carry shares one id and reads double."""
    sums: dict = collections.defaultdict(lambda: collections.defaultdict(collections.Counter))
    for rec in trace.of("cov"):
        for row in rec.get("r", []):
            columns = dict(zip(COVERAGE_COLUMNS, list(row[1:]) + [0] * (len(COVERAGE_COLUMNS) + 1 - len(row))))
            engine = trace.covnodes.get(row[0], {}).get("engine", "?")
            sums[rec.get("g")][engine][row[0]] += sum(columns[name] for name in PASS_COLUMNS)
    return {guid: {engine: collections.Counter(nodes.values()).most_common(1)[0][0]
                   for engine, nodes in engines.items()}
            for guid, engines in sums.items()}


def healer_engine_coverage(trace: Trace) -> dict[str, dict]:
    """Per engine, the bots whose `cov` rows carry a `raid healer engine` node, and its checks, fires and wins
    summed."""
    ids = {node_id for node_id, node in trace.covnodes.items() if node["strategy"] == HEALER_ENGINE}
    out: dict[str, dict] = {}
    for rec in trace.of("cov"):
        for row in rec.get("r", []):
            if row[0] not in ids:
                continue
            columns = dict(zip(COVERAGE_COLUMNS, list(row[1:]) + [0] * (len(COVERAGE_COLUMNS) + 1 - len(row))))
            entry = out.setdefault(trace.covnodes[row[0]]["engine"], {"bots": set(), "checks": 0, "fires": 0, "won": 0})
            entry["bots"].add(rec.get("g"))
            for name in ("checks", "fires", "won"):
                entry[name] += columns[name]
    return out


def hstand_time(trace: Trace) -> dict[int, dict[str, int]]:
    """Per bot, the milliseconds of the pull each `anub.hstand` value held."""
    end = pull_end(trace)
    out: dict[int, dict[str, int]] = {}
    for guid, spans in holder_spans(trace, "anub.hstand", end).items():
        held: dict[str, int] = {}
        for value, start, stop in spans:
            held[value] = held.get(value, 0) + covered([(start, stop)], 0, end)
        out[guid] = held
    return out


def show_healer_engines(trace: Trace, healers: list[int]) -> None:
    if not trace.of("cov"):
        print("  no cov records, so no engine passes and no raid healer engine coverage")
        return
    passes = engine_passes(trace)
    parts = []
    for guid in healers:
        mine = passes.get(guid, {})
        combat, idle = mine.get("c", 0), mine.get("n", 0)
        share = f" ({idle * 100 / (combat + idle):.0f}%)" if combat + idle else ""
        parts.append(f"{trace.name(guid)} " + ", ".join(
            f"{ENGINE_NAMES[engine]} {mine.get(engine, 0)}" + (share if engine == "n" else "") for engine in "cnd"))
    print("  engine passes from the session's open, the non-combat share of combat and non-combat: "
          + ("; ".join(parts) or "none"))
    coverage = healer_engine_coverage(trace)
    if not coverage:
        print(f"  {HEALER_ENGINE} in the coverage: absent from every engine")
        return
    print(f"  {HEALER_ENGINE} in the coverage: " + "; ".join(
        f"{ENGINE_NAMES.get(engine, engine)} {len(entry['bots'])} bot(s), {entry['checks']} checks,"
        f" {entry['fires']} fires, {entry['won']} won" for engine, entry in sorted(coverage.items())))


def show_healers(trace: Trace) -> None:
    print("HEALERS")
    if not is_toc_anubarak(trace):
        print("  not an Anub'arak pull")
        return
    samples = Samples(trace)
    rows = healer_rows(trace, samples)
    if not rows["healers"]:
        print("  no healer on the roster")
        return

    def median(values) -> str:
        return f"{statistics.median(values):.1f}" if values else "-"

    print(f"  median yards from each healer to the main tank ({trace.name(rows['main'])}) and to whoever held each"
          f" side, per phase;\n  far is the seconds over {HEALER_FAR_YD:.0f} yd from a side tank a burrower was on:")
    print(f"  {'healer':14} {'phase':5} {'main':>6} {'side 0':>6} {'side 1':>6} {'far 0':>6} {'far 1':>6}")
    for guid in rows["healers"]:
        for label in rows["labels"]:
            gaps = rows["gaps"].get((guid, label), {})
            far = [rows["far"][(guid, side, label)] / 1000 for side in (0, 1)]
            if not gaps and not any(far):
                continue
            print(f"  {trace.name(guid)[:14]:14} {label:5} {median(gaps.get('main')):>6} {median(gaps.get(0)):>6}"
                  f" {median(gaps.get(1)):>6} {far[0]:6.1f} {far[1]:6.1f}"
                  + ("  human" if guid in trace.humans else ""))

    follows = healer_follows(trace, samples)
    print("  follow acts, OK and alive in the pull: " + (", ".join(
        f"{trace.name(guid)} {len(times)} ({clock(times[0])}-{clock(times[-1])})"
        for guid, times in sorted(follows.items(), key=lambda item: trace.name(item[0]))) or "none")
          + "; rows per verdict change")
    show_healer_engines(trace, rows["healers"])
    show_counts(trace, "anub.hstand")
    held = hstand_time(trace)
    if held:
        print("  anub.hstand seconds: " + "; ".join(
            f"{trace.name(guid)} " + ", ".join(f"{value} {ms / 1000:.1f} s" for value, ms in values.items())
            for guid, values in sorted(held.items(), key=lambda item: trace.name(item[0]))))

    bare: dict[tuple[int, int], list[str]] = collections.defaultdict(list)
    for (side, tank, label), ms in rows["bare"].items():
        if ms:
            bare[(side, tank)].append(f"{label} {ms / 1000:.1f} s")
    print(f"  side tank alive with no living healer within {HEALER_REACH_YD:.0f} yd:"
          + ("" if bare else " none"))
    for (side, tank), parts in sorted(bare.items()):
        print(f"    side {side} {trace.name(tank)[:14]:14} {', '.join(parts)}")


# ---------------------------------------------------------------------------------------------- tanks

def stack_tracks(trace: Trace, spells) -> dict[int, list[tuple[int, int]]]:
    """Per unit, `(t, stacks)` at every aura row under any of `spells`, 0 at a removal."""
    out: dict[int, list[tuple[int, int]]] = collections.defaultdict(list)
    for rec in trace.of("aura"):
        if rec.get("sp") in spells:
            out[rec.get("d")].append((rec["t"], 0 if rec.get("r") else int(rec.get("st") or 1)))
    return dict(out)


def expose_tracks(trace: Trace) -> dict[int, list[tuple[int, int]]]:
    return stack_tracks(trace, EXPOSE_WEAKNESS)


def stack_at(track: list[tuple[int, int]], when: int) -> int:
    return next((stacks for at, stacks in reversed(track) if at <= when), 0)


def stack_window(track: list[tuple[int, int]], low: int, high: int) -> tuple[int, int]:
    """`(peak, ms)`: the highest stack held inside `[low, high)` and the time at DEFENSIVE_STACKS or more."""
    held = next((stacks for at, stacks in reversed(track) if at < low), 0)
    peak, ms, since = held, 0, low
    for at, stacks in track:
        if not low <= at < high:
            continue
        if held >= DEFENSIVE_STACKS:
            ms += at - since
        held, since = stacks, at
        peak = max(peak, held)
    if held >= DEFENSIVE_STACKS:
        ms += high - since
    return peak, ms


def side_tank_list(trace: Trace) -> list[int]:
    """Every holder of `anub.side0` or `anub.side1`, side 0's first, each once."""
    _, holders = side_tanks(trace)
    return list(dict.fromkeys(guid for side in sorted(holders) for guid in holders[side]))


def expose_waves(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per burrower wave and side tank, the peak Expose Weakness stack and the milliseconds at DEFENSIVE_STACKS
    or more, from the wave's spawn to the next wave's or the end."""
    samples = samples or Samples(trace)
    waves = wave_starts(trace, samples)
    end = pull_end(trace)
    tracks = expose_tracks(trace)
    tanks = side_tank_list(trace)
    out = []
    for index, (wave, spawn) in enumerate(waves):
        upto = waves[index + 1][1] if index + 1 < len(waves) else end
        for tank in tanks:
            peak, ms = stack_window(tracks.get(tank, []), spawn, upto)
            out.append({"wave": wave, "start": spawn, "stop": upto, "tank": tank, "peak": peak, "ms": ms})
    return out


def held_side(sides, guid: int, when: int) -> int | None:
    return next((side for side, spans in enumerate(sides) if as_guid(held_at(spans, when)) == guid), None)


def defensive_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Each TANK_DEFENSIVES cast by a tank, from its cast row: the bot, its Expose Weakness stack and health
    then, the phase, and the side it held, None for neither. `anub.defensive` is written once per verdict
    change, so a second cast of the same button never shows there."""
    samples = samples or Samples(trace)
    tracks = expose_tracks(trace)
    sides = side_spans(trace)
    spans = phase_spans(trace)
    out = []
    for rec in trace.of("cast"):
        guid, when = rec.get("s"), rec["t"]
        if rec.get("tr") or rec.get("sp") not in TANK_DEFENSIVES or trace.role(guid) != "tank":
            continue
        row = samples.recent(guid, when)
        out.append({"t": when, "guid": guid, "spell": TANK_DEFENSIVES[rec["sp"]],
                    "stacks": stack_at(tracks.get(guid, []), when),
                    "hp": row[5] if row else None, "phase": phase_at(spans, when),
                    "side": held_side(sides, guid, when)})
    return out


def magic_on_side_tank(row: dict) -> bool:
    """A magic-only defensive on a side tank before the swarm: burrowers hit physical."""
    return row["side"] is not None and row["spell"] in MAGIC_ONLY_DEFENSIVES and not row["phase"].startswith("P3")


def source_kind(trace: Trace, guid) -> str:
    kinds = {NPC_BURROWER: "burrower", NPC_SCARAB: "scarab", NPC_ANUBARAK: "Anub'arak", NPC_SPIKE: "spike",
             NPC_FROST_SPHERE: "sphere"}
    return kinds.get(trace.entries.get(guid)) or trace.name(guid)


def tank_deaths(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Each tank's death: the damage it took in the TANK_DEATH_LOOKBACK_MS before, by source kind, the heals
    landed on it then, gross and overheal, the absorbs, its peak Expose Weakness stack then, the side it
    held, and whether it was the main tank."""
    samples = samples or Samples(trace)
    main = main_tank(trace, samples)
    tracks = expose_tracks(trace)
    sides = side_spans(trace)
    spans = phase_spans(trace)
    hits, heals, shields = trace.of("dmg"), trace.of("heal"), trace.of("abs")
    out = []
    for rec in combat_deaths(trace):
        guid, when = rec.get("g"), rec["t"]
        if trace.role(guid) != "tank":
            continue
        low = when - TANK_DEATH_LOOKBACK_MS

        def inside(row) -> bool:
            return row.get("d") == guid and low <= row["t"] <= when

        taken: collections.Counter = collections.Counter()
        for hit in filter(inside, hits):
            taken[source_kind(trace, hit.get("s"))] += hit.get("a", 0)
        landed = list(filter(inside, heals))
        out.append({"t": when, "guid": guid, "phase": phase_at(spans, when), "side": held_side(sides, guid, when - 1),
                    "main": guid == main, "taken": taken, "heals": len(landed),
                    "gross": sum(row.get("a", 0) for row in landed),
                    "overheal": sum(row.get("oh", 0) for row in landed),
                    "absorbed": sum(row.get("a", 0) for row in filter(inside, shields)),
                    "expose": stack_window(tracks.get(guid, []), low, when + 1)[0]})
    return out


def show_tanks(trace: Trace) -> None:
    print("TANKS")
    if not is_toc_anubarak(trace):
        print("  not an Anub'arak pull")
        return
    samples = Samples(trace)
    waves = expose_waves(trace, samples)
    if not waves:
        print("  no burrower wave or no side tank latched")
    else:
        print(f"  Expose Weakness ({', '.join(str(spell) for spell in EXPOSE_WEAKNESS)}) per burrower wave on each"
              f" side tank: peak stack,\n  seconds at {DEFENSIVE_STACKS} or more")
        by_wave: dict[tuple[int, int], list[str]] = collections.defaultdict(list)
        for row in waves:
            by_wave[(row["wave"], row["start"])].append(f"{trace.name(row['tank'])} {row['peak']}"
                                                        f" ({row['ms'] / 1000:.1f} s)")
        for (wave, start), parts in by_wave.items():
            print(f"    wave {wave}  {clock(start):>9}  {', '.join(parts)}")
    tanks = set(side_tank_list(trace))
    others = {guid: max(stacks for _, stacks in track) for guid, track in expose_tracks(trace).items()
              if guid not in tanks and guid in trace.roles}
    print("  Expose Weakness on anyone else, peak over the pull: "
          + (", ".join(f"{trace.name(guid)} {peak}" for guid, peak in sorted(others.items(), key=lambda item: -item[1]))
             or "none"))

    rows = defensive_rows(trace, samples)
    if not rows:
        print("  no tank defensive cast")
    else:
        print("\n  each tank defensive cast, from its cast row, with the Expose Weakness stack and health then:")
        for row in rows:
            side = "-" if row["side"] is None else f"side {row['side']}"
            hp = "-" if row["hp"] is None else f"{row['hp']:3.0f}%"
            print(f"    {clock(row['t']):>9} {trace.name(row['guid'])[:14]:14} {side:6} {row['phase']:5}"
                  f" {row['spell'][:22]:22} stack {row['stacks']:2}  hp {hp:>4}"
                  + ("  magic only on a side tank" if magic_on_side_tank(row) else ""))
        print(f"  from {DEFENSIVE_STACKS} stacks: "
              + f"{sum(1 for row in rows if row['side'] is not None and row['stacks'] >= DEFENSIVE_STACKS)} of"
                f" {sum(1 for row in rows if row['side'] is not None)} side tank cast(s); magic only on a side tank"
                f" outside phase 3: {sum(1 for row in rows if magic_on_side_tank(row))}")
        per_tank: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
        for row in rows:
            per_tank[row["guid"]][row["spell"]] += 1
        print("  casts per tank: " + "; ".join(
            f"{trace.name(guid)} " + ", ".join(f"{spell} {n}" for spell, n in counts.most_common())
            for guid, counts in sorted(per_tank.items(), key=lambda item: trace.name(item[0]))))

    deaths = tank_deaths(trace, samples)
    if not deaths:
        print("  no tank died")
        return
    print(f"\n  each tank death, the {TANK_DEATH_LOOKBACK_MS / 1000:.0f} s before it:")
    for row in deaths:
        side = f"side {row['side']}" if row["side"] is not None else "main" if row["main"] else "-"
        taken = sum(row["taken"].values())
        sources = ", ".join(f"{kind} {amount:,}" for kind, amount in row["taken"].most_common())
        print(f"    {clock(row['t']):>9} {trace.name(row['guid'])[:14]:14} {side:6} {row['phase']:5}"
              f" taken {taken:,}" + (f" ({sources})" if sources else "")
              + f"; healed {row['gross'] - row['overheal']:,} by {row['heals']} heal(s), {row['overheal']:,} over;"
                f" absorbed {row['absorbed']:,}; Expose peak {row['expose']}")


# -------------------------------------------------------------------------------------------- scarabs

def sampled_at(samples: Samples, guid: int, index: int):
    """The unit's row in snapshot `index`, None when that snapshot left it out."""
    stamps, rows = samples.track(guid)
    at = bisect.bisect_right(stamps, samples.stamps[index]) - 1
    return rows[at] if at >= 0 and stamps[at] == samples.stamps[index] else None


def scarab_lives(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per scarab, its first and last living sample, the phase span it came up in, and `past`, how long it
    lived after that span ended, None for one that died inside it."""
    samples = samples or Samples(trace)
    spans = phase_spans(trace)
    out = []
    for guid in sorted(guids_of_entry(trace, NPC_SCARAB)):
        stamps, rows = samples.track(guid)
        living = [when for when, row in zip(stamps, rows) if row[5] > 0]
        if not living:
            continue
        first, last = living[0], living[-1]
        span = next(((start, stop, label) for start, stop, _, label in spans if start <= first < stop), None)
        out.append({"guid": guid, "first": first, "last": last, "label": span[2] if span else "pre",
                    "start": span[0] if span else None,
                    "past": last - span[1] if span and last >= span[1] else None})
    return out


def scarab_emerges(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per emerge, the living scarabs in the snapshot at or before each SCARAB_OFFSETS_MS after it, as a Counter
    of their targets, and the main tank's Mandibles stack at the emerge. An offset past the trace's last
    snapshot is left out."""
    samples = samples or Samples(trace)
    scarabs = sorted(guids_of_entry(trace, NPC_SCARAB))
    main = main_tank(trace, samples)
    track = stack_tracks(trace, MANDIBLES).get(main, []) if main else []
    out = []
    for window in surfaced_windows(phase_spans(trace)):
        if window["kind"] != "emerge":
            continue
        start = window["start"]
        alive = []
        for offset in SCARAB_OFFSETS_MS:
            index = samples.index_at(start + offset)
            if index < 0 or start + offset > samples.stamps[-1]:
                break
            rows = [row for row in (sampled_at(samples, guid, index) for guid in scarabs) if row and row[5] > 0]
            alive.append((offset, collections.Counter(row[7] if len(row) > 7 else 0 for row in rows)))
        out.append({"label": window["label"], "start": start, "alive": alive, "main": main,
                    "stacks": stack_at(track, start)})
    return out


def mandible_windows(trace: Trace) -> list[dict]:
    """Per phase span, `(guid, peak stack, damage)` for each roster member that carried Acid-Drenched Mandibles
    or took its ticks in it, most damage first. A stack carried in from the span before counts as its peak."""
    roster = roster_guids(trace)
    tracks = {guid: track for guid, track in stack_tracks(trace, MANDIBLES).items() if guid in roster}
    hits = [rec for rec in trace.of("dmg") if rec.get("sp") in MANDIBLES and rec.get("d") in roster]
    out = []
    for start, stop, _, label in phase_spans(trace):
        damage: collections.Counter = collections.Counter()
        for rec in hits:
            if start <= rec["t"] < stop:
                damage[rec["d"]] += rec.get("a", 0)
        peaks = {guid: stack_window(track, start, stop)[0] for guid, track in tracks.items()}
        players = [(guid, peaks.get(guid, 0), damage[guid]) for guid in roster if damage[guid] or peaks.get(guid)]
        out.append({"label": label, "start": start, "stop": stop,
                    "players": sorted(players, key=lambda item: (-item[2], -item[1], trace.name(item[0])))})
    return out


def show_scarabs(trace: Trace) -> None:
    print("SCARABS")
    if not is_toc_anubarak(trace):
        print("  not an Anub'arak pull")
        return
    samples = Samples(trace)
    emerges = scarab_emerges(trace, samples)
    if emerges:
        print("  living scarabs after each emerge, by target, and the main tank's Mandibles stack at the emerge:")
    for row in emerges:
        counts = "; ".join(f"+{offset // 1000} s {sum(targets.values())}"
                           + (" (" + ", ".join(f"{unit_label(trace, guid)} {n}" for guid, n in targets.most_common())
                              + ")" if targets else "")
                           for offset, targets in row["alive"])
        main = f"{trace.name(row['main'])} {row['stacks']}" if row["main"] else "no main tank"
        print(f"    {row['label']:5} {clock(row['start']):>9}  {counts or 'no snapshot after it'}"
              f"\n                     main tank {main}")

    lives = scarab_lives(trace, samples)
    if not lives:
        print("  no scarab sampled")
    else:
        print("\n  scarab lives, first living sample to last, by the phase each came up in:")
        by_label: dict[str, list[dict]] = collections.defaultdict(list)
        for row in sorted(lives, key=lambda row: row["first"]):
            by_label[row["label"]].append(row)
        for label, rows in by_label.items():
            spans = [(row["last"] - row["first"]) / 1000 for row in rows]
            past = [row["past"] / 1000 for row in rows if row["past"] is not None]
            start = "-" if rows[0]["start"] is None else clock(rows[0]["start"])
            print(f"    {label:5} {start:>9}  {len(rows)} scarab(s), median {statistics.median(spans):.1f} s,"
                  f" longest {max(spans):.1f} s; {len(past)} lived past its end"
                  + (f", longest {max(past):.1f} s on" if past else ""))

    windows = [row for row in mandible_windows(trace) if row["players"]]
    if not windows:
        print("  nobody carried Acid-Drenched Mandibles")
        return
    print("\n  Acid-Drenched Mandibles per phase: the raid's tick damage, then each player's peak stack and damage:")
    for row in windows:
        total = sum(damage for _, _, damage in row["players"])
        parts = ", ".join(f"{trace.name(guid)} {peak} {damage:,}" for guid, peak, damage in row["players"])
        print(f"    {row['label']:5} {clock(row['start']):>9}  {total:,}: {parts}")


# ------------------------------------------------------------------------------------------ redirects

def redirect_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Each Misdirection and Tricks of the Trade cast: caster, recipient and its frost group, the caster's
    target at the cast, and for a burrower `after`, its target at each REDIRECT_AFTER_MS, a repeat left out."""
    samples = samples or Samples(trace)
    groups = frost_groups(trace, samples)
    burrowers = guids_of_entry(trace, NPC_BURROWER)
    out = []
    for rec in trace.of("cast"):
        if rec.get("tr") or rec.get("sp") not in REDIRECTS:
            continue
        caster, when, recipient = rec.get("s"), rec["t"], rec.get("tgt")
        row = samples.row(caster, when)
        target = row[7] if row and len(row) > 7 else 0
        after: list[tuple[int, int]] = []
        if target in burrowers:
            for offset in REDIRECT_AFTER_MS:
                unit = samples.recent(target, when + offset)
                victim = unit[7] if unit and len(unit) > 7 else 0
                if unit is not None and (not after or after[-1][1] != victim):
                    after.append((offset, victim))
        out.append({"t": when, "spell": rec["sp"], "caster": caster, "to": recipient,
                    "group": groups.get(recipient, trace.role(recipient)), "target": target,
                    "on_burrower": target in burrowers, "after": after})
    return out


def show_redirects(trace: Trace) -> None:
    print("REDIRECTS")
    if not is_toc_anubarak(trace):
        print("  not an Anub'arak pull")
        return
    rows = redirect_rows(trace)
    if not rows:
        print("  no Misdirection or Tricks of the Trade cast")
        return
    print("  each Misdirection and Tricks of the Trade: caster, recipient, the caster's target, and a burrower's"
          " target after:")
    for row in rows:
        after = ", ".join(f"+{offset // 1000} s {unit_label(trace, victim)}" for offset, victim in row["after"])
        print(f"    {clock(row['t']):>9} {REDIRECTS[row['spell']]:19} {trace.name(row['caster'])[:12]:12}"
              f" > {trace.name(row['to'])[:14]:14} {row['group'][:9]:9} on {unit_label(trace, row['target'])}"
              + (f": {after}" if after else ""))
    for spell, name in REDIRECTS.items():
        mine = [row for row in rows if row["spell"] == spell]
        if not mine:
            print(f"  {name}: none")
            continue
        to = collections.Counter((row["to"], row["group"]) for row in mine)
        groups = collections.Counter(row["group"] for row in mine)
        print(f"  {name} {len(mine)}: " + ", ".join(f"{trace.name(guid)} ({group}) {n}" for (guid, group), n
                                                     in to.most_common())
              + "; by group " + ", ".join(f"{group} {n}" for group, n in groups.most_common())
              + f"; the caster on a burrower {sum(1 for row in mine if row['on_burrower'])}")
    print("  the group is the --frost one: side tank for any anub.side0 or anub.side1 holder, main tank the other"
          " tank\n  he targeted most, else the role")


# -------------------------------------------------------------------------------------------- pursuit

def chase_runs(samples: Samples, spikes, kiter: int, start: int, stop: int) -> list[list[tuple]]:
    """The snapshots inside the mark where a spike targeted the kiter and both were sampled, as `(t, spike,
    kiter, patches)` with ground points, split where PURSUIT_GAP_MS or more passed between two."""
    kiter_stamps, kiter_rows = samples.track(kiter)
    points = []
    for spike in spikes:
        stamps, rows = samples.track(spike)
        for index in range(bisect.bisect_left(stamps, start), bisect.bisect_right(stamps, stop)):
            row, when = rows[index], stamps[index]
            at = bisect.bisect_left(kiter_stamps, when)
            if len(row) > 7 and row[7] == kiter and at < len(kiter_stamps) and kiter_stamps[at] == when:
                points.append((when, (row[1], row[2]), (kiter_rows[at][1], kiter_rows[at][2]), samples.frost_at(when)))
    runs: list[list[tuple]] = []
    for point in sorted(points, key=lambda item: item[0]):
        if runs and point[0] - runs[-1][-1][0] < PURSUIT_GAP_MS:
            runs[-1].append(point)
        else:
            runs.append([point])
    return runs


def replay_pursuit(run: list[tuple], first: int, horizon: int) -> tuple[float, list[float]] | None:
    """A model spike from the real one's spot at `first`, stepping the real spike's step length each snapshot
    straight at where the kiter is then, for `horizon` ms: its end error against the real spike, and per patch
    within PURSUIT_PATCH_YD of the start its closest pass less the real one's. None when the run ends first."""
    times = [point[0] for point in run]
    last = bisect.bisect_left(times, run[first][0] + horizon)
    if last >= len(run):
        return None
    spot = run[first][1]
    path = [spot]
    for index in range(first, last):
        step = dist2(run[index][1], run[index + 1][1])
        goal = run[index][2]
        gap = dist2(spot, goal)
        if gap > 1e-3:
            share = min(step, gap) / gap
            spot = (spot[0] + (goal[0] - spot[0]) * share, spot[1] + (goal[1] - spot[1]) * share)
        path.append(spot)
    real = [point[1] for point in run[first:last + 1]]
    patches = [patch for patch in run[first][3] if dist2(patch, run[first][1]) < PURSUIT_PATCH_YD]
    passes = [min(dist2(point, patch) for point in path) - min(dist2(point, patch) for point in real)
              for patch in patches]
    return dist2(path[-1], real[-1]), passes


def pursuit_rows(trace: Trace, samples: Samples | None = None) -> list[dict]:
    """Per mark, pure pursuit replayed against the real spike from every PURSUIT_STEP_MS of each chase run, as
    long as the longest horizon still fits: per horizon the end errors and the signed patch-pass errors."""
    samples = samples or Samples(trace)
    spikes = guids_of_entry(trace, NPC_SPIKE)
    longest = max(PURSUIT_HORIZONS_MS)
    out = []
    for guid, start, stop, _ in mark_spans(trace):
        ends: dict[int, list[float]] = {horizon: [] for horizon in PURSUIT_HORIZONS_MS}
        passes: dict[int, list[float]] = {horizon: [] for horizon in PURSUIT_HORIZONS_MS}
        for run in chase_runs(samples, spikes, guid, start, stop):
            times = [point[0] for point in run]
            begin = run[0][0]
            while begin + longest <= run[-1][0]:
                first = bisect.bisect_left(times, begin)
                for horizon in PURSUIT_HORIZONS_MS:
                    result = replay_pursuit(run, first, horizon)
                    if result is not None:
                        ends[horizon].append(result[0])
                        passes[horizon].extend(result[1])
                begin += PURSUIT_STEP_MS
        out.append({"guid": guid, "start": start, "stop": stop, "ends": ends, "passes": passes})
    return out


def p90(values: list[float]) -> float:
    ordered = sorted(values)
    return ordered[int(len(ordered) * 0.9)]


def pursuit_flagged(passes: list[float]) -> bool:
    return bool(passes) and p90([abs(value) for value in passes]) > PURSUIT_FLAG_YD


def pursuit_text(ends: list[float], passes: list[float]) -> str:
    text = f"end {statistics.median(ends):.1f} / {p90(ends):.1f} yd"
    if passes:
        sizes = [abs(value) for value in passes]
        text += (f", patch pass {statistics.median(sizes):.1f} / {p90(sizes):.1f} yd bias"
                 f" {statistics.median(passes):+.1f} (n {len(passes)})")
    return text


def show_pursuit(trace: Trace) -> None:
    print("PURSUIT")
    if not is_toc_anubarak(trace):
        print("  not an Anub'arak pull")
        return
    rows = [row for row in pursuit_rows(trace) if any(row["ends"].values())]
    if not rows:
        print(f"  no chase lasted the {max(PURSUIT_HORIZONS_MS) / 1000:.0f} s a replay needs")
        return
    print("  pure pursuit replayed per kite, median / p90 per horizon:")
    for row in rows:
        parts = [f"{horizon // 1000} s n {len(row['ends'][horizon])}: "
                 + pursuit_text(row["ends"][horizon], row["passes"][horizon])
                 for horizon in PURSUIT_HORIZONS_MS if row["ends"][horizon]]
        flagged = any(pursuit_flagged(row["passes"][horizon]) for horizon in PURSUIT_HORIZONS_MS)
        print(f"    {clock(row['start']):>9} {trace.name(row['guid'])[:14]:14} " + "; ".join(parts)
              + (f"  patch pass p90 over {PURSUIT_FLAG_YD} yd" if flagged else ""))
    flagged = []
    for horizon in PURSUIT_HORIZONS_MS:
        ends = [value for row in rows for value in row["ends"][horizon]]
        passes = [value for row in rows for value in row["passes"][horizon]]
        if not ends:
            continue
        print(f"  all kites, {horizon // 1000} s, n {len(ends)}: {pursuit_text(ends, passes)}")
        if pursuit_flagged(passes):
            flagged.append(f"{horizon // 1000} s")
    print(f"  p90 patch-pass error over {PURSUIT_FLAG_YD} yd over all kites: " + (", ".join(flagged) or "no"))
    print(f"  from every {PURSUIT_STEP_MS / 1000:.1f} s of a chase the model spike steps the real one's step"
          " length each snapshot straight\n  at the kiter's spot then; end is its distance from the real spike"
          f" after the horizon, patch pass its closest\n  pass by each patch within {PURSUIT_PATCH_YD:.0f} yd of the"
          " start less the real spike's, bias that error's signed median")


SECTIONS = (
    ("phases", "anub.phase spans, his health at each edge, deaths per phase", show_phases),
    ("pull", "locked out past the Web Door or never landed, stage 9 to the pull", show_pull),
    ("spike", "every Pursued by Anub'arak window, kite branches, outcome, protection, kite stages, boosts,"
     " a kiter marked in a patch, the tank patch spent and one spent in the run stage, the spike's approach to"
     " each tank patch, the spike against the gather and unlatched patches at each mark's end, the speeds at"
     " each cover, the kiter standing still, a flip-flopping walk, the nearest HoP-ready paladin at 12 yd, Feign"
     " Death dropped at once, Impale on others, the crowd where he went down", show_spike),
    ("spheres", "spheres over time, patches the spike used, patch supply at each mark, duty shots, killers,"
     " landings, grips from the cast spot, the grip duty to kill and a sphere back up, anub.spot0 and"
     " anub.spot1 changes, each side's first tank patch, spheres and tank patches spent per submerge against"
     " the allowance and when the side's patch came back", show_spheres),
    ("burrowers", "each burrower: life, submerges, Permafrost, Shadow Strike, dead inside 30 s up, exit, side,"
     " pickup, DPS focus, casts off the focus, focus due per wave and dropped with the focus alive, each focus"
     " pick against its tank and the patches, distance from the patch centre while on a side tank, where each"
     " submerge went under, each opener and the players nearest its exit", show_burrowers),
    ("frost", "seconds carrying Permafrost per group and bot, he up and he down, stints over 3 s with the"
     " bot's target", show_frost),
    ("threat", "per surfaced window: his target by role, time to a tank, drag legs, taunts, threat cap, main"
     " tank in a patch, submerge spot to the tank patches", show_threat),
    ("swarm", "phase 3: Leeching Swarm, lust, Penetrating Cold heals, defensives, deaths", show_swarm),
    ("healers", "per healer and phase the distance to the main and side tanks, far from a side tank on a burrower,"
     " follow acts, engine passes, anub.hstand; side tanks with no healer in reach; raid healer engine coverage",
     show_healers),
    ("tanks", "Expose Weakness per wave per side tank, each tank defensive cast with its stack, each tank death"
     " with the damage and heals before it", show_tanks),
    ("scarabs", "scarabs alive after each emerge and their targets, the main tank's Mandibles stack then, scarab"
     " lives per phase, Mandibles peak stack and damage per player per phase", show_scarabs),
    ("redirects", "each Misdirection and Tricks of the Trade with its caster's target and a burrower's target"
     " after, totals per recipient", show_redirects),
    ("pursuit", "pure pursuit replayed against the real spike per kite, end and patch-pass errors at 3 and 6 s",
     show_pursuit),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
