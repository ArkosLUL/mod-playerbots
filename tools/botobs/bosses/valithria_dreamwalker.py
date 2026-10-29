#!/usr/bin/env python3
"""Read a Valithria Dreamwalker pull from a RaidObs trace: portal waves, Dream State, healers, adds, zombies.

    valithria_dreamwalker.py <file>            every section
    valithria_dreamwalker.py <file> --waves    each portal wave, its claims and who went through
    valithria_dreamwalker.py <file> --dream    each Dream State window, the flight rules and the shared cloud
    valithria_dreamwalker.py <file> --heal     which healers heal the raid and which take portals
    valithria_dreamwalker.py <file> --adds     the resolver's tiers per bot and the abominations' tanks
    valithria_dreamwalker.py <file> --zombies  kites, snares, dodges and Mana Void steps per bot
    valithria_dreamwalker.py <file> --cheats   cheat uses in this pull

Valithria has no boss phase, so the portal waves stand in for one. What the generic views miss here:

- **Portals are friendly, so no snapshot holds them.** `vd.wave` is per bot healer: `pre` while the
  pre-effects show their 15 s visual, `open` while a real portal stands (10 s, or until clicked),
  `none` otherwise. A wave starts at the first healer to note it.
- **Dream State is the `aura` stream**, spell 70766 for 20 s, so no probe repeats it. A bot inside is
  phased away from the adds, and from everyone outside. `vd.dream` is the flight rule per bot:
  `refresh` (its nearest cloud), `gather` (onto the group's leader), `cloud` (the shared one), `human`
  (onto a player in Dream State), `hover`, `descend` (the last 3.5 s), `land` (still in the air after
  it), `none`. `vd.cloud` is written by the bot that moves the shared cloud: `cloud` and the unit, or
  `none`.
- **`vd.healer` is the split**: `1` heals the raid, `2` takes portals and heals her, `0` released.
  `vd.portalclaim` is a portal healer's claimed pre-effect, `0` once it lapses.
- **`vd.target` is the resolver's pick per tank and damage dealer**: `skull`, `owned` (a tank's own
  abomination), the tier (`skeleton`, `archmage`, `worm`, `suppresser`, `zombie`, `abomination`) or
  `none`. `vd.tank` is the abomination each bot tank holds, `0` once it lapses.
- **Zombies**: `vd.kite` (`ring ccw`, `ring cw`, `reverse`, `blocked`, `none`) per kiter, `vd.snare`
  (the spell, or `none`) per elected snarer, `vd.dodge` (`column`, `burst`, `blocked`, `none`) and
  `vd.void` (`step`, `blocked`, `none`) per bot.
- **Valithria has no cheat site.** Any `cheat` row here is another encounter's code running in her pull.
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
NO_CLAIM = "0"
HEALER_DUTIES = {"1": "raid", "2": "portal", "0": "released"}
FLIGHTS = ("refresh", "gather", "cloud", "human", "hover", "descend", "land", "none")
TIERS = ("skull", "owned", "skeleton", "archmage", "worm", "suppresser", "zombie", "abomination", "none")


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
    print(f"probes absent from this trace: {', '.join(gone)}" if gone else "all vd.* probes present")


def value_counts(trace: Trace, key: str) -> dict[int, collections.Counter]:
    """Per guid: how many spans each value of `key` started."""
    return {guid: collections.Counter(value for value, _, _ in spans)
            for guid, spans in holder_spans(trace, key, icc.pull_end(trace)).items()}


def counted(counts: collections.Counter, order: tuple[str, ...] = ()) -> str:
    keys = [key for key in order if counts[key]] + sorted(key for key in counts if key not in order)
    return ", ".join(f"{key} {counts[key]}" for key in keys) or "-"


def by_name(trace: Trace, rows: dict) -> list:
    return sorted(rows.items(), key=lambda item: trace.name(item[0]))


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
    flights = value_counts(trace, "vd.dream")
    if flights:
        print("\n  vd.dream per bot: spans of each flight rule")
        for guid, counts in by_name(trace, flights):
            print(f"    {trace.name(guid):<14}  {counted(counts, FLIGHTS)}")
    else:
        print("\n  vd.dream never written")
    picks = collections.Counter(str(rec.get("txt", "")).partition(" ")[0] for rec in notes(trace, "vd.cloud"))
    if picks:
        print("\n  vd.cloud: " + ", ".join(f"{rule} {count}" for rule, count in sorted(picks.items())))
    else:
        print("\n  vd.cloud never written")


def show_heal(trace: Trace) -> None:
    if refuse(trace, "HEAL"):
        return
    spans = holder_spans(trace, "vd.healer", icc.pull_end(trace))
    if not spans:
        print("  vd.healer never written")
        return
    print("  vd.healer per bot healer, each change\n")
    for guid, rows in sorted(spans.items(), key=lambda item: trace.name(item[0])):
        changes = ", ".join(f"{HEALER_DUTIES.get(value, value)}@{clock(start)}" for value, start, _ in rows)
        print(f"    {trace.name(guid):<14}  {changes}")


def show_adds(trace: Trace) -> None:
    if refuse(trace, "ADDS"):
        return
    targets = value_counts(trace, "vd.target")
    if targets:
        print("  vd.target per bot: spans of each pick\n")
        for guid, counts in by_name(trace, targets):
            print(f"    {trace.name(guid):<14}  {counted(counts, TIERS)}")
    else:
        print("  vd.target never written")
    tanks = holder_spans(trace, "vd.tank", icc.pull_end(trace))
    if not tanks:
        print("\n  vd.tank never written")
        return
    print("\n  vd.tank per bot tank: the abominations it held, from when")
    for guid, spans in sorted(tanks.items(), key=lambda item: trace.name(item[0])):
        held = ", ".join(f"{resolve_guids(trace, value) if value != NO_CLAIM else 'released'} {clock(start)}"
                         for value, start, _ in spans)
        print(f"    {trace.name(guid):<14}  {held}")


def zombie_rows(trace: Trace) -> list[dict]:
    """Per bot that noted any zombie or hazard key: its counts of each value but `none`."""
    keys = ("vd.kite", "vd.snare", "vd.dodge", "vd.void")
    counts = {key: value_counts(trace, key) for key in keys}
    guids = set().union(*(rows.keys() for rows in counts.values()))
    rows = []
    for guid in guids:
        row = {"guid": guid}
        for key in keys:
            found = counts[key].get(guid, collections.Counter())
            row[key] = collections.Counter({value: n for value, n in found.items() if value != "none"})
        if any(row[key] for key in keys):
            rows.append(row)
    return sorted(rows, key=lambda row: trace.name(row["guid"]))


def show_zombies(trace: Trace) -> None:
    if refuse(trace, "ZOMBIES"):
        return
    rows = zombie_rows(trace)
    if not rows:
        print("  no kite, snare, dodge or void step in this pull")
        return
    print("  per bot: spans of each kite leg rule, snare spell, dodge and void step\n")
    for row in rows:
        print(f"    {trace.name(row['guid']):<14}  kite {counted(row['vd.kite'])}; snare {counted(row['vd.snare'])}; "
              f"dodge {counted(row['vd.dodge'])}; void {counted(row['vd.void'])}")


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
    ("dream", "each Dream State window, the flight rules and the shared cloud", show_dream),
    ("heal", "which healers heal the raid and which take portals", show_heal),
    ("adds", "the resolver's tiers per bot and the abominations' tanks", show_adds),
    ("zombies", "kites, snares, dodges and Mana Void steps per bot", show_zombies),
    ("cheats", "cheat uses in this pull", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
