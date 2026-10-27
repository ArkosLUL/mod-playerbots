#!/usr/bin/env python3
"""Read a Blood-Queen Lana'thel pull from a RaidObs trace: air phases, bites, Pact, shadows, slots, cheats.

    blood_queen_lana_thel.py <file>             every section
    blood_queen_lana_thel.py <file> --phases    bql.phase: ground, center, air
    blood_queen_lana_thel.py <file> --bites     each Frenzied Bloodthirst: who the biter went for, and how it ended
    blood_queen_lana_thel.py <file> --pact      each Pact of the Darkfallen: where each member went
    blood_queen_lana_thel.py <file> --shadows   each Swarming Shadows: the lanes its victim walked
    blood_queen_lana_thel.py <file> --slots     per bot: its ground and air slots, and how often they moved
    blood_queen_lana_thel.py <file> --cheats    this encounter's cheat uses

What the generic views miss here:

- **`bql.phase` follows her own air phase**: `center` from the moment she turns passive to walk to the
  middle of the room (Incite Terror there), `air` from takeoff, gravity off, until she lands, `ground`
  otherwise. Per bot, so a step starts at the first bot to note it.
- **`bql.bite` is per biter**: `dps` (a damage dealer or healer) or `tank` and the target, `nobody`
  when none was free, `none` outside Frenzied Bloodthirst. The pick is re-derived each tick, nearest
  first, so a switch inside one window is the target moving.
- **`bql.pact` is per Pact member**: `tank` and the tank walked to, `hold` for that tank itself,
  `center n` for the middle of n members, `partner` and the one other member, `none` outside it.
- **`bql.curve` is the wall lane a shadowed bot walks**, 0 the outermost, and is erased once its
  Swarming Shadows ends. The erase writes `0` too, so only a note inside a window is a lane.
- **`bql.slot` and `bql.airslot` write `0` on an erase as well**, so a slot 0 can be a bot giving its
  slot up.
- **Frenzied Bloodthirst is row 2083** (70877 10-man, 71474 25-man, the same ids on heroic). A window
  ends `bit` on an Essence of the Blood Queen (row 2087) within 1 s, `frenzy` on Uncontrollable Frenzy
  70923, the mind control for a bite never made.
"""
from __future__ import annotations

import pathlib
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from bosses import icecrown_citadel as icc  # noqa: E402
from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of  # noqa: E402
from raidobs.probes import emitted_keys, holder_spans, resolve_guids, silent_keys  # noqa: E402
from raidobs.trace import Trace, clock, first_deaths, notes  # noqa: E402

ENCOUNTER = "blood-queen-lana-thel"
TITLE = "Blood-Queen Lana'thel"
FRENZIED_BLOODTHIRST = (70877, 71474)
ESSENCE_OF_THE_BLOOD_QUEEN = (70879, 71525, 71530, 71531)
UNCONTROLLABLE_FRENZY = 70923
PACT_OF_THE_DARKFALLEN = 71340
SWARMING_SHADOWS = (71264, 71265)
OUTCOME_SLACK_MS = 1000
MERGE_GAP_MS = 500


def mine(trace: Trace) -> bool:
    return encounter_of(trace) == ENCOUNTER


def refuse(trace: Trace, title: str) -> bool:
    print(title)
    if mine(trace):
        return False
    print(f"  not a {TITLE} pull")
    return True


def show_banner(trace: Trace) -> None:
    icc.show_banner(trace)
    if not icc.is_icc(trace):
        return
    if not mine(trace):
        print(f"  not a {TITLE} pull, every section below reads empty")
        return
    gone = [key for key, _, _ in silent_keys(emitted_keys(trace), ENCOUNTER)]
    print(f"probes absent from this trace: {', '.join(gone)}" if gone else "all bql.* probes present")


def show_phases(trace: Trace) -> None:
    if refuse(trace, "PHASES"):
        return
    steps = icc.phase_steps(trace)
    if not steps:
        print("  bql.phase never written")
        return
    print("  bql.phase, each step from the first bot to note it\n")
    print(f"    {'from':>9}  {'held':>9}  phase")
    for value, start, stop in steps:
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {value}")


def aura_windows(trace: Trace, spells) -> list[tuple[int, int, int]]:
    """`(player, start, stop)` per stretch the player held any of `spells`, stretches less than half a
    second apart merged, the last one running to the pull's end."""
    end = icc.pull_end(trace)
    held: dict[int, list[list[int]]] = {}
    open_at: dict[tuple[int, int], int] = {}
    for rec in trace.of("aura"):
        if rec.get("sp") not in spells:
            continue
        key = (rec["d"], rec["sp"])
        if not rec.get("r"):
            open_at.setdefault(key, rec["t"])
        elif key in open_at:
            held.setdefault(rec["d"], []).append([open_at.pop(key), rec["t"]])
    for (player, _), start in open_at.items():
        held.setdefault(player, []).append([start, end])

    windows = []
    for player, spans in held.items():
        merged: list[list[int]] = []
        for start, stop in sorted(spans):
            if merged and start - merged[-1][1] <= MERGE_GAP_MS:
                merged[-1][1] = max(merged[-1][1], stop)
            else:
                merged.append([start, stop])
        windows += [(player, start, stop) for start, stop in merged]
    return sorted(windows, key=lambda window: (window[1], window[0]))


def values_inside(trace: Trace, key: str, guid: int, start: int, stop: int, skip=("none",)) -> list[str]:
    """The notes `guid` wrote under `key` inside the window, a repeat of the one before folded away."""
    out: list[str] = []
    for rec in sorted(notes(trace, key), key=lambda rec: rec["t"]):
        value = str(rec.get("txt", ""))
        if rec["g"] != guid or not start <= rec["t"] <= stop or value in skip:
            continue
        if not out or out[-1] != value:
            out.append(value)
    return out


def applied(trace: Trace, guid: int, spells, start: int, stop: int) -> bool:
    return any(rec["d"] == guid and rec.get("sp") in spells and not rec.get("r") and start <= rec["t"] <= stop
               for rec in trace.of("aura"))


def bite_rows(trace: Trace) -> list[dict]:
    """Per Frenzied Bloodthirst: `biter`, `start`, `stop`, the `picks` bql.bite noted inside, and
    `outcome`: bit, frenzy, died, or open."""
    dead = first_deaths(trace)
    rows = []
    for biter, start, stop in aura_windows(trace, FRENZIED_BLOODTHIRST):
        if applied(trace, biter, ESSENCE_OF_THE_BLOOD_QUEEN, stop - OUTCOME_SLACK_MS, stop + OUTCOME_SLACK_MS):
            outcome = "bit"
        elif applied(trace, biter, (UNCONTROLLABLE_FRENZY,), stop - OUTCOME_SLACK_MS, stop + OUTCOME_SLACK_MS):
            outcome = "frenzy"
        elif start <= dead.get(biter, -1) <= stop + OUTCOME_SLACK_MS:
            outcome = "died"
        else:
            outcome = "open"
        rows.append({"biter": biter, "start": start, "stop": stop, "outcome": outcome,
                     "picks": values_inside(trace, "bql.bite", biter, start, stop)})
    return rows


def show_bites(trace: Trace) -> None:
    if refuse(trace, "BITES"):
        return
    rows = bite_rows(trace)
    if not rows:
        print("  no Frenzied Bloodthirst landed")
        return
    print(f"  {len(rows)} frenzy window(s): the biter, each pick in order, how it ended\n")
    for row in rows:
        picks = " -> ".join(resolve_guids(trace, pick) for pick in row["picks"]) or "-"
        human = " human" if row["biter"] in trace.humans else ""
        print(f"    {clock(row['start']):>9}  {clock(row['stop'] - row['start']):>9}  "
              f"{trace.name(row['biter'])}{human}  {picks}  {row['outcome']}")


def show_pact(trace: Trace) -> None:
    if refuse(trace, "PACT"):
        return
    windows = aura_windows(trace, (PACT_OF_THE_DARKFALLEN,))
    if not windows:
        print("  no Pact of the Darkfallen landed")
        return
    print(f"  {len(windows)} Pact member window(s): each member and the rules bql.pact noted for it\n")
    for member, start, stop in windows:
        noted = values_inside(trace, "bql.pact", member, start, stop)
        rules = " -> ".join(resolve_guids(trace, rule) for rule in noted)
        human = " human" if member in trace.humans else ""
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {trace.name(member)}{human}  {rules or '-'}")


def show_shadows(trace: Trace) -> None:
    if refuse(trace, "SHADOWS"):
        return
    windows = aura_windows(trace, SWARMING_SHADOWS)
    if not windows:
        print("  no Swarming Shadows landed")
        return
    dead = first_deaths(trace)
    print(f"  {len(windows)} Swarming Shadows: the victim and the lanes bql.curve noted inside\n")
    for victim, start, stop in windows:
        lanes = " -> ".join(values_inside(trace, "bql.curve", victim, start, stop, skip=())) or "-"
        died = "  died" if start <= dead.get(victim, -1) <= stop else ""
        human = " human" if victim in trace.humans else ""
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {trace.name(victim)}{human}  lane {lanes}{died}")


def slot_rows(trace: Trace) -> list[dict]:
    """Per bot: for `bql.slot` and `bql.airslot` the first value and how many times it changed."""
    end = icc.pull_end(trace)
    rows: dict[int, dict] = {}
    for key, label in (("bql.slot", "ground"), ("bql.airslot", "air")):
        for guid, spans in holder_spans(trace, key, end).items():
            rows.setdefault(guid, {"guid": guid})[label] = (spans[0][0], len(spans) - 1)
    return sorted(rows.values(), key=lambda row: trace.name(row["guid"]))


def show_slots(trace: Trace) -> None:
    if refuse(trace, "SLOTS"):
        return
    rows = slot_rows(trace)
    if not rows:
        print("  bql.slot and bql.airslot never written")
        return
    print("  per bot: first slot and how many times it moved, on the ground and in the air\n")
    for row in rows:
        parts = [f"{label} {row[label][0]} (moved {row[label][1]})" for label in ("ground", "air") if label in row]
        print(f"    {trace.name(row['guid'])}  {'  '.join(parts)}")


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
    ("phases", "bql.phase: ground, center, air", show_phases),
    ("bites", "each Frenzied Bloodthirst: picks and how it ended", show_bites),
    ("pact", "each Pact of the Darkfallen member: where it went", show_pact),
    ("shadows", "each Swarming Shadows: the lanes its victim walked", show_shadows),
    ("slots", "per bot: ground and air slots and how often they moved", show_slots),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
