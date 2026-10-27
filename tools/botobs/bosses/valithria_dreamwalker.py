#!/usr/bin/env python3
"""Read a Valithria Dreamwalker pull from a RaidObs trace: portal waves, Dream State, healers, marks.

    valithria_dreamwalker.py <file>            every section
    valithria_dreamwalker.py <file> --waves    each portal wave, its claims and who went through
    valithria_dreamwalker.py <file> --dream    each Dream State window and the cloud picks
    valithria_dreamwalker.py <file> --heal     which healers heal the raid and which take portals
    valithria_dreamwalker.py <file> --marks    every move of the skull and cross, and the 25-man groups
    valithria_dreamwalker.py <file> --cheats   this encounter's cheat uses

Valithria has no boss phase, so the portal waves stand in for one. What the generic views miss here:

- **Portals are friendly, so no snapshot holds them.** `vd.wave` is per healer: `pre` while the
  pre-effects show their 15 s visual, `open` while a real portal stands (10 s, or until clicked),
  `none` otherwise. A wave starts at the first healer to note it.
- **Dream State is the `aura` stream**, spell 70766 for 20 s, so no probe repeats it. A bot inside is
  phased away from the adds, and from everyone outside.
- **`vd.cloud` is written by the bot that moves the shared cloud target**: `cloud` and the unit, `none`
  once no cloud is left. `human` and the unit is each bot's own pick while a human is in Dream State.
  A bot repeating its own last value writes nothing.
- **`vd.raidheal` is per healer**: `raid` heals the raid, `boss` takes portals and heals her, `none`
  for a healer the ranking left out.
- **`vd.skull` and `vd.cross` are written only by the bot that moved the icon**: the kill tier
  (`skeleton`, `suppresser`, `archmage`, `zombie`, `abomination`, `worm`), then ` other` when a 25-man
  group took an add off the other group's half, then the unit; or `clear`. `vd.group` is each bot's
  25-man mark: `skull`, `cross`, `single` while at most one priority add is up, or `none`.
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

ENCOUNTER = "valithria-dreamwalker"
TITLE = "Valithria Dreamwalker"
DREAM_STATE = 70766
TWENTY_FIVE = (1, 3)
TWENTY_FIVE_ONLY = {"vd.cross", "vd.group"}
NO_CLAIM = "0"


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
    print(f"probes absent from this trace: {', '.join(gone)}" if gone else "all vd.* probes present")


def dream_windows(trace: Trace) -> list[dict]:
    """Per Dream State: `bot`, `start`, `stop` (the aura's removal, else the pull's end) and whether the
    bot `died` inside it."""
    end = icc.pull_end(trace)
    open_at: dict[int, int] = {}
    windows = []
    for rec in trace.of("aura"):
        if rec.get("sp") != DREAM_STATE:
            continue
        bot = rec["d"]
        if not rec.get("r"):
            open_at.setdefault(bot, rec["t"])
        elif bot in open_at:
            windows.append((bot, open_at.pop(bot), rec["t"]))
    windows += [(bot, start, end) for bot, start in open_at.items()]

    dead = first_deaths(trace)
    return [{"bot": bot, "start": start, "stop": stop, "died": start <= dead.get(bot, -1) <= stop}
            for bot, start, stop in sorted(windows, key=lambda window: (window[1], window[0]))]


def waves(trace: Trace) -> list[dict]:
    """Per portal wave: `pre` and `open`, when each was first noted (None when never), `stop` at the
    next `none` (else the pull's end), `claims`, the healers that claimed a portal inside it, and
    `dreamers`, the bots that entered Dream State inside it."""
    end = icc.pull_end(trace)
    rows: list[dict] = []
    current: dict | None = None
    for value, start, _ in latch_spans(trace, "vd.wave", end):
        if value == "none":
            if current:
                current["stop"] = start
                rows.append(current)
                current = None
            continue
        if current is None:
            current = {"pre": None, "open": None, "stop": end}
        if value == "pre" and current["pre"] is None and current["open"] is None:
            current["pre"] = start
        elif value == "open" and current["open"] is None:
            current["open"] = start
    if current:
        rows.append(current)

    claims = [rec for rec in notes(trace, "vd.portalclaim") if str(rec.get("txt", "")) != NO_CLAIM]
    dreams = dream_windows(trace)
    for row in rows:
        begin = row["pre"] if row["pre"] is not None else row["open"]
        row["claims"] = sorted({rec["g"] for rec in claims if begin <= rec["t"] <= row["stop"]})
        row["dreamers"] = sorted({window["bot"] for window in dreams if begin <= window["start"] <= row["stop"]})
    return rows


def show_waves(trace: Trace) -> None:
    if refuse(trace, "WAVES"):
        return
    rows = waves(trace)
    if not rows:
        print("  vd.wave never written")
        return
    print(f"  {len(rows)} wave(s); pre and open from the first healer to note each\n")
    print(f"    {'pre':>9}  {'open':>9}  {'end':>9}  claimed / went in")
    for row in rows:
        pre = clock(row["pre"]) if row["pre"] is not None else "-"
        opened = clock(row["open"]) if row["open"] is not None else "-"
        claimed = ", ".join(trace.name(guid) for guid in row["claims"]) or "-"
        went = ", ".join(trace.name(guid) for guid in row["dreamers"]) or "-"
        print(f"    {pre:>9}  {opened:>9}  {clock(row['stop']):>9}  {claimed} / {went}")


def show_dream(trace: Trace) -> None:
    if refuse(trace, "DREAM"):
        return
    rows = dream_windows(trace)
    if not rows:
        print("  nobody entered Dream State")
    else:
        print(f"  {len(rows)} Dream State window(s)\n")
        for row in rows:
            human = " human" if row["bot"] in trace.humans else ""
            died = "  died" if row["died"] else ""
            print(f"    {clock(row['start']):>9}  {clock(row['stop'] - row['start']):>9}  "
                  f"{trace.name(row['bot'])}{human}{died}")
    picks = collections.Counter(str(rec.get("txt", "")).partition(" ")[0] for rec in notes(trace, "vd.cloud"))
    if picks:
        print("\n  vd.cloud: " + ", ".join(f"{rule} {count}" for rule, count in sorted(picks.items())))
    else:
        print("\n  vd.cloud never written")


def show_heal(trace: Trace) -> None:
    if refuse(trace, "HEAL"):
        return
    spans = holder_spans(trace, "vd.raidheal", icc.pull_end(trace))
    if not spans:
        print("  vd.raidheal never written")
        return
    print("  vd.raidheal per healer, each change\n")
    for guid, rows in sorted(spans.items(), key=lambda item: trace.name(item[0])):
        print(f"    {trace.name(guid):<14}  " + ", ".join(f"{value}@{clock(start)}" for value, start, _ in rows))


def show_marks(trace: Trace) -> None:
    if refuse(trace, "MARKS"):
        return
    rows = sorted(notes(trace, "vd.skull") + notes(trace, "vd.cross"), key=lambda rec: (rec["t"], rec["k"]))
    if not rows:
        print("  vd.skull and vd.cross never written")
    for rec in rows:
        icon = rec["k"].partition(".")[2]
        print(f"    {clock(rec['t']):>9}  {icon:<5}  {trace.name(rec['g']):<14}  "
              f"{resolve_guids(trace, str(rec.get('txt', '')))}")
    groups = holder_spans(trace, "vd.group", icc.pull_end(trace))
    if not groups:
        if twenty_five(trace):
            print("\n  vd.group never written")
        return
    print("\n  vd.group per bot, each change")
    for guid, spans in sorted(groups.items(), key=lambda item: trace.name(item[0])):
        print(f"    {trace.name(guid):<14}  " + ", ".join(f"{value}@{clock(start)}" for value, start, _ in spans))


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
    ("waves", "each portal wave, its claims and who went through", show_waves),
    ("dream", "each Dream State window and the cloud picks", show_dream),
    ("heal", "which healers heal the raid and which take portals", show_heal),
    ("marks", "every move of the skull and cross, and the 25-man groups", show_marks),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
