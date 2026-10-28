#!/usr/bin/env python3
"""Read a Deathbringer Saurfang pull from a RaidObs trace: stands, picks, swaps, novas, beasts and marks.

    deathbringer_saurfang.py <file>             every section
    deathbringer_saurfang.py <file> --phases    dbs.phase: 1, then 2 once he is in Frenzy
    deathbringer_saurfang.py <file> --slots     each bot's stand: a slot, the stack or a tank spot
    deathbringer_saurfang.py <file> --targets   what the resolver had the bots on, by rule
    deathbringer_saurfang.py <file> --swaps     each Rune of Blood, the swap taunt after it and the rune tank's yield
    deathbringer_saurfang.py <file> --novas     each Blood Nova and how many players it hit
    deathbringer_saurfang.py <file> --kills     each Blood Beast and the ranged on its kill duty
    deathbringer_saurfang.py <file> --control   each control duty on a beast
    deathbringer_saurfang.py <file> --marks     each Mark of the Fallen Champion, its healer and fate
    deathbringer_saurfang.py <file> --defensives  tank buttons, calls to a human tank, the marked bots' own buttons
    deathbringer_saurfang.py <file> --protect   each Hand of Protection duty and whether the hand went out
    deathbringer_saurfang.py <file> --cheats    this encounter's cheat uses

What the generic views miss here:

- **`dbs.slot` counts from 1.** 1-11 are the spread slots (9 and under west of him, 10 and 11 on the
  east causeway), 12 the stack behind him, 13-15 the tank spots; `0` is a release, never a slot. A melee
  bot on a slot is a fill: too few ranged and healers were out to give Blood Nova victims.
- **`dbs.defensive` is the tank ladder's pick**, a cast name, `covered` once one runs, `none` with
  nothing ready, `idle` outside a window. A window is the holder or a Mark carrier from Blood Power 70
  (40 with a Mark) once per cycle, the same all through Frenzy, or any tank under 40%. Blood Power is not
  traced: Frenzy is `dbs.phase` 2, a cycle ends at each Mark.
- **`icc.personal` is shared with the other raids.** Here it is a marked bot's pick at Blood Power 70
  or in Frenzy.
- **A swap is the elected tank's taunt.** `dbs.swap` goes `1` for the bot the rule elects once the rune
  sits on the tank holding him; the taunt is its `icc dbs rune swap` act row. `dbs.yield` then goes `1`
  on the rune tank while it stays off him.
- **One Blood Nova is one stamp of damage rows.** Its victims all take 72380-72440 in the same update, so
  a nova that hit two players is two rows within a few ms.
- **`dbs.markheal` is keyed on the marked player**, its value the healer, so one healer can carry two.
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
from raidobs.trace import Trace, cheat_uses, clock, combat_deaths, first_deaths, notes, roster_guids  # noqa: E402

SLUG = "deathbringer-saurfang"
NAME = "Deathbringer Saurfang"
PHASE = "dbs.phase"
SLOT = "dbs.slot"
TARGET = "dbs.target"
SWAP = "dbs.swap"
YIELD = "dbs.yield"
KILL = "dbs.kill"
CONTROL = "dbs.cc"
MARKHEAL = "dbs.markheal"
DEFENSIVE = "dbs.defensive"
HUMAN_CALL = "dbs.humancall"
PROTECT = "dbs.protect"
PERSONAL = "icc.personal"
SWAP_ACTION = "icc dbs rune swap"
SPELL_RUNE_OF_BLOOD = 72410
SPELL_MARK_OF_THE_FALLEN_CHAMPION = 72293
BLOOD_NOVA = {72380, 72438, 72439, 72440}
RUNE_MS = 20000
NOVA_WINDOW_MS = 200
STACK = 12
FIRST_TANK_SPOT = 13
# a button counts for a window from this long before it opens until this long after it closes
WINDOW_LEAD_MS = 1500
WINDOW_TAIL_MS = 1500
# a Hand of Protection cast counts for a duty from this long before it opens until this long after it ends
PROTECT_LEAD_MS = 500
PROTECT_TAIL_MS = 2500
# cast names, any rank
TANK_BUTTONS = frozenset({"Last Stand", "Shield Wall", "Divine Protection", "Barkskin", "Frenzied Regeneration",
                          "Survival Instincts", "Anti-Magic Shell", "Vampiric Blood", "Icebound Fortitude"})
HAND_OF_PROTECTION = frozenset({"Hand of Protection"})
# icc.personal values that are not a pick
NO_PICK = frozenset({"none", "covered", "channel"})
DIFFICULTIES = {0: "10N", 1: "25N", 2: "10H", 3: "25H"}


def pull_end(trace: Trace) -> int:
    ends = trace.of("end")
    if ends:
        return ends[-1]["t"]
    return trace.records[-1].get("t", 0) if trace.records else 0


def mine(trace: Trace) -> bool:
    return encounter_of(trace) == SLUG


def stand_name(stand: int) -> str:
    if stand >= FIRST_TANK_SPOT:
        return f"tank {stand - FIRST_TANK_SPOT + 1}"
    return "stack" if stand == STACK else f"slot {stand}"


def stands(trace: Trace) -> dict[int, list[int]]:
    """Per bot, every stand it held in order, releases left out."""
    out: dict[int, list[int]] = {}
    for guid, spans in holder_spans(trace, SLOT, pull_end(trace)).items():
        held = [int(value) for value, _, _ in spans if value.isdigit() and value != "0"]
        if held:
            out[guid] = held
    return out


def target_time(trace: Trace) -> dict[str, tuple[int, int]]:
    """Per rule: `(bots, ms held summed over bots)`."""
    bots: dict[str, set[int]] = collections.defaultdict(set)
    held: dict[str, int] = collections.defaultdict(int)
    for guid, spans in holder_spans(trace, TARGET, pull_end(trace)).items():
        for value, start, stop in spans:
            bots[value].add(guid)
            held[value] += stop - start
    return {value: (len(bots[value]), held[value]) for value in held}


def applications(trace: Trace, spell: int) -> list[tuple[int, int]]:
    return sorted((rec["t"], rec["d"]) for rec in trace.of("aura") if rec.get("sp") == spell and not rec.get("r"))


def swaps(trace: Trace) -> list[tuple[int, int, int | None, int | None, int | None]]:
    """`(t, rune tank, elected tank, its swap taunt's t, the rune tank's yield t)` per Rune of Blood."""
    last = pull_end(trace)
    elected = holder_spans(trace, SWAP, last)
    yields = holder_spans(trace, YIELD, last)
    taunts = sorted((rec["t"], rec["g"]) for rec in trace.of("act")
                    if rec.get("a") == SWAP_ACTION and rec.get("vd") == "OK")
    out = []
    for when, tank in applications(trace, SPELL_RUNE_OF_BLOOD):
        stop = when + RUNE_MS
        picks = sorted((start, guid) for guid, spans in elected.items() if guid != tank
                       for value, start, end in spans if value == "1" and start < stop and end > when)
        picked = picks[0][1] if picks else None
        taunt = next((at for at, guid in taunts if guid == picked and when <= at < stop), None)
        yielded = next((start for value, start, _ in yields.get(tank, []) if value == "1" and when <= start < stop),
                       None)
        out.append((when, tank, picked, taunt, yielded))
    return out


def novas(trace: Trace) -> list[tuple[int, list[int]]]:
    """`(t, players hit)` per Blood Nova, its rows grouped by stamp."""
    out: list[tuple[int, list[int]]] = []
    for rec in sorted((rec for rec in trace.of("dmg") if rec.get("sp") in BLOOD_NOVA), key=lambda rec: rec["t"]):
        if out and rec["t"] - out[-1][0] <= NOVA_WINDOW_MS:
            out[-1][1].append(rec["d"])
        else:
            out.append((rec["t"], [rec["d"]]))
    return out


def duty_by_target(trace: Trace, key: str) -> dict[int, tuple[int, set[int]]]:
    """Per duty target: `(t of its first duty, holders)`."""
    out: dict[int, tuple[int, set[int]]] = {}
    for guid, spans in holder_spans(trace, key, pull_end(trace)).items():
        for value, start, _ in spans:
            if not value.isdigit() or value == "0":
                continue
            target = int(value)
            first, holders = out.get(target, (start, set()))
            out[target] = (min(first, start), holders | {guid})
    return out


def marks(trace: Trace) -> list[tuple[int, int, list[int], int | None]]:
    """`(t, marked player, healers on its duty, its death's t or None)` per Mark."""
    duties = holder_spans(trace, MARKHEAL, pull_end(trace))
    dead_at = first_deaths(trace)
    out = []
    for when, victim in applications(trace, SPELL_MARK_OF_THE_FALLEN_CHAMPION):
        healers = sorted({int(value) for value, _, _ in duties.get(victim, []) if value.isdigit() and value != "0"})
        died = dead_at.get(victim)
        out.append((when, victim, healers, died if died is not None and died >= when else None))
    return out


def casts_named(trace: Trace, names: frozenset) -> list[tuple[int, int, str]]:
    """`(t, guid, spell name)` for every roster cast of one of `names`, once per ms."""
    roster = roster_guids(trace)
    named = ((rec["t"], rec.get("s"), trace.spells.get(rec.get("sp"), "")) for rec in trace.of("cast")
             if rec.get("s") in roster)
    return sorted({cast for cast in named if cast[2] in names})


def frenzy_start(trace: Trace) -> int | None:
    """When `dbs.phase` first reads 2, or None."""
    return next((start for value, start, _ in latch_spans(trace, PHASE, pull_end(trace)) if value == "2"), None)


def tank_windows(trace: Trace) -> list[tuple[int, int, int, list[str]]]:
    """`(start, stop, tank, values)` per run of non-idle `dbs.defensive` values of one tank."""
    out = []
    for guid, spans in holder_spans(trace, DEFENSIVE, pull_end(trace)).items():
        run: list[tuple[str, int, int]] = []
        for span in spans + [("idle", pull_end(trace), pull_end(trace))]:
            if span[0] != "idle":
                run.append(span)
            elif run:
                out.append((run[0][1], run[-1][2], guid, [value for value, _, _ in run]))
                run = []
    return sorted(out)


def protect_duties(trace: Trace) -> list[tuple[int, int, int, int, int | None]]:
    """`(start, stop, paladin, target, t of its Hand of Protection cast or None)` per duty."""
    casts = casts_named(trace, HAND_OF_PROTECTION)
    out = []
    for paladin, spans in holder_spans(trace, PROTECT, pull_end(trace)).items():
        for value, start, stop in spans:
            if not value.isdigit() or value == "0":
                continue
            cast = next((when for when, guid, _ in casts
                         if guid == paladin and start - PROTECT_LEAD_MS <= when <= stop + PROTECT_TAIL_MS), None)
            out.append((start, stop, paladin, int(value), cast))
    return sorted(out)


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
    steps = latch_spans(trace, PHASE, pull_end(trace))
    if not steps:
        print(f"  {PHASE} never written")
        return
    for value, start, stop in steps:
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {value}")


def show_slots(trace: Trace) -> None:
    print("STANDS")
    if not mine(trace):
        return
    held = stands(trace)
    if not held:
        print(f"  {SLOT} never written")
        return
    for guid, steps in sorted(held.items(), key=lambda item: (item[1][0], trace.name(item[0]))):
        moved = f"  then {', '.join(stand_name(step) for step in steps[1:])}" if len(steps) > 1 else ""
        print(f"  {stand_name(steps[0]):<8} {trace.name(guid)}{moved}")


def show_targets(trace: Trace) -> None:
    print("TARGETS")
    if not mine(trace):
        return
    rows = target_time(trace)
    if not rows:
        print(f"  {TARGET} never written")
        return
    for value, (bots, held) in sorted(rows.items(), key=lambda row: -row[1][1]):
        print(f"  {value:<8} {bots:>3} bot(s)  {clock(held):>9} held in all")


def show_swaps(trace: Trace) -> None:
    print("RUNE OF BLOOD")
    if not mine(trace):
        return
    rows = swaps(trace)
    if not rows:
        print("  no Rune of Blood landed")
        return
    for when, tank, picked, taunt, yielded in rows:
        if picked is None:
            swap = "nobody elected"
        elif taunt is None:
            swap = f"{trace.name(picked)} elected, no taunt"
        else:
            swap = f"{trace.name(picked)} taunted {(taunt - when) / 1000:.1f} s after"
        stood_off = "never yielded" if yielded is None else f"yielded {(yielded - when) / 1000:.1f} s after"
        print(f"  {clock(when)}  {trace.name(tank)}  {swap}, {stood_off}")


def show_novas(trace: Trace) -> None:
    print("BLOOD NOVA")
    if not mine(trace):
        return
    rows = novas(trace)
    if not rows:
        print("  no Blood Nova hit anyone")
        return
    shared = [row for row in rows if len(row[1]) > 1]
    print(f"  {len(rows)} nova(s), {len(shared)} hit more than one player")
    for when, hit in shared:
        print(f"  {clock(when)}  {names(trace, hit)}")


def show_kills(trace: Trace) -> None:
    print("BLOOD BEASTS")
    if not mine(trace):
        return
    rows = duty_by_target(trace, KILL)
    if not rows:
        print(f"  {KILL} never written")
        return
    for beast, (first, holders) in sorted(rows.items(), key=lambda row: row[1][0]):
        print(f"  {clock(first)}  {trace.name(beast)}  {len(holders)} on it: {names(trace, holders)}")


def show_control(trace: Trace) -> None:
    print("CONTROL")
    if not mine(trace):
        return
    spans = holder_spans(trace, CONTROL, pull_end(trace))
    rows = sorted((start, caster, int(value), stop - start) for caster, held in spans.items()
                  for value, start, stop in held if value.isdigit() and value != "0")
    if not rows:
        print(f"  {CONTROL} never written")
        return
    for start, caster, beast, held in rows:
        print(f"  {clock(start)}  {trace.name(caster)} on {trace.name(beast)}  held {clock(held)}")


def show_marks(trace: Trace) -> None:
    print("MARK OF THE FALLEN CHAMPION")
    if not mine(trace):
        return
    rows = marks(trace)
    if not rows:
        print("  no Mark landed")
        return
    for when, victim, healers, died in rows:
        healer = names(trace, healers) if healers else "no healer"
        fate = f"died {clock(died)}" if died is not None else "alive"
        print(f"  {clock(when)}  {trace.name(victim)}  {healer}  {fate}")


def show_defensives(trace: Trace) -> None:
    print("TANK DEFENSIVES")
    if not mine(trace):
        return
    windows = tank_windows(trace)
    if not windows:
        print(f"  {DEFENSIVE} never offered a button")
    casts = casts_named(trace, TANK_BUTTONS)
    frenzy = frenzy_start(trace)
    for start, stop, tank, values in windows:
        low, high = start - WINDOW_LEAD_MS, stop + WINDOW_TAIL_MS
        spent = [cast for cast in casts if cast[1] == tank and low <= cast[0] <= high]
        kind = "frenzy" if frenzy is not None and start >= frenzy else "peak or low"
        offered = ", ".join(dict.fromkeys(values))
        cast = ", ".join(f"{name} {(when - start) / 1000:+.1f}s" for when, _, name in spent) or "none"
        print(f"  {clock(start)}  {trace.name(tank):<10} {clock(stop - start):>9}  {kind:<11} "
              f"offered {offered}; cast {cast}")
    calls = notes(trace, HUMAN_CALL)
    if calls:
        print("  calls to a human tank  " + ", ".join(f"{rec['txt']} {clock(rec['t'])}" for rec in calls))
    picks = sorted((rec["t"], rec.get("g"), rec["txt"]) for rec in notes(trace, PERSONAL)
                   if str(rec.get("txt", "")).lower() not in NO_PICK)
    if picks:
        print("  marked bots' own buttons  " + ", ".join(f"{name} {trace.name(guid)} {clock(when)}"
                                                         for when, guid, name in picks))


def show_protect(trace: Trace) -> None:
    print("HAND OF PROTECTION")
    if not mine(trace):
        return
    duties = protect_duties(trace)
    if not duties:
        print(f"  {PROTECT} never written")
        return
    for start, stop, paladin, target, cast in duties:
        landed = f"cast {(cast - start) / 1000:+.1f}s" if cast is not None else "never cast"
        print(f"  {clock(start)}  {trace.name(paladin)} on {trace.name(target)}  held {clock(stop - start)}  {landed}")


def show_cheats(trace: Trace) -> None:
    print("CHEATS")
    if not mine(trace):
        return
    rows = own_cheats(trace)
    if not rows:
        print("  no dbs.* or icc.* cheat fired")
        return
    for key, entry in rows:
        print(f"  {key}  {entry['n']} use(s) from {clock(entry['first'])}  by {names(trace, entry['bots'])}")


SECTIONS = (
    ("phases", "dbs.phase: 1, then 2 once he is in Frenzy", show_phases),
    ("slots", "each bot's stand: a slot, the stack or a tank spot", show_slots),
    ("targets", "what the resolver had the bots on, by rule", show_targets),
    ("swaps", "each Rune of Blood, the swap taunt after it and the rune tank's yield", show_swaps),
    ("novas", "each Blood Nova and how many players it hit", show_novas),
    ("kills", "each Blood Beast and the ranged on its kill duty", show_kills),
    ("control", "each control duty on a beast", show_control),
    ("marks", "each Mark of the Fallen Champion, its healer and fate", show_marks),
    ("defensives", "tank buttons, calls to a human tank, the marked bots' own buttons", show_defensives),
    ("protect", "each Hand of Protection duty and whether the hand went out", show_protect),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
