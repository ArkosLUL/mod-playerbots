#!/usr/bin/env python3
"""Read an Icecrown Citadel RaidObs trace raid-wide: which bot cheats fired, and the encounter's phases.

    icecrown_citadel.py <file>            every section
    icecrown_citadel.py <file> --cheats   per cheat key: uses, first use, the bots and whom they acted on
    icecrown_citadel.py <file> --phases   the encounter's `<prefix>.phase` timeline

Raid-wide, so it reads a trace of any ICC encounter. What the generic views miss here:

- **A `cheat` row counts the uses since its bot's previous row for the key.** A bot writes one on its
  first use, then at most one per 10 s, and the close flushes the rest, so a key's uses are the sum of
  `n`, never the row count.
- **`on` lists who was acted on, not how often.** A row names only the unit of the use that wrote it,
  and its `n` spans every use since. `self` means the bot itself, since the recorder leaves `tgt` out
  then. ` human` after a name marks a human player: the `EnableICCBuffs` buffs and some strips reach
  humans too.
- **`other encounter` marks a key whose prefix names a different boss.** ICC has no encounter gate,
  so that is one boss's code running in another's pull, or a key filed under the wrong prefix.
- **A phase step starts at the first bot to note it.** `<prefix>.phase` may be one raid-wide value or
  one per bot, and read as a single change-only stream both give the same steps.
"""
from __future__ import annotations

import pathlib
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import ICC_KEY_PREFIXES, ICC_MAP, encounter_of, map_of, prefix_matches_boss  # noqa: E402
from raidobs.probes import latch_spans  # noqa: E402
from raidobs.trace import Trace, cheat_uses, clock, combat_deaths  # noqa: E402

DIFFICULTIES = {0: "10N", 1: "25N", 2: "10H", 3: "25H"}
CHEAT_SCHEMA = 14
PHASE_KIND = "phase"
HUMAN_SUFFIX = "human"
SELF = 0


def pull_end(trace: Trace) -> int:
    ends = trace.of("end")
    if ends:
        return ends[-1]["t"]
    return trace.records[-1].get("t", 0) if trace.records else 0


def difficulty(trace: Trace) -> str:
    return DIFFICULTIES.get(trace.header.get("diff"), "?")


def is_icc(trace: Trace) -> bool:
    return trace.header.get("map", map_of(trace.path)) == ICC_MAP


def who(trace: Trace, guid: int) -> str:
    if guid == SELF:
        return "self"
    return trace.name(guid) + (f" {HUMAN_SUFFIX}" if guid in trace.humans else "")


def ranked(trace: Trace, counts: dict[int, int]) -> str:
    """`name uses` per unit, most uses first."""
    order = sorted(counts.items(), key=lambda item: (-item[1], who(trace, item[0])))
    return ", ".join(f"{who(trace, guid)} {uses}" for guid, uses in order)


def named(trace: Trace, guids: set[int]) -> str:
    return ", ".join(sorted(who(trace, guid) for guid in guids))


def cheat_rows(trace: Trace) -> list[dict]:
    """Each key's uses, first use first, as `cheat_uses` gives them plus `key` and `own`: False for a key
    whose prefix names another encounter, None when the trace names no contract encounter."""
    encounter = encounter_of(trace)
    named = encounter in ICC_KEY_PREFIXES
    rows = [{"key": key, **entry,
             "own": prefix_matches_boss(key.partition(".")[0], encounter) if named else None}
            for key, entry in cheat_uses(trace).items()]
    return sorted(rows, key=lambda row: (row["first"], row["key"]))


def phase_key(trace: Trace) -> str | None:
    prefix = ICC_KEY_PREFIXES.get(encounter_of(trace))
    return f"{prefix}.{PHASE_KIND}" if prefix else None


def phase_steps(trace: Trace) -> list[tuple[str, int, int]]:
    """`(phase, start, stop)` per step, the last one running to the pull's end."""
    key = phase_key(trace)
    if not key:
        return []
    end = pull_end(trace)
    return [(value, start, max(stop, start)) for value, start, stop in latch_spans(trace, key, end)]


def show_banner(trace: Trace) -> None:
    end = pull_end(trace)
    outcome = trace.of("end")[-1].get("out", "?") if trace.of("end") else "no end record"
    encounter = encounter_of(trace)
    print(f"{encounter} {difficulty(trace)}  {outcome} at {clock(end)}  {len(combat_deaths(trace))} death(s)")
    if not is_icc(trace):
        print("  not an Icecrown Citadel trace, every section below reads empty")
        return
    if encounter not in ICC_KEY_PREFIXES:
        print(f"  {encounter} is no encounter in the ICC naming contract, so no phase key applies")


def show_cheats(trace: Trace) -> None:
    print("CHEATS")
    if not is_icc(trace):
        print("  not an Icecrown Citadel trace")
        return
    version = trace.header.get("v") or 0
    if version < CHEAT_SCHEMA:
        print(f"  schema v{version} predates the cheat record (v{CHEAT_SCHEMA})")
        return

    cfg = trace.header.get("cfg") or {}
    print(f"  conf BotCheats {cfg.get('cheats') or '-'}, EnableICCBuffs {cfg.get('iccbuffs', '-')}")
    rows = cheat_rows(trace)
    if not rows:
        print("  no cheat fired")
        return
    print(f"  {len(rows)} kind(s), {sum(row['n'] for row in rows)} use(s), first used first")
    for row in rows:
        tag = "  other encounter" if row["own"] is False else ""
        print(f"\n  {row['key']}  {row['n']} use(s) from {clock(row['first'])}{tag}")
        print(f"    by  {ranked(trace, row['bots'])}")
        print(f"    on  {named(trace, row['targets'])}")


def show_phases(trace: Trace) -> None:
    print("PHASES")
    if not is_icc(trace):
        print("  not an Icecrown Citadel trace")
        return
    key = phase_key(trace)
    if not key:
        print(f"  {encounter_of(trace)} has no contract prefix, so there is no phase key to read")
        return
    steps = phase_steps(trace)
    if not steps:
        print(f"  {key} never written")
        return
    print(f"  {key}, each step from the first bot to note it\n")
    print(f"    {'from':>9}  {'held':>9}  phase")
    for value, start, stop in steps:
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {value}")


SECTIONS = (
    ("cheats", "per cheat key: uses, first use, the bots and whom they acted on", show_cheats),
    ("phases", "the encounter's <prefix>.phase timeline", show_phases),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
