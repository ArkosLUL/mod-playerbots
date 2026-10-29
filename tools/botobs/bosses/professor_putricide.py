#!/usr/bin/env python3
"""Read a Professor Putricide pull from a RaidObs trace: the adds, the abomination, the plagues, the dodges.

    professor_putricide.py <file>            every section
    professor_putricide.py <file> --phases   pp.phase, 1 to 3
    professor_putricide.py <file> --adds     each Volatile Ooze victim and its stack, each Gas Cloud bloat
    professor_putricide.py <file> --abom     the driver and its duty, every Regurgitated Ooze
    professor_putricide.py <file> --plague   Mutated Plague stacks and swaps, heroic Unbound Plague passes
    professor_putricide.py <file> --dodges   each dodge by hazard and how it went
    professor_putricide.py <file> --spread   the boss spot and the ring slots
    professor_putricide.py <file> --cheats   this encounter's cheat uses

What the generic views miss here:

- **`pp.phase` comes from his health**, the thresholds his own DamageTaken hook tests: 1 above 80%,
  2 above 35%, 3 at 35% and under. It is per bot, so a step starts at the first bot to note it.
- **`pp.target` is per bot**: `skull`, `ooze`, `cloud`, `boss`, `brake` (held off him, nothing else to
  hit), `variable ooze`/`variable cloud`/`variable boss` on a 25H intermission.
- **`pp.stack` is per bot, the rooted Volatile Ooze victim it stands on**, `none` otherwise. Melee stay on
  the ooze and never note one.
- **`pp.kite` is per bloated bot**, the branch of each leg: `away` (straight from the cloud), `wall`
  (clamped to the kite disk's edge), `cross` (through a hazard), `none`, `locked` (the walk was refused).
- **`pp.driver` is the abomination's driver, `pp.abom` its duty**: `drink`, `spit <unit>` (Regurgitated
  Ooze), `eat <puddle>`, `slash <unit>`, `none`.
- **`pp.swap` is per tank or sharer**: `take` before the next Mutated Plague, `share` (a damage dealer
  taking one), `back` (a tank taking him back from it), `pickup` off anyone else, `none`. `pp.brake`
  holds the bots past 90% of the holder's threat.
- **`pp.plague` is per Unbound Plague carrier**: `hold`, `clear` (stepping off everyone), `pass` (walking
  onto its receiver), `none`. `pp.receiver` is keyed on the carrier, its value the receiver.
- **`pp.dodge` is `<hazard> <branch> <yards>`**: hazard `puddle`, `gas`, `blast`, `goo`, `cloud` (melee off
  a cloud about to re-pick) or `carrier`; branch `move`, `tight` or `cross`; `hold`, `<hazard> none`,
  `<hazard> locked`, `pinned`.
- **`pp.spot` is the boss spot**: 1 the room's centre, 2-9 the ring at 8 yd, 10-17 the ring at 16.
  **`pp.slot` counts from 1**: 1-14 the 12 yd ring, 15-38 the 22 yd one; `0` is a release.
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
from raidobs.probes import emitted_keys, holder_spans, latch_spans, resolve_guids, silent_keys  # noqa: E402
from raidobs.trace import Trace, clock, first_deaths, notes  # noqa: E402

ENCOUNTER = "professor-putricide"
TITLE = "Professor Putricide"
HEROIC = (2, 3)
HEROIC_ONLY = {"pp.receiver"}
ADHESIVE = (70447, 72836, 72837, 72838)
GASEOUS_BLOAT = (70672, 72455, 72832, 72833)
MUTATED_PLAGUE = (72451, 72463, 72671, 72672)
UNBOUND_PLAGUE = (70911, 72854, 72855, 72856)
REGURGITATED_OOZE = (70539, 72457, 72875, 72876)
KITE_BRANCHES = ("away", "wall", "cross", "none", "locked")
SWAP_KINDS = ("take", "share", "back", "pickup")
PLAGUE_BRANCHES = ("hold", "clear", "pass")
INNER_SLOTS = 14


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
    print(f"probes absent from this trace: {', '.join(gone)}" if gone else "all pp.* probes present")


def show_phases(trace: Trace) -> None:
    if refuse(trace, "PHASES"):
        return
    steps = icc.phase_steps(trace)
    if not steps:
        print("  pp.phase never written")
        return
    print("  pp.phase, each step from the first bot to note it\n")
    print(f"    {'from':>9}  {'held':>9}  phase")
    for value, start, stop in steps:
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {value}")


def aura_windows(trace: Trace, spells: tuple[int, ...]) -> list[dict]:
    """Per application of one of `spells`: `victim`, `source`, `start`, `stop` (the aura's removal, else the
    pull's end), the most `stacks` seen, and whether the victim `died` inside it."""
    end = icc.pull_end(trace)
    open_at: dict[int, dict] = {}
    rows = []
    for rec in trace.of("aura"):
        if rec.get("sp") not in spells:
            continue
        victim = rec["d"]
        if rec.get("r"):
            if victim in open_at:
                row = open_at.pop(victim)
                row["stop"] = rec["t"]
                rows.append(row)
            continue
        row = open_at.setdefault(victim, {"victim": victim, "source": rec.get("s"), "start": rec["t"], "stacks": 0})
        row["stacks"] = max(row["stacks"], rec.get("st") or 1)
    for row in open_at.values():
        row["stop"] = end
        rows.append(row)
    dead = first_deaths(trace)
    for row in rows:
        row["died"] = row["start"] <= dead.get(row["victim"], -1) <= row["stop"]
    return sorted(rows, key=lambda row: row["start"])


def stackers(trace: Trace, victim: int, start: int, stop: int) -> set[int]:
    """The bots that noted standing on `victim` at some point of the window."""
    return {rec["g"] for rec in notes(trace, "pp.stack")
            if start <= rec["t"] <= stop and str(rec.get("txt", "")) == str(victim)}


def kite_counts(trace: Trace, victim: int, start: int, stop: int) -> collections.Counter:
    return collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "pp.kite")
                               if rec["g"] == victim and start <= rec["t"] <= stop)


def victim_label(trace: Trace, row: dict) -> str:
    human = " human" if row["victim"] in trace.humans else ""
    died = "  died" if row["died"] else ""
    return f"{trace.name(row['victim'])}{human}{died}"


def show_adds(trace: Trace) -> None:
    if refuse(trace, "ADDS"):
        return
    oozes = aura_windows(trace, ADHESIVE)
    print(f"  {len(oozes)} Volatile Ooze victim(s); stack counts the bots that noted standing on each")
    for row in oozes:
        crowd = stackers(trace, row["victim"], row["start"], row["stop"])
        print(f"    {clock(row['start']):>9}  {clock(row['stop'] - row['start']):>9}  "
              f"{victim_label(trace, row)}  stack {len(crowd)}")
    clouds = aura_windows(trace, GASEOUS_BLOAT)
    print(f"\n  {len(clouds)} Gaseous Bloat(s); legs counts the pp.kite branch switches inside each")
    for row in clouds:
        legs = kite_counts(trace, row["victim"], row["start"], row["stop"])
        shown = ", ".join(f"{branch} {legs[branch]}" for branch in KITE_BRANCHES if legs[branch]) or "-"
        print(f"    {clock(row['start']):>9}  {clock(row['stop'] - row['start']):>9}  "
              f"{victim_label(trace, row)}  {shown}")


def duty_counts(trace: Trace) -> collections.Counter:
    """How often the driver switched into each duty, the unit dropped."""
    counts: collections.Counter = collections.Counter()
    for spans in holder_spans(trace, "pp.abom", icc.pull_end(trace)).values():
        for value, _, _ in spans:
            counts[value.split()[0] if value else "none"] += 1
    return counts


def show_abom(trace: Trace) -> None:
    if refuse(trace, "ABOMINATION"):
        return
    end = icc.pull_end(trace)
    drivers = [f"{'none' if value in ('', '0') else resolve_guids(trace, value)}@{clock(start)}"
               for value, start, _ in latch_spans(trace, "pp.driver", end)]
    print("  driver: " + (", ".join(drivers) if drivers else "never written"))
    counts = duty_counts(trace)
    if counts:
        print("  duty switches: " + ", ".join(f"{duty} {n}" for duty, n in sorted(counts.items())))
    spits = aura_windows(trace, REGURGITATED_OOZE)
    if not spits:
        print("  no Regurgitated Ooze landed")
        return
    print(f"  {len(spits)} Regurgitated Ooze")
    for row in spits:
        print(f"    {clock(row['start']):>9}  {trace.name(row['victim'])}")


def plague_peaks(trace: Trace) -> dict[int, int]:
    """The most Mutated Plague stacks each carrier held."""
    peaks: dict[int, int] = {}
    for row in aura_windows(trace, MUTATED_PLAGUE):
        peaks[row["victim"]] = max(peaks.get(row["victim"], 0), row["stacks"])
    return peaks


def swap_rows(trace: Trace) -> list[tuple[int, int, str]]:
    """`(when, bot, kind)` each time a bot was elected to taunt him."""
    rows = []
    for guid, spans in holder_spans(trace, "pp.swap", icc.pull_end(trace)).items():
        rows += [(start, guid, value) for value, start, _ in spans if value in SWAP_KINDS]
    return sorted(rows)


def show_plague(trace: Trace) -> None:
    if refuse(trace, "PLAGUE"):
        return
    peaks = plague_peaks(trace)
    if peaks:
        print("  Mutated Plague peak stacks: " + ", ".join(
            f"{trace.name(guid)} {n}" for guid, n in sorted(peaks.items(), key=lambda item: (-item[1], trace.name(item[0])))))
    else:
        print("  no Mutated Plague landed")
    swaps = swap_rows(trace)
    if swaps:
        print(f"  {len(swaps)} swap election(s)")
        for when, guid, kind in swaps:
            print(f"    {clock(when):>9}  {trace.name(guid):<14}  {kind}")
    braked = {rec["g"] for rec in notes(trace, "pp.brake") if str(rec.get("txt", "")) not in ("", "0")}
    if braked:
        print("  braked: " + ", ".join(sorted(trace.name(guid) for guid in braked)))
    if not heroic(trace):
        print("  normal mode: no Unbound Plague")
        return
    carriers = aura_windows(trace, UNBOUND_PLAGUE)
    print(f"\n  {len(carriers)} Unbound Plague carrier(s)")
    branches = holder_spans(trace, "pp.plague", icc.pull_end(trace))
    for row in carriers:
        seen = collections.Counter(value for value, start, _ in branches.get(row["victim"], [])
                                   if row["start"] <= start <= row["stop"] and value in PLAGUE_BRANCHES)
        shown = ", ".join(f"{branch} {seen[branch]}" for branch in PLAGUE_BRANCHES if seen[branch]) or "-"
        print(f"    {clock(row['start']):>9}  {clock(row['stop'] - row['start']):>9}  "
              f"{victim_label(trace, row)}  {shown}")


def dodge_counts(trace: Trace) -> collections.Counter:
    """Dodge outcomes by `<hazard> <branch>`, yards dropped; `hold` rows are a walk already in flight."""
    counts: collections.Counter = collections.Counter()
    for rec in notes(trace, "pp.dodge"):
        words = str(rec.get("txt", "")).split()
        if words and words[0] != "hold":
            counts[" ".join(words[:2])] += 1
    return counts


def show_dodges(trace: Trace) -> None:
    if refuse(trace, "DODGES"):
        return
    counts = dodge_counts(trace)
    if not counts:
        print("  pp.dodge never written")
        return
    for outcome, n in sorted(counts.items(), key=lambda item: (-item[1], item[0])):
        print(f"    {n:>4}  {outcome}")
    per_bot = collections.Counter(rec["g"] for rec in notes(trace, "pp.dodge")
                                  if str(rec.get("txt", "")).split()[:1] not in (["hold"], []))
    print("\n  by bot: " + ", ".join(f"{trace.name(guid)} {n}" for guid, n in per_bot.most_common()))


def slot_name(value: str) -> str:
    if not value.isdigit():
        return value
    slot = int(value)
    if slot == 0:
        return "released"
    return f"inner {slot}" if slot <= INNER_SLOTS else f"outer {slot}"


def show_spread(trace: Trace) -> None:
    if refuse(trace, "SPREAD"):
        return
    end = icc.pull_end(trace)
    spots = [f"{value}@{clock(start)}" for value, start, _ in latch_spans(trace, "pp.spot", end)]
    print("  boss spot: " + (", ".join(spots) if spots else "never written"))
    slots = holder_spans(trace, "pp.slot", end)
    if not slots:
        print("  pp.slot never written")
        return
    print("\n  pp.slot per bot, each slot it held")
    for guid, spans in sorted(slots.items(), key=lambda item: trace.name(item[0])):
        held = ", ".join(f"{slot_name(value)}@{clock(start)}" for value, start, _ in spans)
        print(f"    {trace.name(guid):<14}  {held}")


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
    ("phases", "pp.phase, 1 to 3", show_phases),
    ("adds", "Volatile Ooze victims and stacks, Gas Cloud bloats and kites", show_adds),
    ("abom", "the abomination's driver, duty and Regurgitated Ooze", show_abom),
    ("plague", "Mutated Plague stacks and swaps, heroic Unbound Plague", show_plague),
    ("dodges", "each dodge by hazard and how it went", show_dodges),
    ("spread", "the boss spot and the ring slots", show_spread),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
