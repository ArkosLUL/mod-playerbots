#!/usr/bin/env python3
"""Score an Algalon the Observer pull from a RaidObs trace: Big Bang, stars, holes, constellations.

    algalon.py <file>                   every section
    algalon.py <file> --phases          phase spans, deaths per phase, boss health at each change, enrage
    algalon.py <file> --bigbang         every Big Bang: holes, soaker and backup, who it hit, what followed
    algalon.py <file> --stars           star deaths, their spacing and raid health, the kill window, the team
    algalon.py <file> --holes           hole stock, empty seconds before each Big Bang, holes lost, urgency
    algalon.py <file> --constellations  activations, victims, the handler and kite branches, removals
    algalon.py <file> --smash           every Cosmic Smash marker: who stood near it, damage by distance
    algalon.py <file> --tanks           Phase Punch stacks, swaps, non-tank pickups, Quantum Strike
    algalon.py <file> --formation       each ring bot's distance to its slot, spot branches, stalls
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
  slot around `ULDUAR_ALGALON_TANK_SLOT`; `--formation` measures from the slot.
- **A swept unit can drop out of the snapshots and come back** on traces from before the sweep's
  anchor preferred an unphased player. A hole, a constellation or a Dark Matter counts as present
  from its first row to its last, gaps included.
- **While `algalon.urgent` reads 1 every ranged dps is on the star team** without being written into
  `algalon.starteam`, so star time there is not off-team.
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
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.geometry import Unknown, anchor, dist2, first_seen, frames, guids_of_entry, radius  # noqa: E402
from raidobs.probes import emitted_keys, holder_spans, latch_spans, silent_keys  # noqa: E402
from raidobs.space import show_share, threat_share  # noqa: E402
from raidobs.stuck import stall_windows  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, death_records, notes, roster_guids  # noqa: E402
from raidobs.validity import DIFFICULTY  # noqa: E402

ENCOUNTER = "algalon"

NPC_ALGALON = 32871
NPC_COLLAPSING_STAR = 32955
NPC_BLACK_HOLE = 32953
NPC_WORM_HOLE = 34099
NPC_LIVING_CONSTELLATION = 33052
NPC_COSMIC_SMASH_MARKERS = (33104, 33105)
NPC_UNLEASHED_DARK_MATTER = 34097
NPC_DARK_MATTER = 33089
HOLE_ENTRIES = (NPC_BLACK_HOLE, NPC_WORM_HOLE)

SPELL_BIG_BANG = (64443, 64584)
SPELL_PHASE_PUNCH = 64412
SPELL_PHASE_PUNCH_PHASE = 64417
SPELL_BLACK_HOLE_PHASE = 62168
SPELL_WORM_HOLE_PHASE = 65250
HOLE_PHASE = (SPELL_BLACK_HOLE_PHASE, SPELL_WORM_HOLE_PHASE)
PHASE_AURAS = HOLE_PHASE + (SPELL_PHASE_PUNCH_PHASE,)
SPELL_QUANTUM_STRIKE = (64395, 64592)
SPELL_COSMIC_SMASH = (62311, 64596)
SPELL_BLACK_HOLE_EXPLOSION = (64122, 65108)
SPELL_ASCEND = 64487
SPELL_DISPERSION = 47585
SPELL_PAIN_SUPPRESSION = 33206
SPELL_GUARDIAN_SPIRIT = 47788
SOAK_SPELLS = (SPELL_DISPERSION, SPELL_PAIN_SUPPRESSION, SPELL_GUARDIAN_SPIRIT)

# Read from the source so a retune shows up here without a second edit.
ROOM_CENTER = anchor("ULDUAR_ALGALON_ROOM_CENTER")
TANK_SLOT = anchor("ULDUAR_ALGALON_TANK_SLOT")
ROOM_RADIUS = radius("ULDUAR_ALGALON_ROOM_RADIUS")
SHELTER_RADIUS = radius("ULDUAR_ALGALON_SHELTER_RADIUS")


def _ring(name: str, slots: int) -> tuple[float, float, float, int]:
    return (radius(f"ULDUAR_ALGALON_{name}_RADIUS"), radius(f"ULDUAR_ALGALON_{name}_ARC_CENTER"),
            radius(f"ULDUAR_ALGALON_{name}_ARC_WIDTH"), slots)


# slot counts are uint8, which the float parser can't read. Without the ring constants --formation
# falls back to the tank slot.
try:
    RINGS = (_ring("HEALER", 4), _ring("RANGED_INNER", 6), _ring("RANGED_OUTER", 8))
except Unknown:
    RINGS = ()

# uint32 in the source, or not named there at all.
BIG_BANG_CAST_MS = 8000
SHELTER_WINDOW_MS = 30000
STAR_GAP_MS = 8000
STAR_RAID_HP = 80.0
ENRAGE_MS = 360000
PHASE_AURA_MS = 10000
LOOP_MS = 10500
P2_HP = 20.0
# He resets whenever no living, unphased player is inside this.
RESET_RANGE = 120.0
SMASH_DELAY_MS = 4800

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
# 33104 and 33105 mark the same impact
SMASH_SAME_YD = 1.0
SMASH_SAME_MS = 1000
SMASH_BANDS = ((6.0, "<6"), (10.0, "6-10"), (15.0, "10-15"), (math.inf, "15+"))
STALL_MS = 3000
PICKUP_MS = 1000
LOOSE_MS = 3000
OFF_TEAM_MS = 3000
KILLED_HP = 5.0
MAX_STEP_MS = 2000
TIMELINE_SHOWN = 16


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
    guids = sorted(guids_of_entry(trace, NPC_ALGALON))
    return guids[0] if guids else None


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
    return [key for key, _, _ in silent_keys(emitted_keys(trace), encounter_of(trace))]


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


def phased_spans(trace: Trace, spells=PHASE_AURAS) -> dict[int, list[list[int]]]:
    """Per player, `[start, stop, applications]` per stretch it wore any of `spells`. A row while the
    aura is held is the field's once-a-second refresh, never a new application; only an application
    within CHAIN_MS of a removal adds one. A stretch still open when the file ends runs to the end."""
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
            if spans and spans[-1][1] is not None and when - spans[-1][1] <= CHAIN_MS:
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


def show_stars(trace: Trace) -> None:
    print("COLLAPSING STARS")
    stars = guids_of_entry(trace, NPC_COLLAPSING_STAR)
    deaths = star_deaths(trace)
    if not stars and not deaths:
        print("  no star was sampled")
        return
    show_absent(trace, "algalon.starwindow", "algalon.focusstar", "algalon.starteam")

    print(f"  {len(stars)} star(s) sampled, {len(deaths)} died")
    if deaths:
        print(f"  {'at':>9} {'star':18} {'how':9} {'hits':>4} {'damage':>9} {'gap':>7} {'raid min':>8}")
    for death in deaths:
        gap = "-" if death["gap"] is None else f"{death['gap'] / 1000:.1f}s"
        flag = " <8s" if death["gap"] is not None and death["gap"] < STAR_GAP_MS else ""
        low = "-" if death["raid_min"] is None else f"{death['raid_min']:.0f}%"
        flag += f" <{STAR_RAID_HP:.0f}%" if death["raid_min"] is not None and death["raid_min"] < STAR_RAID_HP else ""
        print(f"  {clock(death['t']):>9} {trace.name(death['star'])[:18]:18} {death['how']:9} {death['hits']:4}"
              f" {death['damage']:9,} {gap:>7} {low:>8}{flag}")

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

def band(distance: float) -> str:
    return next(name for edge, name in SMASH_BANDS if distance < edge)


def smash_rows(trace: Trace) -> tuple[list[dict], list[dict]]:
    """Every impact with where the raid stood at it, and the damage rows that no impact explains. Both
    marker entries can stand on one impact, so markers on the same spot at the same moment are one."""
    samples = samples_of(trace)
    roster = roster_guids(trace)
    guids = {g for entry in NPC_COSMIC_SMASH_MARKERS for g in guids_of_entry(trace, entry)}
    firsts = sorted((seen[0], guid) for guid in guids if (seen := samples.seen(guid)))
    markers = []
    for appeared, guid in firsts:
        spot = samples.row(guid, appeared)[1:3]
        if any(appeared - m["seen"] <= SMASH_SAME_MS and dist2(spot, m["spot"]) <= SMASH_SAME_YD for m in markers):
            continue
        impact = appeared + SMASH_DELAY_MS
        near = {}
        for member in roster:
            then, first = samples.row(member, impact), samples.row(member, appeared)
            if alive(then):
                near[member] = (dist2(then[1:3], spot), dist2(first[1:3], spot) if first else None)
        markers.append({"guid": guid, "seen": appeared, "impact": impact, "spot": spot, "near": near, "hits": []})

    stray = []
    for rec in trace.of("dmg"):
        if rec.get("sp") not in SPELL_COSMIC_SMASH:
            continue
        candidates = [m for m in markers if abs(rec["t"] - m["impact"]) <= SMASH_MATCH_MS]
        victim = samples.row(rec.get("d"), rec["t"])
        if not candidates or not victim:
            stray.append(rec)
            continue
        marker = min(candidates, key=lambda m: dist2(victim[1:3], m["spot"]))
        marker["hits"].append((rec, dist2(victim[1:3], marker["spot"])))
    return markers, stray


def show_smash(trace: Trace) -> None:
    print("COSMIC SMASH")
    markers, stray = smash_rows(trace)
    if not markers and not stray:
        print("  no marker was sampled and nothing took Cosmic Smash damage")
        return

    by_band: dict[str, list[int]] = collections.defaultdict(list)
    for marker in markers:
        near = marker["near"]
        counts = "/".join(str(sum(1 for d, _ in near.values() if d < edge)) for edge in (6.0, 10.0, 15.0))
        dodged = [trace.name(g) for g, (d, f) in near.items() if f is not None and f < 10.0 <= d]
        stayed = [trace.name(g) for g, (d, f) in near.items() if d < 6.0]
        print(f"  {clock(marker['seen'])} marker, impact {clock(marker['impact'])} at"
              f" ({marker['spot'][0]:.1f},{marker['spot'][1]:.1f}): within 6/10/15 yd {counts}")
        print(f"      dodged out of 10 yd: {', '.join(sorted(dodged)) or 'nobody'};"
              f" inside 6 yd at impact: {', '.join(sorted(stayed)) or 'nobody'}")
        if marker["hits"]:
            print("      hit: " + ", ".join(f"{trace.name(rec.get('d'))} {rec.get('a', 0):,} @{d:.1f}"
                                          for rec, d in marker["hits"]))
        for rec, distance in marker["hits"]:
            by_band[band(distance)].append(rec.get("a", 0))

    if by_band:
        print("  damage by distance: " + ", ".join(
            f"{name} {len(by_band[name])} hit(s) {sum(by_band[name]):,}" for _, name in SMASH_BANDS if by_band[name]))
    if stray:
        print(f"  {len(stray)} Cosmic Smash hit(s) no sampled marker explains: "
              + ", ".join(f"{clock(rec['t'])} {trace.name(rec.get('d'))} {rec.get('a', 0):,}" for rec in stray[:8]))


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

def arc_offset(index: int, count: int, width: float) -> float:
    """Centre-out around the arc's middle, alternating sides: the same order the strategy fills in."""
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


def slot_position(index: int) -> tuple[float, float] | None:
    for ring_radius, centre, width, count in RINGS:
        if index < count:
            bearing = centre + arc_offset(index, count, width)
            return (TANK_SLOT[0] + math.cos(bearing) * ring_radius, TANK_SLOT[1] + math.sin(bearing) * ring_radius)
        index -= count
    return None


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
    held = slots(trace)
    members = set(held) or {guid for guid in roster_guids(trace) if trace.role(guid) in ("heal", "ranged")}
    spot = holder_spans(trace, "algalon.spot", end)
    phased = phased_spans(trace)
    stalls = stall_windows(trace, STALL_MS)

    out = []
    for guid in sorted(members, key=trace.name):
        slot = held.get(guid)
        target = slot_position(slot) if slot is not None else None
        to_slot, to_tank = [], []
        for _, when, _ in samples.steps(0, end):
            row = samples.row(guid, when)
            if not alive(row) or phased_at(phased, guid, when):
                continue
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
    if not RINGS:
        print("  ring constants not found in the source: distance to the tank slot only")

    rows = ring_rows(trace)
    if not rows:
        print("  no ring bot")
        return
    print(f"  {'bot':14} {'role':6} {'slot':>4} {'to slot':>8} {'to tank slot':>12} {'stalls':>10}  spot")
    for row in rows:
        total = sum(row["branches"].values())
        shares = ", ".join(f"{value} {held * 100.0 / total:.0f}%" for value, held in row["branches"].most_common()) \
            if total else "-"
        to_slot = "-" if row["to_slot"] is None else f"{row['to_slot']:.1f}"
        to_tank = "-" if row["to_tank"] is None else f"{row['to_tank']:.1f}"
        slot = "-" if row["slot"] is None else str(row["slot"])
        stalled = f"{row['stalls']} / {row['stalled'] / 1000:.0f}s" if row["stalls"] else "-"
        print(f"  {trace.name(row['guid'])[:14]:14} {trace.role(row['guid']):6} {slot:>4} {to_slot:>8}"
              f" {to_tank:>12} {stalled:>10}  {shares}")
    print("  medians over living, unphased samples, and only while algalon.spot read slot where it exists;"
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

    for death in star_deaths(trace):
        if death["gap"] is not None and death["gap"] < STAR_GAP_MS:
            out.append((death["t"], f"stars died {death['gap'] / 1000:.1f} s apart @ {secs(death['t'])}"))
        if death["raid_min"] is not None and death["raid_min"] < STAR_RAID_HP:
            out.append((death["t"], f"star died with the raid's weakest at {death['raid_min']:.0f}%"
                                    f" @ {secs(death['t'])}"))
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

    for marker in smash_rows(trace)[0]:
        inside = sorted(trace.name(g) for g, (d, _) in marker["near"].items() if d < 6.0)
        if inside:
            out.append((marker["impact"], f"Cosmic Smash @ {secs(marker['impact'])}: {', '.join(inside)}"
                                          f" inside 6 yd"))

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
    ("bigbang", "every Big Bang: holes, soaker, who it hit, tank-alone time, re-phase loops", show_bigbang),
    ("stars", "star deaths, spacing, raid health, the kill window and the star team", show_stars),
    ("holes", "hole stock, empty seconds before each Big Bang, holes lost, urgent spans", show_holes),
    ("constellations", "activations, victims, handler and kite branches, removals", show_constellations),
    ("smash", "Cosmic Smash markers, who stood near each impact, damage by distance", show_smash),
    ("tanks", "Phase Punch stacks, swaps, non-tank pickups, Quantum Strike", show_tanks),
    ("formation", "each ring bot's distance to its slot, spot branches, stalls", show_formation),
    ("darkmatter", "phase 2 Unleashed Dark Matter: loose seconds, handler damage, kills", show_darkmatter),
    ("verdict", "one line per detected failure, in time order", show_verdict),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
