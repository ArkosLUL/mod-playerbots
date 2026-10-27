#!/usr/bin/env python3
"""Read a Festergut pull from a RaidObs trace: the spore cycle, goo holds, the tanks, cheats.

    festergut.py <file>            every section
    festergut.py <file> --phases   fg.phase, the spore cycle
    festergut.py <file> --spores   each spore window: who carried a spore, the ranged spot it used
    festergut.py <file> --goo      heroic Malleable Goo: impacts, who dodged and how long each held
    festergut.py <file> --tanks    who held him, each tank's peak Gastric Bloat, the swaps
    festergut.py <file> --cheats   this encounter's cheat uses

What the generic views miss here:

- **`fg.phase` is per bot**: every bot that asks writes `spore` or `normal`, so a step starts at the
  first bot to note it. A spore window is Gas Spore (69279, every difficulty) on any group member.
- **`fg.sporespot` is the raid-wide ranged spot**, 1 or 2, the second being where the ranged move when
  goo lands within 12 yd of the first. Each ranged bot writes it while it reads the spot, but only on a
  change, so a window's spots are the values written inside it, else the one it opened on.
- **`fg.goohold` is per bot**: `dodge` inside an impact's 12 yd, `hold` while it waits the impact's
  8 s out, `none` once that ends. A spore window hands the bot to the spore node and stops the writes,
  so `hold` can outlast its impact there.
- **Gastric Bloat is row 2198** (72219/72551/72552/72553) and the tanks swap at 6 stacks.
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
from raidobs.trace import Trace, clock  # noqa: E402

ENCOUNTER = "festergut"
TITLE = "Festergut"
NPC_FESTERGUT = 36626
SPELL_GAS_SPORE = 69279
GASTRIC_BLOAT = (72219, 72551, 72552, 72553)
# Putricide's own Malleable Goo and the balcony one he throws on heroic, both difficulty-free
GOO_SPELLS = (70852, 72296)
BLOAT_SWAP = 6
HEROIC = (2, 3)
HEROIC_ONLY = {"fg.goohold"}


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


def spore_windows(trace: Trace) -> list[tuple[int, int]]:
    return [(start, stop) for value, start, stop in icc.phase_steps(trace) if value == "spore"]


def window_rows(trace: Trace) -> list[dict]:
    """Per spore window: `start`, `stop`, the `carriers` that gained a spore inside it, and the `spots`
    written inside it, first written first, else the one it opened on."""
    applied = [(rec["t"], rec["d"]) for rec in trace.of("aura")
               if rec.get("sp") == SPELL_GAS_SPORE and not rec.get("r")]
    spots = latch_spans(trace, "fg.sporespot")
    rows = []
    for start, stop in spore_windows(trace):
        carriers = sorted({guid for when, guid in applied if start - 1000 <= when < stop},
                          key=lambda guid: trace.name(guid))
        held = [value for value, low, _ in spots if start <= low < stop]
        held = held or [value for value, low, high in spots if low < start < high]
        rows.append({"start": start, "stop": stop, "carriers": carriers, "spots": held})
    return rows


def show_spores(trace: Trace) -> None:
    if refuse(trace, "SPORES"):
        return
    rows = window_rows(trace)
    if not rows:
        print("  no spore window: fg.phase never read spore")
        return
    print(f"  {len(rows)} window(s); carriers gained Gas Spore from 1 s before the window opened\n")
    print(f"    {'from':>9}  {'held':>9}  {'spot':<6}  carriers")
    for row in rows:
        spot = "->".join(row["spots"]) or "-"
        names = ", ".join(trace.name(guid) for guid in row["carriers"]) or "-"
        print(f"    {clock(row['start']):>9}  {clock(row['stop'] - row['start']):>9}  {spot:<6}  {names}")


def goo_rows(trace: Trace) -> dict[int, dict]:
    """Per bot that held for goo: `dodges`, the `dodge` entries, and `held` ms spent in `hold`."""
    end = icc.pull_end(trace)
    out: dict[int, dict] = {}
    for guid, spans in holder_spans(trace, "fg.goohold", end).items():
        dodges = sum(1 for value, _, _ in spans if value == "dodge")
        held = sum(stop - start for value, start, stop in spans if value == "hold")
        if dodges or held:
            out[guid] = {"dodges": dodges, "held": held}
    return out


def show_goo(trace: Trace) -> None:
    if refuse(trace, "GOO"):
        return
    impacts = [rec for rec in trace.of("haz") if rec.get("sp") in GOO_SPELLS]
    if not heroic(trace):
        print("  normal mode: Putricide throws no goo at Festergut")
        return
    print(f"  {len(impacts)} impact(s), each a 12 yd circle for 8 s")
    rows = goo_rows(trace)
    if not rows:
        print("  nobody held for goo")
        return
    print(f"\n    {'bot':<14}  {'dodges':>6}  {'held':>9}")
    for guid, row in sorted(rows.items(), key=lambda item: (-item[1]["dodges"], trace.name(item[0]))):
        print(f"    {trace.name(guid):<14}  {row['dodges']:>6}  {clock(row['held']):>9}")


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
    peaks = peak_stacks(trace, GASTRIC_BLOAT)
    if peaks:
        print("\n  Gastric Bloat peak: " + ", ".join(
            f"{trace.name(guid)} {stacks}" for guid, stacks in sorted(peaks.items(), key=lambda item: -item[1])))


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
    ("spores", "each spore window: carriers and the ranged spot", show_spores),
    ("goo", "heroic Malleable Goo: impacts, dodges, holds", show_goo),
    ("tanks", "who he hit, peak Gastric Bloat, swaps", show_tanks),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
