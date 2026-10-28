#!/usr/bin/env python3
"""Read a Festergut pull from a RaidObs trace: stands, spores, goo, the tanks, defensives, cheats.

    festergut.py <file>               every section
    festergut.py <file> --phases      fg.phase, the spore cycle
    festergut.py <file> --stands      each bot's roster stand and how long it held each kind
    festergut.py <file> --spores      each spore window: carriers, where each was sent, who stacked where
    festergut.py <file> --goo         heroic Malleable Goo: impacts, each bot's dodges, the leg flips
    festergut.py <file> --targets     what the resolver had the bots on, by rule
    festergut.py <file> --tanks       who held him, the swap elections, the brake, peak Gastric Bloat
    festergut.py <file> --defensives  tank buttons, calls to a human tank, Pungent Blight cover and buttons
    festergut.py <file> --cheats      this encounter's cheat uses

What the generic views miss here:

- **His trace opens on the engage**, named by the encounter credit: his boss state moves only once
  Putricide reaches his balcony, about 7 s in, and nothing the bots do reads it.
- **`fg.slot` counts from 1.** 1-18 are the Vile Gas slots, 19 the melee legs, 20 and 21 the tank spots;
  `0` is a release. A melee bot on a slot is a fill: too few ranged stood out for Vile Gas (3 on 10-man,
  8 on 25-man).
- **`fg.stand` is the stand a bot holds right now**: `slot`, `leg`, `tank`, `close` or `far` for a spore
  stack, `none` while it holds nothing (the legs release once he stands 3.5 yd off his spot).
- **`fg.carrier` is the group a Gas Spore carrier was sent to**, 1 the tanks and melee, 2 the ranged and
  healers, dealt once a wave and again after a swap inside it.
- **`fg.leg` and `fg.sporespot` are raid-wide**: the melee leg (1 west, 2 east) and the ranged spore
  stack (1 behind him, 2 east), each flipped once goo lands on the one in use.
- **`fg.goo` is a dodge's branch**: `stand` back to a clear stand, `spread` to a spot 8.5 yd from everyone,
  `tight` to the nearest clear one, `none` when nothing cleared.
- **The swap is one elected taunt at 9 Gastric Bloat** (row 2198: 72219/72551/72552/72553). `fg.swap` goes
  `1` for the elected tank; the taunt is its `icc festergut bloat swap` act row. `fg.brake` holds a tank
  not holding him past 90% of the holder's threat, until under 80%.
- **`fg.defensive` is the tank ladder's pick**, a cast name, `idle` outside a window: the holder from 2
  Inhaled Blight once a cycle, from 3 every time one ends, any tank under 40%.
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
from raidobs.geometry import guids_of_entry, track  # noqa: E402
from raidobs.probes import emitted_keys, holder_spans, latch_spans, silent_keys  # noqa: E402
from raidobs.trace import Trace, clock, notes, roster_guids  # noqa: E402

ENCOUNTER = "festergut"
TITLE = "Festergut"
NPC_FESTERGUT = 36626
SPELL_GAS_SPORE = 69279
GASTRIC_BLOAT = (72219, 72551, 72552, 72553)
PUNGENT_BLIGHT = (69195, 71219, 73031, 73032)
# Putricide's own Malleable Goo and the balcony one he throws on heroic, both difficulty-free
GOO_SPELLS = (70852, 72296)
BLOAT_SWAP = 9
SLOT = "fg.slot"
STAND = "fg.stand"
CARRIER = "fg.carrier"
LEG = "fg.leg"
SPORESPOT = "fg.sporespot"
GOO = "fg.goo"
TARGET = "fg.target"
SWAP = "fg.swap"
BRAKE = "fg.brake"
DEFENSIVE = "fg.defensive"
HUMAN_CALL = "fg.humancall"
COVER = "icc.cover"
PERSONAL = "icc.personal"
SWAP_ACTION = "icc festergut bloat swap"
FIRST_LEG = 19
FIRST_TANK_SPOT = 20
GROUPS = {"1": "close", "2": "far"}
HEROIC = (2, 3)
HEROIC_ONLY = {GOO}
# a button counts for a window from this long before it opens until this long after it closes
WINDOW_LEAD_MS = 1500
WINDOW_TAIL_MS = 1500
# cast names, any rank
TANK_BUTTONS = frozenset({"Last Stand", "Shield Wall", "Divine Protection", "Barkskin", "Frenzied Regeneration",
                          "Survival Instincts", "Anti-Magic Shell", "Vampiric Blood", "Icebound Fortitude"})
NO_PICK = {"none", "idle", "covered", "channel", "free", ""}


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
    print(f"probes absent from this trace: {', '.join(gone)}" if gone else "all fg.* probes present")


def show_phases(trace: Trace) -> None:
    if refuse(trace, "PHASES"):
        return
    steps = icc.phase_steps(trace)
    if not steps:
        print("  fg.phase never written")
        return
    print("  fg.phase, each step from the first bot to note it\n")
    print(f"    {'from':>9}  {'held':>9}  phase")
    for value, start, stop in steps:
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {value}")


def stand_name(stand: int) -> str:
    if stand >= FIRST_TANK_SPOT:
        return f"tank {stand - FIRST_TANK_SPOT + 1}"
    return "leg" if stand == FIRST_LEG else f"slot {stand}"


def stand_rows(trace: Trace) -> dict[int, dict]:
    """Per bot: `roster`, every fg.slot it held in order, and `held`, ms per fg.stand kind."""
    end = icc.pull_end(trace)
    rows: dict[int, dict] = collections.defaultdict(lambda: {"roster": [], "held": collections.Counter()})
    for guid, spans in holder_spans(trace, SLOT, end).items():
        rows[guid]["roster"] = [int(value) for value, _, _ in spans if value.isdigit() and value != "0"]
    for guid, spans in holder_spans(trace, STAND, end).items():
        for value, start, stop in spans:
            rows[guid]["held"][value] += stop - start
    return dict(rows)


def show_stands(trace: Trace) -> None:
    if refuse(trace, "STANDS"):
        return
    rows = stand_rows(trace)
    if not rows:
        print(f"  {SLOT} and {STAND} never written")
        return
    for guid, row in sorted(rows.items(), key=lambda item: (item[1]["roster"][:1], icc.who(trace, item[0]))):
        roster = ", then ".join(stand_name(stand) for stand in row["roster"]) or "no roster stand"
        held = ", ".join(f"{kind} {clock(ms)}" for kind, ms in row["held"].most_common() if ms)
        print(f"  {trace.name(guid):<14} {roster:<16} {held}")


def spore_windows(trace: Trace) -> list[tuple[int, int]]:
    return [(start, stop) for value, start, stop in icc.phase_steps(trace) if value == "spore"]


def window_rows(trace: Trace) -> list[dict]:
    """Per spore window: `start`, `stop`, the `carriers` that gained a spore inside it with the group each
    was sent to, the far `spots` in use, first written first, and how many bots `stood` close or far."""
    end = icc.pull_end(trace)
    applied = [(rec["t"], rec["d"]) for rec in trace.of("aura")
               if rec.get("sp") == SPELL_GAS_SPORE and not rec.get("r")]
    sent = holder_spans(trace, CARRIER, end)
    stands = holder_spans(trace, STAND, end)
    spots = latch_spans(trace, SPORESPOT, end)
    rows = []
    for start, stop in spore_windows(trace):
        carriers = []
        for guid in sorted({guid for when, guid in applied if start - 1000 <= when < stop},
                           key=lambda guid: trace.name(guid)):
            groups = [GROUPS.get(value, value) for value, low, _ in sent.get(guid, [])
                      if start - 1000 <= low < stop and value != "0"]
            carriers.append((guid, groups[-1] if groups else "-"))
        held = [value for value, low, high in spots if low < stop and high > start]
        stood = collections.Counter(value for spans in stands.values() for value, low, high in spans
                                    if value in ("close", "far") and low < stop and high > start)
        rows.append({"start": start, "stop": stop, "carriers": carriers, "spots": held, "stood": dict(stood)})
    return rows


def show_spores(trace: Trace) -> None:
    if refuse(trace, "SPORES"):
        return
    rows = window_rows(trace)
    if not rows:
        print("  no spore window: fg.phase never read spore")
        return
    print(f"  {len(rows)} window(s); carriers gained Gas Spore from 1 s before the window opened\n")
    print(f"    {'from':>9}  {'held':>9}  {'far':<5}  {'close':>5}  {'far':>4}  carriers")
    for row in rows:
        spot = "->".join(row["spots"]) or "-"
        carriers = ", ".join(f"{trace.name(guid)} ({group})" for guid, group in row["carriers"]) or "-"
        print(f"    {clock(row['start']):>9}  {clock(row['stop'] - row['start']):>9}  {spot:<5}  "
              f"{row['stood'].get('close', 0):>5}  {row['stood'].get('far', 0):>4}  {carriers}")


def goo_rows(trace: Trace) -> dict[int, collections.Counter]:
    """Per bot that dodged: how many times it took each branch."""
    out: dict[int, collections.Counter] = {}
    for guid, spans in holder_spans(trace, GOO, icc.pull_end(trace)).items():
        out[guid] = collections.Counter(value for value, _, _ in spans)
    return out


def show_goo(trace: Trace) -> None:
    if refuse(trace, "GOO"):
        return
    if not heroic(trace):
        print("  normal mode: Putricide throws no goo at Festergut")
        return
    impacts = [rec for rec in trace.of("haz") if rec.get("sp") in GOO_SPELLS]
    print(f"  {len(impacts)} impact(s); the bots dodge 6.5 yd round each, to a spot 7.5 yd out")
    legs = [value for value, _, _ in latch_spans(trace, LEG, icc.pull_end(trace))]
    if len(legs) > 1:
        print(f"  melee leg {' -> '.join(legs)}")
    rows = goo_rows(trace)
    if not rows:
        print("  nobody dodged")
        return
    print(f"\n    {'bot':<14}  branches")
    for guid, branches in sorted(rows.items(), key=lambda item: (-sum(item[1].values()), trace.name(item[0]))):
        print(f"    {trace.name(guid):<14}  " + ", ".join(f"{value} {n}" for value, n in branches.most_common()))


def target_time(trace: Trace) -> dict[str, tuple[int, int]]:
    """Per rule: `(bots, ms held summed over bots)`."""
    bots: dict[str, set[int]] = collections.defaultdict(set)
    held: dict[str, int] = collections.defaultdict(int)
    for guid, spans in holder_spans(trace, TARGET, icc.pull_end(trace)).items():
        for value, start, stop in spans:
            bots[value].add(guid)
            held[value] += stop - start
    return {value: (len(bots[value]), held[value]) for value in held}


def show_targets(trace: Trace) -> None:
    if refuse(trace, "TARGETS"):
        return
    rows = target_time(trace)
    if not rows:
        print(f"  {TARGET} never written")
        return
    for value, (bots, held) in sorted(rows.items(), key=lambda row: -row[1][1]):
        print(f"  {value:<8} {bots:>3} bot(s)  {clock(held):>9} held in all")


def victim_runs(trace: Trace, guid: int) -> list[tuple[int, int, int]]:
    """`(victim, start, stop)` per run of a unit's target column, 0 for no target."""
    runs: list[list[int]] = []
    for when, target in track(trace, {guid}, ("t", "target")).get(guid, []):
        if runs and runs[-1][0] == target:
            runs[-1][2] = when
            continue
        if runs:
            runs[-1][2] = when
        runs.append([target, when, when])
    return [tuple(run) for run in runs]


def peak_stacks(trace: Trace, spells) -> dict[int, int]:
    peaks: dict[int, int] = collections.defaultdict(int)
    for rec in trace.of("aura"):
        if rec.get("sp") in spells and not rec.get("r"):
            peaks[rec["d"]] = max(peaks[rec["d"]], int(rec.get("st") or 1))
    return dict(peaks)


def elections(trace: Trace) -> list[tuple[int, int, int | None]]:
    """`(t, elected tank, t of its swap taunt or None)` per time fg.swap went 1."""
    end = icc.pull_end(trace)
    taunts = sorted((rec["t"], rec.get("g")) for rec in trace.of("act")
                    if rec.get("a") == SWAP_ACTION and rec.get("vd") == "OK")
    out = []
    for guid, spans in holder_spans(trace, SWAP, end).items():
        for value, start, stop in spans:
            if value != "1":
                continue
            taunt = next((when for when, who in taunts if who == guid and start <= when <= stop + 500), None)
            out.append((start, guid, taunt))
    return sorted(out)


def brake_time(trace: Trace) -> dict[int, tuple[int, int]]:
    """Per tank: `(times braked, ms held)`."""
    out: dict[int, tuple[int, int]] = {}
    for guid, spans in holder_spans(trace, BRAKE, icc.pull_end(trace)).items():
        held = [(start, stop) for value, start, stop in spans if value == "1"]
        if held:
            out[guid] = (len(held), sum(stop - start for start, stop in held))
    return out


def show_tanks(trace: Trace) -> None:
    if refuse(trace, "TANKS"):
        return
    bosses = sorted(guids_of_entry(trace, NPC_FESTERGUT))
    if not bosses:
        print("  Festergut never sampled")
        return
    runs = [run for run in victim_runs(trace, bosses[0]) if run[0]]
    tanks = [victim for victim, _, _ in runs if trace.role(victim) == "tank"]
    swaps = sum(1 for before, after in zip(tanks, tanks[1:]) if before != after)
    print(f"  who he hit, each run; {swaps} swap(s) between tanks, at {BLOAT_SWAP} Gastric Bloat by design\n")
    for victim, start, stop in runs:
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {trace.name(victim)} ({trace.role(victim)})")
    for when, guid, taunt in elections(trace):
        landed = f"taunted {(taunt - when) / 1000:.1f} s after" if taunt is not None else "no taunt"
        print(f"  {clock(when)}  {trace.name(guid)} elected, {landed}")
    for guid, (times, held) in sorted(brake_time(trace).items(), key=lambda item: trace.name(item[0])):
        print(f"  {trace.name(guid)} braked {times} time(s), {clock(held)} in all")
    peaks = peak_stacks(trace, GASTRIC_BLOAT)
    if peaks:
        print("\n  Gastric Bloat peak: " + ", ".join(
            f"{trace.name(guid)} {stacks}" for guid, stacks in sorted(peaks.items(), key=lambda item: -item[1])))


def casts_named(trace: Trace, names: frozenset) -> list[tuple[int, int, str]]:
    """`(t, guid, spell name)` for every roster cast of one of `names`, once per ms."""
    roster = roster_guids(trace)
    named = ((rec["t"], rec.get("s"), trace.spells.get(rec.get("sp"), "")) for rec in trace.of("cast")
             if rec.get("s") in roster)
    return sorted({cast for cast in named if cast[2] in names})


def tank_windows(trace: Trace) -> list[tuple[int, int, int, list[str]]]:
    """`(start, stop, tank, values)` per run of non-idle `fg.defensive` values of one tank."""
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


def pungent_casts(trace: Trace) -> list[int]:
    return sorted({rec["t"] for rec in trace.of("cast") if rec.get("sp") in PUNGENT_BLIGHT})


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
        print("  calls to a human tank  " + ", ".join(f"{rec['txt']} {clock(rec['t'])}" for rec in calls))
    for when in pungent_casts(trace):
        near = [rec for key in (COVER, PERSONAL) for rec in notes(trace, key)
                if when - 4000 <= rec["t"] <= when + 1000 and str(rec.get("txt", "")).lower() not in NO_PICK]
        picks = ", ".join(f"{rec['txt']} {trace.name(rec.get('g'))}" for rec in near) or "nothing spent"
        print(f"  Pungent Blight {clock(when)}: {picks}")


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
    ("phases", "fg.phase, the spore cycle", show_phases),
    ("stands", "each bot's roster stand and how long it held each kind", show_stands),
    ("spores", "each spore window: carriers, where each was sent, who stacked where", show_spores),
    ("goo", "heroic Malleable Goo: impacts, dodges, leg flips", show_goo),
    ("targets", "what the resolver had the bots on, by rule", show_targets),
    ("tanks", "who he hit, swap elections, the brake, peak Gastric Bloat", show_tanks),
    ("defensives", "tank buttons, calls to a human tank, Pungent Blight cover", show_defensives),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
