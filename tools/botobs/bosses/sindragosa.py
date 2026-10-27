#!/usr/bin/env python3
"""Read a Sindragosa pull from a RaidObs trace: phases, the air phase hide, tomb icons, flares, cheats.

    sindragosa.py <file>            every section
    sindragosa.py <file> --phases   sg.phase: ground, air, last
    sindragosa.py <file> --tombs    each bot's tomb group, and the hide rules run in each air phase
    sindragosa.py <file> --icons    every move of the skull, cross and star, and the rule behind it
    sindragosa.py <file> --flares   the smoke flares on the tomb spots
    sindragosa.py <file> --cheats   this encounter's cheat uses

What the generic views miss here:

- **`sg.phase` is her script's own state**, per bot: `air` from takeoff to touchdown, while her air
  phase holds her passive, then `last` once she is at 35% or under. A phase 3 reached in the air
  starts at touchdown, as her script starts it.
- **`sg.group` and `sg.tomb` are traced maps**: each bot's air phase tomb group (0 to 2, two groups on
  10-man) and the tomb it hides behind. Groups are kept for the pull.
- **`sg.losmove` is per bot, the rule that placed it in the air**: `hide` walking behind its tomb,
  `parked` there, `replay` finishing a walk whose tomb went, `zone` waiting behind its group's spot
  with no tomb there, `fallback` freed from a tomb and hiding behind the nearest, `tombed`, `wait` (no
  Frost Bomb or no tomb, so it stops attacking), `lost`, and `none` once she leaves her air stations.
- **The icons are written only by the bot that moved them**, rule and unit or `clear`: `tanktomb`,
  `boss` and `tomb` from the ground positioning; `extra` and `sticky` from the air phase groups;
  `tomb`, `preburn`, `hold` and `kill` from the last phase hide.
- **`sg.flaredred` lists the tomb spots flared red** (0 to 2, the middle one 25-man only) and
  `sg.flaredblue` the last phase spot. Each spot is flared once before the last phase and once in it,
  and a flare burns 5 min.
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
from raidobs.probes import emitted_keys, holder_spans, resolve_guids, silent_keys  # noqa: E402
from raidobs.trace import Trace, clock, notes  # noqa: E402

ENCOUNTER = "sindragosa"
TITLE = "Sindragosa"
TWENTY_FIVE = (1, 3)
TWENTY_FIVE_ONLY = {"sg.star"}
ICONS = ("sg.skull", "sg.cross", "sg.star")
HIDE_RULES = ("hide", "parked", "replay", "zone", "fallback", "tombed", "wait", "lost")


def mine(trace: Trace) -> bool:
    return encounter_of(trace) == ENCOUNTER


def refuse(trace: Trace, title: str) -> bool:
    print(title)
    if mine(trace):
        return False
    print(f"  not a {TITLE} pull")
    return True


def twenty_five(trace: Trace) -> bool:
    return trace.header.get("diff") in TWENTY_FIVE


def missing_probes(trace: Trace) -> list[str]:
    gone = [key for key, _, _ in silent_keys(emitted_keys(trace), ENCOUNTER)]
    return [key for key in gone if twenty_five(trace) or key not in TWENTY_FIVE_ONLY]


def show_banner(trace: Trace) -> None:
    icc.show_banner(trace)
    if not icc.is_icc(trace):
        return
    if not mine(trace):
        print(f"  not a {TITLE} pull, every section below reads empty")
        return
    gone = missing_probes(trace)
    print(f"probes absent from this trace: {', '.join(gone)}" if gone else "all sg.* probes present")


def show_phases(trace: Trace) -> None:
    if refuse(trace, "PHASES"):
        return
    steps = icc.phase_steps(trace)
    if not steps:
        print("  sg.phase never written")
        return
    print("  sg.phase, each step from the first bot to note it\n")
    print(f"    {'from':>9}  {'held':>9}  phase")
    for value, start, stop in steps:
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {value}")


def air_hides(trace: Trace) -> list[dict]:
    """Per air phase: `start`, `stop`, and `rules`, the count of each sg.losmove switch inside it."""
    switches = notes(trace, "sg.losmove")
    rows = []
    for value, start, stop in icc.phase_steps(trace):
        if value != "air":
            continue
        rules = collections.Counter(str(rec.get("txt", "")) for rec in switches if start <= rec["t"] < stop)
        rows.append({"start": start, "stop": stop, "rules": {rule: rules[rule] for rule in HIDE_RULES if rules[rule]}})
    return rows


def last_groups(trace: Trace) -> dict[str, list[int]]:
    """Each sg.group value to the bots whose last note held it."""
    groups: dict[str, list[int]] = collections.defaultdict(list)
    for guid, spans in holder_spans(trace, "sg.group", icc.pull_end(trace)).items():
        groups[spans[-1][0]].append(guid)
    return dict(groups)


def show_tombs(trace: Trace) -> None:
    if refuse(trace, "TOMBS"):
        return
    groups = last_groups(trace)
    if groups:
        print("  sg.group, each bot's last group")
        for value in sorted(groups):
            names = sorted(trace.name(guid) for guid in groups[value])
            print(f"    {value}  {len(names)}: {', '.join(names)}")
    else:
        print("  sg.group never written")
    tombs = holder_spans(trace, "sg.tomb", icc.pull_end(trace))
    if tombs:
        changes = sum(len(spans) for spans in tombs.values())
        print(f"  sg.tomb: {changes} tomb pick(s) over {len(tombs)} bot(s)")
    freed = {rec["g"] for rec in notes(trace, "sg.freedfallback") if str(rec.get("txt", "")) == "1"}
    if freed:
        print(f"  freed from a tomb and hiding behind the nearest: {', '.join(sorted(trace.name(g) for g in freed))}")
    hides = air_hides(trace)
    if not hides:
        print("\n  no air phase in sg.phase")
        return
    print("\n  sg.losmove switches per air phase")
    for row in hides:
        rules = ", ".join(f"{rule} {count}" for rule, count in row["rules"].items()) or "-"
        print(f"    {clock(row['start']):>9}  {clock(row['stop'] - row['start']):>9}  {rules}")


def show_icons(trace: Trace) -> None:
    if refuse(trace, "ICONS"):
        return
    rows = sorted((rec for key in ICONS for rec in notes(trace, key)), key=lambda rec: (rec["t"], rec["k"]))
    if not rows:
        print("  sg.skull, sg.cross and sg.star never written")
        return
    for rec in rows:
        icon = rec["k"].partition(".")[2]
        print(f"    {clock(rec['t']):>9}  {icon:<5}  {trace.name(rec['g']):<14}  "
              f"{resolve_guids(trace, str(rec.get('txt', '')))}")


def show_flares(trace: Trace) -> None:
    if refuse(trace, "FLARES"):
        return
    rows = sorted(notes(trace, "sg.flaredred") + notes(trace, "sg.flaredblue"), key=lambda rec: rec["t"])
    if not rows:
        print("  no flare noted")
        return
    for rec in rows:
        what = "red" if rec["k"] == "sg.flaredred" else "blue"
        print(f"    {clock(rec['t']):>9}  {what:<4}  {trace.name(rec['g']):<14}  {rec.get('txt', '')}")


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
    ("phases", "sg.phase: ground, air, last", show_phases),
    ("tombs", "each bot's tomb group, and the hide rules run in each air phase", show_tombs),
    ("icons", "every move of the skull, cross and star, and its rule", show_icons),
    ("flares", "the smoke flares on the tomb spots", show_flares),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
