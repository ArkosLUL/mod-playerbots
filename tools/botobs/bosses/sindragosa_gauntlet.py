#!/usr/bin/env python3
"""Read a Sindragosa gauntlet pull from a RaidObs trace: its waves of adds, targets, taunts, dodges, cheats.

    sindragosa_gauntlet.py <file>            every section
    sindragosa_gauntlet.py <file> --waves    each kind of add: how many came, first and last sampled
    sindragosa_gauntlet.py <file> --targets  per bot: the resolver rules it was handed
    sindragosa_gauntlet.py <file> --taunts   per tank: each rescue taunt it owed, from when
    sindragosa_gauntlet.py <file> --dodges   per player: Frostblade dodges and hits
    sindragosa_gauntlet.py <file> --kicks    each Sorceress Ice Tomb cast: who owed the kick, did it land
    sindragosa_gauntlet.py <file> --freedom  per bot: each root it owed a freeing spell on; roots taken
    sindragosa_gauntlet.py <file> --cheats   cheat uses in this pull

The gauntlet has no phase, and its keys share Sindragosa's `sg.` prefix, so the banner names only the
silent `sg.gauntlet.*` ones. What the generic views miss here:

- **Three waves, each once only Nerub'ar Broodlings are left**: the Nerub'ar Champions and Webweavers
  waiting on the webs drop first, Frostwarden Warriors and Sorceresses walk in second, a fresh set of
  spiders drops third. Broodlings come with every wave. The controller dies after the third, which
  is the `DONE`.
- **Spiders split on the first Frostwarden sighting**, so the first and third waves read apart. The
  first wave's spiders hang on the webs from the reset, so some may be sampled before the pull.
- **`sg.gauntlet.target` is the resolver's rule**, `none` counted as a rule: a player's `skull`, a
  tank's `rescue` of a Champion or Warrior hitting a non-tank, then the adds with a victim in the
  group, `sorceress`, `webweaver`, `champion`, `warrior`, `broodling`.
- **`sg.gauntlet.taunt` is the kind of add a tank's rescue taunt is due on**, `champion` or `warrior`:
  it hits a non-tank, and this tank is the nearest with its taunt ready and in range. `none` once not.
- **`sg.gauntlet.dodge` is a bot leaving a Frostblade 38199's whirl** for 14.5 yd off it: `melee` to a
  spot that keeps its target in reach, `clear` to any clear spot, `blocked` with none, `none` once
  out. The blade's whirlwind hits are Frostblade 70305 `dmg` rows.
- **`sg.gauntlet.kick` is `sorceress` while a bot owes the interrupt** on a Sorceress casting Ice Tomb,
  the lowest guid whose interrupt reaches her. A cast landed when an Ice Tomb 71330/71331 `aura` row
  from the same Sorceress shows up before its cast time is out.
- **`sg.gauntlet.free` is the spell a paladin or priest owes** a member rooted by Frost Nova 71320 or
  Web 71327 inside a Frostblade's whirl: `hand of freedom`, or `dispel magic` on Frost Nova.
- **No gauntlet code cheats.** Any `cheat` row here is another encounter's code.
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
from raidobs.probes import emitted_keys, holder_spans, resolve_guids, silent_keys  # noqa: E402
from raidobs.trace import Trace, clock, roster_guids  # noqa: E402

ENCOUNTER = "sindragosa-gauntlet"
TITLE = "Sindragosa gauntlet"
PREFIX = "sg.gauntlet."
SPIDERS = (37501, 37502)
FROSTWARDENS = (37228, 37229)
BROODLINGS = (37232,)
FROSTBLADE = 70305
ICE_TOMB = (71330, 71331)
ICE_TOMB_CAST_MS = 1500
# an aura row can trail the cast's end by a tick
LAND_SLACK_MS = 500
ROOTS = (71320, 71327)
OFF = ("none", "0")


def mine(trace: Trace) -> bool:
    return encounter_of(trace) == ENCOUNTER


def refuse(trace: Trace, title: str) -> bool:
    print(title)
    if mine(trace):
        return False
    print(f"  not a {TITLE} pull")
    return True


def missing_probes(trace: Trace) -> list[str]:
    return [key for key, _, _ in silent_keys(emitted_keys(trace), ENCOUNTER) if key.startswith(PREFIX)]


def show_banner(trace: Trace) -> None:
    icc.show_banner(trace)
    if not icc.is_icc(trace):
        return
    if not mine(trace):
        print(f"  not a {TITLE} pull, every section below reads empty")
        return
    gone = missing_probes(trace)
    print(f"probes absent from this trace: {', '.join(gone)}" if gone else f"all {PREFIX}* probes present")


def sampled(trace: Trace, entries: tuple[int, ...]) -> dict[int, tuple[int, int]]:
    """guid -> (first, last) sample, for every unit of these entries the snapshots carried."""
    guids = set().union(*(guids_of_entry(trace, entry) for entry in entries))
    return {guid: (rows[0][0], rows[-1][0]) for guid, rows in track(trace, guids, cols=("t",)).items() if rows}


def wave_rows(trace: Trace) -> list[tuple[str, int, int, int]]:
    """`(kind, units, first, last)` per kind of add sampled, spiders split at the first Frostwarden."""
    wardens = sampled(trace, FROSTWARDENS)
    split = min((first for first, _ in wardens.values()), default=None)
    spiders = sampled(trace, SPIDERS)
    groups = [
        ("spiders, first wave", {g: s for g, s in spiders.items() if split is None or s[0] < split}),
        ("frostwardens", wardens),
        ("spiders, third wave", {g: s for g, s in spiders.items() if split is not None and s[0] >= split}),
        ("broodlings", sampled(trace, BROODLINGS)),
    ]
    return [(kind, len(units), min(first for first, _ in units.values()), max(last for _, last in units.values()))
            for kind, units in groups if units]


def show_waves(trace: Trace) -> None:
    if refuse(trace, "WAVES"):
        return
    rows = wave_rows(trace)
    if not rows:
        print("  no gauntlet add sampled")
        return
    print(f"    {'kind':<20}  {'units':>5}  {'first':>9}  {'last':>9}")
    for kind, units, first, last in rows:
        print(f"    {kind:<20}  {units:>5}  {clock(first):>9}  {clock(last):>9}")


def rule_counts(trace: Trace, key: str, skip=OFF) -> dict[int, collections.Counter]:
    """Per guid: how many spans each rule of `key`, a value's first word, started."""
    return {guid: collections.Counter(value.split(" ", 1)[0] for value, _, _ in spans if value not in skip)
            for guid, spans in holder_spans(trace, key, icc.pull_end(trace)).items()}


def counted(counts: collections.Counter) -> str:
    return ", ".join(f"{key} {counts[key]}" for key in sorted(counts)) or "-"


def show_targets(trace: Trace) -> None:
    if refuse(trace, "TARGETS"):
        return
    rows = rule_counts(trace, "sg.gauntlet.target", skip=())
    if not rows:
        print("  sg.gauntlet.target never written")
        return
    print("  per bot: spans of each resolver rule\n")
    for guid in sorted(rows, key=trace.name):
        print(f"    {trace.name(guid)}  {counted(rows[guid])}")


def show_taunts(trace: Trace) -> None:
    if refuse(trace, "TAUNTS"):
        return
    rows = holder_spans(trace, "sg.gauntlet.taunt", icc.pull_end(trace))
    if not rows:
        print("  sg.gauntlet.taunt never written")
        return
    print("  sg.gauntlet.taunt per tank: each rescue taunt it owed, from when\n")
    for guid in sorted(rows, key=trace.name):
        taunts = ", ".join(f"{resolve_guids(trace, value)} {clock(start)}"
                           for value, start, _ in rows[guid] if value not in OFF)
        print(f"    {trace.name(guid)}  {taunts or '-'}")


def blade_hits(trace: Trace) -> collections.Counter:
    """Per roster player: the Frostblade whirlwind hits it took."""
    roster = roster_guids(trace)
    return collections.Counter(rec["d"] for rec in trace.of("dmg")
                               if rec.get("sp") == FROSTBLADE and rec.get("d") in roster)


def show_dodges(trace: Trace) -> None:
    if refuse(trace, "DODGES"):
        return
    dodges = rule_counts(trace, "sg.gauntlet.dodge")
    taken = blade_hits(trace)
    players = set(dodges) | set(taken)
    if not players:
        print("  no sg.gauntlet.dodge note and no Frostblade hit")
        return
    print("  per player: sg.gauntlet.dodge rules, Frostblade hits\n")
    for guid in sorted(players, key=trace.name):
        print(f"    {icc.who(trace, guid)}  dodges {counted(dodges.get(guid, collections.Counter()))}"
              f"  hits {taken[guid]}")


def kick_rows(trace: Trace) -> list[dict]:
    """Per Ice Tomb cast: `start`, `land`, `kickers` (bots that noted the kick inside it) and `landed` (an
    Ice Tomb aura from the same Sorceress showed up before it was out)."""
    owed = holder_spans(trace, "sg.gauntlet.kick", icc.pull_end(trace))
    tombed = [(rec["t"], rec.get("s")) for rec in trace.of("aura") if rec.get("sp") in ICE_TOMB and not rec.get("r")]
    rows = []
    for rec in trace.of("cast"):
        if rec.get("sp") not in ICE_TOMB:
            continue
        start = rec["t"]
        land = start + (rec.get("ct") or ICE_TOMB_CAST_MS)
        kickers = sorted((guid for guid, spans in owed.items()
                          if any(value not in OFF and low <= land and high >= start for value, low, high in spans)),
                         key=trace.name)
        # Two Sorceresses can cast at once
        landed = any(start <= when <= land + LAND_SLACK_MS and caster == rec.get("s") for when, caster in tombed)
        rows.append({"start": start, "land": land, "kickers": kickers, "landed": landed})
    return rows


def show_kicks(trace: Trace) -> None:
    if refuse(trace, "KICKS"):
        return
    rows = kick_rows(trace)
    if not rows:
        print("  no Ice Tomb cast")
        return
    print(f"  {len(rows)} Ice Tomb cast(s): who owed the kick, whether the tomb landed\n")
    for row in rows:
        kickers = ", ".join(trace.name(guid) for guid in row["kickers"]) or "nobody"
        print(f"    {clock(row['start']):>9}  kick {kickers}  {'landed' if row['landed'] else 'stopped'}")


def show_freedom(trace: Trace) -> None:
    if refuse(trace, "FREEDOM"):
        return
    owed = holder_spans(trace, "sg.gauntlet.free", icc.pull_end(trace))
    roster = roster_guids(trace)
    roots = collections.Counter(rec["d"] for rec in trace.of("aura")
                                if rec.get("sp") in ROOTS and not rec.get("r") and rec.get("d") in roster)
    if not owed and not roots:
        print("  no sg.gauntlet.free note and no Frost Nova or Web root")
        return
    if owed:
        print("  sg.gauntlet.free per bot: each freeing spell it owed, from when")
        for guid in sorted(owed, key=trace.name):
            frees = ", ".join(f"{value} {clock(start)}" for value, start, _ in owed[guid] if value not in OFF)
            print(f"    {trace.name(guid)}  {frees or '-'}")
    else:
        print("  sg.gauntlet.free never written")
    print(f"  Frost Nova and Web roots taken: {icc.ranked(trace, roots) or 'none'}")


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
    print(f"  {sum(row['n'] for row in rows)} use(s), none from gauntlet code: icecrown_citadel.py --cheats")


SECTIONS = (
    ("waves", "each kind of add: how many came, first and last sampled", show_waves),
    ("targets", "per bot: the resolver rules it was handed", show_targets),
    ("taunts", "per tank: each rescue taunt it owed, from when", show_taunts),
    ("dodges", "per player: Frostblade dodges and hits", show_dodges),
    ("kicks", "each Sorceress Ice Tomb cast: who owed the kick, did it land", show_kicks),
    ("freedom", "per bot: each root it owed a freeing spell on; roots taken", show_freedom),
    ("cheats", "cheat uses in this pull", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
