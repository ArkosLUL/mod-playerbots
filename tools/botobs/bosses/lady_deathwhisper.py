#!/usr/bin/env python3
"""Read a Lady Deathwhisper pull from a RaidObs trace: the barrier phase, picks, CC, kicks and dodges.

    lady_deathwhisper.py <file>              every section
    lady_deathwhisper.py <file> --phases     ldw.phase: 1 while Mana Barrier holds, 2 once it breaks
    lady_deathwhisper.py <file> --targets    what the resolver had the bots on, by add kind
    lady_deathwhisper.py <file> --cc         each Dominate Mind, its ldw.cc caster and the CC that landed
    lady_deathwhisper.py <file> --kicks      her Frostbolts and who held ldw.interrupter
    lady_deathwhisper.py <file> --dodges     ldw.dodge: who walked out of which hazard
    lady_deathwhisper.py <file> --cheats     this encounter's cheat uses

What the generic views miss here:

- **Phase 2 starts when the barrier breaks, not at 95% health.** The barrier tops her health up every
  second, so health stays full until her mana is gone; `ldw.phase` reads the aura.
- **`ldw.target` is the kind of unit picked, per bot.** Melee and hunters take the adherents first,
  casters the fanatics; `boss` is her, `loose` and `held` a tank's add, `skull` a player's mark.
- **A Dominate Mind victim takes no damage under Cyclone**, so the area-damage hold lifts once one lands.
  The CC column names the first control aura on the victim inside its 12 s.
- **`ldw.interrupter` is `1` only inside a Frostbolt cast**, for the one bot elected to kick; generic
  interrupts on her current target can beat it to the kick.
"""
from __future__ import annotations

import collections
import pathlib
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of, prefix_matches_boss  # noqa: E402
from raidobs.geometry import guids_of_entry  # noqa: E402
from raidobs.probes import holder_spans, latch_spans  # noqa: E402
from raidobs.trace import Trace, cheat_uses, clock, combat_deaths  # noqa: E402

SLUG = "lady-deathwhisper"
NAME = "Lady Deathwhisper"
PHASE = "ldw.phase"
TARGET = "ldw.target"
CC = "ldw.cc"
INTERRUPTER = "ldw.interrupter"
DODGE = "ldw.dodge"
ENTRY = 36855
SPELL_DOMINATE_MIND = 71289
DOMINATE_MS = 12000
FROSTBOLT = {71420, 72007, 72501, 72502}
CONTROL = {
    33786: "Cyclone",
    853: "Hammer of Justice", 5588: "Hammer of Justice", 5589: "Hammer of Justice", 10308: "Hammer of Justice",
    51514: "Hex",
    118: "Polymorph", 12824: "Polymorph", 12825: "Polymorph", 12826: "Polymorph", 28271: "Polymorph",
    28272: "Polymorph", 61305: "Polymorph", 61721: "Polymorph", 61780: "Polymorph",
    20066: "Repentance",
    5782: "Fear", 6213: "Fear", 6215: "Fear",
    2094: "Blind",
}
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


def boss_guids(trace: Trace) -> set[int]:
    return guids_of_entry(trace, ENTRY)


def target_time(trace: Trace) -> dict[str, tuple[int, int]]:
    """Per picked kind: `(bots, ms held summed over bots)`."""
    bots: dict[str, set[int]] = collections.defaultdict(set)
    held: dict[str, int] = collections.defaultdict(int)
    for guid, spans in holder_spans(trace, TARGET, pull_end(trace)).items():
        for value, start, stop in spans:
            bots[value].add(guid)
            held[value] += stop - start
    return {value: (len(bots[value]), held[value]) for value in held}


def dominations(trace: Trace) -> list[tuple[int, int, list[int], tuple[int, str] | None]]:
    """`(t, victim, duty casters, (t, spell) of the first control aura or None)` per Dominate Mind."""
    auras = sorted(trace.of("aura"), key=lambda rec: rec["t"])
    duties = holder_spans(trace, CC, pull_end(trace))
    out = []
    for rec in auras:
        if rec.get("sp") != SPELL_DOMINATE_MIND or rec.get("r"):
            continue
        when, victim = rec["t"], rec["d"]
        casters = sorted(caster for caster, spans in duties.items()
                         if any(value == str(victim) and start < when + DOMINATE_MS and stop > when
                                for value, start, stop in spans))
        held = next(((cc["t"], CONTROL[cc["sp"]]) for cc in auras
                     if cc.get("d") == victim and cc.get("sp") in CONTROL and not cc.get("r")
                     and when <= cc["t"] <= when + DOMINATE_MS), None)
        out.append((when, victim, casters, held))
    return out


def frostbolts(trace: Trace) -> list[tuple[int, list[int]]]:
    """`(cast start, bots latched interrupter during it)` per Frostbolt she began."""
    bosses = boss_guids(trace)
    latch = holder_spans(trace, INTERRUPTER, pull_end(trace))
    out = []
    for rec in trace.of("cast"):
        if rec.get("s") not in bosses or rec.get("sp") not in FROSTBOLT:
            continue
        start = rec["t"]
        stop = start + int(rec.get("ct", 2000))
        kickers = sorted(guid for guid, spans in latch.items()
                         if any(value == "1" and begin < stop and end > start for value, begin, end in spans))
        out.append((start, kickers))
    return out


def dodge_counts(trace: Trace) -> dict[str, tuple[int, int]]:
    """Per hazard kind: `(times a bot entered it, bots)`."""
    entries: dict[str, int] = collections.defaultdict(int)
    bots: dict[str, set[int]] = collections.defaultdict(set)
    for guid, spans in holder_spans(trace, DODGE, pull_end(trace)).items():
        for value, _, _ in spans:
            if value != "none":
                entries[value] += 1
                bots[value].add(guid)
    return {value: (entries[value], len(bots[value])) for value in entries}


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
    print(f"    {'from':>9}  {'held':>9}  value")
    for value, start, stop in steps:
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {value}")


def show_targets(trace: Trace) -> None:
    print("TARGETS")
    if not mine(trace):
        return
    rows = target_time(trace)
    if not rows:
        print(f"  {TARGET} never written")
        return
    for value, (bots, held) in sorted(rows.items(), key=lambda row: -row[1][1]):
        print(f"  {value:<20} {bots:>3} bot(s)  {clock(held):>9} held in all")


def show_cc(trace: Trace) -> None:
    print("DOMINATE MIND")
    if not mine(trace):
        return
    rows = dominations(trace)
    if not rows:
        print("  nobody was dominated (10N casts none)")
        return
    for when, victim, casters, held in rows:
        human = " human" if victim in trace.humans else ""
        duty = names(trace, casters) if casters else "no duty"
        landed = f"{held[1]} {held[0] - when} ms after" if held else "no CC"
        print(f"  {clock(when)}  {trace.name(victim)}{human}  duty {duty}  {landed}")


def show_kicks(trace: Trace) -> None:
    print("FROSTBOLT")
    if not mine(trace):
        return
    rows = frostbolts(trace)
    if not rows:
        print("  she began no Frostbolt (phase 2 never came)")
        return
    unlatched = sum(1 for _, kickers in rows if not kickers)
    print(f"  {len(rows)} cast(s), {unlatched} with nobody latched as interrupter")
    for when, kickers in rows:
        print(f"  {clock(when)}  {names(trace, kickers)}")


def show_dodges(trace: Trace) -> None:
    print("DODGES")
    if not mine(trace):
        return
    rows = dodge_counts(trace)
    if not rows:
        print(f"  {DODGE} never left none")
        return
    for value, (entries, bots) in sorted(rows.items()):
        print(f"  {value:<10} {entries:>4} time(s) by {bots} bot(s)")


def show_cheats(trace: Trace) -> None:
    print("CHEATS")
    if not mine(trace):
        return
    rows = own_cheats(trace)
    if not rows:
        print("  no ldw.* or icc.* cheat fired")
        return
    for key, entry in rows:
        targets = entry["targets"] - {0}
        print(f"  {key}  {entry['n']} use(s) from {clock(entry['first'])}  by {names(trace, entry['bots'])}"
              f"  on {names(trace, targets) if targets else 'self'}")


SECTIONS = (
    ("phases", "ldw.phase: 1 while Mana Barrier holds, 2 once it breaks", show_phases),
    ("targets", "what the resolver had the bots on, by add kind", show_targets),
    ("cc", "each Dominate Mind, its ldw.cc caster and the CC that landed", show_cc),
    ("kicks", "her Frostbolts and who held ldw.interrupter", show_kicks),
    ("dodges", "ldw.dodge: who walked out of which hazard", show_dodges),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
