#!/usr/bin/env python3
"""Read a Lich King pull from a RaidObs trace: phases, assignments, icons, cheats.

    the_lich_king.py <file>                every section
    the_lich_king.py <file> --phases       lk.phase: one, winter1, two, winter2, three, frostmourne
    the_lich_king.py <file> --assignments  winter staging, Val'kyr buckets, stack slots, Defile, Spirit Bombs
    the_lich_king.py <file> --icons        every move of the skull, cross, star and circle, and its rule
    the_lich_king.py <file> --cheats       this encounter's cheat uses

What the generic views miss here:

- **`lk.phase` is his script's own state**, per bot. `winter1` and `winter2` run from his turn to
  passive at 70% and 40%, through the walk to the centre, the winter and the Quake, until he turns
  aggressive 5 s after it; the winter action itself runs only while the winter is up. `frostmourne`
  is heroic's Harvest Souls, written by the bots it took.
- **Each assignment is written by the bot that made it**: `lk.winterstage` by the bot that latched the
  winter's staging anchor (`spirit1` to `spirit3`, `none`), `lk.stackslot` by the bot that refreshed
  the phase three stack (`spirit1`, `spirit3`, `none`), `lk.valkyr` per bot, the bucket's icon and its
  Val'kyr, `none` once no Val'kyr is grabbing. `lk.sharedslot` is the raid's Vile Spirit slot, 0 to
  2, -1 for none.
- **`lk.bombdodge` is the main tank's rule among heroic's Spirit Bombs**: `safe`, `dodge`, `escape`
  through the lowest bomb, `stuck`.
- **The icons are written only by the bot that moved them**, rule and unit or `clear`: `shambling` and
  `spirit` in the winter; `boss` and `spirit` outside it; `valkyr` on each grabbing Val'kyr;
  `chasing` and `nearest` for the Wicked Spirit star in Frostmourne; `sphere` for the circle.
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
from raidobs.probes import emitted_keys, latch_spans, resolve_guids, silent_keys  # noqa: E402
from raidobs.trace import Trace, clock, notes  # noqa: E402

ENCOUNTER = "the-lich-king"
TITLE = "Lich King"
TWENTY_FIVE = (1, 3)
HEROIC = (2, 3)
# Only a second or third Val'kyr carries the cross or the star, and only 25-man summons three; heroic's
# Wicked Spirits put the star up too, and only heroic has Spirit Bombs.
TWENTY_FIVE_ONLY = {"lk.cross"}
HEROIC_ONLY = {"lk.bombdodge"}
TWENTY_FIVE_OR_HEROIC = {"lk.star"}
ICONS = ("lk.skull", "lk.cross", "lk.star", "lk.circle")
# The Val'kyr buckets' icons, first bucket first
BUCKET_ICONS = ("skull", "cross", "star")
INBOUND = ("lk.winterinbound", "lk.mtinbound", "lk.rangedinbound")
DODGE_RULES = ("safe", "dodge", "escape", "stuck")


def mine(trace: Trace) -> bool:
    return encounter_of(trace) == ENCOUNTER


def refuse(trace: Trace, title: str) -> bool:
    print(title)
    if mine(trace):
        return False
    print(f"  not a {TITLE} pull")
    return True


def spared(trace: Trace, key: str) -> bool:
    diff = trace.header.get("diff")
    if key in TWENTY_FIVE_ONLY:
        return diff not in TWENTY_FIVE
    if key in HEROIC_ONLY:
        return diff not in HEROIC
    if key in TWENTY_FIVE_OR_HEROIC:
        return diff not in TWENTY_FIVE and diff not in HEROIC
    return False


def missing_probes(trace: Trace) -> list[str]:
    gone = [key for key, _, _ in silent_keys(emitted_keys(trace), ENCOUNTER)]
    return [key for key in gone if not spared(trace, key)]


def show_banner(trace: Trace) -> None:
    icc.show_banner(trace)
    if not icc.is_icc(trace):
        return
    if not mine(trace):
        print(f"  not a {TITLE} pull, every section below reads empty")
        return
    gone = missing_probes(trace)
    print(f"probes absent from this trace: {', '.join(gone)}" if gone else "all lk.* probes present")


def show_phases(trace: Trace) -> None:
    if refuse(trace, "PHASES"):
        return
    steps = icc.phase_steps(trace)
    if not steps:
        print("  lk.phase never written")
        return
    print("  lk.phase, each step from the first bot to note it\n")
    print(f"    {'from':>9}  {'held':>9}  phase")
    for value, start, stop in steps:
        print(f"    {clock(start):>9}  {clock(stop - start):>9}  {value}")


def valkyr_buckets(trace: Trace) -> list[dict]:
    """Per Val'kyr, first assignment first: `t`, `icon`, `valkyr` and `bots`, the bots ever sent to it."""
    buckets: dict[tuple[str, str], dict] = {}
    for rec in sorted(notes(trace, "lk.valkyr"), key=lambda rec: rec["t"]):
        icon, _, valkyr = str(rec.get("txt", "")).partition(" ")
        if not valkyr:
            continue
        entry = buckets.setdefault((icon, valkyr), {"t": rec["t"], "icon": icon, "valkyr": valkyr, "bots": set()})
        entry["bots"].add(rec["g"])
    order = {icon: rank for rank, icon in enumerate(BUCKET_ICONS)}
    return sorted(buckets.values(), key=lambda entry: (entry["t"], order.get(entry["icon"], len(order))))


def inbound_counts(trace: Trace) -> dict[str, int]:
    """Per inbound latch, how many bots it ever sent walking."""
    counts = {}
    for key in INBOUND:
        bots = {rec["g"] for rec in notes(trace, key) if str(rec.get("txt", "")) == "1"}
        if bots:
            counts[key] = len(bots)
    return counts


def show_assignments(trace: Trace) -> None:
    if refuse(trace, "ASSIGNMENTS"):
        return
    end = icc.pull_end(trace)
    stages = sorted(notes(trace, "lk.winterstage"), key=lambda rec: rec["t"])
    if stages:
        print("  lk.winterstage, the staging anchor each winter latched")
        for rec in stages:
            print(f"    {clock(rec['t']):>9}  {trace.name(rec['g']):<14}  {rec.get('txt', '')}")
    else:
        print("  lk.winterstage never written")
    inbound = inbound_counts(trace)
    if inbound:
        print("  sent walking: " + ", ".join(f"{key} {count} bot(s)" for key, count in inbound.items()))

    buckets = valkyr_buckets(trace)
    if buckets:
        print("\n  lk.valkyr, each Val'kyr's bucket")
        for entry in buckets:
            names = sorted(trace.name(guid) for guid in entry["bots"])
            print(f"    {clock(entry['t']):>9}  {entry['icon']:<5}  {resolve_guids(trace, entry['valkyr'])}  "
                  f"{len(names)}: {', '.join(names)}")
    else:
        print("\n  lk.valkyr never assigned a Val'kyr")

    for key, what in (("lk.stackslot", "phase three stack"), ("lk.sharedslot", "Vile Spirit slot")):
        spans = latch_spans(trace, key, end)
        if not spans:
            print(f"\n  {key} never written")
            continue
        print(f"\n  {key}, the {what}")
        for value, start, stop in spans:
            print(f"    {clock(start):>9}  {clock(max(stop, start) - start):>9}  {value}")

    defiles = collections.Counter(rec["g"] for rec in notes(trace, "lk.defile"))
    if defiles:
        print(f"\n  lk.defile targets: {icc.ranked(trace, defiles)}")
    dodges = collections.Counter(str(rec.get("txt", "")) for rec in notes(trace, "lk.bombdodge"))
    if dodges:
        print("  lk.bombdodge switches: " + ", ".join(f"{rule} {dodges[rule]}" for rule in DODGE_RULES
                                                     if dodges[rule]))


def show_icons(trace: Trace) -> None:
    if refuse(trace, "ICONS"):
        return
    rows = sorted((rec for key in ICONS for rec in notes(trace, key)), key=lambda rec: (rec["t"], rec["k"]))
    if not rows:
        print("  lk.skull, lk.cross, lk.star and lk.circle never written")
        return
    for rec in rows:
        icon = rec["k"].partition(".")[2]
        print(f"    {clock(rec['t']):>9}  {icon:<6}  {trace.name(rec['g']):<14}  "
              f"{resolve_guids(trace, str(rec.get('txt', '')))}")


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
    ("phases", "lk.phase: one, winter1, two, winter2, three, frostmourne", show_phases),
    ("assignments", "winter staging, Val'kyr buckets, stack slots, Defile targets, Spirit Bombs", show_assignments),
    ("icons", "every move of the skull, cross, star and circle, and its rule", show_icons),
    ("cheats", "this encounter's cheat uses", show_cheats),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
