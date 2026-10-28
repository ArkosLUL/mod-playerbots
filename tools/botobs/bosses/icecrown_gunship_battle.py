#!/usr/bin/env python3
"""Read an Icecrown Gunship Battle pull from a RaidObs trace: the freezes, the roster, the boarding party.

    icecrown_gunship_battle.py <file>             every section
    icecrown_gunship_battle.py <file> --phases    gsb.phase, and the captain's stacks at each thaw and retreat
    icecrown_gunship_battle.py <file> --duties    gsb.duty: who boards, who mans a cannon, who holds the deck
    icecrown_gunship_battle.py <file> --decks     per bot, time on each deck and the trips across
    icecrown_gunship_battle.py <file> --boarding  per freeze, when the party went and came back
    icecrown_gunship_battle.py <file> --targets   time per pick rule, and the stands held
    icecrown_gunship_battle.py <file> --cannons   what the cannons aimed at, and any overheat
    icecrown_gunship_battle.py <file> --dodges    Rocket Artillery and Burning Pitch dodges
    icecrown_gunship_battle.py <file> --cheats    this encounter's cheat uses

What the generic views miss here:

- **`gsb.deck` is by position against the core's own y bounds, not the transport**, so a bot just landed
  reads `enemy` before its transport check moves it across. `between` is airborne, `falling` under the deck
  until the core's Check for Players puts it back; a seated gunner reads `cannon`.
- **`gsb.board` is per bot**: `wait` at the launch point until a tank holds the captain, `go`, `back` once
  the mage dies, `retreat` on the captain's stacks. A bot outside the boarding party stays `home`.
- **`gsb.fury` and `gsb.retreat` are per instance**: the stack count at each thaw, and the one a retreat
  latched at, back to 0 when his stacks fall off.
- **A pull can file under the captain's name.** The credit is a spell, so without the boss-state table
  the trace takes `muradin-bronzebeard` or `high-overlord-saurfang`, which the aliases fold here.
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
from raidobs.trace import Trace, cheat_uses, clock, combat_deaths, notes  # noqa: E402

SLUG = "icecrown-gunship-battle"
NAME = "Gunship Battle"
PHASE = "gsb.phase"
DUTY = "gsb.duty"
DECK = "gsb.deck"
BOARD = "gsb.board"
STAND = "gsb.stand"
TARGET = "gsb.target"
CANNON = "gsb.cannon"
DODGE = "gsb.dodge"
RETREAT = "gsb.retreat"
FURY = "gsb.fury"
DECKS = ("friendly", "between", "enemy", "cannon", "falling")
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


def freezes(trace: Trace) -> list[tuple[int, int]]:
    """`(start, stop)` per Below Zero window."""
    return [(start, stop) for value, start, stop in phase_steps(trace) if value == "frozen"]


def instance_marks(trace: Trace, key: str) -> list[tuple[int, int]]:
    """`(t, value)` for a per-instance number, skipping the zeros that clear it."""
    out = []
    for rec in sorted(notes(trace, key), key=lambda rec: rec["t"]):
        try:
            value = int(str(rec.get("txt", "0")))
        except ValueError:
            continue
        if value:
            out.append((rec["t"], value))
    return out


def duties(trace: Trace) -> dict[str, list[int]]:
    """Per duty, the bots that held it last."""
    out: dict[str, list[int]] = collections.defaultdict(list)
    for guid, spans in holder_spans(trace, DUTY, pull_end(trace)).items():
        out[spans[-1][0]].append(guid)
    return dict(out)


def deck_rows(trace: Trace) -> dict[int, dict]:
    """Per bot: ms held on each deck value, and `trips`, its spans on the enemy deck."""
    out = {}
    for guid, spans in holder_spans(trace, DECK, pull_end(trace)).items():
        held = collections.Counter()
        for value, start, stop in spans:
            held[value] += stop - start
        out[guid] = {"held": dict(held), "trips": sum(1 for value, _, _ in spans if value == "enemy")}
    return out


def boarding(trace: Trace) -> list[dict]:
    """Per freeze: its window, and the first `go` and `back` any bot noted inside it, with who went."""
    boards = holder_spans(trace, BOARD, pull_end(trace))
    out = []
    for start, stop in freezes(trace):
        went, first_go, first_back, retreats = set(), None, None, 0
        for guid, spans in boards.items():
            for value, begin, _ in spans:
                if not start <= begin <= stop + 15000:
                    continue
                if value == "go":
                    went.add(guid)
                    first_go = begin if first_go is None else min(first_go, begin)
                elif value == "back":
                    first_back = begin if first_back is None else min(first_back, begin)
                elif value == "retreat":
                    retreats += 1
        out.append({"start": start, "stop": stop, "go": first_go, "back": first_back, "went": sorted(went),
                    "retreats": retreats})
    return out


def span_time(trace: Trace, key: str) -> dict[str, tuple[int, int]]:
    """Per value of a per-bot key: `(bots, ms held summed over bots)`."""
    bots: dict[str, set[int]] = collections.defaultdict(set)
    held: dict[str, int] = collections.defaultdict(int)
    for guid, spans in holder_spans(trace, key, pull_end(trace)).items():
        for value, start, stop in spans:
            bots[value].add(guid)
            held[value] += stop - start
    return {value: (len(bots[value]), held[value]) for value in held}


def entries(trace: Trace, key: str, idle: str = "none") -> dict[str, tuple[int, int]]:
    """Per value but `idle`: `(times a bot entered it, bots)`."""
    count: dict[str, int] = collections.defaultdict(int)
    bots: dict[str, set[int]] = collections.defaultdict(set)
    for guid, spans in holder_spans(trace, key, pull_end(trace)).items():
        for value, _, _ in spans:
            if value != idle:
                count[value] += 1
                bots[value].add(guid)
    return {value: (count[value], len(bots[value])) for value in count}


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
    print(f"    {'from':>9}  {'held':>9}  {PHASE}")
    for value, start, stop in steps:
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {value}")
    for key, label in ((FURY, "stacks at the thaw"), (RETREAT, "retreat latched at")):
        marks = instance_marks(trace, key)
        if marks:
            print(f"  {label}: " + ", ".join(f"{value} at {clock(when)}" for when, value in marks))


def show_duties(trace: Trace) -> None:
    print("DUTIES")
    if not mine(trace):
        return
    rows = duties(trace)
    if not rows:
        print(f"  {DUTY} never written")
        return
    for duty in sorted(rows):
        print(f"  {duty:<11} {names(trace, rows[duty])}")


def show_decks(trace: Trace) -> None:
    print("DECKS")
    if not mine(trace):
        return
    rows = deck_rows(trace)
    if not rows:
        print(f"  {DECK} never written")
        return
    print(f"  {'bot':<14}" + "".join(f"{value:>10}" for value in DECKS) + "  trips")
    for guid in sorted(rows, key=lambda guid: trace.name(guid)):
        held = rows[guid]["held"]
        cells = "".join(f"{clock(held.get(value, 0)):>10}" for value in DECKS)
        print(f"  {trace.name(guid):<14}{cells}  {rows[guid]['trips']:>5}")


def show_boarding(trace: Trace) -> None:
    print("BOARDING")
    if not mine(trace):
        return
    rows = boarding(trace)
    if not rows:
        print("  no freeze")
        return
    for row in rows:
        went = f"go +{clock(row['go'] - row['start'])}" if row["go"] is not None else "nobody went"
        back = f"back +{clock(row['back'] - row['start'])}" if row["back"] is not None else "no return"
        retreat = f"  {row['retreats']} retreat(s)" if row["retreats"] else ""
        print(f"  frozen {clock(row['start'])} for {clock(row['stop'] - row['start'])}: {went}, {back}{retreat}"
              f"  ({len(row['went'])} bot(s))")


def show_targets(trace: Trace) -> None:
    print("TARGETS")
    if not mine(trace):
        return
    for key in (TARGET, STAND):
        rows = span_time(trace, key)
        if not rows:
            print(f"  {key} never written")
            continue
        print(f"  {key}: " + ", ".join(f"{value} {bots} bot(s) {clock(held)}"
                                       for value, (bots, held) in sorted(rows.items())))


def show_cannons(trace: Trace) -> None:
    print("CANNONS")
    if not mine(trace):
        return
    rows = span_time(trace, CANNON)
    if not rows:
        print(f"  {CANNON} never written")
        return
    for value, (bots, held) in sorted(rows.items()):
        print(f"  {value:<9} {bots} gunner(s)  {clock(held)}")


def show_dodges(trace: Trace) -> None:
    print("DODGES")
    if not mine(trace):
        return
    rows = entries(trace, DODGE)
    if not rows:
        print("  no dodge")
        return
    for value, (count, bots) in sorted(rows.items()):
        print(f"  {value:<7} {count} dodge(s) by {bots} bot(s)")


def show_cheats(trace: Trace) -> None:
    print("CHEATS")
    if not mine(trace):
        return
    rows = own_cheats(trace)
    if not rows:
        print("  no gsb.* or icc.* cheat fired")
        return
    for key, entry in rows:
        print(f"  {key}  {entry['n']} use(s) from {clock(entry['first'])}  by {names(trace, entry['bots'])}")


SECTIONS = (
    ("phases", "gsb.phase, and the captain's stacks at each thaw and retreat", show_phases),
    ("duties", "gsb.duty: who boards, who mans a cannon, who holds the deck", show_duties),
    ("decks", "per bot, time on each deck and the trips across", show_decks),
    ("boarding", "per freeze, when the party went and came back", show_boarding),
    ("targets", "time per pick rule, and the stands held", show_targets),
    ("cannons", "what the cannons aimed at, and any overheat", show_cannons),
    ("dodges", "Rocket Artillery and Burning Pitch dodges", show_dodges),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
