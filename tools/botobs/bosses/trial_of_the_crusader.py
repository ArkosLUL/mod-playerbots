#!/usr/bin/env python3
"""Score Trial of the Crusader's pre-pull staging from a RaidObs trace: where each bot stood when its
boss turned hostile, and whom the boss went for first.

    trial_of_the_crusader.py <file>             every section
    trial_of_the_crusader.py <file> --staging   each release: victim, duty holder, ms to a tank, early hits, stands

Raid-wide, so it reads a trace of any ToC encounter. What the generic views miss here, and what this
reads instead:

- **A release is a unit's first victim.** ToC keeps no boss state, and a Beasts trace opens at
  Gormok's engage, so nothing marks a later beast turning on the raid but the first snapshot row
  whose target column is set. Gormok, each worm, Icehowl, Jaraxxus, each twin and Anub'arak get their
  own; of the 28 champions only the first to take a target. Jaraxxus kills Fizzlebang with a
  triggered cast, which sets no victim, so the intro never reads as one.
- **`toc.stage` is per bot and change only, and a session opens at the engage.** A note written
  before the open never reaches the file, so for 2 s after the arrival closes any bot's tick notes
  every bot's last label again. The note that counts is the bot's latest up to 2 s past the release:
  `none` when that reads `none` (in the window with no stand), else its latest whose label leads with
  the arrival.
- **A stand is rounded to the yard** (`DescribeDerived`), so a bot parked on it reads up to 0.7 yd off.
- **` human` after a duty holder's stand** means a living human stood nearer the release point than
  the stand did, so a forced attack opens on the human.
"""
from __future__ import annotations

import pathlib
import sys

# Run as a script from bosses/, so the raidobs package one level up is not on the path yet.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from raidobs.cli import run_sections  # noqa: E402
from raidobs.encounter import encounter_of, map_of  # noqa: E402
from raidobs.geometry import BEFORE, at, dist2, guids_of_entry, track  # noqa: E402
from raidobs.trace import Trace, clock, combat_deaths, notes, roster_guids  # noqa: E402

MAP_TOC = 649
DIFFICULTIES = {0: "10N", 1: "25N", 2: "10H", 3: "25H"}

STAGE_KEY = "toc.stage"
NO_STAND = "none"
HUMAN_SUFFIX = "human"

NPC_GORMOK = 34796
NPC_DREADSCALE = 34799
NPC_ACIDMAW = 35144
NPC_ICEHOWL = 34797
NPC_JARAXXUS = 34780
NPC_FJOLA = 34497
NPC_EYDIS = 34496
NPC_ANUBARAK = 34564

# (entry, arrival, duty roles), named as a `toc.stage` label's two halves
STAGED_UNITS = (
    (NPC_GORMOK, "gormok", ("tank",)),
    (NPC_DREADSCALE, "worms", ("mobile",)),
    (NPC_ACIDMAW, "worms", ("stationary",)),
    (NPC_ICEHOWL, "icehowl", ("tank",)),
    (NPC_JARAXXUS, "jaraxxus", ("tank",)),
    (NPC_FJOLA, "twins", ("fjola", "both")),
    (NPC_EYDIS, "twins", ("eydis", "both")),
    (NPC_ANUBARAK, "anubarak", ("pickup",)),
)
CHAMPIONS_ARRIVAL = "champions"
# both factions, as ToCFactionChampions lists them
CHAMPION_ENTRIES = frozenset({
    34469, 34470, 34465, 34466, 34459, 34444, 34445, 34447,
    34461, 34460, 34467, 34468, 34471, 34473, 34472, 34463, 34474, 34475,
    34458, 34451, 34448, 34449, 34456, 34441, 34454, 34455, 34450, 34453,
})

# bots re-note their stand on the session's first ticks, which trail the engage
NOTE_SLACK_MS = 2000
EARLY_HIT_MS = 5000
OFF_STAND_YD = 3.0
SHOWN = 12


def pull_end(trace: Trace) -> int:
    ends = trace.of("end")
    if ends:
        return ends[-1]["t"]
    return trace.records[-1].get("t", 0) if trace.records else 0


def difficulty(trace: Trace) -> str:
    return DIFFICULTIES.get(trace.header.get("diff"), "?")


def is_toc(trace: Trace) -> bool:
    return trace.header.get("map", map_of(trace.path)) == MAP_TOC


def parse_stand(text) -> dict | None:
    """A `toc.stage` value as `{label, arrival, role, spot, human}`, None for `none` or anything that
    doesn't parse."""
    parts = str(text).split()
    if len(parts) < 2 or "." not in parts[0]:
        return None
    try:
        spot = tuple(float(value) for value in parts[1].split(","))
    except ValueError:
        return None
    if len(spot) != 3:
        return None
    arrival, role = parts[0].split(".", 1)
    return {"label": parts[0], "arrival": arrival, "role": role, "spot": spot,
            "human": HUMAN_SUFFIX in parts[2:]}


def stands(trace: Trace, arrival: str, until: int) -> dict[int, dict | None]:
    """Per bot that has one, its stand for `arrival` as of `until`: None when its latest note by then
    reads `none`, else its latest stand whose label leads with the arrival."""
    latest: dict[int, str] = {}
    matched: dict[int, dict] = {}
    for rec in notes(trace, STAGE_KEY):
        if rec["t"] > until:
            continue
        guid, text = rec.get("g"), str(rec.get("txt", ""))
        latest[guid] = text
        stand = parse_stand(text)
        if stand and stand["arrival"] == arrival:
            matched[guid] = stand
    out: dict[int, dict | None] = {guid: None for guid, text in latest.items() if text == NO_STAND}
    out.update({guid: stand for guid, stand in matched.items() if guid not in out})
    return out


def releases(trace: Trace) -> list[dict]:
    """Each staged unit's release in time order, `{guid, arrival, duties, t, victim}`, off the first
    snapshot row with its target set. Of the champions only the first to take one."""
    units = [(guid, arrival, duties) for entry, arrival, duties in STAGED_UNITS
             for guid in guids_of_entry(trace, entry)]
    champions = {guid for guid, entry in trace.entries.items() if entry in CHAMPION_ENTRIES}
    rows = track(trace, {guid for guid, _, _ in units} | champions, ("t", "target"))
    first = {guid: next(((when, victim) for when, victim in found if victim), None)
             for guid, found in rows.items()}

    out = []
    for guid, arrival, duties in units:
        if first.get(guid):
            when, victim = first[guid]
            out.append({"guid": guid, "arrival": arrival, "duties": duties, "t": when, "victim": victim})
    opened = sorted((first[guid][0], guid) for guid in champions if first.get(guid))
    if opened:
        when, guid = opened[0]
        out.append({"guid": guid, "arrival": CHAMPIONS_ARRIVAL, "duties": (), "t": when,
                    "victim": first[guid][1]})
    return sorted(out, key=lambda release: (release["t"], release["guid"]))


def first_on_tank(trace: Trace, release: dict) -> int | None:
    """ms from the release until the unit's victim is a tank-role member, None if it never is."""
    found = track(trace, {release["guid"]}, ("t", "target")).get(release["guid"], [])
    return next((when - release["t"] for when, victim in found
                 if when >= release["t"] and trace.role(victim) == "tank"), None)


def early_hits(trace: Trace, release: dict) -> list[dict]:
    """The unit's `dmg` rows on non-tank members in the EARLY_HIT_MS from its release."""
    start = release["t"]
    return [rec for rec in trace.of("dmg")
            if rec.get("s") == release["guid"] and start <= rec["t"] <= start + EARLY_HIT_MS
            and rec.get("d") in trace.roles and trace.role(rec.get("d")) != "tank"]


def stand_rows(trace: Trace, release: dict) -> list[dict]:
    """Per roster bot: `{guid, state, stand, gap, duty}`. State is `stand`, `none` or `no stand`; gap
    the 2D distance from the stand in its last snapshot at or before the release, None unsampled."""
    held = stands(trace, release["arrival"], release["t"] + NOTE_SLACK_MS)
    out = []
    for guid in sorted(roster_guids(trace) - trace.humans):
        row = {"guid": guid, "state": "no stand", "stand": None, "gap": None, "duty": False}
        if guid in held:
            stand = held[guid]
            row["state"] = "stand" if stand else NO_STAND
            if stand:
                spot = at(trace, guid, release["t"], BEFORE)
                row["stand"] = stand
                row["gap"] = dist2(spot, stand["spot"]) if spot else None
                row["duty"] = stand["role"] in release["duties"]
        out.append(row)
    order = {"stand": 1, NO_STAND: 2, "no stand": 3}
    return sorted(out, key=lambda row: (0 if row["duty"] else order[row["state"]],
                                        row["stand"]["role"] if row["stand"] else "",
                                        trace.name(row["guid"])))


def release_report(trace: Trace, release: dict) -> dict:
    rows = stand_rows(trace, release)
    gaps = [row["gap"] for row in rows if row["state"] == "stand"]
    return {
        "release": release,
        "rows": rows,
        "holders": [row for row in rows if row["duty"]],
        "on_tank": first_on_tank(trace, release),
        "hits": early_hits(trace, release),
        "within": sum(1 for gap in gaps if gap is not None and gap <= OFF_STAND_YD),
        "past": sum(1 for gap in gaps if gap is not None and gap > OFF_STAND_YD),
        "unsampled": sum(1 for gap in gaps if gap is None),
        "none": sum(1 for row in rows if row["state"] == NO_STAND),
        "missing": sum(1 for row in rows if row["state"] == "no stand"),
    }


def show_banner(trace: Trace) -> None:
    end = pull_end(trace)
    outcome = trace.of("end")[-1].get("out", "?") if trace.of("end") else "no end record"
    print(f"{encounter_of(trace)} {difficulty(trace)}  {outcome} at {clock(end)}"
          f"  {len(combat_deaths(trace))} death(s)")
    if not is_toc(trace):
        print("  not a Trial of the Crusader trace, every section below reads empty")
        return
    if not notes(trace, STAGE_KEY):
        print(f"  {STAGE_KEY} never written: a build without staging, or no bot's window opened")


def describe_victim(trace: Trace, guid: int) -> str:
    kind = "human" if guid in trace.humans else "bot" if guid in trace.roles else "not a player"
    return f"{trace.name(guid)}, {trace.role(guid)}, {kind}"


def stand_text(stand: dict) -> str:
    return stand["role"] + (f" {HUMAN_SUFFIX}" if stand["human"] else "")


def show_release(trace: Trace, report: dict) -> None:
    release = report["release"]
    print(f"  {trace.name(release['guid'])} ({release['arrival']}) released at {clock(release['t'])}"
          f" on {describe_victim(trace, release['victim'])}")

    if release["duties"]:
        several = len(release["duties"]) > 1
        holders = ", ".join(" ".join([trace.name(row["guid"])] + ([row["stand"]["role"]] if several else [])
                                     + ([HUMAN_SUFFIX] if row["stand"]["human"] else []))
                            for row in report["holders"]) or "nobody"
        print(f"    duty {' or '.join(release['duties'])}: {holders}")
    else:
        print("    no duty stand")

    on_tank = report["on_tank"]
    print("    never on a tank" if on_tank is None else f"    first on a tank after {on_tank} ms")

    hits = report["hits"]
    if hits:
        listed = ", ".join(f"{trace.name(rec.get('d'))} +{rec['t'] - release['t']} ms {rec.get('a', 0):,}"
                           for rec in hits[:SHOWN])
        print(f"    non-tank hits in {EARLY_HIT_MS // 1000} s: {len(hits)},"
              f" {sum(rec.get('a', 0) for rec in hits):,} damage: {listed}"
              + (f" and {len(hits) - SHOWN} more" if len(hits) > SHOWN else ""))
    else:
        print(f"    non-tank hits in {EARLY_HIT_MS // 1000} s: none")

    stood = report["within"] + report["past"] + report["unsampled"]
    print(f"    {stood} stand(s), {report['within']} within {OFF_STAND_YD:.0f} yd, {report['past']} past it,"
          + (f" {report['unsampled']} unsampled," if report["unsampled"] else "")
          + f" {report['none']} none, {report['missing']} no stand")

    print(f"\n    {'bot':14} {'role':6} {'stand':16} {'at':15} {'yd':>6}")
    for row in report["rows"]:
        name, role = trace.name(row["guid"])[:14], trace.role(row["guid"])
        if row["state"] != "stand":
            print(f"    {name:14} {role:6} {row['state']}")
            continue
        stand = row["stand"]
        spot = ",".join(f"{value:.0f}" for value in stand["spot"])
        gap = "-" if row["gap"] is None else f"{row['gap']:.1f}" + ("!" if row["gap"] > OFF_STAND_YD else "")
        print(f"    {name:14} {role:6} {stand_text(stand):16} {spot:15} {gap:>6}")


def show_staging(trace: Trace) -> None:
    print("STAGING")
    if not is_toc(trace):
        print("  not a Trial of the Crusader trace")
        return
    found = releases(trace)
    if not found:
        print("  no staged unit took a target")
        return
    for index, release in enumerate(found):
        if index:
            print()
        show_release(trace, release_report(trace, release))
    print(f"\n  stand is the bot's latest {STAGE_KEY} up to {NOTE_SLACK_MS // 1000} s past the release,"
          " none for a window with no stand;")
    print("  yd is its 2D distance from the stand in its last snapshot at or before the release, ! past"
          f" {OFF_STAND_YD:.0f} yd;")
    print("  human after a duty stand means a living human stood nearer the release point than the stand")


SECTIONS = (
    ("staging", "per release: victim, duty holder, time to a tank, early hits, each bot's stand", show_staging),
)


def main() -> int:
    return run_sections(__doc__, SECTIONS, show_banner)


if __name__ == "__main__":
    sys.exit(main())
