#!/usr/bin/env python3
"""Read a Svalna gauntlet pull from a RaidObs trace: its Ymirjar, targets, taunts, kicks, dodges, cheats.

    svalna_gauntlet.py <file>               every section
    svalna_gauntlet.py <file> --packs       each kind of Ymirjar and summon: how many were seen, how many died
    svalna_gauntlet.py <file> --targets     per bot: the resolver rules it was handed
    svalna_gauntlet.py <file> --taunts      per tank: each rescue taunt it owed, from when
    svalna_gauntlet.py <file> --kicks       per bot: the Deathbringer casts it was elected to kick
    svalna_gauntlet.py <file> --dodges      per bot: Whirlwind and Frozen Orb dodges
    svalna_gauntlet.py <file> --defensives  tank buttons and calls to a human tank
    svalna_gauntlet.py <file> --cheats      cheat uses in this pull

The gauntlet has no boss state and no phase: its definition's tick opens the trace through `MarkPull`
while a Ymirjar fights the raid, and Svalna's own state replaces it once she engages. Its keys share her
`ss.` prefix, so the banner names only the silent `ss.gauntlet.*` ones. What the generic views miss here:

- **Crok walks the hall and waits at each pack**: Battle-Maidens, Huntresses and Warlords in three packs,
  two Deathbringer and Frostbinder pairs among them. A Deathbringer's Summon Undead raises five
  Corrupted Ymirjar; a 25-man Huntress calls a Warhawk.
- **`ss.gauntlet.target` is the resolver's rule**: a player's `skull`; for a tank `rescue` (a melee add
  on a non-tank), `held` (a melee add it or no other tank holds) or `none`; for a damage dealer the kind
  it was handed, `deathbringer` first for ranged and `frostbinder` first for melee, then `huntress`,
  `battlemaiden`, `warlord`, `corrupted`, `warhawk`, or `none`.
- **`ss.gauntlet.taunt` is the kind of melee add a tank's rescue taunt is due on**, `none` once not.
- **`ss.gauntlet.interrupt` is the bot elected to kick a Deathbringer**: `summon` (Summon Undead) or
  `bolt` (Empowered Shadow Bolt) while it holds the kick, `none` otherwise.
- **`ss.gauntlet.dodge` is a bot leaving a hazard about to land**: `whirlwind` (a Warlord's 2 s cast,
  8 yd, tanks stay), `orb` (a Frostbinder's Frozen Orb in its last 3 s, 6 yd), `blocked` with no clear
  spot, `none` once out.
- **`ss.gauntlet.defensive` is the tank ladder's pick**, a cast name, `idle` outside a window: a tank a
  Ymirjar hits under 40%. `ss.gauntlet.humancall` is a call to a human tank.
- **No gauntlet code cheats.** Any `cheat` row here is another encounter's code.
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
from raidobs.probes import declared_keys, emitted_keys, holder_spans, resolve_guids  # noqa: E402
from raidobs.trace import Trace, clock, first_deaths, notes  # noqa: E402

ENCOUNTER = "svalna-gauntlet"
TITLE = "Svalna gauntlet"
PREFIX = "ss.gauntlet."
KINDS = (
    (38125, "Deathbringer"), (37127, "Frostbinder"), (37134, "Huntress"), (37132, "Battle-Maiden"),
    (37133, "Warlord"), (38184, "Corrupted Ymirjar"), (38154, "Warhawk"),
)
OFF = ("none",)
DEFENSIVE = "ss.gauntlet.defensive"
HUMAN_CALL = "ss.gauntlet.humancall"


def mine(trace: Trace) -> bool:
    return encounter_of(trace) == ENCOUNTER


def refuse(trace: Trace, title: str) -> bool:
    print(title)
    if mine(trace):
        return False
    print(f"  not a {TITLE} pull")
    return True


def missing_probes(trace: Trace) -> list[str]:
    """Declared `ss.gauntlet.*` keys the trace never wrote, read off the source rather than the slug's
    prefix row, which Svalna's own pulls share."""
    emitted = emitted_keys(trace)
    return sorted(key for key in declared_keys() if key.startswith(PREFIX) and key not in emitted)


def show_banner(trace: Trace) -> None:
    icc.show_banner(trace)
    if not icc.is_icc(trace):
        return
    if not mine(trace):
        print(f"  not a {TITLE} pull, every section below reads empty")
        return
    gone = missing_probes(trace)
    print(f"probes absent from this trace: {', '.join(gone)}" if gone else f"all {PREFIX}* probes present")


def pack_rows(trace: Trace) -> list[tuple[str, int, int]]:
    """`(kind, seen, died)` per kind of Ymirjar or summon the trace named."""
    dead = first_deaths(trace)
    rows = []
    for entry, kind in KINDS:
        units = [guid for guid, known in trace.entries.items() if known == entry]
        if units:
            rows.append((kind, len(units), sum(1 for guid in units if guid in dead)))
    return rows


def show_packs(trace: Trace) -> None:
    if refuse(trace, "PACKS"):
        return
    rows = pack_rows(trace)
    if not rows:
        print("  no Ymirjar seen")
        return
    print(f"    {'kind':<18}  {'seen':>4}  {'died':>4}")
    for kind, seen, died in rows:
        print(f"    {kind:<18}  {seen:>4}  {died:>4}")


def rule_counts(trace: Trace, key: str, skip: tuple[str, ...] = OFF) -> dict[int, collections.Counter]:
    """Per guid: how many spans each value of `key` started, `skip` left out."""
    return {guid: collections.Counter(value for value, _, _ in spans if value not in skip)
            for guid, spans in holder_spans(trace, key, icc.pull_end(trace)).items()}


def counted(counts: collections.Counter) -> str:
    return ", ".join(f"{key} {counts[key]}" for key in sorted(counts)) or "-"


def show_rules(trace: Trace, title: str, key: str, intro: str, skip: tuple[str, ...] = OFF) -> None:
    if refuse(trace, title):
        return
    rows = {guid: counts for guid, counts in rule_counts(trace, key, skip).items() if counts}
    if not rows:
        print(f"  {key} never written")
        return
    print(f"  {intro}\n")
    for guid in sorted(rows, key=trace.name):
        print(f"    {trace.name(guid):<14}  {counted(rows[guid])}")


def show_targets(trace: Trace) -> None:
    show_rules(trace, "TARGETS", "ss.gauntlet.target", "per bot: spans of each resolver rule", skip=())


def show_taunts(trace: Trace) -> None:
    show_rules(trace, "TAUNTS", "ss.gauntlet.taunt", "per tank: spans of each rescue taunt it owed")


def show_kicks(trace: Trace) -> None:
    show_rules(trace, "KICKS", "ss.gauntlet.interrupt", "per bot: spans of each Deathbringer cast it was to kick")


def show_dodges(trace: Trace) -> None:
    show_rules(trace, "DODGES", "ss.gauntlet.dodge", "per bot: spans of each dodge rule")


def show_defensives(trace: Trace) -> None:
    if refuse(trace, "DEFENSIVES"):
        return
    rows = holder_spans(trace, DEFENSIVE, icc.pull_end(trace))
    offered = {guid: [(value, start) for value, start, _ in spans if value != "idle"] for guid, spans in rows.items()}
    offered = {guid: picks for guid, picks in offered.items() if picks}
    if not offered:
        print(f"  {DEFENSIVE} never offered a button")
    for guid in sorted(offered, key=trace.name):
        print(f"    {trace.name(guid):<14}  " + ", ".join(f"{value} {clock(start)}" for value, start in offered[guid]))
    calls = notes(trace, HUMAN_CALL)
    if calls:
        print("  calls to a human tank  " + ", ".join(f"{resolve_guids(trace, str(rec['txt']))} {clock(rec['t'])}"
                                                    for rec in calls))


def show_cheats(trace: Trace) -> None:
    if refuse(trace, "CHEATS"):
        return
    if (trace.header.get("v") or 0) < icc.CHEAT_SCHEMA:
        print(f"  schema v{trace.header.get('v') or 0} predates the cheat record (v{icc.CHEAT_SCHEMA})")
        return
    rows = icc.cheat_rows(trace)
    if not rows:
        print("  no cheat fired")
        return
    print(f"  {sum(row['n'] for row in rows)} use(s), none from gauntlet code: icecrown_citadel.py --cheats")


SECTIONS = (
    ("packs", "each kind of Ymirjar and summon: how many were seen, how many died", show_packs),
    ("targets", "per bot: the resolver rules it was handed", show_targets),
    ("taunts", "per tank: each rescue taunt it owed, from when", show_taunts),
    ("kicks", "per bot: the Deathbringer casts it was elected to kick", show_kicks),
    ("dodges", "per bot: Whirlwind and Frozen Orb dodges", show_dodges),
    ("defensives", "tank buttons and calls to a human tank", show_defensives),
    ("cheats", "cheat uses in this pull", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
