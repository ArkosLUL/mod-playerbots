#!/usr/bin/env python3
"""Read a Rotface pull from a RaidObs trace: the Big Oozes and their kiter, the carriers, the dodges.

    rotface.py <file>               every section
    rotface.py <file> --oozes       who the Big Oozes hit, the kiter and its duty
    rotface.py <file> --carriers    each bot walking its ooze to a merge, and each cure duty
    rotface.py <file> --dodges      each dodge by hazard and how it went
    rotface.py <file> --spread      the slots, the flooding quadrant and the ooze spot
    rotface.py <file> --vilegas     heroic Vile Gas: each victim
    rotface.py <file> --cheats      this encounter's cheat uses

Rotface has no phase. What the generic views miss here:

- **The Big Oozes' victims are the snapshot's target column.** Nothing fixates them, so a Big Ooze
  on anyone but the kiter is threat the kiter lost.
- **`rf.kiter` is the elected tank, `rf.bigooze` its duty**: `spot` waiting at the ooze spot for the
  first merge, `pickup <ooze>` with one not on it, `tank <ooze>` holding it at the spot under 3 stacks,
  `kite <ooze>` on the kite ring, `hold <ooze>` while the kiter carries an ooze itself, `none`.
- **`rf.carrier` is per bot**: `ooze` walking to a Big Ooze, `little` to another Little Ooze, `spot` to
  the ooze spot, `none` once it carries nothing. `rf.dispel` is keyed on the infected player, its value
  the bot that cures it; the duty opens only once the carrier stands where its ooze merges, or runs low.
- **`rf.dodge` is `<hazard> <branch> <yards>`**: hazard `spray`, `mark`, `sticky`, `flood`, `gas` or
  `ooze`; branch `move`, `tight` or `cross` (a walk across another hazard); `hold` while a dodge is in
  flight, `<hazard> none` with no clear spot, `<hazard> locked` when the walk was refused, `pinned` when
  the bot can't move.
- **`rf.slot` counts from 1.** 1-6 the inner ring, 7-19 the outer, 20 the stack behind him; `0` is a
  release. `rf.flood` and `rf.oozespot` are quadrants counter-clockwise from east, 1-4, `0` for none.
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
from raidobs.geometry import guids_of_entry  # noqa: E402
from raidobs.probes import emitted_keys, holder_spans, latch_spans, resolve_guids, silent_keys  # noqa: E402
from raidobs.space import threat_share  # noqa: E402
from raidobs.trace import Trace, clock, notes  # noqa: E402

ENCOUNTER = "rotface"
TITLE = "Rotface"
BIG_OOZE = (36899, 38123)
HEROIC = (2, 3)
HEROIC_ONLY = {"rf.vilegas"}
STACK_SLOT = 20
INNER_SLOTS = 6
QUADRANTS = {"1": "NE", "2": "NW", "3": "SW", "4": "SE"}


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


def missing_probes(trace: Trace) -> list[str]:
    gone = [key for key, _, _ in silent_keys(emitted_keys(trace), ENCOUNTER)]
    return [key for key in gone if heroic(trace) or key not in HEROIC_ONLY]


def show_banner(trace: Trace) -> None:
    icc.show_banner(trace)
    if not icc.is_icc(trace):
        return
    if not mine(trace):
        print(f"  not a {TITLE} pull, every section below reads empty")
        return
    gone = missing_probes(trace)
    print(f"probes absent from this trace: {', '.join(gone)}" if gone else "all rf.* probes present")


def big_oozes(trace: Trace) -> set[int]:
    return set().union(*(guids_of_entry(trace, entry) for entry in BIG_OOZE))


def ooze_share(trace: Trace) -> dict[str, float]:
    """Percent of the Big Oozes' sampled time on each role."""
    held, _ = threat_share(trace, big_oozes(trace), lambda when: True)
    total = sum(held.values())
    return {role: span * 100.0 / total for role, span in held.most_common()} if total else {}


def spans_by_bot(trace: Trace, key: str) -> dict[int, list[tuple[str, int, int]]]:
    end = icc.pull_end(trace)
    return {guid: [(resolve_guids(trace, value), start, stop) for value, start, stop in spans]
            for guid, spans in holder_spans(trace, key, end).items()}


def duty_rows(trace: Trace) -> dict[int, list[tuple[str, int, int]]]:
    return spans_by_bot(trace, "rf.bigooze")


def kiter_spans(trace: Trace) -> list[tuple[str, int, int]]:
    return [(resolve_guids(trace, value), start, stop)
            for value, start, stop in latch_spans(trace, "rf.kiter", icc.pull_end(trace))]


def print_spans(trace: Trace, rows: dict[int, list[tuple[str, int, int]]], skip: str | None = None) -> None:
    for guid, spans in sorted(rows.items(), key=lambda item: trace.name(item[0])):
        shown = [span for span in spans if span[0] != skip]
        if not shown:
            continue
        print(f"    {trace.name(guid)}")
        for value, start, stop in shown:
            print(f"      {clock(start):>9}  {clock(stop - start):>9}  {value}")


def show_oozes(trace: Trace) -> None:
    if refuse(trace, "OOZES"):
        return
    share = ooze_share(trace)
    if share:
        print("  Big Oozes' time on target: " + ", ".join(f"{role} {pct:.1f}%" for role, pct in share.items()))
    else:
        print("  no Big Ooze sampled with a target")
    kiters = kiter_spans(trace)
    if kiters:
        print("  kiter: " + ", ".join(f"{'none' if value in ('', '0') else value}@{clock(start)}"
                                      for value, start, _ in kiters))
    rows = duty_rows(trace)
    if not rows:
        print("  rf.bigooze never written")
        return
    print("\n  rf.bigooze, each change")
    print_spans(trace, rows)


def dispel_rows(trace: Trace) -> list[tuple[int, int, int]]:
    """`(when, carrier, dispeller)` for each cure duty handed out; a release is not a duty."""
    rows = []
    for rec in notes(trace, "rf.dispel"):
        txt = str(rec.get("txt", ""))
        if txt.isdigit() and int(txt) != 0:
            rows.append((rec["t"], rec["g"], int(txt)))
    return rows


def show_carriers(trace: Trace) -> None:
    if refuse(trace, "CARRIERS"):
        return
    rows = spans_by_bot(trace, "rf.carrier")
    walks = {guid: [span for span in spans if span[0] != "none"] for guid, spans in rows.items()}
    if not any(walks.values()):
        print("  rf.carrier never left none")
    else:
        counts = collections.Counter(span[0] for spans in walks.values() for span in spans)
        print("  merge walks: " + ", ".join(f"{rule} {n}" for rule, n in sorted(counts.items())))
        print_spans(trace, rows, skip="none")
    duties = dispel_rows(trace)
    if not duties:
        print("\n  rf.dispel never handed out")
        return
    print(f"\n  {len(duties)} cure duty(ies)")
    for when, carrier, dispeller in duties:
        print(f"    {clock(when):>9}  {trace.name(carrier):<14}  by {trace.name(dispeller)}")


def dodge_counts(trace: Trace) -> collections.Counter:
    """Dodge outcomes by `<hazard> <branch>`, yards dropped; `hold` rows are a walk already in flight."""
    counts: collections.Counter = collections.Counter()
    for rec in notes(trace, "rf.dodge"):
        words = str(rec.get("txt", "")).split()
        if words and words[0] != "hold":
            counts[" ".join(words[:2])] += 1
    return counts


def show_dodges(trace: Trace) -> None:
    if refuse(trace, "DODGES"):
        return
    counts = dodge_counts(trace)
    if not counts:
        print("  rf.dodge never written")
        return
    for outcome, n in sorted(counts.items(), key=lambda item: (-item[1], item[0])):
        print(f"    {n:>4}  {outcome}")
    per_bot = collections.Counter(rec["g"] for rec in notes(trace, "rf.dodge")
                                  if str(rec.get("txt", "")).split()[:1] not in (["hold"], []))
    print("\n  by bot: " + ", ".join(f"{trace.name(guid)} {n}" for guid, n in per_bot.most_common()))


def slot_name(value: str) -> str:
    if not value.isdigit():
        return value
    slot = int(value)
    if slot == 0:
        return "released"
    if slot == STACK_SLOT:
        return "stack"
    return f"inner {slot}" if slot <= INNER_SLOTS else f"outer {slot}"


def show_spread(trace: Trace) -> None:
    if refuse(trace, "SPREAD"):
        return
    end = icc.pull_end(trace)
    for label, key in (("flood", "rf.flood"), ("ooze spot", "rf.oozespot")):
        marks = [f"{QUADRANTS.get(value, 'none')}@{clock(start)}" for value, start, _ in latch_spans(trace, key, end)]
        print(f"  {label}: " + (", ".join(marks) if marks else "never written"))
    slots = holder_spans(trace, "rf.slot", end)
    if not slots:
        print("  rf.slot never written")
        return
    print("\n  rf.slot per bot, each slot it held")
    for guid, spans in sorted(slots.items(), key=lambda item: trace.name(item[0])):
        held = ", ".join(f"{slot_name(value)}@{clock(start)}" for value, start, _ in spans)
        print(f"    {trace.name(guid):<14}  {held}")


def show_vilegas(trace: Trace) -> None:
    if refuse(trace, "VILE GAS"):
        return
    if not heroic(trace):
        print("  normal mode: Putricide casts no Vile Gas on Rotface")
        return
    rows = notes(trace, "rf.vilegas")
    if not rows:
        print("  rf.vilegas never written")
        return
    print(f"  {len(rows)} cast(s)")
    for rec in rows:
        human = " (human)" if rec["g"] in trace.humans else ""
        print(f"    {clock(rec['t']):>9}  {trace.name(rec['g'])}{human}")
    counts = collections.Counter(rec["g"] for rec in rows)
    print("  by victim: " + ", ".join(f"{trace.name(guid)} {n}" for guid, n in counts.most_common()))


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
    ("oozes", "who the Big Oozes hit, the kiter and its duty", show_oozes),
    ("carriers", "merge walks and cure duties", show_carriers),
    ("dodges", "each dodge by hazard and how it went", show_dodges),
    ("spread", "slots, the flooding quadrant and the ooze spot", show_spread),
    ("vilegas", "heroic Vile Gas victims", show_vilegas),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
