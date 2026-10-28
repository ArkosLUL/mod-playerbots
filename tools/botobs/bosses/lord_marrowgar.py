#!/usr/bin/env python3
"""Read a Lord Marrowgar pull from a RaidObs trace: Bone Storm, the duties, the spikes, the dodges.

    lord_marrowgar.py <file>            every section
    lord_marrowgar.py <file> --phases   lm.phase: each Bone Storm cast and storm, from the first bot to note it
    lord_marrowgar.py <file> --duties   who tanked, stacked, stood inside him and baited Coldflame
    lord_marrowgar.py <file> --storms   each storm's anchors, and who stood on an anchor or the stack
    lord_marrowgar.py <file> --spikes   the spike rule each bot followed
    lord_marrowgar.py <file> --dodges   the Coldflame dodge branches each bot took
    lord_marrowgar.py <file> --defensives  the tank buttons spent as Bone Slice resumes, calls to a human tank
    lord_marrowgar.py <file> --covers   the raid cooldowns dealt and cast in each Bone Storm
    lord_marrowgar.py <file> --cheats   this encounter's cheat uses

What the generic views miss here:

- **`lm.duty` and `lm.anchor` are the roster, rebuilt once a second while he is in progress.** A value
  is the duty number (1 main tank, 2 off-tank, 3 inside him, 4 Coldflame bait) or the anchor slot (1 for
  180 degrees round the storm stack, every tank's, 2 and 3 for 60 and 300); `0` is a bot that lost it,
  dead or gone.
- **`lm.stand` is the stand a bot holds, `none` while it holds none.** A melee or caster on a spike
  holds none, so reach can walk it; so does everyone while he walks between stands.
- **`lm.spike` is the rule that picked, not what was hit.** `skull` a player's mark, `melee` a spike in
  reach, `far` a ranged on a spike no melee reaches, `spread` a ranged helping on the others, `boss`
  him.
- **`lm.dodge` writes `clear` whenever a bot is out of every patch**, so a dodge is the row before it:
  `move N` (yards walked), `tight` (the wide clearance was full), `cross` (the walk crosses another
  line, taken only when no walk is clear), `hold` (still walking), `none`, `locked`, `stunned`.
- **Bone Slice windows are derived, not noted.** He swings plain for 10 s after the engage and after
  each storm ends, so a window opens 10 s into every `normal` phase span. `lm.defensive` is the button
  a main tank or off-tank was offered then: a cast name, `covered` once one runs, `none` with nothing
  ready, `idle` outside a window. The casts themselves are the `cast` rows.
- **`icc.cover` is the deal, not the cast**: one holder per raid cooldown, written when a window of
  three Bone Storm ticks is about to start (3 s before it). Anti-Magic Zone is never cast, Bone Storm
  being physical.
"""
from __future__ import annotations

import collections
import pathlib
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of, prefix_matches_boss  # noqa: E402
from raidobs.probes import holder_spans, latch_spans  # noqa: E402
from raidobs.trace import Trace, cheat_uses, clock, combat_deaths, notes, roster_guids  # noqa: E402

SLUG = "lord-marrowgar"
NAME = "Lord Marrowgar"
PHASE = "lm.phase"
STORM = "bonestorm"
NORMAL = "normal"
DUTY = "lm.duty"
ANCHOR = "lm.anchor"
STAND = "lm.stand"
SPIKE = "lm.spike"
DODGE = "lm.dodge"
DEFENSIVE = "lm.defensive"
HUMAN_CALL = "lm.humancall"
COVER = "icc.cover"
# Berserk, and when Bone Slice resumes after the engage and after a storm (EVENT_ENABLE_BONE_SLICE)
BERSERK_MS = 600000
SLICE_DELAY_MS = 10000
# a tank button counts for a window from this long before Bone Slice resumes until this long after
WINDOW_LEAD_MS = 1500
WINDOW_SPAN_MS = 7000
# a cover cast counts for a storm from this long before it starts
COVER_LEAD_MS = 3000
# icc.cover values, RaidCover's order
COVER_KINDS = {1: "aura mastery", 2: "divine sacrifice", 3: "anti magic zone", 4: "divine hymn", 5: "tranquility"}
# cast names, any rank; Divine Guardian is Divine Sacrifice's proc
RAID_COVERS = frozenset({"Aura Mastery", "Divine Sacrifice", "Divine Guardian", "Anti-Magic Zone", "Divine Hymn",
                         "Tranquility"})
TANK_BUTTONS = frozenset({"Last Stand", "Shield Wall", "Divine Protection", "Barkskin", "Frenzied Regeneration",
                          "Survival Instincts", "Anti-Magic Shell", "Vampiric Blood", "Icebound Fortitude"})
DUTIES = {"1": "main tank", "2": "off-tank", "3": "inside", "4": "bait"}
ANCHOR_SLOTS = {"1": "180", "2": "60", "3": "300"}
STORM_STANDS = ("anchor", "stack")
DIFFICULTIES = {0: "10N", 1: "25N", 2: "10H", 3: "25H"}


def pull_end(trace: Trace) -> int:
    ends = trace.of("end")
    if ends:
        return ends[-1]["t"]
    return trace.records[-1].get("t", 0) if trace.records else 0


def mine(trace: Trace) -> bool:
    return encounter_of(trace) == SLUG


def phase_steps(trace: Trace) -> list[tuple[str, int, int]]:
    return latch_spans(trace, PHASE, pull_end(trace))


def storms(trace: Trace) -> list[tuple[int, int]]:
    return [(start, stop) for value, start, stop in phase_steps(trace) if value == STORM]


def slice_windows(trace: Trace) -> list[int]:
    """When Bone Slice resumes: 10 s into every `normal` span, unless the next storm cast comes first."""
    return [start + SLICE_DELAY_MS for value, start, stop in phase_steps(trace)
            if value == NORMAL and start + SLICE_DELAY_MS < stop]


def casts_named(trace: Trace, names: frozenset) -> list[tuple[int, int, str]]:
    """`(t, guid, spell name)` for every roster cast of one of `names`, once per ms."""
    roster = roster_guids(trace)
    named = ((rec["t"], rec.get("s"), trace.spells.get(rec.get("sp"), "")) for rec in trace.of("cast")
             if rec.get("s") in roster)
    return sorted({cast for cast in named if cast[2] in names})


def holders_at(trace: Trace, key: str, when: int) -> dict[int, str]:
    """`{guid: value}` for every bot whose `key` held a value other than `0` at `when`."""
    out = {}
    for guid, spans in holder_spans(trace, key, pull_end(trace)).items():
        for value, start, stop in spans:
            if start <= when < stop and value != "0":
                out[guid] = value
    return out


def anchors(trace: Trace, start: int, stop: int) -> dict[str, set[int]]:
    """`{angle: guids}` in slot order for the anchors assigned when the storm in `[start, stop)` began."""
    held = holders_at(trace, ANCHOR, start)
    return {ANCHOR_SLOTS.get(value, value): {guid for guid, slot in held.items() if slot == value}
            for value in sorted(set(held.values()))}


def storm_stands(trace: Trace, start: int, stop: int) -> dict[str, set[int]]:
    """Per storm stand, the bots that held it at some point of the storm."""
    out: dict[str, set[int]] = collections.defaultdict(set)
    for guid, spans in holder_spans(trace, STAND, pull_end(trace)).items():
        for value, span_start, span_stop in spans:
            if value in STORM_STANDS and span_start < stop and span_stop > start:
                out[value].add(guid)
    return dict(out)


def held_values(trace: Trace, key: str, skip: tuple[str, ...] = ()) -> dict[str, set[int]]:
    """Per value of a per-bot key, the bots that held it at some point."""
    out: dict[str, set[int]] = collections.defaultdict(set)
    for guid, spans in holder_spans(trace, key, pull_end(trace)).items():
        for value, _, _ in spans:
            if value not in skip:
                out[value].add(guid)
    return dict(out)


def duties(trace: Trace) -> dict[str, set[int]]:
    return {DUTIES.get(value, value): guids for value, guids in held_values(trace, DUTY, ("0",)).items()}


def dodges(trace: Trace) -> dict[int, collections.Counter]:
    """Per bot, how often it took each dodge branch. `move N` counts under `move`."""
    out: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
    for rec in notes(trace, DODGE):
        value = str(rec.get("txt", ""))
        if value != "clear":
            out[rec["g"]][value.split(" ")[0]] += 1
    return dict(out)


def own_cheats(trace: Trace) -> list[tuple[str, dict]]:
    rows = [(key, entry) for key, entry in cheat_uses(trace).items()
            if prefix_matches_boss(key.partition(".")[0], SLUG)]
    return sorted(rows, key=lambda row: (row[1]["first"], row[0]))


def names(trace: Trace, guids) -> str:
    return ", ".join(sorted(trace.name(guid) for guid in guids)) or "nobody"


def show_banner(trace: Trace) -> None:
    end = pull_end(trace)
    outcome = trace.of("end")[-1].get("out", "?") if trace.of("end") else "no end record"
    diff = DIFFICULTIES.get(trace.header.get("diff"), "?")
    print(f"{encounter_of(trace)} {diff}  {outcome} at {clock(end)}  {len(combat_deaths(trace))} death(s)")
    if not mine(trace):
        print(f"  not a {NAME} pull, every section below reads empty")


def show_phases(trace: Trace) -> None:
    print("PHASES")
    if not mine(trace):
        return
    steps = phase_steps(trace)
    if not steps:
        print(f"  {PHASE} never written")
        return
    print(f"    {'from':>9}  {'held':>9}  phase")
    for value, start, stop in steps:
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {value}")

    end = pull_end(trace)
    spare = f"{clock(BERSERK_MS - end)} to spare" if end < BERSERK_MS else f"{clock(end - BERSERK_MS)} past it"
    print(f"  Berserk at {clock(BERSERK_MS)}, pull ended at {clock(end)}: {spare}")


def show_duties(trace: Trace) -> None:
    print("DUTIES")
    if not mine(trace):
        return
    held = duties(trace)
    if not held:
        print(f"  {DUTY} never written")
        return
    for value in sorted(held):
        print(f"  {value:<10} {names(trace, held[value])}")


def show_storms(trace: Trace) -> None:
    print("BONE STORMS")
    if not mine(trace):
        return
    spans = storms(trace)
    if not spans:
        print("  no Bone Storm in this pull")
        return
    for index, (start, stop) in enumerate(spans, 1):
        slots = anchors(trace, start, stop)
        drawn = "; ".join(f"{slot}: {names(trace, guids)}" for slot, guids in slots.items()) or "none"
        stood = storm_stands(trace, start, stop)
        print(f"  {index}  {clock(start)} for {clock(stop - start)}  anchors {drawn}")
        for stand in STORM_STANDS:
            print(f"       on the {stand}: {names(trace, stood.get(stand, ()))}")


def show_spikes(trace: Trace) -> None:
    print("SPIKES")
    if not mine(trace):
        return
    held = held_values(trace, SPIKE)
    if not held:
        print(f"  {SPIKE} never written")
        return
    for value in sorted(held):
        print(f"  {value:<8} {names(trace, held[value])}")


def show_dodges(trace: Trace) -> None:
    print("COLDFLAME DODGES")
    if not mine(trace):
        return
    rows = dodges(trace)
    if not rows:
        print("  no bot dodged")
        return
    for guid, counts in sorted(rows.items(), key=lambda row: trace.name(row[0])):
        line = "  ".join(f"{branch} {count}" for branch, count in sorted(counts.items()))
        print(f"  {trace.name(guid):<16} {line}")


def show_defensives(trace: Trace) -> None:
    print("TANK DEFENSIVES")
    if not mine(trace):
        return
    opens = slice_windows(trace)
    if not opens:
        print("  no Bone Slice window in this pull")
        return
    casts = casts_named(trace, TANK_BUTTONS)
    picks = [rec for rec in notes(trace, DEFENSIVE) if rec.get("txt") != "idle"]
    spent_in = set()
    covered = 0
    for index, opened in enumerate(opens, 1):
        low, high = opened - WINDOW_LEAD_MS, opened + WINDOW_SPAN_MS
        spent = [cast for cast in casts if low <= cast[0] <= high]
        spent_in.update(spent)
        covered += bool(spent)
        offered = collections.Counter(f"{trace.name(rec['g'])} {rec['txt']}" for rec in picks
                                      if low <= rec["t"] <= high)
        print(f"  {index}  Bone Slice from {clock(opened)}")
        print("      cast     " + (", ".join(f"{name} {trace.name(guid)} {(when - opened) / 1000:+.1f}s"
                                              for when, guid, name in spent) or "none"))
        if offered:
            print("      offered  " + ", ".join(f"{what} x{count}" for what, count in sorted(offered.items())))
    elsewhere = [cast for cast in casts if cast not in spent_in]
    if elsewhere:
        print("  off the windows  " + ", ".join(f"{name} {trace.name(guid)} {clock(when)}"
                                                 for when, guid, name in elsewhere))
    calls = notes(trace, HUMAN_CALL)
    if calls:
        print("  calls to a human tank  " + ", ".join(f"{rec['txt']} {clock(rec['t'])}" for rec in calls))
    print(f"\n  {covered} of {len(opens)} windows saw a tank button; a window opens 10 s into each normal phase")


def show_covers(trace: Trace) -> None:
    print("STORM COVER")
    if not mine(trace):
        return
    spans = storms(trace)
    if not spans:
        print("  no Bone Storm in this pull")
        return
    casts = casts_named(trace, RAID_COVERS)
    deals = [rec for rec in notes(trace, COVER) if str(rec.get("txt", "")).isdigit() and int(rec["txt"])]
    for index, (start, stop) in enumerate(spans, 1):
        low = start - COVER_LEAD_MS
        raid = [cast for cast in casts if low <= cast[0] <= stop]
        dealt = [(rec.get("g"), COVER_KINDS.get(int(rec["txt"]), rec["txt"])) for rec in deals
                 if low <= rec["t"] <= stop]
        print(f"  {index}  {clock(start)} for {clock(stop - start)}")
        print("      dealt    " + (", ".join(f"{trace.name(guid)} {kind}" for guid, kind in dealt) or "none"))
        print("      cast     " + (", ".join(f"{name} {trace.name(guid)} {(when - start) / 1000:+.1f}s"
                                              for when, guid, name in raid) or "none"))


def show_cheats(trace: Trace) -> None:
    print("CHEATS")
    if not mine(trace):
        return
    rows = own_cheats(trace)
    if not rows:
        print("  no lm.* or icc.* cheat fired")
        return
    for key, entry in rows:
        print(f"  {key}  {entry['n']} use(s) from {clock(entry['first'])}  by {names(trace, entry['bots'])}")


SECTIONS = (
    ("phases", "lm.phase: each Bone Storm cast and storm, from the first bot to note it", show_phases),
    ("duties", "who tanked, stacked, stood inside him and baited Coldflame", show_duties),
    ("storms", "each storm's anchors, and who stood on an anchor or the stack", show_storms),
    ("spikes", "the spike rule each bot followed", show_spikes),
    ("dodges", "the Coldflame dodge branches each bot took", show_dodges),
    ("defensives", "the tank buttons spent as Bone Slice resumes, calls to a human tank", show_defensives),
    ("covers", "the raid cooldowns dealt and cast in each Bone Storm", show_covers),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
