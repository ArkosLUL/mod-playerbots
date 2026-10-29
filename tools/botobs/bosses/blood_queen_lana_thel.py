#!/usr/bin/env python3
"""Read a Blood-Queen Lana'thel pull from a RaidObs trace: the air phases and every bot duty.

    blood_queen_lana_thel.py <file>               every section
    blood_queen_lana_thel.py <file> --phases      bql.phase: ground, center, air
    blood_queen_lana_thel.py <file> --targets     per bot: the resolver branches it was handed, its brake
    blood_queen_lana_thel.py <file> --tanks       per tank: its role, from when
    blood_queen_lana_thel.py <file> --bites       each Frenzied Bloodthirst: who the biter was handed, how it ended
    blood_queen_lana_thel.py <file> --cc          each mind-controlled vampire: the bots set to control it
    blood_queen_lana_thel.py <file> --pact        each Pact of the Darkfallen member: the spot it walked to
    blood_queen_lana_thel.py <file> --shadows     each Swarming Shadows carrier: the lanes it walked
    blood_queen_lana_thel.py <file> --spread      per bot: ground and air nodes, melee group, shadow dodges
    blood_queen_lana_thel.py <file> --defensives  tank buttons and calls to a human tank
    blood_queen_lana_thel.py <file> --cheats      this encounter's cheat uses

What the generic views miss here:

- **`bql.phase` follows her own air phase**: `center` from the moment she turns passive to walk to the
  middle of the room (Incite Terror there), `air` from takeoff, gravity off, until she lands, `ground`
  otherwise. Per bot, so a step starts at the first bot to note it.
- **`bql.target` is the resolver branch**: `boss`, `skull`, `brake` (held off her by the threat
  brake) or `bite` (frenzied: the bite is all it does). `bql.brake` is `1` while braked: past 90% of
  her holder's threat until under 80%, and every tank but the main one until the first vampire.
- **`bql.tank` is a role per tank**: `1` main (holds her on the tank spot), `2` mirror (stands on the
  main tank, so Blood Mirror takes it), `3` spare (fights in a melee group); `0` off the roster.
- **`bql.bite` is the player a frenzied bot was handed**, `0` once its Frenzy ends. Frenzied
  Bloodthirst is row 2083 (70877, 71474 on 25-man). A window ends `bit` on an Essence of the Blood
  Queen (row 2087) within 1 s, `frenzy` on Uncontrollable Frenzy 70923, the mind control for a bite
  never made.
- **`bql.cc` is the mind-controlled vampire a CC bot holds**, `0` once released.
- **`bql.pact` is a Pact member's spot**: `spot 1` 6 yd south of the centre, `2` and `3` 8 yd either
  side of it, `none` outside a Pact.
- **`bql.curve` is a carrier's lane**: `1` outer rising, `2` outer falling, `3` inner rising, `4`
  inner falling; `0` once its trail ends.
- **`bql.slot`** is a ground node 1-27 (healers, ranged), **`bql.airslot`** a node 1-42 from her walk to
  the centre until Bloodbolt Whirl ends, **`bql.group`** a melee group 1-3; `0` given up.
  **`bql.dodge`** is a bot stepping off a shadow: `melee`, `clear`, `blocked`, `none`.
- **`bql.defensive` is the tank ladder's pick**, a cast name, `idle` outside a window: any tank under
  40%, and the last minute before Berserk for her holder and the mirror tank. `bql.humancall` is a
  call to a human tank.
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
from raidobs.trace import Trace, clock, first_deaths, notes, roster_guids  # noqa: E402

ENCOUNTER = "blood-queen-lana-thel"
TITLE = "Blood-Queen Lana'thel"
FRENZIED_BLOODTHIRST = (70877, 71474)
ESSENCE_OF_THE_BLOOD_QUEEN = (70879, 71525, 71530, 71531)
UNCONTROLLABLE_FRENZY = 70923
PACT_OF_THE_DARKFALLEN = 71340
SWARMING_SHADOWS = (71264, 71265)
OUTCOME_SLACK_MS = 1000
MERGE_GAP_MS = 500
TARGET_BRANCHES = ("boss", "skull", "brake", "bite")
TANK_ROLES = {"1": "main", "2": "mirror", "3": "spare", "0": "off"}
LANES = {"1": "outer rising", "2": "outer falling", "3": "inner rising", "4": "inner falling"}
DEFENSIVE = "bql.defensive"
HUMAN_CALL = "bql.humancall"
# a button counts for a window from this long before it opens until this long after it closes
WINDOW_LEAD_MS = 1500
WINDOW_TAIL_MS = 1500
# cast names, any rank
TANK_BUTTONS = frozenset({"Last Stand", "Shield Wall", "Divine Protection", "Barkskin", "Frenzied Regeneration",
                          "Survival Instincts", "Anti-Magic Shell", "Vampiric Blood", "Icebound Fortitude"})


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


def value_counts(trace: Trace, key: str) -> dict[int, collections.Counter]:
    """Per guid: how many spans each value of `key` started."""
    rows: dict[int, collections.Counter] = {}
    for guid, spans in holder_spans(trace, key, icc.pull_end(trace)).items():
        rows[guid] = collections.Counter(value for value, _, _ in spans)
    return rows


def counted(counts: collections.Counter, order: tuple[str, ...] = ()) -> str:
    keys = [key for key in order if counts[key]] + sorted(key for key in counts if key not in order)
    return ", ".join(f"{key} {counts[key]}" for key in keys) or "-"


def show_targets(trace: Trace) -> None:
    if refuse(trace, "TARGETS"):
        return
    rows = value_counts(trace, "bql.target")
    brakes = value_counts(trace, "bql.brake")
    if not rows and not brakes:
        print("  bql.target and bql.brake never written")
        return
    print("  per bot: spans of each resolver branch, threat brake spans\n")
    for guid in sorted(set(rows) | set(brakes), key=trace.name):
        braked = brakes.get(guid, collections.Counter())["1"]
        print(f"    {trace.name(guid)}  {counted(rows.get(guid, collections.Counter()), TARGET_BRANCHES)}"
              f"  brake {braked}")


def show_tanks(trace: Trace) -> None:
    if refuse(trace, "TANKS"):
        return
    spans = holder_spans(trace, "bql.tank", icc.pull_end(trace))
    if not spans:
        print("  bql.tank never written")
        return
    print("  per tank: its role, from when\n")
    for guid in sorted(spans, key=trace.name):
        held = ", ".join(f"{TANK_ROLES.get(value, value)} {clock(start)}" for value, start, _ in spans[guid])
        print(f"    {trace.name(guid)}  {held}")


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


def values_inside(trace: Trace, key: str, guid: int, start: int, stop: int, skip=("none", "0")) -> list[str]:
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
    """Per Frenzied Bloodthirst: `biter`, `start`, `stop`, the `picks` bql.bite handed it inside, and
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


def picked(trace: Trace, pick: str) -> str:
    """A handed player by name and roster role."""
    guid = int(pick) if pick.isdigit() else None
    if guid is None or guid not in trace.names:
        return pick
    return f"{trace.name(guid)} ({trace.role(guid)})"


def show_bites(trace: Trace) -> None:
    if refuse(trace, "BITES"):
        return
    rows = bite_rows(trace)
    if not rows:
        print("  no Frenzied Bloodthirst landed")
        return
    print(f"  {len(rows)} frenzy window(s): the biter, each player it was handed, how it ended\n")
    for row in rows:
        picks = " -> ".join(picked(trace, pick) for pick in row["picks"]) or "-"
        human = " human" if row["biter"] in trace.humans else ""
        print(f"    {clock(row['start']):>9}  {clock(row['stop'] - row['start']):>9}  "
              f"{trace.name(row['biter'])}{human}  {picks}  {row['outcome']}")


def show_cc(trace: Trace) -> None:
    if refuse(trace, "CC"):
        return
    windows = aura_windows(trace, (UNCONTROLLABLE_FRENZY,))
    if not windows:
        print("  no Uncontrollable Frenzy landed")
        return
    holders = holder_spans(trace, "bql.cc", icc.pull_end(trace))
    print(f"  {len(windows)} mind-controlled vampire(s): the bots bql.cc set on each\n")
    for victim, start, stop in windows:
        set_on = [guid for guid, spans in holders.items()
                  if any(value == str(victim) and held_from <= stop and start <= held_to
                         for value, held_from, held_to in spans)]
        bots = ", ".join(trace.name(guid) for guid in sorted(set_on, key=trace.name)) or "nobody"
        human = " human" if victim in trace.humans else ""
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {trace.name(victim)}{human}  {bots}")


def show_pact(trace: Trace) -> None:
    if refuse(trace, "PACT"):
        return
    windows = aura_windows(trace, (PACT_OF_THE_DARKFALLEN,))
    if not windows:
        print("  no Pact of the Darkfallen landed")
        return
    print(f"  {len(windows)} Pact member window(s): each member and the spots bql.pact noted for it\n")
    for member, start, stop in windows:
        spots = " -> ".join(values_inside(trace, "bql.pact", member, start, stop))
        human = " human" if member in trace.humans else ""
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {trace.name(member)}{human}  {spots or '-'}")


def show_shadows(trace: Trace) -> None:
    if refuse(trace, "SHADOWS"):
        return
    windows = aura_windows(trace, SWARMING_SHADOWS)
    if not windows:
        print("  no Swarming Shadows landed")
        return
    dead = first_deaths(trace)
    print(f"  {len(windows)} Swarming Shadows: the carrier and the lanes bql.curve noted inside\n")
    for carrier, start, stop in windows:
        lanes = " -> ".join(LANES.get(code, code) for code in values_inside(trace, "bql.curve", carrier, start, stop))
        died = "  died" if start <= dead.get(carrier, -1) <= stop else ""
        human = " human" if carrier in trace.humans else ""
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {trace.name(carrier)}{human}  {lanes or '-'}{died}")


def held_values(spans: list[tuple[str, int, int]]) -> tuple[list[str], int]:
    """The distinct values held in order, `0` left out, and how often one went straight to another."""
    held: list[str] = []
    moves = 0
    last = None
    for value, _, _ in spans:
        if value == "0":
            last = None
            continue
        if value not in held:
            held.append(value)
        moves += last is not None and last != value
        last = value
    return held, moves


def spread_rows(trace: Trace) -> list[dict]:
    """Per bot: ground nodes held and node-to-node moves, air nodes held, groups held, and shadow dodges
    by value."""
    end = icc.pull_end(trace)
    rows: dict[int, dict] = {}
    for key, label in (("bql.slot", "ground"), ("bql.airslot", "air"), ("bql.group", "group")):
        for guid, spans in holder_spans(trace, key, end).items():
            rows.setdefault(guid, {"guid": guid})[label] = held_values(spans)
    for guid, counts in value_counts(trace, "bql.dodge").items():
        dodges = collections.Counter({value: n for value, n in counts.items() if value != "none"})
        rows.setdefault(guid, {"guid": guid})["dodges"] = dodges
    return sorted(rows.values(), key=lambda row: trace.name(row["guid"]))


def show_spread(trace: Trace) -> None:
    if refuse(trace, "SPREAD"):
        return
    rows = spread_rows(trace)
    if not rows:
        print("  bql.slot, bql.airslot, bql.group and bql.dodge never written")
        return
    print("  per bot: ground nodes (moves), air nodes, melee group, shadow dodges\n")
    for row in rows:
        parts = []
        if "ground" in row:
            nodes, moves = row["ground"]
            parts.append(f"ground {'/'.join(nodes) or '-'} (moved {moves})")
        if "air" in row:
            parts.append(f"air {'/'.join(row['air'][0]) or '-'}")
        if "group" in row:
            parts.append(f"group {'/'.join(row['group'][0]) or '-'}")
        parts.append(f"dodges {counted(row.get('dodges', collections.Counter()))}")
        print(f"    {trace.name(row['guid'])}  {'  '.join(parts)}")


def casts_named(trace: Trace, names: frozenset) -> list[tuple[int, int, str]]:
    """`(t, guid, spell name)` for every roster cast of one of `names`, once per ms."""
    roster = roster_guids(trace)
    named = ((rec["t"], rec.get("s"), trace.spells.get(rec.get("sp"), "")) for rec in trace.of("cast")
             if rec.get("s") in roster)
    return sorted({cast for cast in named if cast[2] in names})


def tank_windows(trace: Trace) -> list[tuple[int, int, int, list[str]]]:
    """`(start, stop, tank, values)` per run of non-idle `bql.defensive` values of one tank."""
    end = icc.pull_end(trace)
    out = []
    for guid, spans in holder_spans(trace, DEFENSIVE, end).items():
        run: list[tuple[str, int, int]] = []
        for span in spans + [("idle", end, end)]:
            if span[0] != "idle":
                run.append(span)
            elif run:
                out.append((run[0][1], run[-1][2], guid, [value for value, _, _ in run]))
                run = []
    return sorted(out)


def show_defensives(trace: Trace) -> None:
    if refuse(trace, "DEFENSIVES"):
        return
    windows = tank_windows(trace)
    if not windows:
        print(f"  {DEFENSIVE} never offered a button")
    casts = casts_named(trace, TANK_BUTTONS)
    for start, stop, tank, values in windows:
        low, high = start - WINDOW_LEAD_MS, stop + WINDOW_TAIL_MS
        spent = [cast for cast in casts if cast[1] == tank and low <= cast[0] <= high]
        offered = ", ".join(dict.fromkeys(values))
        cast = ", ".join(f"{name} {(when - start) / 1000:+.1f}s" for when, _, name in spent) or "none"
        print(f"  {clock(start)}  {trace.name(tank):<10} {clock(stop - start):>9}  offered {offered}; cast {cast}")
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
    ("targets", "per bot: resolver branches and threat brake", show_targets),
    ("tanks", "per tank: its role, from when", show_tanks),
    ("bites", "each Frenzied Bloodthirst: the player handed and how it ended", show_bites),
    ("cc", "each mind-controlled vampire: the bots set to control it", show_cc),
    ("pact", "each Pact of the Darkfallen member: the spot it walked to", show_pact),
    ("shadows", "each Swarming Shadows carrier: the lanes it walked", show_shadows),
    ("spread", "per bot: ground and air nodes, melee group, shadow dodges", show_spread),
    ("defensives", "tank buttons and calls to a human tank", show_defensives),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
