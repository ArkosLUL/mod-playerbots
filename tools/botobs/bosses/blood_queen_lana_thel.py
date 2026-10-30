#!/usr/bin/env python3
"""Read a Blood-Queen Lana'thel pull from a RaidObs trace: the air phases and every bot duty.

    blood_queen_lana_thel.py <file>               every section
    blood_queen_lana_thel.py <file> --phases      bql.phase: ground, center, air
    blood_queen_lana_thel.py <file> --targets     per bot: the resolver branches it was handed, its brake
    blood_queen_lana_thel.py <file> --tanks       per tank: its role, from when
    blood_queen_lana_thel.py <file> --bites       each Frenzied Bloodthirst: who the biter was handed, how it ended
    blood_queen_lana_thel.py <file> --cc          each mind-controlled vampire: the bots set to control it, a kill
    blood_queen_lana_thel.py <file> --fear        each Incite Terror: how many it feared, the Mass Dispels on them
    blood_queen_lana_thel.py <file> --pact        each Pact of the Darkfallen member: the spot it walked to
    blood_queen_lana_thel.py <file> --shadows     each Swarming Shadows carrier: the lanes it walked
    blood_queen_lana_thel.py <file> --spread      per bot: ground and air nodes, melee group, shadow dodges, detours
    blood_queen_lana_thel.py <file> --defensives  tank buttons and calls to a human tank
    blood_queen_lana_thel.py <file> --cheats      this encounter's cheat uses

What the generic views miss here:

- **`bql.phase` follows her own air phase**: `center` from the moment she turns passive to walk to the
  middle of the room (Incite Terror there), `air` from takeoff, gravity off, until she lands, `ground`
  otherwise. Per bot, so a step starts at the first bot to note it.
- **`bql.target` is the resolver branch**: `boss`, `skull`, `brake` (held off her by the threat
  brake), `bite` (frenzied: the bite is all it does) or `kill` (a mind-controlled vampire no bot can
  control). `bql.brake` is `1` while braked: past 90% of her holder's threat until under 80%, and every
  tank but the main one, a damage-dealer mirror too, until the first vampire.
- **`bql.tank` is a role per tank**: `1` main (holds her on the tank spot), `2` mirror (stands on the
  main tank, so Blood Mirror takes it; a damage dealer when the raid has one tank), `3` spare (fights in
  a melee group); `0` off the roster.
- **`bql.bite` is the player a frenzied bot was handed**, `0` once its Frenzy ends. Frenzied
  Bloodthirst is row 2083 (70877, 71474 on 25-man). A window ends `bit` on an Essence of the Blood
  Queen (row 2087) within 1 s, `frenzy` on Uncontrollable Frenzy 70923, the mind control for a bite
  never made.
- **`bql.cc` is the mind-controlled vampire a CC bot holds**, `0` once released. **`bql.kill`** is `1`
  on a vampire no bot could control for 2 s, which the damage dealers then kill.
- **`bql.massdispel` is the feared player a priest centres Mass Dispel on**, `0` once none is worth
  it. Incite Terror is 73070, Mass Dispel 32375.
- **`bql.pact` is a Pact member's spot**: `spot 1` 6 yd south of the centre, `2` and `3` 8 yd either
  side of it, `human` once the bots walk to a human member who didn't come, `none` outside a Pact.
- **`bql.curve` is a carrier's lane**: `1` outer rising, `2` outer falling, `3` inner rising, `4`
  inner falling; `0` once its trail ends.
- **`bql.slot`** is a ground node 1-30 (healers, ranged), **`bql.airslot`** a node 1-45 from her walk to
  the centre until Bloodbolt Whirl ends, **`bql.group`** a melee group 1-3; `0` given up.
  **`bql.dodge`** is a bot stepping off a shadow: `melee`, `clear`, `blocked`, `none`. **`bql.detour`**
  is a bite or Pact walk round shadows: `via` a point clear of them, `blocked` straight through, `none`.
- **`bql.defensive` is the tank ladder's pick**, a cast name, `idle` outside a window: any tank or
  damage-dealer mirror under 40%, and the last minute before Berserk for her holder and the mirror.
  `bql.humancall` is a call to a human tank.
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
INCITE_TERROR = 73070
MASS_DISPEL = 32375
# her fear lasts 4 s; one removed this long before that was dispelled
FEAR_MS = 4000
FEAR_EARLY_MS = 500
# fear auras starting this close together are one Incite Terror
FEAR_WAVE_MS = 1000
OUTCOME_SLACK_MS = 1000
MERGE_GAP_MS = 500
TARGET_BRANCHES = ("boss", "skull", "brake", "bite", "kill")
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
    end = icc.pull_end(trace)
    holders = holder_spans(trace, "bql.cc", end)
    kills = holder_spans(trace, "bql.kill", end)
    print(f"  {len(windows)} mind-controlled vampire(s): the bots bql.cc set on each, when bql.kill marked it\n")
    for victim, start, stop in windows:
        set_on = [guid for guid, spans in holders.items()
                  if any(value == str(victim) and held_from <= stop and start <= held_to
                         for value, held_from, held_to in spans)]
        bots = ", ".join(trace.name(guid) for guid in sorted(set_on, key=trace.name)) or "nobody"
        human = " human" if victim in trace.humans else ""
        marked = [held_from for value, held_from, _ in kills.get(victim, [])
                  if value == "1" and start <= held_from <= stop]
        killed = f"  kill {clock(marked[0])}" if marked else ""
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {trace.name(victim)}{human}  {bots}{killed}")


def fear_waves(trace: Trace) -> list[dict]:
    """Per Incite Terror: `start`, the `feared` players, how many lost the fear `early`, and per priest the
    centres bql.massdispel handed it and the times it cast Mass Dispel."""
    windows = aura_windows(trace, (INCITE_TERROR,))
    waves: list[dict] = []
    for player, start, stop in sorted(windows, key=lambda window: window[1]):
        if not waves or start - waves[-1]["start"] > FEAR_WAVE_MS:
            waves.append({"start": start, "stop": stop, "feared": [], "early": 0})
        wave = waves[-1]
        wave["feared"].append(player)
        wave["stop"] = max(wave["stop"], stop)
        wave["early"] += stop - start < FEAR_MS - FEAR_EARLY_MS

    casts = [(rec["t"], rec.get("s")) for rec in trace.of("cast") if rec.get("sp") == MASS_DISPEL]
    for wave in waves:
        priests: dict[int, dict] = {}
        for rec in notes(trace, "bql.massdispel"):
            if wave["start"] <= rec["t"] <= wave["stop"] and str(rec.get("txt", "0")) != "0":
                priests.setdefault(rec["g"], {"centres": [], "casts": []})
        for guid, row in priests.items():
            row["centres"] = values_inside(trace, "bql.massdispel", guid, wave["start"], wave["stop"])
        for when, guid in casts:
            if wave["start"] <= when <= wave["stop"]:
                priests.setdefault(guid, {"centres": [], "casts": []})["casts"].append(when)
        wave["priests"] = priests
    return waves


def show_fear(trace: Trace) -> None:
    if refuse(trace, "FEAR"):
        return
    waves = fear_waves(trace)
    if not waves:
        print("  no Incite Terror landed")
        return
    print(f"  {len(waves)} Incite Terror(s): players feared and freed early, each priest's Mass Dispel centres"
          " and casts\n")
    for wave in waves:
        print(f"    {clock(wave['start']):>9}  feared {len(wave['feared'])}, freed early {wave['early']}")
        for guid in sorted(wave["priests"], key=trace.name):
            row = wave["priests"][guid]
            centres = " -> ".join(picked(trace, pick) for pick in row["centres"]) or "-"
            casts = ", ".join(f"{(when - wave['start']) / 1000:+.1f}s" for when in row["casts"]) or "none"
            print(f"      {trace.name(guid)}  on {centres}  cast {casts}")


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
    """Per bot: ground nodes held and node-to-node moves, air nodes held, groups held, shadow dodges by
    value, and bite or Pact walks round shadows by value."""
    end = icc.pull_end(trace)
    rows: dict[int, dict] = {}
    for key, label in (("bql.slot", "ground"), ("bql.airslot", "air"), ("bql.group", "group")):
        for guid, spans in holder_spans(trace, key, end).items():
            rows.setdefault(guid, {"guid": guid})[label] = held_values(spans)
    for key, label in (("bql.dodge", "dodges"), ("bql.detour", "detours")):
        for guid, counts in value_counts(trace, key).items():
            moves = collections.Counter({value: n for value, n in counts.items() if value != "none"})
            rows.setdefault(guid, {"guid": guid})[label] = moves
    return sorted(rows.values(), key=lambda row: trace.name(row["guid"]))


def show_spread(trace: Trace) -> None:
    if refuse(trace, "SPREAD"):
        return
    rows = spread_rows(trace)
    if not rows:
        print("  bql.slot, bql.airslot, bql.group, bql.dodge and bql.detour never written")
        return
    print("  per bot: ground nodes (moves), air nodes, melee group, shadow dodges, walks round shadows\n")
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
        if "detours" in row:
            parts.append(f"detours {counted(row['detours'])}")
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
    ("cc", "each mind-controlled vampire: the bots set to control it, a kill", show_cc),
    ("fear", "each Incite Terror: how many it feared, the Mass Dispels on them", show_fear),
    ("pact", "each Pact of the Darkfallen member: the spot it walked to", show_pact),
    ("shadows", "each Swarming Shadows carrier: the lanes it walked", show_shadows),
    ("spread", "per bot: ground and air nodes, melee group, shadow dodges, detours", show_spread),
    ("defensives", "tank buttons and calls to a human tank", show_defensives),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
