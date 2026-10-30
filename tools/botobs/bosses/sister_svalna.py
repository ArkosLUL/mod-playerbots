#!/usr/bin/env python3
"""Read a Sister Svalna pull from a RaidObs trace: Aether Shield, the spears, the duties, cheats.

    sister_svalna.py <file>            every section
    sister_svalna.py <file> --shield   each Aether Shield and how long it held
    sister_svalna.py <file> --spears   each Impaling Spear victim, and who threw the Infernal Spears
    sister_svalna.py <file> --duties   spear pullers, throw rules, kicks, captain tanks and targets per bot
    sister_svalna.py <file> --defensives  tank buttons and calls to a human tank
    sister_svalna.py <file> --cheats   cheat uses in this pull

Svalna has no phase, and her gauntlet's keys share her `ss.` prefix, so the banner leaves out the silent
`ss.gauntlet.*` ones (`svalna_gauntlet.py` reads those). What the generic views miss here:

- **No trace records an aura on a creature**, so `ss.shield` carries her Aether Shield 71463: `up`
  or `down`, per bot within 30 yd of her. A shield starts at the first bot to note `up` and ends at
  the first to note `down`.
- **Impaling Spear 71443 is an aura on the victim** until someone clicks the spear off, which hands
  the clicker an Infernal Spear. Its throw is Hurl Spear 71466, a `cast` from that bot on Svalna, and
  the hit that breaks the shield.
- **`ss.spear` is the spear each puller was elected for**, `0` once it lapses. `ss.hurl` is a holder's
  rule: `throw`, `walk` (into 24 yd), `wait` (another holder throws), `hold` (she is unshielded).
- **`ss.tank` is the raised captain each bot tank holds**, `0` once it lapses. `ss.target` is the
  resolver's branch: `skull`, `owned`, `captain`, `svalna`, `none`.
- **`ss.interrupt` is the bot elected to kick Arnath**: `dominate` (Dominate Mind, 25-man) or `heal`
  (Flash Heal) while it holds the kick, `none` otherwise.
- **`ss.defensive` is the tank ladder's pick**, a cast name, `idle` outside a window: a tank a captain
  hits under 40%. `ss.humancall` is a call to a human tank.
- **Svalna has no cheat site.** Any `cheat` row here is another encounter's code running in her pull.
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

ENCOUNTER = "sister-svalna"
TITLE = "Sister Svalna"
GAUNTLET_PREFIX = "ss.gauntlet."
IMPALING_SPEAR = 71443
HURL_SPEAR = 71466
RELEASED = "0"
HURLS = ("throw", "walk", "wait", "hold")
INTERRUPTS = ("dominate", "heal", "none")
TARGETS = ("skull", "owned", "captain", "svalna", "none")
DEFENSIVE = "ss.defensive"
HUMAN_CALL = "ss.humancall"


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
    gone = [key for key, _, _ in silent_keys(emitted_keys(trace), ENCOUNTER) if not key.startswith(GAUNTLET_PREFIX)]
    print(f"probes absent from this trace: {', '.join(gone)}" if gone else "all ss.* probes present")


def shields(trace: Trace) -> list[tuple[int, int]]:
    """`(start, stop)` per Aether Shield, the last one running to the pull's end."""
    return [(start, stop) for value, start, stop in latch_spans(trace, "ss.shield", icc.pull_end(trace))
            if value == "up"]


def show_shield(trace: Trace) -> None:
    if refuse(trace, "SHIELD"):
        return
    rows = shields(trace)
    if not rows:
        print("  ss.shield never noted up")
        return
    print(f"  {len(rows)} shield(s), from the first bot to note each edge\n")
    for start, stop in rows:
        print(f"    {clock(start):>9}  held {clock(stop - start):>9}")


def impales(trace: Trace) -> list[dict]:
    """Per Impaling Spear: `victim`, `start`, `stop` (the aura's removal, else the pull's end) and whether
    the victim `died` inside it."""
    end = icc.pull_end(trace)
    open_at: dict[int, int] = {}
    windows = []
    for rec in trace.of("aura"):
        if rec.get("sp") != IMPALING_SPEAR:
            continue
        victim = rec["d"]
        if not rec.get("r"):
            open_at.setdefault(victim, rec["t"])
        elif victim in open_at:
            windows.append((victim, open_at.pop(victim), rec["t"]))
    windows += [(victim, start, end) for victim, start in open_at.items()]
    dead = first_deaths(trace)
    return [{"victim": victim, "start": start, "stop": stop, "died": start <= dead.get(victim, -1) <= stop}
            for victim, start, stop in sorted(windows, key=lambda window: window[1])]


def show_spears(trace: Trace) -> None:
    if refuse(trace, "SPEARS"):
        return
    rows = impales(trace)
    if rows:
        print(f"  {len(rows)} impale(s)\n")
        for row in rows:
            human = " human" if row["victim"] in trace.humans else ""
            died = "  died" if row["died"] else ""
            print(f"    {clock(row['start']):>9}  {clock(row['stop'] - row['start']):>9}  "
                  f"{trace.name(row['victim'])}{human}{died}")
    else:
        print("  nobody was impaled")
    throws = [rec for rec in trace.of("cast") if rec.get("sp") == HURL_SPEAR]
    if not throws:
        print("\n  no Hurl Spear cast")
        return
    print("\n  Hurl Spear casts")
    for rec in throws:
        print(f"    {clock(rec['t']):>9}  {trace.name(rec.get('s')):<14}  on {trace.name(rec.get('tgt'))}")


def held(trace: Trace, key: str) -> dict[int, list[tuple[str, int]]]:
    """Per holder of `key`: each unit it held, by name, or `released`, with when."""
    return {guid: [(resolve_guids(trace, value) if value != RELEASED else "released", start)
                   for value, start, _ in spans]
            for guid, spans in holder_spans(trace, key, icc.pull_end(trace)).items()}


def value_counts(trace: Trace, key: str) -> dict[int, collections.Counter]:
    return {guid: collections.Counter(value for value, _, _ in spans)
            for guid, spans in holder_spans(trace, key, icc.pull_end(trace)).items()}


def counted(counts: collections.Counter, order: tuple[str, ...]) -> str:
    keys = [key for key in order if counts[key]] + sorted(key for key in counts if key not in order)
    return ", ".join(f"{key} {counts[key]}" for key in keys) or "-"


def show_duties(trace: Trace) -> None:
    if refuse(trace, "DUTIES"):
        return
    for key, title in (("ss.spear", "spear pullers"), ("ss.tank", "captain tanks")):
        rows = held(trace, key)
        if not rows:
            print(f"  {key} never written")
            continue
        print(f"  {key}, {title}: each unit held, from when")
        for guid, spans in sorted(rows.items(), key=lambda item: trace.name(item[0])):
            print(f"    {trace.name(guid):<14}  " + ", ".join(f"{name} {clock(start)}" for name, start in spans))
    for key, order in (("ss.hurl", HURLS), ("ss.interrupt", INTERRUPTS), ("ss.target", TARGETS)):
        rows = value_counts(trace, key)
        if not rows:
            print(f"  {key} never written")
            continue
        print(f"  {key} per bot: spans of each rule")
        for guid, counts in sorted(rows.items(), key=lambda item: trace.name(item[0])):
            print(f"    {trace.name(guid):<14}  {counted(counts, order)}")


def show_defensives(trace: Trace) -> None:
    if refuse(trace, "DEFENSIVES"):
        return
    rows = holder_spans(trace, DEFENSIVE, icc.pull_end(trace))
    offered = {guid: [(value, start) for value, start, _ in spans if value != "idle"] for guid, spans in rows.items()}
    offered = {guid: picks for guid, picks in offered.items() if picks}
    if not offered:
        print(f"  {DEFENSIVE} never offered a button")
    for guid, picks in sorted(offered.items(), key=lambda item: trace.name(item[0])):
        print(f"    {trace.name(guid):<14}  " + ", ".join(f"{value} {clock(start)}" for value, start in picks))
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
    print(f"  {sum(row['n'] for row in rows)} use(s), none of them Svalna's: icecrown_citadel.py --cheats")


SECTIONS = (
    ("shield", "each Aether Shield and how long it held", show_shield),
    ("spears", "each Impaling Spear victim and the Hurl Spear casts", show_spears),
    ("duties", "spear pullers, throw rules, kicks, captain tanks and targets per bot", show_duties),
    ("defensives", "tank buttons and calls to a human tank", show_defensives),
    ("cheats", "cheat uses in this pull", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
