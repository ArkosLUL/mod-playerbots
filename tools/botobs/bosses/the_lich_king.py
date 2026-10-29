#!/usr/bin/env python3
"""Read a Lich King pull from a RaidObs trace: his phases, every bot duty, and Frostmourne.

    the_lich_king.py <file>                every section
    the_lich_king.py <file> --phases       lk.phase: one, winter1, two, winter2, three, frostmourne, outro
    the_lich_king.py <file> --tanks        tank roles, taunts by rule, the brake, tank buttons, human calls
    the_lich_king.py <file> --stands       per bot: the stands and stacks it held, and the phase three side
    the_lich_king.py <file> --plague       Necrotic Plague: each carrier's steps, cures by rule, ticks taken
    the_lich_king.py <file> --adds         Shamblings: Enrage and Tranquilizing Shot, the Frenzy stuns
    the_lich_king.py <file> --traps        heroic Shadow Traps: dodges and hits
    the_lich_king.py <file> --winters      each winter: sphere and spirit picks, silence dispels, the Quake hold
    the_lich_king.py <file> --valkyrs      each Val'kyr: its bucket, stuns and slows; heroic siphon taunts
    the_lich_king.py <file> --defile       each Defile: its target and the runs, puddle step-outs and hits
    the_lich_king.py <file> --vile         Vile Spirits: soakers, Frost Traps, side flips, bursts taken
    the_lich_king.py <file> --frostmourne  Harvest Soul duties, heroic bomb rules and hits
    the_lich_king.py <file> --targets      per bot: the resolver rules it was handed
    the_lich_king.py <file> --cheats       this encounter's cheat uses

What the generic views miss here:

- **`lk.phase` is his script's own state**, per bot: `winter1` and `winter2` from his turn to passive
  at 70% and 40% until he turns aggressive after the Quake, `frostmourne` for a bot in heroic's
  Harvest Souls, `outro` once combat is disallowed at 10%. The banner names silent keys only for the
  phases the pull reached, spares `lk.trap`, `lk.stun`, `lk.siphon` and `lk.bombdodge` off heroic and
  `lk.harvest` on it, and never names `lk.winterinbound`, `lk.mtinbound`, `lk.rangedinbound` or
  `lk.sharedslot`, which no code writes.
- **`lk.tank` is a role per tank**: `1` the holder, `2` free, `0` off. `lk.brake` is `1` while braked,
  past 90% of the holder's threat until under 80%. `lk.side` is the raid's phase three side, `1` east,
  `2` west.
- **The other per-bot keys note `none` when their duty ends**, and a value leads with its rule, which
  this reader counts. A unit in a value is its guid counter or its whole guid. `lk.stun`, `lk.enrage`,
  `lk.silence` and `lk.siphon` name the unit alone: the stunned add, the enraged Shambling, the
  silenced member, the Val'kyr.
- **Winter picks come from `lk.target`** by the picked unit's entry, Ice Sphere or Raging Spirit,
  whatever the rule is called. The Quake hold is `lk.stand` `quake`.
- **`lk.defile` is the Defile listener's**, one note per cast on its target; `lk.defilerun` is what
  each bot did about it.
- **`lk.defensive` is the tank ladder's pick**, a cast name, `idle` outside a window. `lk.humancall`
  is a call to a human tank.
"""
from __future__ import annotations

import collections
import pathlib
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from bosses import icecrown_citadel as icc  # noqa: E402
from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.probes import emitted_keys, holder_spans, latch_spans, resolve_guids, silent_keys  # noqa: E402
from raidobs.trace import Trace, clock, notes, roster_guids  # noqa: E402

ENCOUNTER = "the-lich-king"
TITLE = "Lich King"
PHASE = "lk.phase"
PHASES = ("one", "winter1", "two", "winter2", "three", "frostmourne", "outro")
WINTERS = ("winter1", "winter2")
HEROIC = (2, 3)
# IccInstanceState members that no code writes
UNUSED_KEYS = frozenset({"lk.winterinbound", "lk.mtinbound", "lk.rangedinbound", "lk.sharedslot"})
HEROIC_ONLY = frozenset({"lk.trap", "lk.stun", "lk.siphon", "lk.bombdodge"})
NORMAL_ONLY = frozenset({"lk.harvest"})
# the phases a key's duty runs in; a key missing here runs in all of them
KEY_PHASES = {
    "lk.brake": frozenset({"one", "two", "three"}),
    "lk.stand": frozenset({"one", *WINTERS}),
    "lk.plague": frozenset({"one"}),
    "lk.cure": frozenset({"one"}),
    "lk.enrage": frozenset({"one"}),
    "lk.stun": frozenset({"one"}),
    "lk.trap": frozenset({"one"}),
    "lk.silence": frozenset(WINTERS),
    "lk.stack": frozenset({"two", "three", "frostmourne"}),
    "lk.defile": frozenset({"two", "three"}),
    "lk.defilerun": frozenset({"two", "three"}),
    "lk.valkyr": frozenset({"two"}),
    "lk.siphon": frozenset({"two"}),
    "lk.side": frozenset({"three"}),
    "lk.vile": frozenset({"three"}),
    "lk.harvest": frozenset({"three"}),
    "lk.bombdodge": frozenset({"frostmourne"}),
}
OFF = ("none", "0")
LIST_LIMIT = 8
COUNTER_MASK = 0xFFFFFFFF
TANK_ROLES = {"1": "holder", "2": "free", "0": "off"}
SIDES = {"1": "east", "2": "west"}
DEFENSIVE_OFF = "idle"
# a button counts for a window from this long before it opens until this long after it closes
WINDOW_LEAD_MS = 1500
WINDOW_TAIL_MS = 1500
# cast names, any rank
TANK_BUTTONS = frozenset({"Last Stand", "Shield Wall", "Divine Protection", "Barkskin", "Frenzied Regeneration",
                          "Survival Instincts", "Anti-Magic Shell", "Vampiric Blood", "Icebound Fortitude"})
DEFILE_CAST_MS = 2000
# the duty tick that notes a run can trail the cast's end
DEFILE_TAIL_MS = 1000

ICE_SPHERE = 36633
RAGING_SPIRIT = 36701
VILE_SPIRIT = 37799
WICKED_SPIRIT = 39190
NECROTIC_PLAGUE = (70337, 73912, 73913, 73914)
ENRAGE = (72143, 72146, 72147, 72148)
FRENZY = (28747,)
TRANQUILIZING_SHOT = (19801,)
SHADOW_TRAP = (73529,)
DEFILE = (72754, 73708, 73709, 73710)
VILE_SPIRITS = (70498,)
# Vile and Wicked Spirits both burst with it, so hits are told apart by their source
SPIRIT_BURST = (70503, 73806, 73807, 73808)
SPIRIT_BOMB = (73576, 73803, 73804, 73805)
HARVEST_SOUL = (68980, 74325)


def mine(trace: Trace) -> bool:
    return encounter_of(trace) == ENCOUNTER


def refuse(trace: Trace, title: str) -> bool:
    print(title)
    if mine(trace):
        return False
    print(f"  not a {TITLE} pull")
    return True


def heroic(trace: Trace) -> bool:
    return trace.header.get("diff") in HEROIC


def reached(trace: Trace) -> set[str]:
    """Every lk.phase value any bot noted."""
    return {value for value, _, _ in latch_spans(trace, PHASE)}


def spared(trace: Trace, key: str, phases: set[str]) -> bool:
    """Whether the banner leaves a silent key unnamed: no code writes it, this difficulty has no such
    duty, or it runs only in phases the pull never reached."""
    if key in UNUSED_KEYS or key in (NORMAL_ONLY if heroic(trace) else HEROIC_ONLY):
        return True
    wanted = KEY_PHASES.get(key)
    return wanted is not None and not wanted & phases


def missing_probes(trace: Trace) -> list[str]:
    phases = reached(trace)
    return [key for key, _, _ in silent_keys(emitted_keys(trace), ENCOUNTER) if not spared(trace, key, phases)]


def show_banner(trace: Trace) -> None:
    icc.show_banner(trace)
    if not icc.is_icc(trace):
        return
    if not mine(trace):
        print(f"  not a {TITLE} pull, every section below reads empty")
        return
    phases = reached(trace)
    if not phases:
        print(f"{PHASE} never written, so no probe is called absent")
        return
    gone = missing_probes(trace)
    found = f"probes absent from this trace: {', '.join(gone)}" if gone else "all its lk.* probes present"
    order = [phase for phase in PHASES if phase in phases] + sorted(phases - set(PHASES))
    print(f"reached {', '.join(order)}; {found}")


def off(value: str) -> bool:
    return value in OFF


def rule_of(value: str) -> str:
    return value.split(" ", 1)[0]


def spans_of(trace: Trace, key: str) -> dict[int, list[tuple[str, int, int]]]:
    return holder_spans(trace, key, icc.pull_end(trace))


def creature_counters(trace: Trace) -> dict[int, int]:
    """Per guid counter that exactly one non-player unit carries: that unit's whole guid."""
    cached = getattr(trace, "_lk_counters", None)
    if cached is None:
        units = [guid for guid in trace.names if guid >> 32]
        seen = collections.Counter(guid & COUNTER_MASK for guid in units)
        cached = {guid & COUNTER_MASK: guid for guid in units if seen[guid & COUNTER_MASK] == 1}
        trace._lk_counters = cached
    return cached


def unit_of(trace: Trace, word: str, creature: bool = True) -> int | None:
    """The unit a value's word names: a whole guid, or a bare counter, read as a creature's first when
    `creature` and as a player's first otherwise."""
    if not word.isdigit():
        return None
    number = int(word)
    if number >> 32:
        return number if number in trace.names else None
    mob = creature_counters(trace).get(number)
    if creature and mob is not None:
        return mob
    return number if number in trace.names else mob


def described(trace: Trace, value: str, creature: bool = True) -> str:
    """A value's rule, then the words after it with every unit named."""
    rule, _, rest = value.partition(" ")
    words = [rule]
    for word in rest.split():
        unit = unit_of(trace, word, creature)
        words.append(icc.who(trace, unit) if unit is not None else word)
    return " ".join(words)


def unit_names(trace: Trace, units: list[int]) -> str:
    """The units' names in order, each name once: Raging Spirits and Val'kyrs share theirs."""
    return ", ".join(dict.fromkeys(icc.who(trace, unit) for unit in units))


def counted(counts: collections.Counter) -> str:
    return ", ".join(f"{key} {counts[key]}" for key in sorted(counts)) or "-"


def joined(values: list[str], sep: str) -> str:
    """`values` joined, cut after LIST_LIMIT with a count of the rest."""
    rest = len(values) - LIST_LIMIT
    return (sep.join(values[:LIST_LIMIT]) or "-") + (f" ... {rest} more" if rest > 0 else "")


def held_values(spans: list[tuple[str, int, int]]) -> tuple[list[str], int]:
    """The distinct values held in order, `none` and `0` left out, and how often one went straight to
    another."""
    held: list[str] = []
    moves = 0
    last = None
    for value, _, _ in spans:
        if off(value):
            last = None
            continue
        if value not in held:
            held.append(value)
        moves += last is not None and last != value
        last = value
    return held, moves


def episodes(spans: list[tuple[str, int, int]]) -> list[list[str]]:
    """The values of each duty in order, a duty running until `none` or `0`."""
    runs: list[list[str]] = []
    run: list[str] = []
    for value, _, _ in spans:
        if off(value):
            if run:
                runs.append(run)
            run = []
            continue
        run.append(value)
    if run:
        runs.append(run)
    return runs


def notes_inside(trace: Trace, key: str, start: int, stop: int) -> dict[int, list[str]]:
    """Per guid: the values it noted under `key` from `start` until before `stop`, repeats folded,
    `none` and `0` left out."""
    out: dict[int, list[str]] = {}
    for rec in sorted(notes(trace, key), key=lambda rec: rec["t"]):
        value = str(rec.get("txt", ""))
        if not start <= rec["t"] < stop or off(value):
            continue
        held = out.setdefault(rec.get("g", 0), [])
        if not held or held[-1] != value:
            held.append(value)
    return out


def duties(trace: Trace, key: str, ruled: bool = True,
           creature: bool = True) -> dict[int, tuple[collections.Counter, list[int]]]:
    """Per guid: how many spans each rule of `key` started, and the units its values named, first
    mention first, `none` and `0` left out. Without `ruled` a value names units alone and every span
    counts under one blank rule."""
    out = {}
    for guid, spans in spans_of(trace, key).items():
        rules: collections.Counter = collections.Counter()
        units: list[int] = []
        for value, _, _ in spans:
            words = value.split()
            if off(value) or not words:
                continue
            rules[words[0] if ruled else ""] += 1
            for word in words[1:] if ruled else words:
                unit = unit_of(trace, word, creature)
                if unit is not None and unit not in units:
                    units.append(unit)
        if rules:
            out[guid] = (rules, units)
    return out


def show_duty(trace: Trace, key: str, what: str, ruled: bool = True, creature: bool = True) -> None:
    rows = duties(trace, key, ruled, creature)
    if not rows:
        print(f"  {key} noted no duty")
        return
    print(f"  {key} per bot: {what}")
    for guid in sorted(rows, key=trace.name):
        rules, units = rows[guid]
        told = counted(rules) if ruled else str(sum(rules.values()))
        on = f"  on {unit_names(trace, units)}" if units else ""
        print(f"    {icc.who(trace, guid)}  {told}{on}")


def hits(trace: Trace, spells, source_entry: int | None = None) -> collections.Counter:
    """Per roster player: the `dmg` rows of `spells` it took, only from units of `source_entry` when
    given."""
    roster = roster_guids(trace)
    return collections.Counter(
        rec["d"] for rec in trace.of("dmg")
        if rec.get("sp") in spells and rec.get("d") in roster
        and (source_entry is None or trace.entries.get(rec.get("s")) == source_entry))


def applied(trace: Trace, spells, on_roster: bool) -> collections.Counter:
    """Per unit: the `aura` rows that gave it one of `spells`, on roster players or on everyone else."""
    roster = roster_guids(trace)
    return collections.Counter(rec["d"] for rec in trace.of("aura")
                               if rec.get("sp") in spells and not rec.get("r")
                               and (rec.get("d") in roster) == on_roster)


def cast_count(trace: Trace, spells, roster_only: bool = False) -> int:
    roster = roster_guids(trace)
    return sum(1 for rec in trace.of("cast")
               if rec.get("sp") in spells and (not roster_only or rec.get("s") in roster))


def casts_named(trace: Trace, names: frozenset) -> list[tuple[int, int, str]]:
    """`(t, guid, spell name)` for every roster cast of one of `names`, once per ms."""
    roster = roster_guids(trace)
    named = ((rec["t"], rec.get("s"), trace.spells.get(rec.get("sp"), "")) for rec in trace.of("cast")
             if rec.get("s") in roster)
    return sorted({cast for cast in named if cast[2] in names})


def tank_windows(trace: Trace, key: str) -> list[tuple[int, int, int, list[str]]]:
    """`(start, stop, tank, values)` per run of non-idle `key` values of one tank."""
    end = icc.pull_end(trace)
    out = []
    for guid, spans in holder_spans(trace, key, end).items():
        run: list[tuple[str, int, int]] = []
        for span in spans + [(DEFENSIVE_OFF, end, end)]:
            if span[0] != DEFENSIVE_OFF:
                run.append(span)
            elif run:
                out.append((run[0][1], run[-1][2], guid, [value for value, _, _ in run]))
                run = []
    return sorted(out)


def show_buttons(trace: Trace, key: str, call_key: str) -> None:
    windows = tank_windows(trace, key)
    if windows:
        print(f"  {key}, each window: the tank, how long, what it offered and cast")
    else:
        print(f"  {key} never offered a button")
    casts = casts_named(trace, TANK_BUTTONS)
    for start, stop, tank, values in windows:
        low, high = start - WINDOW_LEAD_MS, stop + WINDOW_TAIL_MS
        spent = [cast for cast in casts if cast[1] == tank and low <= cast[0] <= high]
        offered = ", ".join(dict.fromkeys(values))
        cast = ", ".join(f"{name} {(when - start) / 1000:+.1f}s" for when, _, name in spent) or "none"
        print(f"    {clock(start)}  {trace.name(tank):<10} {clock(stop - start):>9}  offered {offered}; cast {cast}")
    calls = notes(trace, call_key)
    if calls:
        print("  calls to a human tank  " + ", ".join(f"{resolve_guids(trace, str(rec['txt']))} {clock(rec['t'])}"
                                                    for rec in calls))


def show_brakes(trace: Trace, key: str) -> None:
    rows = spans_of(trace, key)
    if not rows:
        print(f"  {key} never written")
        return
    print(f"  {key} per bot: times braked, time held")
    for guid in sorted(rows, key=trace.name):
        runs = [stop - start for value, start, stop in rows[guid] if value == "1"]
        print(f"    {icc.who(trace, guid)}  {len(runs)}, {clock(sum(runs))}")


def show_phases(trace: Trace) -> None:
    if refuse(trace, "PHASES"):
        return
    steps = icc.phase_steps(trace)
    if not steps:
        print(f"  {PHASE} never written")
        return
    print(f"  {PHASE}, each step from the first bot to note it\n")
    print(f"    {'from':>9}  {'held':>9}  phase")
    for value, start, stop in steps:
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {value}")


def show_tanks(trace: Trace) -> None:
    if refuse(trace, "TANKS"):
        return
    roles = spans_of(trace, "lk.tank")
    if roles:
        print("  lk.tank per tank: its role, from when")
        for guid in sorted(roles, key=trace.name):
            held = ", ".join(f"{TANK_ROLES.get(value, value)} {clock(start)}" for value, start, _ in roles[guid])
            print(f"    {icc.who(trace, guid)}  {held}")
    else:
        print("  lk.tank never written")
    show_duty(trace, "lk.taunt", "taunts by rule, the units taunted")
    show_brakes(trace, "lk.brake")
    show_buttons(trace, "lk.defensive", "lk.humancall")


def side_steps(trace: Trace) -> list[tuple[str, int]]:
    """`(side, from)` per lk.side value, the unset `0` left out."""
    return [(value, start) for value, start, _ in latch_spans(trace, "lk.side") if not off(value)]


def show_stands(trace: Trace) -> None:
    if refuse(trace, "STANDS"):
        return
    for key, what in (("lk.stand", "phase one and winter stands"),
                      ("lk.stack", "phase two, three and Frostmourne stacks")):
        rows = spans_of(trace, key)
        if not rows:
            print(f"  {key} never written")
            continue
        print(f"  {key} per bot: the {what} held in order (moves between two)")
        for guid in sorted(rows, key=trace.name):
            held, moves = held_values(rows[guid])
            print(f"    {icc.who(trace, guid)}  {joined(held, '/')} (moved {moves})")
    sides = " -> ".join(f"{SIDES.get(side, side)} {clock(start)}" for side, start in side_steps(trace))
    print(f"  lk.side, the phase three side: {sides or 'never set'}")


def show_plague(trace: Trace) -> None:
    if refuse(trace, "PLAGUE"):
        return
    carriers = {guid: episodes(spans) for guid, spans in spans_of(trace, "lk.plague").items()}
    carriers = {guid: runs for guid, runs in carriers.items() if runs}
    if carriers:
        print("  lk.plague per carrier: each plague's steps")
        for guid in sorted(carriers, key=trace.name):
            steps = [" -> ".join(described(trace, value) for value in run) for run in carriers[guid]]
            print(f"    {icc.who(trace, guid)}  {joined(steps, '; ')}")
    else:
        print("  lk.plague noted no carrier")
    show_duty(trace, "lk.cure", "cures by rule, the carriers", creature=False)
    print(f"  Necrotic Plague ticks taken: {icc.ranked(trace, hits(trace, NECROTIC_PLAGUE)) or 'none'}")


def show_adds(trace: Trace) -> None:
    if refuse(trace, "ADDS"):
        return
    enrages = sum(applied(trace, ENRAGE, False).values())
    shots = cast_count(trace, TRANQUILIZING_SHOT, roster_only=True)
    frenzies = sum(applied(trace, FRENZY, False).values())
    print(f"  Shambling Enrage {enrages} time(s), Tranquilizing Shot cast {shots} time(s), Frenzy {frenzies} time(s)")
    show_duty(trace, "lk.enrage", "Enrage calls, the Shamblings", ruled=False)
    if not heroic(trace) and "lk.stun" not in emitted_keys(trace):
        print("  lk.stun: Frenzy is heroic only")
        return
    show_duty(trace, "lk.stun", "Frenzy stuns called, the units", ruled=False)


def show_traps(trace: Trace) -> None:
    if refuse(trace, "TRAPS"):
        return
    if not heroic(trace) and "lk.trap" not in emitted_keys(trace):
        print("  Shadow Traps are heroic only")
        return
    show_duty(trace, "lk.trap", "dodges, the traps")
    print(f"  Shadow Trap hits: {icc.ranked(trace, hits(trace, SHADOW_TRAP)) or 'none'}")


def winter_rows(trace: Trace) -> list[dict]:
    """Per winter in lk.phase: `phase`, `start`, `stop`, the lk.target spans started on a `sphere` and on
    a `spirit` per bot, `silence` as (dispeller, members) from lk.silence, and `quake` as (from, bot) per
    bot whose lk.stand took `quake`."""
    targets = spans_of(trace, "lk.target")
    stands = spans_of(trace, "lk.stand")
    rows = []
    for phase, start, stop in icc.phase_steps(trace):
        if phase not in WINTERS:
            continue
        picks = {ICE_SPHERE: collections.Counter(), RAGING_SPIRIT: collections.Counter()}
        for guid, spans in targets.items():
            for value, at, _ in spans:
                if not start <= at < stop or off(value):
                    continue
                entries = {trace.entries.get(unit_of(trace, word)) for word in value.split()[1:]}
                for entry in entries & set(picks):
                    picks[entry][guid] += 1
        silence = []
        for guid, values in notes_inside(trace, "lk.silence", start, stop).items():
            members = [unit_of(trace, word, creature=False) for value in values for word in value.split()]
            silence.append((guid, [unit for unit in members if unit is not None]))
        quake = []
        for guid, spans in stands.items():
            at = next((at for value, at, _ in spans if start <= at < stop and rule_of(value) == "quake"), None)
            if at is not None:
                quake.append((at, guid))
        rows.append({"phase": phase, "start": start, "stop": stop, "sphere": picks[ICE_SPHERE],
                     "spirit": picks[RAGING_SPIRIT], "silence": sorted(silence, key=lambda pair: trace.name(pair[0])),
                     "quake": sorted(quake, key=lambda pair: (pair[0], trace.name(pair[1])))})
    return rows


def show_winters(trace: Trace) -> None:
    if refuse(trace, "WINTERS"):
        return
    rows = winter_rows(trace)
    if not rows:
        print(f"  no winter in {PHASE}")
        return
    print(f"  {len(rows)} winter(s): sphere and spirit picks per bot from lk.target, silence dispels, the Quake hold")
    for row in rows:
        print(f"\n  {row['phase']}  {clock(row['start'])}  held {clock(row['stop'] - row['start'])}")
        print(f"    sphere picks  {icc.ranked(trace, row['sphere']) or 'none'}")
        print(f"    spirit picks  {icc.ranked(trace, row['spirit']) or 'none'}")
        dispels = "; ".join(f"{icc.who(trace, guid)} on {unit_names(trace, members) or '-'}"
                            for guid, members in row["silence"])
        print(f"    silence dispels  {dispels or 'none'}")
        held = ", ".join(f"{icc.who(trace, guid)} {clock(at)}" for at, guid in row["quake"])
        print(f"    Quake hold  {held or 'nobody'}")


def valkyr_rows(trace: Trace) -> list[dict]:
    """Per Val'kyr lk.valkyr named, first named first: `t`, `valkyr` (its guid, else the word naming it),
    `dps`, the bots bucketed on it, and `stun` and `slow`, per bot the spans of each it was handed."""
    rows: dict = {}
    for guid, spans in spans_of(trace, "lk.valkyr").items():
        for value, at, _ in spans:
            rule, _, word = value.partition(" ")
            if off(value) or not word:
                continue
            valkyr = unit_of(trace, word) or word
            row = rows.setdefault(valkyr, {"t": at, "valkyr": valkyr, "dps": set(),
                                           "stun": collections.Counter(), "slow": collections.Counter()})
            row["t"] = min(row["t"], at)
            if rule == "dps":
                row["dps"].add(guid)
            elif rule in ("stun", "slow"):
                row[rule][guid] += 1
    return sorted(rows.values(), key=lambda row: (row["t"], str(row["valkyr"])))


def valkyr_label(trace: Trace, valkyr) -> str:
    """The Val'kyr's name and guid counter, since every Val'kyr carries the same name."""
    return f"{trace.name(valkyr)} {valkyr & COUNTER_MASK}" if isinstance(valkyr, int) else valkyr


def show_valkyrs(trace: Trace) -> None:
    if refuse(trace, "VALKYRS"):
        return
    rows = valkyr_rows(trace)
    if rows:
        print("  lk.valkyr, each Val'kyr from its first call: its DPS bucket, then the stuns and slows called per bot")
        for row in rows:
            bucket = ", ".join(sorted(icc.who(trace, guid) for guid in row["dps"])) or "-"
            print(f"    {clock(row['t']):>9}  {valkyr_label(trace, row['valkyr'])}  dps {bucket}  "
                  f"stun {icc.ranked(trace, row['stun']) or '-'}  slow {icc.ranked(trace, row['slow']) or '-'}")
    else:
        print("  lk.valkyr never named a Val'kyr")
    if heroic(trace) or "lk.siphon" in emitted_keys(trace):
        show_duty(trace, "lk.siphon", "siphon taunts, the Val'kyrs", ruled=False)


def defile_rows(trace: Trace) -> list[dict]:
    """Per lk.defile cast: `t`, `target`, `steps`, the target's lk.defilerun rules until the cast lands,
    and `cleared`, the other bots that noted `clear` then."""
    rows = []
    for rec in sorted(notes(trace, "lk.defile"), key=lambda rec: rec["t"]):
        start = rec["t"]
        target = rec.get("g", 0)
        runs = notes_inside(trace, "lk.defilerun", start, start + DEFILE_CAST_MS + DEFILE_TAIL_MS)
        cleared = [guid for guid, values in runs.items()
                   if guid != target and any(rule_of(value) == "clear" for value in values)]
        rows.append({"t": start, "target": target, "steps": [rule_of(value) for value in runs.get(target, [])],
                     "cleared": sorted(cleared, key=trace.name)})
    return rows


def show_defile(trace: Trace) -> None:
    if refuse(trace, "DEFILE"):
        return
    rows = defile_rows(trace)
    if rows:
        print(f"  {len(rows)} Defile cast(s): the target, its lk.defilerun steps, the others who cleared off it")
        for row in rows:
            cleared = ", ".join(icc.who(trace, guid) for guid in row["cleared"]) or "nobody"
            print(f"    {clock(row['t']):>9}  {icc.who(trace, row['target'])}  "
                  f"{' -> '.join(row['steps']) or 'no run'}  cleared {cleared}")
    else:
        print("  lk.defile never noted a cast")
    puddles = collections.Counter({guid: rules["puddle"] for guid, (rules, _) in duties(trace, "lk.defilerun").items()
                                   if rules["puddle"]})
    print(f"  puddle step-outs: {icc.ranked(trace, puddles) or 'none'}")
    print(f"  Defile hits: {icc.ranked(trace, hits(trace, DEFILE)) or 'none'}")


def side_flips(trace: Trace) -> int:
    sides = [side for side, _ in side_steps(trace)]
    return sum(1 for before, after in zip(sides, sides[1:]) if before != after)


def show_vile(trace: Trace) -> None:
    if refuse(trace, "VILE"):
        return
    casts = cast_count(trace, VILE_SPIRITS)
    print(f"  Vile Spirits cast {casts} time(s), lk.side flipped {side_flips(trace)} time(s)")
    show_duty(trace, "lk.vile", "duties by rule")
    print(f"  Vile Spirit bursts taken: {icc.ranked(trace, hits(trace, SPIRIT_BURST, VILE_SPIRIT)) or 'none'}")


def show_frostmourne(trace: Trace) -> None:
    if refuse(trace, "FROSTMOURNE"):
        return
    if not heroic(trace):
        print(f"  Harvest Soul victims: {icc.ranked(trace, applied(trace, HARVEST_SOUL, True)) or 'none'}")
        show_duty(trace, "lk.harvest", "duties by rule")
        return
    trips = [(start, stop) for phase, start, stop in icc.phase_steps(trace) if phase == "frostmourne"]
    print(f"  heroic Harvest Souls in {PHASE}: "
          + (", ".join(f"{clock(start)} for {clock(stop - start)}" for start, stop in trips) or "none"))
    show_duty(trace, "lk.bombdodge", "bomb rules")
    followers = sorted((guid for guid, spans in spans_of(trace, "lk.stack").items()
                        if any(rule_of(value) == "fm" for value, _, _ in spans)), key=trace.name)
    print("  lk.stack fm, the bots that followed a leader: "
          + (", ".join(icc.who(trace, guid) for guid in followers) or "none"))
    print(f"  Spirit Bomb hits: {icc.ranked(trace, hits(trace, SPIRIT_BOMB)) or 'none'}")
    print(f"  Wicked Spirit bursts taken: {icc.ranked(trace, hits(trace, SPIRIT_BURST, WICKED_SPIRIT)) or 'none'}")


def show_targets(trace: Trace) -> None:
    if refuse(trace, "TARGETS"):
        return
    rows = spans_of(trace, "lk.target")
    if not rows:
        print("  lk.target never written")
        return
    print("  lk.target per bot: spans of each resolver rule")
    for guid in sorted(rows, key=trace.name):
        rules = collections.Counter(rule_of(value) for value, _, _ in rows[guid])
        print(f"    {icc.who(trace, guid)}  {counted(rules)}")


def show_cheats(trace: Trace) -> None:
    if refuse(trace, "CHEATS"):
        return
    if (trace.header.get("v") or 0) < icc.CHEAT_SCHEMA:
        print(f"  schema v{trace.header.get('v') or 0} predates the cheat record (v{icc.CHEAT_SCHEMA})")
        return
    rows = icc.cheat_rows(trace)
    own = [row for row in rows if row["own"]]
    if not own:
        print("  no cheat of this encounter fired")
    for row in own:
        print(f"  {row['key']}  {row['n']} use(s) from {clock(row['first'])}")
        print(f"    by  {icc.ranked(trace, row['bots'])}")
        print(f"    on  {icc.named(trace, row['targets'])}")
    others = sum(row["n"] for row in rows if not row["own"])
    if others:
        print(f"  {others} use(s) under other encounters' keys: icecrown_citadel.py --cheats")


SECTIONS = (
    ("phases", "lk.phase: one, winter1, two, winter2, three, frostmourne, outro", show_phases),
    ("tanks", "tank roles, taunts by rule, the brake, tank buttons, human calls", show_tanks),
    ("stands", "per bot: the stands and stacks it held, and the phase three side", show_stands),
    ("plague", "Necrotic Plague: each carrier's steps, cures by rule, ticks taken", show_plague),
    ("adds", "Shamblings: Enrage and Tranquilizing Shot, the Frenzy stuns", show_adds),
    ("traps", "heroic Shadow Traps: dodges and hits", show_traps),
    ("winters", "each winter: sphere and spirit picks, silence dispels, the Quake hold", show_winters),
    ("valkyrs", "each Val'kyr: its bucket, stuns and slows; heroic siphon taunts", show_valkyrs),
    ("defile", "each Defile: its target and the runs, puddle step-outs and hits", show_defile),
    ("vile", "Vile Spirits: soakers, Frost Traps, side flips, bursts taken", show_vile),
    ("frostmourne", "Harvest Soul duties, heroic bomb rules and hits", show_frostmourne),
    ("targets", "per bot: the resolver rules it was handed", show_targets),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
