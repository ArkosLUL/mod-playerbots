#!/usr/bin/env python3
"""Score an Algalon the Observer pull from a RaidObs trace: Big Bang, stars, holes, constellations.

    algalon.py <file>                   every section
    algalon.py <file> --phases          phase spans, deaths per phase, boss health at each change, enrage
    algalon.py <file> --pull            the intro's end: who stood in his reach, his first victim and swings
    algalon.py <file> --deaths          every death by the mechanic that killed it, cause by role
    algalon.py <file> --loose           every stretch he spent off the swap pair: cause, hits, deaths, pickup
    algalon.py <file> --swaps           the swap pair, Phase Punch, swaps due and their latency, threat steals
    algalon.py <file> --reach           seconds each raid member outside the pair stood in his reach
    algalon.py <file> --bigbang         every Big Bang: holes, soaker and backup, who it hit, what followed
    algalon.py <file> --phasing         every hole phase: Big Bang or stray, and what put a stray one there
    algalon.py <file> --stars           star deaths, what each landed into, the kill window, the team
    algalon.py <file> --holes           hole stock, empty seconds before each Big Bang, holes lost, urgency
    algalon.py <file> --constellations  activations, victims, the handler and kite branches, removals
    algalon.py <file> --smash           every Cosmic Smash wave: who stood near it, the dodge, damage
    algalon.py <file> --tanks           Phase Punch stacks, swaps, non-tank pickups, Quantum Strike
    algalon.py <file> --formation       each ring bot's distance to its slot and to him, spot branches
    algalon.py <file> --darkmatter      phase 2 Unleashed Dark Matter: loose seconds, handler damage, kills
    algalon.py <file> --verdict         one line per detected failure, in time order

What the generic views get wrong here, and what this reads instead:

- **Nothing counts from the pull.** The intro runs 26 s on a first pull and 8.5 s after, so the
  6 min enrage runs from P1's start. `algalon.phase` names it; a trace without the probe falls back
  to his first cast, and to 20 % health or the first Worm Hole for P2.
- **Being phased is an aura, not a place, and it has no fixed length.** A hole's field re-applies
  62168/65250 every second to whoever stands in it, and Big Bang's 64445 strips every phase aura,
  64417 included, about a second after the hit. Who a Big Bang could hit is read off those spans,
  who it did hit off its `dmg` rows. A row while the aura is held is a refresh; a new application
  right after a removal is the re-phase loop a bot still inside the field gets.
- **The soaker, the backup and a handler on `algalon.hide=hold` stay out on purpose**, so Big Bang
  damage on them is not a failure. Everyone else hit was unphased at the impact.
- **A creature's death reaches no record.** A star died where a Black Hole Explosion burst names it
  or a hole appears on its last spot; a constellation left through a hole when both rows vanish
  together.
- **`algalon.soaker`, `algalon.backup`, `algalon.focusstar` and `algalon.handler` are raid-wide
  latches**: `g` is whichever bot's tick flipped them and the guid is in `txt`. `algalon.hide`, `kite`,
  `spot` and `starwindow` are per bot and change-only, so a value holds until that bot's next row.
- **`postmortem.py --from`/`--band` measure every bot from one point**, and each ring bot has its own
  slot in `ULDUAR_ALGALON_RING_SLOTS`; `--formation` measures from the slot.
- **His victim is the target column plus his own swings.** Whoever he one-shots dies between two
  snapshots, so a melee or Quantum Strike row names a victim the column never shows. He is loose
  whenever that victim is outside the swap pair, `algalon.swappair` or else the roster's tanks.
- **The intro ends at his first swing when it beats the phase latch**, which trails the tracker's tick.
- **33105 is the meteor**, the damage's own source; 33104 stands about 2 yd off it. Two meteors of one
  wave can land a yard apart, so markers are never merged by place.
- **A swept unit can drop out of the snapshots and come back** on traces from before the sweep's
  anchor preferred an unphased player. A hole, a constellation or a Dark Matter counts as present
  from its first row to its last, gaps included.
- **While `algalon.urgent` reads 1 every ranged dps is on the star team** without being written into
  `algalon.starteam`, so star time there is not off-team.
"""
from __future__ import annotations

import bisect
import collections
import functools
import math
import pathlib
import re
import statistics
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.geometry import NUMBER, Unknown, anchor, dist2, first_seen, frames, guids_of_entry, radius  # noqa: E402
from raidobs.paths import RAID_ROOT  # noqa: E402
from raidobs.probes import emitted_keys, holder_spans, latch_spans, silent_keys  # noqa: E402
from raidobs.space import show_share, threat_share  # noqa: E402
from raidobs.stuck import stall_windows  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, death_records, notes, roster_guids  # noqa: E402
from raidobs.validity import DIFFICULTY  # noqa: E402

ENCOUNTER = "algalon"

NPC_ALGALON = 32871
NPC_ALGALON_ENTRIES = (NPC_ALGALON, 33070)
NPC_COLLAPSING_STAR = 32955
NPC_BLACK_HOLE = 32953
NPC_WORM_HOLE = 34099
NPC_LIVING_CONSTELLATION = 33052
NPC_COSMIC_SMASH_MARKERS = (33104, 33105)
NPC_UNLEASHED_DARK_MATTER = 34097
NPC_DARK_MATTER = 33089
HOLE_ENTRIES = (NPC_BLACK_HOLE, NPC_WORM_HOLE)

SPELL_BIG_BANG = (64443, 64584)
SPELL_BIG_BANG_UNPHASE = 64445
SPELL_PHASE_PUNCH = 64412
SPELL_PHASE_PUNCH_PHASE = 64417
SPELL_BLACK_HOLE_PHASE = 62168
SPELL_WORM_HOLE_PHASE = 65250
HOLE_PHASE = (SPELL_BLACK_HOLE_PHASE, SPELL_WORM_HOLE_PHASE)
PHASE_AURAS = HOLE_PHASE + (SPELL_PHASE_PUNCH_PHASE,)
SPELL_QUANTUM_STRIKE = (64395, 64592)
BOSS_SWINGS = (0,) + SPELL_QUANTUM_STRIKE
SPELL_COSMIC_SMASH = (62311, 64596)
SPELL_BLACK_HOLE_EXPLOSION = (64122, 65108)
SPELL_BLACK_HOLE_DOT = 62169
SPELL_CONSTELLATION_BARRAGE = (64599, 64607)
SPELL_ASCEND = 64487
SPELL_DISPERSION = 47585
SPELL_PAIN_SUPPRESSION = 33206
SPELL_GUARDIAN_SPIRIT = 47788
SOAK_SPELLS = (SPELL_DISPERSION, SPELL_PAIN_SUPPRESSION, SPELL_GUARDIAN_SPIRIT)
SPELL_RIGHTEOUS_DEFENSE = 31789
# Taunt, Hand of Reckoning, Righteous Defense, Growl, Dark Command, Death Grip, Mocking Blow by rank
TAUNT_SPELLS = frozenset((355, 62124, SPELL_RIGHTEOUS_DEFENSE, 6795, 56222, 49576, 694, 7400, 7402, 20559, 20560,
                          25266))
SMASH_MOVER = "algalon cosmic smash action"


def _anchor_or(name: str, fallback):
    try:
        return anchor(name)
    except Unknown:
        return fallback


def _radius_or(name: str, fallback: float) -> float:
    try:
        return radius(name)
    except Unknown:
        return fallback


# Read from the source so a retune shows up here without a second edit. Fallbacks for a checkout that
# doesn't declare one, or not as a literal.
ROOM_CENTER = _anchor_or("ULDUAR_ALGALON_ROOM_CENTER", (1632.668, -302.7656, 417.3211))
TANK_SLOT = _anchor_or("ULDUAR_ALGALON_TANK_SLOT", (1632.7, -321.5, 417.321))
OFFTANK_SLOT = _anchor_or("ULDUAR_ALGALON_OFFTANK_SLOT", None)
ROOM_RADIUS = _radius_or("ULDUAR_ALGALON_ROOM_RADIUS", 47.0)
SHELTER_RADIUS = _radius_or("ULDUAR_ALGALON_SHELTER_RADIUS", 6.0)
# combat reach 9 + 1.5 + 4/3 against a player
MELEE_REACH = _radius_or("ULDUAR_ALGALON_MELEE_REACH", 11.83)
DODGE_TRIGGER_YD = _radius_or("ULDUAR_ALGALON_COSMIC_SMASH_TRIGGER_RADIUS", 12.0)

ALGALON_SOURCE = RAID_ROOT / "Uld" / "Util" / "UldEncounter_Algalon.cpp"
RING_SLOT_TABLE = re.compile(r"\bULDUAR_ALGALON_RING_SLOTS\s*=\s*\{\{?(.*?)\}?\};", re.S)
RING_SLOT_ROW = re.compile(r"\bPosition\s*\(\s*" + NUMBER + r"\s*,\s*" + NUMBER + r"\s*,\s*" + NUMBER + r"\s*\)")

# uint32 in the source, or not named there at all.
BIG_BANG_CAST_MS = 8000
SHELTER_WINDOW_MS = 30000
STAR_GAP_MS = 8000
ENRAGE_MS = 360000
PHASE_AURA_MS = 10000
LOOP_MS = 10500
P2_HP = 20.0
# He resets whenever no living, unphased player is inside this.
RESET_RANGE = 120.0
SMASH_DELAY_MS = 4800
SWAP_STACKS = 4
# Black Hole Explosion's top roll, by raid size. Someone with less max health than this is safe only at
# full health.
EXPLOSION_SAFE_HP = {10: 16913, 25: 21526}

PHASE_NAMES = {"0": "idle", "1": "intro", "2": "P1", "3": "P2", "4": "won"}

HIT_SLACK_MS = 3000
DEFENSIVE_LEAD_MS = 3000
EVADE_WINDOW_MS = 6000
# a re-application this soon after a removal continues the same phased stretch
CHAIN_MS = 1500
# a unit last sampled this close to the end is still up
GONE_MS = 1000
BURST_GAP_MS = 1000
HOLE_MATCH_MS = 3000
HOLE_MATCH_YD = 10.0
SMASH_MATCH_MS = 2000
SMASH_BANDS = ((6.0, "<6"), (10.0, "6-10"), (15.0, "10-15"), (math.inf, "15+"))
WAVE_MS = 1000
IMPACT_MIN_MS = 3500
IMPACT_MAX_MS = 6500
SMASH_NEAR_YD = 20.0
SMASH_INSIDE_YD = 10.0
SMASH_CLEAR_YD = 15.0
METEOR_LIVE_MS = 5500
STALL_MS = 3000
PICKUP_MS = 1000
LOOSE_MS = 3000
OFF_TEAM_MS = 3000
KILLED_HP = 5.0
MAX_STEP_MS = 2000
TIMELINE_SHOWN = 16

# The phase latch and the pull note trail his first swing by up to a tick.
INTRO_LEAD_MS = 1500
INTRO_CAUSE_MS = 2000
OPENING_MS = 10000
FIRST_SWINGS = 5
TAUNT_MS = 1000
# the snapshot names a new victim up to a sample after it changed
SAMPLE_LAG_MS = 300
CAUSE_MS = 1500
LOOSE_MIN_MS = 300
# he drops one victim a moment before he picks the next
BRIDGE_MS = 2000
KILL_MATCH_MS = 1000
SHARE_MS = 3000
EXPLOSION_SHARE = 0.5
MATTER_BANG_MS = 3000
BANG_LEAD_MS = 1000
REPHASE_MS = 2000
NEW_HOLE_MS = 1500
NEW_HOLE_YD = 7.0
STAR_NOTE_MS = 2000
STAR_NEAR_YD = 9.0
DEATH_ROLES = ("tank", "heal", "melee", "ranged")


def secs(ms: int) -> str:
    return f"{ms / 1000:.1f}s"


def applications(count: int) -> str:
    return f"{count} application{'s' if count != 1 else ''}"


def pull_end(trace: Trace) -> int:
    ends = trace.of("end")
    if ends:
        return ends[-1]["t"]
    return trace.records[-1].get("t", 0) if trace.records else 0


def boss_guid(trace: Trace) -> int | None:
    guids = sorted(guid for entry in NPC_ALGALON_ENTRIES for guid in guids_of_entry(trace, entry))
    return guids[0] if guids else None


_UNSET = object()


def cached(trace: Trace, name: str, build):
    """`build(trace)`, kept on the trace: every view asks for the same derived tracks."""
    found = getattr(trace, name, _UNSET)
    if found is _UNSET:
        found = build(trace)
        setattr(trace, name, found)
    return found


def as_guid(text) -> int:
    try:
        return int(text)
    except (TypeError, ValueError):
        return 0


def who(trace: Trace, guid) -> str:
    return f"{trace.name(guid)} ({trace.role(guid)})"


def emitted(trace: Trace) -> set[str]:
    cached = getattr(trace, "_algalon_emitted", None)
    if cached is None:
        cached = emitted_keys(trace)
        trace._algalon_emitted = cached
    return cached


def absent(trace: Trace, *keys: str) -> list[str]:
    have = emitted(trace)
    return [key for key in keys if key not in have]


def show_absent(trace: Trace, *keys: str) -> None:
    gone = absent(trace, *keys)
    if gone:
        print(f"  absent from this trace: {', '.join(gone)}")


def value_at(spans, when: int):
    """What a `latch_spans`/`holder_spans` track held at `when`, or None outside every span."""
    for value, start, stop in spans:
        if start <= when < stop:
            return value
    return None


def last_set_in(spans, low: int, high: int) -> int:
    """The last non-zero guid a latch held anywhere in `[low, high]`."""
    found = 0
    for value, start, stop in spans:
        if start <= high and stop > low and as_guid(value):
            found = as_guid(value)
    return found


class Samples:
    """Every snapshot row by guid and by frame, so a view can ask for a row at a time, or for what one
    frame held, without rescanning."""

    def __init__(self, trace: Trace):
        self.stamps: list[int] = []
        self.units: list[dict[int, list]] = []
        self.rows: dict[int, tuple[list[int], list[list]]] = {}
        for snap in frames(trace):
            when = snap["t"]
            self.stamps.append(when)
            units = {}
            for row in snap.get("u", []):
                units[row[0]] = row
                stamps, rows = self.rows.setdefault(row[0], ([], []))
                stamps.append(when)
                rows.append(row)
            self.units.append(units)

    def row(self, guid: int, when: int):
        """The guid's last row at or before `when`, or None."""
        found = self.rows.get(guid)
        if not found:
            return None
        index = bisect.bisect_right(found[0], when)
        return found[1][index - 1] if index else None

    def between(self, guid: int, low: int, high: int) -> list[list]:
        found = self.rows.get(guid)
        if not found:
            return []
        return found[1][bisect.bisect_left(found[0], low):bisect.bisect_right(found[0], high)]

    def frame(self, when: int) -> dict[int, list]:
        """Every row of the last snapshot at or before `when`."""
        index = bisect.bisect_right(self.stamps, when)
        return self.units[index - 1] if index else {}

    def seen(self, guid: int) -> tuple[int, int] | None:
        found = self.rows.get(guid)
        return (found[0][0], found[0][-1]) if found else None

    def present(self, guid: int, when: int) -> bool:
        """From the guid's first row until the snapshot after its last, gaps in between included."""
        seen = self.seen(guid)
        if not seen or when < seen[0]:
            return False
        index = bisect.bisect_right(self.stamps, seen[1])
        return index >= len(self.stamps) or when < self.stamps[index]

    def steps(self, low: int = -(1 << 62), high: int = 1 << 62):
        """`(index, t, weight)` per snapshot in range, weighted by the gap to the next one."""
        for index, when in enumerate(self.stamps):
            if when < low or when >= high:
                continue
            nxt = self.stamps[index + 1] if index + 1 < len(self.stamps) else when
            yield index, when, min(nxt - when, MAX_STEP_MS)

    def unit_steps(self, guid: int, cap: int | None = MAX_STEP_MS):
        """`(t, row, weight)` over one unit's own rows, weighted by the gap to its next row. `cap=None`
        carries a row across a gap in the sweep, for a unit that was there all along."""
        found = self.rows.get(guid)
        if not found:
            return
        stamps, rows = found
        for index, (when, row) in enumerate(zip(stamps, rows)):
            nxt = stamps[index + 1] if index + 1 < len(stamps) else when
            yield when, row, nxt - when if cap is None else min(nxt - when, cap)


def samples_of(trace: Trace) -> Samples:
    cached = getattr(trace, "_algalon_samples", None)
    if cached is None:
        cached = Samples(trace)
        trace._algalon_samples = cached
    return cached


def position_at(samples: Samples, guid: int, when: int) -> tuple[float, float] | None:
    """Where a unit stood at `when`, read linearly between its rows either side. Across a gap longer
    than MAX_STEP_MS the earlier row stands."""
    found = samples.rows.get(guid)
    if not found:
        return None
    stamps, rows = found
    index = bisect.bisect_right(stamps, when)
    if not index:
        return None
    before = rows[index - 1]
    gap = stamps[index] - stamps[index - 1] if index < len(stamps) else 0
    if gap <= 0 or gap > MAX_STEP_MS:
        return before[1], before[2]
    after = rows[index]
    share = (when - stamps[index - 1]) / gap
    return before[1] + (after[1] - before[1]) * share, before[2] + (after[2] - before[2]) * share


def alive(row) -> bool:
    return bool(row) and len(row) > 5 and row[5] > 0


def target_of(row) -> int:
    return row[7] if row and len(row) > 7 else 0


def hole_guids(trace: Trace) -> set[int]:
    return {guid for entry in HOLE_ENTRIES for guid in guids_of_entry(trace, entry)}


def final_despawn(trace: Trace) -> int | None:
    """When whatever was left of the room's adds vanished in one scan well before the file ended: the
    kill or the Ascend despawning them, not a removal or a kill of each."""
    samples = samples_of(trace)
    end = pull_end(trace)
    entries = (NPC_BLACK_HOLE, NPC_WORM_HOLE, NPC_LIVING_CONSTELLATION, NPC_COLLAPSING_STAR,
               NPC_UNLEASHED_DARK_MATTER)
    lasts = [seen[1] for entry in entries for guid in guids_of_entry(trace, entry) if (seen := samples.seen(guid))]
    if not lasts or max(lasts) >= end - GONE_MS:
        return None
    last = max(lasts)
    ascended = any(0 <= last - rec["t"] <= EVADE_WINDOW_MS for rec in trace.of("cast") if rec.get("sp") == SPELL_ASCEND)
    return last if ascended or lasts.count(last) >= 2 else None


def holes_at(samples: Samples, holes: set[int], when: int) -> list[int]:
    return sorted(guid for guid in holes if samples.present(guid, when))


def missing_probes(trace: Trace) -> list[str]:
    """Keys the source declares for Algalon that stayed silent, plus the ones this pull's events
    call for: a key the source does not declare yet is still owed."""
    silent = {key for key, _, _ in silent_keys(emitted_keys(trace), encounter_of(trace))}
    owed = {key for key in expected_probes(trace) if key not in emitted(trace)}
    return sorted(silent | owed)


def show_banner(trace: Trace) -> None:
    end = pull_end(trace)
    outcome = trace.of("end")[-1].get("out", "?") if trace.of("end") else "no end record"
    encounter = encounter_of(trace)
    mode = DIFFICULTY.get(trace.header.get("diff"), f"difficulty {trace.header.get('diff')}")
    print(f"{encounter}  {mode}  {outcome} at {clock(end)}  {len(combat_deaths(trace))} death(s)")

    # the probe check matches keys to the encounter, so on another boss it would pass on nothing
    if encounter != ENCOUNTER:
        print("  not an Algalon pull, every section below reads empty")
        return

    gone = missing_probes(trace)
    print("all algalon.* probes present" if not gone else f"probes absent from this trace: {', '.join(gone)}")


# --------------------------------------------------------------------------------------------- phases

def fallback_phases(trace: Trace) -> list[tuple[int, int, str]]:
    """Spans for a trace without `algalon.phase`. The intro leaves him unselectable, which no record
    shows, so P1 opens on his first cast."""
    boss = boss_guid(trace)
    if boss is None:
        return []
    end = pull_end(trace)
    casts = [rec["t"] for rec in trace.of("cast") if rec.get("s") == boss]
    p1 = min(casts) if casts else 0
    low = next((when for when, row, _ in samples_of(trace).unit_steps(boss) if 0 < row[5] <= P2_HP), None)
    worm = first_seen(trace, guids_of_entry(trace, NPC_WORM_HOLE))
    p2 = min((when for when in (low, worm) if when is not None), default=None)

    marks = [(0, "intro")] if p1 > 0 else []
    marks.append((p1, "P1"))
    if p2 is not None and p2 > p1:
        marks.append((p2, "P2"))
    return [(when, marks[index + 1][0] if index + 1 < len(marks) else end, label)
            for index, (when, label) in enumerate(marks)]


def phases(trace: Trace) -> tuple[list[tuple[int, int, str]], str]:
    """`((start, stop, label), ...)` and where they came from. A phase that comes round twice gets a
    letter, so each span keeps its own deaths."""
    spans = latch_spans(trace, "algalon.phase", pull_end(trace))
    if not spans:
        return fallback_phases(trace), "fallback"
    seen: collections.Counter = collections.Counter()
    out = []
    for value, start, stop in spans:
        label = PHASE_NAMES.get(value, value)
        seen[label] += 1
        if seen[label] > 1:
            label += chr(ord("a") + seen[label] - 1)
        out.append((start, stop, label))
    return out, "algalon.phase"


def phase_start(trace: Trace, label: str) -> int | None:
    return next((start for start, _, name in phases(trace)[0] if name == label), None)


def phase_at(spans, when: int) -> str:
    for start, stop, label in spans:
        if start <= when < stop:
            return label
    if spans and when >= spans[-1][1]:
        return spans[-1][2]
    return "pre"


def show_phases(trace: Trace) -> None:
    print("PHASES")
    spans, source = phases(trace)
    if not spans:
        print("  Algalon was never sampled")
        return
    if source == "fallback":
        print("  absent from this trace: algalon.phase; P1 from his first cast, P2 from 20 % health or"
              " the first Worm Hole")

    samples = samples_of(trace)
    boss = boss_guid(trace)
    dead: dict[str, list[dict]] = collections.defaultdict(list)
    for rec in combat_deaths(trace):
        dead[phase_at(spans, rec["t"])].append(rec)

    for start, stop, label in spans:
        row = samples.row(boss, start) if boss else None
        health = f"boss {row[5]:5.1f}%" if row else "boss     -"
        rows = dead.get(label, [])
        print(f"  {label:6} {clock(start)} .. {clock(stop)}  {(stop - start) / 1000:6.1f} s  {health}"
              f"  {len(rows):2} dead")
        for rec in rows:
            blow = rec.get("blow") or [0, 0]
            print(f"           {clock(rec['t'])}  {trace.name(rec['g'])[:14]:14} {trace.role(rec['g']):6}"
                  f"  by {trace.name(rec.get('killer'))} ({blow[1] if len(blow) > 1 else 0})")
    resets = sum(1 for rec in death_records(trace) if rec.get("cause") == "reset")
    if resets:
        print(f"  plus {resets} to the wipe command")

    end = pull_end(trace)
    p1 = phase_start(trace, "P1")
    if p1 is not None:
        due = p1 + ENRAGE_MS
        print(f"  enrage due {clock(due)}, 360 s after P1 began; the pull used {(end - p1) / 1000:.1f} s"
              f" of it and ended {abs(due - end) / 1000:.1f} s {'short of' if end < due else 'past'} it")
    ascends = sorted(rec["t"] for rec in trace.of("cast") if rec.get("sp") == SPELL_ASCEND)
    for when in ascends:
        since = f", {(when - p1) / 1000:.1f} s after P1 began" if p1 is not None else ""
        print(f"  Ascend to the Heavens at {clock(when)}{since}")
    if not ascends:
        print("  no Ascend to the Heavens cast")


# ------------------------------------------------------------------------------------- his victim

def swings(trace: Trace) -> list[dict]:
    """His melee and Quantum Strike rows, in time order."""
    def build(trace: Trace) -> list[dict]:
        boss = boss_guid(trace)
        return [rec for rec in trace.of("dmg")
                if boss is not None and rec.get("s") == boss and rec.get("sp") in BOSS_SWINGS]
    return cached(trace, "_algalon_swings", build)


def taunts(trace: Trace) -> list[tuple[int, int]]:
    """`(t, caster)` per taunt a raid member cast at him. Righteous Defense goes on the friend he is
    hitting, so on a raid member it counts too."""
    def build(trace: Trace) -> list[tuple[int, int]]:
        boss = boss_guid(trace)
        roster = roster_guids(trace)
        return [(rec["t"], rec["s"]) for rec in trace.of("cast")
                if rec.get("sp") in TAUNT_SPELLS and rec.get("s") in roster
                and (rec.get("tgt") == boss or (rec.get("sp") == SPELL_RIGHTEOUS_DEFENSE and rec.get("tgt") in roster))]
    return cached(trace, "_algalon_taunts", build)


def taunt_near(trace: Trace, guid: int, when: int) -> int | None:
    """When `guid` taunted him within TAUNT_MS before `when`, or None."""
    return next((cast for cast, caster in taunts(trace)
                 if caster == guid and when - TAUNT_MS <= cast <= when + SAMPLE_LAG_MS), None)


def swap_pair(trace: Trace) -> tuple[frozenset, str]:
    """The two tanks that trade him and where that came from: `algalon.swappair`, else the roster's
    tanks."""
    def build(trace: Trace) -> tuple[frozenset, str]:
        tracks = holder_spans(trace, "algalon.swappair")
        pair = frozenset(guid for guid, spans in tracks.items() if any(value == "1" for value, _, _ in spans))
        if pair:
            return pair, "algalon.swappair"
        return frozenset(guid for guid in roster_guids(trace) if trace.role(guid) == "tank"), "roster tanks"
    return cached(trace, "_algalon_pair", build)


def intro_end(trace: Trace) -> int | None:
    """When he turned on the raid: `algalon.pull`, else P1's start, pulled in to a swing or a victim
    that came first."""
    def build(trace: Trace) -> int | None:
        pulls = notes(trace, "algalon.pull")
        boundary = pulls[0]["t"] if pulls else phase_start(trace, "P1")
        if boundary is None:
            return None
        early = [rec["t"] for rec in swings(trace) if boundary - INTRO_LEAD_MS <= rec["t"] <= boundary]
        early += [when for when, victim in boss_victims(trace)
                  if victim and boundary - INTRO_LEAD_MS <= when <= boundary]
        return min(early, default=boundary)
    return cached(trace, "_algalon_intro", build)


def victim_samples(trace: Trace) -> list[tuple[int, int]]:
    """`(t, victim)` per row of his while he lives and per swing he landed, in time order."""
    def build(trace: Trace) -> list[tuple[int, int]]:
        boss = boss_guid(trace)
        if boss is None:
            return []
        rows = [(when, target_of(row)) for when, row, _ in samples_of(trace).unit_steps(boss) if alive(row)]
        rows += [(rec["t"], rec.get("d", 0)) for rec in swings(trace)]
        return sorted(rows)
    return cached(trace, "_algalon_victims", build)


def all_phases(trace: Trace) -> dict[int, list[list[int]]]:
    return cached(trace, "_algalon_phased", phased_spans)


def hole_phases(trace: Trace) -> dict[int, list[list[int]]]:
    return cached(trace, "_algalon_hole_phased", lambda trace: phased_spans(trace, HOLE_PHASE))


def phased_during(spans, guid: int, low: int, high: int) -> bool:
    return any(start <= high and stop >= low for start, stop, _ in spans.get(guid, ()))


def died_during(trace: Trace, guid: int, low: int, high: int) -> bool:
    return any(rec.get("g") == guid and low <= rec["t"] <= high for rec in death_records(trace))


# ----------------------------------------------------------------------------------------------- pull

def parse_pull(text) -> tuple[int | None, int]:
    """`algalon.pull` reads `inreach=<n> victim=<guid>`."""
    fields = dict(part.split("=", 1) for part in str(text or "").split() if "=" in part)
    try:
        inreach = int(fields.get("inreach", ""))
    except ValueError:
        inreach = None
    return inreach, as_guid(fields.get("victim"))


def opening_frame(samples: Samples, when: int) -> dict[int, list]:
    """The snapshot at or before `when`, so whoever his first swing killed is still in it; the one
    after when nothing came before."""
    frame = samples.frame(when)
    if frame:
        return frame
    index = bisect.bisect_left(samples.stamps, when)
    return samples.units[index] if index < len(samples.units) else {}


def stage_share(trace: Trace, low: int, high: int) -> dict[int, float]:
    """Per melee bot, the share of `[low, high)` it spent on `algalon.spot=stage`."""
    spots = holder_spans(trace, "algalon.spot", pull_end(trace))
    out = {}
    for guid in roster_guids(trace):
        if trace.role(guid) != "melee" or guid in trace.humans or high <= low:
            continue
        held = sum(max(0, min(stop, high) - max(start, low)) for value, start, stop in spots.get(guid, [])
                   if value == "stage")
        out[guid] = held / (high - low)
    return out


def pull_row(trace: Trace) -> dict | None:
    """Where everyone stood when the intro ended, whom he took first, and how long the pair took to
    get him."""
    begin = intro_end(trace)
    boss = boss_guid(trace)
    if begin is None or boss is None:
        return None
    samples = samples_of(trace)
    probe = notes(trace, "algalon.pull")
    frame = opening_frame(samples, begin)
    him = frame.get(boss) or samples.row(boss, begin)
    if not him:
        return None
    pair, _ = swap_pair(trace)
    roster = roster_guids(trace)
    near = sorted((dist2(row[1:3], him[1:3]), guid) for guid, row in frame.items() if guid in roster and alive(row))
    inreach, victim = parse_pull(probe[0].get("txt")) if probe else (None, 0)
    after = [(when, guid) for when, guid in victim_samples(trace) if when >= begin and guid]
    intro = next(((start, stop) for start, stop, label in phases(trace)[0] if label == "intro"), None)
    return {
        "t": begin,
        "probe": bool(probe),
        "spot": tuple(him[1:3]),
        "near": near,
        "inside": [guid for gap, guid in near if gap <= MELEE_REACH and guid not in pair],
        "inreach": inreach,
        "victim": victim or (after[0][1] if after else 0),
        "swings": [rec for rec in swings(trace)
                   if rec["t"] >= begin - SAMPLE_LAG_MS and rec.get("sp") == 0][:FIRST_SWINGS],
        "tanked": next(((when, guid) for when, guid in after if guid in pair), None),
        "deaths": [rec for rec in combat_deaths(trace) if begin - SAMPLE_LAG_MS <= rec["t"] <= begin + OPENING_MS],
        "stage": stage_share(trace, intro[0], begin) if intro else {},
    }


def show_pull(trace: Trace) -> None:
    print("PULL")
    row = pull_row(trace)
    if row is None:
        print("  the intro never ended, or Algalon was never sampled")
        return
    pair, source = swap_pair(trace)
    print(f"  intro ended {clock(row['t'])}{'' if row['probe'] else ' (absent from this trace: algalon.pull)'};"
          f" Algalon at ({row['spot'][0]:.1f}, {row['spot'][1]:.1f}); swap pair from {source}")
    said = f", algalon.pull said {row['inreach']}" if row["inreach"] is not None else ""
    print(f"  inside {MELEE_REACH:.2f} yd outside the pair: {len(row['inside'])}{said}"
          + (": " + ", ".join(who(trace, guid) for guid in row["inside"]) if row["inside"] else ""))
    print("  by distance: " + ", ".join(
        f"{trace.name(guid)}{'*' if guid in pair else '!' if guid in row['inside'] else ''} {gap:.1f}"
        for gap, guid in row["near"]))
    print("    * swap pair, ! in his reach")
    print(f"  first victim: {who(trace, row['victim']) if row['victim'] else '-'}")
    print("  first swings: " + (", ".join(f"{clock(rec['t'])} {trace.name(rec.get('d'))} {rec.get('a', 0):,}"
                                          f"{' (lethal)' if rec.get('ok', 0) > 0 else ''}" for rec in row["swings"])
                                or "none"))
    tanked = row["tanked"]
    print(f"  a pair tank had him {(tanked[0] - row['t']) / 1000:.1f} s after the intro ended ({trace.name(tanked[1])})"
          if tanked else "  no pair tank had him after the intro")
    print(f"  deaths in the first {OPENING_MS // 1000} s: " + (", ".join(
        f"{clock(rec['t'])} {who(trace, rec['g'])}" for rec in row["deaths"]) or "none"))
    staged = [(share, guid) for guid, share in row["stage"].items()]
    if not any(value == "stage" for spans in holder_spans(trace, "algalon.spot").values() for value, _, _ in spans):
        print("  absent from this trace: algalon.spot=stage")
    elif staged:
        print(f"  melee bots on algalon.spot=stage before the intro ended:"
              f" {sum(share for share, _ in staged) * 100.0 / len(staged):.0f}% of their intro time ("
              + ", ".join(f"{trace.name(guid)} {share * 100.0:.0f}%" for share, guid in sorted(staged, reverse=True))
              + ")")


# ---------------------------------------------------------------------------------------------- loose

def victim_runs(trace: Trace, pair) -> list[list]:
    """`[start, stop, kind, victims]` per run of his victim after the intro, kind `pair`, `loose` or
    `none`. A short `none` beside a loose run joins it: he drops one victim before he picks the next."""
    begin = intro_end(trace)
    if begin is None:
        return []
    rows = [(when, victim) for when, victim in victim_samples(trace) if when >= begin]
    runs: list[list] = []
    for index, (when, victim) in enumerate(rows):
        stop = rows[index + 1][0] if index + 1 < len(rows) else when
        kind = "none" if not victim else "pair" if victim in pair else "loose"
        if runs and runs[-1][2] == kind:
            runs[-1][1] = stop
        else:
            runs.append([when, stop, kind, []])
        if victim and victim not in runs[-1][3]:
            runs[-1][3].append(victim)
    for index in range(1, len(runs) - 1):
        start, stop, kind, _ = runs[index]
        if kind == "none" and stop - start <= BRIDGE_MS and "loose" in (runs[index - 1][2], runs[index + 1][2]):
            runs[index][2] = "loose"
    return runs


def loose_cause(trace: Trace, pair, start: int, holder: int, first: int) -> tuple[str, str]:
    """Why he left the pair, read at the window's start."""
    intro = intro_end(trace)
    if intro is not None and start - intro <= INTRO_CAUSE_MS:
        return "intro end", ""
    low, high = start - CAUSE_MS, start + SAMPLE_LAG_MS
    punched = next((guid for when, guid in punched_out(trace) if guid in pair and low <= when <= high), 0)
    if punched:
        return "5th stack", trace.name(punched)
    if holder and died_during(trace, holder, low, high):
        return "holder died", trace.name(holder)
    if holder and phased_during(hole_phases(trace), holder, start, high):
        return "holder phased", trace.name(holder)
    if first and taunt_near(trace, first, start) is not None:
        return "other", f"{trace.name(first)} taunted"
    if holder:
        return "threat", f"off {trace.name(holder)}"
    return "other", ""


def loose_row(trace: Trace, pair, start: int, stop: int, victims: list[int], hits: list[dict]) -> dict:
    samples = victim_samples(trace)
    holder = next((victim for when, victim in reversed(samples) if when < start and victim in pair), 0)
    ender = next((victim for when, victim in samples if when >= stop), 0)
    ender = ender if ender in pair else 0
    cause, detail = loose_cause(trace, pair, start, holder, victims[0] if victims else 0)
    return {
        "start": start, "stop": stop, "victims": victims, "holder": holder, "cause": cause, "detail": detail,
        "hits": hits,
        "deaths": [rec for rec in combat_deaths(trace) if start <= rec["t"] <= stop + SAMPLE_LAG_MS],
        "ender": ender,
        "taunt": taunt_near(trace, ender, stop) if ender else None,
    }


def loose_windows(trace: Trace) -> list[dict]:
    """Every stretch after the intro he spent on someone outside the swap pair. One shorter than
    LOOSE_MIN_MS counts only if he landed a swing in it, since the target column flickers."""
    def build(trace: Trace) -> list[dict]:
        pair, _ = swap_pair(trace)
        merged: list[list] = []
        for start, stop, kind, victims in victim_runs(trace, pair):
            if kind != "loose":
                continue
            if merged and merged[-1][1] == start:
                merged[-1][1] = stop
                merged[-1][2] += [victim for victim in victims if victim not in merged[-1][2]]
            else:
                merged.append([start, stop, list(victims)])
        out = []
        for start, stop, victims in merged:
            hits = [rec for rec in swings(trace) if start <= rec["t"] <= stop and rec.get("d") not in pair]
            if stop - start >= LOOSE_MIN_MS or hits:
                out.append(loose_row(trace, pair, start, stop, victims, hits))
        return out
    return cached(trace, "_algalon_loose", build)


def window_cause(window: dict) -> str:
    return window["cause"] + (f", {window['detail']}" if window["detail"] else "")


def show_loose(trace: Trace) -> None:
    print("ALGALON LOOSE")
    if boss_guid(trace) is None or intro_end(trace) is None:
        print("  the intro never ended, or Algalon was never sampled")
        return
    pair, source = swap_pair(trace)
    print(f"  swap pair from {source}: {', '.join(who(trace, guid) for guid in sorted(pair, key=trace.name))}")
    windows = loose_windows(trace)
    boss = boss_guid(trace)
    for window in windows:
        span = window["stop"] - window["start"]
        print(f"  {clock(window['start'])} .. {clock(window['stop'])}  {span / 1000:5.1f} s  {window_cause(window)}")
        dealt: dict[int, list[int]] = collections.defaultdict(list)
        for rec in window["hits"]:
            dealt[rec.get("d", 0)].append(rec.get("a", 0))
        print(f"      on {', '.join(trace.name(guid) for guid in window['victims']) or '-'};"
              f" hit {', '.join(f'{trace.name(g)} {sum(a):,}' for g, a in dealt.items()) or 'nobody'}")
        if window["deaths"]:
            print("      died: " + ", ".join(trace.name(rec["g"]) + ("" if rec.get("killer") == boss else " (not his)")
                                         for rec in window["deaths"]))
        ended = (f"{trace.name(window['ender'])} took him, {'taunt' if window['taunt'] is not None else 'no taunt'}"
                 if window["ender"] else "no pair tank took him")
        print(f"      ended: {ended}")
    total = sum(window["stop"] - window["start"] for window in windows)
    deaths = [rec for window in windows for rec in window["deaths"]]
    print(f"  {len(windows)} window(s), {total / 1000:.1f} s loose, {len(deaths)} death(s) while loose,"
          f" {sum(1 for rec in deaths if rec.get('killer') == boss)} of them his")


# ---------------------------------------------------------------------------------------------- swaps

def swap_due(trace: Trace, pair, guid: int, when: int) -> dict:
    """`taken` is when the other pair tank became his victim; `missed` names what came first instead."""
    taken = next(((at, victim) for at, victim in victim_samples(trace)
                  if at >= when and victim in pair and victim != guid), None)
    ends = [(at, "5th stack") for at, holder in punched_out(trace) if holder == guid and at >= when]
    ends += [(rec["t"], "died") for rec in death_records(trace) if rec.get("g") == guid and rec["t"] >= when]
    first = min(ends + [(pull_end(trace), "pull ended")])
    if taken and taken[0] <= first[0]:
        return {"t": when, "tank": guid, "taken": taken[0], "by": taken[1], "missed": None}
    return {"t": when, "tank": guid, "taken": None, "by": 0, "missed": first}


def swap_dues(trace: Trace) -> list[dict]:
    """Each time a pair tank's Phase Punch reached SWAP_STACKS."""
    pair, _ = swap_pair(trace)
    out = []
    for guid, rows in phase_punch(trace).items():
        if guid not in pair:
            continue
        before = 0
        for when, count, removed in rows:
            count = 0 if removed else count
            if count >= SWAP_STACKS > before:
                out.append(swap_due(trace, pair, guid, when))
            before = count
    return sorted(out, key=lambda due: due["t"])


def threat_steals(trace: Trace) -> list[tuple[int, int, int]]:
    """`(t, from, to)` each time he went straight from one pair tank to the other with no taunt from
    the new one while the old one was still up and unphased."""
    pair, _ = swap_pair(trace)
    phased = all_phases(trace)
    out: list[tuple[int, int, int]] = []
    held = 0
    for when, victim in victim_samples(trace):
        if not victim:
            continue
        if victim not in pair:
            held = 0
            continue
        if (held and victim != held and taunt_near(trace, victim, when) is None
                and not died_during(trace, held, when - CAUSE_MS, when + SAMPLE_LAG_MS)
                and not phased_during(phased, held, when - CAUSE_MS, when)):
            out.append((when, held, victim))
        held = victim
    return out


def spot_distance(trace: Trace, guid: int, value: str, point) -> float | None:
    """Median distance to `point` while the bot's `algalon.spot` read `value`."""
    if point is None:
        return None
    samples = samples_of(trace)
    spans = [(start, stop) for held, start, stop in holder_spans(trace, "algalon.spot", pull_end(trace)).get(guid, [])
             if held == value]
    gaps = [dist2(row[1:3], point) for start, stop in spans for row in samples.between(guid, start, stop - 1)
            if alive(row)]
    return statistics.median(gaps) if gaps else None


def show_swaps(trace: Trace) -> None:
    print("TANK SWAPS")
    if boss_guid(trace) is None:
        print("  Algalon was never sampled")
        return
    show_absent(trace, "algalon.swappair", "algalon.tankrole", "algalon.call")
    pair, source = swap_pair(trace)
    end = pull_end(trace)
    stacks = phase_punch(trace)
    print(f"  pair from {source}:")
    for guid in sorted(pair, key=trace.name):
        steps = [f"{clock(when)} {'off' if removed else count}" for when, count, removed in stacks.get(guid, [])]
        cast = sum(1 for _, caster in taunts(trace) if caster == guid)
        print(f"    {who(trace, guid)}{' human' if guid in trace.humans else ''}, {cast} taunt(s) at him;"
              f" Phase Punch {', '.join(steps[:TIMELINE_SHOWN]) or 'never'}")
    for due in swap_dues(trace):
        line = f"  swap due {clock(due['t'])}: {trace.name(due['tank'])} at {SWAP_STACKS} stacks, "
        if due["taken"] is not None:
            line += f"{trace.name(due['by'])} had him {(due['taken'] - due['t']) / 1000:.1f} s later"
        else:
            line += f"missed: {due['missed'][1]} at {clock(due['missed'][0])}"
        print(line)
    for when, guid in punched_out(trace):
        if guid in pair:
            print(f"  missed swap: {trace.name(guid)} phased out by the 5th stack at {clock(when)}")
    calls = notes(trace, "algalon.call")
    if calls:
        print("  algalon.call: " + ", ".join(f"{clock(rec['t'])} {rec.get('txt')} ({trace.name(rec.get('g'))})"
                                            for rec in calls))
    roles = holder_spans(trace, "algalon.tankrole", end)
    for guid, spans in sorted(roles.items(), key=lambda item: trace.name(item[0])):
        totals = span_totals({guid: spans})
        print(f"  algalon.tankrole {trace.name(guid)}: "
              + ", ".join(f"{value} {held / 1000:.1f} s" for value, held in totals.most_common())
              + "; " + ", ".join(f"{clock(start)} {value}" for value, start, _ in spans[:TIMELINE_SHOWN]))
    for guid in sorted(pair, key=trace.name):
        for value, point in (("tank", TANK_SLOT), ("offtank", OFFTANK_SLOT)):
            gap = spot_distance(trace, guid, value, point)
            if gap is not None:
                print(f"  {trace.name(guid)} on algalon.spot={value}: {gap:.1f} yd median from its slot")
    steals = threat_steals(trace)
    for when, old, new in steals:
        print(f"  threat steal {clock(when)}: {trace.name(new)} pulled him off {trace.name(old)} with no taunt")
    if not steals:
        print("  no threat steal between the pair")


# ---------------------------------------------------------------------------------------------- reach

def reach_rows(trace: Trace) -> dict[int, list[int]]:
    """Per raid member outside the pair, `[held, loose]` ms alive and unphased within MELEE_REACH of
    him after the intro, split by whether he was inside a loose window."""
    samples = samples_of(trace)
    boss = boss_guid(trace)
    begin = intro_end(trace)
    if boss is None or begin is None:
        return {}
    pair, _ = swap_pair(trace)
    phased = all_phases(trace)
    windows = [(window["start"], window["stop"]) for window in loose_windows(trace)]
    members = [guid for guid in roster_guids(trace) if guid not in pair]
    out: dict[int, list[int]] = collections.defaultdict(lambda: [0, 0])
    for _, when, weight in samples.steps(begin, pull_end(trace)):
        him = samples.row(boss, when)
        if not alive(him):
            continue
        loose = any(start <= when < stop for start, stop in windows)
        for guid in members:
            row = samples.row(guid, when)
            if alive(row) and not phased_at(phased, guid, when) and dist2(row[1:3], him[1:3]) <= MELEE_REACH:
                out[guid][1 if loose else 0] += weight
    return dict(out)


def show_reach(trace: Trace) -> None:
    print("IN HIS REACH")
    if boss_guid(trace) is None or intro_end(trace) is None:
        print("  the intro never ended, or Algalon was never sampled")
        return
    rows = reach_rows(trace)
    if not rows:
        print(f"  nobody outside the swap pair stood within {MELEE_REACH:.2f} yd of him after the intro")
        return
    print(f"  alive and unphased within {MELEE_REACH:.2f} yd of him after the intro, the swap pair left out:")
    for guid, (held, loose) in sorted(rows.items(), key=lambda item: -sum(item[1])):
        print(f"    {who(trace, guid):28} {held / 1000:6.1f} s while the pair held him, {loose / 1000:5.1f} s loose")
    print(f"  total {sum(held for held, _ in rows.values()) / 1000:.1f} s held,"
          f" {sum(loose for _, loose in rows.values()) / 1000:.1f} s loose")


# -------------------------------------------------------------------------------------------- bigbang

def big_bang_casts(trace: Trace) -> list[dict]:
    """`{n, start, end, src}` per Big Bang. Cast rows first; without them the `algalon.bigbang` latch,
    then the damage, which cannot see a Big Bang that hit nobody."""
    starts: list[tuple[int, int, str]] = []
    for rec in trace.of("cast"):
        if rec.get("sp") in SPELL_BIG_BANG:
            if starts and rec["t"] - starts[-1][0] < BIG_BANG_CAST_MS:
                continue
            starts.append((rec["t"], rec.get("ct") or BIG_BANG_CAST_MS, "cast"))
    if not starts:
        for value, start, _ in latch_spans(trace, "algalon.bigbang"):
            if as_guid(value) > 0:
                starts.append((start, BIG_BANG_CAST_MS, "algalon.bigbang"))
    if not starts:
        for when in sorted(rec["t"] for rec in trace.of("dmg") if rec.get("sp") in SPELL_BIG_BANG):
            if not starts or when - (starts[-1][0] + BIG_BANG_CAST_MS) > HIT_SLACK_MS:
                starts.append((when - BIG_BANG_CAST_MS, BIG_BANG_CAST_MS, "damage"))
    return [{"n": index + 1, "start": start, "end": start + cast, "src": src}
            for index, (start, cast, src) in enumerate(starts)]


def phased_spans(trace: Trace, spells=PHASE_AURAS, chain_ms: int = CHAIN_MS) -> dict[int, list[list[int]]]:
    """Per player, `[start, stop, applications]` per stretch it wore any of `spells`. A row while the
    aura is held is the field's once-a-second refresh, never a new application; only an application
    within `chain_ms` of a removal adds one. A stretch still open when the file ends runs to the end."""
    end = pull_end(trace)
    held: dict[int, set[int]] = collections.defaultdict(set)
    out: dict[int, list[list[int]]] = collections.defaultdict(list)
    for rec in trace.of("aura"):
        spell = rec.get("sp")
        if spell not in spells:
            continue
        guid, when = rec.get("d", 0), rec["t"]
        spans, mine = out[guid], held[guid]
        if rec.get("r"):
            if spell in mine:
                mine.discard(spell)
                if not mine and spans:
                    spans[-1][1] = when
            continue
        if not mine:
            if spans and spans[-1][1] is not None and when - spans[-1][1] <= chain_ms:
                spans[-1][1] = None
                spans[-1][2] += 1
            else:
                spans.append([when, None, 1])
        mine.add(spell)
    for spans in out.values():
        for span in spans:
            if span[1] is None:
                span[1] = end
    return out


def phased_at(spans, guid: int, when: int) -> bool:
    return any(start <= when < stop for start, stop, _ in spans.get(guid, ()))


def rephase_loops(trace: Trace) -> list[dict]:
    """Hole phasing that went round again: an application chained onto a removal, or one stretch past
    the aura's own 10 s, which only a bot standing in the field keeps up. Phase Punch's 64417 is left
    out; it is not a hole."""
    out = []
    for guid, spans in phased_spans(trace, HOLE_PHASE).items():
        for start, stop, applications in spans:
            if applications >= 2 or stop - start > LOOP_MS:
                out.append({"guid": guid, "start": start, "stop": stop, "applications": applications})
    return sorted(out, key=lambda loop: loop["start"])


def healer_return(phased, healers, victims, stop: int, end: int) -> int | None:
    """When the first healer was back beside the soaker: the impact for one that never hid, else the
    end of the phase it wore at the impact. A phase still on when the file ends returned nobody."""
    back = []
    for guid in healers:
        if guid in victims:
            back.append(stop)
            continue
        back.extend(until for start, until, _ in phased.get(guid, ()) if start <= stop < until < end)
    return min(back) if back else None


def nobody_near(trace: Trace, samples: Samples, phased, boss: int | None, low: int, high: int) -> int | None:
    """The first sample in `[low, high)` with nobody alive and unphased within reset range of him."""
    if boss is None:
        return None
    roster = roster_guids(trace)
    for _, when, _ in samples.steps(low, high):
        him = samples.row(boss, when)
        if not him:
            continue
        if not any(alive(row) and not phased_at(phased, guid, when) and dist2(row[1:3], him[1:3]) <= RESET_RANGE
                   for guid, row in ((guid, samples.row(guid, when)) for guid in roster)):
            return when
    return None


def big_bang_rows(trace: Trace) -> list[dict]:
    samples = samples_of(trace)
    roster = roster_guids(trace)
    boss = boss_guid(trace)
    end = pull_end(trace)
    holes = hole_guids(trace)
    soakers = latch_spans(trace, "algalon.soaker", end)
    backups = latch_spans(trace, "algalon.backup", end)
    hole_count = latch_spans(trace, "algalon.holes", end)
    hide = holder_spans(trace, "algalon.hide", end)
    phased = phased_spans(trace)
    loops = rephase_loops(trace)
    hits = [rec for rec in trace.of("dmg") if rec.get("sp") in SPELL_BIG_BANG]
    deaths = death_records(trace)
    defensives = notes(trace, "algalon.defensive")
    externals = [rec for rec in trace.of("cast") if rec.get("sp") in SOAK_SPELLS]
    matter = guids_of_entry(trace, NPC_DARK_MATTER)
    matter_hits = [rec for rec in trace.of("dmg") if rec.get("s") in matter]
    ascends = [rec["t"] for rec in trace.of("cast") if rec.get("sp") == SPELL_ASCEND]
    finish = trace.of("end")[-1] if trace.of("end") else None
    healers = {guid for guid in roster if trace.role(guid) == "heal"}

    casts = big_bang_casts(trace)
    out = []
    for index, cast in enumerate(casts):
        start, stop = cast["start"], cast["end"]
        following = casts[index + 1]["start"] if index + 1 < len(casts) else end + 1
        after = min(stop + HIT_SLACK_MS, following)
        tail = stop + PHASE_AURA_MS + CHAIN_MS

        soaker, soaker_from = last_set_in(soakers, start, stop), "algalon.soaker"
        if not soakers:
            held = target_of(samples.row(boss, stop - 1)) if boss else 0
            soaker, soaker_from = (held, "his victim") if trace.role(held) == "tank" else (0, "-")
        backup = last_set_in(backups, start, stop)
        at_impact = {guid: value_at(spans, stop - 1) for guid, spans in hide.items()}
        holding = {guid for guid, value in at_impact.items() if value == "hold"}
        sanctioned = ({soaker, backup} | holding) - {0}

        # Per bot that was on `wait` inside the cast: ms left before the impact when it set off, or
        # None if it was still waiting at the impact.
        waited: dict[int, int | None] = {}
        for guid, spans in hide.items():
            for value, begin, until in spans:
                if value == "wait" and begin < stop and until > start:
                    waited[guid] = None if until >= stop else stop - until

        victims: dict[int, dict] = {}
        for rec in hits:
            if start <= rec["t"] <= after:
                victims.setdefault(rec.get("d"), rec)
        killed = {guid for guid, rec in victims.items()
                  if rec.get("ok", 0) > 0
                  or any(d.get("g") == guid and rec["t"] <= d["t"] <= rec["t"] + CHAIN_MS for d in deaths)}

        back = healer_return(phased, healers, victims, stop, end)
        until = back if back is not None else stop + PHASE_AURA_MS
        soaker_death = next((d["t"] for d in deaths if soaker and d.get("g") == soaker and start <= d["t"] <= until),
                            None)
        before = samples.row(soaker, start) if soaker else None
        later = samples.row(soaker, stop + 1000) if soaker else None

        ended = None
        if finish and stop <= finish["t"] <= stop + EVADE_WINDOW_MS and finish.get("out") in ("reset", "wipe"):
            ended = (finish["t"], finish["out"])

        taken: collections.Counter = collections.Counter()
        for rec in matter_hits:
            if start <= rec["t"] <= tail:
                taken[rec.get("d")] += rec.get("a", 0)

        watched = sanctioned | {soaker}
        out.append({
            **cast,
            "holes": holes_at(samples, holes, start),
            "holes_probe": value_at(hole_count, start),
            "soaker": soaker,
            "soaker_from": soaker_from,
            "elected": bool(soakers),
            "soaker_phased": bool(soaker) and phased_at(phased, soaker, stop),
            "backup": backup,
            "holding": holding,
            "waited": waited,
            "at_impact": at_impact,
            "soaker_hp": (before[5] if before else None, later[5] if later else None),
            "soaker_death": soaker_death,
            "defensives": [(rec["t"], rec.get("g"), str(rec.get("txt", ""))) for rec in defensives
                           if start - DEFENSIVE_LEAD_MS <= rec["t"] <= stop],
            "externals": [(rec["t"], rec.get("s"), rec.get("sp"), rec.get("tgt")) for rec in externals
                          if start - DEFENSIVE_LEAD_MS <= rec["t"] <= stop
                          and (rec.get("s") in watched or rec.get("tgt") in watched)],
            "victims": victims,
            "killed": killed,
            "stray": sorted((guid for guid in victims if guid not in sanctioned), key=trace.name),
            "back": back,
            "nobody": nobody_near(trace, samples, phased, boss, stop, stop + PHASE_AURA_MS),
            "ended": ended,
            "ascend": next((when for when in ascends if stop - 500 <= when <= stop + EVADE_WINDOW_MS), None),
            "loops": [loop for loop in loops if loop["start"] < tail and loop["stop"] > start],
            "matter": taken,
            "hide": collections.Counter(value for value in at_impact.values() if value is not None),
        })
    return out


def show_bigbang(trace: Trace) -> None:
    print("BIG BANG")
    rows = big_bang_rows(trace)
    if not rows:
        print("  no Big Bang was cast")
        return
    show_absent(trace, "algalon.bigbang", "algalon.soaker", "algalon.backup", "algalon.hide", "algalon.defensive")

    for row in rows:
        src = "" if row["src"] == "cast" else f"  (from {row['src']})"
        probe = f", algalon.holes {row['holes_probe']}" if row["holes_probe"] is not None else ""
        print(f"  #{row['n']}  {clock(row['start'])} .. {clock(row['end'])}{src}"
              f"  holes at the cast: {len(row['holes'])}{probe}")

        if row["soaker"]:
            first, then = row["soaker_hp"]
            health = f"{first:.0f}% -> {then:.0f}%" if first is not None and then is not None else "health -"
            died = f", died {clock(row['soaker_death'])}" if row["soaker_death"] is not None else ""
            print(f"      soaker {who(trace, row['soaker'])} from {row['soaker_from']}, {health}{died}")
        else:
            print("      soaker unknown")
        print(f"      backup {who(trace, row['backup']) if row['backup'] else '-'}")

        saves = [f"{clock(when)} {text} ({trace.name(guid)})" for when, guid, text in row["defensives"]]
        saves += [f"{clock(when)} {trace.name(caster)} {trace.spell(spell)} -> {trace.name(target)}"
                  for when, caster, spell, target in row["externals"]]
        print(f"      defensives: {'; '.join(saves) if saves else 'none'}")

        hit = []
        for guid, rec in sorted(row["victims"].items(), key=lambda item: trace.name(item[0])):
            tag = ("soaker" if guid == row["soaker"] else "backup" if guid == row["backup"]
                   else "hold" if guid in row["holding"] else trace.role(guid))
            hit.append(f"{trace.name(guid)} ({tag}) {rec.get('a', 0):,}{' DIED' if guid in row['killed'] else ''}")
        print(f"      hit unphased: {', '.join(hit) if hit else 'nobody'}")
        if row["stray"]:
            print(f"      {len(row['stray'])} non-soaker(s) hit: " + ", ".join(
                f"{trace.name(g)} (hide {row['at_impact'].get(g) or '-'})" for g in row["stray"]))
        if row["waited"]:
            left = sorted((ms, trace.name(g)) for g, ms in row["waited"].items() if ms is not None)
            still = sorted(trace.name(g) for g, ms in row["waited"].items() if ms is None)
            latest = f", the latest {left[0][1]} {left[0][0] / 1000:.1f} s before the impact" if left else ""
            print(f"      left the wait: {len(left)}{latest}; still waiting at the impact:"
                  f" {', '.join(still) or 'nobody'}")

        if row["soaker_phased"]:
            alone = "soaker phased at the impact"
        else:
            alone = "tank alone " + (f"{(row['back'] - row['end']) / 1000:.1f} s" if row["back"] is not None else "-")
        nobody = (f"nobody alive and unphased within {RESET_RANGE:.0f} yd from {clock(row['nobody'])}"
                  if row["nobody"] is not None else "someone alive and unphased in range throughout")
        print(f"      {alone}; {nobody}")
        for loop in row["loops"]:
            print(f"      re-phase loop: {trace.name(loop['guid'])} {applications(loop['applications'])},"
                  f" {(loop['stop'] - loop['start']) / 1000:.1f} s phased from {clock(loop['start'])}")
        matter = [(guid, amount) for guid, amount in row["matter"].most_common() if amount]
        if matter:
            print("      Dark Matter on phased players: " + ", ".join(f"{trace.name(g)} {a:,}" for g, a in matter))
        if row["hide"]:
            print("      algalon.hide at the impact: " + ", ".join(f"{v} {n}" for v, n in row["hide"].most_common()))
        after = []
        if row["ascend"] is not None:
            after.append(f"Ascend {(row['ascend'] - row['end']) / 1000:+.1f} s")
        if row["ended"]:
            after.append(f"pull ended {row['ended'][1]} {(row['ended'][0] - row['end']) / 1000:+.1f} s")
        print(f"      after: {', '.join(after) if after else 'the pull went on'}")


# -------------------------------------------------------------------------------------------- phasing

def moves_of(trace: Trace) -> dict[int, list[dict]]:
    """Accepted `move` rows by bot, in time order."""
    def build(trace: Trace) -> dict[int, list[dict]]:
        out: dict[int, list[dict]] = collections.defaultdict(list)
        for rec in trace.of("move"):
            if rec.get("ok"):
                out[rec.get("g", 0)].append(rec)
        return out
    return cached(trace, "_algalon_moves", build)


def victim_hits(trace: Trace) -> dict[int, list[dict]]:
    """Every `dmg` row by victim, in time order."""
    def build(trace: Trace) -> dict[int, list[dict]]:
        out: dict[int, list[dict]] = collections.defaultdict(list)
        for rec in trace.of("dmg"):
            out[rec.get("d", 0)].append(rec)
        return out
    return cached(trace, "_algalon_victim_hits", build)


def unphased_at(trace: Trace) -> list[tuple[int, int]]:
    """`(t, guid)` per 64445 Big Bang put on a raid member."""
    return cached(trace, "_algalon_unphase", lambda trace: [
        (rec["t"], rec.get("d", 0)) for rec in trace.of("aura")
        if rec.get("sp") == SPELL_BIG_BANG_UNPHASE and not rec.get("r")])


def stray_cause(trace: Trace, guid: int, start: int) -> str:
    """What put a raid member into a hole outside a Big Bang: Big Bang's 64445 stripping it while it
    still stood in the field, a hole spawning on it, or a walk."""
    if any(target == guid and start - REPHASE_MS <= when <= start for when, target in unphased_at(trace)):
        return "re-phase"
    samples = samples_of(trace)
    spot = position_at(samples, guid, start)
    for hole in hole_guids(trace):
        seen = samples.seen(hole)
        if (spot and seen and abs(seen[0] - start) <= NEW_HOLE_MS
                and dist2(samples.row(hole, seen[0])[1:3], spot) <= NEW_HOLE_YD):
            return "new hole"
    walk = next((rec for rec in reversed(moves_of(trace).get(guid, [])) if rec["t"] <= start), None)
    return f"walked in ({walk.get('by') or '?'})" if walk else "walked in"


def phasing_rows(trace: Trace) -> list[dict]:
    """Every hole phase on a raid member, refreshes merged and each application its own row. `Big Bang`
    when it began inside a Big Bang cast, else `stray`."""
    bangs = [(cast["start"] - BANG_LEAD_MS, cast["end"]) for cast in big_bang_casts(trace)]
    matter = guids_of_entry(trace, NPC_DARK_MATTER)
    roster = roster_guids(trace)
    out = []
    for guid, stretches in phased_spans(trace, HOLE_PHASE, chain_ms=0).items():
        if guid not in roster:
            continue
        for start, stop, _ in stretches:
            bang = any(low <= start <= high for low, high in bangs)
            out.append({
                "guid": guid, "start": start, "stop": stop,
                "label": "Big Bang" if bang else "stray",
                "cause": "" if bang else stray_cause(trace, guid, start),
                "matter": sum(rec.get("a", 0) for rec in victim_hits(trace).get(guid, [])
                              if rec.get("s") in matter and start <= rec["t"] <= stop),
                "died": died_during(trace, guid, start, stop + SAMPLE_LAG_MS),
            })
    return sorted(out, key=lambda row: (row["start"], trace.name(row["guid"])))


def show_phasing(trace: Trace) -> None:
    print("PHASING")
    rows = phasing_rows(trace)
    if not rows:
        print("  nobody in the raid was phased by a hole")
        return
    stray = [row for row in rows if row["label"] == "stray"]
    causes = collections.Counter(row["cause"].split(" (")[0] for row in stray)
    print(f"  {len(rows)} hole phase(s): {len(rows) - len(stray)} in a Big Bang, {len(stray)} stray"
          + (" (" + ", ".join(f"{cause} {n}" for cause, n in causes.most_common()) + ")" if stray else ""))
    print(f"  {'at':>9}  {'who':28} {'label':8} {'phased':>7} {'dark matter':>11}  {'died':4}  cause")
    for row in rows:
        print(f"  {clock(row['start']):>9}  {who(trace, row['guid']):28} {row['label']:8}"
              f" {(row['stop'] - row['start']) / 1000:6.1f}s {row['matter']:11,}  {'yes' if row['died'] else '-':4}"
              f"  {row['cause'] or '-'}")


# ---------------------------------------------------------------------------------------------- stars

def star_deaths(trace: Trace) -> list[dict]:
    """`{t, star, how, hits, damage, gap, raid_min}` per star death, in order. `how` is `explosion` for
    a Black Hole Explosion burst, `hole` for a star that left no burst but a hole on its last spot."""
    samples = samples_of(trace)
    end = pull_end(trace)
    roster = roster_guids(trace)
    stars = guids_of_entry(trace, NPC_COLLAPSING_STAR)
    black = guids_of_entry(trace, NPC_BLACK_HOLE)

    bursts: list[dict] = []
    latest: dict = {}
    for rec in sorted((rec for rec in trace.of("dmg") if rec.get("sp") in SPELL_BLACK_HOLE_EXPLOSION),
                      key=lambda rec: rec["t"]):
        source = rec.get("s") if rec.get("s") in stars else None
        burst = latest.get(source)
        if burst and rec["t"] - burst["last"] <= BURST_GAP_MS:
            burst["hits"] += 1
            burst["damage"] += rec.get("a", 0)
            burst["last"] = rec["t"]
            continue
        burst = {"t": rec["t"], "last": rec["t"], "star": source, "how": "explosion", "hits": 1,
                 "damage": rec.get("a", 0)}
        bursts.append(burst)
        latest[source] = burst

    named = {burst["star"] for burst in bursts}
    for star in sorted(stars - named):
        seen = samples.seen(star)
        if not seen or seen[1] >= end - GONE_MS:
            continue
        gone = seen[1]
        spot = samples.row(star, gone)[1:3]
        blind = next((burst for burst in bursts if burst["star"] is None and abs(burst["t"] - gone) <= HOLE_MATCH_MS),
                     None)
        if blind:
            blind["star"] = star
            continue
        hole = False
        for guid in black:
            seen_hole = samples.seen(guid)
            if seen_hole and gone - GONE_MS <= seen_hole[0] <= gone + HOLE_MATCH_MS:
                hole = hole or dist2(samples.row(guid, seen_hole[0])[1:3], spot) <= HOLE_MATCH_YD
        if hole:
            bursts.append({"t": gone, "last": gone, "star": star, "how": "hole", "hits": 0, "damage": 0})

    bursts.sort(key=lambda burst: burst["t"])
    previous = None
    for burst in bursts:
        frame = samples.frame(burst["t"] - 1)
        health = [row[5] for guid, row in frame.items() if guid in roster and alive(row)]
        burst["raid_min"] = min(health) if health else None
        burst["gap"] = burst["t"] - previous if previous is not None else None
        previous = burst["t"]
    return bursts


def span_totals(tracks) -> collections.Counter:
    """Milliseconds each value was held, summed over every holder of a per-bot key."""
    held: collections.Counter = collections.Counter()
    for spans in tracks.values():
        for value, start, stop in spans:
            held[value] += stop - start
    return held


def star_team(trace: Trace) -> dict:
    """Who was on the star team, the share of each member's window-open time spent on the focus star,
    and damage dealers off the team who spent time on any star. `drafted` is the ranged dps counted
    in while `algalon.urgent` read 1."""
    samples = samples_of(trace)
    end = pull_end(trace)
    roster = roster_guids(trace)
    stars = guids_of_entry(trace, NPC_COLLAPSING_STAR)
    team = holder_spans(trace, "algalon.starteam", end)
    focus = latch_spans(trace, "algalon.focusstar", end)
    urgent = latch_spans(trace, "algalon.urgent", end)
    members = sorted((guid for guid, spans in team.items() if any(value == "1" for value, _, _ in spans)),
                     key=trace.name)

    on: collections.Counter = collections.Counter()
    open_ms: collections.Counter = collections.Counter()
    off: collections.Counter = collections.Counter()
    off_from: dict[int, int] = {}
    drafted: set[int] = set()
    if stars:
        for _, when, weight in samples.steps(0, end):
            star = as_guid(value_at(focus, when))
            everyone_ranged = value_at(urgent, when) == "1"
            for guid in roster:
                row = samples.row(guid, when)
                if not alive(row):
                    continue
                listed = value_at(team.get(guid, []), when) == "1"
                if listed or (everyone_ranged and trace.role(guid) == "ranged"):
                    if not listed:
                        drafted.add(guid)
                    if star:
                        open_ms[guid] += weight
                        on[guid] += weight if target_of(row) == star else 0
                elif target_of(row) in stars and trace.role(guid) in ("melee", "ranged"):
                    off[guid] += weight
                    off_from.setdefault(guid, when)
    return {"members": members, "drafted": drafted, "on": on, "open": open_ms, "off": off, "off_from": off_from}


def parse_stardeath(text) -> tuple[str, float | None]:
    """`algalon.stardeath` reads `<killed|collapsed> <yd>`, the yd `-` when nobody was unphased."""
    how, _, gap = str(text or "").strip().partition(" ")
    try:
        return how, float(gap)
    except ValueError:
        return how, None


def explosion_safe_hp(trace: Trace) -> int:
    return EXPLOSION_SAFE_HP[25 if trace.header.get("diff") in (1, 3) else 10]


def health_of(trace: Trace, guid: int, row) -> int | None:
    """Hit points from a snapshot row's percentage, or None without the unit's max health."""
    top = trace.maxhp.get(guid)
    return round(row[5] * top / 100.0) if top else None


def below_safe(trace: Trace, up: dict[int, list]) -> list[tuple[int, int, float]]:
    """`(hp, guid, pct)` for each of `up` a Black Hole Explosion could kill, weakest first: below its top
    roll, or below full health for someone whose max health is under it."""
    safe = explosion_safe_hp(trace)
    low = [(hp, guid, row[5]) for guid, row in up.items() if (hp := health_of(trace, guid, row)) is not None]
    return sorted(entry for entry in low if entry[0] < min(safe, trace.maxhp.get(entry[1], safe)))


def note_near(trace: Trace, key: str, when: int, slack: int) -> dict | None:
    found = [rec for rec in notes(trace, key) if abs(rec["t"] - when) <= slack]
    return min(found, key=lambda rec: abs(rec["t"] - when)) if found else None


def value_before(trace: Trace, key: str, when: int) -> str | None:
    """The latest value any bot wrote to `key` before `when`."""
    found = [rec for rec in notes(trace, key) if rec["t"] < when]
    return str(found[-1].get("txt", "")) if found else None


def star_rows(trace: Trace) -> list[dict]:
    """`star_deaths`, plus what each landed into: the probe's own note, the kill window, a live meteor,
    a Big Bang cast, the nearest unphased raid member, who it could kill, and who it did."""
    samples = samples_of(trace)
    roster = roster_guids(trace)
    phased = all_phases(trace)
    casts = big_bang_casts(trace)
    seen = [meteor["t"] for meteor in meteors(trace)[0]]
    killed = [row["t"] for row in death_rows(trace) if row["cause"] == "star explosion"]
    out = []
    for death in star_deaths(trace):
        when = death["t"]
        up = {guid: row for guid, row in samples.frame(when - 1).items()
              if guid in roster and alive(row) and not phased_at(phased, guid, when)}
        last = samples.seen(death["star"]) if death["star"] else None
        spot = samples.row(death["star"], last[1])[1:3] if last else None
        weakest = min(((health_of(trace, guid, row), guid, row[5]) for guid, row in up.items()
                       if health_of(trace, guid, row) is not None), default=None)
        probe = note_near(trace, "algalon.stardeath", when, STAR_NOTE_MS)
        out.append({
            **death,
            "note": parse_stardeath(probe.get("txt")) if probe else None,
            "window": value_before(trace, "algalon.starwindow", when),
            "meteor": any(when - METEOR_LIVE_MS <= appeared <= when for appeared in seen),
            "bang": next((cast["n"] for cast in casts if cast["start"] <= when <= cast["end"]), None),
            "nearest": min(((dist2(row[1:3], spot), guid) for guid, row in up.items()), default=None) if spot else None,
            "weakest": weakest,
            "below": below_safe(trace, up),
            "killed": sum(1 for at in killed if when - SAMPLE_LAG_MS <= at <= death["last"] + SHARE_MS),
        })
    return out


def star_flags(row: dict) -> str:
    flags = " <8s" if row["gap"] is not None and row["gap"] < STAR_GAP_MS else ""
    flags += " lowhp" if row["below"] else ""
    flags += " smash" if row["meteor"] else ""
    flags += " bigbang" if row["bang"] else ""
    flags += " near" if row["nearest"] and row["nearest"][0] < STAR_NEAR_YD else ""
    return flags


def show_stars(trace: Trace) -> None:
    print("COLLAPSING STARS")
    stars = guids_of_entry(trace, NPC_COLLAPSING_STAR)
    rows = star_rows(trace)
    if not stars and not rows:
        print("  no star was sampled")
        return
    show_absent(trace, "algalon.starwindow", "algalon.stardeath", "algalon.focusstar", "algalon.starteam")

    safe = explosion_safe_hp(trace)
    print(f"  {len(stars)} star(s) sampled, {len(rows)} died; an explosion can kill anyone unphased under {safe:,} hp")
    if rows:
        print(f"  {'at':>9} {'star':18} {'how':9} {'hits':>4} {'damage':>9} {'gap':>7} {'raid min':>8}")
    for row in rows:
        gap = "-" if row["gap"] is None else f"{row['gap'] / 1000:.1f}s"
        low = "-" if row["raid_min"] is None else f"{row['raid_min']:.0f}%"
        print(f"  {clock(row['t']):>9} {trace.name(row['star'])[:18]:18} {row['how']:9} {row['hits']:4}"
              f" {row['damage']:9,} {gap:>7} {low:>8}{star_flags(row)}")
        parts = []
        if row["note"]:
            parts.append(f"algalon.stardeath {row['note'][0]} {'-' if row['note'][1] is None else row['note'][1]}")
        parts.append(f"window {row['window'] or '-'}")
        if row["nearest"]:
            parts.append(f"nearest {trace.name(row['nearest'][1])} {row['nearest'][0]:.1f} yd")
        if row["weakest"]:
            parts.append(f"weakest {trace.name(row['weakest'][1])} {row['weakest'][0]:,} hp ({row['weakest'][2]:.0f}%)")
        parts.append(f"{len(row['below'])} under {safe:,}")
        if row["meteor"]:
            parts.append("a meteor live")
        if row["bang"]:
            parts.append(f"Big Bang {row['bang']} casting")
        parts.append(f"{row['killed']} died to it")
        print(f"      {'; '.join(parts)}")

    window = span_totals(holder_spans(trace, "algalon.starwindow", pull_end(trace)))
    total = sum(window.values())
    if total:
        print("  algalon.starwindow: " + ", ".join(f"{value} {held / 1000:.1f} s ({held * 100.0 / total:.0f}%)"
                                                  for value, held in window.most_common()))

    team = star_team(trace)
    if team["members"] or team["drafted"]:
        print("  star team, + for ranged counted in while algalon.urgent read 1:")
        for guid in team["members"] + sorted(team["drafted"] - set(team["members"]), key=trace.name):
            share = (f"{team['on'][guid] * 100.0 / team['open'][guid]:.0f}% of {team['open'][guid] / 1000:.1f} s"
                     if team["open"][guid] else "window never open while on the team")
            mark = "+" if guid not in team["members"] else " "
            print(f"   {mark}{who(trace, guid):28} on the focus star {share}")
    else:
        print("  no star team recorded")
    if team["off"]:
        print("  damage dealers off the team on a star: " + ", ".join(
            f"{trace.name(guid)} {held / 1000:.1f} s" for guid, held in team["off"].most_common()))


# ---------------------------------------------------------------------------------------------- holes

def hole_counts(trace: Trace) -> list[int]:
    samples = samples_of(trace)
    holes = hole_guids(trace)
    return [len(holes_at(samples, holes, when)) for when in samples.stamps]


def hole_stock(trace: Trace) -> list[tuple[int, int]]:
    """`(t, count)` at every change in how many holes the sweep saw."""
    out: list[tuple[int, int]] = []
    for when, count in zip(samples_of(trace).stamps, hole_counts(trace)):
        if not out or out[-1][1] != count:
            out.append((when, count))
    return out


def empty_before(trace: Trace, counts: list[int], when: int) -> int:
    """Milliseconds with no hole in the shelter window before `when`."""
    return sum(weight for index, _, weight in samples_of(trace).steps(when - SHELTER_WINDOW_MS, when)
               if counts[index] == 0)


def show_holes(trace: Trace) -> None:
    print("HOLES")
    holes = hole_guids(trace)
    lost = notes(trace, "algalon.holelost")
    if not holes and not lost:
        print("  no hole was sampled")
        return
    show_absent(trace, "algalon.holes", "algalon.holelost", "algalon.urgent")

    stock = hole_stock(trace)
    shown = stock[:TIMELINE_SHOWN]
    print("  swept stock: " + ", ".join(f"{clock(when)} {count}" for when, count in shown)
          + (f" and {len(stock) - len(shown)} more" if len(stock) > len(shown) else ""))
    probe = latch_spans(trace, "algalon.holes", pull_end(trace))
    if probe:
        print("  algalon.holes: " + ", ".join(f"{clock(start)} {value}" for value, start, _ in probe[:TIMELINE_SHOWN])
              + (f" and {len(probe) - TIMELINE_SHOWN} more" if len(probe) > TIMELINE_SHOWN else ""))

    counts = hole_counts(trace)
    samples = samples_of(trace)
    for cast in big_bang_casts(trace):
        present = len(holes_at(samples, holes, cast["start"]))
        print(f"  Big Bang #{cast['n']} {clock(cast['start'])}: {present} hole(s) at the cast,"
              f" {empty_before(trace, counts, cast['start']) / 1000:.1f} s with none in the"
              f" {SHELTER_WINDOW_MS // 1000} s before")

    reasons = collections.Counter(str(rec.get("txt", "")) for rec in lost)
    if lost:
        print("  algalon.holelost: " + ", ".join(f"{reason} {n}" for reason, n in reasons.most_common())
              + "  (" + ", ".join(f"{clock(rec['t'])} {rec.get('txt')}" for rec in lost[:TIMELINE_SHOWN]) + ")")
    urgent = [(start, stop) for value, start, stop in latch_spans(trace, "algalon.urgent", pull_end(trace))
              if value == "1"]
    if urgent:
        print(f"  algalon.urgent: {len(urgent)} span(s), {sum(b - a for a, b in urgent) / 1000:.1f} s: "
              + ", ".join(f"{clock(a)}..{clock(b)}" for a, b in urgent))


# ------------------------------------------------------------------------------------- constellations

def victim_changes(samples: Samples, guid: int) -> list[tuple[int, int]]:
    """`(t, victim)` at each change of a unit's target column while it lives."""
    out: list[tuple[int, int]] = []
    for when, row, _ in samples.unit_steps(guid):
        victim = target_of(row)
        if alive(row) and (not out or out[-1][1] != victim):
            out.append((when, victim))
    return out


def constellation_rows(trace: Trace) -> list[dict]:
    """Per constellation: when it woke (first victim, cast or hit), who it chased, and how it left."""
    samples = samples_of(trace)
    end = pull_end(trace)
    holes = hole_guids(trace)
    handler = latch_spans(trace, "algalon.handler", end)
    p2 = phase_start(trace, "P2")
    despawn = final_despawn(trace)
    casts = trace.of("cast")
    hits = trace.of("dmg")

    out = []
    # removal is the last row, not the first gap: a unit can drop out of the sweep and come back
    for guid in sorted(guids_of_entry(trace, NPC_LIVING_CONSTELLATION)):
        found = samples.rows.get(guid)
        if not found:
            continue
        stamps, rows = found
        woke = [when for when, row in zip(stamps, rows) if target_of(row)][:1]
        woke += [rec["t"] for rec in casts if rec.get("s") == guid][:1]
        woke += [rec["t"] for rec in hits if rec.get("s") == guid][:1]
        active = min(woke) if woke else None

        removed = stamps[-1] if stamps[-1] < end - GONE_MS else None
        how, spent = None, []
        if removed is not None:
            spot = rows[-1][1:3]
            spent = [hole for hole in sorted(holes)
                     if (seen := samples.seen(hole)) and abs(seen[1] - removed) <= HOLE_MATCH_MS
                     and dist2(samples.row(hole, seen[1])[1:3], spot) <= HOLE_MATCH_YD]
            if p2 is not None and abs(removed - p2) <= 2000:
                how = "phase2"
            elif removed == despawn:
                how, spent = "despawn", []
            elif spent:
                how = "hole"
            elif min(row[5] for row in samples.between(guid, removed - 2000, removed)) <= KILLED_HP:
                how = "killed"
            else:
                how = "gone"
        out.append({
            "guid": guid,
            "active": active,
            "victims": [(when, victim) for when, victim in victim_changes(samples, guid)
                        if active is not None and when >= active],
            "removed": removed,
            "how": how,
            "spent": spent,
            "last_victim": target_of(rows[-1]),
            "handler": as_guid(value_at(handler, removed)) if removed is not None else 0,
        })
    return out


def show_constellations(trace: Trace) -> None:
    print("LIVING CONSTELLATIONS")
    rows = constellation_rows(trace)
    if not rows:
        print("  no constellation was sampled")
        return
    show_absent(trace, "algalon.handler", "algalon.kite", "algalon.kitehole")

    woke = [row for row in rows if row["active"] is not None]
    print(f"  {len(rows)} sampled, {len(woke)} woke")
    for row in woke:
        chase = ", ".join(f"{clock(when)} {who(trace, victim)}" for when, victim in row["victims"][:6] if victim)
        left = "-"
        if row["removed"] is not None:
            left = (f"{row['how']} at {clock(row['removed'])}, {(row['removed'] - row['active']) / 1000:.1f} s awake,"
                    f" last on {trace.name(row['last_victim'])}")
        print(f"    woke {clock(row['active'])}  chased {chase or 'nobody'}\n      left: {left}")

    held: collections.Counter = collections.Counter()
    for row in woke:
        # each over its own awake span, an inactive one has no victim to count
        until = row["removed"] if row["removed"] is not None else math.inf
        held += threat_share(trace, {row["guid"]}, lambda when, a=row["active"], b=until: a <= when < b)[0]
    if sum(held.values()):
        show_share(held, "victims by role while awake")

    handler = latch_spans(trace, "algalon.handler", pull_end(trace))
    if handler:
        print("  algalon.handler: " + ", ".join(f"{clock(start)} {trace.name(as_guid(value))}"
                                               for value, start, _ in handler))
    kite = holder_spans(trace, "algalon.kite", pull_end(trace))
    totals = span_totals(kite)
    if totals:
        print("  algalon.kite: " + ", ".join(f"{value} {held / 1000:.1f} s" for value, held in totals.most_common()))
        busy = sorted(((sum(stop - start for value, start, stop in spans if value not in ("none", "")), guid)
                       for guid, spans in kite.items()), reverse=True)
        print("    by bot, outside none: " + ", ".join(f"{trace.name(guid)} {held / 1000:.1f} s"
                                                     for held, guid in busy if held))
    assigned = [rec for rec in notes(trace, "algalon.kitehole") if as_guid(rec.get("txt"))]
    if assigned:
        print(f"  algalon.kitehole: {len(assigned)} assignment(s) to"
              f" {len({rec.get('txt') for rec in assigned})} hole(s)")
    spent = sum(len(row["spent"]) for row in rows if row["how"] == "hole")
    lost = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "algalon.holelost"))
    print(f"  holes spent on constellations: {spent} seen swallowed, algalon.holelost kite {lost['kite']},"
          f" stray {lost['stray']}")


# ---------------------------------------------------------------------------------------------- smash

def meteors(trace: Trace) -> tuple[list[dict], str]:
    """`{t, spot}` per meteor and where they came from: the `haz` rows, else each 33105 marker's first
    row, else each 33104's."""
    def build(trace: Trace) -> tuple[list[dict], str]:
        hazards = [rec for rec in trace.of("haz") if rec.get("sp") in SPELL_COSMIC_SMASH]
        if hazards:
            return [{"t": rec["t"], "spot": (rec.get("x", 0.0), rec.get("y", 0.0))} for rec in hazards], "haz"
        samples = samples_of(trace)
        for entry in reversed(NPC_COSMIC_SMASH_MARKERS):
            found = sorted((seen[0], guid) for guid in guids_of_entry(trace, entry) if (seen := samples.seen(guid)))
            if found:
                return ([{"t": when, "spot": tuple(samples.row(guid, when)[1:3])} for when, guid in found],
                        f"{entry} markers")
        return [], "-"
    return cached(trace, "_algalon_meteors", build)


def smash_waves(trace: Trace) -> list[dict]:
    """Meteors that fell together, `{seen, impact, meteors, hits}`. The impact is the wave's first
    Cosmic Smash row, or SMASH_DELAY_MS after it showed when nothing was hit."""
    def build(trace: Trace) -> list[dict]:
        hits = [rec for rec in trace.of("dmg") if rec.get("sp") in SPELL_COSMIC_SMASH]
        waves: list[dict] = []
        for meteor in sorted(meteors(trace)[0], key=lambda meteor: meteor["t"]):
            if waves and meteor["t"] - waves[-1]["seen"] <= WAVE_MS:
                waves[-1]["meteors"].append(meteor)
            else:
                waves.append({"seen": meteor["t"], "meteors": [meteor]})
        for wave in waves:
            first = next((rec["t"] for rec in hits
                          if wave["seen"] + IMPACT_MIN_MS <= rec["t"] <= wave["seen"] + IMPACT_MAX_MS), None)
            wave["impact"] = first if first is not None else wave["seen"] + SMASH_DELAY_MS
            wave["hits"] = [rec for rec in hits
                            if wave["impact"] - SAMPLE_LAG_MS <= rec["t"] <= wave["impact"] + SMASH_MATCH_MS]
        return waves
    return cached(trace, "_algalon_waves", build)


def to_meteor(wave: dict, spot) -> float | None:
    return min(dist2(spot, meteor["spot"]) for meteor in wave["meteors"]) if spot else None


def wave_dodge(spans, wave: dict):
    """The dodge branch this wave set. The probe falls back to `idle` once the bot is clear, so the
    value at the impact is usually that rather than how the dodge went."""
    found = None
    for value, start, stop in spans:
        if start <= wave["impact"] and stop > wave["seen"] and value != "idle":
            found = value
    return found


def wave_rows(trace: Trace, wave: dict) -> list[dict]:
    """Per raid member within SMASH_NEAR_YD of a meteor when it showed or when it hit: how soon the
    dodge set off, the probe's verdict, where it stood at the impact, and what walked it back."""
    samples = samples_of(trace)
    dodges = holder_spans(trace, "algalon.dodge", pull_end(trace))
    rows = []
    for guid in roster_guids(trace):
        first = samples.row(guid, wave["seen"])
        if not alive(first):
            continue
        at_seen = to_meteor(wave, first[1:3])
        at_impact = to_meteor(wave, position_at(samples, guid, wave["impact"]))
        if min(at_seen, at_impact if at_impact is not None else math.inf) > SMASH_NEAR_YD:
            continue
        moves = [rec for rec in moves_of(trace).get(guid, []) if wave["seen"] <= rec["t"] <= wave["impact"]]
        dodge = next((rec for rec in moves if rec.get("by") == SMASH_MOVER), None)
        back = next((rec for rec in reversed(moves)
                     if dodge and rec["t"] > dodge["t"] and rec.get("by") != SMASH_MOVER), None)
        rows.append({
            "guid": guid,
            "at_seen": at_seen,
            "moved": dodge["t"] - wave["seen"] if dodge else None,
            "dodge": wave_dodge(dodges.get(guid, []), wave),
            "at_impact": at_impact,
            "damage": sum(rec.get("a", 0) for rec in wave["hits"] if rec.get("d") == guid),
            "back": back.get("by") if back else None,
        })
    return sorted(rows, key=lambda row: math.inf if row["at_impact"] is None else row["at_impact"])


def band(distance: float) -> str:
    return next(name for edge, name in SMASH_BANDS if distance < edge)


def show_smash(trace: Trace) -> None:
    print("COSMIC SMASH")
    waves = smash_waves(trace)
    explained = {id(rec) for wave in waves for rec in wave["hits"]}
    stray = [rec for rec in trace.of("dmg") if rec.get("sp") in SPELL_COSMIC_SMASH and id(rec) not in explained]
    if not waves and not stray:
        print("  no meteor fell and nothing took Cosmic Smash damage")
        return
    source = meteors(trace)[1]
    print("  meteors from haz" if source == "haz" else f"  absent from this trace: haz meteors; read off {source}")
    show_absent(trace, "algalon.dodge")

    by_band: dict[str, list[int]] = collections.defaultdict(list)
    for wave in waves:
        print(f"  wave {clock(wave['seen'])}: {len(wave['meteors'])} meteor(s), impact {clock(wave['impact'])}"
              f" (+{(wave['impact'] - wave['seen']) / 1000:.1f} s)")
        rows = wave_rows(trace, wave)
        if rows:
            print(f"      {'who':28} {'at seen':>7} {'moved':>7} {'dodge':>6} {'at impact':>9} {'damage':>8}"
                  f"  walked back by")
        for row in rows:
            moved = "-" if row["moved"] is None else f"+{row['moved'] / 1000:.1f}s"
            impact = "-" if row["at_impact"] is None else f"{row['at_impact']:.1f}"
            name = who(trace, row["guid"]) + ("*" if row["guid"] in trace.humans else "")
            print(f"      {name:28} {row['at_seen']:7.1f} {moved:>7} {row['dodge'] or '-':>6}"
                  f" {impact:>9} {row['damage']:8,}  {row['back'] or '-'}")
            if row["damage"] and row["at_impact"] is not None:
                by_band[band(row["at_impact"])].append(row["damage"])
    if by_band:
        print("  damage by distance at the impact: " + ", ".join(
            f"{name} {len(by_band[name])} x {sum(by_band[name]):,}" for _, name in SMASH_BANDS if by_band[name]))
    if stray:
        print(f"  {len(stray)} Cosmic Smash hit(s) no wave explains: "
              + ", ".join(f"{clock(rec['t'])} {trace.name(rec.get('d'))} {rec.get('a', 0):,}" for rec in stray[:8]))


def smash_faults(trace: Trace) -> list[tuple[int, str]]:
    """Verdict lines: who stood inside SMASH_INSIDE_YD at an impact, and who dodged and was walked back
    inside SMASH_CLEAR_YD by something else before it."""
    out = []
    for wave in smash_waves(trace):
        rows = wave_rows(trace, wave)
        inside = [row for row in rows if row["at_impact"] is not None and row["at_impact"] < SMASH_INSIDE_YD]
        if inside:
            out.append((wave["impact"], f"Cosmic Smash @ {secs(wave['impact'])}: inside {SMASH_INSIDE_YD:.0f} yd at"
                                        f" the impact: " + ", ".join(f"{trace.name(row['guid'])} {row['at_impact']:.1f}"
                                                                     for row in inside)))
        back = [row for row in rows
                if row["back"] and row["at_impact"] is not None and row["at_impact"] < SMASH_CLEAR_YD]
        if back:
            out.append((wave["impact"], f"Cosmic Smash @ {secs(wave['impact'])}: dodged, then walked back inside"
                                        f" {SMASH_CLEAR_YD:.0f} yd by the impact: " + ", ".join(
                                            f"{trace.name(row['guid'])} {row['at_impact']:.1f} ({row['back']})"
                                            for row in back)))
    return out


# --------------------------------------------------------------------------------------------- deaths

def killing_row(trace: Trace, death: dict) -> dict | None:
    """The blow that killed: the lethal `dmg` row just before the death, else the rewind's last."""
    rows = [rec for rec in victim_hits(trace).get(death["g"], [])
            if death["t"] - KILL_MATCH_MS <= rec["t"] <= death["t"]]
    lethal = [rec for rec in rows if rec.get("ok", 0) > 0]
    if lethal or rows:
        return (lethal or rows)[-1]
    rewind = death.get("rewind") or []
    if rewind and len(rewind[-1]) >= 4:
        when, source, spell, amount = rewind[-1][:4]
        return {"t": when, "s": source, "d": death["g"], "sp": spell, "a": amount}
    return None


def explosion_share(trace: Trace, death: dict, since: int) -> float:
    """Black Hole Explosion's share of the damage taken in the SHARE_MS before the death, this life only."""
    rows = [rec for rec in victim_hits(trace).get(death["g"], [])
            if max(since, death["t"] - SHARE_MS) < rec["t"] <= death["t"]]
    total = sum(rec.get("a", 0) for rec in rows)
    burst = sum(rec.get("a", 0) for rec in rows if rec.get("sp") in SPELL_BLACK_HOLE_EXPLOSION)
    return burst / total if total else 0.0


def algalon_death(trace: Trace, death: dict) -> tuple[str, str]:
    pair, _ = swap_pair(trace)
    if death["g"] in pair:
        return "Algalon on tank", ""
    window = next((window for window in loose_windows(trace)
                   if window["start"] <= death["t"] <= window["stop"] + SAMPLE_LAG_MS), None)
    intro = intro_end(trace)
    if (window and window["cause"] == "intro end") or (intro is not None and 0 <= death["t"] - intro <= INTRO_CAUSE_MS):
        return "Algalon melee (intro end)", ""
    return "Algalon melee (loose)", window_cause(window) if window else "no loose window"


def smash_band_at(trace: Trace, death: dict, when: int) -> str:
    if "x" in death and "y" in death:
        spot = (death["x"], death["y"])
    else:
        spot = position_at(samples_of(trace), death["g"], when)
    gaps = [gap for wave in smash_waves(trace) if abs(wave["impact"] - when) <= SMASH_MATCH_MS
            if (gap := to_meteor(wave, spot)) is not None]
    return f"{band(min(gaps))} yd" if gaps else ""


def classify_death(trace: Trace, death: dict, since: int) -> tuple[str, str]:
    """The mechanic that killed, and a detail. A star explosion owns any death it did half the last
    SHARE_MS of damage to, whatever landed the blow."""
    if death.get("cause") == "reset":
        return "wipe command", ""
    blow = killing_row(trace, death)
    if blow is None:
        return "other", death.get("cause") or "no blow"
    spell, source = blow.get("sp"), blow.get("s")
    share = explosion_share(trace, death, since)
    if spell in SPELL_BLACK_HOLE_EXPLOSION:
        return "star explosion", ""
    if share >= EXPLOSION_SHARE:
        return "star explosion", f"{share:.0%} of the last {SHARE_MS // 1000} s"
    if trace.entries.get(source) == NPC_DARK_MATTER:
        bang = any(cast["start"] <= death["t"] <= cast["end"] + MATTER_BANG_MS for cast in big_bang_casts(trace))
        return f"Dark Matter while phased ({'Big Bang' if bang else 'stray'})", ""
    if spell in SPELL_BIG_BANG:
        return "Big Bang", ""
    if source == boss_guid(trace):
        return algalon_death(trace, death)
    if spell in SPELL_COSMIC_SMASH:
        return "Cosmic Smash", smash_band_at(trace, death, blow["t"])
    if spell in SPELL_CONSTELLATION_BARRAGE:
        return "Constellation Barrage", ""
    if spell == SPELL_BLACK_HOLE_DOT:
        return "Black Hole DoT", ""
    return "other", f"{trace.name(source)} {trace.spell(spell)}"


def death_rows(trace: Trace) -> list[dict]:
    """`{t, guid, cause, detail}` per death, the wipe command's included."""
    def build(trace: Trace) -> list[dict]:
        out, lives = [], {}
        for rec in death_records(trace):
            cause, detail = classify_death(trace, rec, lives.get(rec["g"], -(1 << 62)))
            lives[rec["g"]] = rec["t"]
            out.append({"t": rec["t"], "guid": rec["g"], "cause": cause, "detail": detail})
        return out
    return cached(trace, "_algalon_deaths", build)


def show_deaths(trace: Trace) -> None:
    print("DEATHS")
    if boss_guid(trace) is None:
        print("  Algalon was never sampled")
        return
    rows = death_rows(trace)
    if not rows:
        print("  nobody died")
        return
    roles = list(DEATH_ROLES) + sorted({trace.role(row["guid"]) for row in rows} - set(DEATH_ROLES))
    count: collections.Counter = collections.Counter()
    humans: collections.Counter = collections.Counter()
    for row in rows:
        count[(row["cause"], trace.role(row["guid"]))] += 1
        humans[(row["cause"], trace.role(row["guid"]))] += row["guid"] in trace.humans
    causes = collections.Counter(row["cause"] for row in rows)
    print(f"  {'cause':38}" + "".join(f" {role:>7}" for role in roles) + f" {'total':>6}")
    for cause, total in causes.most_common():
        cells = [str(count[(cause, role)]) + "*" * humans[(cause, role)] if count[(cause, role)] else "-"
                 for role in roles]
        print(f"  {cause:38}" + "".join(f" {cell:>7}" for cell in cells) + f" {total:6}")
    print("  each * is a human among them")
    for row in rows:
        human = "*" if row["guid"] in trace.humans else ""
        print(f"  {clock(row['t']):>9}  {who(trace, row['guid']) + human:28} {row['cause']}"
              + (f": {row['detail']}" if row["detail"] else ""))


# ---------------------------------------------------------------------------------------------- tanks

def phase_punch(trace: Trace) -> dict[int, list[tuple[int, int, bool]]]:
    """Per target, `(t, stacks, removed)` for every Phase Punch row."""
    out: dict[int, list[tuple[int, int, bool]]] = collections.defaultdict(list)
    for rec in trace.of("aura"):
        if rec.get("sp") == SPELL_PHASE_PUNCH:
            out[rec.get("d", 0)].append((rec["t"], rec.get("st", 1) or 1, bool(rec.get("r"))))
    return out


def punched_out(trace: Trace) -> list[tuple[int, int]]:
    return [(rec["t"], rec.get("d", 0)) for rec in trace.of("aura")
            if rec.get("sp") == SPELL_PHASE_PUNCH_PHASE and not rec.get("r")]


def boss_victims(trace: Trace) -> list[tuple[int, int]]:
    boss = boss_guid(trace)
    return victim_changes(samples_of(trace), boss) if boss else []


def pickups(trace: Trace) -> list[tuple[int, int, int]]:
    """`(start, stop, guid)` for every stretch he spent on someone who is not a tank."""
    victims = boss_victims(trace)
    end = pull_end(trace)
    out = []
    for index, (when, victim) in enumerate(victims):
        stop = victims[index + 1][0] if index + 1 < len(victims) else end
        if victim and trace.role(victim) != "tank":
            out.append((when, stop, victim))
    return out


def show_tanks(trace: Trace) -> None:
    print("TANKS")
    boss = boss_guid(trace)
    if boss is None:
        print("  Algalon was never sampled")
        return

    stacks = phase_punch(trace)
    if stacks:
        print("  Phase Punch:")
        for guid, rows in sorted(stacks.items(), key=lambda item: trace.name(item[0])):
            steps = [f"{clock(when)} {'off' if removed else count}" for when, count, removed in rows]
            peak = max((count for _, count, removed in rows if not removed), default=0)
            print(f"    {who(trace, guid):26} max {peak}: {', '.join(steps[:TIMELINE_SHOWN])}"
                  + (f" and {len(steps) - TIMELINE_SHOWN} more" if len(steps) > TIMELINE_SHOWN else ""))
    else:
        print("  no Phase Punch rows")
    for when, guid in punched_out(trace):
        print(f"  5th stack: {who(trace, guid)} phased out at {clock(when)}")

    victims = boss_victims(trace)
    swaps = sum(1 for index in range(1, len(victims))
                if trace.role(victims[index][1]) == "tank" and trace.role(victims[index - 1][1]) == "tank")
    print(f"  his victim, {swaps} tank swap(s): " + ", ".join(f"{clock(when)} {trace.name(victim)}"
                                                           for when, victim in victims[:TIMELINE_SHOWN]))
    for start, stop, guid in pickups(trace):
        print(f"    held by a non-tank: {who(trace, guid)} {(stop - start) / 1000:.1f} s from {clock(start)}")

    strikes: dict[int, list[int]] = collections.defaultdict(list)
    for rec in trace.of("dmg"):
        if rec.get("sp") in SPELL_QUANTUM_STRIKE:
            strikes[rec.get("d", 0)].append(rec.get("a", 0))
    if strikes:
        print("  Quantum Strike: " + ", ".join(
            f"{who(trace, guid)} {len(hits)} x {sum(hits):,} (max {max(hits):,})"
            for guid, hits in sorted(strikes.items(), key=lambda item: -sum(item[1]))))

    reach = [(dist2(row[1:3], ROOM_CENTER), when) for when, row, _ in samples_of(trace).unit_steps(boss)]
    if reach:
        far, when = max(reach)
        print(f"  furthest from the room centre: {far:.1f} yd at {clock(when)} (room {ROOM_RADIUS:.0f} yd)")


# ------------------------------------------------------------------------------------------ formation

def parse_ring_slots(text: str) -> list[tuple[float, float, float]]:
    """Every `Position(x, y, z)` in the `ULDUAR_ALGALON_RING_SLOTS` table, in slot order, or nothing when
    the source has no table."""
    found = RING_SLOT_TABLE.search(text)
    if not found:
        return []
    return [(float(x), float(y), float(z)) for x, y, z in RING_SLOT_ROW.findall(found.group(1))]


def arc_offset(index: int, count: int, width: float) -> float:
    """Centre-out around the arc's middle, alternating sides: the same order the strategy filled in."""
    if count <= 1:
        return 0.0
    step = width / (count - 1)
    if count % 2:
        if index == 0:
            return 0.0
        offset = step * ((index + 1) // 2)
        return -offset if index % 2 == 0 else offset
    offset = step / 2 + step * (index // 2)
    return -offset if index % 2 else offset


def arc_slots() -> list[tuple[float, float]]:
    """The slots of a source from before the table: three arcs around the tank slot, 4, 6 and 8 wide.
    Empty when those constants are gone too."""
    out = []
    try:
        for name, count in (("HEALER", 4), ("RANGED_INNER", 6), ("RANGED_OUTER", 8)):
            ring = radius(f"ULDUAR_ALGALON_{name}_RADIUS")
            centre = radius(f"ULDUAR_ALGALON_{name}_ARC_CENTER")
            width = radius(f"ULDUAR_ALGALON_{name}_ARC_WIDTH")
            for index in range(count):
                bearing = centre + arc_offset(index, count, width)
                out.append((TANK_SLOT[0] + math.cos(bearing) * ring, TANK_SLOT[1] + math.sin(bearing) * ring))
    except (Unknown, ValueError):
        return []
    return out


@functools.lru_cache(maxsize=1)
def ring_slots() -> tuple[tuple[float, float], ...]:
    """Each ring slot's spot by index: the source's table, else the older arcs."""
    try:
        text = ALGALON_SOURCE.read_text(encoding="utf-8", errors="replace")
    except OSError:
        text = ""
    table = parse_ring_slots(text)
    return tuple((x, y) for x, y, _ in table) if table else tuple(arc_slots())


def slot_position(index: int) -> tuple[float, float] | None:
    spots = ring_slots()
    return spots[index] if 0 <= index < len(spots) else None


def slots(trace: Trace) -> dict[int, int]:
    """First `algalon.slot` per bot. First rather than latest: an erase is written as "0", which is
    also a real slot."""
    out: dict[int, int] = {}
    for rec in notes(trace, "algalon.slot"):
        try:
            out.setdefault(rec["g"], int(rec.get("txt", "")))
        except ValueError:
            continue
    return out


def ring_rows(trace: Trace) -> list[dict]:
    samples = samples_of(trace)
    end = pull_end(trace)
    boss = boss_guid(trace)
    begin = intro_end(trace)
    held = slots(trace)
    members = set(held) or {guid for guid in roster_guids(trace) if trace.role(guid) in ("heal", "ranged")}
    spot = holder_spans(trace, "algalon.spot", end)
    phased = phased_spans(trace)
    stalls = stall_windows(trace, STALL_MS)

    out = []
    for guid in sorted(members, key=trace.name):
        slot = held.get(guid)
        target = slot_position(slot) if slot is not None else None
        to_slot, to_tank, to_boss = [], [], []
        for _, when, _ in samples.steps(0, end):
            row = samples.row(guid, when)
            if not alive(row) or phased_at(phased, guid, when):
                continue
            him = samples.row(boss, when) if boss is not None and begin is not None and when >= begin else None
            if alive(him):
                to_boss.append(dist2(row[1:3], him[1:3]))
            if spot and value_at(spot.get(guid, []), when) not in (None, "slot"):
                continue
            to_tank.append(dist2(row[1:3], TANK_SLOT))
            if target:
                to_slot.append(dist2(row[1:3], target))
        branches = collections.Counter()
        for value, start, stop in spot.get(guid, []):
            branches[value] += stop - start
        mine = [window for window in stalls if window["guid"] == guid]
        out.append({
            "guid": guid,
            "slot": slot,
            "to_slot": statistics.median(to_slot) if to_slot else None,
            "to_tank": statistics.median(to_tank) if to_tank else None,
            "to_boss": statistics.median(to_boss) if to_boss else None,
            "in_reach": sum(1 for gap in to_boss if gap <= MELEE_REACH) / len(to_boss) if to_boss else None,
            "branches": branches,
            "stalls": len(mine),
            "stalled": sum(window["end"] - window["start"] for window in mine),
        })
    return out


def show_formation(trace: Trace) -> None:
    print("FORMATION")
    if boss_guid(trace) is None:
        print("  Algalon was never sampled")
        return
    show_absent(trace, "algalon.slot", "algalon.spot")
    if not ring_slots():
        print("  no ring slot table or ring constants in the source: distance to the tank slot only")

    rows = ring_rows(trace)
    if not rows:
        print("  no ring bot")
        return
    print(f"  {'bot':14} {'role':6} {'slot':>4} {'to slot':>8} {'to tank slot':>12} {'to boss':>8} {'in reach':>8}"
          f" {'stalls':>10}  spot")
    for row in rows:
        total = sum(row["branches"].values())
        shares = ", ".join(f"{value} {held * 100.0 / total:.0f}%" for value, held in row["branches"].most_common()) \
            if total else "-"
        to_slot = "-" if row["to_slot"] is None else f"{row['to_slot']:.1f}"
        to_tank = "-" if row["to_tank"] is None else f"{row['to_tank']:.1f}"
        to_boss = "-" if row["to_boss"] is None else f"{row['to_boss']:.1f}"
        in_reach = "-" if row["in_reach"] is None else f"{row['in_reach'] * 100.0:.0f}%"
        slot = "-" if row["slot"] is None else str(row["slot"])
        stalled = f"{row['stalls']} / {row['stalled'] / 1000:.0f}s" if row["stalls"] else "-"
        print(f"  {trace.name(row['guid'])[:14]:14} {trace.role(row['guid']):6} {slot:>4} {to_slot:>8}"
              f" {to_tank:>12} {to_boss:>8} {in_reach:>8} {stalled:>10}  {shares}")
    print("  medians over living, unphased samples; to slot and to tank slot only while algalon.spot read slot"
          f" where it exists, to boss and in reach ({MELEE_REACH:.2f} yd) over every sample after the intro;"
          f" stalls are {STALL_MS // 1000} s+ stationary windows with accepted moves")


# ----------------------------------------------------------------------------------------- darkmatter

def dark_matter_rows(trace: Trace) -> list[dict]:
    """Per Unleashed Dark Matter: time on anyone but the handler, the damage it did, and its death.
    Without `algalon.handler`, any tank counts as the handler."""
    samples = samples_of(trace)
    end = pull_end(trace)
    handler = latch_spans(trace, "algalon.handler", end)
    guids = sorted(guids_of_entry(trace, NPC_UNLEASHED_DARK_MATTER))
    hits = [rec for rec in trace.of("dmg") if rec.get("s") in guids]
    despawn = final_despawn(trace)

    def handled(victim: int, when: int) -> bool:
        held = as_guid(value_at(handler, when))
        return victim == held if held else trace.role(victim) == "tank"

    out = []
    for guid in guids:
        seen = samples.seen(guid)
        if not seen:
            continue
        loose: collections.Counter = collections.Counter()
        loose_from = None
        # uncapped: one that drops out of the sweep is still on whoever it held
        for when, row, weight in samples.unit_steps(guid, cap=None):
            victim = target_of(row)
            if alive(row) and victim and not handled(victim, when):
                loose[victim] += weight
                loose_from = when if loose_from is None else loose_from
        mine = [rec for rec in hits if rec.get("s") == guid]
        out.append({
            "guid": guid,
            "spawn": seen[0],
            "loose": loose,
            "loose_from": loose_from,
            "on_handler": sum(rec.get("a", 0) for rec in mine if handled(rec.get("d", 0), rec["t"])),
            "elsewhere": sum(rec.get("a", 0) for rec in mine if not handled(rec.get("d", 0), rec["t"])),
            "killed": seen[1] if seen[1] < end - GONE_MS and seen[1] != despawn else None,
            "despawned": despawn if seen[1] == despawn else None,
        })
    return out


def show_darkmatter(trace: Trace) -> None:
    print("UNLEASHED DARK MATTER")
    rows = dark_matter_rows(trace)
    if not rows:
        print("  none was sampled")
        return
    show_absent(trace, "algalon.handler")

    for row in rows:
        loose = sum(row["loose"].values())
        on = ", ".join(f"{trace.name(g)} {ms / 1000:.1f} s" for g, ms in row["loose"].most_common(4))
        killed = (f"killed {clock(row['killed'])}" if row["killed"] is not None
                  else f"despawned {clock(row['despawned'])}" if row["despawned"] is not None else "alive at the end")
        print(f"  spawned {clock(row['spawn'])}: loose {loose / 1000:.1f} s{' (' + on + ')' if on else ''},"
              f" {row['on_handler']:,} on the handler, {row['elsewhere']:,} elsewhere, {killed}")


# -------------------------------------------------------------------------------------------- verdict

def expected_probes(trace: Trace) -> dict[str, str]:
    """Contract keys this pull's own events say should have been written, and why. Keys that are
    silent for honest reasons (no backup elected, no hole lost) are left out."""
    samples = samples_of(trace)
    boss = boss_guid(trace)
    out: dict[str, str] = {}
    if boss is not None and boss in samples.rows:
        out["algalon.phase"] = "Algalon was sampled"
        if any(trace.role(guid) in ("heal", "ranged") for guid in roster_guids(trace)):
            out["algalon.slot"] = out["algalon.spot"] = "ring roles were present"
    if big_bang_casts(trace):
        for key in ("algalon.bigbang", "algalon.soaker", "algalon.hide"):
            out[key] = "a Big Bang was cast"
    if any(guid in samples.rows for guid in hole_guids(trace)):
        out["algalon.holes"] = "a hole was sampled"
    if any(guid in samples.rows for guid in guids_of_entry(trace, NPC_COLLAPSING_STAR)):
        out["algalon.starwindow"] = "a star was sampled"
    if any(row["active"] is not None for row in constellation_rows(trace)):
        out["algalon.handler"] = "a constellation woke"
        # Only a bot can be the handler, so with a human tank it stays 0 and nobody kites.
        if any(as_guid(rec.get("txt")) for rec in notes(trace, "algalon.handler")):
            out["algalon.kite"] = "a handler was elected"
    if any(guid in samples.rows for guid in guids_of_entry(trace, NPC_UNLEASHED_DARK_MATTER)):
        out.setdefault("algalon.handler", "Unleashed Dark Matter spawned")
    out.update(tank_probes(trace))
    # Calls go to humans only, so a bot-only raid writes none.
    if big_bang_casts(trace) and any(guid in trace.humans for guid in roster_guids(trace)):
        out["algalon.call"] = "a Big Bang was cast with a human in the raid"
    if star_deaths(trace):
        out["algalon.stardeath"] = "a star died"
    if meteor_beside_bot(trace):
        out["algalon.dodge"] = "a meteor fell beside a bot"
    return out


def tank_probes(trace: Trace) -> dict[str, str]:
    boss = boss_guid(trace)
    if boss is None or boss not in samples_of(trace).rows:
        return {}
    out = {"algalon.swappair": "Algalon was sampled"}
    if any(guid not in trace.humans for guid in swap_pair(trace)[0]):
        out["algalon.tankrole"] = "a bot tank was in the swap pair"
    if intro_end(trace) is not None:
        out["algalon.pull"] = "the intro ended"
    return out


def meteor_beside_bot(trace: Trace) -> bool:
    samples = samples_of(trace)
    bots = [guid for guid in roster_guids(trace) if guid not in trace.humans]
    return any(alive(row := samples.row(guid, wave["seen"])) and to_meteor(wave, row[1:3]) <= DODGE_TRIGGER_YD
               for wave in smash_waves(trace) for guid in bots)


def tank_faults(trace: Trace) -> list[tuple[int, str]]:
    """Verdict lines for his reach at the intro, loose windows, swaps that never came, threat steals."""
    out = []
    pull = pull_row(trace)
    if pull and pull["inside"]:
        out.append((pull["t"], f"{len(pull['inside'])} player(s) inside {MELEE_REACH:.2f} yd of him when the intro"
                               f" ended @ {secs(pull['t'])} ({', '.join(trace.name(g) for g in pull['inside'])})"))
    for window in loose_windows(trace):
        span = window["stop"] - window["start"]
        if window["deaths"] or span >= PICKUP_MS:
            count = len(window["deaths"])
            out.append((window["start"], f"Algalon loose {span / 1000:.1f} s @ {secs(window['start'])}"
                                         f" ({window_cause(window)}): {count} death{'' if count == 1 else 's'}"))
    pair, _ = swap_pair(trace)
    for due in swap_dues(trace):
        if due["taken"] is None:
            partners = ", ".join(trace.name(guid) for guid in sorted(pair - {due["tank"]})) or "nobody"
            out.append((due["t"], f"swap due @ {secs(due['t'])}: {trace.name(due['tank'])} at {SWAP_STACKS} stacks,"
                                  f" {partners} never took him ({due['missed'][1]} @ {secs(due['missed'][0])})"))
    for when, old, new in threat_steals(trace):
        out.append((when, f"threat steal @ {secs(when)}: {trace.name(new)} pulled him off {trace.name(old)}"
                          f" with no taunt"))
    return out


def star_faults(trace: Trace) -> list[tuple[int, str]]:
    out = []
    safe = explosion_safe_hp(trace)
    for row in star_rows(trace):
        at = secs(row["t"])
        if row["gap"] is not None and row["gap"] < STAR_GAP_MS:
            out.append((row["t"], f"stars died {row['gap'] / 1000:.1f} s apart @ {at}"))
        if row["below"]:
            hp, guid, _ = row["below"][0]
            out.append((row["t"], f"star died with {len(row['below'])} raid member(s) under {safe:,} hp"
                                  f" (weakest {trace.name(guid)} {hp:,}) @ {at}"))
        if row["meteor"]:
            out.append((row["t"], f"star died with a Cosmic Smash meteor live @ {at}"))
        if row["bang"]:
            out.append((row["t"], f"star died during Big Bang {row['bang']}'s cast @ {at}"))
        if row["nearest"] and row["nearest"][0] < STAR_NEAR_YD:
            out.append((row["t"], f"star died {row['nearest'][0]:.1f} yd from {trace.name(row['nearest'][1])} @ {at}"))
    return out


def failures(trace: Trace) -> list[tuple[int | None, str]]:
    """`(t, line)` per detected failure; t is None for what has no time of its own."""
    out: list[tuple[int | None, str]] = []
    have = emitted(trace)
    gone = sorted(key for key in expected_probes(trace) if key not in have)
    if gone:
        out.append((None, f"missing probes: {', '.join(gone)}"))

    explained: set[int] = set()
    for row in big_bang_rows(trace):
        n, at = row["n"], row["start"]
        if not row["holes"]:
            out.append((at, f"no hole at Big Bang {n} @ {secs(at)}"))
        if row["elected"] and not row["soaker"]:
            out.append((at, f"Big Bang {n} @ {secs(at)}: no soaker elected"))
        if row["soaker_phased"]:
            out.append((at, f"Big Bang {n} @ {secs(at)}: soaker {trace.name(row['soaker'])} phased at the impact"))
        if row["stray"]:
            count = len(row["stray"])
            out.append((at, f"Big Bang {n} @ {secs(at)}: {count} non-soaker{'s' if count > 1 else ''} hit unphased"
                            f" ({', '.join(trace.name(g) for g in row['stray'])})"))
        if row["soaker_death"] is not None:
            out.append((row["soaker_death"], f"Big Bang {n} @ {secs(at)}: soaker {trace.name(row['soaker'])} died"
                                             f" before the raid returned"))
        if row["nobody"] is not None:
            out.append((row["nobody"], f"Big Bang {n} @ {secs(at)}: nobody alive and unphased within"
                                       f" {RESET_RANGE:.0f} yd at +{(row['nobody'] - row['end']) / 1000:.1f} s"))
        if row["ended"] or row["ascend"] is not None:
            parts = []
            if row["ascend"] is not None:
                explained.add(row["ascend"])
                parts.append(f"Ascend at {(row['ascend'] - row['end']) / 1000:+.1f} s")
            if row["ended"]:
                explained.add(row["ended"][0])
                parts.append(f"pull ended {row['ended'][1]}")
            first = row["ended"][0] if row["ended"] else row["ascend"]
            kind = "evade" if not row["ended"] or row["ended"][1] == "reset" else "wipe"
            out.append((first, f"{kind} {(first - row['end']) / 1000:.1f} s after Big Bang {n} ({', '.join(parts)})"))

    for loop in rephase_loops(trace):
        out.append((loop["start"], f"re-phase loop: {trace.name(loop['guid'])} @ {secs(loop['start'])}"
                                   f" ({applications(loop['applications'])},"
                                   f" {(loop['stop'] - loop['start']) / 1000:.1f} s phased)"))

    out += [(row["start"], f"stray phase ended in a death: {trace.name(row['guid'])} @ {secs(row['start'])}"
                           f" ({row['cause']})")
            for row in phasing_rows(trace) if row["label"] == "stray" and row["died"]]

    out += star_faults(trace)
    team = star_team(trace)
    if team["members"]:
        for guid, held in team["off"].most_common():
            if held >= OFF_TEAM_MS:
                since = team["off_from"][guid]
                out.append((since, f"{trace.name(guid)} on stars for {held / 1000:.1f} s outside the star team,"
                                   f" from {secs(since)}"))

    for rec in notes(trace, "algalon.holelost"):
        if rec.get("txt") == "stray":
            out.append((rec["t"], f"constellation removed by a stray hole @ {secs(rec['t'])}"))
    for row in constellation_rows(trace):
        if row["how"] == "killed":
            out.append((row["removed"], f"constellation killed by damage @ {secs(row['removed'])}"))
        elif row["how"] == "hole" and row["handler"] and row["last_victim"] != row["handler"]:
            out.append((row["removed"], f"constellation removed by {trace.name(row['last_victim'])}, not the"
                                        f" handler, @ {secs(row['removed'])}"))

    out += smash_faults(trace)
    out += tank_faults(trace)

    for when, guid in punched_out(trace):
        out.append((when, f"Phase Punch phased {trace.name(guid)} out @ {secs(when)}"))
    for start, stop, guid in pickups(trace):
        if stop - start >= PICKUP_MS:
            out.append((start, f"Algalon on {who(trace, guid)} for {(stop - start) / 1000:.1f} s @ {secs(start)}"))
    boss = boss_guid(trace)
    if boss is not None:
        for when, row, _ in samples_of(trace).unit_steps(boss):
            if dist2(row[1:3], ROOM_CENTER) > ROOM_RADIUS:
                out.append((when, f"Algalon {dist2(row[1:3], ROOM_CENTER):.1f} yd from the room centre"
                                  f" @ {secs(when)}"))
                break

    for row in dark_matter_rows(trace):
        loose = sum(row["loose"].values())
        if loose >= LOOSE_MS:
            victim = row["loose"].most_common(1)[0][0]
            out.append((row["loose_from"], f"Unleashed Dark Matter loose for {loose / 1000:.1f} s (mostly on"
                                           f" {trace.name(victim)}) @ {secs(row['loose_from'])}"))

    p1 = phase_start(trace, "P1")
    for rec in trace.of("cast"):
        if rec.get("sp") == SPELL_ASCEND and rec["t"] not in explained:
            enraged = p1 is not None and rec["t"] >= p1 + ENRAGE_MS - 2000
            out.append((rec["t"], f"Ascend to the Heavens @ {secs(rec['t'])}{' (enrage)' if enraged else ''}"))
    finish = trace.of("end")[-1] if trace.of("end") else None
    if finish and finish.get("out") == "reset" and finish["t"] not in explained:
        out.append((finish["t"], f"pull ended reset @ {secs(finish['t'])}, not after a Big Bang"))

    return sorted(out, key=lambda item: (item[0] is not None, item[0] or 0))


def show_verdict(trace: Trace) -> None:
    print("VERDICT")
    if boss_guid(trace) is None and not big_bang_casts(trace):
        print("  Algalon was never sampled, nothing to judge")
        return
    rows = failures(trace)
    if not rows:
        print("  no failures detected")
        return
    for _, line in rows:
        print(f"  {line}")


SECTIONS = (
    ("phases", "phase spans, deaths per phase, boss health at each change, the enrage", show_phases),
    ("pull", "the intro's end: who stood in his reach, his first victim and swings", show_pull),
    ("deaths", "every death by the mechanic that killed it, cause by role", show_deaths),
    ("loose", "every stretch he spent off the swap pair: cause, hits, deaths, who took him back", show_loose),
    ("swaps", "the swap pair, Phase Punch, swaps due and their latency, calls, threat steals", show_swaps),
    ("reach", "seconds each raid member outside the pair stood alive and unphased in his reach", show_reach),
    ("bigbang", "every Big Bang: holes, soaker, who it hit, tank-alone time, re-phase loops", show_bigbang),
    ("phasing", "every hole phase: Big Bang or stray, what caused a stray one, Dark Matter, deaths", show_phasing),
    ("stars", "star deaths, what each landed into, the kill window and the star team", show_stars),
    ("holes", "hole stock, empty seconds before each Big Bang, holes lost, urgent spans", show_holes),
    ("constellations", "activations, victims, handler and kite branches, removals", show_constellations),
    ("smash", "every Cosmic Smash wave: who stood near it, the dodge, distance at the impact", show_smash),
    ("tanks", "Phase Punch stacks, swaps, non-tank pickups, Quantum Strike", show_tanks),
    ("formation", "each ring bot's distance to its slot and to him, spot branches, stalls", show_formation),
    ("darkmatter", "phase 2 Unleashed Dark Matter: loose seconds, handler damage, kills", show_darkmatter),
    ("verdict", "one line per detected failure, in time order", show_verdict),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
